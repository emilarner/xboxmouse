#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>

#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <linux/types.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>

/* definitions for the way Linux represents controller inputs. */
#define JS_EVENT_BUTTON		0x01	/* button pressed/released */
#define JS_EVENT_AXIS		0x02	/* joystick moved */
#define JS_EVENT_INIT		0x80	/* initial state of device */

struct js_event {
	__u32 time;	/* event timestamp in milliseconds */
	__s16 value;	/* value */
	__u8 type;	/* event type */
	__u8 number;	/* axis/button number */
};

/* global variables for a simple program like this are fine. */
Display *display;
Window root_window;
pthread_mutex_t mutex;

double joystick_angle = 0.00;
double joystick_magnitude = 0.00;

time_t x_click_time = 0;

/* move a mouse to (x, y) */
void move_mouse(int x, int y) {
	XSelectInput(display, root_window, KeyReleaseMask);
	XWarpPointer(display, None, None, 0, 0, 0, 0, x, y);
	XFlush(display);
}

/* send a mouse click--right click and pressed/released functionality too. */
void click_mouse(bool right_click, bool released) {
	int code = right_click ? 3 : 1;
				// branchless: int code = 1 + 2*right_click !
				// but we're not that nerdy
	
	if (!released)
		XTestFakeButtonEvent(display, code, True, CurrentTime);
	else
		XTestFakeButtonEvent(display, code, False, CurrentTime);
	
	XFlush(display);
}

/* every n microseconds, move the mouse along a velocity vector as defined by global vector components. */
void *mouse_moving_thread(void *args) {
	while (true) {
		int x = joystick_magnitude * cos(joystick_angle);
		int y = joystick_magnitude * sin(joystick_angle);

		move_mouse(x, y);

		usleep(10000);
	}
}

/* send a character as if you were the keyboard. */
void send_character(char character) {
	char string[2] = {0};
	string[0] = character;

	KeyCode modcode = 0;
	modcode = XKeysymToKeycode(display, XStringToKeysym(string));
	XTestFakeKeyEvent(display, modcode, True, 0);
	XTestFakeKeyEvent(display, modcode, False, 0);
	XFlush(display);
}

/* send either a left or right arrow press. */
void send_left_right(bool right) {
	KeyCode modcode;
	modcode = XKeysymToKeycode(display, right ? XK_Right : XK_Left);
	XTestFakeKeyEvent(display, modcode, True, 0);
	XTestFakeKeyEvent(display, modcode, False, 0);
	XFlush(display);
}
       
#ifdef SMART_TV
/* open a virtual keyboard if in Smart TV mode */
void open_virtual_keyboard(void) {
	FILE *fp = fopen("/dev/null", "wb");
	int fd = fileno(fp);
	int oldstdout = dup(1);
	int oldstderr = dup(2);

	// it just werkzzzz
	dup2(fd, 1);
	dup2(fd, 2);

	system("(i3-msg 'layout splitv; exec xvkbd') &> /dev/null");
	usleep(200000);
	system("(i3-msg '[class=\"XVkbd\"] focus') &> /dev/null");
	system("(i3-msg 'resize shrink height 170') &> /dev/null");

	dup2(oldstdout, 1);
	dup2(oldstderr, 2);
}

/* close all virtual keyboards */
void close_virtual_keyboard(void) {
	system("pkill xvkbd");
}

/* get the active workspace, so we know which virtual keyboard to resize. */
/* but also for workspace changing features. */
int get_current_workspace(void) {
	char buffer[65536] = {0};
	FILE *fp = popen("i3-msg -t get_workspaces", "r");
	fread(buffer, 1, sizeof(buffer), fp);

	//printf("%s\n", buffer);

	char *num = strstr(buffer, "\"num\":");
	char *visible = strstr(buffer, "\"visible\":");

	/* c magic (: */
	/* basically, keep finding 'num' and 'visible' fields in parallel in the i3-msg JSON output */
	/* then detect if, once, visible is true (by checking the first letter after it) */
	/* then, the corresponding 'num' we found ought to be the current workspace. */
	/* no JSON or formal grammars/parsing/lexing/etc needed! */
	/* nlohmann, another day... */

	do {
		if ((visible + sizeof("\"visible\":") - 1)[0] == 't') {
			char *num_border = strchr(num, ',');
			char num_buffer[3] = {0};
			memcpy(num_buffer, num + sizeof("\"num\":") - 1, num_border - sizeof("\"num\":") + 1 - num);
			return atoi(num_buffer);
		}

		num = strstr(num + sizeof("\"num\":"), "\"num\":");
		visible = strstr(visible + sizeof("\"visible\":"), "\"visible\":");
	} while (num);

	return -1;
}

/* change workspace for smart tv mode*/
void change_workspace(int n) {
	char command[256];
	sprintf(command, "i3-msg workspace %d", n);
	system(command);
}

#endif

double scroll_velocity = 0.00;
bool scroll_up = true;

void *scrolling_thread(void *args) {
	while (true) {
		if (scroll_velocity > 0.25) {
			for (int i = 0; i < 12*(int)scroll_velocity; i++) {
				double tmp_scroll = scroll_velocity;
				XTestFakeButtonEvent(display, scroll_up ? 4 : 5, True, CurrentTime);
				XTestFakeButtonEvent(display, scroll_up ? 4 : 5, False, CurrentTime);
				usleep(100000.0);
				
				XFlush(display);
			}

			XSync(display, False);
		}

		usleep(100000);
	}
}

#define bad_arg(x) do { 																\
				   		fprintf(stderr, "Error: the %s arg was not provided\n", x); 	\
				   		return -1; 														\
				   } while (0); 														\


const char *HELP_TEXT = "xboxmouse - control your *nix computer with a wired/wireless XBOX-like controller\n"
#ifdef SMART_TV
						"compiled with smart TV capabilities\n"
#endif
						"USAGE:\n"
						"-p/--path: where is the special file pertaining to controller inputs?\n"
						"			HINT: this is typically /dev/input/js0\n\n"
						"-s/--sensitivity: how sensitive do you want the velocity?\n"
						"			HINT: 3 or 4 usually works best, but experiment!\n\n"
						"-h/--help: display this menu, obviously!\n"
#ifdef SMART_TV
						"-u/--username: username for smart TV capabilities such as volume and i3 stuff.\n"
						"-hp/--homepage: home page for smart TV mode (brings you to a specified website)\n"
						"				 ... when the home button, or similar, is pressed.\n\n"
#endif
						"\nEND OF OPTIONS\n";

int main(int argc, char *argv[], char *envp[]) {
	char *controller_path = NULL;
	char *username = NULL;
	int sensitivity = -1;
	char *homepage = NULL;

	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			if (!strcmp(argv[i], "--username") || !strcmp(argv[i], "-u")) {
				if (argv[i + 1] == NULL)
					bad_arg("username");

				username = argv[i + 1];
			}

			else if (!strcmp(argv[i], "--path") || !strcmp(argv[i], "-p")) {
				if (argv[i + 1] == NULL)
					bad_arg("controller path");

				controller_path = argv[i + 1];
			}

			else if (!strcmp(argv[i], "--sensitivity") || !strcmp(argv[i], "-s")) {
				if (argv[i + 1] == NULL)
					bad_arg("sensitivity");

				sensitivity = atoi(argv[i + 1]);
			} 
			
			else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
				puts(HELP_TEXT);
				return 0;	
			} 
			
#ifdef SMART_TV
			else if (!strcmp(argv[i], "-hp") || !strcmp(argv[i], "--home-page")) {
				if (argv[i + 1] == NULL)
					bad_arg("home page");

				homepage = argv[i + 1];
			}
#endif
			
			else {
				fprintf(stderr, "FATAL: The argument provided was not recognized\n");
				return -1;
			}
		}
	}

	if (controller_path == NULL) {
		fprintf(stderr, "FATAL: The controller path was never provided.\n");
		return -1;
	}

	if (username == NULL) {
		fprintf(stderr, "FATAL: The username was never provided, we need this to adjust audio.\n");
		return -1;
	}

	if (sensitivity == -1) {
		fprintf(stderr, "WARNING: The sensitivity was never set, using the default of 3\n");
		sensitivity = 3;
	}

#ifdef SMART_TV
	char pactl_command[512] = {0}; 
	sprintf(pactl_command, "sudo -u %s pactl set-sink-volume @DEFAULT_SINK@ +10%%", username);
	size_t pactl_length = strlen(pactl_command);

	if (homepage == NULL) {
		fprintf(stderr, "warning: in smart TV mode, but no home page defined. defaulting to DuckDuckGo\n");
		homepage = "https://duckduckgo.com";
	}

	char homepage_command[256];
	snprintf(homepage_command, sizeof(homepage_command), "sudo -u %s xdg-open %s", username, homepage);
#endif

	display = XOpenDisplay(0);
	root_window = DefaultRootWindow(display);
	
	pthread_t mouse_thread;
	pthread_create(&mouse_thread, NULL, &mouse_moving_thread, NULL);

	pthread_t scroll_thread;
	pthread_create(&scroll_thread, NULL, &scrolling_thread, NULL);
	
	bool keyboard_opened = false;
	bool launcher_opened = false;
	bool input_enabled = true;

	int x = 0;
	int y = 0;

	time_t last_y = 0;

	char characters[] = "abcdefghijklmnopqrstuvwxyz ";
	int character_index = 0;

	FILE *fp = fopen(controller_path, "rb");
	
	if (fp == NULL) {
		fprintf(stderr, "Error opening device: %s\n", strerror(errno));
		fprintf(stderr, "Is it connected? Do you have the kernel drivers/modules?\n");
		return -1;
	}

	while (!feof(fp)) {
		struct js_event event;
		size_t length = fread(&event, 1, sizeof(event), fp);

		switch (event.type) {
			case JS_EVENT_AXIS: {
				if (!input_enabled)
					break;

				// axises for primary analogue stick
				if (event.number == 0 || event.number == 1) {
					int16_t number = event.value;
					switch (event.number) {
						case 0: {
							x = number;
							break;
						}

						case 1: {
							y = number;
							break;
						}		
					}

					joystick_angle = atan2(y, x);
					joystick_magnitude = sqrt(y*y + x*x) * 0.0001 * sensitivity; 
				}

				// up and down axis of secondary analogue stick
				else if (event.number == 4) {
					int16_t scroll = event.value;
					scroll_up = scroll < 0;
					scroll_velocity = (double)scroll * 0.0001 * (scroll < 0 ? -1 : 1);
				}

				// dpad left and right 
				else if (event.number == 6) {
					int16_t value = event.value;
					bool right = value > 0;
					if (value != 0)
						send_left_right(right);
				}

#ifdef SMART_TV
				// dpad up and down
				else if (event.number == 7) {
					if (!event.value)
						break;
					
					bool up = event.value < 0;
					char signage = up ? '+' : '-';
					
					pactl_command[pactl_length - 4] = signage;
					
					puts(pactl_command);
					system(pactl_command);
				}

				break;
#endif
			}

			case JS_EVENT_BUTTON: {
				if (!input_enabled && event.number != 8)
					break;

				switch (event.number) {
					// a button
					case 0: {
						if (event.value)
							click_mouse(true, false);
						else
							click_mouse(true, true);
						break;
					}

					// b button
					case 1: {
						if (event.value)
							click_mouse(false, false);
						else
							click_mouse(false, true);

						break;
					}

#ifdef SMART_TV
					// x button, for terminating the current window w/ i3 in smart tv mode
					case 2: {
                        if (!event.value)
							system("i3-msg kill");
						//character_index = (character_index + 1) % 27;
						break;
					}
					
					// y button
					case 3: {
						if (event.value)
							system(homepage_command);

						break;
					}

					// left shoulder
					case 4: {
						if (!event.value)
							break;
						
						int workspace = get_current_workspace() - 1;
						if (workspace < 1)
							break;

						change_workspace(workspace);
						break;
					}

					// right shoulder
					case 5: {
						if (!event.value)
							break;

						int workspace = get_current_workspace() + 1;
						if (workspace > 10)
							break;
						
						change_workspace(workspace);
						break;
					}

#endif

					case 8: {
						if (!event.value) {
							input_enabled = !input_enabled;
							joystick_magnitude = 0;
							scroll_velocity = 0;
						}

						break;
					}

					case 9: {
						if (!event.value)
							joystick_magnitude = 0;

						break;
					}

					// button for dmenu or another launcher (rofi?)
#ifdef SMART_TV
					case 6: {
						if (event.value)
							break;

						if (!launcher_opened) {
							system("dmenu_run &");
							launcher_opened = true;
						} else {
							system("pkill dmenu");
							launcher_opened = false;
						}

						break;
					}

					// virtual keyboard handler for home button
					case 7: {
						if (event.value)
							break;

						if (!keyboard_opened) {
							open_virtual_keyboard();
							keyboard_opened = true;
						} else {
							close_virtual_keyboard();
							keyboard_opened = false;
						}

						break;
					}
#endif
				
				}

				break;
			}
		}
	}


	return 0;
}

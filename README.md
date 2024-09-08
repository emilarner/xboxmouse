# xboxmouse

Control your Linux computer using a wireless or wired Xbox controller (or something similar), useful for a media station.

## Overview
xboxmouse is a small and simple program written in C that allows one to use their Xbox controller, or something similar, whether it is wireless or wired, to control the mouse on their UNIX-like system (only tested vigorously and known to only work on Linux, beware), provided that it can fulfill the dependencies that this program requires. Any device which has similar buttons and controls to an Xbox controller will work, as well as any device which exposes itself in /dev/input (typically in the form of /dev/input/jsX) and has information consistent with the Linux Joystick API will work.

In addition to that, this program has a compilation feature which will compile Smart TV-like capabilities into the program. This feature is *only* compatible with users using **i3wm**, along with certain required related programs. This feature will open and close a virtual keyboard with the pressing of the Xbox home button, to allow users to type alongside the mouse features the program provides. Additionally, the shoulder buttons will switch the current workspace on i3, the X button will close the current focused window, and the Y button will open a predefined homepage in the user's default web browser (through xdg-open). This feature is indispensable for creating a home-cooked Smart TV or home entertainment centre of some sorts.

## Dependencies

Since this program is relatively small, there is a small amount of dependencies:

 - i3-msg (only for Smart TV mode users, to manage the i3 environment)
 - XVkbd (only for Smart TV mode users, the virtual keyboard software)

## Building & Installation

To build xboxmouse without Smart TV features, simply run:

    make

If you want Smart TV features compiled into the program, because you intend to use this in tandem with i3wm:

    make FEATURES=SMART_TV

Then, to install the compiled program (to /usr/local/bin) so that it may be in your PATH:

    sudo make install

## Usage

A lot of the arguments for the program can be found by issuing -h or --help to the compiled and installed xboxmouse program. However, generally, the xboxmouse program just runs passively (though not necessarily in the background as a daemon) and listens to controller inputs and then acts accordingly. Importantly though, you need to provide the actual special device used for representing your controller to the program with -p or --path. If you cannot find it, try `ls /dev/input/`; it is typically `/dev/input/js0`. 

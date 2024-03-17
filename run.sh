#!/bin/sh

make all
# killall xinit
# startx ./_dev/c_dev/zwm/.xinitrc
startx ./.xinitrc -- /usr/bin/Xephyr -screen 1600x1150 -reset

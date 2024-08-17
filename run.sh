#!/bin/sh

# this is used for local quick testing
sudo make for_gdb install
startx ./.xinitrc -- /usr/bin/Xephyr -screen 1800x1350 -reset -core

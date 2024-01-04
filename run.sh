

#!/bin/sh

make all
killall xinit
startx ./_dev/c_dev/zwm/.xinitrc
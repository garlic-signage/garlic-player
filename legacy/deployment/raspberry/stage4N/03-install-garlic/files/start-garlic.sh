#!/bin/bash

PIWIZPATH="/etc/xdg/autostart/piwiz.desktop"

if [ -e "$PIWIZPATH" ]; then
    exit 0
fi

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib
/usr/local/bin/garlic-player -platform xcb -m fullscreen
#comment out for more debug informations
export QT_LOGGING_RULES="*.debug=false"

exit 0

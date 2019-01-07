#!/bin/sh
source $mountpoint/etc/options_menu/themes/scripts/om_vars
script_init

# hasLang="$(find $omModSubCmds -type f -name 'c00[0-9][1-9]_*')"

# wget -q --spider http://google.com
# if [ $? -eq 0 ] && [ ! -z "$hasLang" ]; then
[ -f "$disableDownloads" ] && mv "$disableDownloads" "$enableDownloads"
# else
# 	[ -f "$enableDownloads" ] && mv "$enableDownloads" "$disableDownloads"
# fi

[ -f "$omDummy" ] && rm $omDummy

if [ "$(ls -A $themePath)" ]; then
touch $omDummy
    themeLoader 
else
   [ -f "$omModSpacer" ] && rm "$omModSpacer"
fi
noThemesNoClear

usleep 50000 && $optionsMenu/options --commandPath $omModCommands/ --scriptPath $omModScripts --title "Theme Selector" &

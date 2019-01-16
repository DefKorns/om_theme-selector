#!/bin/sh
source $mountpoint/etc/options_menu/themes/scripts/om_vars
script_init

chMenu
fntFix

if ping -q -c 1 -W 1 google.com >/dev/null; then
  [ -f "$disableDownloads" ] && mv "$disableDownloads" "$enableDownloads"
else
  [ -f "$enableDownloads" ] && mv "$enableDownloads" "$disableDownloads"
fi

[ -f "$omDummy" ] && rm "$omDummy"

if [ "$(ls -A $themePath)" ]; then
	touch "$omDummy"
	themeLoader
else
	[ -f "$omModSpacer" ] && rm "$omModSpacer"
fi
noThemesNoClear

if [ -f "$b_file" ] && [ -f "$p_file" ]; then
	[ -f "$disableSettings" ] && mv "$disableSettings" "$enableSettings"
else
	[ -f "$enableSettings" ] && mv "$enableSettings" "$disableSettings"
fi

usleep 50000 && $optionsMenu/options --commandPath $omModCommands/ --scriptPath $omModScripts --title "Theme Selector" &

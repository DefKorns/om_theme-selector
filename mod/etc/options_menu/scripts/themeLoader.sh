#!/bin/sh
source $mountpoint/etc/options_menu/themes/scripts/om_vars
script_init

chMenu
fntFix

# hasLang="$(find $omModSubCmds -type f -name 'c00[0-9][1-9]_*')"

if ping -q -c 1 -W 1 google.com >/dev/null; then
  mv "$disableDownloads" "$enableDownloads"
else
  mv "$enableDownloads" "$disableDownloads"
fi

[ -f "$omDummy" ] && rm $omDummy

if [ "$(ls -A $themePath)" ]; then
	touch $omDummy
	themeLoader
else
	[ -f "$omModSpacer" ] && rm "$omModSpacer"
fi
noThemesNoClear

if [ ! -f "$rootfs/overlaythemes" ]; then
	cp -r "$omModScripts/om_overlay" "$rootfs/overlaythemes"
else
	if [ "$(cksum "$rootfs/overlaythemes" | awk '{ print $1; }')" != "$(cksum "$omModScripts/om_overlay" | awk '{ print $1; }')" ]; then
		cp -r "$omModScripts/om_overlay" "$rootfs/overlaythemes"
	fi
fi

if [ -f "$b_file" ] && [ -f "$p_file" ]; then
	mv "$disableSettings" "$enableSettings"
else
	mv "$enableSettings" "$disableSettings"
fi

usleep 50000 && $optionsMenu/options --commandPath $omModCommands/ --scriptPath $omModScripts --title "Theme Selector" &

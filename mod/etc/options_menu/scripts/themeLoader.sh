#!/bin/sh
#
#  Copyright 2019 DefKorns (https://gitlab.com/DefKorns/om_theme-selector/LICENSE)
#  
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#  
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#  
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
source $mountpoint/etc/options_menu/themes/scripts/om_vars
script_init

chMenu
fntFix

if ping -q -c 1 -W 1 google.com >/dev/null; then
  rename "$disableDownloads" "$enableDownloads"
else
  rename "$enableDownloads" "$disableDownloads"
fi

remove "$omDummy"

if [ "$(ls -A $themePath)" ]; then
	touch "$omDummy"
	themeLoader
else
	remove "$omModSpacer"
fi
noThemesNoClear

if [ -f "$b_file" ] && [ -f "$p_file" ]; then
	rename "$disableSettings" "$enableSettings"
else
	rename "$enableSettings" "$disableSettings"
fi

usleep 50000 && $optionsMenu/options --commandPath $omModCommands/ --scriptPath $omModScripts --title "Theme Selector" &

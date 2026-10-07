/**
  * Copyright (c) 2026 DefKorns (https://defkorns.github.io/LICENSE)
  *
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  */

#ifndef COMMAND_LOADER_H_
#define COMMAND_LOADER_H_

#include "command.h"

#include <string>
#include <vector>

struct AppOptions
{
    std::string commandLocation;
    std::string scriptLocation;
    std::string titleKey = "THEME_SELECTOR";
    bool isRootScreen = true; // no --commandPath override - top of the theme-selector's own tree
    bool gridLayout = true;
    std::string folderThemesDir;
    std::string folderThemesMapping;
    std::string folderMusicMapping;
    bool folderMusicScreen = false;
};

// each screen remembers its last focused row here; cleared on leaving the theme manager
constexpr const char * FocusStatePrefix = "/tmp/om_focus";

std::string ReadSftype();

AppOptions ParseArgs(int argc, char * argv[], const std::string & optionsLocation);

// OM_BACK_STACK is set by OptionsMenu before launching "Theme Options"
std::string ReadBackStack(bool isRootScreen);

struct ItemInfo
{
    bool isThemeItem; // false for a fixed "c0000_*" action, true for a theme/asset row
    std::string author;
};

bool LoadCommands(const std::string & commandLocation, const std::string & scriptLocation, const std::string & optionsLocation,
                   std::vector<Command> & commands, std::vector<ItemInfo> & items);

void AppendBackNavigation(const std::string & optionsLocation, const AppOptions & options, const std::string & backStack,
                           std::vector<Command> & commands, std::vector<ItemInfo> & items);

#endif

/**
  * Copyright (c) 2026 DefKorns (https://defkorns.github.io/LICENSE)
  *
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  */

// Standalone theme manager using the vendored OptionsMenu engine. Each
// screen relaunches this binary with a different --commandPath.

#include "single_instance_lock.h"
#include "command_loader.h"
#include "theme_manager_app.h"
#include "framework/sdl_helper.h"
#include "localization.h"

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char * argv[])
{
    SingleInstanceLock::Acquire();

    const std::string optionsLocation = "/etc/options_menu/";
    AppOptions options = ParseArgs(argc, argv, optionsLocation);
    const std::string backStack = ReadBackStack(options.isRootScreen);

    LoadLanguageFromConfig(optionsLocation);
    SetTTFFontPath(optionsLocation);
    UiTheme::LoadThemeConfig(optionsLocation);

    std::vector<Command> commands;
    std::vector<bool> isThemeItem;
    if(!LoadCommands(options.commandLocation, options.scriptLocation, optionsLocation, commands, isThemeItem))
        return 1;

    AppendBackNavigation(optionsLocation, options, backStack, commands, isThemeItem);

    if(commands.empty())
    {
        std::cerr << "No usable commands in " << options.commandLocation << "\n";
        return 1;
    }

    ThemeManagerApp app(optionsLocation, std::move(options), std::move(commands), std::move(isThemeItem));
    return app.Run();
}

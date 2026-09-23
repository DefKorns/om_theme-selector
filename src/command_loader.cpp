/**
  * Copyright (c) 2026 DefKorns (https://defkorns.github.io/LICENSE)
  *
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  */

#include "command_loader.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <list>
#include <memory>
#include <dirent.h>

namespace {

void ReplaceAll(std::string & command, const std::string & oldString, const std::string & newString)
{
    size_t pos;
    while((pos = command.find(oldString)) != std::string::npos)
        command.replace(pos, oldString.size(), newString);
}

struct DirCloser { void operator()(DIR * dir) const { if(dir) closedir(dir); } };
using DirHandle = std::unique_ptr<DIR, DirCloser>;

struct PipeCloser { void operator()(FILE * pipe) const { if(pipe) pclose(pipe); } };
using PipeHandle = std::unique_ptr<FILE, PipeCloser>;

// child screens inherit this via fork+exec, same as OM_BACK_STACK - so only the first screen
// that needs sftype pays for the shell fork
std::string ReadSftype()
{
    if(const char * env = std::getenv("OM_SFTYPE"))
        return env;

    std::string result;
    PipeHandle pipe(popen("source /etc/preinit; script_init; echo $sftype", "r"));
    if(pipe)
    {
        char buffer[32] = {0};
        if(fgets(buffer, sizeof(buffer), pipe.get()))
            result = buffer;
    }
    while(!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();

    setenv("OM_SFTYPE", result.c_str(), 1);
    return result;
}

// my own convention, not OptionsMenu's Command format
struct ConsoleOnlyFlags { bool nesOnly = false; bool snesOnly = false; };

ConsoleOnlyFlags ReadConsoleOnlyFlags(const std::string & path)
{
    ConsoleOnlyFlags flags;
    std::ifstream in(path);
    std::string line;
    while(std::getline(in, line))
    {
        if(!line.empty() && line.back() == '\r')
            line.pop_back();
        if(line == "NES_ONLY=TRUE")
            flags.nesOnly = true;
        else if(line == "SNES_ONLY=TRUE")
            flags.snesOnly = true;
    }
    return flags;
}

}

AppOptions ParseArgs(int argc, char * argv[], const std::string & optionsLocation)
{
    AppOptions options;
    options.commandLocation = optionsLocation + "themes/commands/";
    options.scriptLocation = optionsLocation + "themes/scripts/";

    for(int i = 1; i < argc; ++i)
    {
        if(std::strcmp(argv[i], "--commandPath") == 0 && i+1 < argc)
        {
            options.commandLocation = argv[++i];
            options.isRootScreen = false;
        }
        else if(std::strcmp(argv[i], "--scriptPath") == 0 && i+1 < argc)
            options.scriptLocation = argv[++i];
        else if(std::strcmp(argv[i], "--title") == 0 && i+1 < argc)
            options.titleKey = argv[++i];
        else if(std::strcmp(argv[i], "--layout") == 0 && i+1 < argc)
            options.gridLayout = (std::string(argv[++i]) == "grid");
    }
    if(!options.commandLocation.empty() && options.commandLocation.back() != '/')
        options.commandLocation += '/';
    if(!options.scriptLocation.empty() && options.scriptLocation.back() != '/')
        options.scriptLocation += '/';
    return options;
}

std::string ReadBackStack(bool isRootScreen)
{
    const char * backStackEnv = std::getenv("OM_BACK_STACK");
    return (isRootScreen && backStackEnv) ? backStackEnv : "";
}

bool LoadCommands(const std::string & commandLocation, const std::string & scriptLocation, const std::string & optionsLocation,
                   std::vector<Command> & commands, std::vector<bool> & isThemeItem)
{
    DirHandle dir(opendir(commandLocation.c_str()));
    if(!dir)
    {
        std::cerr << "Cannot open input folder.\n";
        return false;
    }

    std::list<std::string> fileList;
    while(auto entry = readdir(dir.get()))
        if(entry->d_type == DT_REG && entry->d_name[0] == 'c')
            fileList.push_back(entry->d_name);
    fileList.sort();

    std::ifstream in;
    std::string sftype;
    bool sftypeLoaded = false;
    for(auto & file : fileList)
    {
        in.open(commandLocation + file);
        if(!in.is_open())
            continue;
        Command c(in); // closes `in` itself once read

        // c0000_0000 leading sentinel only - a later empty COMMAND_STR is a reserved blank slot instead
        if(commands.empty() && c.command.empty())
            continue;
        ConsoleOnlyFlags consoleOnly = ReadConsoleOnlyFlags(commandLocation + file);
        if(consoleOnly.nesOnly || consoleOnly.snesOnly)
        {
            if(!sftypeLoaded)
            {
                sftype = ReadSftype();
                sftypeLoaded = true;
            }
            if((consoleOnly.nesOnly && sftype != "nes") || (consoleOnly.snesOnly && sftype == "nes"))
                continue;
        }
        if(!c.command.empty())
        {
            ReplaceAll(c.command, "%options_path%", optionsLocation);
            ReplaceAll(c.command, "%script_dir%", scriptLocation);
            ReplaceAll(c.deleteCommand, "%options_path%", optionsLocation);
            ReplaceAll(c.deleteCommand, "%script_dir%", scriptLocation);
            if(c.isToggle)
            {
                ReplaceAll(c.stateCommand, "%options_path%", optionsLocation);
                ReplaceAll(c.stateCommand, "%script_dir%", scriptLocation);
                c.UpdateState();
            }
        }
        commands.push_back(c);
        isThemeItem.push_back(file.compare(0, 6, "c0000_") != 0);
    }
    return true;
}

void AppendBackNavigation(const std::string & optionsLocation, const AppOptions & options, const std::string & backStack,
                           std::vector<Command> & commands, std::vector<bool> & isThemeItem)
{
    std::string backCommand;
    if(!backStack.empty())
    {
        std::vector<std::string> entries;
        for(size_t start = 0; start < backStack.size();)
        {
            size_t sep = backStack.find(';', start);
            if(sep == std::string::npos) sep = backStack.size();
            entries.push_back(backStack.substr(start, sep-start));
            start = sep+1;
        }
        std::string lastEntry = entries.back();
        entries.pop_back();
        std::string remainingStack;
        for(size_t i = 0; i < entries.size(); ++i)
        {
            if(i) remainingStack += ";";
            remainingStack += entries[i];
        }

        size_t c1 = lastEntry.find(','), c2 = lastEntry.find(',', c1+1);
        std::string backPath = lastEntry.substr(0, c1);
        std::string backScriptPath = lastEntry.substr(c1+1, c2-c1-1);
        std::string backTitleKey = lastEntry.substr(c2+1);

        backCommand = "usleep 50000 && OM_BACK_STACK=\"" + remainingStack + "\" " + optionsLocation + "options --commandPath " + backPath
            + (backScriptPath.empty() ? "" : " --scriptPath " + backScriptPath)
            + " --title \"" + backTitleKey + "\" &";
    }
    else if(!options.isRootScreen)
    {
        std::ifstream returnScript(options.scriptLocation + "om_return");
        if(returnScript.good())
            backCommand = "sh " + options.scriptLocation + "om_return &";
        else
            backCommand = "usleep 50000 && " + optionsLocation + "lib/theme_manager --title INSTALLED_THEMES --layout grid &";
    }
    else
        return;

    Command back;
    back.name = "BACK";
    back.runInternal = false;
    back.restartUI = false;
    back.previewImage = optionsLocation + "images/preview_placeholder.png";
    back.command = backCommand;
    commands.push_back(back);
    isThemeItem.push_back(false);
}

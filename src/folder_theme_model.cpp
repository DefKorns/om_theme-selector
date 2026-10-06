/**
  * Copyright (c) 2026 DefKorns (https://defkorns.github.io/LICENSE)
  *
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  */

#include "folder_theme_model.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>

namespace
{
    const char * const HomeKey = "home";
    const char * const HomeNameMatch = "default";
    const char * const RandomGlobal = "@random";

    std::vector<std::string> SplitTabs(const std::string & line)
    {
        std::vector<std::string> fields;
        size_t start = 0;
        for(size_t tab; (tab = line.find('\t', start)) != std::string::npos; start = tab + 1)
            fields.push_back(line.substr(start, tab - start));
        fields.push_back(line.substr(start));
        return fields;
    }

    bool ReadLine(std::ifstream & in, std::string & line)
    {
        if(!std::getline(in, line))
            return false;
        if(!line.empty() && line.back() == '\r')
            line.pop_back();
        return true;
    }

    std::vector<FolderOption> ReadOptions(const std::string & path)
    {
        std::vector<FolderOption> options;
        std::ifstream in(path);
        for(std::string line; ReadLine(in, line);)
        {
            std::vector<std::string> f = SplitTabs(line);
            if(f[0].empty())
                continue;
            f.resize(5);
            options.push_back({ f[0], f[1].empty() ? f[0] : f[1], f[2], f[3] == "1", f[4] == "1" });
        }
        return options;
    }
}

bool FolderThemeModel::Load(const std::string & dataDir, const std::string & themeMapping, const std::string & musicMapping)
{
    std::ifstream folders(dataDir + "/folders.list");
    std::vector<int> lastAtDepth;
    for(std::string line; ReadLine(folders, line);)
    {
        std::vector<std::string> f = SplitTabs(line);
        if(f.size() < 3 || f[0].empty())
            continue;
        FolderEntry entry{ f[0], f[1], std::atoi(f[2].c_str()), -1 };
        if(entry.depth < 0 || entry.depth > static_cast<int>(lastAtDepth.size()))
            entry.depth = static_cast<int>(lastAtDepth.size());
        if(entry.depth > 0)
            entry.parent = lastAtDepth[entry.depth - 1];
        lastAtDepth.resize(entry.depth);
        lastAtDepth.push_back(static_cast<int>(folders_.size()));
        folders_.push_back(entry);
    }

    slot(Channel::Theme).options = ReadOptions(dataDir + "/themes.list");
    slot(Channel::Music).options = ReadOptions(dataDir + "/music.list");

    std::ifstream global(dataDir + "/global");
    std::string globalId;
    ReadLine(global, globalId);
    globalThemeRandom_ = globalId == RandomGlobal;
    globalTheme_ = OptionIndex(slot(Channel::Theme), globalId);

    std::ifstream audio(dataDir + "/audio");
    for(std::string line; ReadLine(audio, line);)
    {
        musicRandomHome_ = musicRandomHome_ || line == "random_home";
        musicRandomFolders_ = musicRandomFolders_ || line == "random_folders";
    }

    themesPerFolder_ = !themeMapping.empty();
    LoadMapping(slot(Channel::Theme), themeMapping);
    LoadMapping(slot(Channel::Music), musicMapping);
    return !folders_.empty();
}

void FolderThemeModel::LoadMapping(Slot & s, const std::string & mappingPath)
{
    s.mappingPath = mappingPath;
    s.assigned.assign(folders_.size(), -1);
    if(mappingPath.empty())
        return;
    std::ifstream mapping(mappingPath);
    for(std::string line; ReadLine(mapping, line);)
    {
        size_t eq = line.rfind('=');
        int option = eq == std::string::npos ? -1 : OptionIndex(s, line.substr(eq + 1));
        int folder = eq == std::string::npos ? -1 : FolderIndex(line.substr(0, eq));
        if(option < 0 || folder < 0)
        {
            if(!line.empty())
                s.unknownLines.push_back(line);
            continue;
        }
        for(size_t i = 0; i < folders_.size(); ++i)
            if(folders_[i].key == folders_[folder].key)
                s.assigned[i] = option;
    }
}

bool FolderThemeModel::Save(Channel channel) const
{
    const Slot & s = slot(channel);
    if(s.mappingPath.empty())
        return false;
    const std::string tmpPath = s.mappingPath + ".tmp";
    {
        std::ofstream out(tmpPath, std::ios::trunc);
        if(!out)
            return false;
        for(const std::string & line : s.unknownLines)
            out << line << '\n';
        std::vector<std::string> written;
        for(size_t i = 0; i < folders_.size(); ++i)
        {
            if(s.assigned[i] < 0)
                continue;
            bool seen = false;
            for(const std::string & key : written)
                seen = seen || key == folders_[i].key;
            if(seen)
                continue;
            written.push_back(folders_[i].key);
            out << folders_[i].key << '=' << s.options[s.assigned[i]].id << '\n';
        }
        if(!out)
            return false;
    }
    return std::rename(tmpPath.c_str(), s.mappingPath.c_str()) == 0;
}

void FolderThemeModel::Assign(Channel channel, int folder, int option)
{
    // the mapping is keyed by folder name, so same-named folders share it
    Slot & s = slot(channel);
    for(size_t i = 0; i < folders_.size(); ++i)
        if(folders_[i].key == folders_[folder].key)
            s.assigned[i] = option;
}

ResolvedOption FolderThemeModel::Resolve(Channel channel, int folder) const
{
    int own = slot(channel).assigned[folder];
    if(own >= 0)
        return { own, OptionSource::Assigned, folder };
    return ResolveAutomatic(channel, folder);
}

ResolvedOption FolderThemeModel::ResolveAutomatic(Channel channel, int folder) const
{
    const FolderEntry & entry = folders_[folder];
    if(channel == Channel::Theme && (themesPerFolder_ || IsHome(folder)))
    {
        int nameMatch = OptionIndex(slot(channel), IsHome(folder) ? HomeNameMatch : entry.key);
        if(nameMatch >= 0)
            return { nameMatch, OptionSource::NameMatch, folder };
    }
    if(channel == Channel::Theme && !themesPerFolder_)
        return { globalTheme_, OptionSource::Global, -1 };

    if(entry.parent >= 0)
    {
        ResolvedOption up = Resolve(channel, entry.parent);
        if(up.source == OptionSource::Assigned || up.source == OptionSource::NameMatch || up.source == OptionSource::Inherited)
        {
            up.source = OptionSource::Inherited;
            return up;
        }
    }

    if(channel == Channel::Music)
        return AutomaticMusic(folder);
    return { globalTheme_, OptionSource::Global, -1 };
}

// mirrors om_themescript: the theme's own .wav wins, else the randomizer where it applies, else stock
ResolvedOption FolderThemeModel::AutomaticMusic(int folder) const
{
    int theme = Resolve(Channel::Theme, folder).option;
    const std::vector<FolderOption> & themes = slot(Channel::Theme).options;
    if(theme >= 0 && themes[theme].hasMusic)
        return { -1, OptionSource::ThemeMusic, -1 };
    bool randomApplies = IsHome(folder) ? musicRandomHome_ : musicRandomHome_ && musicRandomFolders_;
    if(randomApplies && !(theme >= 0 && themes[theme].needsOverlay))
        return { -1, OptionSource::RandomMusic, -1 };
    return { -1, OptionSource::OriginalMusic, -1 };
}

int FolderThemeModel::UsageCount(Channel channel, int option) const
{
    int count = 0;
    for(int assigned : slot(channel).assigned)
        count += assigned == option ? 1 : 0;
    return count;
}

int FolderThemeModel::OptionIndex(const Slot & s, const std::string & id) const
{
    for(size_t i = 0; i < s.options.size(); ++i)
        if(s.options[i].id == id)
            return static_cast<int>(i);
    return -1;
}

int FolderThemeModel::FolderIndex(const std::string & key) const
{
    for(size_t i = 0; i < folders_.size(); ++i)
        if(folders_[i].key == key)
            return static_cast<int>(i);
    return -1;
}

bool FolderThemeModel::IsHome(int folder) const
{
    return folders_[folder].key == HomeKey && folders_[folder].depth == 0;
}

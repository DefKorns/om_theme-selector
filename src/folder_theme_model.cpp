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
}

bool FolderThemeModel::Load(const std::string & dataDir, const std::string & mappingPath)
{
    mappingPath_ = mappingPath;

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

    std::ifstream themes(dataDir + "/themes.list");
    for(std::string line; ReadLine(themes, line);)
    {
        std::vector<std::string> f = SplitTabs(line);
        if(f.size() < 3 || f[0].empty())
            continue;
        themes_.push_back({ f[0], f[1].empty() ? f[0] : f[1], f[2] });
    }

    std::ifstream global(dataDir + "/global");
    std::string globalId;
    ReadLine(global, globalId);
    globalRandom_ = globalId == RandomGlobal;
    globalTheme_ = ThemeIndex(globalId);

    assigned_.assign(folders_.size(), -1);
    std::ifstream mapping(mappingPath_);
    for(std::string line; ReadLine(mapping, line);)
    {
        size_t eq = line.rfind('=');
        int theme = eq == std::string::npos ? -1 : ThemeIndex(line.substr(eq + 1));
        int folder = eq == std::string::npos ? -1 : FolderIndex(line.substr(0, eq));
        if(theme < 0 || folder < 0)
        {
            if(!line.empty())
                unknownLines_.push_back(line);
            continue;
        }
        for(size_t i = 0; i < folders_.size(); ++i)
            if(folders_[i].key == folders_[folder].key)
                assigned_[i] = theme;
    }

    return !folders_.empty();
}

bool FolderThemeModel::Save() const
{
    const std::string tmpPath = mappingPath_ + ".tmp";
    {
        std::ofstream out(tmpPath, std::ios::trunc);
        if(!out)
            return false;
        for(const std::string & line : unknownLines_)
            out << line << '\n';
        std::vector<std::string> written;
        for(size_t i = 0; i < folders_.size(); ++i)
        {
            if(assigned_[i] < 0)
                continue;
            bool seen = false;
            for(const std::string & key : written)
                seen = seen || key == folders_[i].key;
            if(seen)
                continue;
            written.push_back(folders_[i].key);
            out << folders_[i].key << '=' << themes_[assigned_[i]].id << '\n';
        }
        if(!out)
            return false;
    }
    return std::rename(tmpPath.c_str(), mappingPath_.c_str()) == 0;
}

void FolderThemeModel::Assign(int folder, int theme)
{
    // the mapping is keyed by folder name, so same-named folders share it
    for(size_t i = 0; i < folders_.size(); ++i)
        if(folders_[i].key == folders_[folder].key)
            assigned_[i] = theme;
}

ResolvedTheme FolderThemeModel::Resolve(int folder) const
{
    if(assigned_[folder] >= 0)
        return { assigned_[folder], ThemeSource::Assigned, folder };
    return ResolveAutomatic(folder);
}

ResolvedTheme FolderThemeModel::ResolveAutomatic(int folder) const
{
    const FolderEntry & entry = folders_[folder];
    int nameMatch = ThemeIndex(entry.key == HomeKey && entry.depth == 0 ? HomeNameMatch : entry.key);
    if(nameMatch >= 0)
        return { nameMatch, ThemeSource::NameMatch, folder };

    if(entry.parent >= 0)
    {
        ResolvedTheme up = Resolve(entry.parent);
        if(up.source != ThemeSource::Global)
            up.source = ThemeSource::Inherited;
        return up;
    }
    return { globalTheme_, ThemeSource::Global, -1 };
}

int FolderThemeModel::UsageCount(int theme) const
{
    int count = 0;
    for(int assigned : assigned_)
        count += assigned == theme ? 1 : 0;
    return count;
}

int FolderThemeModel::ThemeIndex(const std::string & id) const
{
    for(size_t i = 0; i < themes_.size(); ++i)
        if(themes_[i].id == id)
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

/**
  * Copyright (c) 2026 DefKorns (https://defkorns.github.io/LICENSE)
  *
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  */

#ifndef FOLDER_THEME_MODEL_H_
#define FOLDER_THEME_MODEL_H_

#include <string>
#include <vector>

struct FolderEntry
{
    std::string key; // snake_case of the folder's Name=, as om_themescript derives it
    std::string name;
    int depth;
    int parent;
};

struct ThemeEntry
{
    std::string id;
    std::string title;
    std::string preview;
};

enum class ThemeSource { Assigned, NameMatch, Inherited, Global };

struct ResolvedTheme
{
    int theme; // -1: stock UI, or a random theme when the randomizer is on
    ThemeSource source;
    int fromFolder;
};

// Folder -> theme assignments, resolved the same way om_themescript applies them on chmenu
class FolderThemeModel
{
public:
    bool Load(const std::string & dataDir, const std::string & mappingPath);
    bool Save() const;

    const std::vector<FolderEntry> & Folders() const { return folders_; }
    const std::vector<ThemeEntry> & Themes() const { return themes_; }
    bool GlobalIsRandom() const { return globalRandom_; }

    int Assigned(int folder) const { return assigned_[folder]; }
    void Assign(int folder, int theme); // -1 clears
    ResolvedTheme Resolve(int folder) const;
    ResolvedTheme ResolveAutomatic(int folder) const; // what the folder gets without its own assignment
    int UsageCount(int theme) const;

private:
    int ThemeIndex(const std::string & id) const;
    int FolderIndex(const std::string & key) const;

    std::vector<FolderEntry> folders_;
    std::vector<ThemeEntry> themes_;
    std::vector<int> assigned_;
    std::vector<std::string> unknownLines_; // folders or themes not on this console right now, kept on save
    std::string mappingPath_;
    int globalTheme_ = -1;
    bool globalRandom_ = false;
};

#endif

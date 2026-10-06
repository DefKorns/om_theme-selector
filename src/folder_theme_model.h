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
    std::string key;
    std::string name;
    int depth;
    int parent;
};

struct FolderOption
{
    std::string id;
    std::string title;
    std::string preview;
    bool hasMusic;
    bool needsOverlay;
};

enum class Channel { Theme = 0, Music = 1 };

enum class OptionSource { Assigned, NameMatch, Inherited, Global, ThemeMusic, RandomMusic, OriginalMusic };

struct ResolvedOption
{
    int option; // -1: nothing specific (stock UI, a random theme, the theme's own or random music)
    OptionSource source;
    int fromFolder;
};

// Folder -> theme and folder -> music assignments, resolved the same way om_themescript applies them on chmenu
class FolderThemeModel
{
public:
    bool Load(const std::string & dataDir, const std::string & themeMapping, const std::string & musicMapping);
    bool Save(Channel channel) const;

    const std::vector<FolderEntry> & Folders() const { return folders_; }
    const std::vector<FolderOption> & Options(Channel channel) const { return slot(channel).options; }
    bool GlobalThemeIsRandom() const { return globalThemeRandom_; }

    int Assigned(Channel channel, int folder) const { return slot(channel).assigned[folder]; }
    void Assign(Channel channel, int folder, int option); // -1 clears
    ResolvedOption Resolve(Channel channel, int folder) const;
    ResolvedOption ResolveAutomatic(Channel channel, int folder) const;
    int UsageCount(Channel channel, int option) const;

private:
    struct Slot
    {
        std::vector<FolderOption> options;
        std::vector<int> assigned;
        std::vector<std::string> unknownLines; // folders or options not on this console right now, kept on save
        std::string mappingPath;
    };

    Slot & slot(Channel channel) { return slots_[static_cast<int>(channel)]; }
    const Slot & slot(Channel channel) const { return slots_[static_cast<int>(channel)]; }
    void LoadMapping(Slot & s, const std::string & mappingPath);
    int OptionIndex(const Slot & s, const std::string & id) const;
    int FolderIndex(const std::string & key) const;
    bool IsHome(int folder) const;
    ResolvedOption AutomaticMusic(int folder) const;

    std::vector<FolderEntry> folders_;
    Slot slots_[2];
    int globalTheme_ = -1;
    bool globalThemeRandom_ = false;
    bool musicRandomHome_ = false;
    bool musicRandomFolders_ = false;
    bool themesPerFolder_ = false; // off: om_themescript ignores assignments and name matches alike
};

#endif

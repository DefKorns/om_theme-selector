/**
  * Copyright (c) 2026 DefKorns (https://defkorns.github.io/LICENSE)
  *
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  */

#ifndef THEME_MANAGER_APP_H_
#define THEME_MANAGER_APP_H_

#include "command_loader.h"
#include "command.h"
#include "framework/badge.h"
#include "framework/controller.h"
#include "framework/dialog.h"
#include "framework/sdl_helper.h"

#include <memory>
#include <string>
#include <vector>

// Owns the SDL/controller session for one screen and runs its grid or list layout loop.
class ThemeManagerApp
{
public:
    ThemeManagerApp(std::string optionsLocation, AppOptions options, std::vector<Command> commands, std::vector<ItemInfo> items);

    int Run()
    {
        if(!options_.folderThemesDir.empty())
            return RunFolderThemesLayout();
        return options_.gridLayout ? RunGridLayout() : RunListLayout();
    }

private:
    enum class FrameEvent { Continue, Quit, PowerButtonPressed };

    std::string optionsLocation_;
    AppOptions options_;
    std::vector<Command> commands_;
    std::vector<ItemInfo> items_;
    int pinnedStartIndex_ = 0; // BACK/EXIT sort last - pinned near the footer instead of scrolling
    int themeStart_ = 0; // grid layout only: first non-fixed-action row

    // unique_ptr so construction can be deferred past the CHECKPOINT logs below
    std::unique_ptr<SDL_Context> sdlContext_;
    SDL_Renderer * renderer_ = nullptr;
    std::unique_ptr<Controller> controller_;
    int currentCommandId_ = 0;
    Color bg_{};

    Texture gearIcon_, switchOn_, switchOff_;
    Texture appTitleText_, appVersionText_, titleText_, creditText_;
    std::unique_ptr<BadgePainter> badges_;
    bool deleteRequested_ = false;
    Badge badgeA_;
    Badge badgeHold_;
    int badgeRowRightEdge_ = 0; // x just left of badge A - fixed once badgeA_ is built

    static constexpr unsigned int BHoldThresholdMs = 1000;
    bool bWasHeld_ = false;
    bool bHoldFired_ = false;

    void ComputePinnedAndThemeRanges();

    int DrawBadge(Badge & badge, int rightEdgeX);
    bool ConfirmDelete(const ScreenSnapshot & background);
    bool FinishFrame();
    void DrawChromeCommon();
    void DrawSectionTitle();
    Texture MakeScrollArrow(int x, int y) const;

    FrameEvent PollFrameEvents();
    bool ActivateCommand(Command & cmd); // true if the caller's loop should break
    void UpdateBButton(bool & tapped);

    void ResumeUnderlyingUi() const; // CONTs the game PauseUI.sh stopped - nothing else does this on power-button exit

    std::string FocusStatePath() const;
    void SaveFocusState() const;
    int LoadFocusIndex() const; // -1 if none saved

    int RunGridLayout();
    int RunListLayout();
    int RunFolderThemesLayout();
};

#endif

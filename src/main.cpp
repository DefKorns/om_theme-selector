/**
  * Copyright (c) 2026 DefKorns (https://defkorns.github.io/LICENSE)
  *
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  */

// Standalone theme manager - a dedicated Modern-UI screen for om_theme-selector,
// reusing OptionsMenu's engine (framework/ + command.cpp + localization.cpp,
// vendored as a submodule) instead of the generic `options` binary. Every
// om_theme-selector screen (root, downloads, DIY creator at every depth,
// settings) is just this same binary relaunched with a different
// --commandPath, exactly like `options` itself - the shell side already
// generates real command files per screen (om_vars' themeLoader/getThemeList/
// diyThemeChecker etc.), this only needs to read and render them.

#include "framework/sdl_helper.h"
#include "framework/controller.h"
#include "framework/powerwatch.h"
#include "framework/draw_helpers.h"
#include "framework/utf8.h"
#include "command.h"
#include "localization.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>
#include <list>
#include <dirent.h>

#ifndef MOD_VERSION
#define MOD_VERSION "dev"
#endif

static void sReplace(std::string & command, std::string oldString, std::string newString)
{
    size_t pos;
    while((pos = command.find(oldString)) != std::string::npos)
        command.replace(pos, oldString.size(), newString);
}

int main(int argc, char * argv[])
{
    const std::string optionsLocation = "/etc/options_menu/";
    std::string commandLocation = optionsLocation + "themes/commands/";
    std::string scriptLocation = optionsLocation + "themes/scripts/";
    std::string titleKey = "THEME_SELECTOR";
    bool isRootScreen = true; // no --commandPath override - this is the top of the theme-selector's own tree

    for(int i = 1; i < argc; ++i)
    {
        if(strcmp(argv[i], "--commandPath") == 0 && i+1 < argc)
        {
            commandLocation = argv[++i];
            isRootScreen = false;
        }
        else if(strcmp(argv[i], "--scriptPath") == 0 && i+1 < argc)
            scriptLocation = argv[++i];
        else if(strcmp(argv[i], "--title") == 0 && i+1 < argc)
            titleKey = argv[++i];
    }
    if(!commandLocation.empty() && commandLocation.back() != '/')
        commandLocation += '/';
    if(!scriptLocation.empty() && scriptLocation.back() != '/')
        scriptLocation += '/';

    // set only by OptionsMenu itself (main.cpp), right before running the "Theme
    // Options" row - a leftover pointer to OptionsMenu's own screen, so the root
    // of the theme selector can offer a real way back to it (see the synthesized
    // BACK row below, after commands load)
    const char * backStackEnv = getenv("OM_BACK_STACK");
    std::string backStack = (isRootScreen && backStackEnv) ? backStackEnv : "";

    LoadLanguageFromConfig(optionsLocation);
    SetTTFFontPath(optionsLocation);

    // Read commands from the real, shell-generated command folder for this screen
    std::vector<Command> commands;
    std::ifstream in;
    if(auto dir = opendir(commandLocation.c_str()))
    {
        std::list<std::string> fileList;
        while(auto entry = readdir(dir))
        {
            if(entry->d_type == DT_REG && entry->d_name[0] == 'c')
                fileList.push_back(entry->d_name);
        }
        closedir(dir);

        fileList.sort();
        for(auto & file : fileList)
        {
            in.open(commandLocation+file);
            if(in.is_open())
            {
                Command c(in);
                if(!(commands.size() == 0 && c.command.size() == 0))
                {
                    sReplace(c.command, "%options_path%", optionsLocation);
                    sReplace(c.command, "%script_dir%", scriptLocation);
                    sReplace(c.deleteCommand, "%options_path%", optionsLocation);
                    sReplace(c.deleteCommand, "%script_dir%", scriptLocation);
                    if(c.isToggle)
                    {
                        sReplace(c.stateCommand, "%options_path%", optionsLocation);
                        sReplace(c.stateCommand, "%script_dir%", scriptLocation);
                        c.UpdateState();
                    }
                    commands.push_back(c);
                }
            }
        }
    }
    else
    {
        std::cerr << "Cannot open input folder.\n";
        exit(1);
    }

    // real way back to OptionsMenu's own screen, only offered on the theme
    // selector's own root (sub-screens already have their own static Back row
    // pointing back to this root) - mirrors OptionsMenu main.cpp's own BACK
    // synthesis so returning there lands on the exact screen we came from
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

        Command back;
        back.name = "BACK";
        back.runInternal = false;
        back.restartUI = false;
        back.previewImage = optionsLocation + "images/preview_placeholder.png";
        back.command = "usleep 50000 && OM_BACK_STACK=\"" + remainingStack + "\" " + optionsLocation + "options --commandPath " + backPath
            + (backScriptPath.empty() ? "" : " --scriptPath " + backScriptPath)
            + " --title \"" + backTitleKey + "\" &";
        commands.push_back(back);
    }
    else if(!isRootScreen)
    {
        // every sub-screen used to need its own static c9999_Back file just
        // to relaunch theme_manager - synthesized here instead. Screens with
        // real cleanup to do first (e.g. the DIY sprite pickers re-enabling
        // "Preview"/"DiY" rows, or Downloads refreshing the theme list) keep
        // that as an om_return script in their own scriptLocation; this only
        // needs to know whether one exists, not what it does. Screens with
        // nothing to clean up (Settings, the DIY category list) just relaunch
        // straight back to the theme selector's own root.
        Command back;
        back.name = "BACK";
        back.runInternal = false;
        back.restartUI = false;
        back.previewImage = optionsLocation + "images/preview_placeholder.png";
        std::ifstream returnScript(scriptLocation + "om_return");
        if(returnScript.good())
            back.command = "sh " + scriptLocation + "om_return &";
        else
            back.command = "usleep 50000 && " + optionsLocation + "themes/theme_manager &";
        commands.push_back(back);
    }

    if(commands.empty())
    {
        std::cerr << "No usable commands in " << commandLocation << "\n";
        exit(1);
    }

    // BACK/EXIT rows (the synthesized one above, or a static c9999_Back file -
    // both sort last) are pinned near the footer instead of scrolling with
    // the rest of the list, matching OptionsMenu's own main.cpp
    int pinnedStartIndex = static_cast<int>(commands.size());
    while(pinnedStartIndex > 0 && (commands[pinnedStartIndex-1].name == "BACK" || commands[pinnedStartIndex-1].name == "EXIT"))
        --pinnedStartIndex;

    int currentCommandId = 0;

    SDL_Context sdl_context(std::chrono::milliseconds(33), false);
    auto renderer = sdl_context.renderer;
    Controller controller(1);

    const Uint8 bgR = UiTheme::BgR;
    const Uint8 bgG = UiTheme::BgG;
    const Uint8 bgB = UiTheme::BgB;
    SDL_SetRenderDrawColor(renderer, bgR, bgG, bgB, 0xFF);

    Texture gearIcon(optionsLocation + UiTheme::AssetGear, renderer, UiTheme::GearX, UiTheme::GearY);
    Texture switchOn(optionsLocation + UiTheme::AssetSwitchOn, renderer);
    Texture switchOff(optionsLocation + UiTheme::AssetSwitchOff, renderer);

    Texture appTitleText("Theme Manager", UiTheme::TitleFontSize, renderer, UiTheme::TitleX, UiTheme::TitleY, false, 0xFFFFFFFF, true);
    appTitleText.rect.y -= appTitleText.rect.h / 2;
    Texture appVersionText(MOD_VERSION, UiTheme::VersionFontSize, renderer, appTitleText.rect.x + appTitleText.rect.w + UiTheme::VersionGap, UiTheme::TitleY, false, 0xFFFFFFFF, true);
    appVersionText.rect.y -= appVersionText.rect.h / 2;
    Texture titleText(Translate(titleKey), UiTheme::SectionTitleFontSize, renderer, UiTheme::SectionTitleX, UiTheme::SectionTitleY, false, 0xFFFFFFFF, true);
    SDL_Rect selectedRowRect{ UiTheme::ListX, UiTheme::RowFirstY - 2, UiTheme::ListContentRightX - UiTheme::ListX, 0 };
    Texture creditText("Theme Manager - by DefKorns", 16, renderer, UiTheme::CreditX, UiTheme::CreditY, false, 0xFFFFFFFF, true);
    Texture scrollUp("^", 16, renderer, UiTheme::ScrollX, UiTheme::ScrollUpY, false, 0xFFFFFFFF, true);
    scrollUp.rect.x -= scrollUp.rect.w / 2;
    Texture scrollDown = scrollUp;
    scrollDown.rect.y = UiTheme::ScrollDownY;

    struct Badge { Texture letter; Texture label; UiTheme::BadgeColor rim; UiTheme::BadgeColor fill; };
    Badge badgeA{ Texture("A", 16, renderer, 0, 0, false, UiTheme::BadgeLetterColor, true), Texture(Translate("HINT_SELECT"), 16, renderer, 0, 0, false, 0xFFFFFFFF, true), UiTheme::BadgeADark, UiTheme::BadgeA };
    Texture badgeOuter(optionsLocation + UiTheme::AssetBadgeOuter, renderer);
    Texture badgeInner(optionsLocation + UiTheme::AssetBadgeInner, renderer);
    auto DrawBadge = [&](Badge & badge, int rightEdgeX) -> int
    {
        int groupW = UiTheme::BadgeOuterSize + UiTheme::BadgeLabelGap + badge.label.rect.w;
        int x = rightEdgeX - groupW;
        int y = UiTheme::BadgeBandY;
        badgeOuter.rect = { x, y, UiTheme::BadgeOuterSize, UiTheme::BadgeOuterSize };
        SDL_SetTextureColorMod(badgeOuter.texture.get(), badge.rim.r, badge.rim.g, badge.rim.b);
        badgeOuter.Draw(renderer);
        int innerOffset = (UiTheme::BadgeOuterSize - UiTheme::BadgeInnerSize) / 2;
        badgeInner.rect = { x+innerOffset, y+innerOffset, UiTheme::BadgeInnerSize, UiTheme::BadgeInnerSize };
        SDL_SetTextureColorMod(badgeInner.texture.get(), badge.fill.r, badge.fill.g, badge.fill.b);
        badgeInner.Draw(renderer);
        badge.letter.rect.x = x + (UiTheme::BadgeOuterSize - badge.letter.rect.w)/2;
        badge.letter.rect.y = y + (UiTheme::BadgeOuterSize - badge.letter.rect.h)/2;
        badge.letter.Draw(renderer);
        badge.label.rect.x = x + UiTheme::BadgeOuterSize + UiTheme::BadgeLabelGap;
        badge.label.rect.y = y + (UiTheme::BadgeOuterSize - badge.label.rect.h)/2;
        badge.label.Draw(renderer);
        return x - UiTheme::BadgeGroupGap;
    };

    const int RowGlyphSize = 16;
    const int RowTextGapPx = 16;
    const int ChildIndent = 4*16;
    for(Command & c : commands)
    {
        int textX = UiTheme::RowTextX + (c.child ? ChildIndent : 0);
        std::string label = Translate(c.name);

        int maxRight = UiTheme::RowControlRightX - RowTextGapPx;
        bool rowUsesTTF = CanRenderWithTTF(label, RowGlyphSize);
        int available = std::max(0, maxRight - textX);
        if(rowUsesTTF)
        {
            if(MeasureTTFWidth(label, RowGlyphSize) > available)
            {
                int n = Utf8Length(label);
                while(n > 0 && MeasureTTFWidth(TruncateUtf8(label, n) + "...", RowGlyphSize) > available)
                    --n;
                label = TruncateUtf8(label, n) + "...";
            }
        }
        else
        {
            int maxChars = available / RowGlyphSize;
            if(TruncateUtf8(label, maxChars).size() != label.size())
                label = TruncateUtf8(label, std::max(0, maxChars - 3)) + "...";
        }

        c.texture = Texture(label, RowGlyphSize, renderer, textX, 0, false, 0xFFFFFFFF, true);
    }

    const int modernRowPitch = std::max(UiTheme::RowPitch, GetTTFLineHeight(RowGlyphSize));
    int pinnedAreaTop;
    {
        int slotBottom = UiTheme::FooterDividerY - UiTheme::PinnedBottomMargin;
        for(int i = static_cast<int>(commands.size())-1; i >= pinnedStartIndex; --i)
        {
            int slotTop = slotBottom - modernRowPitch;
            commands[i].texture.rect.y = slotTop + (modernRowPitch - commands[i].texture.rect.h) / 2 + UiTheme::RowTextYNudge;
            slotBottom = slotTop;
        }
        pinnedAreaTop = slotBottom;
    }
    const int DisplayItemCount = std::max(1, (pinnedAreaTop - UiTheme::RowFirstY) / modernRowPitch);

    int topListItemNumber = 1; // forces SetCurrentCommand(0) below to lay out row Y positions the first time
    std::shared_ptr<Texture> PreviewImage;
    auto SetCurrentCommand = [&] (int newCommandId)
    {
        currentCommandId = newCommandId;
        const Command & currentCommand = commands[currentCommandId];

        // pinned rows don't affect scroll position
        int scrollTarget = std::min(currentCommandId, std::max(0, pinnedStartIndex-1));

        bool updateCommandYPos = false;
        if(scrollTarget < topListItemNumber)
        {
            topListItemNumber = scrollTarget;
            updateCommandYPos = true;
        }
        else if(scrollTarget >= topListItemNumber+DisplayItemCount)
        {
            topListItemNumber = scrollTarget-DisplayItemCount+1;
            updateCommandYPos = true;
        }
        if(updateCommandYPos)
        {
            int y = UiTheme::RowFirstY;
            for(int i = 0, count = std::min(DisplayItemCount, pinnedStartIndex-topListItemNumber); i < count; ++i)
            {
                Texture & rowTexture = commands[i+topListItemNumber].texture;
                rowTexture.rect.y = y + (modernRowPitch - rowTexture.rect.h) / 2 + UiTheme::RowTextYNudge;
                y += modernRowPitch;
            }
        }

        selectedRowRect.y = currentCommand.texture.rect.y - 2;
        selectedRowRect.h = currentCommand.texture.rect.h + 4;

        if(currentCommand.previewImage.size())
        {
            // linear by default (photo-like screenshots); nearest for small
            // pixel-art sprites blown up a lot, where linear just blurs them
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, currentCommand.previewNearest ? "0" : "1");
            PreviewImage = std::make_shared<Texture>(currentCommand.previewImage, renderer, 0, 0);
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
            if(PreviewImage->rect.w > 0 && PreviewImage->rect.h > 0)
            {
                const int maxW = UiTheme::DetailW - 2*UiTheme::ContentPadding;
                const int maxH = UiTheme::PreviewBoxH - 2*UiTheme::ContentPadding;
                double scale = std::min((double)maxW / PreviewImage->rect.w, (double)maxH / PreviewImage->rect.h);
                PreviewImage->rect.w = static_cast<int>(PreviewImage->rect.w * scale);
                PreviewImage->rect.h = static_cast<int>(PreviewImage->rect.h * scale);
            }
        }
        else
            PreviewImage.reset();
    };
    SetCurrentCommand(0);

    auto DrawRow = [&](Command & rowCommand, bool isLastOverall)
    {
        bool selected = &rowCommand == &commands[currentCommandId];
        rowCommand.texture.Draw(renderer);
        if(rowCommand.isToggle)
        {
            Texture & rowSwitch = rowCommand.stateOn ? switchOn : switchOff;
            rowSwitch.rect.x = UiTheme::SwitchRightX - UiTheme::SwitchW;
            rowSwitch.rect.y = rowCommand.texture.rect.y + (rowCommand.texture.rect.h - UiTheme::SwitchH) / 2;
            rowSwitch.Draw(renderer);
        }
        // skip for separators and the last row - nothing to separate there
        if(!selected && !rowCommand.command.empty() && !isLastOverall)
            DrawHLine(renderer, UiTheme::ListX, UiTheme::ListContentRightX, rowCommand.texture.rect.y + rowCommand.texture.rect.h + 3, UiTheme::BorderR, UiTheme::BorderG, UiTheme::BorderB);
    };

    auto DrawChrome = [&]()
    {
        DrawStrokeRect(renderer, UiTheme::OuterRect, UiTheme::BorderR, UiTheme::BorderG, UiTheme::BorderB, UiTheme::BorderWidth, UiTheme::BorderRadius);
        gearIcon.Draw(renderer);
        appTitleText.Draw(renderer);
        appVersionText.Draw(renderer);
        DrawHLine(renderer, UiTheme::HeaderDividerX, UiTheme::HeaderDividerX + UiTheme::HeaderDividerW, UiTheme::HeaderDividerY, UiTheme::BorderR, UiTheme::BorderG, UiTheme::BorderB, UiTheme::BorderWidth);
        DrawHLine(renderer, UiTheme::OuterRect.x, UiTheme::OuterRect.x + UiTheme::OuterRect.w, UiTheme::FooterDividerY, UiTheme::BorderR, UiTheme::BorderG, UiTheme::BorderB, UiTheme::BorderWidth);
        int accentBarY = titleText.rect.y + (titleText.rect.h - UiTheme::SectionAccentBarH) / 2;
        DrawFillRect(renderer, { UiTheme::SectionTitleX - UiTheme::SectionAccentBarW - 14, accentBarY, UiTheme::SectionAccentBarW, UiTheme::SectionAccentBarH }, UiTheme::AccentR, UiTheme::AccentG, UiTheme::AccentB);

        DrawRoundedFillRect(renderer, selectedRowRect, UiTheme::SelectedRowBgR, UiTheme::SelectedRowBgG, UiTheme::SelectedRowBgB, UiTheme::BoxRadius);
        DrawStrokeRect(renderer, selectedRowRect, UiTheme::AccentR, UiTheme::AccentG, UiTheme::AccentB, 2, UiTheme::BoxRadius);

        if(PreviewImage.get())
        {
            SDL_Rect previewBox{ UiTheme::DetailX, UiTheme::PreviewBoxY, UiTheme::DetailW, UiTheme::PreviewBoxH };
            DrawStrokeRect(renderer, previewBox, UiTheme::BorderR, UiTheme::BorderG, UiTheme::BorderB, UiTheme::BorderWidth, UiTheme::BoxRadius);
            PreviewImage->rect.x = previewBox.x + (previewBox.w - PreviewImage->rect.w) / 2;
            PreviewImage->rect.y = previewBox.y + (previewBox.h - PreviewImage->rect.h) / 2;
            PreviewImage->Draw(renderer);
        }

        // B has nothing to do on any screen here - navigation is always via a
        // real "Back" row in the list (like every other row), not a shortcut
        DrawBadge(badgeA, UiTheme::BadgeClusterRightX);
    };

    for(;;)
    {
        sdl_context.StartFrame();
        controller.Update();

        SDL_Event e;
        while(SDL_PollEvent(&e))
            if(e.type == SDL_QUIT)
                return 0;

        if(sdl_context.powerwatch->buttonPress())
            break;

        if(controller.GetButtonStatus(A) || controller.GetButtonStatus(START))
        {
            if(commands[currentCommandId].runInternal)
            {
                commands[currentCommandId].RunCommand(sdl_context, &controller, { gearIcon, appTitleText, appVersionText, creditText }, bgR, bgG, bgB);
                if(commands[currentCommandId].isToggle)
                    commands[currentCommandId].UpdateState();
            }
            else
            {
                system(commands[currentCommandId].command.c_str());
                break;
            }
        }
        else if(controller.GetButtonStatus(UP))
        {
            // bounded so an all-separator list can't spin forever
            int newCommandId = currentCommandId;
            for(size_t tries = 0; tries < commands.size(); ++tries)
            {
                newCommandId = (newCommandId-1+commands.size())%commands.size();
                if(commands[newCommandId].command.size() != 0)
                    break;
            }
            SetCurrentCommand(newCommandId);
        }
        else if(controller.GetButtonStatus(DOWN))
        {
            int newCommandId = currentCommandId;
            for(size_t tries = 0; tries < commands.size(); ++tries)
            {
                newCommandId = (newCommandId+1)%commands.size();
                if(commands[newCommandId].command.size() != 0)
                    break;
            }
            SetCurrentCommand(newCommandId);
        }
        // tap B = jump straight to the last row (BACK here, EXIT on the real
        // OptionsMenu) - matches main.cpp's own B behavior
        else if(controller.GetButtonStatus(B))
        {
            SetCurrentCommand(commands.size()-1);
        }

        DrawChrome();
        titleText.Draw(renderer);

        int lastCommandIndex = static_cast<int>(commands.size()) - 1;
        for(int i = 0, count = std::min(DisplayItemCount, pinnedStartIndex-topListItemNumber); i < count; ++i)
            DrawRow(commands[i+topListItemNumber], (i+topListItemNumber) == lastCommandIndex);

        // pinned trailing rows (Back/Exit) - always visible near the footer
        for(int i = pinnedStartIndex; i <= lastCommandIndex; ++i)
            DrawRow(commands[i], i == lastCommandIndex);

        creditText.Draw(renderer);

        // bounded by pinnedStartIndex, not commands.size() - pinned rows need no scroll
        if(topListItemNumber != 0)
            scrollUp.Draw(renderer);
        if((topListItemNumber + DisplayItemCount) < pinnedStartIndex)
            scrollDown.Draw(renderer, SDL_FLIP_VERTICAL);

        SDL_SetRenderDrawColor(renderer, bgR, bgG, bgB, 0xFF);

        sdl_context.EndFrame();
    }

    return 0;
}

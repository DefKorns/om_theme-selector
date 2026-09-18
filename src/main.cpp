/**
  * Copyright (c) 2026 DefKorns (https://defkorns.github.io/LICENSE)
  *
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  */

// Standalone theme manager for om_theme-selector, reusing OptionsMenu's
// engine (framework/ + command.cpp + localization.cpp, vendored submodule)
// instead of the generic `options` binary. Every screen is this same binary
// relaunched with a different --commandPath, reading command files the
// shell side already generates.
//
// --layout list (default): single-column option picker.
// --layout grid: fixed actions as a chip strip, everything else (themes,
// DIY assets) as a scrollable tile grid.

#include "framework/sdl_helper.h"
#include "framework/controller.h"
#include "framework/powerwatch.h"
#include "framework/draw_helpers.h"
#include "framework/utf8.h"
#include "command.h"
#include "localization.h"

#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>
#include <list>
#include <dirent.h>
#include <unistd.h>

#ifndef MOD_VERSION
#define MOD_VERSION "dev"
#endif

static void sReplace(std::string & command, std::string oldString, std::string newString)
{
    size_t pos;
    while((pos = command.find(oldString)) != std::string::npos)
        command.replace(pos, oldString.size(), newString);
}

// scale-to-fit-centered
static void FitCentered(Texture & tex, int boxX, int boxY, int boxW, int boxH, int padding)
{
    const int maxW = boxW - 2*padding;
    const int maxH = boxH - 2*padding;
    if(tex.rect.w <= 0 || tex.rect.h <= 0)
        return;
    double scale = std::min((double)maxW / tex.rect.w, (double)maxH / tex.rect.h);
    tex.rect.w = static_cast<int>(tex.rect.w * scale);
    tex.rect.h = static_cast<int>(tex.rect.h * scale);
    tex.rect.x = boxX + (boxW - tex.rect.w) / 2;
    tex.rect.y = boxY + (boxH - tex.rect.h) / 2;
}

// dark gradient over the tile's bottom edge, solid for the lower ~60% (where
// the overlaid label sits) and fading to transparent above that, so the
// label stays legible over any image - SDL has no gradient fill primitive,
// so this is a stack of alpha-stepped bands
static void DrawBottomScrim(SDL_Renderer * renderer, int x, int y, int w, int h)
{
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    const int bands = 24;
    const Uint8 maxAlpha = 235;
    const double rampFrac = 0.4; // reaches maxAlpha by 40% down the scrim, solid for the rest - the label sits in that solid tail, not the fade
    for(int i = 0; i < bands; ++i)
    {
        double t = (double)i / (bands - 1); // 0 at top of scrim, 1 at bottom
        double ramp = std::min(1.0, t / rampFrac);
        Uint8 alpha = static_cast<Uint8>(maxAlpha * ramp);
        SDL_Rect band{ x, y + h * i / bands, w, h / bands + 1 };
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, alpha);
        SDL_RenderFillRect(renderer, &band);
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

// scale-to-cover-centered (crops the overflow) - pair with a clip rect at
// (boxX+padding, boxY+padding, boxW-2*padding, boxH-2*padding) so the
// overflow doesn't bleed past the box, and DrawRoundedCornerMask afterwards
// to fake-round that clip rect's square corners back to match a rounded
// container
static void FitCover(Texture & tex, int boxX, int boxY, int boxW, int boxH, int padding)
{
    const int maxW = boxW - 2*padding;
    const int maxH = boxH - 2*padding;
    if(tex.rect.w <= 0 || tex.rect.h <= 0)
        return;
    double scale = std::max((double)maxW / tex.rect.w, (double)maxH / tex.rect.h);
    tex.rect.w = static_cast<int>(tex.rect.w * scale);
    tex.rect.h = static_cast<int>(tex.rect.h * scale);
    tex.rect.x = boxX + (boxW - tex.rect.w) / 2;
    tex.rect.y = boxY + (boxH - tex.rect.h) / 2;
}

static std::string TruncateToWidth(const std::string & text, int glyphSize, int available)
{
    std::string label = text;
    if(CanRenderWithTTF(label, glyphSize))
    {
        if(MeasureTTFWidth(label, glyphSize) > available)
        {
            int n = Utf8Length(label);
            while(n > 0 && MeasureTTFWidth(TruncateUtf8(label, n) + "...", glyphSize) > available)
                --n;
            label = TruncateUtf8(label, n) + "...";
        }
    }
    else
    {
        int maxChars = available / glyphSize;
        if(TruncateUtf8(label, maxChars).size() != label.size())
            label = TruncateUtf8(label, std::max(0, maxChars - 3)) + "...";
    }
    return label;
}

// only one theme_manager should ever hold /dev/fb0 at a time - two screens
// (e.g. a stuck/crashed instance plus a freshly-launched one) fight over it
// and both flicker. Takes over rather than refusing to start, so a lock left
// behind by a crash never blocks every future launch.
static const char * LockPath = "/tmp/theme_manager.lock";

static void ReleaseSingleInstanceLock()
{
    remove(LockPath);
}

static void AcquireSingleInstanceLock()
{
    std::ifstream in(LockPath);
    pid_t oldPid = 0;
    if(in.good())
        in >> oldPid;
    in.close();
    if(oldPid > 0 && oldPid != getpid() && kill(oldPid, 0) == 0)
    {
        kill(oldPid, SIGTERM);
        for(int i = 0; i < 40 && kill(oldPid, 0) == 0; ++i)
            usleep(50000);
        if(kill(oldPid, 0) == 0)
            kill(oldPid, SIGKILL);
    }
    std::ofstream(LockPath, std::ios::trunc) << getpid();
    atexit(ReleaseSingleInstanceLock);
    signal(SIGTERM, [](int) { exit(0); }); // so a newer instance's takeover runs our atexit cleanup too
}

int main(int argc, char * argv[])
{
    AcquireSingleInstanceLock();

    const std::string optionsLocation = "/etc/options_menu/";
    std::string commandLocation = optionsLocation + "themes/commands/";
    std::string scriptLocation = optionsLocation + "themes/scripts/";
    std::string titleKey = "THEME_SELECTOR";
    bool isRootScreen = true; // no --commandPath override - this is the top of the theme-selector's own tree
    bool gridLayout = false;

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
        else if(strcmp(argv[i], "--layout") == 0 && i+1 < argc)
            gridLayout = (std::string(argv[++i]) == "grid");
    }
    if(!commandLocation.empty() && commandLocation.back() != '/')
        commandLocation += '/';
    if(!scriptLocation.empty() && scriptLocation.back() != '/')
        scriptLocation += '/';

    // set by OptionsMenu before launching the "Theme Options" row - lets the
    // root screen offer a real way back to it
    const char * backStackEnv = getenv("OM_BACK_STACK");
    std::string backStack = (isRootScreen && backStackEnv) ? backStackEnv : "";

    LoadLanguageFromConfig(optionsLocation);
    SetTTFFontPath(optionsLocation);

    // isThemeItem: false for a "c0000_*" fixed action, true for a theme/asset
    // row - only used by grid layout, to split strip vs. tile
    std::vector<Command> commands;
    std::vector<bool> isThemeItem;
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
                // c0000_0000: leading empty sentinel, always skipped. A
                // later empty COMMAND_STR is a reserved blank slot instead -
                // kept and skipped by the strip/grid draw loops below
                if(commands.empty() && c.command.empty())
                    continue;
                if(!c.command.empty())
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
                }
                commands.push_back(c);
                isThemeItem.push_back(file.compare(0, 6, "c0000_") != 0);
            }
        }
    }
    else
    {
        std::cerr << "Cannot open input folder.\n";
        exit(1);
    }

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
        isThemeItem.push_back(false);
    }
    else if(!isRootScreen)
    {
        // use the screen's own om_return for cleanup if it has one, else
        // just relaunch straight back to root
        Command back;
        back.name = "BACK";
        back.runInternal = false;
        back.restartUI = false;
        back.previewImage = optionsLocation + "images/preview_placeholder.png";
        std::ifstream returnScript(scriptLocation + "om_return");
        if(returnScript.good())
            back.command = "sh " + scriptLocation + "om_return &";
        else
            back.command = "usleep 50000 && " + optionsLocation + "themes/theme_manager --title INSTALLED_THEMES --layout grid &";
        commands.push_back(back);
        isThemeItem.push_back(false);
    }

    if(commands.empty())
    {
        std::cerr << "No usable commands in " << commandLocation << "\n";
        exit(1);
    }

    // BACK/EXIT sort last - pin them near the footer instead of scrolling
    int pinnedStartIndex = static_cast<int>(commands.size());
    while(pinnedStartIndex > 0 && (commands[pinnedStartIndex-1].name == "BACK" || commands[pinnedStartIndex-1].name == "EXIT"))
        --pinnedStartIndex;

    // grid layout only: first non-fixed-action row (themeStart==pinnedStartIndex
    // for an empty list - still renders as grid, just with an empty grid area)
    int themeStart = 0;
    while(themeStart < pinnedStartIndex && !isThemeItem[themeStart])
        ++themeStart;

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
    Texture creditText("Theme Manager - by DefKorns", 16, renderer, UiTheme::CreditX, UiTheme::CreditY, false, 0xFFFFFFFF, true);

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

    // hold-to-delete hint, different color than the real B badge
    Badge badgeHold{ Texture("B", 16, renderer, 0, 0, false, UiTheme::BadgeLetterColor, true), Texture(Translate("HINT_DELETE"), 16, renderer, 0, 0, false, 0xFFFFFFFF, true), UiTheme::BadgeXDark, UiTheme::BadgeX };

    // true if deleted - caller should break out of the main loop
    auto ConfirmDelete = [&]() -> bool
    {
        const std::string & confirmKey = commands[currentCommandId].deleteConfirmKey;
        Texture confirmTitle(Translate(confirmKey.empty() ? "DELETE_CONFIRM_GENERIC" : confirmKey), 24, renderer, 640, 320, true, 0xFFFFFFFF, true);
        Texture confirmHint(Translate("DELETE_CONFIRM_HINT"), 16, renderer, 640, 360, true, 0xFFFFFFFF, true);
        controller.GetButtonStatus(B); // consume the still-held B from the triggering long-press
        bool confirmed = false;
        for(;;)
        {
            controller.Update();
            if(controller.GetButtonStatus(B))
                break;
            if(controller.GetButtonStatus(A) || controller.GetButtonStatus(START))
            {
                confirmed = true;
                break;
            }
            sdl_context.StartFrame();
            DrawFillRect(renderer, UiTheme::FrameRect, UiTheme::BgR, UiTheme::BgG, UiTheme::BgB);
            DrawStrokeRect(renderer, UiTheme::FrameRect, UiTheme::BorderR, UiTheme::BorderG, UiTheme::BorderB, UiTheme::BorderWidth, UiTheme::BorderRadius);
            confirmTitle.Draw(renderer);
            confirmHint.Draw(renderer);
            SDL_SetRenderDrawColor(renderer, bgR, bgG, bgB, 0xFF); // flat helpers leave the draw color dirty
            sdl_context.EndFrame();
        }
        if(!confirmed)
            return false;
        system(commands[currentCommandId].deleteCommand.c_str());
        return true;
    };

    const unsigned int bHoldThresholdMs = 1000;
    bool bWasHeld = false, bHoldFired = false;

    // shared chrome; section title is separate since grid positions it lower
    auto DrawChromeCommon = [&]()
    {
        DrawStrokeRect(renderer, UiTheme::OuterRect, UiTheme::BorderR, UiTheme::BorderG, UiTheme::BorderB, UiTheme::BorderWidth, UiTheme::BorderRadius);
        gearIcon.Draw(renderer);
        appTitleText.Draw(renderer);
        appVersionText.Draw(renderer);
        DrawHLine(renderer, UiTheme::HeaderDividerX, UiTheme::HeaderDividerX + UiTheme::HeaderDividerW, UiTheme::HeaderDividerY, UiTheme::BorderR, UiTheme::BorderG, UiTheme::BorderB, UiTheme::BorderWidth);
        DrawHLine(renderer, UiTheme::OuterRect.x, UiTheme::OuterRect.x + UiTheme::OuterRect.w, UiTheme::FooterDividerY, UiTheme::BorderR, UiTheme::BorderG, UiTheme::BorderB, UiTheme::BorderWidth);
        DrawBadge(badgeA, UiTheme::BadgeClusterRightX);
        creditText.Draw(renderer);
    };
    auto DrawSectionTitle = [&]()
    {
        int accentBarY = titleText.rect.y + (titleText.rect.h - UiTheme::SectionAccentBarH) / 2;
        DrawFillRect(renderer, { UiTheme::SectionTitleX - UiTheme::SectionAccentBarW - 14, accentBarY, UiTheme::SectionAccentBarW, UiTheme::SectionAccentBarH }, UiTheme::AccentR, UiTheme::AccentG, UiTheme::AccentB);
        titleText.Draw(renderer);
    };

    if(gridLayout)
    {
        // ============================= GRID LAYOUT =============================
        // squareTiles: pack more/narrower columns for square-ish previews,
        // unless the category overrides the column count itself
        const bool squareTiles = themeStart < pinnedStartIndex && commands[themeStart].previewSquare;
        const int explicitCols = themeStart < pinnedStartIndex ? commands[themeStart].previewGridCols : -1;
        const int GridCols = explicitCols > 0 ? explicitCols : (squareTiles ? 7 : 4);
        const int GridGap = 20;

        // strip = fixed actions [0,themeStart). An empty entry stays a
        // reserved-width slot rather than being compacted out, so a
        // conditional button appearing/disappearing doesn't shift the rest.
        // stripSlots drives layout; stripIndices is the navigable subset -
        // the D-pad skips blank slots. Back/Exit aren't chips here - hinted
        // in the footer instead (badgeB)
        std::vector<int> stripSlots;
        for(int i = 0; i < themeStart; ++i)
            stripSlots.push_back(i);
        const int stripSlotCount = static_cast<int>(stripSlots.size());
        std::vector<int> stripIndices;
        for(int i : stripSlots)
            if(!commands[i].command.empty())
                stripIndices.push_back(i);
        const int stripCount = static_cast<int>(stripIndices.size());
        auto StripToCommand = [&](int pos) -> int { return stripIndices[pos]; };
        auto CommandToStrip = [&](int idx) -> int
        {
            for(int p = 0; p < stripCount; ++p)
                if(stripIndices[p] == idx)
                    return p;
            return 0;
        };
        const int chipGap = 14;
        const int chipIconSize = 22; // small, matches the 14px label's own scale

        // no strip on a pure-tile screen (e.g. a DIY category) - use the row for a 3rd grid row
        const bool hasStrip = stripSlotCount > 0;
        const int StripTop = UiTheme::HeaderDividerY + 22;
        const int StripH = hasStrip ? 58 : 0;
        const int GridLeft = UiTheme::FrameX + 32;
        const int GridRight = UiTheme::FrameX + UiTheme::FrameW - 32;
        // capped so a strip with few slots doesn't stretch its chips wide
        const int MaxChipW = 200;
        const int chipW = hasStrip ? std::min(MaxChipW, (GridRight - GridLeft - (stripSlotCount-1)*chipGap) / stripSlotCount) : 0;
        titleText = Texture(Translate(titleKey), UiTheme::SectionTitleFontSize - 6, renderer, UiTheme::SectionTitleX, 0, false, 0xFFFFFFFF, true);
        titleText.rect.y = hasStrip ? (StripTop + StripH + 22) : (UiTheme::HeaderDividerY + 22);
        const int GridTop = titleText.rect.y + titleText.rect.h + 20;
        const int GridBottom = UiTheme::FooterDividerY - 14;
        const int TileW = (GridRight - GridLeft - (GridCols-1)*GridGap) / GridCols;
        int TileH, GridRowsVisible;
        if(hasStrip)
        {
            TileH = TileW * 9 / 16;
            GridRowsVisible = std::max(1, (GridBottom - GridTop + GridGap) / (TileH + GridGap));
        }
        else
        {
            GridRowsVisible = 3;
            TileH = (GridBottom - GridTop - (GridRowsVisible-1)*GridGap) / GridRowsVisible;
        }
        const int GridRowPitch = TileH + GridGap;

        Texture gridScrollUp("^", 16, renderer, GridRight + 22, GridTop + 24, false, 0xFFFFFFFF, true);
        gridScrollUp.rect.x -= gridScrollUp.rect.w / 2;
        Texture gridScrollDown = gridScrollUp;
        gridScrollDown.rect.y = GridBottom - 24 - gridScrollDown.rect.h;

        // Back/Exit hint, footer badge next to A's - B runs it directly
        Badge badgeB{ Texture("B", 16, renderer, 0, 0, false, UiTheme::BadgeLetterColor, true),
                      Texture(pinnedStartIndex < (int)commands.size() ? Translate(commands[pinnedStartIndex].name) : "", 16, renderer, 0, 0, false, 0xFFFFFFFF, true),
                      UiTheme::BadgeBDark, UiTheme::BadgeB };

        std::vector<Texture> chipIcons(commands.size());
        std::vector<Texture> chipLabels(commands.size());
        std::vector<Texture> tileImages(commands.size());
        std::vector<Texture> tileLabels(commands.size());
        for(int i = 0; i < (int)commands.size(); ++i)
        {
            Command & c = commands[i];
            if(c.command.empty())
                continue; // reserved blank slot, not a chip or tile
            std::string label = Translate(c.name);
            if(i < themeStart || i >= pinnedStartIndex)
            {
                int labelAvail = chipW - chipIconSize - 24;
                if(c.previewImage.size())
                {
                    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1"); // smooth - these are small glyphs, nearest looks jagged
                    Texture icon(c.previewImage, renderer, 0, 0);
                    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
                    FitCentered(icon, 0, 0, chipIconSize, chipIconSize, 0);
                    SDL_SetTextureColorMod(icon.texture.get(), UiTheme::AccentR, UiTheme::AccentG, UiTheme::AccentB);
                    chipIcons[i] = icon;
                    labelAvail = chipW - chipIconSize - 32;
                }
                label = TruncateToWidth(label, 14, labelAvail);
                chipLabels[i] = Texture(label, 14, renderer, 0, 0, false, 0xFFFFFFFF, true);
            }
            else
            {
                // tile image loaded lazily, see EnsureTileImage below -
                // overlaid on the image's scrim, left-aligned with margins
                // on both sides (see the draw loop)
                label = TruncateToWidth(label, 15, TileW - 28);
                tileLabels[i] = Texture(label, 15, renderer, 0, 0, false, 0xFFFFFFFF, true);
            }
        }
        std::vector<bool> tileImageLoaded(commands.size(), false);
        auto EnsureTileImage = [&](int idx)
        {
            if(tileImageLoaded[idx])
                return;
            tileImageLoaded[idx] = true;
            const Command & c = commands[idx];
            if(c.previewImage.empty())
                return;
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, c.previewNearest ? "0" : "1");
            Texture art(c.previewImage, renderer, 0, 0);
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
            // fills the whole tile - matches the per-frame FitCover below
            FitCover(art, 0, 0, TileW, TileH, 0);
            tileImages[idx] = art;
        };

        int gridTopRow = 0;
        auto SetCurrentCommand = [&](int newId)
        {
            currentCommandId = newId;
            if(newId >= themeStart && newId < pinnedStartIndex)
            {
                int row = (newId - themeStart) / GridCols;
                if(row < gridTopRow) gridTopRow = row;
                else if(row >= gridTopRow + GridRowsVisible) gridTopRow = row - GridRowsVisible + 1;
            }
        };
        SetCurrentCommand(themeStart < pinnedStartIndex ? themeStart : (stripCount > 0 ? StripToCommand(0) : 0));

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

            bool inGrid = currentCommandId >= themeStart && currentCommandId < pinnedStartIndex;

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
            else if(controller.HeldRepeat(LEFT))
            {
                if(inGrid && currentCommandId > themeStart)
                    SetCurrentCommand(currentCommandId - 1);
                else if(!inGrid && stripCount > 0)
                    SetCurrentCommand(StripToCommand(std::max(0, CommandToStrip(currentCommandId) - 1)));
            }
            else if(controller.HeldRepeat(RIGHT))
            {
                if(inGrid && currentCommandId < pinnedStartIndex - 1)
                    SetCurrentCommand(currentCommandId + 1);
                else if(!inGrid && stripCount > 0)
                    SetCurrentCommand(StripToCommand(std::min(stripCount - 1, CommandToStrip(currentCommandId) + 1)));
            }
            else if(controller.HeldRepeat(UP))
            {
                if(inGrid)
                {
                    int col = (currentCommandId - themeStart) % GridCols;
                    if(currentCommandId - themeStart < GridCols) // top grid row - jump up to the strip
                    {
                        if(stripCount > 0)
                            SetCurrentCommand(StripToCommand(std::min(stripCount - 1, col)));
                    }
                    else
                        SetCurrentCommand(currentCommandId - GridCols);
                }
            }
            else if(controller.HeldRepeat(DOWN))
            {
                if(!inGrid && themeStart < pinnedStartIndex)
                {
                    int col = std::min(GridCols - 1, CommandToStrip(currentCommandId));
                    SetCurrentCommand(std::min(pinnedStartIndex - 1, themeStart + col));
                }
                else if(inGrid && currentCommandId + GridCols < pinnedStartIndex)
                    SetCurrentCommand(currentCommandId + GridCols);
            }
            // B tracked outside the else-if chain, independent of pinned Back/Exit
            {
                bool bHeldNow = controller.PeekButtonStatus(B);
                if(bHeldNow)
                {
                    if(!bHoldFired && controller.HeldMillis(B) >= bHoldThresholdMs)
                    {
                        bHoldFired = true;
                        if(!commands[currentCommandId].deleteCommand.empty() && ConfirmDelete())
                            break;
                    }
                }
                else
                {
                    if(bWasHeld && !bHoldFired && pinnedStartIndex < (int)commands.size())
                    {
                        // quick tap - Back/Exit isn't a chip, B runs it directly
                        Command & backCmd = commands[pinnedStartIndex];
                        if(backCmd.runInternal)
                        {
                            backCmd.RunCommand(sdl_context, &controller, { gearIcon, appTitleText, appVersionText, creditText }, bgR, bgG, bgB);
                            if(backCmd.isToggle)
                                backCmd.UpdateState();
                        }
                        else
                        {
                            system(backCmd.command.c_str());
                            break;
                        }
                    }
                    bHoldFired = false;
                }
                bWasHeld = bHeldNow;
            }

            DrawChromeCommon();
            DrawSectionTitle();
            {
                int rightEdge = UiTheme::BadgeClusterRightX - UiTheme::BadgeOuterSize - UiTheme::BadgeLabelGap - badgeA.label.rect.w - UiTheme::BadgeGroupGap;
                if(pinnedStartIndex < (int)commands.size())
                    rightEdge = DrawBadge(badgeB, rightEdge);
                if(!commands[currentCommandId].deleteCommand.empty())
                    DrawBadge(badgeHold, rightEdge);
            }

            // action strip
            {
                int x = GridLeft;
                for(int pos = 0; pos < stripSlotCount; ++pos)
                {
                    int idx = stripSlots[pos];
                    if(commands[idx].command.empty())
                    {
                        x += chipW + chipGap; // reserved gap - just leave it blank
                        continue;
                    }
                    bool selected = idx == currentCommandId;
                    SDL_Rect chipRect{ x, StripTop, chipW, StripH };
                    if(selected)
                    {
                        DrawRoundedFillRect(renderer, chipRect, UiTheme::SelectedRowBgR, UiTheme::SelectedRowBgG, UiTheme::SelectedRowBgB, UiTheme::BoxRadius);
                        DrawStrokeRect(renderer, chipRect, UiTheme::AccentR, UiTheme::AccentG, UiTheme::AccentB, 2, UiTheme::BoxRadius);
                    }
                    else
                        DrawStrokeRect(renderer, chipRect, UiTheme::BorderR, UiTheme::BorderG, UiTheme::BorderB, UiTheme::BorderWidth, UiTheme::BoxRadius);

                    // icon+label centered as one group, not each alone
                    Texture & icon = chipIcons[idx];
                    Texture & label = chipLabels[idx];
                    int iconW = icon.rect.h > 0 ? chipIconSize : 0;
                    int groupW = iconW + (iconW > 0 ? 12 : 0) + label.rect.w;
                    int gx = x + (chipW - groupW) / 2;
                    if(icon.rect.h > 0)
                    {
                        icon.rect.x = gx + (chipIconSize - icon.rect.w) / 2;
                        icon.rect.y = StripTop + (StripH - icon.rect.h) / 2;
                        icon.Draw(renderer);
                        gx += iconW + 12;
                    }
                    label.rect.x = gx;
                    label.rect.y = StripTop + (StripH - label.rect.h) / 2;
                    label.Draw(renderer);

                    x += chipW + chipGap;
                }
            }

            // theme/asset grid
            {
                int lastThemeIndex = pinnedStartIndex - 1;
                for(int row = 0; row < GridRowsVisible; ++row)
                {
                    int rowStart = themeStart + (gridTopRow+row)*GridCols;
                    if(rowStart > lastThemeIndex)
                        break;
                    int y = GridTop + row*GridRowPitch;
                    for(int col = 0; col < GridCols; ++col)
                    {
                        int idx = rowStart + col;
                        if(idx > lastThemeIndex)
                            break;
                        if(commands[idx].command.empty())
                            continue; // reserved blank cell, not a tile
                        int x = GridLeft + col*(TileW+GridGap);
                        bool selected = idx == currentCommandId;
                        SDL_Rect tileRect{ x, y, TileW, TileH };
                        DrawRoundedFillRect(renderer, tileRect, UiTheme::SelectedRowBgR, UiTheme::SelectedRowBgG, UiTheme::SelectedRowBgB, UiTheme::BoxRadius);
                        EnsureTileImage(idx);
                        if(tileImages[idx].rect.w > 0)
                        {
                            // cover-fills the whole tile (crops overflow)
                            // instead of letterboxing, touching all 4 edges -
                            // clip to the tile so the crop doesn't bleed into
                            // neighboring tiles, then mask the clip's square
                            // corners back to rounded
                            SDL_RenderSetClipRect(renderer, &tileRect);
                            FitCover(tileImages[idx], x, y, TileW, TileH, 0);
                            tileImages[idx].Draw(renderer);
                            const int ScrimH = 56;
                            DrawBottomScrim(renderer, x, y + TileH - ScrimH, TileW, ScrimH);
                            SDL_RenderSetClipRect(renderer, nullptr);
                            DrawRoundedCornerMask(renderer, tileRect, UiTheme::SelectedRowBgR, UiTheme::SelectedRowBgG, UiTheme::SelectedRowBgB, UiTheme::BoxRadius);
                        }
                        DrawStrokeRect(renderer, tileRect, selected ? UiTheme::AccentR : UiTheme::BorderR, selected ? UiTheme::AccentG : UiTheme::BorderG, selected ? UiTheme::AccentB : UiTheme::BorderB, selected ? 3 : UiTheme::BorderWidth, UiTheme::BoxRadius);
                        // overlaid on the scrim, left-aligned with a small margin
                        tileLabels[idx].rect.x = x + 14;
                        tileLabels[idx].rect.y = y + TileH - tileLabels[idx].rect.h - 12;
                        tileLabels[idx].Draw(renderer);
                    }
                }

                int totalRows = (lastThemeIndex - themeStart + GridCols) / GridCols;
                if(gridTopRow > 0)
                    gridScrollUp.Draw(renderer);
                if(gridTopRow + GridRowsVisible < totalRows)
                    gridScrollDown.Draw(renderer, SDL_FLIP_VERTICAL);
            }

            SDL_SetRenderDrawColor(renderer, bgR, bgG, bgB, 0xFF);
            sdl_context.EndFrame();
        }

        return 0;
    }

    // ============================= LIST LAYOUT =============================
    Texture scrollUp("^", 16, renderer, UiTheme::ScrollX, UiTheme::ScrollUpY, false, 0xFFFFFFFF, true);
    scrollUp.rect.x -= scrollUp.rect.w / 2;
    Texture scrollDown = scrollUp;
    scrollDown.rect.y = UiTheme::ScrollDownY;
    SDL_Rect selectedRowRect{ UiTheme::ListX, UiTheme::RowFirstY - 2, UiTheme::ListContentRightX - UiTheme::ListX, 0 };

    const int RowGlyphSize = 16;
    const int RowTextGapPx = 16;
    const int ChildIndent = 4*16;
    for(Command & c : commands)
    {
        int textX = UiTheme::RowTextX + (c.child ? ChildIndent : 0);
        std::string label = TruncateToWidth(Translate(c.name), RowGlyphSize, UiTheme::RowControlRightX - RowTextGapPx - textX);
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
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, currentCommand.previewNearest ? "0" : "1");
            PreviewImage = std::make_shared<Texture>(currentCommand.previewImage, renderer, 0, 0);
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
            FitCentered(*PreviewImage, UiTheme::DetailX, UiTheme::PreviewBoxY, UiTheme::DetailW, UiTheme::PreviewBoxH, UiTheme::ContentPadding);
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
        DrawChromeCommon();
        DrawSectionTitle();
        if(!commands[currentCommandId].deleteCommand.empty())
            DrawBadge(badgeHold, UiTheme::BadgeClusterRightX - UiTheme::BadgeOuterSize - UiTheme::BadgeLabelGap - badgeA.label.rect.w - UiTheme::BadgeGroupGap);

        DrawRoundedFillRect(renderer, selectedRowRect, UiTheme::SelectedRowBgR, UiTheme::SelectedRowBgG, UiTheme::SelectedRowBgB, UiTheme::BoxRadius);
        DrawStrokeRect(renderer, selectedRowRect, UiTheme::AccentR, UiTheme::AccentG, UiTheme::AccentB, 2, UiTheme::BoxRadius);

        if(PreviewImage.get())
        {
            SDL_Rect previewBox{ UiTheme::DetailX, UiTheme::PreviewBoxY, UiTheme::DetailW, UiTheme::PreviewBoxH };
            DrawStrokeRect(renderer, previewBox, UiTheme::BorderR, UiTheme::BorderG, UiTheme::BorderB, UiTheme::BorderWidth, UiTheme::BoxRadius);
            PreviewImage->Draw(renderer);
        }
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
        else if(controller.HeldRepeat(UP))
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
        else if(controller.HeldRepeat(DOWN))
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
        // B tracked outside the else-if chain: tap = last row, hold ~1s = delete
        bool bHeldNow = controller.PeekButtonStatus(B);
        if(bHeldNow)
        {
            if(!bHoldFired && controller.HeldMillis(B) >= bHoldThresholdMs)
            {
                bHoldFired = true;
                if(!commands[currentCommandId].deleteCommand.empty() && ConfirmDelete())
                    break;
            }
        }
        else
        {
            if(bWasHeld && !bHoldFired)
                SetCurrentCommand(commands.size()-1);
            bHoldFired = false;
        }
        bWasHeld = bHeldNow;

        DrawChrome();

        int lastCommandIndex = static_cast<int>(commands.size()) - 1;
        for(int i = 0, count = std::min(DisplayItemCount, pinnedStartIndex-topListItemNumber); i < count; ++i)
            DrawRow(commands[i+topListItemNumber], (i+topListItemNumber) == lastCommandIndex);

        // pinned trailing rows (Back/Exit) - always visible near the footer
        for(int i = pinnedStartIndex; i <= lastCommandIndex; ++i)
            DrawRow(commands[i], i == lastCommandIndex);

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

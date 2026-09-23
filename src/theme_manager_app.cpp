/**
  * Copyright (c) 2026 DefKorns (https://defkorns.github.io/LICENSE)
  *
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  */

#include "theme_manager_app.h"
#include "texture_utils.h"
#include "framework/draw_helpers.h"
#include "framework/powerwatch.h"
#include "localization.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>

#ifndef MOD_VERSION
#define MOD_VERSION "dev"
#endif

ThemeManagerApp::ThemeManagerApp(std::string optionsLocation, AppOptions options, std::vector<Command> commands, std::vector<bool> isThemeItem)
    : optionsLocation_(std::move(optionsLocation))
    , options_(std::move(options))
    , commands_(std::move(commands))
    , isThemeItem_(std::move(isThemeItem))
{
    fprintf(stderr, "CHECKPOINT 1: commands loaded, count=%zu\n", commands_.size()); fflush(stderr);

    // std::make_unique is C++14; this project builds as C++11
    sdlContext_.reset(new SDL_Context(std::chrono::milliseconds(33), false));
    fprintf(stderr, "CHECKPOINT 2: sdl_context created\n"); fflush(stderr);
    renderer_ = sdlContext_->renderer;
    controller_.reset(new Controller(1));
    fprintf(stderr, "CHECKPOINT 3: controller created\n"); fflush(stderr);

    bgR_ = UiTheme::BgR;
    bgG_ = UiTheme::BgG;
    bgB_ = UiTheme::BgB;
    SDL_SetRenderDrawColor(renderer_, bgR_, bgG_, bgB_, 0xFF);

    gearIcon_ = Texture(optionsLocation_ + UiTheme::AssetGear, renderer_, UiTheme::GearX, UiTheme::GearY);
    switchOn_ = Texture(optionsLocation_ + UiTheme::AssetSwitchOn, renderer_);
    switchOff_ = Texture(optionsLocation_ + UiTheme::AssetSwitchOff, renderer_);

    appTitleText_ = Texture("Theme Manager", UiTheme::TitleFontSize, renderer_, UiTheme::TitleX, UiTheme::TitleY, false, UiTheme::TextColor, true);
    appTitleText_.rect.y -= appTitleText_.rect.h / 2;
    appVersionText_ = Texture(MOD_VERSION, UiTheme::VersionFontSize, renderer_, appTitleText_.rect.x + appTitleText_.rect.w + UiTheme::VersionGap, UiTheme::TitleY, false, UiTheme::TextColor, true);
    appVersionText_.rect.y -= appVersionText_.rect.h / 2;
    titleText_ = Texture(Translate(options_.titleKey), UiTheme::SectionTitleFontSize, renderer_, UiTheme::SectionTitleX, UiTheme::SectionTitleY, false, UiTheme::TextColor, true);
    creditText_ = Texture("Theme Manager - by DefKorns", 16, renderer_, UiTheme::CreditX, UiTheme::CreditY, false, UiTheme::TextColor, true);
    fprintf(stderr, "CHECKPOINT 4: basic textures created\n"); fflush(stderr);

    badgeA_ = Badge{ Texture("A", 16, renderer_, 0, 0, false, UiTheme::BadgeLetterColor, true), Texture(Translate("HINT_SELECT"), 16, renderer_, 0, 0, false, UiTheme::TextColor, true), UiTheme::BadgeADark, UiTheme::BadgeA };
    badgeRowRightEdge_ = UiTheme::BadgeClusterRightX - UiTheme::BadgeOuterSize - UiTheme::BadgeLabelGap - badgeA_.label.rect.w - UiTheme::BadgeGroupGap;
    badgeOuter_ = Texture(optionsLocation_ + UiTheme::AssetBadgeOuter, renderer_);
    badgeInner_ = Texture(optionsLocation_ + UiTheme::AssetBadgeInner, renderer_);
    badgeHold_ = Badge{ Texture("B", 16, renderer_, 0, 0, false, UiTheme::BadgeLetterColor, true), Texture(Translate("HINT_DELETE"), 16, renderer_, 0, 0, false, UiTheme::TextColor, true), UiTheme::BadgeXDark, UiTheme::BadgeX };

    ComputePinnedAndThemeRanges();
}

void ThemeManagerApp::ComputePinnedAndThemeRanges()
{
    pinnedStartIndex_ = static_cast<int>(commands_.size());
    while(pinnedStartIndex_ > 0 && (commands_[pinnedStartIndex_-1].name == "BACK" || commands_[pinnedStartIndex_-1].name == "EXIT"))
        --pinnedStartIndex_;

    themeStart_ = 0;
    while(themeStart_ < pinnedStartIndex_ && !isThemeItem_[themeStart_])
        ++themeStart_;
}

int ThemeManagerApp::DrawBadge(Badge & badge, int rightEdgeX)
{
    int groupW = UiTheme::BadgeOuterSize + UiTheme::BadgeLabelGap + badge.label.rect.w;
    int x = rightEdgeX - groupW;
    int y = UiTheme::BadgeBandY;
    badgeOuter_.rect = { x, y, UiTheme::BadgeOuterSize, UiTheme::BadgeOuterSize };
    SDL_SetTextureColorMod(badgeOuter_.texture.get(), badge.rim.r, badge.rim.g, badge.rim.b);
    badgeOuter_.Draw(renderer_);
    int innerOffset = (UiTheme::BadgeOuterSize - UiTheme::BadgeInnerSize) / 2;
    badgeInner_.rect = { x+innerOffset, y+innerOffset, UiTheme::BadgeInnerSize, UiTheme::BadgeInnerSize };
    SDL_SetTextureColorMod(badgeInner_.texture.get(), badge.fill.r, badge.fill.g, badge.fill.b);
    badgeInner_.Draw(renderer_);
    // glyph bearing makes the pure-math center look 1px down/left - nudged
    badge.letter.rect.x = x + (UiTheme::BadgeOuterSize - badge.letter.rect.w)/2 + 1;
    badge.letter.rect.y = y + (UiTheme::BadgeOuterSize - badge.letter.rect.h)/2 - 1;
    badge.letter.Draw(renderer_);
    badge.label.rect.x = x + UiTheme::BadgeOuterSize + UiTheme::BadgeLabelGap;
    badge.label.rect.y = y + (UiTheme::BadgeOuterSize - badge.label.rect.h)/2;
    badge.label.Draw(renderer_);
    return x - UiTheme::BadgeGroupGap;
}

// true if deleted - caller should break out of the main loop
bool ThemeManagerApp::ConfirmDelete()
{
    const int DialogCenterX = 640, DialogTitleY = 320, DialogHintY = 360; // screen center (1280x720)
    const std::string & confirmKey = commands_[currentCommandId_].deleteConfirmKey;
    Texture confirmTitle(Translate(confirmKey.empty() ? "DELETE_CONFIRM_GENERIC" : confirmKey), 24, renderer_, DialogCenterX, DialogTitleY, true, UiTheme::TextColor, true);
    Texture confirmHint(Translate("DELETE_CONFIRM_HINT"), 16, renderer_, DialogCenterX, DialogHintY, true, UiTheme::TextColor, true);
    controller_->GetButtonStatus(B); // consume the still-held B from the triggering long-press
    bool confirmed = false;
    for(;;)
    {
        controller_->Update();
        if(controller_->GetButtonStatus(B))
            break;
        if(controller_->GetButtonStatus(A) || controller_->GetButtonStatus(START))
        {
            confirmed = true;
            break;
        }
        sdlContext_->StartFrame();
        DrawFillRect(renderer_, UiTheme::FrameRect, UiTheme::BgR, UiTheme::BgG, UiTheme::BgB);
        DrawStrokeRect(renderer_, UiTheme::FrameRect, UiTheme::BorderR, UiTheme::BorderG, UiTheme::BorderB, UiTheme::BorderWidth, UiTheme::BorderRadius);
        confirmTitle.Draw(renderer_);
        confirmHint.Draw(renderer_);
        SDL_SetRenderDrawColor(renderer_, bgR_, bgG_, bgB_, 0xFF); // flat helpers leave the draw color dirty
        sdlContext_->EndFrame();
    }
    if(!confirmed)
        return false;
    system(commands_[currentCommandId_].deleteCommand.c_str());
    return true;
}

void ThemeManagerApp::DrawChromeCommon()
{
    DrawStrokeRect(renderer_, UiTheme::OuterRect, UiTheme::BorderR, UiTheme::BorderG, UiTheme::BorderB, UiTheme::BorderWidth, UiTheme::BorderRadius);
    gearIcon_.Draw(renderer_);
    appTitleText_.Draw(renderer_);
    appVersionText_.Draw(renderer_);
    DrawHLine(renderer_, UiTheme::HeaderDividerX, UiTheme::HeaderDividerX + UiTheme::HeaderDividerW, UiTheme::HeaderDividerY, UiTheme::BorderR, UiTheme::BorderG, UiTheme::BorderB, UiTheme::BorderWidth);
    DrawHLine(renderer_, UiTheme::OuterRect.x, UiTheme::OuterRect.x + UiTheme::OuterRect.w, UiTheme::FooterDividerY, UiTheme::BorderR, UiTheme::BorderG, UiTheme::BorderB, UiTheme::BorderWidth);
    DrawBadge(badgeA_, UiTheme::BadgeClusterRightX);
    creditText_.Draw(renderer_);
}

void ThemeManagerApp::DrawSectionTitle()
{
    int accentBarY = titleText_.rect.y + (titleText_.rect.h - UiTheme::SectionAccentBarH) / 2;
    DrawFillRect(renderer_, { UiTheme::SectionTitleX - UiTheme::SectionAccentBarW - 14, accentBarY, UiTheme::SectionAccentBarW, UiTheme::SectionAccentBarH }, UiTheme::AccentR, UiTheme::AccentG, UiTheme::AccentB);
    titleText_.Draw(renderer_);
}

Texture ThemeManagerApp::MakeScrollArrow(int x, int y) const
{
    Texture arrow("^", 16, renderer_, x, y, false, UiTheme::TextColor, true);
    arrow.rect.x -= arrow.rect.w / 2;
    return arrow;
}

ThemeManagerApp::FrameEvent ThemeManagerApp::PollFrameEvents()
{
    sdlContext_->StartFrame();
    controller_->Update();

    SDL_Event e;
    while(SDL_PollEvent(&e))
        if(e.type == SDL_QUIT)
            return FrameEvent::Quit;

    if(sdlContext_->powerwatch->buttonPress())
        return FrameEvent::PowerButtonPressed;

    return FrameEvent::Continue;
}

bool ThemeManagerApp::ActivateCommand(Command & cmd)
{
    if(cmd.runInternal)
    {
        cmd.RunCommand(*sdlContext_, controller_.get(), { gearIcon_, appTitleText_, appVersionText_, creditText_ }, bgR_, bgG_, bgB_);
        if(cmd.isToggle)
            cmd.UpdateState();
        return false;
    }
    system(cmd.command.c_str());
    return true;
}

bool ThemeManagerApp::UpdateBButton(bool & tapped)
{
    tapped = false;
    bool bHeldNow = controller_->PeekButtonStatus(B);
    if(bHeldNow)
    {
        if(!bHoldFired_ && controller_->HeldMillis(B) >= BHoldThresholdMs)
        {
            bHoldFired_ = true;
            if(!commands_[currentCommandId_].deleteCommand.empty() && ConfirmDelete())
                return true;
        }
    }
    else
    {
        tapped = bWasHeld_ && !bHoldFired_;
        bHoldFired_ = false;
    }
    bWasHeld_ = bHeldNow;
    return false;
}

int ThemeManagerApp::RunGridLayout()
{
    const bool squareTiles = themeStart_ < pinnedStartIndex_ && commands_[themeStart_].previewSquare;
    const int explicitCols = themeStart_ < pinnedStartIndex_ ? commands_[themeStart_].previewGridCols : -1;
    const int GridCols = explicitCols > 0 ? explicitCols : (squareTiles ? 7 : 4);
    const bool hasCaptions = themeStart_ < pinnedStartIndex_ && !commands_[themeStart_].previewHideLabel;
    const int GridGap = 20;

    // an empty slot stays reserved-width rather than compacted out, so a
    // conditional button appearing/disappearing doesn't shift the rest
    std::vector<int> stripSlots;
    for(int i = 0; i < themeStart_; ++i)
        stripSlots.push_back(i);
    const int stripSlotCount = static_cast<int>(stripSlots.size());
    std::vector<int> stripIndices;
    for(int i : stripSlots)
        if(!commands_[i].command.empty())
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
    const int chipIconSize = 22;

    const bool hasStrip = stripSlotCount > 0;
    const int StripTop = UiTheme::HeaderDividerY + 22;
    const int StripH = hasStrip ? 58 : 0;
    const int GridLeft = UiTheme::FrameX + 32;
    const int GridRight = UiTheme::FrameX + UiTheme::FrameW - 32;
    // capped so a strip with few slots doesn't stretch its chips wide
    const int MaxChipW = 200;
    const int chipW = hasStrip ? std::min(MaxChipW, (GridRight - GridLeft - (stripSlotCount-1)*chipGap) / stripSlotCount) : 0;
    titleText_ = Texture(Translate(options_.titleKey), UiTheme::SectionTitleFontSize - 6, renderer_, UiTheme::SectionTitleX, 0, false, UiTheme::TextColor, true);
    titleText_.rect.y = hasStrip ? (StripTop + StripH + 22) : (UiTheme::HeaderDividerY + 22);
    const int GridTop = titleText_.rect.y + titleText_.rect.h + 20;
    const int GridBottom = UiTheme::FooterDividerY - 14;
    const int LabelH = hasCaptions ? 18 : 0;
    const int RowGap = hasCaptions ? 8 : GridGap;
    const int TileW = (GridRight - GridLeft - (GridCols-1)*GridGap) / GridCols;
    int TileH, GridRowsVisible;
    if(hasStrip)
    {
        TileH = hasCaptions ? TileW / 2 : TileW * 9 / 16;
        GridRowsVisible = std::max(1, (GridBottom - GridTop + RowGap) / (TileH + LabelH + RowGap));
    }
    else
    {
        GridRowsVisible = 3;
        TileH = (GridBottom - GridTop - (GridRowsVisible-1)*RowGap - GridRowsVisible*LabelH) / GridRowsVisible;
    }
    const int GridRowPitch = TileH + LabelH + RowGap;

    Texture gridScrollUp = MakeScrollArrow(GridRight + 22, GridTop + 24);
    Texture gridScrollDown = gridScrollUp;
    gridScrollDown.rect.y = GridBottom - 24 - gridScrollDown.rect.h;

    // B runs Back/Exit directly - it's a footer badge, not a chip
    Badge badgeB{ Texture("B", 16, renderer_, 0, 0, false, UiTheme::BadgeLetterColor, true),
                  Texture(pinnedStartIndex_ < (int)commands_.size() ? Translate(commands_[pinnedStartIndex_].name) : "", 16, renderer_, 0, 0, false, UiTheme::TextColor, true),
                  UiTheme::BadgeBDark, UiTheme::BadgeB };

    std::vector<Texture> chipIcons(commands_.size());
    std::vector<Texture> chipLabels(commands_.size());
    std::vector<Texture> tileImages(commands_.size());
    std::vector<Texture> tileLabels(commands_.size());
    std::vector<std::vector<Texture>> tileRunFrames(commands_.size()); // sized >1 only for an animated _run01.png tile
    std::vector<bool> tileHideLabel(commands_.size(), false); // PREVIEW_HIDE_LABEL
    std::vector<bool> tileFitContain(commands_.size(), false); // PREVIEW_FIT_CONTAIN
    for(int i = 0; i < (int)commands_.size(); ++i)
    {
        Command & c = commands_[i];
        if(c.command.empty())
            continue; // reserved blank slot, not a chip or tile
        std::string label = Translate(c.name);
        if(i < themeStart_ || i >= pinnedStartIndex_)
        {
            int labelAvail = chipW - chipIconSize - 24;
            if(c.previewImage.size())
            {
                SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1"); // smooth - these are small glyphs, nearest looks jagged
                Texture icon(c.previewImage, renderer_, 0, 0);
                SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
                FitCentered(icon, 0, 0, chipIconSize, chipIconSize, 0);
                SDL_SetTextureColorMod(icon.texture.get(), UiTheme::AccentR, UiTheme::AccentG, UiTheme::AccentB);
                chipIcons[i] = icon;
                labelAvail = chipW - chipIconSize - 32;
            }
            label = TruncateToWidth(label, 14, labelAvail);
            chipLabels[i] = Texture(label, 14, renderer_, 0, 0, false, UiTheme::TextColor, true);
        }
        else
        {
            label = TruncateToWidth(label, 15, TileW - 28);
            tileLabels[i] = Texture(label, 15, renderer_, 0, 0, false, UiTheme::TextColor, true);
        }
    }
    std::vector<bool> tileImageLoaded(commands_.size(), false);
    auto EnsureTileImage = [&](int idx)
    {
        if(tileImageLoaded[idx])
            return;
        tileImageLoaded[idx] = true;
        const Command & c = commands_[idx];
        if(c.previewImage.empty())
            return;
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, c.previewNearest ? "0" : "1");
        Texture art(c.previewImage, renderer_, 0, 0);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

        tileHideLabel[idx] = c.previewHideLabel;
        tileFitContain[idx] = c.previewFitContain;

        // _run01.png with sibling _run02/_run03/... is this project's naming
        // convention for a multi-frame run-cycle animation
        const std::string runSuffix = "_run01.png";
        bool isRunCycle = c.previewImage.size() > runSuffix.size() &&
           c.previewImage.compare(c.previewImage.size() - runSuffix.size(), runSuffix.size(), runSuffix) == 0;

        if(tileFitContain[idx])
            FitCentered(art, 0, 0, TileW, TileH, 0);
        else
            FitCover(art, 0, 0, TileW, TileH, 0);
        tileImages[idx] = art;

        if(isRunCycle)
        {
            std::string base = c.previewImage.substr(0, c.previewImage.size() - runSuffix.size()) + "_run";
            std::vector<Texture> frames{ art };
            for(int n = 2; n <= 8; ++n)
            {
                std::string framePath = base + (n < 10 ? "0" : "") + std::to_string(n) + ".png";
                if(!std::ifstream(framePath).good())
                    break;
                Texture frame(framePath, renderer_, 0, 0);
                FitCentered(frame, 0, 0, TileW, TileH, 0);
                frames.push_back(frame);
            }
            if(frames.size() > 1)
                tileRunFrames[idx] = std::move(frames);
        }
    };

    int gridTopRow = 0;
    auto SetCurrentCommand = [&](int newId)
    {
        currentCommandId_ = newId;
        if(newId >= themeStart_ && newId < pinnedStartIndex_)
        {
            int row = (newId - themeStart_) / GridCols;
            if(row < gridTopRow) gridTopRow = row;
            else if(row >= gridTopRow + GridRowsVisible) gridTopRow = row - GridRowsVisible + 1;
        }
    };
    SetCurrentCommand(themeStart_ < pinnedStartIndex_ ? themeStart_ : (stripCount > 0 ? StripToCommand(0) : 0));

    // fixed for the whole screen (only gridTopRow scrolls within them)
    const int lastThemeIndex = pinnedStartIndex_ - 1;
    const int totalRows = (lastThemeIndex - themeStart_ + GridCols) / GridCols;

    for(;;)
    {
        FrameEvent frameEvent = PollFrameEvents();
        if(frameEvent == FrameEvent::Quit)
            return 0;
        if(frameEvent == FrameEvent::PowerButtonPressed)
            break;

        bool inGrid = currentCommandId_ >= themeStart_ && currentCommandId_ < pinnedStartIndex_;

        if(controller_->GetButtonStatus(A) || controller_->GetButtonStatus(START))
        {
            if(ActivateCommand(commands_[currentCommandId_]))
                break;
        }
        else if(controller_->HeldRepeat(LEFT))
        {
            if(inGrid && currentCommandId_ > themeStart_)
                SetCurrentCommand(currentCommandId_ - 1);
            else if(!inGrid && stripCount > 0)
                SetCurrentCommand(StripToCommand(std::max(0, CommandToStrip(currentCommandId_) - 1)));
        }
        else if(controller_->HeldRepeat(RIGHT))
        {
            if(inGrid && currentCommandId_ < pinnedStartIndex_ - 1)
                SetCurrentCommand(currentCommandId_ + 1);
            else if(!inGrid && stripCount > 0)
                SetCurrentCommand(StripToCommand(std::min(stripCount - 1, CommandToStrip(currentCommandId_) + 1)));
        }
        else if(controller_->HeldRepeat(UP))
        {
            if(inGrid)
            {
                int col = (currentCommandId_ - themeStart_) % GridCols;
                if(currentCommandId_ - themeStart_ < GridCols) // top grid row - jump up to the strip
                {
                    if(stripCount > 0)
                        SetCurrentCommand(StripToCommand(std::min(stripCount - 1, col)));
                }
                else
                    SetCurrentCommand(currentCommandId_ - GridCols);
            }
        }
        else if(controller_->HeldRepeat(DOWN))
        {
            if(!inGrid && themeStart_ < pinnedStartIndex_)
            {
                int col = std::min(GridCols - 1, CommandToStrip(currentCommandId_));
                SetCurrentCommand(std::min(pinnedStartIndex_ - 1, themeStart_ + col));
            }
            else if(inGrid && currentCommandId_ + GridCols < pinnedStartIndex_)
                SetCurrentCommand(currentCommandId_ + GridCols);
        }
        {
            bool tapped = false;
            if(UpdateBButton(tapped))
                break;
            // quick tap - Back/Exit isn't a chip, B runs it directly
            if(tapped && pinnedStartIndex_ < (int)commands_.size() && ActivateCommand(commands_[pinnedStartIndex_]))
                break;
        }

        DrawChromeCommon();
        DrawSectionTitle();
        {
            int rightEdge = badgeRowRightEdge_;
            if(pinnedStartIndex_ < (int)commands_.size())
                rightEdge = DrawBadge(badgeB, rightEdge);
            if(!commands_[currentCommandId_].deleteCommand.empty())
                DrawBadge(badgeHold_, rightEdge);
        }

        {
            int x = GridLeft;
            for(int pos = 0; pos < stripSlotCount; ++pos)
            {
                int idx = stripSlots[pos];
                if(commands_[idx].command.empty())
                {
                    x += chipW + chipGap;
                    continue;
                }
                bool selected = idx == currentCommandId_;
                SDL_Rect chipRect{ x, StripTop, chipW, StripH };
                if(selected)
                {
                    DrawRoundedFillRect(renderer_, chipRect, UiTheme::SelectedRowBgR, UiTheme::SelectedRowBgG, UiTheme::SelectedRowBgB, UiTheme::BoxRadius);
                    DrawStrokeRect(renderer_, chipRect, UiTheme::AccentR, UiTheme::AccentG, UiTheme::AccentB, 2, UiTheme::BoxRadius);
                }
                else
                    DrawStrokeRect(renderer_, chipRect, UiTheme::BorderR, UiTheme::BorderG, UiTheme::BorderB, UiTheme::BorderWidth, UiTheme::BoxRadius);

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
                    icon.Draw(renderer_);
                    gx += iconW + 12;
                }
                label.rect.x = gx;
                label.rect.y = StripTop + (StripH - label.rect.h) / 2;
                label.Draw(renderer_);

                x += chipW + chipGap;
            }
        }

        {
            for(int row = 0; row < GridRowsVisible; ++row)
            {
                int rowStart = themeStart_ + (gridTopRow+row)*GridCols;
                if(rowStart > lastThemeIndex)
                    break;
                int y = GridTop + row*GridRowPitch;
                for(int col = 0; col < GridCols; ++col)
                {
                    int idx = rowStart + col;
                    if(idx > lastThemeIndex)
                        break;
                    if(commands_[idx].command.empty())
                        continue; // reserved blank cell, not a tile
                    int x = GridLeft + col*(TileW+GridGap);
                    bool selected = idx == currentCommandId_;
                    SDL_Rect tileRect{ x, y, TileW, TileH };
                    DrawRoundedFillRect(renderer_, tileRect, UiTheme::SelectedRowBgR, UiTheme::SelectedRowBgG, UiTheme::SelectedRowBgB, UiTheme::BoxRadius);
                    EnsureTileImage(idx);
                    std::vector<Texture> & runFrames = tileRunFrames[idx];
                    Texture & tileArt = runFrames.size() > 1
                        ? runFrames[(SDL_GetTicks() / 100) % runFrames.size()]
                        : tileImages[idx];
                    if(tileArt.rect.w > 0)
                    {
                        // clip to the tile, then mask the clip's square corners back to rounded
                        SDL_RenderSetClipRect(renderer_, &tileRect);
                        if(tileFitContain[idx])
                            // 10px margin keeps clear of the rounded-corner mask below
                            FitCentered(tileArt, x, y, TileW, TileH, 10);
                        else
                            FitCover(tileArt, x, y, TileW, TileH, 0);
                        tileArt.Draw(renderer_);
                        SDL_RenderSetClipRect(renderer_, nullptr);
                        DrawRoundedCornerMask(renderer_, tileRect, UiTheme::SelectedRowBgR, UiTheme::SelectedRowBgG, UiTheme::SelectedRowBgB, UiTheme::BoxRadius);
                    }
                    DrawStrokeRect(renderer_, tileRect, selected ? UiTheme::AccentR : UiTheme::BorderR, selected ? UiTheme::AccentG : UiTheme::BorderG, selected ? UiTheme::AccentB : UiTheme::BorderB, selected ? 3 : UiTheme::BorderWidth, UiTheme::BoxRadius);
                    if(!tileHideLabel[idx])
                    {
                        tileLabels[idx].rect.x = x + (TileW - tileLabels[idx].rect.w) / 2;
                        tileLabels[idx].rect.y = y + TileH + (LabelH - tileLabels[idx].rect.h) / 2;
                        tileLabels[idx].Draw(renderer_);
                    }
                }
            }

            if(gridTopRow > 0)
                gridScrollUp.Draw(renderer_);
            if(gridTopRow + GridRowsVisible < totalRows)
                gridScrollDown.Draw(renderer_, SDL_FLIP_VERTICAL);
        }

        SDL_SetRenderDrawColor(renderer_, bgR_, bgG_, bgB_, 0xFF);
        sdlContext_->EndFrame();
    }

    return 0;
}

int ThemeManagerApp::RunListLayout()
{
    Texture scrollUp = MakeScrollArrow(UiTheme::ScrollX, UiTheme::ScrollUpY);
    Texture scrollDown = scrollUp;
    scrollDown.rect.y = UiTheme::ScrollDownY;
    SDL_Rect selectedRowRect{ UiTheme::ListX, UiTheme::RowFirstY - 2, UiTheme::ListContentRightX - UiTheme::ListX, 0 };

    const int RowGlyphSize = 16;
    const int RowTextGapPx = 16;
    const int ChildIndent = 4*16;
    for(Command & c : commands_)
    {
        int textX = UiTheme::RowTextX + (c.child ? ChildIndent : 0);
        std::string label = TruncateToWidth(Translate(c.name), RowGlyphSize, UiTheme::RowControlRightX - RowTextGapPx - textX);
        c.texture = Texture(label, RowGlyphSize, renderer_, textX, 0, false, UiTheme::TextColor, true);
    }

    const int modernRowPitch = std::max(UiTheme::RowPitch, GetTTFLineHeight(RowGlyphSize));
    int pinnedAreaTop;
    {
        int slotBottom = UiTheme::FooterDividerY - UiTheme::PinnedBottomMargin;
        for(int i = static_cast<int>(commands_.size())-1; i >= pinnedStartIndex_; --i)
        {
            int slotTop = slotBottom - modernRowPitch;
            commands_[i].texture.rect.y = slotTop + (modernRowPitch - commands_[i].texture.rect.h) / 2 + UiTheme::RowTextYNudge;
            slotBottom = slotTop;
        }
        pinnedAreaTop = slotBottom;
    }
    const int DisplayItemCount = std::max(1, (pinnedAreaTop - UiTheme::RowFirstY) / modernRowPitch);

    int topListItemNumber = 1; // forces SetCurrentCommand(0) below to lay out row Y positions the first time
    std::shared_ptr<Texture> PreviewImage;
    auto SetCurrentCommand = [&] (int newCommandId)
    {
        currentCommandId_ = newCommandId;
        const Command & currentCommand = commands_[currentCommandId_];

        // pinned rows don't affect scroll position
        int scrollTarget = std::min(currentCommandId_, std::max(0, pinnedStartIndex_-1));

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
            for(int i = 0, count = std::min(DisplayItemCount, pinnedStartIndex_-topListItemNumber); i < count; ++i)
            {
                Texture & rowTexture = commands_[i+topListItemNumber].texture;
                rowTexture.rect.y = y + (modernRowPitch - rowTexture.rect.h) / 2 + UiTheme::RowTextYNudge;
                y += modernRowPitch;
            }
        }

        selectedRowRect.y = currentCommand.texture.rect.y - 2;
        selectedRowRect.h = currentCommand.texture.rect.h + 4;

        if(currentCommand.previewImage.size())
        {
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, currentCommand.previewNearest ? "0" : "1");
            PreviewImage = std::make_shared<Texture>(currentCommand.previewImage, renderer_, 0, 0);
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
            FitCentered(*PreviewImage, UiTheme::DetailX, UiTheme::PreviewBoxY, UiTheme::DetailW, UiTheme::PreviewBoxH, UiTheme::ContentPadding);
        }
        else
            PreviewImage.reset();
    };
    SetCurrentCommand(0);

    auto DrawRow = [&](Command & rowCommand, bool isLastOverall)
    {
        bool selected = &rowCommand == &commands_[currentCommandId_];
        rowCommand.texture.Draw(renderer_);
        if(rowCommand.isToggle)
        {
            Texture & rowSwitch = rowCommand.stateOn ? switchOn_ : switchOff_;
            rowSwitch.rect.x = UiTheme::SwitchRightX - UiTheme::SwitchW;
            rowSwitch.rect.y = rowCommand.texture.rect.y + (rowCommand.texture.rect.h - UiTheme::SwitchH) / 2;
            rowSwitch.Draw(renderer_);
        }
        // skip for separators and the last row - nothing to separate there
        if(!selected && !rowCommand.command.empty() && !isLastOverall)
            DrawHLine(renderer_, UiTheme::ListX, UiTheme::ListContentRightX, rowCommand.texture.rect.y + rowCommand.texture.rect.h + 3, UiTheme::BorderR, UiTheme::BorderG, UiTheme::BorderB);
    };

    auto DrawChrome = [&]()
    {
        DrawChromeCommon();
        DrawSectionTitle();
        if(!commands_[currentCommandId_].deleteCommand.empty())
            DrawBadge(badgeHold_, badgeRowRightEdge_);

        DrawRoundedFillRect(renderer_, selectedRowRect, UiTheme::SelectedRowBgR, UiTheme::SelectedRowBgG, UiTheme::SelectedRowBgB, UiTheme::BoxRadius);
        DrawStrokeRect(renderer_, selectedRowRect, UiTheme::AccentR, UiTheme::AccentG, UiTheme::AccentB, 2, UiTheme::BoxRadius);

        if(PreviewImage.get())
        {
            SDL_Rect previewBox{ UiTheme::DetailX, UiTheme::PreviewBoxY, UiTheme::DetailW, UiTheme::PreviewBoxH };
            DrawStrokeRect(renderer_, previewBox, UiTheme::BorderR, UiTheme::BorderG, UiTheme::BorderB, UiTheme::BorderWidth, UiTheme::BoxRadius);
            PreviewImage->Draw(renderer_);
        }
    };

    const int lastCommandIndex = static_cast<int>(commands_.size()) - 1; // commands_ never resized during this loop

    for(;;)
    {
        FrameEvent frameEvent = PollFrameEvents();
        if(frameEvent == FrameEvent::Quit)
            return 0;
        if(frameEvent == FrameEvent::PowerButtonPressed)
            break;

        if(controller_->GetButtonStatus(A) || controller_->GetButtonStatus(START))
        {
            if(ActivateCommand(commands_[currentCommandId_]))
                break;
        }
        else if(controller_->HeldRepeat(UP))
        {
            // bounded so an all-separator list can't spin forever
            int newCommandId = currentCommandId_;
            for(size_t tries = 0; tries < commands_.size(); ++tries)
            {
                newCommandId = (newCommandId-1+commands_.size())%commands_.size();
                if(commands_[newCommandId].command.size() != 0)
                    break;
            }
            SetCurrentCommand(newCommandId);
        }
        else if(controller_->HeldRepeat(DOWN))
        {
            int newCommandId = currentCommandId_;
            for(size_t tries = 0; tries < commands_.size(); ++tries)
            {
                newCommandId = (newCommandId+1)%commands_.size();
                if(commands_[newCommandId].command.size() != 0)
                    break;
            }
            SetCurrentCommand(newCommandId);
        }
        {
            bool tapped = false;
            if(UpdateBButton(tapped))
                break;
            if(tapped)
                SetCurrentCommand(commands_.size()-1);
        }

        DrawChrome();

        for(int i = 0, count = std::min(DisplayItemCount, pinnedStartIndex_-topListItemNumber); i < count; ++i)
            DrawRow(commands_[i+topListItemNumber], (i+topListItemNumber) == lastCommandIndex);

        for(int i = pinnedStartIndex_; i <= lastCommandIndex; ++i)
            DrawRow(commands_[i], i == lastCommandIndex);

        // bounded by pinnedStartIndex_, not commands_.size() - pinned rows need no scroll
        if(topListItemNumber != 0)
            scrollUp.Draw(renderer_);
        if((topListItemNumber + DisplayItemCount) < pinnedStartIndex_)
            scrollDown.Draw(renderer_, SDL_FLIP_VERTICAL);

        SDL_SetRenderDrawColor(renderer_, bgR_, bgG_, bgB_, 0xFF);

        sdlContext_->EndFrame();
    }

    return 0;
}

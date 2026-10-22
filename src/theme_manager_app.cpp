/**
  * Copyright (c) 2026 DefKorns (https://defkorns.github.io/LICENSE)
  *
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  */

#include "theme_manager_app.h"
#include "download_screen.h"
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

namespace
{
    Color ActiveThemeBorder{ 0x4C, 0xAF, 0x6E }; // green

    void LoadActiveThemeBorderColor(const std::string & optionsLocation)
    {
        std::ifstream in(optionsLocation + "theme.cfg");
        std::string line;
        while(std::getline(in, line))
        {
            if(!line.empty() && line.back() == '\r')
                line.pop_back();
            size_t eq = line.find('=');
            if(eq == std::string::npos || line[0] == '#' || line.compare(0, eq, "ActiveThemeBorder") != 0)
                continue;
            int r, g, b;
            if(std::sscanf(line.c_str() + eq + 1, "%d,%d,%d", &r, &g, &b) == 3)
                ActiveThemeBorder = { static_cast<Uint8>(r), static_cast<Uint8>(g), static_cast<Uint8>(b) };
        }
    }
}

ThemeManagerApp::ThemeManagerApp(std::string optionsLocation, AppOptions options, std::vector<Command> commands, std::vector<ItemInfo> items)
    : optionsLocation_(std::move(optionsLocation))
    , options_(std::move(options))
    , commands_(std::move(commands))
    , items_(std::move(items))
{
    LoadActiveThemeBorderColor(optionsLocation_);

    sdlContext_ = std::make_unique<SDL_Context>(std::chrono::milliseconds(33), false);
    renderer_ = sdlContext_->renderer;
    controller_ = std::make_unique<Controller>(1);

    bg_ = UiTheme::Bg;
    SetDrawColor(renderer_, bg_);

    switchOn_ = Texture(optionsLocation_ + UiTheme::AssetSwitchOn, renderer_);
    switchOff_ = Texture(optionsLocation_ + UiTheme::AssetSwitchOff, renderer_);

    chrome_ = std::make_unique<ScreenChrome>(optionsLocation_, renderer_, "Theme Manager", MOD_VERSION, "Theme Manager - by DefKorns");
    scrollArrows_ = std::make_unique<ScrollArrows>(optionsLocation_, renderer_);
    titleText_ = Texture(Translate(options_.titleKey), UiTheme::SectionTitleFontSize, renderer_, UiTheme::SectionTitleX, UiTheme::SectionTitleY, false, ToAbgr(UiTheme::Text), true);

    badges_ = std::make_unique<BadgePainter>(optionsLocation_, renderer_);
    badgeA_ = badges_->Make("A", Translate("HINT_SELECT"), UiTheme::BadgeADark, UiTheme::BadgeA);
    badgeRowRightEdge_ = UiTheme::BadgeClusterRightX - UiTheme::BadgeOuterSize - UiTheme::BadgeLabelGap - badgeA_.label.rect.w - UiTheme::BadgeGroupGap;
    if(options_.downloadsScreen)
        storage_ = std::make_unique<StorageIndicator>(renderer_);
    badgeHold_ = badges_->Make("B", Translate("HINT_DELETE"), UiTheme::BadgeXDark, UiTheme::BadgeX);

    ComputePinnedAndThemeRanges();
}

void ThemeManagerApp::ComputePinnedAndThemeRanges()
{
    pinnedStartIndex_ = static_cast<int>(commands_.size());
    while(pinnedStartIndex_ > 0 && (commands_[pinnedStartIndex_-1].name == "BACK" || commands_[pinnedStartIndex_-1].name == "EXIT"))
        --pinnedStartIndex_;

    themeStart_ = 0;
    while(themeStart_ < pinnedStartIndex_ && !items_[themeStart_].isThemeItem)
        ++themeStart_;
}

int ThemeManagerApp::DrawBadge(Badge & badge, int rightEdgeX)
{
    return badges_->Draw(badge, rightEdgeX, UiTheme::BadgeBandY);
}

bool ThemeManagerApp::Confirm(const ScreenSnapshot & background, const std::string & title)
{
    ConfirmDialog dialog(renderer_, *badges_, title,
                         badges_->Make("A", Translate("HINT_CONFIRM"), UiTheme::BadgeADark, UiTheme::BadgeA),
                         badges_->Make("B", Translate("HINT_CANCEL"), UiTheme::BadgeBDark, UiTheme::BadgeB));
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
        background.Draw();
        dialog.Draw();
        SetDrawColor(renderer_, bg_);
        sdlContext_->EndFrame();
    }
    return confirmed;
}

bool ThemeManagerApp::ConfirmDelete(const ScreenSnapshot & background)
{
    const std::string & blockedKey = commands_[currentCommandId_].deleteBlockedKey;
    if(!blockedKey.empty())
    {
        MessageDialog dialog(renderer_, *badges_, Translate(blockedKey),
                             badges_->Make("B", Translate("HINT_BACK"), UiTheme::BadgeBDark, UiTheme::BadgeB));
        controller_->GetButtonStatus(B);
        for(;;)
        {
            controller_->Update();
            if(controller_->GetButtonStatus(B) || controller_->GetButtonStatus(A) || controller_->GetButtonStatus(START))
                break;
            sdlContext_->StartFrame();
            background.Draw();
            dialog.Draw();
            SetDrawColor(renderer_, bg_);
            sdlContext_->EndFrame();
        }
        return false;
    }
    const std::string & confirmKey = commands_[currentCommandId_].deleteConfirmKey;
    if(!Confirm(background, Translate(confirmKey.empty() ? "DELETE_CONFIRM_GENERIC" : confirmKey)))
        return false;
    system(commands_[currentCommandId_].deleteCommand.c_str());
    return true;
}

bool ThemeManagerApp::FinishFrame()
{
    SetDrawColor(renderer_, bg_);
    if(!deleteRequested_ && !overwriteRequested_)
    {
        sdlContext_->EndFrame();
        return false;
    }
    const ScreenSnapshot background(renderer_);
    sdlContext_->EndFrame();
    if(overwriteRequested_)
    {
        overwriteRequested_ = false;
        if(Confirm(background, Translate("DOWNLOAD_OVERWRITE_CONFIRM")))
            RunDownload(commands_[currentCommandId_]);
        return false;
    }
    deleteRequested_ = false;
    return ConfirmDelete(background);
}

void ThemeManagerApp::DrawFooterBadges(Badge & badgeB)
{
    int rightEdge = badgeRowRightEdge_;
    if(pinnedStartIndex_ < (int)commands_.size())
        rightEdge = DrawBadge(badgeB, rightEdge);
    if(!commands_[currentCommandId_].deleteCommand.empty() || !commands_[currentCommandId_].deleteBlockedKey.empty())
        rightEdge = DrawBadge(badgeHold_, rightEdge);
    DrawFooterDivider(renderer_, rightEdge);
}

void ThemeManagerApp::DrawChromeCommon()
{
    chrome_->DrawFrame();
    DrawBadge(badgeA_, UiTheme::BadgeClusterRightX);
}

void ThemeManagerApp::DrawSectionTitle()
{
    int accentBarY = titleText_.rect.y + (titleText_.rect.h - UiTheme::SectionAccentBarH) / 2;
    DrawFillRect(renderer_, { UiTheme::SectionTitleX - UiTheme::SectionAccentBarW - 14, accentBarY, UiTheme::SectionAccentBarW, UiTheme::SectionAccentBarH }, UiTheme::Accent);
    titleText_.Draw(renderer_);
}

Texture ThemeManagerApp::MakeScrollArrow(int x, int y) const
{
    Texture arrow(optionsLocation_ + UiTheme::AssetChevronUp, renderer_, x, y);
    SetColorMod(arrow.texture.get(), UiTheme::ScrollArrow);
    arrow.rect.x -= arrow.rect.w / 2;
    return arrow;
}

std::string ThemeManagerApp::FocusStatePath() const
{
    std::string sanitized = options_.commandLocation;
    for(char & c : sanitized)
        if(c == '/') c = '_';
    return FocusStatePrefix + sanitized;
}

void ThemeManagerApp::SaveFocusState() const
{
    std::ofstream(FocusStatePath(), std::ios::trunc) << commands_[currentCommandId_].name;
}

int ThemeManagerApp::LoadFocusIndex() const
{
    std::ifstream in(FocusStatePath());
    std::string name;
    if(!std::getline(in, name) || name.empty())
        return -1;
    for(int i = 0; i < static_cast<int>(commands_.size()); ++i)
        if(commands_[i].name == name)
            return i;
    return -1;
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

void ThemeManagerApp::ResumeUnderlyingUi() const
{
    system(("rm -f " + std::string(FocusStatePrefix) + "*; /bin/sh " + optionsLocation_ + "scripts/ResumeUI.sh").c_str());
}

bool ThemeManagerApp::ActivateCommand(Command & cmd)
{
    if(!cmd.runInternal)
    {
        system(cmd.command.c_str());
        return true;
    }
    if(options_.downloadsScreen && cmd.command.find("/get_all") == std::string::npos)
    {
        if(items_[static_cast<size_t>(&cmd - commands_.data())].installed)
            overwriteRequested_ = true;
        else
            RunDownload(cmd);
        return false;
    }
    cmd.RunCommand(*sdlContext_, controller_.get(), *chrome_, bg_);
    if(cmd.isToggle)
        cmd.UpdateState();
    // the caller's frame was cleared before the command drew over it
    SetDrawColor(renderer_, bg_);
    SDL_RenderClear(renderer_);
    return false;
}

void ThemeManagerApp::ReloadCommands()
{
    std::vector<Command> commands;
    std::vector<ItemInfo> items;
    if(!LoadCommands(options_.commandLocation, options_.scriptLocation, optionsLocation_, options_.themePath, commands, items))
        return;
    AppendBackNavigation(optionsLocation_, options_, ReadBackStack(options_.isRootScreen), commands, items);
    if(commands.empty())
        return;
    commands_ = std::move(commands);
    items_ = std::move(items);
    ComputePinnedAndThemeRanges();
}

void ThemeManagerApp::RunDownload(Command & cmd)
{
    const size_t index = static_cast<size_t>(&cmd - commands_.data());
    const std::string title = cmd.name.substr(0, cmd.name.find(" ["));
    if(RunDownloadScreen(cmd.command, Translate("INSTALLING_THEME"), Translate(title), items_[index].author, *sdlContext_, *controller_, *chrome_))
    {
        items_[index].isNew = false;
        items_[index].installed = true;
    }
    SetDrawColor(renderer_, bg_);
    SDL_RenderClear(renderer_);
}

void ThemeManagerApp::UpdateBButton(bool & tapped)
{
    tapped = false;
    bool bHeldNow = controller_->PeekButtonStatus(B);
    if(bHeldNow)
    {
        if(!bHoldFired_ && controller_->HeldMillis(B) >= BHoldThresholdMs)
        {
            bHoldFired_ = true;
            if(!commands_[currentCommandId_].deleteCommand.empty() || !commands_[currentCommandId_].deleteBlockedKey.empty())
                deleteRequested_ = true;
        }
    }
    else
    {
        tapped = bWasHeld_ && !bHoldFired_;
        bHoldFired_ = false;
        controller_->HeldMillis(B); // releases the hold timer, or the next press starts already past the threshold
    }
    bWasHeld_ = bHeldNow;
}

int ThemeManagerApp::RunGridLayout()
{
    const bool squareTiles = themeStart_ < pinnedStartIndex_ && commands_[themeStart_].previewSquare;
    const int explicitCols = themeStart_ < pinnedStartIndex_ ? commands_[themeStart_].previewGridCols : -1;
    const int GridCols = explicitCols > 0 ? explicitCols : (squareTiles ? 7 : 4);
    const bool hasCaptions = themeStart_ < pinnedStartIndex_ && !commands_[themeStart_].previewHideLabel;
    const int GridGap = 20;

    // empty slots keep their width so the other chips don't shift
    int stripEnd = themeStart_;
    while(stripEnd > 0 && commands_[stripEnd-1].command.empty())
        --stripEnd;
    std::vector<int> stripSlots;
    for(int i = 0; i < stripEnd; ++i)
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
    const int chipIconGap = 12;
    const int chipPadX = 20;
    const int chipLabelSize = 14;
    const int MinChipW = 140;

    const bool hasStrip = stripSlotCount > 0;
    const int StripTop = UiTheme::HeaderDividerY + 22;
    const int StripH = hasStrip ? 58 : 0;
    const int GridLeft = UiTheme::FrameX + 32;
    const int GridRight = UiTheme::FrameX + UiTheme::FrameW - 32;
    auto ChipIconW = [&](int idx) { return commands_[idx].previewImage.empty() ? 0 : chipIconSize + chipIconGap; };
    std::vector<int> chipWidths;
    for(int idx : stripSlots)
    {
        const Command & c = commands_[idx];
        const int contentW = c.command.empty() ? 0 : ChipIconW(idx) + TextWidth(Translate(c.name), chipLabelSize);
        chipWidths.push_back(std::max(MinChipW, contentW + 2*chipPadX));
    }
    const int stripAvail = GridRight - GridLeft - std::max(0, stripSlotCount-1)*chipGap;
    auto StripWidthAt = [&](int cap)
    {
        int total = 0;
        for(int w : chipWidths)
            total += std::min(w, cap);
        return total;
    };
    int chipCap = chipWidths.empty() ? 0 : *std::max_element(chipWidths.begin(), chipWidths.end());
    while(chipCap > MinChipW && StripWidthAt(chipCap) > stripAvail)
        --chipCap;
    for(int & w : chipWidths)
        w = std::min(w, chipCap);
    std::vector<int> chipWidthOf(commands_.size(), 0);
    for(int pos = 0; pos < stripSlotCount; ++pos)
        chipWidthOf[stripSlots[pos]] = chipWidths[pos];
    titleText_ = Texture(Translate(options_.titleKey), UiTheme::SectionTitleFontSize - 6, renderer_, UiTheme::SectionTitleX, 0, false, ToAbgr(UiTheme::Text), true);
    titleText_.rect.y = hasStrip ? (StripTop + StripH + 22) : (UiTheme::HeaderDividerY + 22);
    const int GridTop = titleText_.rect.y + titleText_.rect.h + 20;

    const int noticeSize = 14;
    const int noticeGap = 24;
    const int attentionDotSize = 10;
    const int attentionDotInset = 7;
    Texture noticeText;
    for(int i = 0; i < (int)commands_.size(); ++i)
    {
        if(!items_[i].needsAttention || items_[i].attentionNotice.empty())
            continue;
        const int noticeAvail = GridRight - (titleText_.rect.x + titleText_.rect.w + noticeGap);
        noticeText = Texture(TruncateToWidth(Translate(items_[i].attentionNotice), noticeSize, noticeAvail), noticeSize, renderer_, 0, 0, false, ToAbgr(UiTheme::Accent), true);
        noticeText.rect.x = GridRight - noticeText.rect.w;
        noticeText.rect.y = titleText_.rect.y + (titleText_.rect.h - noticeText.rect.h) / 2;
        break;
    }
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
    Badge badgeB = badges_->Make("B", pinnedStartIndex_ < (int)commands_.size() ? Translate(commands_[pinnedStartIndex_].name) : "", UiTheme::BadgeBDark, UiTheme::BadgeB);

    std::vector<Texture> chipIcons(commands_.size());
    std::vector<Texture> chipLabels(commands_.size());
    std::vector<Texture> tileImages(commands_.size());
    std::vector<Texture> tileLabels(commands_.size());
    std::vector<Texture> tileAuthors(commands_.size());
    std::vector<std::vector<Texture>> tileRunFrames(commands_.size()); // sized >1 only for an animated _run01.png tile
    std::vector<bool> tileHideLabel(commands_.size(), false); // PREVIEW_HIDE_LABEL
    std::vector<bool> tileFitContain(commands_.size(), false); // PREVIEW_FIT_CONTAIN
    Texture newBadge(Translate("NEW_BADGE"), 13, renderer_, 0, 0, false, ToAbgr(UiTheme::Bg), true);
    const int newBadgePad = 7;
    for(int i = 0; i < (int)commands_.size(); ++i)
    {
        Command & c = commands_[i];
        if(c.command.empty())
            continue; // reserved blank slot, not a chip or tile
        std::string label = Translate(c.name);
        if(i < themeStart_ || i >= pinnedStartIndex_)
        {
            const int labelAvail = chipWidthOf[i] - 2*chipPadX - ChipIconW(i);
            if(c.previewImage.size())
            {
                SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1"); // nearest looks jagged on small glyphs
                Texture icon(c.previewImage, renderer_, 0, 0);
                SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
                FitCentered(icon, 0, 0, chipIconSize, chipIconSize, 0);
                SetColorMod(icon.texture.get(), UiTheme::Accent);
                chipIcons[i] = icon;
            }
            label = TruncateToWidth(label, chipLabelSize, labelAvail);
            chipLabels[i] = Texture(label, chipLabelSize, renderer_, 0, 0, false, ToAbgr(UiTheme::Text), true);
        }
        else
        {
            label = TruncateToWidth(label, 15, TileW - 28);
            tileLabels[i] = Texture(label, 15, renderer_, 0, 0, false, ToAbgr(UiTheme::Text), true);
            if(!items_[i].author.empty())
            {
                std::string author = TruncateToWidth("by: " + items_[i].author, 13, TileW - 20);
                tileAuthors[i] = Texture(author, 13, renderer_, 0, 0, false, ToAbgr(UiTheme::Text), true);
            }
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

        const std::string runSuffix = "01.png";
        bool isRunCycle = c.previewNearest && c.previewImage.size() > runSuffix.size() &&
           c.previewImage.compare(c.previewImage.size() - runSuffix.size(), runSuffix.size(), runSuffix) == 0;

        if(tileFitContain[idx])
            FitCentered(art, 0, 0, TileW, TileH, 0);
        else
            FitCover(art, 0, 0, TileW, TileH, 0);
        tileImages[idx] = art;

        if(isRunCycle)
        {
            std::string base = c.previewImage.substr(0, c.previewImage.size() - runSuffix.size());
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

    // the theme applied on the console
    std::vector<bool> isActiveTheme(commands_.size(), false);
    const std::string activeMarker = options_.titleKey == "INSTALLED_THEMES" ? "/var/lib/clover/profiles/0/hakchi/lastTheme" : options_.selectedMarker;
    auto RefreshActiveTiles = [&]()
    {
        std::fill(isActiveTheme.begin(), isActiveTheme.end(), false);
        std::string activeName;
        if(!activeMarker.empty())
        {
            std::ifstream in(activeMarker);
            std::getline(in, activeName);
        }
        if(!activeName.empty() && activeName.back() == '\r')
            activeName.pop_back();
        if(activeName.empty())
            return;

        const std::string suffix = ".sh";
        for(int i = themeStart_; i < pinnedStartIndex_; ++i)
        {
            const std::string & cmd = commands_[i].command;
            if(cmd.size() <= suffix.size() || cmd.compare(cmd.size()-suffix.size(), suffix.size(), suffix) != 0)
                continue;
            size_t slashPos = cmd.find_last_of('/');
            size_t start = (slashPos == std::string::npos) ? 0 : slashPos+1;
            if(cmd.compare(start, cmd.size()-suffix.size()-start, activeName) == 0)
                isActiveTheme[i] = true;
        }
    };
    RefreshActiveTiles();

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
        SaveFocusState();
    };
    int restoredId = LoadFocusIndex();
    SetCurrentCommand(restoredId >= 0 ? restoredId : (themeStart_ < pinnedStartIndex_ ? themeStart_ : (stripCount > 0 ? StripToCommand(0) : 0)));

    // fixed for the whole screen (only gridTopRow scrolls within them)
    const int lastThemeIndex = pinnedStartIndex_ - 1;
    const int totalRows = (lastThemeIndex - themeStart_ + GridCols) / GridCols;

    for(;;)
    {
        FrameEvent frameEvent = PollFrameEvents();
        if(frameEvent == FrameEvent::Quit)
            return 0;
        if(frameEvent == FrameEvent::PowerButtonPressed)
        {
            ResumeUnderlyingUi();
            break;
        }

        bool inGrid = currentCommandId_ >= themeStart_ && currentCommandId_ < pinnedStartIndex_;

        if(controller_->GetButtonStatus(A) || controller_->GetButtonStatus(START))
        {
            if(ActivateCommand(commands_[currentCommandId_]))
                break;
            RefreshActiveTiles();
            if(storage_)
                storage_->Refresh();
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
            UpdateBButton(tapped);
            // quick tap runs Back/Exit
            if(tapped && pinnedStartIndex_ < (int)commands_.size() && ActivateCommand(commands_[pinnedStartIndex_]))
                break;
        }

        DrawChromeCommon();
        DrawSectionTitle();
        DrawFooterBadges(badgeB);

        {
            int x = GridLeft;
            for(int pos = 0; pos < stripSlotCount; ++pos)
            {
                const int idx = stripSlots[pos];
                const int chipW = chipWidths[pos];
                if(commands_[idx].command.empty())
                {
                    x += chipW + chipGap;
                    continue;
                }
                bool selected = idx == currentCommandId_;
                SDL_Rect chipRect{ x, StripTop, chipW, StripH };
                if(selected)
                {
                    DrawRoundedFillRect(renderer_, chipRect, UiTheme::SelectedRowBg, UiTheme::BoxRadius);
                    DrawStrokeRect(renderer_, chipRect, UiTheme::Accent, 2, UiTheme::BoxRadius);
                }
                else
                    DrawStrokeRect(renderer_, chipRect, UiTheme::Border, UiTheme::BorderWidth, UiTheme::BoxRadius);

                // icon+label centered as one group, not each alone
                Texture & icon = chipIcons[idx];
                Texture & label = chipLabels[idx];
                int iconW = icon.rect.h > 0 ? chipIconSize : 0;
                int groupW = iconW + (iconW > 0 ? chipIconGap : 0) + label.rect.w;
                int gx = x + (chipW - groupW) / 2;
                if(icon.rect.h > 0)
                {
                    icon.rect.x = gx + (chipIconSize - icon.rect.w) / 2;
                    icon.rect.y = StripTop + (StripH - icon.rect.h) / 2;
                    icon.Draw(renderer_);
                    gx += iconW + chipIconGap;
                }
                label.rect.x = gx;
                label.rect.y = StripTop + (StripH - label.rect.h) / 2;
                label.Draw(renderer_);

                if(items_[idx].needsAttention)
                    DrawRoundedFillRect(renderer_, { x + chipW - attentionDotSize - attentionDotInset, StripTop + attentionDotInset, attentionDotSize, attentionDotSize }, UiTheme::Accent, attentionDotSize / 2);

                x += chipW + chipGap;
            }
        }
        noticeText.Draw(renderer_);
        if(storage_)
        {
            storage_->Update();
            storage_->Draw(GridRight, titleText_.rect.y + titleText_.rect.h / 2);
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
                    DrawRoundedFillRect(renderer_, tileRect, UiTheme::SelectedRowBg, UiTheme::BoxRadius);
                    EnsureTileImage(idx);
                    std::vector<Texture> & runFrames = tileRunFrames[idx];
                    Texture & tileArt = runFrames.size() > 1
                        ? runFrames[(SDL_GetTicks() / 100) % runFrames.size()]
                        : tileImages[idx];
                    Texture & author = tileAuthors[idx];
                    if(tileArt.rect.w > 0 || author.rect.w > 0)
                    {
                        // clip to the tile, then mask the clip's square corners back to rounded
                        SDL_RenderSetClipRect(renderer_, &tileRect);
                        if(tileArt.rect.w > 0)
                        {
                            if(tileFitContain[idx])
                                // 10px margin keeps clear of the rounded-corner mask below
                                FitCentered(tileArt, x, y, TileW, TileH, 10);
                            else
                            {
                                FitCover(tileArt, x, y, TileW, TileH, 0);
                                if(items_[idx].previewAlignTop)
                                    tileArt.rect.y = y;
                            }
                            tileArt.Draw(renderer_);
                        }
                        if(author.rect.w > 0)
                        {
                            const int stripH = author.rect.h + 6;
                            const SDL_Rect strip{ x, y + TileH - stripH, TileW, stripH };
                            SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
                            SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0xB4);
                            SDL_RenderFillRect(renderer_, &strip);
                            SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
                            author.rect.x = x + (TileW - author.rect.w) / 2;
                            author.rect.y = strip.y + (stripH - author.rect.h) / 2;
                            author.Draw(renderer_);
                        }
                        SDL_RenderSetClipRect(renderer_, nullptr);
                        DrawRoundedCornerMask(renderer_, tileRect, UiTheme::SelectedRowBg, UiTheme::BoxRadius);
                    }
                    Color stroke = UiTheme::Border;
                    int strokeW = UiTheme::BorderWidth;
                    if(selected)
                    {
                        stroke = UiTheme::Accent;
                        strokeW = 3;
                    }
                    else if(isActiveTheme[idx])
                    {
                        stroke = ActiveThemeBorder;
                        strokeW = 3;
                    }
                    DrawStrokeRect(renderer_, tileRect, stroke, strokeW, UiTheme::BoxRadius);
                    if(items_[idx].isNew)
                    {
                        const SDL_Rect pill{ x + 10, y + 10, newBadge.rect.w + 2 * newBadgePad, newBadge.rect.h + 4 };
                        DrawRoundedFillRect(renderer_, pill, UiTheme::Accent, pill.h / 2);
                        newBadge.rect.x = pill.x + newBadgePad;
                        newBadge.rect.y = pill.y + 2;
                        newBadge.Draw(renderer_);
                    }
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

        if(FinishFrame())
            break;
    }

    return 0;
}

int ThemeManagerApp::RunListLayout()
{
    SDL_Rect selectedRowRect{ UiTheme::ListX, UiTheme::RowFirstY - 2, UiTheme::ListContentRightX - UiTheme::ListX, 0 };

    // B runs Back/Exit, as in the grid
    Badge badgeB = badges_->Make("B", pinnedStartIndex_ < (int)commands_.size() ? Translate(commands_[pinnedStartIndex_].name) : "", UiTheme::BadgeBDark, UiTheme::BadgeB);

    const int RowGlyphSize = 16;
    const int RowTextGapPx = 16;
    const int ChildIndent = 4*16;
    auto BuildRowTextures = [&]()
    {
        for(Command & c : commands_)
        {
            int textX = UiTheme::RowTextX + (c.child ? ChildIndent : 0);
            std::string label = TruncateToWidth(Translate(c.name), RowGlyphSize, UiTheme::RowControlRightX - RowTextGapPx - textX);
            c.texture = Texture(label, RowGlyphSize, renderer_, textX, 0, false, ToAbgr(UiTheme::Text), true);
        }
    };
    BuildRowTextures();

    const int modernRowPitch = std::max(UiTheme::RowPitch, GetTTFLineHeight(RowGlyphSize));
    const int DisplayItemCount = std::max(1, (UiTheme::FooterDividerY - UiTheme::ListBottomMargin - UiTheme::RowFirstY) / modernRowPitch);

    int topListItemNumber = pinnedStartIndex_ + 1; // forces the first SetCurrentCommand to lay out row positions
    bool rowsReloaded = false;
    std::shared_ptr<Texture> PreviewImage;
    auto SetCurrentCommand = [&] (int newCommandId)
    {
        currentCommandId_ = newCommandId;
        const Command & currentCommand = commands_[currentCommandId_];

        // pinned rows don't affect scroll position
        int scrollTarget = std::min(currentCommandId_, std::max(0, pinnedStartIndex_-1));

        bool updateCommandYPos = rowsReloaded;
        rowsReloaded = false;
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

        // a short list never scrolls, or a restored focus would hide earlier rows
        const int maxTopListItemNumber = std::max(0, pinnedStartIndex_-DisplayItemCount);
        if(topListItemNumber > maxTopListItemNumber)
        {
            topListItemNumber = maxTopListItemNumber;
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
        SaveFocusState();
    };
    int restoredId = LoadFocusIndex();
    SetCurrentCommand(restoredId >= 0 && restoredId < pinnedStartIndex_ ? restoredId : 0);

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
            DrawHLine(renderer_, UiTheme::ListX, UiTheme::ListContentRightX, rowCommand.texture.rect.y + rowCommand.texture.rect.h + 3, UiTheme::Border);
    };

    auto DrawChrome = [&]()
    {
        DrawChromeCommon();
        DrawSectionTitle();
        DrawFooterBadges(badgeB);

        DrawRoundedFillRect(renderer_, selectedRowRect, UiTheme::SelectedRowBg, UiTheme::BoxRadius);
        DrawStrokeRect(renderer_, selectedRowRect, UiTheme::Accent, 2, UiTheme::BoxRadius);

        if(PreviewImage.get())
        {
            SDL_Rect previewBox{ UiTheme::DetailX, UiTheme::PreviewBoxY, UiTheme::DetailW, UiTheme::PreviewBoxH };
            DrawStrokeRect(renderer_, previewBox, UiTheme::Border, UiTheme::BorderWidth, UiTheme::BoxRadius);
            PreviewImage->Draw(renderer_);
        }
    };

    for(;;)
    {
        FrameEvent frameEvent = PollFrameEvents();
        if(frameEvent == FrameEvent::Quit)
            return 0;
        if(frameEvent == FrameEvent::PowerButtonPressed)
        {
            ResumeUnderlyingUi();
            break;
        }

        if(controller_->GetButtonStatus(A) || controller_->GetButtonStatus(START))
        {
            const bool isInternalToggle = commands_[currentCommandId_].runInternal && commands_[currentCommandId_].isToggle;
            const std::string focusedName = commands_[currentCommandId_].name;
            if(ActivateCommand(commands_[currentCommandId_]))
                break;
            if(isInternalToggle)
            {
                ReloadCommands();
                BuildRowTextures();
                int focusId = std::min(currentCommandId_, std::max(0, pinnedStartIndex_-1));
                for(int i = 0; i < pinnedStartIndex_; ++i)
                    if(commands_[i].name == focusedName)
                    {
                        focusId = i;
                        break;
                    }
                rowsReloaded = true;
                SetCurrentCommand(focusId);
            }
        }
        else if(controller_->HeldRepeat(UP) && pinnedStartIndex_ > 0)
        {
            // bounded for an all-separator list; Back/Exit isn't a row
            int newCommandId = currentCommandId_;
            for(int tries = 0; tries < pinnedStartIndex_; ++tries)
            {
                newCommandId = (newCommandId-1+pinnedStartIndex_)%pinnedStartIndex_;
                if(commands_[newCommandId].command.size() != 0)
                    break;
            }
            SetCurrentCommand(newCommandId);
        }
        else if(controller_->HeldRepeat(DOWN) && pinnedStartIndex_ > 0)
        {
            int newCommandId = currentCommandId_;
            for(int tries = 0; tries < pinnedStartIndex_; ++tries)
            {
                newCommandId = (newCommandId+1)%pinnedStartIndex_;
                if(commands_[newCommandId].command.size() != 0)
                    break;
            }
            SetCurrentCommand(newCommandId);
        }
        {
            bool tapped = false;
            UpdateBButton(tapped);
            // quick tap runs Back/Exit
            if(tapped && pinnedStartIndex_ < (int)commands_.size() && ActivateCommand(commands_[pinnedStartIndex_]))
                break;
        }

        DrawChrome();

        for(int i = 0, count = std::min(DisplayItemCount, pinnedStartIndex_-topListItemNumber); i < count; ++i)
            DrawRow(commands_[i+topListItemNumber], (i+topListItemNumber) == pinnedStartIndex_-1);

        // bounded by pinnedStartIndex_, not commands_.size() - pinned rows need no scroll
        scrollArrows_->Draw(topListItemNumber != 0, (topListItemNumber + DisplayItemCount) < pinnedStartIndex_);

        if(FinishFrame())
            break;
    }

    return 0;
}

/**
  * Copyright (c) 2026 DefKorns (https://defkorns.github.io/LICENSE)
  *
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  */

#include "theme_manager_app.h"
#include "folder_theme_model.h"
#include "texture_utils.h"
#include "framework/draw_helpers.h"
#include "framework/powerwatch.h"
#include "localization.h"

#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

namespace
{
    const int ListRight = 760;
    const int DetailX = 800;
    const int DetailW = 384;
    const int PreviewH = 216;
    const int TreeIndent = 26;
    const int RowGlyphSize = 16;
    const int ValueMaxW = 250;
    const int ChevronGap = 10;

    const SDL_Rect PickerRect{ DetailX - 16, UiTheme::HeaderDividerY + 14, DetailW + 32, UiTheme::FooterDividerY - UiTheme::HeaderDividerY - 28 };
    const int PickerHeaderH = 44;
    const int PickerRowH = 40;
    const int ThumbW = 60, ThumbH = 34;

    bool ReadParameter(const std::string & name)
    {
        std::ifstream in("/sys/module/clovercon/parameters/" + name);
        int value = 0;
        return (in >> value) && value != 0;
    }

    // hakchi's "Use X/Y on Classic Controller as autofire A/B" - Y then arrives as B presses
    bool PadHasY()
    {
        return ReadSftype() != "nes" && !ReadParameter("autofire_xy");
    }

    std::string Format(const std::string & key, const std::string & value)
    {
        std::string text = Translate(key);
        size_t at = text.find("%s");
        return at == std::string::npos ? text : text.replace(at, 2, value);
    }
}

int ThemeManagerApp::RunFolderThemesLayout()
{
    FolderThemeModel model;
    if(!model.Load(options_.folderThemesDir, options_.folderThemesMapping, options_.folderMusicMapping))
        return 1;
    const std::vector<FolderEntry> & folders = model.Folders();
    const int folderCount = static_cast<int>(folders.size());
    const bool hasY = PadHasY();
    const Channel channel = options_.folderMusicScreen ? Channel::Music : Channel::Theme;
    auto OptionCount = [&]() { return static_cast<int>(model.Options(channel).size()) + 1; }; // 0 = Automatic

    const int rowPitch = std::max(UiTheme::RowPitch, GetTTFLineHeight(RowGlyphSize));
    const int displayRows = std::max(1, (UiTheme::FooterDividerY - UiTheme::ListBottomMargin - UiTheme::RowFirstY) / rowPitch);
    const int pickerRows = (PickerRect.h - PickerHeaderH - 8) / PickerRowH;

    Texture chevron(optionsLocation_ + UiTheme::AssetChevronRight, renderer_);
    SetColorMod(chevron.texture.get(), UiTheme::Accent);
    Texture scrollUp = MakeScrollArrow(UiTheme::ScrollX, UiTheme::ScrollUpY);
    Texture scrollDown = scrollUp;
    scrollDown.rect.y = UiTheme::ScrollDownY;

    auto MakeText = [&](const std::string & text, int size, Color color) { return Texture(text, size, renderer_, 0, 0, false, ToAbgr(color), true); };
    auto MakeBadge = [&](const char * letter, const std::string & labelKey, Color rim, Color fill)
    {
        return Badge{ MakeText(letter, 16, UiTheme::BadgeLetter), MakeText(Translate(labelKey), 16, UiTheme::Text), rim, fill };
    };
    const Badge listBadgeA = MakeBadge("A", "FOLDER_THEME_LIST", UiTheme::BadgeADark, UiTheme::BadgeA);
    const Badge pickBadgeA = MakeBadge("A", "HINT_SELECT", UiTheme::BadgeADark, UiTheme::BadgeA);
    Badge backBadge = MakeBadge("B", "BACK", UiTheme::BadgeBDark, UiTheme::BadgeB);
    Badge cancelBadge = MakeBadge("B", "FOLDER_THEME_CANCEL", UiTheme::BadgeBDark, UiTheme::BadgeB);
    Badge autoBadge = MakeBadge("Y", "AUTOMATIC", UiTheme::BadgeYDark, UiTheme::BadgeY);
    Texture changeLabel = MakeText(Translate("FOLDER_THEME_CHANGE"), 16, UiTheme::Text);

    auto OptionName = [&](Channel ch, const ResolvedOption & resolved)
    {
        if(resolved.option >= 0)
            return model.Options(ch)[resolved.option].title;
        switch(resolved.source)
        {
        case OptionSource::ThemeMusic: return Translate("FOLDER_MUSIC_THEME");
        case OptionSource::RandomMusic: return Translate("FOLDER_MUSIC_RANDOM");
        case OptionSource::OriginalMusic: return Translate("FOLDER_MUSIC_ORIGINAL");
        default: return Translate(model.GlobalThemeIsRandom() ? "FOLDER_THEME_RANDOM" : "FOLDER_THEME_STOCK");
        }
    };
    auto AutomaticLabel = [&](int folder) { return Translate("AUTOMATIC") + " · " + OptionName(channel, model.ResolveAutomatic(channel, folder)); };

    const std::vector<FolderOption> & themes = model.Options(Channel::Theme);
    std::vector<Texture> previews(themes.size());
    std::vector<char> previewLoaded(themes.size(), 0); // not vector<bool>: Preview() needs a real reference
    Texture fallbackPreview;
    char fallbackLoaded = 0;
    auto Preview = [&](int theme) -> Texture &
    {
        char & loaded = theme >= 0 ? previewLoaded[theme] : fallbackLoaded;
        Texture & slot = theme >= 0 ? previews[theme] : fallbackPreview;
        if(!loaded)
        {
            loaded = 1;
            std::string path = theme >= 0 ? themes[theme].preview : "";
            if(path.empty() || !std::ifstream(path).good())
                path = optionsLocation_ + (theme < 0 && model.GlobalThemeIsRandom() ? "images/preview_randtheme.png" : "images/preview_default.png");
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
            slot = Texture(path, renderer_, 0, 0);
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
        }
        return slot;
    };
    auto DrawCover = [&](Texture art, const SDL_Rect & box, int radius, Color around)
    {
        DrawRoundedFillRect(renderer_, box, UiTheme::SelectedRowBg, radius);
        if(art.rect.w <= 0)
            return;
        SDL_RenderSetClipRect(renderer_, &box);
        FitCover(art, box.x, box.y, box.w, box.h, 0);
        art.Draw(renderer_);
        SDL_RenderSetClipRect(renderer_, nullptr);
        DrawRoundedCornerMask(renderer_, box, around, radius);
    };

    std::vector<Texture> nameTex(folders.size()), valueTex(folders.size());
    auto RebuildRows = [&]()
    {
        for(int i = 0; i < folderCount; ++i)
        {
            int own = model.Assigned(channel, i);
            std::string value = own >= 0 ? model.Options(channel)[own].title : AutomaticLabel(i);
            valueTex[i] = MakeText(TruncateToWidth(value, RowGlyphSize, ValueMaxW), RowGlyphSize, own >= 0 ? UiTheme::Text : UiTheme::TextDim);
            int textX = UiTheme::RowTextX + folders[i].depth * TreeIndent + (folders[i].depth > 0 ? 14 : 0);
            int nameAvail = ListRight - 16 - valueTex[i].rect.w - 2 * (chevron.rect.w + ChevronGap) - 16 - textX;
            nameTex[i] = MakeText(TruncateToWidth(folders[i].name, RowGlyphSize, nameAvail), RowGlyphSize, UiTheme::Text);
            nameTex[i].rect.x = textX;
        }
    };

    int selected = 0, topRow = 0;
    Texture detailName, detailTag, detailSource, detailUsage;
    Color detailTagFill{};
    int detailTheme = -1;
    auto RebuildDetail = [&]()
    {
        detailTheme = model.Resolve(Channel::Theme, selected).option;
        ResolvedOption resolved = model.Resolve(channel, selected);
        detailName = MakeText(TruncateToWidth(OptionName(channel, resolved), 22, DetailW), 22, UiTheme::Text);

        std::string tagKey = "FOLDER_THEME_TAG_GLOBAL", sourceText = Translate("FOLDER_THEME_SOURCE_GLOBAL");
        Color tagText = UiTheme::Text;
        detailTagFill = UiTheme::Border;
        switch(resolved.source)
        {
        case OptionSource::Assigned:
            tagKey = "FOLDER_THEME_TAG_ASSIGNED";
            sourceText = Translate("FOLDER_THEME_SOURCE_ASSIGNED");
            detailTagFill = UiTheme::Accent;
            tagText = UiTheme::Bg;
            break;
        case OptionSource::NameMatch:
            tagKey = "FOLDER_THEME_TAG_NAME";
            sourceText = Translate("FOLDER_THEME_SOURCE_NAME");
            break;
        case OptionSource::Inherited:
            tagKey = "FOLDER_THEME_TAG_INHERITED";
            sourceText = Format("FOLDER_THEME_SOURCE_INHERITED", folders[resolved.fromFolder].name);
            detailTagFill = UiTheme::BadgeXDark;
            break;
        case OptionSource::ThemeMusic:
            tagKey = "FOLDER_MUSIC_TAG_THEME";
            sourceText = Translate("FOLDER_MUSIC_SOURCE_THEME");
            break;
        case OptionSource::RandomMusic:
            tagKey = "FOLDER_MUSIC_TAG_RANDOM";
            sourceText = Translate("FOLDER_MUSIC_SOURCE_RANDOM");
            break;
        case OptionSource::OriginalMusic:
            tagKey = "FOLDER_MUSIC_TAG_ORIGINAL";
            sourceText = Translate("FOLDER_MUSIC_SOURCE_ORIGINAL");
            break;
        case OptionSource::Global:
            break;
        }
        detailTag = MakeText(Translate(tagKey), 13, tagText);
        int tagW = detailTag.rect.w + 16;
        detailSource = MakeText(TruncateToWidth(sourceText, 16, DetailW - tagW - 10), 16, UiTheme::Text);

        int usage = resolved.option >= 0 ? model.UsageCount(channel, resolved.option) : 0;
        std::string usageText;
        if(resolved.option >= 0)
            usageText = usage > 1 ? Format("FOLDER_THEME_USED_IN", std::to_string(usage))
                      : Translate(usage == 1 ? "FOLDER_THEME_USED_ONCE" : "FOLDER_THEME_UNUSED");
        detailUsage = MakeText(usageText, 14, UiTheme::TextDim);
    };

    auto Select = [&](int folder)
    {
        selected = folder;
        if(selected < topRow)
            topRow = selected;
        else if(selected >= topRow + displayRows)
            topRow = selected - displayRows + 1;
        RebuildDetail();
    };
    auto Assign = [&](int option)
    {
        if(model.Assigned(channel, selected) == option)
            return;
        model.Assign(channel, selected, option);
        model.Save(channel);
        RebuildRows();
        RebuildDetail();
    };
    auto Cycle = [&](int step)
    {
        int option = model.Assigned(channel, selected) + 1;
        Assign((option + step + OptionCount()) % OptionCount() - 1);
    };

    bool picking = false;
    int pick = 0, pickTop = 0;
    Texture pickerTitle;
    std::vector<Texture> optionTex;
    auto OpenPicker = [&]()
    {
        picking = true;
        pick = model.Assigned(channel, selected) + 1;
        pickTop = std::max(0, std::min(pick - pickerRows / 2, OptionCount() - pickerRows));
        pickerTitle = MakeText(TruncateToWidth(Format(channel == Channel::Theme ? "FOLDER_THEME_FOR" : "FOLDER_MUSIC_FOR", folders[selected].name), 16, PickerRect.w - 32), 16, UiTheme::Text);
        optionTex.clear();
        int labelW = PickerRect.w - 24 - (channel == Channel::Theme ? ThumbW + 12 : 8) - 24;
        optionTex.push_back(MakeText(TruncateToWidth(AutomaticLabel(selected), 16, labelW), 16, UiTheme::Text));
        for(const FolderOption & option : model.Options(channel))
            optionTex.push_back(MakeText(TruncateToWidth(option.title, 16, labelW), 16, UiTheme::Text));
    };
    auto MovePick = [&](int to)
    {
        pick = std::max(0, std::min(OptionCount() - 1, to));
        if(pick < pickTop)
            pickTop = pick;
        else if(pick >= pickTop + pickerRows)
            pickTop = pick - pickerRows + 1;
    };

    RebuildRows();
    Select(0);

    const bool hasBack = pinnedStartIndex_ < static_cast<int>(commands_.size());
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

        if(picking)
        {
            if(controller_->GetButtonStatus(A) || controller_->GetButtonStatus(START))
            {
                Assign(pick - 1);
                picking = false;
            }
            else if(controller_->GetButtonStatus(B))
                picking = false;
            else if(controller_->HeldRepeat(UP))
                MovePick(pick - 1);
            else if(controller_->HeldRepeat(DOWN))
                MovePick(pick + 1);
        }
        else
        {
            if(controller_->GetButtonStatus(A) || controller_->GetButtonStatus(START))
                OpenPicker();
            else if(controller_->GetButtonStatus(B))
            {
                if(hasBack && ActivateCommand(commands_[pinnedStartIndex_]))
                    break;
            }
            else if(hasY && controller_->GetButtonStatus(Y))
                Assign(-1);
            else if(controller_->HeldRepeat(UP))
                Select((selected - 1 + folderCount) % folderCount);
            else if(controller_->HeldRepeat(DOWN))
                Select((selected + 1) % folderCount);
            else if(controller_->HeldRepeat(LEFT))
                Cycle(-1);
            else if(controller_->HeldRepeat(RIGHT))
                Cycle(1);
        }

        badgeA_ = picking ? pickBadgeA : listBadgeA;
        DrawChromeCommon();
        DrawSectionTitle();
        {
            int rightEdge = UiTheme::BadgeClusterRightX - UiTheme::BadgeOuterSize - UiTheme::BadgeLabelGap - badgeA_.label.rect.w - UiTheme::BadgeGroupGap;
            if(picking)
                DrawBadge(cancelBadge, rightEdge);
            else
            {
                if(hasBack)
                    rightEdge = DrawBadge(backBadge, rightEdge);
                if(hasY)
                    rightEdge = DrawBadge(autoBadge, rightEdge);
                int midY = UiTheme::BadgeBandY + UiTheme::BadgeOuterSize / 2;
                int groupW = 2 * chevron.rect.w + 4 + UiTheme::BadgeLabelGap + changeLabel.rect.w;
                int x = rightEdge - groupW;
                chevron.rect.x = x;
                chevron.rect.y = midY - chevron.rect.h / 2;
                chevron.Draw(renderer_, SDL_FLIP_HORIZONTAL);
                chevron.rect.x = x + chevron.rect.w + 4;
                chevron.Draw(renderer_);
                changeLabel.rect.x = chevron.rect.x + chevron.rect.w + UiTheme::BadgeLabelGap;
                changeLabel.rect.y = midY - changeLabel.rect.h / 2;
                changeLabel.Draw(renderer_);
            }
        }

        for(int row = 0; row < displayRows && topRow + row < folderCount; ++row)
        {
            int i = topRow + row;
            int y = UiTheme::RowFirstY + row * rowPitch;
            bool isSelected = i == selected;
            SDL_Rect rowRect{ UiTheme::ListX, y - 2, ListRight - UiTheme::ListX, rowPitch - 2 };
            if(isSelected)
            {
                DrawRoundedFillRect(renderer_, rowRect, UiTheme::SelectedRowBg, UiTheme::BoxRadius);
                DrawStrokeRect(renderer_, rowRect, UiTheme::Accent, 2, UiTheme::BoxRadius);
            }
            else if(i + 1 < folderCount && row + 1 < displayRows)
                DrawHLine(renderer_, UiTheme::ListX, ListRight, y + rowPitch - 3, UiTheme::Border);

            int midY = y - 2 + (rowPitch - 2) / 2;
            if(folders[i].depth > 0)
            {
                int twigX = nameTex[i].rect.x - 14;
                DrawVLine(renderer_, twigX, y + 6, midY, UiTheme::Border, 2);
                DrawHLine(renderer_, twigX, twigX + 9, midY, UiTheme::Border, 2);
            }
            nameTex[i].rect.y = midY - nameTex[i].rect.h / 2;
            nameTex[i].Draw(renderer_);

            int valueRight = ListRight - 16;
            if(isSelected && !picking)
            {
                chevron.rect.x = valueRight - chevron.rect.w;
                chevron.rect.y = midY - chevron.rect.h / 2;
                chevron.Draw(renderer_);
                valueRight = chevron.rect.x - ChevronGap;
            }
            valueTex[i].rect.x = valueRight - valueTex[i].rect.w;
            valueTex[i].rect.y = midY - valueTex[i].rect.h / 2;
            valueTex[i].Draw(renderer_);
            if(isSelected && !picking)
            {
                chevron.rect.x = valueTex[i].rect.x - ChevronGap - chevron.rect.w;
                chevron.Draw(renderer_, SDL_FLIP_HORIZONTAL);
            }
        }
        if(topRow > 0)
            scrollUp.Draw(renderer_);
        if(topRow + displayRows < folderCount)
            scrollDown.Draw(renderer_, SDL_FLIP_VERTICAL);

        {
            SDL_Rect previewBox{ DetailX, UiTheme::RowFirstY, DetailW, PreviewH };
            DrawCover(Preview(detailTheme), previewBox, UiTheme::BoxRadius, UiTheme::Bg);
            DrawStrokeRect(renderer_, previewBox, UiTheme::Border, UiTheme::BorderWidth, UiTheme::BoxRadius);
            int y = previewBox.y + PreviewH + 16;
            detailName.rect.x = DetailX;
            detailName.rect.y = y;
            detailName.Draw(renderer_);
            y += detailName.rect.h + 12;
            SDL_Rect tagRect{ DetailX, y, detailTag.rect.w + 16, detailTag.rect.h + 8 };
            DrawRoundedFillRect(renderer_, tagRect, detailTagFill, 6);
            detailTag.rect.x = tagRect.x + 8;
            detailTag.rect.y = tagRect.y + 4;
            detailTag.Draw(renderer_);
            detailSource.rect.x = tagRect.x + tagRect.w + 10;
            detailSource.rect.y = tagRect.y + (tagRect.h - detailSource.rect.h) / 2;
            detailSource.Draw(renderer_);
            detailUsage.rect.x = DetailX;
            detailUsage.rect.y = tagRect.y + tagRect.h + 12;
            detailUsage.Draw(renderer_);
        }

        if(picking)
        {
            DrawRoundedFillRect(renderer_, PickerRect, UiTheme::Bg, UiTheme::BoxRadius);
            DrawStrokeRect(renderer_, PickerRect, UiTheme::Accent, 2, UiTheme::BoxRadius);
            pickerTitle.rect.x = PickerRect.x + 16;
            pickerTitle.rect.y = PickerRect.y + (PickerHeaderH - pickerTitle.rect.h) / 2;
            pickerTitle.Draw(renderer_);

            const bool thumbs = channel == Channel::Theme;
            int current = model.Assigned(channel, selected) + 1;
            for(int row = 0; row < pickerRows && pickTop + row < OptionCount(); ++row)
            {
                int option = pickTop + row;
                SDL_Rect rowRect{ PickerRect.x + 8, PickerRect.y + PickerHeaderH + row * PickerRowH, PickerRect.w - 16, PickerRowH - 4 };
                if(option == pick)
                {
                    DrawRoundedFillRect(renderer_, rowRect, UiTheme::SelectedRowBg, UiTheme::BoxRadius);
                    DrawStrokeRect(renderer_, rowRect, UiTheme::Accent, 2, UiTheme::BoxRadius);
                }
                int labelX = rowRect.x + 16;
                if(thumbs)
                {
                    SDL_Rect thumb{ rowRect.x + 8, rowRect.y + (rowRect.h - ThumbH) / 2, ThumbW, ThumbH };
                    if(option == 0)
                        DrawStrokeRect(renderer_, thumb, UiTheme::Border, 2, 4);
                    else
                        DrawCover(Preview(option - 1), thumb, 4, option == pick ? UiTheme::SelectedRowBg : UiTheme::Bg);
                    labelX = thumb.x + ThumbW + 12;
                }
                Texture & label = optionTex[option];
                label.rect.x = labelX;
                label.rect.y = rowRect.y + (rowRect.h - label.rect.h) / 2;
                label.Draw(renderer_);
                if(option == current)
                    DrawRoundedFillRect(renderer_, { rowRect.x + rowRect.w - 22, rowRect.y + rowRect.h / 2 - 5, 10, 10 }, UiTheme::BadgeA, 5);
            }
        }

        SetDrawColor(renderer_, bg_);
        sdlContext_->EndFrame();
    }

    return 0;
}

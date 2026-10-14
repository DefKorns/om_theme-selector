/**
  * Copyright (c) 2026 DefKorns (https://defkorns.github.io/LICENSE)
  *
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  */

#include "storage_indicator.h"
#include "framework/draw_helpers.h"
#include "localization.h"

#include <algorithm>
#include <cstdio>
#include <sys/stat.h>
#include <sys/statvfs.h>

namespace
{
    constexpr const char * UsbPath = "/media";
    constexpr const char * NandPath = "/var/lib/hakchi";
    constexpr int FontSize = 14;
    constexpr int BarW = 110;
    constexpr int BarH = 10;
    constexpr int InnerGap = 10;
    constexpr int MeterGap = 32;
    constexpr double LowFree = 0.25;
    constexpr double CriticalFree = 0.10;
    constexpr Color Healthy{ 0x4C, 0xAF, 0x6E };

    bool UsbMounted()
    {
        struct stat media, root;
        return stat(UsbPath, &media) == 0 && stat("/", &root) == 0 && media.st_dev != root.st_dev;
    }

    std::string FormatFree(double bytes)
    {
        const double mb = bytes / (1024.0 * 1024.0);
        char text[32];
        if(mb >= 1024.0)
            std::snprintf(text, sizeof(text), "%.1f GB", mb / 1024.0);
        else
            std::snprintf(text, sizeof(text), "%.1f MB", mb);
        return text;
    }

    Color LevelColor(double freeFraction)
    {
        if(freeFraction < CriticalFree)
            return UiTheme::BadgeB;
        if(freeFraction < LowFree)
            return UiTheme::Accent;
        return Healthy;
    }
}

StorageIndicator::StorageIndicator(SDL_Renderer * renderer)
    : renderer_(renderer)
{
    Refresh();
}

void StorageIndicator::Update()
{
    if(SDL_GetTicks() - lastRefreshMs_ >= RefreshMs)
        Refresh();
}

void StorageIndicator::Refresh()
{
    lastRefreshMs_ = SDL_GetTicks();
    const bool usb = UsbMounted();
    if(meters_.size() != (usb ? 2u : 1u))
    {
        meters_.assign(usb ? 2 : 1, Meter{});
        meters_[0].name = "NAND";
        meters_[0].path = NandPath;
        if(usb)
        {
            meters_[1].name = "USB";
            meters_[1].path = UsbPath;
        }
    }
    destination_ = static_cast<int>(meters_.size()) - 1;
    for(int i = 0; i < static_cast<int>(meters_.size()); ++i)
        ReadMeter(meters_[i], i == destination_);
}

void StorageIndicator::ReadMeter(Meter & meter, bool isDestination)
{
    struct statvfs vfs;
    if(statvfs(meter.path.c_str(), &vfs) != 0 || vfs.f_blocks == 0)
        return;
    const double blockSize = static_cast<double>(vfs.f_frsize);
    const double total = vfs.f_blocks * blockSize;
    const double available = vfs.f_bavail * blockSize;
    meter.usedFraction = (vfs.f_blocks - vfs.f_bfree) * blockSize / total;
    meter.freeFraction = available / total;

    const std::string valueText = FormatFree(available) + " " + Translate("FREE_SPACE");
    if(valueText == meter.valueText && meter.label.texture)
        return;
    meter.valueText = valueText;
    const Uint32 textColor = ToAbgr(isDestination ? UiTheme::Text : UiTheme::TextDim);
    meter.label = Texture(meter.name, FontSize, renderer_, 0, 0, false, textColor, true);
    meter.value = Texture(valueText, FontSize, renderer_, 0, 0, false, textColor, true);
}

void StorageIndicator::Draw(int rightX, int centerY)
{
    int x = rightX;
    for(int i = static_cast<int>(meters_.size()) - 1; i >= 0; --i)
    {
        Meter & meter = meters_[i];
        x -= meter.value.rect.w;
        meter.value.rect.x = x;
        meter.value.rect.y = centerY - meter.value.rect.h / 2;
        meter.value.Draw(renderer_);

        x -= InnerGap + BarW;
        const SDL_Rect track{ x, centerY - BarH / 2, BarW, BarH };
        DrawRoundedFillRect(renderer_, track, UiTheme::Border, BarH / 2);
        const int usedW = static_cast<int>(BarW * meter.usedFraction + 0.5);
        if(usedW > 0)
        {
            const Color level = i == destination_ ? LevelColor(meter.freeFraction) : UiTheme::TextDim;
            DrawRoundedFillRect(renderer_, { x, track.y, std::max(usedW, BarH), BarH }, level, BarH / 2);
        }

        x -= InnerGap + meter.label.rect.w;
        meter.label.rect.x = x;
        meter.label.rect.y = centerY - meter.label.rect.h / 2;
        meter.label.Draw(renderer_);

        x -= MeterGap;
    }
}

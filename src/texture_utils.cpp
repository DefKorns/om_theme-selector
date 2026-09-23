/**
  * Copyright (c) 2026 DefKorns (https://defkorns.github.io/LICENSE)
  *
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  */

#include "texture_utils.h"
#include "framework/utf8.h"

#include <algorithm>

namespace {

void FitScaled(Texture & tex, int boxX, int boxY, int boxW, int boxH, int padding, bool cover)
{
    const int maxW = boxW - 2*padding;
    const int maxH = boxH - 2*padding;
    if(tex.rect.w <= 0 || tex.rect.h <= 0)
        return;
    const double scaleW = static_cast<double>(maxW) / tex.rect.w;
    const double scaleH = static_cast<double>(maxH) / tex.rect.h;
    const double scale = cover ? std::max(scaleW, scaleH) : std::min(scaleW, scaleH);
    tex.rect.w = static_cast<int>(tex.rect.w * scale);
    tex.rect.h = static_cast<int>(tex.rect.h * scale);
    tex.rect.x = boxX + (boxW - tex.rect.w) / 2;
    tex.rect.y = boxY + (boxH - tex.rect.h) / 2;
}

} // namespace

void FitCentered(Texture & tex, int boxX, int boxY, int boxW, int boxH, int padding)
{
    FitScaled(tex, boxX, boxY, boxW, boxH, padding, false);
}

void FitCover(Texture & tex, int boxX, int boxY, int boxW, int boxH, int padding)
{
    FitScaled(tex, boxX, boxY, boxW, boxH, padding, true);
}

std::string TruncateToWidth(const std::string & text, int glyphSize, int available)
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

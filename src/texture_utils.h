/**
  * Copyright (c) 2026 DefKorns (https://defkorns.github.io/LICENSE)
  *
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  */

#ifndef TEXTURE_UTILS_H_
#define TEXTURE_UTILS_H_

#include "framework/sdl_helper.h"

#include <string>

void FitCentered(Texture & tex, int boxX, int boxY, int boxW, int boxH, int padding); // letterboxed
void FitCover(Texture & tex, int boxX, int boxY, int boxW, int boxH, int padding); // cropped
std::string TruncateToWidth(const std::string & text, int glyphSize, int available);

#endif

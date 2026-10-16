/**
  * Copyright (c) 2026 DefKorns (https://defkorns.github.io/LICENSE)
  *
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  */

#ifndef STORAGE_INDICATOR_H_
#define STORAGE_INDICATOR_H_

#include "framework/sdl_helper.h"

#include <string>
#include <vector>

class StorageIndicator
{
public:
    explicit StorageIndicator(SDL_Renderer * renderer);

    void Update();
    void Refresh();
    void Draw(int rightX, int centerY);

private:
    struct Meter
    {
        std::string name;
        std::string path;
        double usedFraction = 0;
        double freeFraction = 1;
        std::string valueText;
        Texture label;
        Texture value;
    };

    static constexpr Uint32 RefreshMs = 2000;

    SDL_Renderer * renderer_;
    std::vector<Meter> meters_;
    int destination_ = 0;
    Uint32 lastRefreshMs_ = 0;

    void ReadMeter(Meter & meter, bool isDestination);
};

#endif

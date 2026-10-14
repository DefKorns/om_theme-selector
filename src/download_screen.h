/**
  * Copyright (c) 2026 DefKorns (https://defkorns.github.io/LICENSE)
  *
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  */

#ifndef DOWNLOAD_SCREEN_H_
#define DOWNLOAD_SCREEN_H_

#include "command.h"
#include "framework/controller.h"
#include "framework/sdl_helper.h"

#include <string>

// Splash with a progress bar for a theme download; returns on its own once the command exits 0.
bool RunDownloadCommand(const std::string & command, const std::string & title, const std::string & author,
                        SDL_Context & sdlContext, Controller & controller, const ModernChrome & chrome);

#endif

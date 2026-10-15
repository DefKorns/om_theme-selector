#!/usr/bin/env bash
set -euo pipefail

[ -f vendor/hmod-build/docker-make.sh ] || git submodule update --init vendor/hmod-build
[ -f vendor/OptionsMenu/src/framework/sdl_context.cpp ] || git submodule update --init --recursive vendor/OptionsMenu

sh vendor/hmod-build/docker-make.sh "$@"

ls -la mod/etc/options_menu/lib/theme_manager out/*.hmod 2>/dev/null

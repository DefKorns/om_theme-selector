#!/usr/bin/env bash
set -euo pipefail

# stops Git Bash from mangling the /src path below into a Windows path
export MSYS_NO_PATHCONV=1

IMAGE="classicmini-cross-toolchain:jessie-armhf"

if [ ! -f toolchain/Dockerfile.jessie-armhf ]; then
    echo "==> Fetching toolchain submodule..."
    git submodule update --init --recursive toolchain
fi

if [ ! -f vendor/OptionsMenu/src/framework/sdl_context.cpp ]; then
    echo "==> Fetching vendor/OptionsMenu submodule..."
    git submodule update --init --recursive vendor/OptionsMenu
fi

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    echo "==> Building Docker image..."
    docker build -f toolchain/Dockerfile.jessie-armhf -t "$IMAGE" toolchain
fi

echo "==> Running make inside the container..."
docker run --rm -v "$PWD:/src" "$IMAGE" make -f Makefile.docker CROSS_PREFIX=arm-linux-gnueabihf- "$@"

echo "==> Build complete:"
ls -la mod/etc/options_menu/lib/theme_manager mod/bin/theme_downloader 2>/dev/null

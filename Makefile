SHELL = /bin/sh
UNAME := $(shell uname)
MOD_NAME := Options Menu - Theme Selector
MOD_CREATOR := DefKorns
MOD_CATEGORY := User Interface

LAST_TAG_COMMIT := $(shell git rev-list --tags --max-count=1)
LAST_TAG := $(shell git describe --tags $(LAST_TAG_COMMIT) 2> /dev/null || echo "v0.0.0")
TAG_PREFIX := "v"
CURRENT_BRANCH := $(shell git rev-parse --abbrev-ref HEAD)
GIT_REMOTES    := $(shell git remote | xargs echo )
GIT_DIRTY      := $(shell git diff --shortstat 2> /dev/null | tail -n1 )
GET_VER    := $(shell git describe --tags $(LAST_TAG_COMMIT) 2> /dev/null | sed "s/^$(TAG_PREFIX)//" || echo "0.0.0")
MOD_VER    := $(shell [ -f VERSION ] && head VERSION || echo $(GET_VER))
MAJOR      := $(shell echo $(MOD_VER) | sed "s/^\([0-9]*\).*/\1/")
MINOR      := $(shell echo $(MOD_VER) | sed "s/[0-9]*\.\([0-9]*\).*/\1/")
PATCH      := $(shell echo $(MOD_VER) | sed "s/[0-9]*\.[0-9]*\.\([0-9]*\).*/\1/")
RC		   := $(shell echo $(MOD_VER) | sed -n "s/.*rc\([0-9]*\)$$/\1/p")
RC		   := $(if $(RC),$(RC),0)

# total number of commits
BUILD      := $(shell git log --oneline | wc -l | sed -e "s/[ \t]*//g")
NEXT_MAJOR_VERSION := $(shell expr $(MAJOR) + 1).0.0
NEXT_MINOR_VERSION := $(MAJOR).$(shell expr $(MINOR) + 1).0-b$(BUILD)
NEXT_PATCH_VERSION := $(MAJOR).$(MINOR).$(shell expr $(PATCH) + 1)-b$(BUILD)
NEXT_RC_VERSION := $(MAJOR).$(MINOR).$(PATCH)-rc$(shell expr $(RC) + 1)

MOD_URL := $(shell git config --get remote.origin.url)
GIT_COMMIT := $(shell git rev-parse --short HEAD)$(shell git diff-index --quiet HEAD -- || echo -dirty)
# Derive the package name from the remote; Docker mounts every repo at /src.
MOD_FILENAME := $(shell basename $(MOD_URL) .git)
DEV_DIR := $(CURDIR)
OUT := $(DEV_DIR)/out

# Build without Docker when the ARM toolchain and dependencies are installed.
# Set CROSS_PREFIX=arm-linux-gnueabihf- to cross-compile for the console;
# leave unset to build natively.
FRAMEWORK_DIR = vendor/OptionsMenu/src/framework
CXX = g++
STRIP = strip
ifdef CROSS_PREFIX
PKG_CONFIG_LIBDIR = /usr/lib/arm-linux-gnueabihf/pkgconfig
SDL_CFLAGS = -I/usr/include/arm-linux-gnueabihf $(shell PKG_CONFIG_LIBDIR=$(PKG_CONFIG_LIBDIR) pkg-config --cflags sdl2 SDL2_ttf libpng)
SDL_LIBS = $(shell PKG_CONFIG_LIBDIR=$(PKG_CONFIG_LIBDIR) pkg-config --libs sdl2 SDL2_ttf libpng)
LDFLAGS = -Wl,--allow-shlib-undefined
else
SDL_CFLAGS = $(shell sdl2-config --cflags) $(shell pkg-config --cflags SDL2_ttf)
SDL_LIBS = $(shell sdl2-config --libs) $(shell pkg-config --libs SDL2_ttf) -lpng
LDFLAGS =
endif
CXXFLAGS = -std=c++11 -Os -Ivendor/OptionsMenu/src $(SDL_CFLAGS)
LDLIBS = $(SDL_LIBS)
VENDOR_SRC_DIR = vendor/OptionsMenu/src
SOURCES = src/main.cpp $(VENDOR_SRC_DIR)/command.cpp $(VENDOR_SRC_DIR)/localization.cpp $(FRAMEWORK_DIR)/sdl_context.cpp $(FRAMEWORK_DIR)/texture.cpp $(FRAMEWORK_DIR)/controller.cpp $(FRAMEWORK_DIR)/powerwatch.cpp $(FRAMEWORK_DIR)/draw_helpers.cpp $(FRAMEWORK_DIR)/utf8.cpp $(FRAMEWORK_DIR)/font8x8_lookup.cpp $(FRAMEWORK_DIR)/uitheme.cpp
OBJECTS = $(SOURCES:.cpp=.o)

# Build theme_downloader with the Docker toolchain's static curl+OpenSSL.
# No native fallback is available because the libraries live at CURL_PREFIX.
CC = gcc
CURL_PREFIX = /opt/curl-static
CFLAGS = -O2 -Wall -I$(CURL_PREFIX)/include
CURL_LDLIBS = -L$(CURL_PREFIX)/lib -L/usr/lib/arm-linux-gnueabihf -lcurl -lssl -lcrypto -lz -ldl -lpthread

# `all`/`hmod` package mod/ without compiling for toolchain-free CI.
# Use `full` to build the binaries from source first, then package.
# Avoid "build": GNU Make's .sh rule conflicts with build.sh.
all: hmod

full: compile hmod

compile: mod/etc/options_menu/lib/theme_manager mod/bin/theme_downloader

mod/etc/options_menu/lib/theme_manager: $(OBJECTS)
	$(CROSS_PREFIX)$(CXX) $(OBJECTS) $(LDLIBS) $(LDFLAGS) -Wl,-rpath,/etc/options_menu/lib -o mod/etc/options_menu/lib/theme_manager
	$(CROSS_PREFIX)$(STRIP) mod/etc/options_menu/lib/theme_manager

%.o: %.cpp
	$(CROSS_PREFIX)$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.c
	$(CROSS_PREFIX)$(CC) $(CFLAGS) -c $< -o $@

# Fetch the gitignored CA bundle for fresh builds.
src/cacert.pem:
	curl -fL -o src/cacert.pem https://curl.se/ca/cacert.pem

# Embed cacert.pem from src/ to keep "src/" out of the symbol names.
src/ca_bundle.o: src/cacert.pem
	cd src && $(CROSS_PREFIX)ld -r -b binary -o ca_bundle.o cacert.pem

mod/bin/theme_downloader: src/theme_downloader.o src/ca_bundle.o
	$(CROSS_PREFIX)$(CC) src/theme_downloader.o src/ca_bundle.o $(CURL_LDLIBS) -o mod/bin/theme_downloader
	$(CROSS_PREFIX)$(STRIP) mod/bin/theme_downloader
	upx --best mod/bin/theme_downloader

hmod: clean

	mkdir -p out/ temp/
	rsync -a --exclude-from=exclude-file.txt mod/ temp/ --links --delete

	printf "%s\n" \
	"---" \
	"Name: $(MOD_NAME)" \
	"Creator: $(MOD_CREATOR)" \
	"Category: $(MOD_CATEGORY)" \
	"Version: $(MOD_VER)" \
	"Built on: $(shell date +"%A, %d %b %Y - %T")" \
	"Git commit: $(GIT_COMMIT)" \
	"---" > temp/readme.md
	
	sed 1d mod/readme.md >> temp/readme.md

	cd temp/; tar -czf $(OUT)/$(MOD_FILENAME)-$(MOD_VER).hmod *
	rm -r temp/

fix: hmod
	@ver="$(NEXT_PATCH_VERSION)" && \
	git tag "v$(MOD_VER)" && \
	echo "$$ver" > VERSION && \
	git add VERSION && \
	git commit -m "Bump version to v$$ver"

rc: hmod
	@ver="$(NEXT_RC_VERSION)" && \
	git tag "v$(MOD_VER)" && \
	echo "$$ver" > VERSION && \
	git add VERSION && \
	git commit -m "Bump version to v$$ver"

update: all
	@ver="$(NEXT_MINOR_VERSION)" && \
	git tag "v$(MOD_VER)" && \
	echo "$$ver" > VERSION && \
	git add VERSION && \
	git commit -m "Bump version to v$$ver"

upgrade: all
	@ver="$(NEXT_MAJOR_VERSION)" && \
	git tag "v$(MOD_VER)" && \
	echo "$$ver" > VERSION && \
	git add VERSION && \
	git commit -m "Bump version to v$$ver"

info:
	@echo "Mod Dir: $(MOD_FILENAME)"
	@echo "Current version: $(MOD_VER)"
	@echo "Last tag: $(LAST_TAG)"
	@echo "$(shell git rev-list $(LAST_TAG).. --count) commit(s) since last tag"
	@echo "Build: $(BUILD) (total number of commits)"
	@echo "next major version: $(NEXT_MAJOR_VERSION)"
	@echo "next minor version: $(NEXT_MINOR_VERSION)"
	@echo "next patch version: $(NEXT_PATCH_VERSION)"

update-headers:
	sh tools/update-license-headers.sh $(YEAR)

clean:
	rm -rf out/ temp/


.PHONY: all full compile hmod fix rc update upgrade info update-headers clean

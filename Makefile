MOD_ID       := om_theme-selector
MOD_NAME     := Options Menu - Theme Selector
MOD_CATEGORY := Options Menu - Addons
MOD_EXCLUDE  := exclude-file.txt
MOD_DEPS     := mod/etc/options_menu/lib/theme_manager mod/bin/theme_downloader mod/bin/sprite_crop

# Set CROSS_PREFIX=arm-linux-gnueabihf- to cross-compile for the console
# (build.sh does this inside the Docker toolchain); leave unset to build natively.
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
CXXFLAGS = -std=c++11 -Os -Ivendor/OptionsMenu/src $(SDL_CFLAGS) -DMOD_VERSION=\"v$(MOD_VER)\"
LDLIBS = $(SDL_LIBS)
VENDOR_SRC_DIR = vendor/OptionsMenu/src
SOURCES = src/main.cpp src/single_instance_lock.cpp src/command_loader.cpp src/texture_utils.cpp src/theme_manager_app.cpp src/folder_theme_model.cpp src/folder_themes_layout.cpp $(VENDOR_SRC_DIR)/command.cpp $(VENDOR_SRC_DIR)/localization.cpp $(FRAMEWORK_DIR)/sdl_context.cpp $(FRAMEWORK_DIR)/texture.cpp $(FRAMEWORK_DIR)/controller.cpp $(FRAMEWORK_DIR)/powerwatch.cpp $(FRAMEWORK_DIR)/draw_helpers.cpp $(FRAMEWORK_DIR)/utf8.cpp $(FRAMEWORK_DIR)/font8x8_lookup.cpp $(FRAMEWORK_DIR)/uitheme.cpp $(FRAMEWORK_DIR)/badge.cpp $(FRAMEWORK_DIR)/dialog.cpp
OBJECTS = $(SOURCES:.cpp=.o)

# Build theme_downloader with the toolchain's static curl+OpenSSL.
CC = gcc
CURL_PREFIX = /opt/curl-static
CFLAGS = -O2 -Wall -I$(CURL_PREFIX)/include
CURL_LDLIBS = -L$(CURL_PREFIX)/lib -L/usr/lib/arm-linux-gnueabihf -lcurl -lssl -lcrypto -lz -ldl -lpthread

all: hmod

compile: $(MOD_DEPS)

mod/etc/options_menu/lib/theme_manager: $(OBJECTS)
	mkdir -p $(@D)
	$(CROSS_PREFIX)$(CXX) $(OBJECTS) $(LDLIBS) $(LDFLAGS) -Wl,-rpath,/etc/options_menu/lib -o $@
	$(CROSS_PREFIX)$(STRIP) $@

DEPDIR = .deps

%.o: %.cpp
	@mkdir -p $(DEPDIR)
	$(CROSS_PREFIX)$(CXX) $(CXXFLAGS) -MMD -MP -MF $(DEPDIR)/$(subst /,_,$*).d -c $< -o $@

-include $(wildcard $(DEPDIR)/*.d)

%.o: %.c
	$(CROSS_PREFIX)$(CC) $(CFLAGS) -c $< -o $@

# Fetch the gitignored CA bundle for fresh builds.
src/cacert.pem:
	curl -fL -o src/cacert.pem https://curl.se/ca/cacert.pem

# Embed cacert.pem from src/ to keep "src/" out of the symbol names.
src/ca_bundle.o: src/cacert.pem
	cd src && $(CROSS_PREFIX)ld -r -b binary -o ca_bundle.o cacert.pem

mod/bin/theme_downloader: src/theme_downloader.o src/ca_bundle.o
	mkdir -p $(@D)
	$(CROSS_PREFIX)$(CC) src/theme_downloader.o src/ca_bundle.o $(CURL_LDLIBS) -o $@
	$(CROSS_PREFIX)$(STRIP) $@
	upx --best $@

# DIY sprite previews (om_vars). Built by pngpack's own Makefile; its default
# LDFLAGS (-dead_strip) is macOS-only.
mod/bin/sprite_crop: pngpack/pngpacker/sprite_crop.cpp pngpack/pngpacker/pixeltools.cpp
	mkdir -p $(@D)
	$(MAKE) -C pngpack build/sprite_crop CROSS=$(CROSS_PREFIX) LDFLAGS=-Wl,--gc-sections
	cp pngpack/build/sprite_crop $@

update-headers:
	sh tools/update-license-headers.sh $(YEAR)

clean:
	find . -name "*.o" -type f -not -path "./toolchain/*" -delete
	rm -rf $(DEPDIR)
	rm -f $(MOD_DEPS)
	rm -rf out/

include hmod-build/hmod.mk

.PHONY: all compile update-headers clean

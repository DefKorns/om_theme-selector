# Options Menu - Theme Selector

[![Build](https://github.com/DefKorns/om_theme-selector/actions/workflows/build.yml/badge.svg)](https://github.com/DefKorns/om_theme-selector/actions/workflows/build.yml)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)

**Requires [my Options Menu fork](https://github.com/DefKorns/OptionsMenu/releases) as a base — not compatible with any other UI.**

`Click the image below to view it on YouTube.`
[![Theme Selector - Demo](https://i.imgur.com/7JgP6JI.png)](https://youtu.be/3UilWr1NvFA "Theme Selector - Demo")

## What is it?

A graphical theme manager for the NES, SNES, Famicom and Super Famicom Classic consoles, built on top of [my fork](https://github.com/DefKorns/OptionsMenu) of [CompCom's OptionsMenu](https://github.com/CompCom/OptionsMenu). Pick a theme from a preview grid, download more from an online catalog, build your own from pieces of the themes you already have, and give each folder its own theme and music — all from the console itself, no PC needed after install.

The UI is a standalone C++/SDL app (`theme_manager`) built against the vendored Options Menu engine, not a set of scripts layered on the stock menu.

Full guides are on the **[wiki](https://github.com/DefKorns/om_theme-selector/wiki)**.

## Features

- **Installed themes grid** — browse your themes by their preview and apply one; the active theme is outlined
- **Download themes** from an online catalog of 90+ themes, each credited to its author (**Wi-Fi mod required**)
- **DIY theme** — mix the UI, background and demo sprites (or background color, on a NES) of the themes you already have, preview it live, then save it as a theme of your own
- **Theme and music per folder** — give any folder (and Home) its own theme and its own menu music from one screen; folders without one inherit their parent's
- **Theme randomizer** — a different theme every time you change folder (off by default)
- **Audio randomizer** on Home, optionally on every folder (off by default)
- Custom fonts per theme
- Delete a theme from the grid by holding **B**
- **Clean Up** removes the theme files your console type doesn't use
- **Reset Settings** puts the mod back to its defaults without uninstalling it or deleting your themes
- Custom color palette for the Theme Manager UI itself (`theme.cfg`)
- Every console and region: NES, Famicom, Famicom Shonen Jump, SNES (USA/EUR) and Super Famicom

## Requirements

- [Hakchi2 CE](https://github.com/TeamShinkansen/hakchi2/releases/latest)
- [My Options Menu fork](https://github.com/DefKorns/OptionsMenu/releases) — the base UI this mod plugs into
- [Hakchi Wi-Fi mod (WPA Supplicant)](https://hakchi.net/hakchi/hmods/wpa-supplicant.hmod) — only to download themes from the console
- A USB/SD drive is recommended: themes are often 5–30 MB each

## Install

**From my Mod Hub (recommended):** in hakchi open **Manage repositories**, add `https://defkorns.github.io/hakchi-repo/`, then install **Options Menu** and **Options Menu - Theme Selector** from it. Updates show up there automatically.

**By hand:** download `om_theme-selector.hmod` from [Releases](https://github.com/DefKorns/om_theme-selector/releases), put it in hakchi's `user_mods` folder (or drag and drop it onto the hakchi window), then install it from hakchi (**Modules → Install extra modules**).

Then open the Options Menu (hold **L+R** on a SNES/Super Famicom, **B+Down** on a NES/Famicom) and go to **Themes**. Step by step, with screenshots: [Installation](https://github.com/DefKorns/om_theme-selector/wiki/Installation) and [Usage](https://github.com/DefKorns/om_theme-selector/wiki/Usage).

## Getting themes

**From the console** (needs the Wi-Fi mod):

1. Go to **Themes → Download Themes** and select **Update Theme List**.
2. Pick a theme from the grid — each one shows its preview, size and author — and press **A**. Press **B** when the download log finishes.
3. Back in **Installed Themes**, select the new theme and press **A** to apply it. The console reboots with it.

**All SNES Themes** / **All NES Themes** downloads the whole catalog at once. It's several hundred MB, so it only shows up when a USB/SD drive is connected.

**By hand:** copy a theme folder, or a catalog package such as `SNES.<name>.tar.gz`, into the themes folder for your console (see [Where things live](#where-things-live)), then select **Refresh** in **Themes**. Packages are in the [om-theme-downloads release](https://github.com/DefKorns/om-theme-downloads/releases/tag/themes-v1); the prefix (`SNES.`, `NES.`, `SHONEN.`) must match your console.

More: [Usage → Downloading themes](https://github.com/DefKorns/om_theme-selector/wiki/Usage#downloading-themes) and [Adding themes by hand](https://github.com/DefKorns/om_theme-selector/wiki/Usage#adding-themes-by-hand).

## Where things live

| | USB/SD | NAND |
| --- | --- | --- |
| Themes | `/media/hakchi/themes/<console>` | `/var/lib/hakchi/rootfs/usr/share/themes/<console>` |
| Menu music | `/media/hakchi/music_menu` | `/var/lib/hakchi/rootfs/usr/share/music_menu` |

`<console>` is `nes`, `snes` or `shonen`. With a USB/SD drive connected, themes always go there.

## Customizing the look

The Theme Manager's own UI (not your game themes) reads its color palette from `/etc/options_menu/theme.cfg` at startup — edit the `Key=R,G,B` lines (background, borders, accent, badges, the active-theme border...) or delete a line to fall back to the default.

## Building from source

```sh
git clone --recursive https://github.com/DefKorns/om_theme-selector.git
cd om_theme-selector
./build.sh            # cross-compiles theme_manager, theme_downloader and sprite_crop, then packages out/om_theme-selector.hmod
./build.sh compile    # binaries only
```

`build.sh` runs `make` inside the [classicmini-cross-toolchain](https://github.com/DefKorns/classicmini-cross-toolchain) Docker image, so no local ARM toolchain is needed. Versioning and packaging come from [hmod-build](https://github.com/DefKorns/hmod-build): the version is the latest `v*` git tag, and pushing a tag builds and publishes the release.

The downloadable themes catalog lives in [om-theme-downloads](https://github.com/DefKorns/om-theme-downloads).

## Credits

- [DefKorns](https://github.com/DefKorns)
- [DanTheMan827](https://github.com/DanTheMan827) — packed.png extractor
- Theme authors — credited on each theme in the download catalog

## Thanks

- AluCarD
- [BsLeNuL](https://github.com/bslenul)
- [CompCom](https://github.com/CompCom) — original Options Menu
- [KMFDManic](https://github.com/KMFDManic)
- [ModMyClassic](https://modmyclassic.com/)
- NESminiling0618
- [Swingflip](https://github.com/swingflip)

## License

GPLv3 or later — see [LICENSE](LICENSE).

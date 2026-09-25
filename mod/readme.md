# Options Menu - Theme Selector

**Requires [my Options Menu fork](https://github.com/DefKorns/OptionsMenu/releases) as a base — not compatible with any other UI.**

[![Theme Selector](https://i.imgur.com/7JgP6JI.png)](https://i.imgur.com/7JgP6JI.png)

## What is it?

A graphical theme manager for NES/SNES/Famicom/Super Famicom Classic consoles, built on top of [my fork](https://github.com/DefKorns/OptionsMenu) of [CompCom's OptionsMenu](https://github.com/CompCom/OptionsMenu). Pick a theme from a preview grid, download more from a catalog, build your own from pieces of the themes you already have, and tune per-folder/randomizer/audio behavior — all from the console itself, no PC required after install.

The UI is my own standalone C++/SDL app (`theme_manager`), not a set of Lua scripts layered on the stock menu.

## Features

- Preview grid to browse and apply installed themes; the theme currently applied is outlined so you can spot it at a glance
- Download themes directly from the internet (**Wi-Fi mod required**)
- Build your own DIY theme from pieces of the themes you already have (background, sprites, colors, packed art, fonts...)
- Theme randomizer — a different theme each time you go Home (off by default)
- Audio randomizer on the Home folder, optionally extended to every folder (off by default)
- Theme per folder: give a folder its own theme by naming a theme after it; a folder with its own theme keeps its own icon instead of inheriting the parent theme's
- Custom fonts per theme
- Custom color palette for the Theme Manager UI itself (`theme.cfg`)
- System clean-up to remove files your console type doesn't need
- Reset just this mod's settings without a full uninstall

## Set a theme per folder, what is that?

If you have your games organized into system folders, you can give each one its own theme instead of one theme for the whole console. Turn on **Theme Per Folder** in Settings, then name a theme folder after the game folder it should apply to (lower snake_case). Eg: game folder `Nintendo - Nintendo Entertainment System` → theme folder `nintendo_-_nintendo_entertainment_system`.

By default, a subfolder inherits its parent's theme. Turn on **Allow Different Theme To Subfolder** if you want a subfolder to use its own matching theme instead — and if it does, that subfolder keeps its own real icon rather than being overwritten by the parent theme's.

## What if I want a specific theme on my main menu?

Name a theme `default` and it becomes your Home menu theme.

## What do you mean by "build your own DIY theme"?

Exactly that — pick and mix backgrounds, sprites, colors and packed art from the themes already on your console/USB into a theme of your own, previewed live as you build it.

## I own a Famicom/Shonen/Super Famicom, can I install this?

Yes — it supports all Nintendo Classic consoles, all regions.

## Requirements

- [Hakchi CE](https://github.com/TeamShinkansen/hakchi2/releases/latest)
- [My Options Menu fork](https://github.com/DefKorns/OptionsMenu/releases) — the base UI this mod plugs into
- [Hakchi Wi-Fi mod (WPA Supplicant)](https://hakchi.net/hakchi/hmods/wpa-supplicant.hmod) — only needed to download themes from the internet

## How do I use it

Works the same whether your games are on NAND or USB/SD:

- Install the hmod
- Open Theme Options from the Options Menu, download or build a theme, and apply it

On NAND, themes live at `/var/lib/hakchi/usr/share/themes/<consoletype>`.
On USB/SD, themes live at `/media/hakchi/themes/<consoletype>`.

*`consoletype` is `nes`, `snes` or `shonen`.*

## Customizing the look

The Theme Manager's own UI (not your game themes) reads its color palette from `/etc/options_menu/theme.cfg` at startup — edit `Key=R,G,B` lines there (background, borders, accent, badges, the active-theme border...) to restyle it, or delete a line to fall back to the default.

## Credits

- [DefKorns](https://github.com/DefKorns)
- [DanTheMan827](https://github.com/DanTheMan827) (packed.png extractor)

## Thanks

- AluCarD
- [BsLeNuL](https://github.com/bslenul)
- [CompCom](https://github.com/CompCom) — original Options Menu
- [KMFDManic](https://github.com/KMFDManic)
- [ModMyClassic](https://modmyclassic.com/)
- NESminiling0618
- [Swingflip](https://github.com/swingflip)

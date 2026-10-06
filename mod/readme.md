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
- Theme and music per folder: give any folder (and Home) its own theme and its own menu music from one screen; subfolders without one inherit their parent folder's
- Custom fonts per theme
- Custom color palette for the Theme Manager UI itself (`theme.cfg`)
- System clean-up to remove files your console type doesn't need
- Reset just this mod's settings without a full uninstall

## Set a theme or music per folder, what is that?

If you have your games organized into system folders, you can give each one its own theme instead of one theme for the whole console. Turn on **Theme Per Folder** in Settings, then open **Assign Theme to Folder** right below it. Your folders are listed as a tree: press Left/Right on a folder to cycle through the themes, or A to pick one from a list. The preview shows where each folder's theme comes from. The same theme can be assigned to as many folders as you like, no copies or renaming needed. **Automatic**, the first entry when cycling and in the list, clears a folder's assignment.

Naming a theme folder after the game folder (lower snake_case) still works for folders without an assignment. Eg: game folder `Nintendo - Nintendo Entertainment System` → theme folder `nintendo_-_nintendo_entertainment_system`.

A folder on Automatic uses its parent folder's theme, all the way up; a top-level folder on Automatic uses the theme you applied for the whole console. A folder's own theme always wins over its parent's.

**Music Per Folder** works the same way: turn it on and open **Assign Music to Folder** to give folders their own menu music from `music_menu` (on USB, else the console's own). It only shows up when `music_menu` has `.wav` files, and works with or without Theme Per Folder. Music assigned to a folder plays instead of the theme's own music; a folder on Automatic keeps today's behaviour (the theme's music, the Audio Randomizer, or the original music).

## What if I want a specific theme on my main menu?

With **Theme Per Folder** on, open **Assign Theme to Folder** and set a theme on **Home**, the first entry in the list. Naming a theme `default` also still works.

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

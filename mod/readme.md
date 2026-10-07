# Options Menu - Theme Selector

**Requires [my Options Menu fork](https://github.com/DefKorns/OptionsMenu/releases) as a base — not compatible with any other UI.**

[![Theme Selector](https://i.imgur.com/7JgP6JI.png)](https://youtu.be/3UilWr1NvFA)

## What is it?

A graphical theme manager for the NES, SNES, Famicom and Super Famicom Classic consoles. Pick a theme from a preview grid, download more from an online catalog, build your own from pieces of the themes you already have, and give each folder its own theme and music — all from the console itself.

Open the Options Menu (hold **L+R** on a SNES/Super Famicom, **B+Down** on a NES/Famicom) and go to **Themes**.

Full guides: [github.com/DefKorns/om_theme-selector/wiki](https://github.com/DefKorns/om_theme-selector/wiki)

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

## Getting themes

**From the console** (needs the Wi-Fi mod): go to **Themes → Download Themes**, select **Update Theme List**, then pick a theme and press **A**. Back in **Installed Themes**, select it and press **A** to apply it.

**By hand:** copy a theme folder, or a catalog package such as `SNES.<name>.tar.gz` from [om-theme-downloads](https://github.com/DefKorns/om-theme-downloads/releases/tag/themes-v1), into your themes folder (see below), then select **Refresh** in **Themes**.

## Theme and music per folder

Turn on **Theme Per Folder** in **Settings**, then open **Assign Theme to Folder** right below it. Your folders are listed as a tree: press Left/Right on a folder to cycle through the themes, or A to pick one from a list. **Automatic** clears a folder's assignment, so it uses its parent folder's theme (a top-level folder uses the console's theme). The same theme can go on as many folders as you like.

**Music Per Folder** works the same way with the `.wav` files in `music_menu`, with or without Theme Per Folder.

A theme named after a folder (lower snake_case, e.g. `nintendo_-_nintendo_entertainment_system`) or named `default` (for Home) still works too.

## Requirements

- [Hakchi2 CE](https://github.com/TeamShinkansen/hakchi2/releases/latest)
- [My Options Menu fork](https://github.com/DefKorns/OptionsMenu/releases)
- [Hakchi Wi-Fi mod (WPA Supplicant)](https://hakchi.net/hakchi/hmods/wpa-supplicant.hmod) — only to download themes from the console
- A USB/SD drive is recommended: themes are often 5–30 MB each

## Where things live

| | USB/SD | NAND |
| --- | --- | --- |
| Themes | `/media/hakchi/themes/<console>` | `/var/lib/hakchi/rootfs/usr/share/themes/<console>` |
| Menu music | `/media/hakchi/music_menu` | `/var/lib/hakchi/rootfs/usr/share/music_menu` |

`<console>` is `nes`, `snes` or `shonen`. With a USB/SD drive connected, themes always go there.

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

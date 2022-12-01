## New Features and Improvements
* Don't draw status bar or any other UI in savegame snapshots.
* Add "Walk Under Solid Hanging Bodies" feature from PrBoom+.
* Support face gib animations in the status bar widget as in the 3DO/Jaguar/PSX ports (graphics lumps need to occupy the `STFXDTH0` to `STFXDTH9` name space).
* Allow COMP cheat to change complevel in-game (report current complevel when entered, expecting two-digit new complevel argument).
* Allow the SSG in Doom 1 if the corresponding assets are available.
* Implement SDL-native sound looping for moving walls (removing clicking noise between sounds).
* Have "Clear Marks" key clear just the last mark in the Automap (by @MrAlaux).
* Limit the number of identical sounds playing at once, based on priority ordering (based on code from Odamex and DSDA-Doom).
* Make color of health and armor count gray in the status bar and HUD widgets when invulnerable (by @MrAlaux).
* Add VGA "porch" behaviour emulation from Chocolate Doom (enabled by `vga_porch_flash` key in the config file).
* Implement dark automap overlay (by @MrAlaux).
* Support dedicated music (lumps names `D_E4M1` to `D_E4M9`) for Episode 4 of Ultimate Doom. Also, support to select these tracks (and the music tracks for UMAPINFO maps) by the IDMUS cheat.
* Implement crosshair target lock-on (by @MrAlaux).

## Bug Fixes
* Take into account "smooth diminishing lighting" for "level brightness" feature.
* Set 'fastdemo_timer' to false before warping (by @rrPKrr).
* Fix starting "new game" or "load game" when fast-forwarding a demo.
* Do not switch timer if fastdemo is not enabled.
* Fix midi_player config setting description.
* Fix background drawing on screen size 3 in 21:9 widescreen mode.
* UMAPINFO: Don't show menu for only one episode.
* Fix "Show Demo Progress Bar" being disabled in the menu instead of "Default Compatibility" if the `-complevel` command-line parameter was used (by @MrAlaux).
* Do not allow autoloading in shareware gamemode.
* An IWAD called `doom1.wad` is now always checked for its content instead of blindly assuming the shareware IWAD.
* Fix NULL string comparison in DEH parser.

## Miscellaneous
* The Native MIDI implementation on Windows has been partly rewritten to use SysEx messages.

## New Features and Improvements

* **Mod Support:**
  - Add support for optional menu and intermission sounds.
  - Add support for ambient sound definitions by SNDINFO lumps.
  - Color translation tables from PWADs are extended but never overridden.
  - Leave some more space in the HUD menu for the layout description.
  - Add support for optional chaingun sound, via `DSCHGUN`.

* **Quality of Life:**
  - Player view no longer abruptly jolts when running across very shallow floor height changes
  - The Minimap now always follows the player
  - The Automap player arrow has been widened for better visibility
  - The Load/Save Game menus are now organized in tabs, instead of pages.
    The left-most tab of the Load Game menu is for Quick Saves, its first slot is reserved for Auto Save.
    Quick Saves are now immediate and always override the oldest available slot.

* **Rendering:**
  - Made the SDL renderer not be cleared every frame, for improved rendering performance in general.
  - Improved palettization of PNG graphics, and general color approximation in the engine.
  - Implemented transposed rendering for improved rendering performance in general.
  - Added "Cylindrical" sky projection as a third option besides "Vanilla" and "Linear" (from Nugget Doom).

## Bug Fixes

* Fixed setup desktop action exiting immediately when run from the AppImage
* Do not restart MUSINFO level music when restoring a savegame.
* Fixed autoloading of the DEHACKED lump that fixed colored blood for HACX
* The status bar border is now draw for all non-fullscreen HUDs
* [Linux] Fixed "Woof Setup" icon erroneously displaying on main "Woof" executable.
* Quicksave does not change the active Load Game / Save Game menu page anymore
* Fixed level title missing from the automap when the automap was not in overlay mode.
  * This affected only SBARDEF 1.0.0 HUDs, such as the ones included with D+D2 (id24res.wad/extras.wad)
* Fixed minimap overlapping chat in Legacy of Rust
* Fixed minimap key binding block cheat codes and chat messages
* Fixed demo desync by menu-pausing during the intermission screen
* Fixed DEHACKED-related crashes caused by calling player sprite actions as thinker functions (e.g. Blues Brothers 2023)

## Miscellaneous

* Rearranged default HUD layouts: "Nightdive" and "Crispy" layouts swapped places
* Renamed MacOS build from "`Woof-<version>-uni.zip`" to "`Woof-<version>-MacOS-universal.zip`"
* Savegame description and snapshot are now saved outside the compressed keyframe to speed up populating the Load/Save Game menu pages

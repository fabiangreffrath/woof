## New Features and Improvements

* **Mod Support:**
  - Add support for optional menu and intermission sounds.
  - Add support for ambient sound definitions by SNDINFO lumps.
  - Color translation tables from PWADs are extended but never overridden.
  - Leave some more space in the HUD menu for the layout description.

* **Quality of Life:**
  - Player view no longer abruptly jolts when running across very shallow floor height changes
  - The Minimap now always follows the player

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

## Miscellaneous

* Renamed MacOS build from "`Woof-<version>-uni.zip`" to "`Woof-<version>-MacOS-universal.zip`"

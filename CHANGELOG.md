**New Features and Improvements**
* Increase main thread priority when window is visible and focused.
* Bring back "Emulate Intercepts Overflow" option to the "Doom Compatibility" menu.
* Apply free look toggle to current input type only. Rename `input_mouselook` to `input_freelook` in config.
* Add `shorttics` config item.
* "Back" key/button also works in the main menu.
* Restore vanilla "Read This!" screens.
* Redo translucency options.
  - Rename "Enable Translucency" to "Sprite Translucency".
  - Instantly enable/disable translucency for things.
* Show preview of currently selected crosshair in the menu.
* "Run" key/button no longer affects gamepad turn speed.
* Change automap "Flashing Keyed Doors" to "Color Keyed Doors" with "Off", "On", and "Flashing" options.
* Remove conditional disabling of HUD menu items.
* Rename "Forward Sensitivity" to "Move Sensitivity".
* Use `M_OPTTTL` patch for "Options" menu title.
* In deathmatch show "Frags" widget instead of "Level Stats".
* Do not reset chosen player view across levels in multiplayer demo playback spy mode.

**Bug Fixes**
* Fix framerate limiter in capped mode.
* Remove SDL renderer option from the config due to poor frame times with Direct3D11. The renderer can still be changed with the `SDL_RENDER_DRIVER` environment variable (works with any SDL2 application).
* Fix HOM line appearing when using various non-standard FOV and resolution values.
* Fix mouse controls in menu on macOS.
* Restore original fix for "garbage lines at the top of weapon sprites". This will help for weapons "wobble" issue.
* Fix status bar height calculation endianess issue.
* Prevent on-death-action reloads from activating specials (thanks @JNechaevsky).
* Fix `HOM` cheat crash.
* Fix drawing "unlimited" thermo values (mouse sensitivity).
* Fix translucent columns drawing.
* Fix `ENDOOM` crash.
* Fix main menu item hilighting for e.g. `hr2final.wad`.
* Fix bottom-aligned widgets being drawn one pixel too high.
* Fix mis-colored blood voxels after invulnarability.

**Miscellaneous**
* Limit auto aspect ratio to 21:9.

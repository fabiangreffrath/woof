**New Features and Improvements**
* Increase main thread priority when window is visible and focused.
* Bring back "Emulate Intercepts Overflow" option to the "Doom Compatibility" menu.
* Apply free look toggle to current input type only. Rename `input_mouselook` to `input_freelook` in config.
* Add `shorttics` config item.
* "Back" key/button also works in main menu.
* Restore vanilla "Read This!" screens.
* Redo translucency options.
  - Rename "Enable Translucency" to "Sprite Translucency".
  - Instantly enable/disable translucency for things.
* Draw crosshair in the menu.
* Don't use two-stage gamepad turn speed.
* Change "Flashing Keyed Doors" to "Color Keyed Doors" with "On", "Flashing", and "Off" options.
* Remove conditional disabling of HUD menu items.
* Rename "Forward Sensitivity" to "Move Sensitivity".
* Use `M_OPTTTL` patch for "Options" menu title.

**Bug Fixes**
* Fix capped mode.
* Remove the SDL render driver option from the config, because the Direct3D11 backend causes poor frame times. It can still be changed with the `SDL_RENDER_DRIVER` environment variable (works with any SDL2 application).
* Fix the HOM column occurring at various non-standard FOV and resolution values.
* Fix mouse controls in menu on macOS.
* Restore original fix for "garbage lines at the top of weapon sprites". This will help for weapons "wobble" issue.
* Fix status bar height calculation endianess issue.
* Prevent on-death-action reloads from activating specials (thanks @JNechaevsky).
* Fix `HOM` cheat crash.
* Fix drawing "unlimited" thermo values (mouse sensivity).
* Fix translucent columns drawing.
* Fix `ENDOOM` crash.

**Miscellaneous**
* Limit auto aspect ratio to 21:9.

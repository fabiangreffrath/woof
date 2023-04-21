**New Features and Improvements**
* HUD updates.
  - Introduce new `WOOFHUD` lump with ability to tweak widgets positions. See `woofhud.lmp` as example and `woofhud.md` documentation in `docs/`.
  - Ability to choose standard Doom font for widgets.
  - Smooth Health/Armor count (@MrAlaux).
  - HUD font patches updates (@liPillON).
* Switch to OpenAl Soft for sound mixing.
  - Massive improvements to sound mixing quality. Fixes issues with sound "clicking" (first room of DBP25.wad), sound overload (Revenants scream) and others.
  - Use `libsndfile` for SFX and music files loading. Support for a lot of WAV formats, Ogg, FLAC, MP3, Opus and others.
  - Switch from `libmodplug` to `libxmp` for tracker music.
  - Support multi-channel samples by converting them to mono first.
  - Use a linear resampler and simple 2D panning to not differ much from vanilla sound.
* Support `BRGHTMPS` lump from Doom Retro.
  - Format extension with the ability to set `SPRITE`, `FLAT` and `STATE` brightmaps.
  - De hardcode in-engine brightmaps. See `brghtmps.lmp` files in `autoload/` directory.
* Generate color translation tables.
  - Improve readability and colors of custom fonts in menus and HUD.
  - Always draw demo status bar with the lightest and darkest color available.
* Textscreen updates (`ENDOOM` screen and `woof-setup`)
  - Resizable textscreen windows.
  - Increase the default window size.
  - Render textscreen content to an upscaled intermediate texture. Improve non-integer window size scaling.
* Add framerate limiting (@mikeday0).
* Add ability to turn on "clean screenshots" without any HUD elements.
* Rearrange the startup messages.
* Support monster infight field in Dehacked (taken from Chocolate Doom). Fixes monsters infight in 100krevs.wad.
* Add support for loading old Doom (< v1.2) IWADs. Not demo compatible.
* Complete donut overrun emulation (from PrBoom+/Chocolate Doom).
* Only delete the entire savegame name if not modified.
* Update strings edit in menu. Set cursor position at end of line, Backspace and Del work as expected.
* Play quit sound only if showing `ENDOOM` (@ceski-1).
* Disable "180 turn" in strict mode (new DSDA rule).
* Config updates. Do not store comments and deprecated entries, sort and group, clean up.
* Check if drag-n-dropped `.lmp` files could be demo lumps.
* Always interpolate idle weapon bob with uncapped FPS (@ceski-1).
* Add `M_VBOX` and `M_PALSEL` lumps from PrBoom+.
* Play a sound if the menu is activated with a different key than ESC.
* Support for `QUITMSG1..QUITMSG14` in Dehacked (quit messages in D2ISOv2.wad).

**Bug Fixes**
* Better automap controls, fix some rotate/follow/overlay inconsistencies.
* Fix misleading indentation in level title initialization.
* Windows Native MIDI fixes.
  - Fix songs with missing "hold pedal off" events (@ceski-1).
  - Update volume after "reset all controllers" event. In certain cases the channel can be audible even if the music volume slider is set to zero (@ceski-1).
* Fix stutter in custom weapon switch animations (thanks to @MrAlaux).
* Fix colorized player names in network chats.
* Clip interpolated weapon sprites (thanks to @mikeday0).
* Fix always gray percent / always red mismatch in status bar.
* Fix `-dogs` default value.
* Fix desync due to randomly mirrored corpses feature (fixes DBP31.wad).
* Add check for wrong indexes in `P_LoadSegs()` (fixes 1killtng.wad map13).
* ESC key resets a menu item with multiple options.
* Fix crash when trying to send chat macro with key ASCII code < '0' (thanks to @MrAlaux).
* Properly center colorized messages (thanks to @MrAlaux).
* Fix alt-tab with exclusive fullscreen on Windows.
* Fix `-dumplumps` command line parameter.
* Fix puff interpolation on the floor level (thanks to @JNechaevsky).
* Print error and skip PNG patch (fixes practicehub.wad).
* Avoid ZIP file directory name clashes.

**Miscellaneous**
* Remove the signal handler. No more "Signal 11" on Windows, just a standard crash.

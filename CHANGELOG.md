**New Features and Improvements**
* HUD updates.
  - Introduce new `WOOFHUD` lump with ability to tweak widgets positions. See the `woofhud.lmp` example in `docs/` and documentation in the [wiki](https://github.com/fabiangreffrath/woof/wiki/Custom-HUD).
  - The three-line level stats and player coords widgets have been consolidated into single lines, the FPS counter is now a separate widget.
  - Ability to choose standard Doom font for widgets.
  - Smooth Health/Armor count (@MrAlaux).
  - HUD font patches updates (@liPillON).
* Switch to OpenAl Soft for sound mixing.
  - Massive improvements to sound mixing quality. Fixes issues with sound "clicking" (first room of DBP25.wad), sound overload (Revenants scream) and others.
  - Use `libsndfile` for SFX and music files loading. Support for a lot of WAV formats, Ogg, FLAC, MP3, Opus and others.
  - Use `libxmp` for tracker music.
  - Support multi-channel samples by converting them to mono first.
  - Use a linear resampler and simple 2D panning to not differ too much from vanilla sound.
* New video options:
  - Add framerate limiting (@mikeday0).
  - Exclusive fullscreen mode. Activated only when normal fullscreen mode is enabled.
  - "Smooth pixel scaling" from Crispy Doom.
* Make mouse settings exactly the same as in Crispy Doom. Add mouse acceleration options to the General menu.
* Support `BRGHTMPS` lump from Doom Retro.
  - Format extension with the ability to set `SPRITE`, `FLAT` and `STATE` brightmaps.
  - De hardcode in-engine brightmaps. See `brghtmps.lmp` files in `autoload/` directory.
* Generate color translation tables.
  - Improve readability and colors of custom fonts in menus and HUD.
  - Always draw demo progress bar with the lightest and darkest color available.
* Textscreen updates (`ENDOOM` screen and `woof-setup`)
  - Resizable textscreen windows.
  - Increase the default window size.
  - Render textscreen content to an upscaled intermediate texture. Improve non-integer window size scaling.
* Add a menu for binding cheats to keys/buttons. Ability to bind "Fake Archvile Jump".
* New cheats:
  - `FREEZE` Stops all monsters, projectiles and item animations, but not the player animations (from ZDoom).
  - `IDDKT/IDDST/IDDIT` (kill, secret, item) finder cheats from DSDA-Doom.
  - `IDBEHOLDH` (health) and `IDBEHOLDM` (megaarmor) from PrBoom.
  - `SKILL` cheat to show (or change) game skill level from Crispy Doom.
  - List of all cheats available in the [wiki](https://github.com/fabiangreffrath/woof/wiki/Cheats).
* Add options to disable certain HUD messages (@MrAlaux).
* Introduce hide weapon cosmetic option (see Weapons menu).
* Implement support for new `author` field in `UMAPINFO`.
* Add key binding for "clean screenshots" without any HUD elements.
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
* The demo footer is now compatible with PrBoom+/DSDA-Doom demo autoplay.

**Bug Fixes**
* Better automap controls, fix some rotate/follow/overlay inconsistencies.
* Windows Native MIDI fixes.
  - Fix songs with missing "hold pedal off" events (@ceski-1).
  - Update volume after "reset all controllers" event. In certain cases the channel can be audible even if the music volume slider is set to zero (@ceski-1).
  - Detect SysEx "part level" messages. Fixes volume in Valiant.wad MAP30 (@ceski-1).
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
* Fix disappearing icon on fullscreen switch on Windows.
* Fix restart `MUSINFO` music loaded from save.

**Miscellaneous**
* Remove the signal handler. No more "Signal 11" on Windows, just a standard crash.

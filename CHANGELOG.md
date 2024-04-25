**New Features and Improvements**
* Implement support for native MIDI output on all major platforms (WINMM on Windows, ALSA on Linux, CoreMIDI on MacOS). Recommended to check out [Nuked-SC55](https://github.com/nukeykt/Nuked-SC55/), perfect Roland SC-55 emulation.
  - Many MIDI features previously only available on Windows now work on all platforms - `midi_complevel`, `midi_reset_type`, EMIDI and RPG Maker/FF loops.
  - Automatic reset delay for better compatibility with Roland and Yamaha hardware. See `midi_reset_delay` config option.
  - Smoother switching between MIDI songs.
* Context related sound pitch shifting.
* Independent keyboard/mouse controls in menus. Mouse movement does not affect keyboard cursor. Various minor improvements.
* Ability to set maximum resolution, see `max_video_width` and `max_video_height` in config.
* Add `change_display_resolution` config option, which will only work if exclusive fullscreen mode is enabled and the maximum resolution is set. For CRT monitors users.
* Better support for -1, -2, -3 command line parameters.
* Add `all-all`, `doom-all`, `doom1-all`, `doom2-all` autoload folders. Allow autoload in shareware mode.
* Unify Vanilla Doom and Boom HUD widgets, do not hard-code HUDs anymore (see `woofhud.lmp` in `all-all` autoload folder).
* Add a parser for `Obituary_Deh_Actor` DEHACKED strings (thanks to @SirBofu).
* Use UMAPINFO labels, instead of map marker, on save files.
* Move secret message higher, don't disable crosshair.
* Add `snd_limiter` config key to toggle sound limiter.

**Bug Fixes**
* Fix error out with some weapon sprites on some resolutions.
* Fix various HOM columns that appeared on some resolutions.
* Prevent light scale overflow. Fix dark strips of light on very-close walls.
* Fix skies being shifted down a few pixels.
* Fix mouse movement after running the game with the -warp parameter on Linux.
* Parse UMAPINFO lump in IWAD (fix chex3v.wad).
* Disable compatibility menu items according to command line parameters.
* Fix savegames restore pitch even with freelook disabled.
* Use safe circle to square calculation for gamepad. Fix some odd behavior with analog sticks.
* Fix gamepad initialization in multiplayer.
* Fix `align_bottom` HUD widgets erasing.
* Fix wrong keycard is sometimes displayed in the Boom HUD on Windows (thanks to @xemonix0)
* Fix some weapon-switching issues.
* Fix wrong DBIGFONT menu strings in some cases (fix chex3v.wad).
* Fix blinking of drawing Tower of Babel on intermission screen.
* Fix the exit text is lower than it should be normally.

**Miscellanions**
* Make the build reproducible.
* AppImage for Linux now supports music in MP3 format.
* Don't use any of the predefined resampler names - these will be changed in a future release of OpenAL Soft.

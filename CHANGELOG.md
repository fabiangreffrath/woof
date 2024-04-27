**New Features and Improvements**
* Implemented support for native MIDI output on all major platforms (WinMM on Windows, ALSA on Linux, Core MIDI on macOS):
  - As a result, Woof! can now output to any software or hardware device that uses a MIDI port and supports General MIDI. [Nuked-SC55](https://github.com/nukeykt/Nuked-SC55/) is recommended for accurate Roland SC-55 emulation.
  - Features previously exclusive to Windows are now available on all platforms. This includes `midi_complevel`, `midi_reset_type`, full EMIDI support, loop points (Final Fantasy, RPG Maker), and more.
  - Automatic reset delay for better compatibility with Roland and Yamaha hardware. See `midi_reset_delay` config option.
  - Capital tone fallback to fix songs with invalid instruments. Emulates the behavior of early SC-55 models. See `midi_ctf` config option.
  - Smoother switching between MIDI songs.
* Context-related sound pitch shifting.
* Independent keyboard/mouse controls in menus. Mouse movement does not affect keyboard cursor. Various minor improvements.
* Ability to set maximum resolution -- see `max_video_width` and `max_video_height` in config.
* Added `change_display_resolution` config option, which will only work if exclusive fullscreen mode is enabled and the maximum resolution is set. For CRT monitors users.
* Better support for `-1`, `-2` and `-3` command-line parameters.
* Added `all-all`, `doom-all`, `doom1-all` and `doom2-all` autoload folders.
* Allowed autoloading in shareware mode.
* Unified Vanilla Doom and Boom HUD widgets, which are now configurable -- see `woofhud.lmp` in the `all-all` autoload folder.
* Added a parser for `Obituary_Deh_Actor` DEHACKED strings (thanks to @SirBofu).
* Save files now use UMAPINFO labels instead of map markers.
* Moved secret message higher, and prevented it from disabling the crosshair.
* Added `snd_limiter` config key to toggle the sound limiter.

**Bug Fixes**
* Fixed error-out with some weapon sprites on some resolutions.
* Fixed various HOM columns that appeared on some resolutions.
* Prevented light scale overflow, fixing dark strips of light on very-close walls.
* Fixed skies being shifted down a few pixels.
* Fixed mouse movement after running the game with the `-warp` parameter on Linux.
* Fixed UMAPINFO lump not being parsed in IWADs (fixes _chex3v.wad_).
* Compatibility menu items are now disabled according to command-line parameters.
* Fixed savegames restoring pitch even with freelook disabled.
* Fixed gamepad analog movement when using `joy_scale_diagonal_movement`.
* Fixed gamepad initialization in multiplayer.
* Fixed erasing of `align_bottom` HUD widgets.
* Fixed wrong keycard sometimes being displayed in the Boom HUD on Windows (thanks to @xemonix0).
* Fixed some weapon-switching issues.
* Fixed wrong usage of `DBIGFONT` for menu strings in some cases (fixes _chex3v.wad_).
* Fixed blinking Tower of Babel on intermission screen.
* Fixed exit text being lower than it should be normally.

**Miscellaneous**
* Made the build reproducible.
* AppImage for Linux now supports music in MP3 format.
* Made `snd_resampler` use strings instead of numbers. Default is "Linear".

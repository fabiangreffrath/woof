**New Features and Improvements**
* Implement support for native MIDI output on all major platforms (WinMM on Windows, ALSA on Linux, Core MIDI on macOS).
  - As a result, Woof! can output to any software or hardware device that uses a MIDI port and supports General MIDI. [Nuked-SC55](https://github.com/nukeykt/Nuked-SC55/) is recommended for accurate Roland SC-55 emulation.
  - Features previously exclusive to Windows are now available on all platforms. This includes `midi_complevel`, `midi_reset_type`, full EMIDI support, loop points (Final Fantasy, RPG Maker), and more.
  - Automatic reset delay for better compatibility with Roland and Yamaha hardware. See `midi_reset_delay` config option.
  - Capital tone fallback to fix songs with invalid instruments. Emulates the behavior of early SC-55 models. See `midi_ctf` config option.
  - Smoother switching between MIDI songs.
* Context-related sound pitch shifting.
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
* Fix error-out with some weapon sprites on some resolutions.
* Fix various HOM columns that appeared on some resolutions.
* Prevent light scale overflow. Fix dark strips of light on very-close walls.
* Fix skies being shifted down a few pixels.
* Fix mouse movement after running the game with the -warp parameter on Linux.
* Parse UMAPINFO lump in IWAD (fix chex3v.wad).
* Disable compatibility menu items according to command-line parameters.
* Fix savegames restoring pitch even with freelook disabled.
* Fix gamepad analog movement when using `joy_scale_diagonal_movement`.
* Fix gamepad initialization in multiplayer.
* Fix erasing of `align_bottom` HUD widgets.
* Fix wrong keycard sometimes being displayed in the Boom HUD on Windows (thanks to @xemonix0)
* Fix some weapon-switching issues.
* Fix wrong DBIGFONT menu strings in some cases (fix chex3v.wad).
* Fix blinking of drawing Tower of Babel on intermission screen.
* Fix the exit text being lower than it should be normally.

**Miscellaneous**
* Make the build reproducible.
* AppImage for Linux now supports music in MP3 format.
* `snd_resampler` uses strings instead of numbers. Default is "Linear".

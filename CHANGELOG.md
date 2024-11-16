**New Features and Improvements**

* Mod Support:
  - ID24 features:
    - `SBARDEF`, with additional widescreen layout and widgets; consequently, removed Boom HUD.
    - `SKYDEFS`.
    - `TRAKINFO` (remixed soundtrack from "Doom + Doom II").
    - `UMAPINFO` intermission extensions.
    - `extras.wad`, which will be autoloaded if found.
  - Added support for PNG graphics, textures, flats, and sprites, which are automatically converted to palettized Doom graphics. 32-bit PNGs are converted to 8-bit with color degradation (not recommended).
  - Added resource directories and partial support for PK3 archives.
  - Added `VX_START`/`VX_END` namespace for voxels.

* Video:
  - Added 32:9 aspect ratio support for ultrawide monitors.
  - Draw sprites overlapping into visible sectors: enhancement to stop things from disappearing due to "anchor" sector not being in line of sight (thanks to @SirBofu).
  - Limited max distance for sprites to 8192 map units, as before (improves performance in `comatose.wad`).
  - Improved FPS limiter (thanks to @gendlin).

* Sound:
  - Added 4-band equalizer for sound effects.
  - Added output limiter and SFX channels options to menu.
  - Added OpenAL Doppler effect option to menu.
  - Added click/pop prevention for sound lumps that start or end with a non-zero amplitude.

* Music:
  - Added auto gain (loudness normalization) menu option for music, which applies to sampled music (MP3/OGG/XM/etc.), FluidSynth, and OPL3.
  - Added support for multiple OPL3 chips, and made various improvements (thanks to @jpernst).
  - Changed native MIDI music volume curve to linear.
  - Added FluidSynth and OPL3 menu items.
  - Updated default FluidSynth reverb and chorus parameters. These parameters are configurable in `woof.cfg`.
  - Improved EMIDI support to match behavior of Build engine games.
  - Added DMXOPL enhanced FM patch set.

* Input:
  - Added option for fake high-resolution turning when using short tics: see `fake_longtics` in `woof.cfg`.
  - Added quickstart cache option (from DSDA-Doom): see `quickstart_cache_tics` in `woof.cfg`.
  - Added "Last" weapon key binding.
  - Added "Cycle Through All Weapons" option.
  - Added support for SSG key binding when using vanilla compatibility level.
  - Improved performance when handling many input events at once.
  - Optimized input polling for older computers (thanks to @gendlin).

* [Gamepad](https://github.com/fabiangreffrath/woof/wiki/Gamepad):
  - Added custom weapon slots: the default configuration uses the D-pad and matches the Doom II Xbox 360 port.
  - Added rumble support.
  - Added gyro support.
  - Added flick stick support.
  - Added option to force platform-specific (i.e. Xbox, Playstation, or Switch) button names: see `joy_platform` in `woof.cfg`.
  - Added option to force keyboard or gamepad menu help text, useful for mixed input devices: see `menu_help` in `woof.cfg`.
  - Streamlined save game menu when using a gamepad.

* Quality of Life:
  - Added "Auto Save" option: the game is automatically saved at the beginning of a map, after completing the previous one.
  - Added compatibility database from DSDA-Doom (`COMPDB` lump), which automatically applies compatibility options.
  - Added option to correct the automap's aspect ratio (thanks to @thearst3rd).
  - Added aliases for popular command-line parameters (`-cl`, `-nomo`, `-uv`, `-nm`).
  - Added announcement of map titles (from DSDA-Doom).
  - Added Crispy Doom automap color scheme.
  - Added "count" option for secret-revealed message (thanks to @MrAlaux).
  - Added level stats widget format settings (thanks to @MrAlaux).
  - Fast exit when pressing Alt+F4 or the window close button.
  - Deathmatch HUD widgets improvements (score board and level time countdown).

* Tools:
  - Added advanced coordinates widget (from DSDA-Doom).
  - Added command history widget (from DSDA-Doom).

* Miscellaneous:
  - Replaced "Show ENDOOM Screen" menu item with "Exit Sequence" (Off, Sound Only, PWAD ENDOOM, Full).
  - Allowed drag-and-drop on non-Windows systems.
  - Added `SPEED` cheat to show a speedometer.
  - Rewrote help strings in `woof.cfg` (thanks to @MrAlaux).
  - Added interpolation of wipe melt animation (from Rum and Raisin Doom).
  - Fit finale text to widescreen instead of inserting line breaks.
  - Always use traditional menu ordering (removed Boom choice).
  - Moved all autoload and internal resources to `woof.pk3` archive.
  - Allowed setting `gamedescription` (and thus the window title) with DEHACKED.
  - Allowed up to 10 episodes in UMAPINFO.
  - Added more paths to find IWADs.
  - Removed credits screen, restoring the vanilla demo loop.

**Bug Fixes**

* Fixed player being added to monster friend list when damaged below 50% health, improving performance on maps with many monsters (from DSDA-Doom).
* Fixed `UMAPINFO` `bossaction` walk-over actions (from DSDA-Doom).
* Fixed MBF21 SSG autoswitch causing weapons to disappear (from DSDA-Doom).
* Fixed MBF21 ripper weapons causing additional damage when "Improved Hit Detection" is enabled.
* Fixed adding/removing `NOBLOCKMAP` or `NOSECTOR` with MBF21 DEHACKED (from DSDA-Doom).
* Fixed sound origin for huge levels (from PrBoom+).
* Fixed direct SSG switches in vanilla demos (fixes desync in `b109xm-00463.lmp`).
* Fixed highlighting of death-exit sectors on automap.
* Fixed pressing Alt+Tab and Windows key when using exclusive fullscreen.
* Fixed OpenGL exclusive fullscreen.
* Fixed free look range.
* Fixed free look behavior when loading a level or using toggles.
* Fixed various issues with the "On Death Action" feature.
* Fixed misaligned 200-pixels-tall sky textures.
* Fixed stretching of vertically scrolling skies.
* Fixed looping of MIDIs that don't set initial channel volumes.
* Fixed ENDOOM aspect ratio.
* Fixed game speed changes affecting MIDI playback.
* Fixed a crash in the MAP06->MAP07 transition in `PUSS33_DIEROWDY.wad`.
* Fixed potential desyncs by MBF's correction of the SSG flash.
* Fixed garbage columns when aspect ratio correction is disabled.
* Fixed automap thing interpolation with the `FREEZE` cheat enabled.
* Fixed resolution affecting minimum/maximum automap scale.
* Fixed crosshair target lock-on not working on voxel models.
* Fixed clean screenshots on demo and intermission screens.
* Fixed game speed affecting dynamic resolution.

**Acknowledgments**

* El Juancho for testing and providing feedback on quickstart cache.
* gendlin and Trov (bangstk) for testing and providing feedback on fake high-resolution turning.
* gandlin for FPS limiter improvements and various general optimizations.
* Jibb Smart for publishing reference implementations of gyro and flick stick features.
* JustinWayland, protocultor, sndein, and AL2009man for testing and providing feedback on gyro and flick stick features.
* Julia Nechaevskaya for help with 32:9 support.

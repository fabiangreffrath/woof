**New Features and Improvements**
* Now you can load zip archives with WAD, lmp and music files. Use the usual `-file` parameter or put them in the `autoload` folder or drag-and-drop them onto the `woof` executable.
* Support for high quality music packs (see https://sc55.duke4.net/ or http://sc-d70.retrohost.net/). Unpack them into the `autoload` folder or load as zip archives.
* Apply various interpolations for automap (@JNechaevsky).
* Pan/zoom automap faster by holding run button (@JNechaevsky).
* Choose use button action on death (default reborn, load last save or nothing).
* Optimization for drawing huge amount of drawsegs from PrBoom+. Improve FPS on planisf2.wad and Eviternity.wad MAP26 and others (@JNechaevsky).

**Bug Fixes**
* Update to SDL_Mixer 2.6.0, which fixes loop points in .mod, .ogg, .flac, .mp3 music files.
* UMAPINFO: fix `exitpic` and `enterpic` fields.
* Ask for confirmation on window close with Alt-F4 (@joanbm).
* Fix Boom weapon autoswitch (from DSDA-Doom).
* Set window focus on startup (fix wrong player's angle at the start if using `-warp`).
* UMAPINFO: fix desync in finale skipping (fixes DBP37_AUGZEN.wad AUGZEND2ALL-00027.lmp demo).
* Don't let failed loadgame attempts reset `gameepisode`/`gamemap`.
* Switch automap to FRACTOMAPBITS coordinate system from PrBoom+. Fixes automap glitches on planisf2.wad and others (@JNechaevsky).
* Center mouse if permanent mouselook is disabled.
* Disable interpolation for sectors without thinkers. Fixes flickering on PAR.wad E1M2.

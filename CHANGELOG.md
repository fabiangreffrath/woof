**New Features and Improvements**

* Dynamic resolution improvements:
  - Added 30-frame history and 15-frame cooldown period after resolution change.
  - Rename the "FPS Limiter" option to "Target FPS" to indicate that it's also a dynamic resolution target.
* Added saving button states in savegames (from Doom Retro).
* Added support for alternative music tracks for Final Doom introduced in DoomMetalVol5.wad (from Crispy Doom).
* Added vertical option for level stats and player coords widgets in SBARDEF, rearranged widgets in automap mode.

**Bug Fixes**

* Fixed demo footer and file name after restart.
* Fixed evil grin getting triggered at level start (and by ID(K)FA).
* Fixed UMAPINFO Doom 1 intermission text skipping at episode end.
* Fixed rendering of voxels on top/bottom of screen (thanks to @MrAlaux).
* Fixed invulnerability option for skies defined in SKYDEFS lump.
* Fixed undefined behavior in "Linear Sky" calculation on MacOS ARM CPUs.
* Fixed some local options overriding netgame settings.

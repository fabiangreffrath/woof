**New Features and Improvements**
* Continue demo recording: Start the game with `-playdemo demo1 -record demo2` or `-recordfromto demo1 demo2` (also works in multiplayer).
* Join demo key that allows to start recording a demo from anywhere while the demo is playing (also works in multiplayer).
* Looking up/down with gamepad (padlook key).
* Add a FluidSynth gain setting to config. It allows to manually adjust the volume for "quiet" soundfonts.
* Replace the "Enable translucency" option with "Enable predefined translucency". Turn off translucency for things that were defined in Boom/MBF, but don't change the translucency defined in PWAD/Dehacked.
* The cosmetic recoil option works with some MBF21 codepointers. All weapons in Vesper.wad now have recoil.
* Implement NOTARGET cheat.
* Always display level title in automap overlay mode.
* Print complevel in console when loading level.
* `-skipsec` takes negative numbers to skip from the end of the demo instead of from the start.
* Restart level/demo key also restarts demo playback.

**Bug Fixes**
* Fix of hanging decoration disappearing (FW.wad MAP02).
* Compatibility for non-Latin paths in environment variables on Windows (e.g. DOOMWADDIR).
* Fix misplaced level stats text on Boom HUD.
* "Pain/pickup/powerup flashes" option also affect invulnerability.
* Enable colored blood in strict mode, but don't change vanilla monsters. Fixes colored blood in Judgement.wad
* Don't allow strafe and turn at the same time with gamepad (prevent easy SR50).
* Fix suppress savegame in `G_ReadDemoTiccmd()`.
* Fix restart demo recording after death in single player.
* "Demo recording" message did not appear after restarting level.
* Fix a crash when loading a saved game with the automap open.
* Fix forced savegame load.
* Properly update status bar every tick (MAYhem19.wad, Avactor.wad).
* Allow adding `*.zip` files from DOOMWADDIR and other standard paths.
* Remove beta assets, add UMAPDEF for BETALEVL.WAD.
* Reset weapon bobbing interpolation when changing screen size/automap leaving.

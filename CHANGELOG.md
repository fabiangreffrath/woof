**New Features and Improvements**

* Support for rendering of translucent translated sprites (enables e.g. translucent colored blood)
* Optimized rendering of very small and/or distant sprites

**Bug Fixes**

* Fixed "Bobbing" Weapon Alignment misbehaving with DeHackEd-set weapon sprite offsets (fixes jittering weapons in D4V.wad)
* Disabled interpolation of weapon sprites when sprite and DeHackEd offsets change simultaneously (fixes jittering pistol in DoomZero.wad)
* Included player weapon-switching state in savegames (fixes some visual bugs with weapons when loading savegames)
* Ensured that savegame description strings are null-terminated

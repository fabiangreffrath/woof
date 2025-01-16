**New Features and Improvements**

* Partial implementation ID24 DEMOLOOP specification (thanks to @elf-alchemist).
* Added ID24 "Carousel icon" DEHACKED field. Support for new carousel icons in "Legacy of Rust".
* Added "Refraction" and "Shadow" modes for "Partial invisibility" option.
* Added "Crispy HUD" option. Switch HUD with "-/+" keys or with "Screen Size" slider in "Status Bar/HUD" menu.
* Bring back extras.wad autoload, support for chex3v.wad and chex3d2.wad.
* Expand exit sequence support to include PWAD endoom + no sound (thanks to @elf-alchemist).
* Improved the handling of 'intertext = clear' for the finale in UMAPINFO.
* Sort ZIP directory by filename.
* Prevents the chainsaw idle sound and the player pain sound from interrupting each other.
* Optimization of fixed division for older CPUs (thanks to @gendlin).
* Added Clang vector extension (thanks to @jopadan).
* Allow to toggle "Organize save files" at runtime.
* Minor menu cleanup ("Gray Percent Sign" and "Translucency Filter" now config only).
* Draw the time right-aligned if there is no par time and draw the total time right-aligned.
* Improve "IWAD not found" error message.
* Allow setting complevel in COMPDB lump. Added "Doomsday of UAC" E1M8 fix.

**Bug Fixes**

* Fixed "ouch face" again.
* Fixed 2px offset in various HUD elements.
* Fixed "-levelstat" command line option in release build.
* Fixed SKYDEF sky scroll speed.
* Fixed fast doors reopening with wrong sound.
* Fixed drawing of fuzz columns beyond screen boundaries on some resolutions.
* Fixed end buffer being visible in rightmost column in melt wipe on some resolutions.
* Fixed active slider when prematurely exiting out of the menu.
* Fixed loading "VILE\*" sprite lump files from PK3 files.
* Fixed "dropoff" handling regression (desync in evit32x333.lmp).

**Regressions**

* Remove UMAPDEF. Modern versions of masterlevels.wad and nerve.wad now include UMAPINFO.
* Don't search WAD files in HOME directory on Linux by default.

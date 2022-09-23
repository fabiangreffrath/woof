## New Features and Improvements
* Add new steam dirs for Unity IWAD and Final Doom.
* Add optional BLOCKMAP bug fix by Terry Hearst. Add a "compatibility breaking" menu category for this and "Pistol Starts".
* Implement player view/weapon bobbing (off, full, 75%).
* Complete "missed backside" emulation and disable it by default.
* Avoid demo lump name collisions (e.g. `MAP01.lmp`).
* Add rudimentary support for "REKKR: Sunken Land" (rename IWAD extension to `.wad`), fixes unwanted colored blood.
* Add reverb/chorus settings for Native MIDI.
* Let exit lines blink on the Automap as well if "blinking keyed doors" are enabled.
* Add smooth texture and flat scrolling.
* Synchronize animated flats.
* Remove the "STS" prefix from the Level Stats widget.

## Bug Fixes
* Skip response file in `M_CheckCommandLine()`, fixes network games initiated from the setup tool.
* Fixes for netgame demos and automap (fix playback skipping and progress bar for netgame demos, fix angles and interpolate other player arrows in automap, allow restart level/demo in deathmatch).
* Try loading a song again if SDL_Mixer couldn't detect it, fixes playback of some obscure MP3 files.
* Fix binding the mouse wheel to any action, excluding movement (forward/backward, turning and strafe).
* Fix Windows native MIDI level transitions (by @ceski-1).
* Remove SDL version check for Windows 11 "freezing" issue.
* Fix printing of timing demo results.
* Fix an issue with caching zero-sized lumps.
* Fix blood color setting (resp. unsetting) in DEH.
* Fix brightmap of the COMPUTE1 texture (by @JNechaevsky).

## Miscellaneous
* The code base has been made compatible with Clang 15 and now requires a C99 capable compiler.

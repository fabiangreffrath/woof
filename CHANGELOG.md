## New Features and Improvements
* Add new steam dirs for Unity IWAD and Final Doom.
* Add optional BLOCKMAP bug fix by Terry Hearst. Add a "compatibility breaking" menu category for this and "Pistol Starts".
* Implement player view/weapon bobbing (off, full, 75%).
* Complete "missed backside" emulation and disable it by default.
* Avoid demo lump name collisions (e.g. `MAP01.lmp`).

## Bug Fixes
* Skip response file in `M_CheckCommandLine()`, fixes network games from the setup tool.
* Fixes for netgame demos and automap (fix playback skipping and progress bar for netgame demos, fix angles and interpolate other player arrows in automap, allow restart level/demo in deathmatch).
* Try loading a song again if SDL_Mixer couldn't detect it, fixes playback of some obscure MP3 files.
* Fix binding the mouse wheel to any action, excluding movement (forward/backward, turning and strafe).
* Fix Windows native MIDI level transitions (by @ceski-1).
* Remove SDL version check for Windows 11 "freezing" issue.

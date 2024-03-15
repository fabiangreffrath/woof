**New Features and Improvements**
* Add "Crossfade" and "Fizzle" screen wipe options.
* Improve simultaneous mouse and gamepad input.
* Don't apply threshold to raw mouse input. Fix "sluggish" mouse movement at high frame rates.
* Display "off" when mouse acceleration is disabled.
* Add support for the `DBIGFONT' lump. Improve large font menus in some PWADs (Eviternity I-II, Dimension of the Boomed).
* Draw the IDRATE widget exclusively.
* Do not increase the default gain of the FluidSynth MIDI player to avoid distortion. This can be set using the `mus_gain` config parameter.
* Open menu with mouse click during demo playback.
* Disable "Classic BFG" in strict mode.
* Print warning instead of error out with empty PWAD (fix NULL.WAD in eternal.zip).

**Bug Fixes**
* Fix "Monsters can see through walls" bug with "Fast Line-of-Sight Calculation" option enabled. Taken from ZDoom.
* Revert changes to mouse look calculations (fix "garbage" pixels on some sprites).
* Print date and time in the Load/Save Game menus in the current locale.
* Fix initialisation with invalid `video_display` setting (thanks to @joanbm).
* Using "-strict" always disables related menu items.
* Fix "Invulnerability Effect" option affecting beta light amplifier.

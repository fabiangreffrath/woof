**New Features and Improvements**

* Mod Support:
  - Improvements to Chex support (thanks to @MelodicSpaceship):
    - Added proper obituaries for Chex 1 in its DEHACKED file.
    - Added a DEHACKED file for Chex 2 which sets correct level names and fitting obituaries for Chex 2's reskinned flemoids.
    - Enabled brightmaps to work in the Chex 3 Vanilla IWADs as well.
  - Enabled ANIMATE feature for default sky and SKYDEFS.
  - Added support for non-power-of-2 textures.
  - Added support for `scalex` SKYDEFS field.
  - Added BEX mnemonic for beta pickup sprite.
  - Added NyanDoom widescreen patch support.
  - Added partial support for GAMEVERS from NyanDoom (compatibility with RUST series).

* Sound and music:
  - Seamlessly pause and resume sounds. Side effect: No more chainsaw "rev up" sound when pausing game.
  - Improve doppler effect. Uses the correct velocity calculation. Adjust doppler effect scaling range.
  - Improve sound curve. Sound attenuation over distance is now more gradual when using the OpenAL 3D sound module.
  - Reduce PC speaker square wave amplitude. It was way too loud in comparison to the other sound modules.
  - Improve looping order for older MIDI hardware devices.
  - Disable SysEx for Fluidsynth (fixes DBP37_AUGZEN.wad MAP22).

* Input:
  - The maximum pitch angle is 45 when direct aim is enabled. Add the `max_pitch_angle` config variable.
  - Added gamepad device selection to menu.
  - Added reset camera options for gyro button.
  - Don't turn off gamepad LED when exiting game.
  - Added support binding the non-US backslash key.

* Miscellaneous:
  - The map reveal cheats (IDDT) can now be repeated by repeating the final character (from DSDA-Doom).
  - Remove "Animated Health/Armor Count" feature.
  - Bring back "Original" fuzz mode option.
  - Remove SSG in Doom 1 feature (compatibility with RUST series).
  - If level title announce string is too long, draw author on next line.
  - Prevent `P_LookForPlayers` from retrying sighting a player who failed (from Eternity Engine). Improve perfomance Necromantic Thirst MAP25.
  - Use $TMPDIR to find tempdir on Unix (thanks to @Usinganame).
  - Replace whole words in strings, use in player messages.
  - Update SUCKS time to match PrBoom's 100 hours.
  - Various menu tweaks.

**Bug Fixes**

* Fixed buffer overflow in d_deh.c.
* Fixed compatibility with "The Sky May Be" WAD.
* Fixed switching weapons while zooming automap.
* Fixed inconsistencies caused by load order:
  - Replace UMAPINFO with DEHACKED for E1M4b and E1M8b.
  - Always parse CHEX.DEH first.
* Fixed restore defaults for advanced gamepad menu.
* Fixed vanilla weapon switch logic, for example: DEHACKED chainsaw with ammo (thanks to @andrikpowell).
* Fixed ghost monsters being blood-colored.
* Fixed indicators flashing when invulnerability powerup fades out.
* Fixed DEMOLOOP missing support for DEHACKED Text substitution.
* Fixed exit from program if lump is not found in TRAKINFO.
* Fixed raw input for XInput devices under Windows.
* Fixed crash if animation speed is 0 in ANIMATED lump.
* Fixed tracker music looping.
* Fixed MUSINFO reset after level reload.
* Fixed desync in mh1910-430.lmp demo. Match PrBoom+ `A_FireOldBFG` autoaim behavior.

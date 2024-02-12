**New Features and Improvements**
* Implement support for arbitrary resolutions. Inspired by the solution in Eternity Engine.
  - Resolution scale slider. From 100% (original 320x200) up to native display resolution.
  - Dynamic resolution scaling. Automatically changes the internal resolution to maintain a target framerate (display refresh rate by default).
  - "Blocky Spectre Drawing" adapted to any resolution. Fixed some visual artifacts.
  - Vanilla wipe at any resolution (from Diet Boom).
* Add adjustable field of view (FOV). This uses a 4:3 horizontal FOV that is scaled internally. The default value of 90 is correct for all aspect ratios.
* Integration of the voxel code by Andrew Apted. Support for models in KVX format. Recommended mod: [Voxel Doom II v2.3](https://www.moddb.com/mods/voxel-doom-ii/addons/voxel-doom-ii-with-parallax-textures). Load the ZIP/PK3 file using the `-file` parameter or place it in the autoload folder.
* Mouse controls overhaul.
  - Fast mouse polling. Mouse input is sampled per frame instead of per tic. This lowers perceived input lag at high framerates, especially when using a high refresh rate monitor. Toggle with the `raw_input` config setting. Inspired by a similar concept in Odamex.
  - Turn and look sensitivity are now consistent with each other (1:1).
  - Smoother free look using a solution adapted from PrBoom+.
  - Improvements to direct vertical aiming.
* Gamepad controls overhaul.
  - Fast gamepad polling. See above mouse details.
  - Response curve support. Configurable from 1.0 (linear) to 3.0 (cubed).
  - Deadzone support. Stick magnitude is remapped properly for precise control. Two deadzone types are available: axial or radial (default).
  - Independent adjustment of forward, strafe, turn, and look sensitivity.
  - Optional extra sensitivity at outer threshold of analog stick, with a configurable ramp time.
  - Optional diagonal movement correction. Remaps circular analog input to a square to account for Doom's unique diagonal movement.
  - "Run" switches between two speeds for turning and looking, similar to existing forward and strafe behavior.
  - Axes configuration is simplified to 4 presets (default, swap, legacy, legacy swap).
* Add auto "strafe50" option.
* Menu overhaul. Retains the classic Doom menu, supports original graphics and 320x200 in the revamped Boom/MBF options menu.
  - Implement mouse controls for all menu elements.
  - Tabs for fast page switching.
  - Rearrange menu items for simplicity and faster navigation.
  - Don't limit menus to 35 fps.
  - Introduce Automap color presets - Vanilla, Boom (default) and ZDoom.
  - Don't disable menu items during demo playback.
* HUD widgets improvements.
  - Show smart kill count in stats widget (from DSDA-Doom).
  - Remove kills percent from level stats widget.
  - "Use" button timer.
  - Add key binding to toggle level stats and time.
* Integration of the NanoBSP node builder by Andrew Apted. Run using the `-bsp` parameter to fix various problems, e.g. [slime trails](https://doomwiki.org/wiki/Slime_trail) (not demo compatible).
* Implement per-PWAD savegame directories and "Organize Save Files" QOL option. Will be enabled by default if no saves are found in the source port's directory.
* Fade in sounds to prevent clicking. Applies to sounds that start at a non-zero initial amplitude (e.g. all original Doom sounds).
* Apply brightmaps to translucent and translated columns (thanks to @JNechaevsky).
* Adapt Eternity Engine's smooth Automap line drawing approach.
* Add "Blink Missing Keys" option.
* Improve logging to terminal.
* Add ability to set SDL render driver (`sdl_renderdriver` in config).
* Don't autoload from doom-all folder for FreeDoom and miniwad.


**Bug Fixes**
* Fix for Hexen flowing water from DSDA-Doom (fixes midtex bleeding in "Eviternity II RC5.wad" MAP18).
* Fix crash on Chex intermission.
* Report (and secure) `weaponinfo` overruns.
* Fix overflow hiding distant sprites (from DSDA-Doom).
* Fix level stats widget string overflow.
* `IDDQD`: fix reviving if god mode was already enabled (thanks to @tomas7770).
* UMAPINFO:
  - Fix music when using `-skisec`.
  - Behavior of `endpic` and `secretexit` are now compatible with PrBoom+/DSDA-Doom.
  - Fix off-center level names on intermission screen.
  - Update forgotten `nextsecret` map entry (fix secret exit in NERVE.WAD).
  - Fix episode selection for nightmare skill.
* Automap:
  - Do not highlight monster-only teleporter lines.
  - Fix initial min/max automap zoom factor.
  - Fix key-locked one-sided lines getting colored on automap.
* No horizontal padding for Vanilla HUD widgets.
* Fix `Max Health` setting in Dehacked using vanilla complevel.
* Optimize HUD widget erasing, avoid flicker when screen size is reduced.
* Fix MIDI looping with native macOS player.


**Miscellaneous**
* Remove non-functioning "interpolation fix" for multiplayer. The `-oldsync` parameter is recommended for uncapped mode at the expense of latency.
* Default player name is "Player". Change using `net_player_name` config setting.

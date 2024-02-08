**New Features and Improvements**
* Implement support for arbitrary resolutions. Inspired by the solution in Eternity Engine.
  - Resolution scale slider. From 100% (original 320x200) up to native display resolution.
  - Dynamic resolution scaling. Automatically changes the internal resolution to maintain a target framerate (display refresh rate by default).
  - "Blocky Fuzz" adapted to any resolution. Fixed some visual artefacts.
  - Vanilla wipe at any resolution (from Diet Boom).
* Add adjustable field of view (FOV). This uses a 4:3 horizontal FOV that is scaled internally. The default value of 90 is correct for all aspect ratios.
* Integration of Andrew Apted voxel code. Support for models in KVX format. Recommended mod: [Voxel Doom II v2.3](https://www.moddb.com/mods/voxel-doom-ii/addons/voxel-doom-ii-with-parallax-textures). Just run ZIP/PK3 file with `-file` parameter or place it in autoload folder.
* Mouse controls overhaul.
  - Fast mouse polling. Mouse input is sampled per frame instead of per tic. This lowers perceived input lag at high framerates, especially when using a high refresh rate monitor. Toggle with the `raw_input` config setting.
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
* Menu Overhaul. Retains the classic Doom menu, supports original graphics and 320x200 in the revamped Boom/MBF options menu.
  - Implement mouse controls for all menu elements.
  - Tabs for fast page switching.
  - Regroup and rearrange many items in the options menu and make it more compact and simple.
  - Don't limit menus to 35 fps.
  - Introduce Automap color presets - Vanilla, Boom (default) and ZDoom.
  - Don't disable menu items during demo playback.
* HUD widgets improvements.
  - Show smart kill count in stats widget (from DSDA-Doom).
  - Remove kills percent from level stats widget.
  - "Use" button timer.
  - Add key to show level stats and time.
* Integration of Andrew Apted NanoBSP node builder. Run it with `-bsp` parameter, it can fix various problems, e.g. [slime trails](https://doomwiki.org/wiki/Slime_trail) (not demo compatible).
* Implement per-PWAD savegame directories and "Organize Save Files" QOL option.
Will be enabled by default if no saves found in port directory.
* Fade in sounds to prevent clicking. Applies to sounds that start at a non-zero initial amplitude (e.g. all original Doom sounds).
* Apply brightmaps to translucent and translated columns (thanks to @JNechaevsky).
* Adapt Eternity Engine's smooth Automap line drawing approach.
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
  - Fix level names are off center on intermission screen.
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
* Remove "interpolation fix" for multiplayer, it didn't work properly. Recommend using the `-oldsync` parameter for uncapped mode (lag would be worse in oldsync mode).
* Default player name is "Player". Change using `net_player_name` config setting.

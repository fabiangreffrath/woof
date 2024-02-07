**New Features and Improvements**
* Implement support for arbitrary resolutions.
  - Resolution scaling slider. From 100% (original resolution 320x200) and up to native display resolution.
  - Dynamic resolution scaling option. Automatically changes the internal resolution to maintain target frame rate (display refresh rate by default).
  - "Blocky Fuzz" adapted to any resolution. Fixed some visual artefacts.
  - Vanilla wipe at any resolution (from Diet Boom).
* Add adjustable FOV. This uses a 4:3 horizontal FOV and automatically adjusts it to the current aspect ratio. That means most users should leave the FOV at 90 (default) unless they have a specific reason to change it.
* Integration of Andrew Apted voxel code. Support for models in KVX format. Recommended mod: [Voxel Doom II v2.3](https://www.moddb.com/mods/voxel-doom-ii/addons/voxel-doom-ii-with-parallax-textures). Just run ZIP/PK3 file with `-file` parameter or place it in autoload folder.
* Mouse controls overhaul.
  - Fast mouse polling. Instead of sampling the mouse input once per tic, it's now sampled once per frame. This lowers the input lag considerably at high frame rates, especially with a high refresh rate monitor.
  - Update carry/input calculations. Enables shorttics with fast mouse polling.
  - Use 1:1 mouse turning/looking sensitivity.
  - Adapt Free Look from PrBoom+ (improve accuracy for high resolutions).
  - Blend interpolated composite input (e.g. keyboard) turning with direct mouse turning to prevent choppy transitions.
  - Improved direct vertical aiming and monster height calculations.
* Gamepad controls overhaul.
  - Fast gamepad polling.
  - Response curve support. Configurable from 1.0 (linear) to 3.0 (cubed).
  - Deadzone support. Now stick magnitude is remapped properly from 0 to 1, which feels more precise. Two deadzone types are available: axial or radial (default).
  - Added independent control of forward, strafe, turn, and look sensitivity.
  - Optional extra sensitivity at outer threshold of analog stick, with a configurable ramp time.
  - Optional diagonal movement correction. Remaps circular analog input to a square to account for Doom's unique diagonal movement.
  - The "run" key switches between two speeds for turning and looking, similar to existing forward and strafe behavior.
  - Side speed restrictions added in strict mode and net games for "strafe + turn axis" (mouse/gamepad) and "strafe axis" (gamepad only).
  - Axes configuration has been simplified to 4 standard presets (default, flipped, legacy, legacy flipped).
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
  - Add key to show level stats and time.
* Integration of Andrew Apted NanoBSP node builder. Run it with `-bsp` parameter, it can fix various problems, e.g. [slime trail](https://doomwiki.org/wiki/Slime_trail) (not demo compatible).
* Implement per-PWAD savegame directories and "Organize Save Files" QOL option.
Will be enabled by default if no saves found in port directory.
* Fade in sounds to prevent clicking. All of the original Doom sounds start at a non-zero amplitude. When these sounds are upsampled on a modern computer, the non-zero starting amplitude creates clicking.
* Apply brightmaps to translucent and translated columns (thanks to @JNechaevsky).
* Adapt Eternity Engine's smooth Automap line drawing approach.
* Improve logging to console.
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
  - Fix key-locked one-sided lines getting coloured on automap.
* No horizontal padding for Vanilla HUD widgets.
* Fix `Max Health` setting in Dehacked using vanilla complevel.
* Optimize HUD widget erasing, avoid flicker when screen size is reduced.
* Fix MIDI looping with native macOS player.


**Miscellaneous**
* Remove "interpolation fix" for multiplayer, it didn't work properly. Recommend using the `-oldsync` parameter for uncapped mode (lag would be worse in oldsync mode).
* "Player" as the default player name, could be edited in config.

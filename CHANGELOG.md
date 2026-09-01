**New Features and Improvements**

* Mod Support:
  - Added support for the base UDMF spec, and the new exclusive `"woof"` UDMF namespace.
  - Added support for XGL2, XGL3, ZGL2, and ZGL3 node formats.
  - Added support for UZDoom-style obituary assignments in `DEHACKED`,
    via `Obituary = "x"`, `Melee Obituary = "x"`, and `Self Obituary = "x"`.
  - Added support for an exclusive `Woof Bits = x` thing property in `DEHACKED`.
    Monsters in Legacy of Rust now respect corpse flipping.
  - Added support for ID24 0.99.2 line specials (sector tinting, pistol-start exits, music changers, scrollers, offsets).
  - Added support for ID24 flat rotation.
  - Added initial support for the `TX_START`/`TX_END` namespace.
  - Added support for visual-only Boom scrollers in vanilla complevels.
  - Added support for a dedicated `rekkr-all/` autoload directory for Rekkr (by @MelodicSpaceship).
  - Added support for ID24 per-state `TRANMAP`s in `DEHACKED`, via the `Tranmap = "x"` thing property.
  - Added support for custom screen wipes defined in `DEMOLOOP`.
  - Added support for the new XBM1 blockmap lump format.
  - Added support for the ID24 `endfinale` lump, used for custom cast sequences in Legacy of Rust and Dominus Diabolicus.
  - Added support for `SKYDEFS` flatmapping.
  - Updated `SBARDEF` support up to [version 1.2.0](https://github.com/doom-cross-port-collab/id24/blob/e96a9e1c9ee34621b03a4894f4053c2a3426496e/community_version/SBARDEF-v1.2.0.md)
  - Added initial support for [DECLARATE](https://github.com/doom-cross-port-collab/declarate/blob/f72fee1b0e38a13b0cdb2eb2fb596f3d6ef4fdfa/declarate.md).
    See examples in `woof.pk3/all-all/actors/`.
    - Added support for (ambient) sounds through DECLARATE, using `SNDINFO`-like syntax.
  - Improved Freedoom support:
    - Added support for `freedoom-all`, `freedoom1-all`, and `freedoom2-all` autoload directories.
    - Setup program now uses Freedoom skill descriptions when applicable.
  - Added support for vanilla game version `final2` (Final Doom with teleporter bug fixed).
  - Added support for `HELP2` graphic in Ultimate Doom PWADs.

* Quality of Life:
  - Added rewinding, similar to DSDA-Doom.
  - Added a custom skill menu.
  - Combined "Free Look" and "Direct Vertical Aiming" menu items (settings are still separate).
  - Disabled free look in strict mode.
  - Added a "Previous Map" button.
  - Replaced "[HUD] Layout" setting with "HUD Anchoring" setting.
  - Added "Reduced" option for palette-changes setting.
  - Made savegame slot used for quick saves be highlighted in save/load menus.
  - Reworked handling of strict mode, now more akin to DSDA-Doom:
    - Now only applicable to demo recording, and enabled by default in it; can be disabled via the `-tas` parameter.
  - Reworked "Exit Sequence" and "PWAD ENDOOM Only" settings into the following settings:
    - "Show Quit Prompt".
    - "Play Quit Sound".
    - "Show ENDOOM Screen".
  - Added "Pause Demos in Menu" setting.
  - Added a net-game stats overlay button (<kbd>F1</kbd> by default).
  - Added a dedicated binding for message review (`input_msgreview` config key).

* Audio:
  - Improved sound limiter: limits the volume when too many channels are playing the same sound,
    and omits low-priority sounds.
  - Made pitch shifting match Chocolate Doom. Removed `pitch_bend_range` config key.
  - Sounds from linedefs now use the midpoint of the linedef as the origin,
    instead of the midpoint of the line's front sector, matching DSDA's behavior.
  - The game is now muted when the window loses focus. This can be toggled via the `mute_unfocused` config key.
  - Replaced Nuked-OPL3 with Nuked-OPL3-fast, a byte-identical faster implementation of the original.
  - Updated FluidSynth:
    - Removed `TimGM6mb.sf2` from Windows builds; FluidSynth now uses `gm.dls` instead.
    - Added support for `.dls` files in the `soundfonts/` directory.
    - Added FluidSynth "note cut" setting.
  - The `soundfont_dir` config key was removed. Formerly, this key contained a hard-coded, architecture-dependent list
    of search directories for soundfonts. Now, a list of common search directories is created dynamically at run-time.
    In order to override this list, point the new `soundfont_dirs` (mind the plural) config key
    to a custom soundfont directory (or a list thereof).

* Rendering:
  - Improved rounding of rendering resolutions:
    the game can now render at the display's native width exactly in most cases.
  - Reworked internal handling of transparency tables:
    - Generated `TRANMAP` lumps are now cached locally on-disk.
    - Generator now uses linear sRGB blending, as opposed to gamma sRGB, for more-accurate color mixing.
    - Transparent fullbright sprites now use additive transparency.
  - Added support for rendering of translucent translated sprites (enables e.g. translucent colored blood).
  - Interpolated rotation of transferred skies.
  - Optimized rendering of planes.
  - Optimized rendering of very small and/or distant sprites.
  - Improved changes of voxel-rendering distance upon dynamic resolution changes.
  - Missing flats and invalid PNG flats are now rendered as magenta-and-black checkerboards.

* Automap:
  - Added minimap mode.
  - Added "[Automap] Line Thickness" setting.
  - Made automap colors be converted to the current palette.
  - Made `IDDKT` cheat skip monsters spawned by Icon of Sin.
  - Replaced automap player arrow with the new one from KEX, and made it customizable through an `AMAPDEF` lump.

* Miscellaneous:
  - New savegames now use a compressed JSON key-frame format. Old savegames can still be loaded.
  - Restored silent BFG trick for complevels below MBF.
  - Changed DeHackEd loading order to match DSDA-Doom:
    patches loaded through `-deh` are now processed after everything else.
  - Optimized search of teleport destinations when using teleport specials.
  - Removed side-loading of `extras.wad`.
  - Implemented Discord Rich Presence.
  - Replaced built-in `PLS2` sprites with red replacements, to better match the original press-beta sprites.
  - Added freely licensed sprites for the following:
    - `SMFLAM*` (carousel icon for Legacy of Rust's incinerator).
    - `SMHEAT*` (carousel icon for Legacy of Rust's calamity blade).
    - `BON3A0` (beta evil-sceptre pickup).
    - `BON4A0` (beta unholy-bible pickup).
  - Improved text-screen rendering (for ENDOOM screen and setup executable).
  - Replaced ENDOOM font with improved "Hauge" font by Zokum.
  - Improved detection of installed (Steam, GOG) IWADs.
  - Slightly improved palettization of PNG graphics.
  - Improved HUD stats widget on some non-Doom games (e.g. on Chex it now reads 'F/I/S' instead of 'K/I/S').
  - Turned `no button slots left!` error into a warning (fixes `SPECHITS` cheat on 'Box Doom' MAP02).
  - Program window position isn't saved anymore; it now appears centered upon every launch.
  - Removed "Smooth Diminishing Lighting" feature.

* Build:
  - Replaced SDL2 with SDL3 for modernized input and display support:
    - Removed "Exclusive Fullscreen" setting.
  - Replaced SDL_net with netlib.
  - Added minimp3 as a fallback for MP3 files not supported by libsndfile.
  - Added universal macOS builds to continuous integration.
  - Removed `woof.com` executable; console output is now shown by `woof.exe` directly, but only with debug builds.
  - Added update information to AppImages (by @electricbrass).
  - Removed extraneous example WADs and documentation files from final builds.

* Refactor:
  - Moved from Boom-based DeHackEd parser to Chocolate Doom's DeHackEd parser.
    - Added extra warnings when in debug mode on the terminal.
    - Removed `-dehout`, `-bexout`, and `-bex` CLI options.
  - Added DSDA's `ssline` optimization for `P_CrossSubsector()`.

**Bug Fixes**

* Fixed many SKYDEFS incongruencies to better match the ID24 spec.
* Fixed MBF high-precision scroller math to match DSDA demo behavior.
* Fixed interpolation of Boom scrolling floors/ceilings and textures.
* Fixed how friendly monsters count towards the kill count, to match DSDA demo behavior
  (fixes kill counts in 'Fast Food 2' and 'One Of Everything').
* Fixed MBF Helper Dog spawning behavior to match DSDA-Doom.
* Fixed crash when loading invalid `REJECT` tables (fixes 'PUSS IV' MAP01).
* Fixed obtuse crash when encountering unknown thing types in UMAPINFO `bossaction` definitions
  (now crashes with a readable error message).
* Fixed weapon carousel appearing and staying on screen when trying to switch weapons as a zombie player.
* Fixed MBF21 homing player projectiles misbehaving when using Direct Vertical Aiming.
* Fixed voodoo dolls becoming invisible when voxels were enabled.
* Fixed rotation of entities mirrored by the "Randomly Mirrored Corpses" feature.
* Fixed "Bobbing" Weapon Alignment misbehaving with DeHackEd-set weapon sprite offsets
  (fixes jittering weapons in `D4V.wad`).
* Disabled interpolation of weapon sprites when sprite and DeHackEd offsets change simultaneously
  (fixes jittering pistol in `DoomZero.wad`).
* Fixed height calculation of level-title graphics on intermission (fixes intermission on 'Hell Revealations').
* Fixed SBARDEF game-mode condition checks.
* Fixed crosshair appearing on clean screenshots.
* Fixed direct switch to SSG in demos being mistaken as a demo-join command.
* Fixed obscure crash related to voxel rendering on certain systems.
* Fixed `A_MonsterMeleeAttack` triggering normal obituaries instead of melee obituaries.
* Improved calculation of HUD font size (fixes text widgets in 'RUST').
* Fixed incorrect culling of off-screen text in finale text screens (fixes such screens in 'RUST').
* Fixed state of HUD messages being tied to each HUD layout.
* Fixed synchronization of weapon carousel and weapon switching when switching with the previous/next-weapon buttons.
* Fixed multiple cheats with identical cheat codes being applied simultaneously.
* Made chat messages from other players be displayed even if messages are disabled, to match vanilla Doom behavior.
* Allowed player starts with an angle of 360 degrees, to match vanilla Doom behavior.
* Allowed `TRAKINFO` lump definitions to stack, overwriting previous definitions.
* Fixed quick-save slot being cleared when changing to a different save/load page.
* Made long quit messages be broken into multiple lines.
* Made some door code consistent with DSDA-Doom (`atce2x722.lmp` now plays back identically in both ports).
* Included player weapon-switching state in savegames (fixes some visual bugs with weapons when loading savegames).
* Fixed demo progress bar remaining on screen when using "End Game" to stop demos.
* Made transparent pixels on fullscreen graphics be drawn as black (fixes HOM in `ETERNALL.WAD`'s credits screen).
* Fixed loading disk icon changing size upon dynamic resolution changes.
* Fixed `.pk3` files not being loaded through drag-and-drop.
* Fixed `-uncapped` and `-nouncapped` parameters not being respected.
* Fixed some instances of music incorrectly being resumed when it should be paused.
* Fixed potential vulnerability in "scanner" parsing code (UMAPINFO, MUSINFO, etc.).
* Fixed potential crash in PNG conversion code.
* Fixed potential crash in net code.

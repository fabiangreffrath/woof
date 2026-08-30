**New Features and Improvements**

* Mod Support:
  - Added support for the base UDMF spec, and the new exclusive `"woof"` UDMF namespace.
  - Added support for XGL2, XGL3, ZGL2 and ZGL3 node formats.
  - Added support for UZDoom-style obituary assignments in `DEHACKED`, with `Obituary = "x"`, `Melee Obituary = "x"` and `Self Obituary = "x"`.
  - Added support for an exclusive `Woof Bits = x` thing property in `DEHACKED`. Monsters in Legacy of Rust now respect corpse flipping.
  - Added support for ID24 0.99.2 line specials (sector tinting, pistol-start exits, music changers, scrollers, offsets).
  - Added support for ID24 flat rotation.
  - Added initial support for the `TX_START`/`TX_END` namespace.
  - Added support for visual-only Boom scrollers in vanilla complevels.
  - Added support for a dedicated Rekkr autoload directory (via `rekkr-all/`) (thanks @MelodicSpaceship).
  - Added support for ID24 per-state TRANMAPs in `DEHACKED`, via the `Tranmap = "x"` thing property.
  - Added support for custom screen wipes defined in `DEMOLOOP`.
  - Added support for the new XBM1 blockmap lump format.
  - Added support for the ID24 `endfinale` lump, used for custom cast sequences in Legacy of Rust and Dominus Diabolicus.
  - Added support for SKYDEFS flatmapping.
  - Updated SBARDEF up to [version 1.2.0](https://github.com/doom-cross-port-collab/id24/blob/e96a9e1c9ee34621b03a4894f4053c2a3426496e/community_version/SBARDEF-v1.2.0.md)
  - Improved Freedoom support:
    - Added support for `freedoom-all`, `freedoom1-all` and `freedoom2-all` autoload directories.
    - Setup program now uses Freedoom skill descriptions when applicable.
  - Added support for `HELP2` graphic in Ultimate Doom PWADs.

* Quality of Life:
  - Added rewinding, similar to DSDA-Doom.
  - Added a custom skill menu.
  - Combined "Free Look" and "Direct Vertical Aiming" menu items (settings are still separate).
  - Disabled free look in strict mode.
  - Added a "Previous Map" button.
  - Reworked handling of strict mode, now more akin to DSDA-Doom:
    - Now only applicable to demo recording, and enabled by default in it; can be disabled via the `-tas` parameter.
  - Reworked "Exit Sequence" and "PWAD ENDOOM Only" settings into the following settings:
    - "Show Quit Prompt"
    - "Play Quit Sound"
    - "Show ENDOOM Screen"
  - Added "Pause Demos in Menu" setting.
  - Added a net-game stats overlay button (<kbd>F1</kbd> by default).
  - Added `input_msgreview` binding for message review.

* Audio:
  - Improved sound limiter: limits the volume when too many channels are playing the same sound, and omits low-priority sounds.
  - Sounds from linedefs now use the midpoint of the linedef as the origin, instead of the midpoint of the line's front sector, matching DSDA's behavior.
  - Replaced Nuked-OPL3 with Nuked-OPL3-fast, a byte-identical and faster implementation of the original.

* Rendering:
  - Reworked internal handling of transparency tables:
    - TRANMAPs are now cached locally on-disk.
    - Generator now uses linear sRGB blending, as opposed to gamma sRGB, for more accurate color mixing.
    - The "transparent ghost monsters" feature now uses additive transparency.
  - Added built-in magenta-and-black checkerboard fallback flat for missing flats.
  - Added support for rendering of translucent translated sprites (enables e.g. translucent colored blood).
  - Optimized rendering of plane rendering.
  - Optimized rendering of very small and/or distant sprites.
  - Interpolated rotation of transferred skies.
  - Made automap colors be converted to the current palette.

* Miscellaneous:
  - Improved detection of installed (Steam, GOG) IWADs.
  - Turned `no button slots left!` error into a warning (fixes `SPECHITS` cheat on Box Doom MAP02).

* Build:
  - Replaced SDL2 with SDL3:
    - Removed "Exclusive Fullscreen" setting.
  - Replaced SDL_net with netlib.
  - Replaced libsndfile with minimp3 for MP3 playback.
  - Added universal macOS builds to continuous integration.
  - Added `WITH_FLUIDSYNTH`, `WITH_XMP`, `ENABLE_LTO`, and various debugging compile-time flags.
  - Removed `woof.com` executable; console output is now shown by `woof.exe` directly, but only with debug builds.
  - Removed extraneous example WADs and documentation files from final builds.

* Refactor:
  - Moved from Boom-based Dehacked parser to Chocolate Doom's Dehacked parser.
    - Added extra warnings when in debug mode on the terminal.
    - Removed `-dehout`, `-bexout` and `-bex` CLI options.
  - Added DSDA's `ssline` optimization for `P_CrossSubsector`.

**Bug Fixes**

* Fixed many SKYDEFS incongruencies to better match ID24 spec.
* Fixed MBF high-precision scroller math to match DSDA demo behavior.
* Fixed interpolation of Boom scrolling floors/ceilings and textures.
* Fixed how friendly monsters count towards the kill count to match DSDA demo behavior (fixes kill counts in 'Fast Food 2' and 'One Of Everything').
* Fixed MBF Helper Dog spawning behavior to match DSDA-Doom.
* Fixed obtuse crash when encountering unknown thing types in UMAPINFO `bossaction` definitions (now crashes with a readable error message).
* Fixed weapon carousel appearing and staying on screen when trying to switch weapons as a zombie player.
* Fixed MBF21 homing player projectiles misbehaving when using Direct Vertical Aiming.
* Fixed voodoo dolls becoming invisible when voxels were enabled.
* Fixed rotation of entities mirrored by the Randomly Mirrored Corpses feature.
* Fixed "Bobbing" Weapon Alignment misbehaving with DeHackEd-set weapon sprite offsets (fixes jittering weapons in D4V.wad).
* Disabled interpolation of weapon sprites when sprite and DeHackEd offsets change simultaneously (fixes jittering pistol in DoomZero.wad).
* Fixed SBARDEF game-mode condition checks.
* Fixed buffer overflow when parsing MBF21 thing flags.
* Fixed obscure crash related to voxel rendering on certain systems.
* Made long quit messages be broken into new lines.
* Made some door code consistent with DSDA-Doom (`atce2x722.lmp` now plays back identically in both ports).
* Included player weapon-switching state in savegames (fixes some visual bugs with weapons when loading savegames).
* Fixed demo progress bar remaining on screen when using "End Game" to stop demos.
* Made transparent pixels on fullscreen graphics be drawn as black (fixes HOM in `ETERNALL.WAD`'s credits screen).
* Fixed `-uncapped` and `-nouncapped` parameters not being respected.
* Ensured that savegame description strings are null-terminated.

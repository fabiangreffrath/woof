**New Features and Improvements**

* Mod Support:
 - Added support for th base UDMF spec. Including support for new exclusive `"woof"` UDMF namespace.
 - Added support for XGL2, XGL3, ZGL2 and ZGL3 node formats.
 - Added support for UZDoom style obituary assignments in `DEHACKED`, with `Obituary = "x"`, `Melee Obituary = "x"` and `Self Obituary = "x"`.
 - Added support for exclusive `Woof Bits = x` thing property in `DEHACKED`. Monsters in Legacy of Rust now respect corpse flipping.
 - Added support for ID24 sector tinting line specials.
 - Added support for visual only Boom scrollers in vanilla complevels.
 - Added support for dedicated Rekkr autoload directory (via `rekkr-all/`) (thanks @MelodicSpaceship).
 - Added support for ID24 per-state TRANMAPs in `DEHACKED`, via `Tranmap = "x"` thing property.
 - Added support for custom screen wipe defined in `DEMOLOOP`.
 - Added support for the new XBM1 blockmap lump format.
 - Added support for ID24 `endfinale` lump, now plays custom cast sequence in Legacy of Rust and Dominus Diabolicus.

* Audio:
 - Sounds from linedefs now use the mid point of the linedef as the origin, no the midpoint of the line's front sector, matching DSDA's behavior.
 - Replaced Nuked-OPL3 with Nuked-OPL3-fast, a byte identical and faster implementation of the original.

* Rendering:
 - Reworked internal handling of transparency tables:
  - TRANMAPs are now cached locally on-disk.
  - generator now uses linear sRGB blending, as opposed to gamma sRGB, for more accurate color mixing.
  - "transparent ghost monsters" feature now uses additive transparency.
 - Added built-in magenta and black chekcerboard fallback flat to missing floor or ceiling flats.
 - Support for rendering of translucent translated sprites (enables e.g. translucent colored blood).
 - Optimized rendering of very small and/or distant sprites.

* Build:
 - Removed extraneous example WADs and documentation files from final builds.

* Refactor:
 - Moved from Boom-based Dehacked parser to Chocolate Doom's Dehacked parser.
  - Add extra warnings when in debug mode on the terminal.
  - Removed `-dehout`, `-bexout` and `-bex` CLI options.
 - Added DSDA's `ssline` optimiazation for `P_CrossSubsector`.

**Bug Fixes**

* Fixed many SKYDEFS incongruencies to better match ID24 spec.
* Fixed ID24 inventory-reset exits missing from UMAPINFO 0-tag check.
* Fixed MBF high precision scroller math to match DSDA demo behavior.
* Fixed friendly kill count to match DSDA demo behavior (fixes friendly death counts in 'Fast Food 2' and 'One Of Everything').
* Fixed some internal `MT_DOGS` to match DSDA demo behavior.
* Fixed "Bobbing" Weapon Alignment misbehaving with DeHackEd-set weapon sprite offsets (fixes jittering weapons in D4V.wad).
* Disabled interpolation of weapon sprites when sprite and DeHackEd offsets change simultaneously (fixes jittering pistol in DoomZero.wad).
* Included player weapon-switching state in savegames (fixes some visual bugs with weapons when loading savegames).
* Ensured that savegame description strings are null-terminated.

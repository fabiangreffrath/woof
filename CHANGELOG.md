**New Features and Improvements**
* Two-colored HUD widgets (as in Crispy Doom).

**Bug Fixes**
* Fix complevel vanilla scrollers.
* Fix reload level during intermission.
* Check if the lump can be a Doom patch in `R_GenerateLookup()`.
* Fix `gcc-12 -O3` compiler warnings.
* Only create autoload subdirectory relative to `D_DoomPrefDir()`.
* Workaround for optimization bug in clang (taken from Eternity Engine, fixes
  desync in competn/doom/fp2-3655.lmp and dmn01m909.lmp).

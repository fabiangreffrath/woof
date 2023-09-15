**New Features and Improvements**
* 3D audio support (@ceski-1)
  - Stereo and up to 7.1 surround sound.
  - HRTF mode ("Headphones mode" in the General menu).
  - Air absorption and Doppler effects settings in the config.
* PC Speaker emulation sound module (taken from Chocolate Doom).
* Various HUD additions and fixes:
  - Optional widescreen widget arrangement.
  - Bring back three-lined coords/stats widgets.
  - Optionally draw bar graphs in Boom HUD widgets.
  - Ability to position "message" and "secret" text widgets by WOOFHUD lump.
* Add obituaries from ZDoom, enabled by default (Options->Setup->Messages->Show Obituaries option).
* Add support for XGLN/ZGLN nodes.
* Color console messages and optional verbosity level (`default_verbosity` in config).
* Allow separate key binding for the numeric keypad.
* Replace and extend crosshair patches with the shaded variants from Nugget Doom.
* Ignore DMX sound padding (@ceski-1).
* Implement sky top color algorithm from Eternity Engine (@tomas7770).
* Attempt to play `demo4` also for Doom 2 (Final Doom).
* Clean screenshots are taken with the base palette.

**Bug Fixes**
* Fix a savegame loading crash related to the use of MUSINFO.
* Consistently rename `-nodehlump` command line parameter to `-nodeh`.
* Fix mouselook/padlook checks for direct vertical aiming (@ceski-1).
* Fix sector movement doesn't render sometimes using Boom fake floors (Line action 242), when uncapped framerate is enabled.
* Fix automap marks in non-follow mode.
* Various fixes to weapon lowering and switching animation (thanks to @MrAlaux).
* Disable returning to the episodes menu if only one episode is present.
* Fix ESC reset with mini-thermo menu items affects multi-choice select items.
* Reset menu string editing with ESC.
* Fix `PIT_ApplyTorque` when line has no length (from DSDA-Doom).
* Reorder sprites rendering, so that objects with higher map indices appear in front (thanks to @JNechaevsky).
* Various brightmaps fixes (@JNechaevsky).
* Skip "hidden" files in ZIP archives (fixes opening archives created by MacOS).
* Reinitialize automap if screen size changes while it is enabled (thanks to @MrAlaux).

**Miscellaneous**
* Add Linux distribution package in AppImage format (@qurious-pixel).

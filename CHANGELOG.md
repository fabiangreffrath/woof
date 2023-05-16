**New Features and Improvements**
* Allow bind keys to be unbound in menus by pressing the same key.
* Implementation of the Native MIDI module for macOS (fixed regression after removing SDL_Mixer).
* Add `-dumptables` command line option to export generated translation tables to PWAD.
* Windows Native MIDI improvements (@ceski-1):
  - Fix EMIDI global looping.
  - Add MIDI compatibility levels. `winmm_complevel` config option:  
    0: Vanilla (Emulates DMX MPU-401 mode)  
    1: Standard (Emulates MS GS Synth) (Default)  
    2: Full (Send everything to device, including SysEx)  
* Distinguish exit with message on error and on success.

**Bug Fixes**
* Fix memory issues in dehacked parser found with ASan.
* Fix "Smooth pixel scaling" inconsistencies (now it should match Crispy Doom).
* More robust fallback logic for music modules (@joanbm).
* Always print player coords if automap inactive.
* Eat key if cheat found (e.g. don't switch weapons when typing IDCLEV11).
* Resetting the MUSINFO track after changing the level.
* Add initialization checks to music modules. Fixed crash if sound device not found (thanks to @joanbm).
* Fix initialization with invalid `video_display` setting (@joanbm).

**Miscellaneous**

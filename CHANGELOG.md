**New Features and Improvements**
* Add direct aiming from Crispy Doom. More accurate vertical aiming (thanks to @ceski-1).
* Optimizations for plane rendering, taken from Eternity Engine (thanks to @JNechaevsky).

**Bug Fixes**
* Avoid the `midiOutUnprepareHeader()` function in the Windows MIDI music module. This may fix rare crashes for some users.
* Fixed flickering sectors rendering when interpolation is enabled (Ancient Aliens MAP24 and others).
* Do not apply weapon centering in strict mode.
* Fix pause for Fluidsynth v2.3.3.
* Fix the rightmost column when rendering weapons with interpolation enabled (thanks to @MrAlaux).

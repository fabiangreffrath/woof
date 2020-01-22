# This is Woof!
[![Woof! Icon](https://raw.githubusercontent.com/fabiangreffrath/woof/master/data/woof.png)](https://github.com/fabiangreffrath/woof)

[![Travis Build Status](https://img.shields.io/travis/com/fabiangreffrath/woof.svg?style=flat&logo=travis)](https://travis-ci.com/fabiangreffrath/woof/)

Woof! is a continuation of Lee Killough's Doom source port MBF targeted at modern systems.

## Synopsis

[MBF](https://doomwiki.org/wiki/MBF) stands for "Marine's Best Friend" and is regarded by many as the successor of the Boom source port by TeamTNT. It serves as the code base for many of today's successful Doom source ports such as [PrBoom+](https://github.com/coelckers/prboom-plus) or [The Eternity Engine](https://github.com/team-eternity/eternity). As the original engine was limited to run only under MS-DOS, it has been ported to Windows by Team Eternity under the name [WinMBF](https://github.com/team-eternity/WinMBF) in 2004. Woof! is developed based on the WinMBF code with the aim to make MBF more widely available and convenient to use on modern systems.

To achieve this goal, this source port is less strict regarding its faithfulness to the original MBF. It is focused on quality-of-life enhancements, bug fixes and compatibility improvements. However, all changes have been introduced in good faith that they are in line with the original author's intentions and even for the trained eye, this source port should be hard to distinguish from the original MBF.

## What's with the name?

If you turn the [Doom logo upside down](https://www.reddit.com/r/Doom/comments/8476cr/i_would_so_olay_wood/) it reads "Wood" - which would be a pretty stupid name for a source port. "Woof" is just as stupid a name for a source port, but at least it contains a reference to dogs - and dogs are the Marine's Best Friend. ;-)

# Code changes

The following code changes have been introduced in Woof! relative to MBF or WinMBF, respectively.

## General improvements

 * The code has been made 64-bit clean.
 * The code has been ported to SDL-2, the game scene is now rendered to screen using hardware acceleration (if available).
 * Fullscreen mode can be toggled in the General menu section or by pressing <kbd>Alt</kbd>+<kbd>Enter</kbd>, and it is saved in the config file.
 * The complete SDL input and event handling system has been overhauled based on code from Chocolate Doom 3.0.
 * The search path for IWADs has been adapted to more modern requirements, taking the install locations for common download packages into account.
 * On non-Windows systems, volatile data such as config files and savegames are stored in a user writable directory.
 * On Windows systems, support has been added for dragging and dropping WAD and DEH files onto the executable.
 * If the SDL2_Image library is found at runtime, screnshots may be saved in PNG format.
 * The flashing disk icon has been brought back.

## Input

 * Support for up to 5 mouse buttons and up to 12 joystick buttons has been added.
 * Keyboard and mouse bindings for prev/next weapon have been added (bound to buttons 4 and 5, resp. the mouse wheel), joystick bindings for strafe left/right have been added (bound to buttons 5 and 6), joystick bindings for prev/next weapon have been added (prev weapon bound to button 3), joystick bindings for the automap and the main menu have been added.
 * Key and button bindings may be cleared in the respective menu using the <kbd>DEL</kbd> key.

## Bug fixes

 * The famous "3-key door opens with only two keys" bug has been fixed with a compatibility flag.
 * A crash that happened when returning from the menu before a map was loaded has been fixed.
 * A crash when loading a trivial single subsector map has been fixed.
 * Playback of Vanilla Doom demos has been vastly improved.
 * A crash when playing back too short demo lumps (e.g. sunlust.wad) has been fixed.

## Support for more WAD files

 * The IWAD files shipped with the "Doom 3: BFG Edition" and the ones published by the FreeDoom project are now supported.
 * The level building code has been upgraded to use unsigned data types internally, which allows for loading maps that have been built in "extended nodes" format. Other map formats like e.g. DeePBSP and ZDBSP are not (and will probably never be) supported, though.
 * The renderer has been upgraded to use 32-bit integer variables internally, which fixes crashes in levels with extreme heights or height differences.
 * A crash when loading maps with too short REJECT matrices has been fixed.

## Known issues

 * Savegames stored with a 64-bit executable are not compatible with a 32-bit executable - and thus with the original MBF. This is because raw struct data is stored in savegames, and structs contain pointers, and pointers have different sizes on 32-bit and 64-bit architectures.

# Download

The Woof! source code is available at GitHub: https://github.com/fabiangreffrath/woof.

It can cloned via

```
 git clone https://github.com/fabiangreffrath/woof.git
```

Compilation should be as simple as


```
 cd woof
 autoreconf -fiv
 ./configure
 make
```

After successful compilation the resulting binary can be found in the `Source/` directory.

# Contact

The canonical homepage for Woof! is https://github.com/fabiangreffrath/woof

Woof! is maintained by [Fabian Greffrath](mailto:fabian@greffXremovethisXrath.com). 

Please report any bugs, glitches or crashes that you encounter to the GitHub [Issue Tracker](https://github.com/fabiangreffrath/woof/issues).

## Acknowledgement

MBF has always been a special Doom source port for me, as it tought me how to implement 640x400 resolution rendering which was the base for my own source port project [Crispy Doom](https://github.com/fabiangreffrath/crispy-doom). As the original MBF was limited to run only under MS-DOS, I used its pure port WinMBF by Team Eternity to study and debug the code. Over time, I got increasingly frustrated that the code would only compile and run on 32-bit systems and still use the meanwhile outdated SDL-1 libraries. This is how this project was born.

Many additions and improvements to this source port were taken from fraggle's [Chocolate Doom](https://www.chocolate-doom.org/wiki/index.php/Chocolate_Doom), taking advantage of its exceptional portability, accuracy and compatibility.

# Legalese

Copyright © 1993-1996 Id Software, Inc.;
Copyright © 1993-2008 Raven Software;
Copyright © 1999 by id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman;
Copyright © 2004 James Haley;
Copyright © 2005-2014 Simon Howard;
Copyright © 2020 Fabian Greffrath.

This program is free software; you can redistribute it and/or
modify it under the terms of the [GNU General Public License](https://www.gnu.org/licenses/gpl-2.0.html)
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

SDL 2.0, SDL_mixer 2.0 and SDL_net 2.0 are © 1997-2016 Sam Lantinga and are released under the [zlib license](http://www.gzip.org/zlib/zlib_license.html).

The Woof! icon (as shown at the top of this page) has been kindly contributed by @JNechaevsky.

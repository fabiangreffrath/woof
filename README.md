# This is Woof!
[![Woof! Icon](https://raw.githubusercontent.com/fabiangreffrath/woof/master/data/woof.png)](https://github.com/fabiangreffrath/woof)

[![Top Language](https://img.shields.io/github/languages/top/fabiangreffrath/woof.svg?style=flat)](https://github.com/fabiangreffrath/woof)
[![Code Size](https://img.shields.io/github/languages/code-size/fabiangreffrath/woof.svg?style=flat)](https://github.com/fabiangreffrath/woof)
[![License](https://img.shields.io/github/license/fabiangreffrath/woof.svg?style=flat&logo=gnu)](https://github.com/fabiangreffrath/woof/blob/master/COPYING.md)
[![Release](https://img.shields.io/github/release/fabiangreffrath/woof.svg?style=flat)](https://github.com/fabiangreffrath/woof/releases)
[![Release Date](https://img.shields.io/github/release-date/fabiangreffrath/woof.svg?style=flat)](https://github.com/fabiangreffrath/woof/releases)
[![Downloads](https://img.shields.io/github/downloads/fabiangreffrath/woof/latest/total.svg?style=flat)](https://github.com/fabiangreffrath/woof/releases)
[![Commits](https://img.shields.io/github/commits-since/fabiangreffrath/woof/latest.svg?style=flat)](https://github.com/fabiangreffrath/woof/commits/master)
[![Last Commit](https://img.shields.io/github/last-commit/fabiangreffrath/woof.svg?style=flat)](https://github.com/fabiangreffrath/woof/commits/master)
[![Travis Build Status](https://img.shields.io/travis/com/fabiangreffrath/woof.svg?style=flat&logo=travis)](https://travis-ci.com/fabiangreffrath/woof/)

Woof! is a continuation of Lee Killough's Doom source port MBF targeted at modern systems.

## Synopsis

[MBF](https://doomwiki.org/wiki/MBF) stands for "Marine's Best Friend" and is regarded by many as the successor of the Boom source port by TeamTNT. It serves as the code base for many of today's successful Doom source ports such as [PrBoom+](https://github.com/coelckers/prboom-plus) or [The Eternity Engine](https://github.com/team-eternity/eternity). As the original engine was limited to run only under MS-DOS, it has been ported to Windows by Team Eternity under the name [WinMBF](https://github.com/team-eternity/WinMBF) in 2004. Woof! is developed based on the WinMBF code with the aim to make MBF more widely available and convenient to use on modern systems.

To achieve this goal, this source port is less strict regarding its faithfulness to the original MBF. It is focused on quality-of-life enhancements, bug fixes and compatibility improvements. However, all changes have been introduced in good faith that they are in line with the original author's intentions and even for the trained eye, this source port should be hard to distinguish from the original MBF.

In summary, this project's goal is to forward-port MBF.EXE from DOS to year 2020 and remove all the stumblings blocks on the way.

## What's with the name?

If you turn the [Doom logo upside down](https://www.reddit.com/r/Doom/comments/8476cr/i_would_so_olay_wood/) it reads "Wood" - which would be a pretty stupid name for a source port. "Woof" is just as stupid a name for a source port, but at least it contains a reference to dogs - and dogs are the Marine's Best Friend. ;-)

# Code changes

The following code changes have been introduced in Woof! relative to MBF or WinMBF, respectively.

## General improvements

 * The code has been made 64-bit compatible.
 * The code has been ported to SDL-2, the game scene is now rendered to screen using hardware acceleration (if available).
 * The build system has been ported to CMake with support for building on Linux and Windows, using either MSVC or MinGW and the latter either in a cross-compiling or a native MSYS2 environment (@AlexMax).
 * Support for rendering with uncapped frame rate and frame interpolation has been added (since Woof! 2.0.0).
 * Fullscreen mode can be toggled in the General menu section or by pressing <kbd>Alt</kbd>+<kbd>Enter</kbd>, and it is now saved in the config file.
 * The complete SDL input and event handling system has been overhauled based on code from Chocolate Doom 3.0 (mouse acceleration is disabled since Woof! 1.1.0).
 * The search path for IWADs has been adapted to modern requirements, taking the install locations for common download packages into account.
 * On non-Windows systems, volatile data such as config files and savegames are stored in a user writable directory.
 * On Windows systems, support for dragging and dropping WAD and DEH files onto the executable has been added (fixed in Woof! 1.0.1).
 * Screnshots are saved in PNG format using the SDL2_Image library (since Woof! 1.1.0, optional since Woof! 1.0.0).
 * The sound system has been completely overhauled, letting SDL_Mixer do the actual sound mixing and getting rid of the fragile sound channel locking mechanism.
 * The original Spectre/Invisibility fuzz effect has been brought back.
 * The flashing disk icon has been brought back.
 * Error messages now appear in a pop-up window.
 * All non-free embedded lumps have been either removed or replaced.
 * Support for helper dogs and beta emulation is now unconditionally enabled.
 * 32 MiB are now allocated by default for zone memory.
 * The rendering of flats has been improved (visplanes with the same flats now match up far better than before and the distortion of flats towards the right of the screen has been fixed, since Woof! 1.1.0).
 * The "long wall wobble" glitch has been fixed (since Woof! 1.1.0).
 * Sectors with the same visplane for ceiling and sky are now rendered correctly (e.g. eviternity.wad MAP30).
 * The automap now updates while playing (since Woof! 1.2.0).
 * A "demowarp" feature has been added allowing to fast-forward to the desired map in a demo (since Woof! 1.2.0).
 * Level stats and level time widgets have been added to the automap (since Woof! 2.2.0).
 * The weapon sprites can now be centered during attacks (since Woof! 2.2.0).
 * The player coordinates widget on the Automap is now optional (since Woof! 3.0.0).
 * Sounds may now be played in their full length (since Woof! 3.0.0). However, this only applies to sounds originating from (removed) map objects, not to those that emerge "in the player's head".
 * All textures are now always composed, whether they are multi-patched or not. Furthermore, two separate composites are created, one for opaque and one for translucent mid-textures on 2S walls. Additionally, textures may now be arbitrarily tall (since Woof! 3.0.0).
 * A new wrapping column getter function has been introduced to allow for non-power-of-two wide mid-textures on 2S walls (since Woof! 3.0.0).

## Input

 * Support for up to 5 mouse buttons and up to 12 joystick buttons has been added.
 * Keyboard and mouse bindings for prev/next weapon have been added (bound to buttons 4 and 5, resp. the mouse wheel), joystick bindings for strafe left/right have been added (bound to buttons 5 and 6), joystick bindings for prev/next weapon have been added (prev weapon bound to button 3), joystick bindings for the automap and the main menu have been added.
 * Key and button bindings may be cleared in the respective menu by using the <kbd>DEL</kbd> key.
 * Movement keys are bound to the WASD scheme by default.
 * Menu control by mouse has been disabled.
 * The "Run" key inverts "Always Run" behaviour (since Woof! 2.1.0).
 * Key bindings have been added to restart a level or go to the next level (since Woof! 2.1.0).
 * A mouse button binding for the "Use" action has been added. Double click acting as "Use" has been made optional (since Woof! 2.3.0).

## Bug fixes

 * The famous "3-key door opens with only two keys" bug has been fixed with a compatibility flag.
 * A crash when returning from the menu before a map was loaded has been fixed.
 * A crash when loading a trivial single subsector map has been fixed.
 * A crash when playing back too short demo lumps (e.g. sunlust.wad) has been fixed.
 * A crash when the attack sound for the Lost Soul is missing has been fixed (e.g. ludicrm.wad MAP05).
 * A bug in the translucency table caching has been fixed which would lead to garbled translucency effects for WAD files with custom PLAYPAL lumps.
 * Playback compatility with Vanilla Doom and Boom 2.02 demos has been vastly improved.
 * A crash when a Dehacked patch attempts to assign a non-existent code pointer has been fixed (since Woof! 2.2.0).
 * The "Ouch Face" and the "Picked up a Medikit that you really need" message are now shown as intended (since Woof! 2.3.0).

## Support for more WAD files

 * The IWAD files shipped with the "Doom 3: BFG Edition" and the ones published by the Freedoom project are now supported.
 * The level building code has been upgraded to use unsigned data types internally, which allows for loading maps that have been built in "extended nodes" format. 
 * Furthermore, maps using nodes in DeePBSP and (compressed or uncompressed) ZDBSP formats can now be loaded.
 * The renderer has been upgraded to use 32-bit integer types internally (64-bit integer types in parts since Woof! 3.0.0), which fixes crashes in levels with extreme heights or height differences (e.g. ludicrm.wad MAP03 or Eviternity.wad MAP27).
 * A crash when loading maps with too short REJECT matrices has been fixed.
 * A crash when loading maps with too short BLOCKMAP lumps has been fixed (since Woof! 1.2.0).
 * The port is now more forgiving when composing textures with missing patches (which will be substituted by a dummy patch) or empty columns (which will be treated as transparent areas).
 * The port is now more forgiving when a flat lump cannot be found and renders the sky texture instead (since Woof! 1.1.0).
 * The port is now more forgiving when a texture lump cannot be found and renders no texture instead (since Woof! 1.2.2).
 * The port is now more forgiving when a sprite rotation is missing (since Woof! 1.2.0).
 * Some nasty rendering and automap glitches have been fixed which became apparent especially in extremely huge levels (e.g. planisf2.wad, since Woof! 1.1.0).
 * Maps without level name graphics do not crash during the intermission screen anymore.
 * Extra states, sprites and mobjtypes have been added for use in Dehacked patches (since Woof! 1.2.0).
 * Support for tall textures and sprites in DeePsea format has been added (since Woof! 1.2.2).
 * A crash is fixed when loading a PWAD which contains empty DEHACKED lumps (e.g. ElevenZero.wad, since Woof! 3.0.0).

## Known issues

 * Savegames stored by a 64-bit executable cannot be restored by a 32-bit executable - and are thus incompatible with the original MBF. This is because raw struct data is stored in savegames, and these structs contain pointers, and pointers have different sizes on 32-bit and 64-bit architectures.
 * IWAD files of Doom version 1.1 and earlier are not supported anymore, as they are missing lumps that are not embedded into the executable anymore (full support for Doom 1.2 since Woof! 1.2.0).

# Download

The Woof! source code is available at GitHub: https://github.com/fabiangreffrath/woof.

It can cloned via

```
 git clone https://github.com/fabiangreffrath/woof.git
```

## Linux

You will need to install the SDL2, SDL2_image, SDL2_mixer and SDL2_net libraries.  Usually your distribution has these libraries in its repositories, and if your distribution has "dev" versions of those libraries, those are the ones you'll need.

Once installed, compilation should be as simple as:

```
 cd woof
 mkdir build; cd build
 cmake -DCMAKE_BUILD_TYPE=Debug ..
 make
```

If you want a release build, use `Release` for the build type instead of `Debug`.  Also, if you happen to have [Ninja](https://ninja-build.org/) installed, you can add `-G Ninja` to the `cmake` invocation for faster build times.

After successful compilation the resulting binary can be found in the `Source/` directory.

## Windows

Visual Studio 2019 comes with built-in support for CMake by opening the source tree as a folder.  Otherwise, you should probably use the GUI tool included with CMake to set up the project and generate build files for your tool or IDE of choice.

It's worth noting that you do not need to download any dependencies.  The build system will automatically download them for you.

## Cross-compiling

You can cross-compile from Linux to Windows.  First, make sure you have a reasonably recent version of mingw-w64 installed.  From there, cross-compiling should be as easy as:

```
 cd woof
 mkdir build; cd build
 cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=../CrossToWin64.cmake ..
 make
```

Much like a native Windows build, you do not need to download any dependencies.

# Version History

 * 0.9.0 (Jan 21, 2020)  
   First test build under the moniker Woof!.
 * 1.0.0 (Feb 28, 2020)  
   Initial release, introducing support for extended nodes in DeePBSP and ZDBSP formats.
 * 1.0.1 (Mar 03, 2020)  
   Hot-fix release, fixing drag-n-drop and adding back support for the Doom 1.2 IWAD.
 * 1.1.0 (Mar 13, 2020)  
   Major release, fixing rendering glitches in huge levels.
 * 1.2.0 (Apr 14, 2020)  
   Major release, introducing precaching of sound effects.
 * 1.2.1 (May 05, 2020)  
   Bug-fix release, fixing drag-n-drop for IWAD files and endianess for extended nodes.
 * 1.2.2 (Jun 09, 2020)  
   Minor release, adding support for tall patches and sprites.
 * 2.0.0 (Jul 03, 2020)  
   Major release, introducing rendering with uncapped frame rate and frame interpolation.
 * 2.0.1 (Jul 10, 2020)  
   Bug-fix release, fixing rendering of linedef type 242 fake floors and ceilings.
 * 2.0.2 (Jul 20, 2020)  
   Bug-fix release, more fixes to linedef type 242 fake floors and ceilings rendering.
 * 2.1.0 (Aug 17, 2020)  
   Minor release, adding support for inverting "Always Run" with the "Run" key and new key bindings to restart a level or go to the next level.
 * 2.1.1 (Sep 03, 2020)  
   Bug-fix release, fixing linedef type 242 rendering with moving control sectors and SDL2_Mixer opening a different number of audio channels than requested.
 * 2.2.0 (Sep 11, 2020)  
   Feature release, adding level stats and level time widgets to the automap and optional weapon sprite centering during attacks.
 * 2.3.0 (Sep 21, 2020)  
   Feature release, adding a mouse button binding for the "Use" action.
 * 2.3.1 (Sep 30, 2020)  
   Bug-fix release, fixing the vertical position of the level stats widgets and finale text lines exceeding screen width.
 * 2.3.2 (Oct 19, 2020)  
   Bug-fix release, fixing a crash when the second finale text screen is shown.
 * 3.0.0 (Nov 30, 2020)  
   Major release, attempting to fix all known texture rendering bugs. Also adding support for sounds played at full length and optional player coordinates on the Automap.
 * 3.1.0 (Jan 08, 2021)  
   Feature release, adding a choice of centered or bobbing weapon sprite during attack, a default save slot name when the user saves to an empty slot and total time for all completed levels.

# Contact

The canonical homepage for Woof! is https://github.com/fabiangreffrath/woof

Woof! is maintained by [Fabian Greffrath](mailto:fabian@greffXremovethisXrath.com). 

Please report any bugs, glitches or crashes that you encounter to the GitHub [Issue Tracker](https://github.com/fabiangreffrath/woof/issues).

## Acknowledgement

MBF has always been a special Doom source port for me, as it tought me how to implement 640x400 resolution rendering which was the base for my own source port project [Crispy Doom](https://github.com/fabiangreffrath/crispy-doom). As the original MBF was limited to run only under MS-DOS, I used its pure port WinMBF by Team Eternity to study and debug the code. Over time, I got increasingly frustrated that the code would only compile and run on 32-bit systems and still use the meanwhile outdated SDL-1 libraries. This is how this project was born.

Many additions and improvements to this source port were taken from fraggle's [Chocolate Doom](https://www.chocolate-doom.org/wiki/index.php/Chocolate_Doom), taking advantage of its exceptional portability, accuracy and compatibility.

# Legalese

Files: `*`  
Copyright: © 1993-1996 Id Software, Inc.;  
 © 1993-2008 Raven Software;  
 © 1999 by id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman;  
 © 2004 James Haley;  
 © 2005-2014 Simon Howard;  
 © 2020-2021 Fabian Greffrath;  
 © 2020 Alex Mayfield.  
License: [GPL-2.0+](https://www.gnu.org/licenses/old-licenses/gpl-2.0.html)

Files: `Source/beta.h`  
Copyright: © 2001-2019 Contributors to the Freedoom project.  
License: [BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause)

Files: `Source/dogs.h`  
Copyright: © 2017 Nash Muhandes;  
 © apolloaiello;  
 © TobiasKosmos.  
License: [CC-BY-3.0](https://creativecommons.org/licenses/by/3.0/) and [CC0-1.0](https://creativecommons.org/publicdomain/zero/1.0/)

Files: `data/woof.ico,  
 data/woof.png,  
 data/woof8.ico,  
 Source/icon.c`  
Copyright: © 2020 Julia Nechaevskaya.  
License: [CC-BY-3.0](https://creativecommons.org/licenses/by/3.0/)

Files: `cmake/FindSDL2.cmake,  
 cmake/FindSDL2_image.cmake,  
 cmake/FindSDL2_mixer.cmake,  
 cmake/FindSDL2_net.cmake`  
Copyright: © 2018 Alex Mayfield.  
License: [BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause)

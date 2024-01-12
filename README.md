# This is Woof!
[![Woof! Icon](https://raw.githubusercontent.com/fabiangreffrath/woof/master/data/woof.png)](https://github.com/fabiangreffrath/woof)

[![Top Language](https://img.shields.io/github/languages/top/fabiangreffrath/woof.svg)](https://github.com/fabiangreffrath/woof)
[![Code Size](https://img.shields.io/github/languages/code-size/fabiangreffrath/woof.svg)](https://github.com/fabiangreffrath/woof)
[![License](https://img.shields.io/github/license/fabiangreffrath/woof.svg?logo=gnu)](https://github.com/fabiangreffrath/woof/blob/master/COPYING.md)
[![Release](https://img.shields.io/github/release/fabiangreffrath/woof.svg)](https://github.com/fabiangreffrath/woof/releases/latest)
[![Release Date](https://img.shields.io/github/release-date/fabiangreffrath/woof.svg)](https://github.com/fabiangreffrath/woof/releases/latest)
[![Downloads](https://img.shields.io/github/downloads/fabiangreffrath/woof/latest/total.svg)](https://github.com/fabiangreffrath/woof/releases/latest)
[![Commits](https://img.shields.io/github/commits-since/fabiangreffrath/woof/latest.svg)](https://github.com/fabiangreffrath/woof/commits/master)
[![Last Commit](https://img.shields.io/github/last-commit/fabiangreffrath/woof.svg)](https://github.com/fabiangreffrath/woof/commits/master)
[![Build Status](https://github.com/fabiangreffrath/woof/actions/workflows/main.yml/badge.svg)](https://github.com/fabiangreffrath/woof/actions/workflows/main.yml)

Woof! is a continuation of Lee Killough's Doom source port MBF targeted at modern systems.

## Synopsis

[MBF](https://doomwiki.org/wiki/MBF) stands for "Marine's Best Friend" and is widely regarded as the successor of the Boom source port by TeamTNT.
It serves as the code base for popular Doom source ports such as [PrBoom+](https://github.com/coelckers/prboom-plus)/[DSDA-Doom](https://github.com/kraflab/dsda-doom) or [The Eternity Engine](https://github.com/team-eternity/eternity).
As the original engine was limited to run only under MS-DOS, it has been ported to Windows by Team Eternity under the name [WinMBF](https://github.com/team-eternity/WinMBF) in 2004.
Woof! is developed based on the WinMBF code with the aim to make MBF more widely available and convenient to use on modern systems.

To achieve this goal, this source port is less strict regarding its faithfulness to the original MBF.
It is focused on quality-of-life enhancements, bug fixes and compatibility improvements.
However, all changes have been introduced in good faith that they are in line with the original author's intentions and even for the trained eye, this source port should still look very familiar to the original MBF.

In summary, this project's goal is to fast-forward `MBF.EXE` from DOS to 21st century and remove all the stumbling blocks on the way.
Furthermore, just as MBF was ahead of its time, this project dedicates itself to early adoption of new modding features such as [DEHEXTRA](https://doomwiki.org/wiki/DEHEXTRA)+[DSDHacked](https://doomwiki.org/wiki/DSDHacked), [UMAPINFO](https://doomwiki.org/wiki/UMAPINFO) and [MBF21](https://doomwiki.org/wiki/MBF21).

## What's with the name?

If you turn the [Doom logo upside down](https://www.reddit.com/r/Doom/comments/8476cr/i_would_so_olay_wood/) it reads "Wood" - which would be a pretty stupid name for a source port.
"Woof" is just as stupid a name for a source port, but at least it contains a reference to dogs - and dogs are the *Marine's Best Friend*. :wink:

# Key features

The following key features have been introduced in Woof! relative to MBF or WinMBF, respectively.

## General

 * The code has been made 64-bit and big-endian compatible.
 * The code has been ported to SDL2, so the game scene is now rendered to screen using hardware acceleration if available.
 * The build system has been ported to CMake with support for building on Linux and Windows (the latter using either vcpkg or a MSYS2 environment).
 * All non-free embedded lumps have been either removed or replaced.
 * Support for "autoload" directories has been added, both for common ("doom-all") and per IWAD files. WAD files in these directories are loaded before those passed to the `-file` parameter, DEHACKED files in these directories are processed after those passed to the `-deh` parameter and before those embedded into WAD files. Additionally, autoload directories for PWADs are also supported in a similar manner, but these directories will need to be created manually.
 * Savegame backward compatibility across different platforms and releases is maintained, so it is possible to restore savegames from previous versions and even `MBF.EXE`.
 * Integration of the Chocolate Doom network code. Demo compatible multiplayer for all supported complevels allowing for cross-port network games with the Chocolate family of ports, e.g. Crispy Doom (remember to set `-complevel vanilla` for the server).

## Rendering

 * The renderer has been made much more robust against common rendering bugs which were found especially in extremely huge levels and levels with extreme heights or height differences.
 * Support for rendering with uncapped frame rate and frame interpolation has been added.
 * A widescreen rendering mode has been added with proper support for the widescreen assets found e.g. in the Unity version of Doom.
 * Screenshots are saved in PNG format and are actual representations of the game screen rendering.
 * Support for Voxels in KVX format has been added.

## Sound

 * The port uses OpenAL for music and sound mixing. It is able to play back sounds in any format supported by libsndfile and precaches them in memory at start up.
 * The port features optional 3D audio, supporting stereo and up to 7.1 surround sound with an optional HRTF mode, as well as PC speaker emulation.
 * The port provides several music backends: MIDI music is played back natively on Windows, or using the fluidsynth renderer with a soundfont (if available), or using the built-in OPL3 emulator. Digital music formats are played back with libsndfile, module music is rendered with libxmp.

## Input

 * The port allows to bind each action to multiple keys and mouse or joystick buttons, and unbind them using the <kbd>Del</kbd> key in the menu.
 * The port provides modern gamepad support allowing to bind any action to shoulder triggers or analog sticks.
 * Support for mouselook has been added.

## Capability

 * The level building code has been upgraded to allow for loading maps in "extended nodes" format. Furthermore, maps using nodes in uncompressed XNOD/XGLN or compressed ZNOD/ZGLN formats, or DeePBSP format can now be loaded.
 * Support for tall textures and sprites in DeePsea format has been added.
 * Support for unlimited extra states, sprites, mobjtypes and sounds has been added for use in Dehacked patches (supporting the "DEHEXTRA" and "DSDHacked" specs).
 * Support for changing in-game music using the "MUSINFO" lump has been added.
 * Support for the "UMAPINFO" lump has been added, compliant to Rev 2.2 of the [spec](https://github.com/kraflab/umapinfo).
 * Support for the "MBF21" compatibility level has been added, compliant to Rev 1.4 of the [spec](https://github.com/kraflab/mbf21).
 * Support for SMMU swirling animated flats.
 * Ability to customize the appearance of the extended Boom HUD using the [WOOFHUD](https://github.com/fabiangreffrath/woof/blob/master/docs/woofhud.md) lump.

## Compatibility

 * All IWAD files since Doom 1.2 are supported, including Doom Shareware, The Ultimate Doom, Doom 2 and Final Doom. Furthermore, the IWAD files shipped with the "Doom 3: BFG Edition" and the ones published by the Freedoom project as well as Chex Quest, HACX and REKKR are supported.
 * UMAPINFO lumps for MASTERLEVELS.WAD, NERVE.WAD, SIGIL_V1_21.WAD, E1M4B.WAD and E1M8B.WAD have been added which are meant to be autoloaded with the corresponding PWAD and change map title, level transitions, par times, music and skies accordingly.
 * The concept of compatibility levels has been added, currently offering "Vanilla", "Boom", "MBF" and "MBF21" (default). The default compatibility level may be changed through the menu and overridden with the `-complevel` parameter, allowing for both numeric and named arguments, or the `COMPLVL` lump. Menu items in the Setup menu that don't apply to the current compatibility level are disabled and grayed out.
 * The project strives for full demo compatibility for all supported complevels. Furthermore, this port offers quality-of-life features like e.g. a demo progress bar, demo playback warp and skip as well as continued demo recording.
 * The SPECHITS, REJECT, INTERCEPTS and DONUT overflow emulations have been ported over from Chocolate Doom / PrBoom+, allowing for some more obscure Vanilla demos to keep sync.

# Releases

Source code and Windows binaries (MSVC builds for Windows 7 and newer) for the latest release can be found on the [Release](https://github.com/fabiangreffrath/woof/releases/latest) page.

The most recent list of changes can be found in the [Changelog](https://github.com/fabiangreffrath/woof/blob/master/CHANGELOG.md).

A complete history of changes and releases can be found in the [Wiki](https://github.com/fabiangreffrath/woof/wiki/Changelog) or on the [Releases](https://github.com/fabiangreffrath/woof/releases) page.

# Compiling

The Woof! source code is available at GitHub: <https://github.com/fabiangreffrath/woof>.

It can be cloned via

```
 git clone https://github.com/fabiangreffrath/woof.git
```

## Linux, and Windows with MSYS2

The following libraries need to be installed:

 * [SDL2](https://github.com/libsdl-org/SDL/tree/SDL2) (>= 2.0.18)
 * [SDL2_net](https://github.com/libsdl-org/SDL_net)
 * [openal-soft](https://github.com/kcat/openal-soft) (>= 1.22.0 for PC Speaker emulation)
 * [libsndfile](https://github.com/libsndfile/libsndfile) (>= 1.1.0 for MPEG support)
 * [fluidsynth](https://github.com/FluidSynth/fluidsynth) (>= 2.2.0, optional)
 * [libxmp](https://github.com/libxmp/libxmp) (optional)
 
Usually your distribution should have the corresponding packages in its repositories, and if your distribution has "dev" versions of those libraries, those are the ones you'll need.

Once installed, compilation should be as simple as:

```
 cd woof
 mkdir build; cd build
 cmake ..
 make
```

After successful compilation the resulting binary can be found in the `src/` directory.

## Windows with Visual Studio

Visual Studio 2019 and [VSCode](https://code.visualstudio.com/) comes with built-in support for CMake by opening the source tree as a folder.

Install vcpkg <https://github.com/Microsoft/vcpkg#quick-start-windows>. Integrate it into CMake or use toolchain file:
```
 cd woof
 cmake -B build -DCMAKE_TOOLCHAIN_FILE="[path to vcpkg]/scripts/buildsystems/vcpkg.cmake"
 cmake --build build
```
CMake will automatically download and build all dependencies for you.

# Contact

The canonical homepage for Woof! is <https://github.com/fabiangreffrath/woof>.

Woof! is maintained by [Fabian Greffrath](mailto:fabian@greffXremovethisXrath.com). 

Please report any bugs, glitches or crashes that you encounter to the GitHub [Issue Tracker](https://github.com/fabiangreffrath/woof/issues).

## Acknowledgement

MBF has always been a special Doom source port for me, as it taught me how to implement 640x400 resolution rendering which was the base for my own source port project [Crispy Doom](https://github.com/fabiangreffrath/crispy-doom).
As the original MBF was limited to run only under MS-DOS, I used its pure port WinMBF by Team Eternity to study and debug the code.
Over time, I got increasingly frustrated that the code would only compile and run on 32-bit systems and still use the meanwhile outdated SDL-1 libraries.
This is how this project was born.

Many additions and improvements to this source port were taken from fraggle's [Chocolate Doom](https://www.chocolate-doom.org/wiki/index.php/Chocolate_Doom), taking advantage of its exceptional portability, accuracy and compatibility.

# Legalese

Files: `*`  
Copyright:  
 © 1993-1996 Id Software, Inc.;  
 © 1993-2008 Raven Software;  
 © 1999 by id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman;  
 © 1999-2004 by Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze;  
 © 2004 James Haley;  
 © 2005-2006 by Florian Schulze, Colin Phipps, Neil Stevens, Andrey Budko;  
 © 2005-2018 Simon Howard;  
 © 2006 Ben Ryves;  
 © 2017 Christoph Oelckers;  
 © 2019 Fernando Carmona Varo;  
 © 2020 Alex Mayfield;  
 © 2020-2023 Fabian Greffrath;  
 © 2020-2023 Roman Fomin;  
 © 2021 Ryan Krafnick;  
 © 2022-2023 Alaux;  
 © 2022-2023 ceski;  
 © 2023 Andrew Apted;  
 © 2023 liPillON.  
License: [GPL-2.0+](https://www.gnu.org/licenses/old-licenses/gpl-2.0.html)

Files: `src/beta.h`  
Copyright: © 2001-2019 Contributors to the Freedoom project.  
License: [BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause)

Files: `src/dogs.h`  
Copyright:  
 © 2017 Nash Muhandes;  
 © apolloaiello;  
 © TobiasKosmos.  
License: [CC-BY-3.0](https://creativecommons.org/licenses/by/3.0/) and [CC0-1.0](https://creativecommons.org/publicdomain/zero/1.0/)

Files: `src/nano_bsp.*`  
Copyright: © 2023 Andrew Apted.  
License: [MIT](https://opensource.org/licenses/MIT)

Files: `src/u_scanner.*`  
Copyright:  
 © 2010 Braden "Blzut3" Obrzut;  
 © 2019 Fernando Carmona Varo.  
License: [BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause)

Files: `src/v_flextran.*`  
Copyright:  
 © 2013 James Haley et al.;  
 © 1998-2012 Marisa Heit.  
License: [GPL-3.0+](https://www.gnu.org/licenses/gpl-3.0)

Files: `cmake/FindSDL2.cmake, cmake/FindSDL2_net.cmake`  
Copyright: © 2018 Alex Mayfield.  
License: [BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause)

Files: `data/woof.ico, data/woof.png, src/icon.c, data/setup.ico, data/woof-setup.png, setup/setup_icon.c, src/thermo.h`  
Copyright: © 2020-2022 Julia Nechaevskaya.  
License: [CC-BY-3.0](https://creativecommons.org/licenses/by/3.0/)

Files: `miniz/*`  
Copyright:  
 © 2010-2014 Rich Geldreich and Tenacious Software LLC;  
 © 2013-2014 RAD Game Tools and Valve Software.  
License: [MIT](https://opensource.org/licenses/MIT)

Files: `opl/*`  
Copyright:  
 © 2005-2014 Simon Howard;  
 © 2013-2018 Alexey Khokholov (Nuke.YKT).  
License: [GPL-2.0+](https://www.gnu.org/licenses/old-licenses/gpl-2.0.html)

Files: `soundfonts/TimGM6mb.sf2`  
Copyright:  
 © 2004 Tim Brechbill;  
 © 2010 David Bolton.  
License: [GPL-2.0](https://www.gnu.org/licenses/old-licenses/gpl-2.0.html)

Files: `textscreen/*`  
Copyright:  
 © 1993-1996 Id Software, Inc.;  
 © 2002-2004 The DOSBox Team;  
 © 2005-2017 Simon Howard.  
License: [GPL-2.0+](https://www.gnu.org/licenses/old-licenses/gpl-2.0.html)

Files: `win32/win_opendir.*`  
License: public-domain

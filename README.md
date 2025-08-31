# This is Woof!
[![Woof! Icon](https://raw.githubusercontent.com/fabiangreffrath/woof/master/data/woof.png)](https://github.com/fabiangreffrath/woof)

[![Top Language](https://img.shields.io/github/languages/top/fabiangreffrath/woof.svg)](https://github.com/fabiangreffrath/woof)
[![Code Size](https://img.shields.io/github/languages/code-size/fabiangreffrath/woof.svg)](https://github.com/fabiangreffrath/woof)
[![License](https://img.shields.io/github/license/fabiangreffrath/woof.svg?logo=gnu)](https://github.com/fabiangreffrath/woof/blob/master/COPYING.md)
[![Release](https://img.shields.io/github/release/fabiangreffrath/woof.svg)](https://github.com/fabiangreffrath/woof/releases/latest)
[![Release Date](https://img.shields.io/github/release-date/fabiangreffrath/woof.svg)](https://github.com/fabiangreffrath/woof/releases/latest)
[![Downloads (total)](https://img.shields.io/github/downloads/fabiangreffrath/woof/total)](https://github.com/fabiangreffrath/woof/releases/latest)
[![Downloads (latest)](https://img.shields.io/github/downloads/fabiangreffrath/woof/latest/total.svg)](https://github.com/fabiangreffrath/woof/releases/latest)
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

 * Arbitrary resolutions up to native display resolution with dynamic resolution scaling (DRS) to automatically change the internal resolution to maintain a target framerate.
 * Widescreen rendering with proper support for widescreen assets.
 * Rendering with uncapped frame rate and frame interpolation.
 * Adjustable field of view (FOV).
 * Support for voxels in KVX format.
 * 3D audio, supporting stereo and up to 7.1 surround sound with an optional HRTF mode, as well as PC speaker emulation.
 * Several music backends: native MIDI, FluidSynth with a bundled soundfont, built-in OPL3 emulator. Digital music and sound formats supported by libsndfile, module music supported by libxmp.
 * [Modern gamepad support](https://github.com/fabiangreffrath/woof/wiki/Gamepad) featuring rumble, gyro, flick stick, custom weapon slots, and full control of sensitivities, response curves, and deadzones.
 * Mouselook.
 * Autoload directories.
 * Savegame backward compatibility up to `MBF.EXE`.
 * Integration of the Chocolate Doom network code.
 * Compatibility levels: "Vanilla", "Boom", "MBF" and "MBF21" (default).

## Capabilities

 * Support for extended nodes in uncompressed XNOD or XGLN and compressed ZNOD or ZGLN formats, and DeePBSP format.
 * Integration of the NanoBSP node builder (enforce with `-bsp`) for maps without nodes or with unsupported nodes (not demo compatible).
 * Tall textures and sprites in DeePsea format.
 * Unlimited extra states, sprites, mobjtypes and sounds for use in Dehacked patches (supporting the "DEHEXTRA" and "DSDHacked" specs).
 * Ambient sounds using SNDINFO and DoomEdNums 14001 to 14064.
 * In-game music changing using MUSINFO.
 * UMAPINFO support, compliant to Rev 2.2 of the [spec](https://github.com/kraflab/umapinfo).
 * MBF21 compatibility level, compliant to Rev 1.4 of the [spec](https://github.com/kraflab/mbf21).
 * Support for PNG graphics.
 * SMMU-style swirling animated flats.

## Usage

 * [Getting Started](https://github.com/fabiangreffrath/woof/wiki/Getting-Started)
 * [Cheat Codes](https://github.com/fabiangreffrath/woof/wiki/Cheats)
 * [Command Line Parameters](https://github.com/fabiangreffrath/woof/wiki/Command-Line-Parameters)

# Releases

Source code, Windows binaries (MSVC builds for Windows 7 and newer) and Linux AppImages for the latest release can be found on the [Release](https://github.com/fabiangreffrath/woof/releases/latest) page.

The most recent list of changes can be found in the current [Changelog](https://github.com/fabiangreffrath/woof/blob/master/CHANGELOG.md).

# Compiling

The Woof! source code is available at GitHub: <https://github.com/fabiangreffrath/woof>.

It can be cloned via

```
 git clone https://github.com/fabiangreffrath/woof.git
```

## Linux, and Windows with MSYS2

The following build system and libraries need to be installed:
 
 * [CMake](https://cmake.org) (>= 3.15)
 * [SDL2](https://github.com/libsdl-org/SDL/tree/SDL2) (>= 2.0.18)
 * [openal-soft](https://github.com/kcat/openal-soft) (>= 1.22.0 for PC Speaker emulation)
 * [libsndfile](https://github.com/libsndfile/libsndfile) (>= 1.1.0 for MPEG support)
 * [libebur128](https://github.com/jiixyj/libebur128) (>= 1.2.0)
 * [yyjson](https://github.com/ibireme/yyjson) (>= 0.10.0, optional)
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

Install vcpkg <https://github.com/Microsoft/vcpkg?tab=readme-ov-file#get-started>. 

Run the CMake configuration:
```
 cd woof
 cmake -B build -DCMAKE_TOOLCHAIN_FILE="[path to vcpkg]/scripts/buildsystems/vcpkg.cmake"
```
During this step, vcpkg will build all the dependencies.

Build the project:
```
 cmake --build build
```

# Contact

The canonical homepage for Woof! is <https://github.com/fabiangreffrath/woof>.

Woof! is maintained by [Fabian Greffrath](mailto:fabian@greffXremovethisXrath.com). 

Please report any bugs, glitches or crashes that you encounter to the GitHub [Issue Tracker](https://github.com/fabiangreffrath/woof/issues).

## Acknowledgement

MBF has always been a special Doom source port for me, as it taught me how to implement 640x400 resolution rendering which was the base for my own source port project [Crispy Doom](https://github.com/fabiangreffrath/crispy-doom).
As the original MBF was limited to run only under MS-DOS, I used its pure port WinMBF by Team Eternity to study and debug the code.
Over time, I got increasingly frustrated that the code would only compile and run on 32-bit systems and still use the meanwhile outdated SDL-1 libraries.
This is how this project was born.

Many additions and improvements to this source port were taken from fraggle's [Chocolate Doom](https://www.chocolate-doom.org/wiki/index.php/Chocolate_Doom), taking advantage of its exceptional portability, accuracy and compatibility. Further sources of inspiration are [PrBoom+](https://github.com/coelckers/prboom-plus), [DSDA-Doom](https://github.com/kraflab/dsda-doom) and [The Eternity Engine](https://github.com/team-eternity/eternity).

Special thanks to @rfomin and @ceski-1 for implementing the more advanced features of this port, and @JNechaevsky and @MrAlaux for helping to port back some useful features from their own source port projects!

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
 © 2006-2025 by The Odamex Team;  
 © 2007-2011 Moritz "Ripper" Kroll;  
 © 2008-2019 Simon Judd;  
 © 2013-2025 Brad Harding;  
 © 2017 Christoph Oelckers;  
 © 2020 Alex Mayfield;  
 © 2020 Ethan Watson;  
 © 2020-2024 Fabian Greffrath;  
 © 2020-2024 Roman Fomin;  
 © 2021-2022 Ryan Krafnick;  
 © 2022-2024 Alaux;  
 © 2022-2024 ceski;  
 © 2023 Andrew Apted;  
 © 2023 liPillON;  
 © 2025 Guilherme Miranda.  
License: [GPL-2.0+](https://www.gnu.org/licenses/old-licenses/gpl-2.0.html)

Files: `src/i_flickstick.*, src/i_gyro.*`  
Copyright:  
 © 2018-2021 Julian "Jibb" Smart;  
 © 2021-2024 Nicolas Lessard;  
 © 2024 ceski.  
License: [MIT](https://opensource.org/licenses/MIT)

Files: `src/nano_bsp.*`  
Copyright:  
 © 2023 Andrew Apted.  
License: [MIT](https://opensource.org/licenses/MIT)

Files: `src/m_scanner.*`  
Copyright:  
 © 2015 Braden "Blzut3" Obrzut.  
License: [BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause)

Files: `src/v_flextran.*`  
Copyright:  
 © 2013 James Haley et al.;  
 © 1998-2012 Marisa Heit.  
License: [GPL-3.0+](https://www.gnu.org/licenses/gpl-3.0)

Files: `src/v_video.*`  
Copyright:  
 © 1999 by id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman;  
 © 2013 James Haley et al.  
License: [GPL-3.0+](https://www.gnu.org/licenses/gpl-3.0)

Files: `base/all-all/sprites/pls*, man/simplecpp`  
Copyright:  
 © 2001-2019 Contributors to the Freedoom project.  
License: [BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause)

Files: `base/all-all/dsdg*, base/all-all/sprites/dog*`  
Copyright:  
 © 2017 Nash Muhandes;  
 © apolloaiello;  
 © TobiasKosmos.  
License: [CC-BY-3.0](https://creativecommons.org/licenses/by/3.0/) and [CC0-1.0](https://creativecommons.org/publicdomain/zero/1.0/)

Files: `base/all-all/sbardef.lmp`  
Copyright:  
 © 2024 Ethan Watson.  
License: [CC0-1.0](https://creativecommons.org/publicdomain/zero/1.0/)

Files: `base/all-all/dmxopl.op2`  
Copyright:  
 © 2017 Shannon Freeman.  
License: [MIT](https://github.com/sneakernets/DMXOPL/blob/DMXOPL3/LICENSE)

Files: `base/all-all/sm*.png, data/setup.ico, data/woof-setup.png, data/woof.ico, data/woof.png, setup/setup_icon.c, src/icon.c`  
Copyright:  
 © 2020-2024 Julia Nechaevskaya.  
License: [CC-BY-3.0](https://creativecommons.org/licenses/by/3.0/)

Files: `data/io.github.fabiangreffrath.woof.metainfo.*`  
Copyright:  
 © 2023-2024 Fabian Greffrath.  
License: [CC0-1.0](https://creativecommons.org/publicdomain/zero/1.0/)

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

Files: `netlib/*`  
Copyright:  
 © 1997-2025 Sam Lantinga;  
 © 2012 Simeon Maxein.    
License: [zlib](https://opensource.org/license/zlib)

Files: `third-party/md5/*`  
License: public-domain

Files: `third-party/minimp3/*`  
Copyright:  
 © 2021 lief.      
License: [CC0-1.0](https://creativecommons.org/publicdomain/zero/1.0/)

Files: `third-party/miniz/*`  
Copyright:  
 © 2010-2014 Rich Geldreich and Tenacious Software LLC;  
 © 2013-2014 RAD Game Tools and Valve Software.  
License: [MIT](https://opensource.org/licenses/MIT)

Files: `third-party/pffft/*`  
Copyright:  
 © 2004 The University Corporation for Atmospheric Research ("UCAR");  
 © 2013 Julien Pommier.  
License: [FFTPACK License](https://bitbucket.org/jpommier/pffft/src/master/pffft.h)

Files: `third-party/sha1/*`  
Copyright:  
 © 1998-2001 Free Software Foundation, Inc.;  
 © 2005-2014 Simon Howard.  
License: [GPL-2.0+](https://www.gnu.org/licenses/old-licenses/gpl-2.0.html)

Files: `third-party/spng/*`  
Copyright:  
 © 2018-2023 Randy.  
License: [BSD-2-Clause](https://opensource.org/license/bsd-2-clause)

Files: `third-party/yyjson/*`  
Copyright:  
 © 2020 YaoYuan.  
License: [MIT](https://opensource.org/licenses/MIT)

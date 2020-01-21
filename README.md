# This is Woof!
[![Woof! Icon](https://raw.githubusercontent.com/fabiangreffrath/woof/master/data/woof.png)](https://github.com/fabiangreffrath/woof)

[![Travis Build Status](https://img.shields.io/travis/com/fabiangreffrath/woof.svg?style=flat&logo=travis)](https://travis-ci.com/fabiangreffrath/woof/)

Woof! is a continuation of MBF, the Doom source port created by Lee Killough, for modern systems.

MBF stands for "Marine's Best Friend" and is regarded by many as the successor of the Boom source port by TeamTNT. As the original engine was limited to running under MS-DOS, it has been ported to Windows by Team Eternity under the name [WinMBF](https://github.com/team-eternity/WinMBF). Woof! is based on this porting project but has brought the code further with many bug fixes and enhancements.

## The Name

If you turn the Doom logo upside down it reads "Wood" - which would be a pretty stupid name for a source port. "Woof" is just as stupid a name for a source port, but at least it contains a reference to dogs - and dogs are the Marine's Best Friend. ;-)

## Download

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

## Contact

The canonical homepage for Woof! is https://github.com/fabiangreffrath/woof

Woof! is maintained by [Fabian Greffrath](mailto:fabian@greffXremovethisXrath.com). 

Please report any bugs, glitches or crashes that you encounter to the GitHub [Issue Tracker](https://github.com/fabiangreffrath/woof/issues).

## Legalese

Copyright © 1993-1996 Id Software, Inc.
Copyright © 1993-2008 Raven Software
Copyright © 1999 by id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
Copyright © 2004 James Haley
Copyright © 2005-2014 Simon Howard
Copyright © 2020 Fabian Greffrath

This program is free software; you can redistribute it and/or
modify it under the terms of the [GNU General Public License](https://www.gnu.org/licenses/gpl-2.0.html)
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

SDL 2.0, SDL_mixer 2.0 and SDL_net 2.0 are © 1997-2016 Sam Lantinga and are released under the [zlib license](http://www.gzip.org/zlib/zlib_license.html).

The Woof! icon (as shown at the top of this page) has been kindly contributed by @JNechaevsky.

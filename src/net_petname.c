//
// Copyright(C) 2019 Jonathan Dowland
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//     Generate a randomized, private, memorable name for a Player
//

#include <stdlib.h>
#include <time.h>
#include "doomtype.h"
#include "m_misc2.h"

static const char * const names[] = {
    "Marshall",
    "Chase",
    "Skye",
    "Rocky",
    "Zuma",
    "Everest",
    "Rubble",
    "Tracker",
    "Chickaletta",
    "Westie",
    "Arrby",
    "Ryder",
    "Katie",
    "Wally",
    "Rex",
    "Pluto",
    "Odie",
    "Pongo",
    "Balto",
    "Astro",
    "Spike",
    "Cooper",
    "Gromit",
    "Martha",
};

/*
 * ideally we would export this and the caller would invoke it during
 * their setup routine. But, the two callers only invoke getRandomPetName
 * once, so the initialization might as well occur then.
 */
static void InitPetName()
{
    srand((unsigned int)time(NULL));
}

char *NET_GetRandomPetName()
{
    const char *n;

    InitPetName();
    n = names[rand() % arrlen(names)];

    return M_StringDuplicate(n);
}

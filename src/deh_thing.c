//
// Copyright(C) 2005-2014 Simon Howard
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
//
// Parses "Thing" sections in dehacked files
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "deh_io.h"
#include "doomtype.h"
#include "deh_defs.h"
#include "deh_main.h"
#include "deh_mapping.h"
#include "i_system.h"
#include "info.h"
#include "p_mobj.h"

static const bex_bitflags_t mobj_flags_vanilla[] = {
    {"SPECIAL",      MF_SPECIAL     },
    {"SOLID",        MF_SOLID       },
    {"SHOOTABLE",    MF_SHOOTABLE   },
    {"NOSECTOR",     MF_NOSECTOR    },
    {"NOBLOCKMAP",   MF_NOBLOCKMAP  },
    {"AMBUSH",       MF_AMBUSH      },
    {"JUSTHIT",      MF_JUSTHIT     },
    {"JUSTATTACKED", MF_JUSTATTACKED},
    {"SPAWNCEILING", MF_SPAWNCEILING},
    {"NOGRAVITY",    MF_NOGRAVITY   },
    {"DROPOFF",      MF_DROPOFF     },
    {"PICKUP",       MF_PICKUP      },
    {"NOCLIP",       MF_NOCLIP      },
    {"SLIDE",        MF_SLIDE       },
    {"FLOAT",        MF_FLOAT       },
    {"TELEPORT",     MF_TELEPORT    },
    {"MISSILE",      MF_MISSILE     },
    {"DROPPED",      MF_DROPPED     },
    {"SHADOW",       MF_SHADOW      },
    {"NOBLOOD",      MF_NOBLOOD     },
    {"CORPSE",       MF_CORPSE      },
    {"INFLOAT",      MF_INFLOAT     },
    {"COUNTKILL",    MF_COUNTKILL   },
    {"COUNTITEM",    MF_COUNTITEM   },
    {"SKULLFLY",     MF_SKULLFLY    },
    {"NOTDMATCH",    MF_NOTDMATCH   },
    {"TRANSLATION1", MF_TRANSLATION1},
    {"TRANSLATION2", MF_TRANSLATION2},
    // MBF
    {"TOUCHY",       MF_TOUCHY      },
    {"BOUNCES",      MF_BOUNCES     },
    {"FRIEND",       MF_FRIEND      },
    // Boom
    {"TRANSLUCENT",  MF_TRANSLUCENT },
    // Boom bug compatibility
    {"TRANSLATION",  MF_TRANSLATION1},
    {"UNUSED1",      MF_TRANSLATION2},
    {"UNUSED2",      MF_TOUCHY      },
    {"UNUSED3",      MF_BOUNCES     },
    {"UNUSED4",      MF_FRIEND      },
};

static const bex_bitflags_t mobj_flags_mbf21[] = {
    {"LOGRAV",         MF2_LOGRAV        },
    {"SHORTMRANGE",    MF2_SHORTMRANGE   },
    {"DMGIGNORED",     MF2_DMGIGNORED    },
    {"NORADIUSDMG",    MF2_NORADIUSDMG   },
    {"FORCERADIUSDMG", MF2_FORCERADIUSDMG},
    {"HIGHERMPROB",    MF2_HIGHERMPROB   },
    {"RANGEHALF",      MF2_RANGEHALF     },
    {"NOTHRESHOLD",    MF2_NOTHRESHOLD   },
    {"LONGMELEE",      MF2_LONGMELEE     },
    {"BOSS",           MF2_BOSS          },
    {"MAP07BOSS1",     MF2_MAP07BOSS1    },
    {"MAP07BOSS2",     MF2_MAP07BOSS2    },
    {"E1M8BOSS",       MF2_E1M8BOSS      },
    {"E2M8BOSS",       MF2_E2M8BOSS      },
    {"E3M8BOSS",       MF2_E3M8BOSS      },
    {"E4M6BOSS",       MF2_E4M6BOSS      },
    {"E4M8BOSS",       MF2_E4M8BOSS      },
    {"RIP",            MF2_RIP           },
    {"FULLVOLSOUNDS",  MF2_FULLVOLSOUNDS },
};

static const bex_bitflags_t mobj_flags_woof[] = {
    {"MIRROREDCORPSE", MFX_MIRROREDCORPSE}
};

DEH_BEGIN_MAPPING(thing_mapping, mobjinfo_t)
    DEH_MAPPING("ID #", doomednum)
    DEH_MAPPING("Initial frame", spawnstate)
    DEH_MAPPING("Hit points", spawnhealth)
    DEH_MAPPING("First moving frame", seestate)
    DEH_MAPPING("Alert sound", seesound)
    DEH_MAPPING("Reaction time", reactiontime)
    DEH_MAPPING("Attack sound", attacksound)
    DEH_MAPPING("Injury frame", painstate)
    DEH_MAPPING("Pain chance", painchance)
    DEH_MAPPING("Pain sound", painsound)
    DEH_MAPPING("Close attack frame", meleestate)
    DEH_MAPPING("Far attack frame", missilestate)
    DEH_MAPPING("Death frame", deathstate)
    DEH_MAPPING("Exploding frame", xdeathstate)
    DEH_MAPPING("Death sound", deathsound)
    DEH_MAPPING("Speed", speed)
    DEH_MAPPING("Width", radius)
    DEH_MAPPING("Height", height)
    DEH_MAPPING("Mass", mass)
    DEH_MAPPING("Missile damage", damage)
    DEH_MAPPING("Action sound", activesound)
    DEH_MAPPING("Bits", flags)
    DEH_MAPPING("Respawn frame", raisestate)
    // dehextra
    DEH_MAPPING("Dropped item", droppeditem)
    // mbf21
    DEH_MAPPING("MBF21 Bits", flags2)
    DEH_MAPPING("Infighting group", infighting_group)
    DEH_MAPPING("Projectile group", projectile_group)
    DEH_MAPPING("Splash group", splash_group)
    DEH_MAPPING("Fast speed", altspeed)
    DEH_MAPPING("Melee range", meleerange)
    DEH_MAPPING("Rip sound", ripsound)
    // id24
    DEH_UNSUPPORTED_MAPPING("ID24 Bits")
    DEH_UNSUPPORTED_MAPPING("Min respawn tics")
    DEH_UNSUPPORTED_MAPPING("Respawn dice")
    DEH_UNSUPPORTED_MAPPING("Pickup ammo type")
    DEH_UNSUPPORTED_MAPPING("Pickup ammo category")
    DEH_UNSUPPORTED_MAPPING("Pickup weapon type")
    DEH_UNSUPPORTED_MAPPING("Pickup item type")
    DEH_UNSUPPORTED_MAPPING("Pickup bonus count")
    DEH_UNSUPPORTED_MAPPING("Pickup sound")
    DEH_UNSUPPORTED_MAPPING("Pickup message")
    DEH_UNSUPPORTED_MAPPING("Translation")
    // mbf2y
    DEH_UNSUPPORTED_MAPPING("MBF2y Bits")
    DEH_UNSUPPORTED_MAPPING("Projectile collision group") // i.b mbf21
    DEH_UNSUPPORTED_MAPPING("Pickup health amount")       // i.b id24
    DEH_UNSUPPORTED_MAPPING("Pickup armor amount")        // i.b id24
    DEH_UNSUPPORTED_MAPPING("Pickup powerup duration")    // i.b id24
    DEH_MAPPING("Obituary", obituary)                     // p.f ZDoom
    DEH_MAPPING("Melee obituary", obituary_melee)         // p.f ZDoom
    DEH_MAPPING("Self obituary", obituary_self)           // p.f ZDoom
    DEH_UNSUPPORTED_MAPPING("Gib Health")                 // p.f Retro
    DEH_UNSUPPORTED_MAPPING("Blood Thing")                // i.b Eternity
    DEH_UNSUPPORTED_MAPPING("Crush State")                // i.b Eternity
    DEH_UNSUPPORTED_MAPPING("Melee threshold")            // p.f crispy
    DEH_UNSUPPORTED_MAPPING("Max target range")           // p.f crispy
    DEH_UNSUPPORTED_MAPPING("Min missile chance")         // p.f crispy
    DEH_UNSUPPORTED_MAPPING("Missile chance multiplier")  // p.f crispy
    // eternity
    DEH_MAPPING("Blood Color", bloodcolor)
DEH_END_MAPPING

//
// Notable, unsupported properties:
//
// From Retro:
// * "Retro Bits"
// * "Pickup Width"
// * "Fullbright"
// * "Shadow Offset"
// * "Name", "Name1", "Name2", "Name3", "Plural", "Plural1", "Plural2", "Plura3"
//
// From ZDoom:
// * "Tag"
// * "No Ice Death"
// * "Translucency"
// * "Render Style"
// * "Alpha"
// * "Scale"
// * "Decal"
// * "Physical height"
// * "Projectile pass height"
//

// [crispy] initialize Thing extra properties (keeping vanilla props in info.c)
static void DEH_InitThingProperties(void)
{
    // TODO: rope in the dsdhacked code
    mobjinfo[MT_POSSESSED].droppeditem = MT_CLIP;
    mobjinfo[MT_WOLFSS].droppeditem = MT_CLIP;
    mobjinfo[MT_SHOTGUY].droppeditem = MT_SHOTGUN;
    mobjinfo[MT_CHAINGUY].droppeditem = MT_CHAINGUN;
}

static void *DEH_ThingStart(deh_context_t *context, char *line)
{
    int thing_number = 0;

    if (sscanf(line, "Thing %i", &thing_number) != 1)
    {
        DEH_Warning(context, "Parse error on section start");
        return NULL;
    }

    // dehacked files are indexed from 1
    --thing_number;

    if (thing_number < 0 || thing_number >= NUMMOBJTYPES)
    {
        DEH_Warning(context, "Invalid thing number: %i", thing_number);
        return NULL;
    }

    mobjinfo_t *mobj = &mobjinfo[thing_number];

    return mobj;
}

static void DEH_ThingParseLine(deh_context_t *context, char *line, void *tag)
{
    if (tag == NULL)
    {
        return;
    }

    mobjinfo_t *mobj = (mobjinfo_t *)tag;

    // Parse the assignment
    char *variable_name, *value;
    if (!DEH_ParseAssignment(line, &variable_name, &value))
    {
        DEH_Warning(context, "Failed to parse assignment");
        return;
    }

    //    printf("Set %s to %s for mobj\n", variable_name, value);

    // most values are integers
    int ivalue = atoi(value);

    if (!strcasecmp(variable_name, "Bits"))
    {
        ivalue = DEH_BexParseBitFlags(ivalue, value, mobj_flags_vanilla, arrlen(mobj_flags_vanilla));

        if ((ivalue & (MF_NOBLOCKMAP | MF_MISSILE)) == MF_MISSILE)
        {
            DEH_Warning(context, "Thing %ld has MF_MISSILE without MF_NOBLOCKMAP", (long)(mobj - mobjinfo) + 1);
        }
    }
    else if (!strcasecmp(variable_name, "MBF21 Bits"))
    {
        ivalue = DEH_BexParseBitFlags(ivalue, value, mobj_flags_mbf21, arrlen(mobj_flags_mbf21));
    }
    else if (!strcasecmp(variable_name, "Woof Bits"))
    {
        ivalue = DEH_BexParseBitFlags(ivalue, value, mobj_flags_woof, arrlen(mobj_flags_woof));
    }
    else if (!strcasecmp(variable_name, "Dropped Item"))
    {
        if (ivalue < 0)
        {
            I_Error("Dropped item must be >= 0 (check your dehacked)");
        }
        ivalue += MT_NULL; // DeHackEd is off-by-one
    }
    else if (!strcasecmp(variable_name, "Infighting group"))
    {
        if (ivalue < 0)
        {
            I_Error("Infighting groups must be >= 0 (check your dehacked)");
        }
        ivalue += IG_END;
    }
    else if (!strcasecmp(variable_name, "Splash group"))
    {
        if (ivalue < 0)
        {
            I_Error("Splash groups must be >= 0 (check your dehacked)");
        }
        ivalue += SG_END;
    }
    else if (!strcasecmp(variable_name, "Projectile group"))
    {
        if (ivalue >= PG_DEFAULT)
        {
            ivalue += PG_END;
        }
        else
        {
            ivalue = PG_GROUPLESS;
        }
    }
    else if (!strcasecmp(variable_name, "Obituray")
            || !strcasecmp(variable_name, "Melee Obituray")
            || !strcasecmp(variable_name, "Self Obituray"))
    {
        if (strlen(value) >= 1)
        {
            DEH_SetStringMapping(context, &thing_mapping, mobj, variable_name, value);
        }
        else
        {
            DEH_Warning(context, "%s is empty!", variable_name);
        }
        return;
    }

    // Set the field value
    DEH_SetMapping(context, &thing_mapping, mobj, variable_name, ivalue);
}

static void DEH_ThingSHA1Sum(sha1_context_t *context)
{
    for (int i = 0; i < NUMMOBJTYPES; ++i)
    {
        DEH_StructSHA1Sum(context, &thing_mapping, &mobjinfo[i]);
    }
}

deh_section_t deh_section_thing =
{
    "Thing",
    DEH_InitThingProperties, // [crispy] initialize Thing extra properties
    DEH_ThingStart,
    DEH_ThingParseLine,
    NULL,
    DEH_ThingSHA1Sum,
};

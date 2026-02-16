//
//  Copyright (c) 2015, Braden "Blzut3" Obrzut
//  Copyright (c) 2026, Roman Fomin
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the
//       distribution.
//     * The names of its contributors may be used to endorse or promote
//       products derived from this software without specific prior written
//       permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.

#include "decl_defs.h"
#include "decl_misc.h"
#include "dsdh_main.h"
#include "doomtype.h"
#include "info.h"
#include "m_array.h"
#include "m_fixed.h"
#include "m_misc.h"
#include "m_scanner.h"
#include "p_mobj.h"
#include "sounds.h"

typedef struct
{
    const char *name;
    uint32_t flag;
} flag_t;

static flag_t decl_flags[] = {
    {"SPECIAL",        MF_SPECIAL     },
    {"SOLID",          MF_SOLID       },
    {"SHOOTABLE",      MF_SHOOTABLE   },
    {"NOSECTOR",       MF_NOSECTOR    },
    {"NOBLOCKMAP",     MF_NOBLOCKMAP  },
    {"AMBUSH",         MF_AMBUSH      },
    {"JUSTHIT",        MF_JUSTHIT     },
    {"JUSTATTACKED",   MF_JUSTATTACKED},
    {"SPAWNCEILING",   MF_SPAWNCEILING},
    {"NOGRAVITY",      MF_NOGRAVITY   },
    {"DROPOFF",        MF_DROPOFF     },
    {"PICKUP",         MF_PICKUP      },
    {"NOCLIP",         MF_NOCLIP      },
    {"SLIDE",          MF_SLIDE       },
    {"FLOAT",          MF_FLOAT       },
    {"TELEPORT",       MF_TELEPORT    },
    {"MISSILE",        MF_MISSILE     },
    {"DROPPED",        MF_DROPPED     },
    {"SHADOW",         MF_SHADOW      },
    {"NOBLOOD",        MF_NOBLOOD     },
    {"CORPSE",         MF_CORPSE      },
    {"INFLOAT",        MF_INFLOAT     },
    {"COUNTKILL",      MF_COUNTKILL   },
    {"COUNTITEM",      MF_COUNTITEM   },
    {"SKULLFLY",       MF_SKULLFLY    },
    {"NOTDMATCH",      MF_NOTDMATCH   },
    {"TRANSLATION1",   MF_TRANSLATION1},
    {"TRANSLATION2",   MF_TRANSLATION2},
    // MBF
    {"TOUCHY",         MF_TOUCHY      },
    {"BOUNCES",        MF_BOUNCES     },
    {"FRIEND",         MF_FRIEND      },
    // Boom
    {"TRANSLUCENT",    MF_TRANSLUCENT }
};

static flag_t decl_flags_mbf21[]  = {
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

static uint32_t GetFlagByName(const char *name, flag_t *flags, size_t count)
{
    for (int i = 0; i < count; ++i)
    {
        if (!strcasecmp(name, flags[i].name))
        {
            return flags[i].flag;
        }
    }
    return 0;
}

void DECL_ParseArgFlag(scanner_t *sc, arg_t *arg)
{
    SC_MustGetToken(sc, TK_Identifier);
    const char *name = SC_GetString(sc);

    uint32_t flag = GetFlagByName(name, decl_flags, arrlen(decl_flags));
    if (flag)
    {
        arg->value |= flag;
    }
    else
    {
        flag = GetFlagByName(name, decl_flags_mbf21, arrlen(decl_flags_mbf21));
        if (flag)
        {
            arg->data.integer |= flag;
        }
        else
        {
            SC_Error(sc, "Unknown flag '%s'.", name);
        }
    }
}

void DECL_ParseActorFlag(scanner_t *sc, proplist_t *proplist, boolean set)
{
    SC_MustGetToken(sc, TK_Identifier);
    const char *name = SC_GetString(sc);

    uint32_t flag = GetFlagByName(name, decl_flags, arrlen(decl_flags));
    if (flag)
    {
        if (set)
        {
            proplist->flags |= flag;
        }
        else
        {
            proplist->flags &= ~flag;
        }
    }
    else
    {
        flag = GetFlagByName(name, decl_flags_mbf21, arrlen(decl_flags_mbf21));
        if (flag)
        {
            if (set)
            {
                proplist->flags2 |= flag;
            }
            else
            {
                proplist->flags2 &= ~flag;
            }
        }
        else
        {
            SC_Error(sc, "Unknown flag '%s'.", name);
        }
    }
}

typedef enum
{
    TYPE_None = -1,
    TYPE_Int,
    TYPE_Fixed,
    TYPE_Sound,
    TYPE_Item,
    TYPE_String,
    TYPE_Group,
} valtype_t;

static struct
{
    valtype_t valtype;
    char *name;
} decl_properties[] = {
    [prop_health] =       {TYPE_Int,   "Health"      },
    [prop_seesound] =     {TYPE_Sound, "SeeSound"    },
    [prop_reactiontime] = {TYPE_Int,   "ReactionTime"},
    [prop_attacksound] =  {TYPE_Sound, "AttackSound" },
    [prop_painchance] =   {TYPE_Int,   "PainChance"  },
    [prop_painsound] =    {TYPE_Sound, "PainSound"   },
    [prop_deathsound] =   {TYPE_Sound, "DeathSound"  },
    [prop_speed] =        {TYPE_Int,   "Speed"       },
    [prop_radius] =       {TYPE_Fixed, "Radius"      },
    [prop_height] =       {TYPE_Fixed, "Height"      },
    [prop_mass] =         {TYPE_Int,   "Mass"        },
    [prop_damage] =       {TYPE_Int,   "Damage"      },
    [prop_activesound] =  {TYPE_Sound, "ActiveSound" },

    // MBF21
    [prop_dropitem] =         {TYPE_Item,  "DropItem"},
    [prop_infighting_group] = {TYPE_Group, "InfightingGroup"},
    [prop_projectile_group] = {TYPE_Group, "ProjectileGroup"},
    [prop_splash_group] =     {TYPE_Group, "SplashGroup"},
    [prop_ripsound] =         {TYPE_Sound, "RipSound"},
    [prop_altspeed] =         {TYPE_Int,   "AltSpeed"},
    [prop_meleerange] =       {TYPE_Fixed, "MeleeRange"},

    // Declarate
    [prop_renderstyle] =    {TYPE_None,  "RenderStyle"},
    [prop_translation] =    {TYPE_None,  "Translation"},
    [prop_obituary] =       {TYPE_String, "Obituary"},
    [prop_obituary_melee] = {TYPE_String, "HitObituary"},
    [prop_obituary_self] =  {TYPE_String, "SelfObituary"}
};

static struct
{
    mobjtype_t type;
    const char *name;
} decl_items[] = {
// TODO
    {MT_CLIP, "clip"},
    {MT_SHOTGUN, "shotgun"},
    {MT_CHAINGUN, "chaingun"},
    {MT_MISC22, "shell"},
    {MT_ROCKET, "rocket"},
    {MT_MISC20, "cell"}
};

int DECL_SoundMapping(scanner_t *sc)
{
    SC_MustGetToken(sc, TK_StringConst);
    const char *name = SC_GetString(sc);

    for (int i = 0; i < num_sfx; ++i)
    {
        if (S_sfx[i].name && !strcasecmp(name, S_sfx[i].name))
        {
            return i;
        }
    }

    int sfx_number = DSDH_SoundsGetNewIndex();
    S_sfx[sfx_number].name = M_StringDuplicate(name);
    return sfx_number;
}

static int ItemMapping(scanner_t *sc)
{
    SC_MustGetToken(sc, TK_StringConst);
    const char *name = SC_GetString(sc);

    for (int i = 0; i < arrlen(decl_items); ++i)
    {
        if (!strcasecmp(name, decl_items[i].name))
        {
            return decl_items[i].type;
        }
    }
    return MT_NULL;
}

static int GroupMapping(scanner_t *sc, proptype_t type)
{
    SC_MustGetToken(sc, TK_IntConst);
    int ivalue = SC_GetNumber(sc);
    switch (type)
    {
        case prop_infighting_group:
            if (ivalue < 0)
            {
                SC_Error(sc, "Infighting groups must be >= 0.");
            }
            ivalue += IG_END;
            break;
        case prop_projectile_group:
            if (ivalue >= PG_DEFAULT)
            {
                ivalue += PG_END;
            }
            else
            {
                ivalue = PG_GROUPLESS;
            }
            break;
        case prop_splash_group:
            if (ivalue < 0)
            {
                SC_Error(sc, "Splash groups must be >= 0.");
            }
            ivalue += SG_END;
            break;
        default:
            break;
    }
    return ivalue;
}

void DECL_ParseActorProperty(scanner_t *sc, proplist_t *proplist)
{
    const char *name = SC_GetString(sc);
    proptype_t type;
    for (type = 0; type < prop_number; ++type)
    {
        if (!strcasecmp(name, decl_properties[type].name))
        {
            break;
        }
    }
    if (type == prop_number)
    {
        SC_Error(sc, "Unknown property '%s'.", name);
        return;
    }

    switch (type)
    {
        case prop_renderstyle:
            {
                SC_MustGetToken(sc, TK_StringConst);
                switch (DECL_CheckKeyword(sc, "none", "fuzzy"))
                {
                    case 0:
                        proplist->flags &= ~MF_SHADOW;
                        break;
                    case 1:
                        proplist->flags |= MF_SHADOW;
                        break;
                    default:
                        SC_Error(sc, "Unknown render style '%s'.",
                                 SC_GetString(sc));
                        break;
                }
            }
            break;
        case prop_translation:
            {
                SC_MustGetToken(sc, TK_IntConst);
                int number = SC_GetNumber(sc);
                if (number > 3)
                {
                    SC_Error(sc, "Translation out of range.");
                }
                proplist->flags =
                    (proplist->flags & (~(MF_TRANSLATION1 | MF_TRANSLATION2)))
                    | (number * MF_TRANSLATION1);
            }
            break;
        default:
            {
                propvalue_t value = {0};
                switch (decl_properties[type].valtype)
                {
                    case TYPE_Int:
                        value.number = DECL_GetNegativeInteger(sc);
                        break;
                    case TYPE_Fixed:
                        value.number = DoubleToFixed(DECL_GetNegativeDecimal(sc));
                        break;
                    case TYPE_Sound:
                        value.number = DECL_SoundMapping(sc);
                        break;
                    case TYPE_String:
                        SC_MustGetToken(sc, TK_StringConst);
                        value.string = M_StringDuplicate(SC_GetString(sc));
                        break;
                    case TYPE_Item:
                        value.number = ItemMapping(sc);
                        break;
                    case TYPE_Group:
                        value.number = GroupMapping(sc, type);
                        break;
                    default:
                        break;
                }

                property_t *prop;
                array_foreach(prop, proplist->props)
                {
                    if (prop->type == type)
                    {
                        prop->value = value;
                        break;
                    }
                }
                if (prop == array_end(proplist->props))
                {
                    property_t new_prop = {.type = type, .value = value};
                    array_push(proplist->props, new_prop);
                }
            }
            break;
    }
}

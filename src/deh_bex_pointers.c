//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2014 Fabian Greffrath
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
// Parses [CODEPTR] sections in BEX files
//

#include <stdio.h>
#include <string.h>

#include "deh_io.h"
#include "deh_main.h"
#include "info.h"
#include "p_action.h"

typedef struct
{
    const char *mnemonic;
    const actionf_t pointer;
} bex_codepointer_t;

static const bex_codepointer_t bex_pointer_table[] =
{
    {"Light0",              {.p2 = A_Light0}             },
    {"WeaponReady",         {.p2 = A_WeaponReady}        },
    {"Lower",               {.p2 = A_Lower}              },
    {"Raise",               {.p2 = A_Raise}              },
    {"Punch",               {.p2 = A_Punch}              },
    {"ReFire",              {.p2 = A_ReFire}             },
    {"FirePistol",          {.p2 = A_FirePistol}         },
    {"Light1",              {.p2 = A_Light1}             },
    {"FireShotgun",         {.p2 = A_FireShotgun}        },
    {"Light2",              {.p2 = A_Light2}             },
    {"FireShotgun2",        {.p2 = A_FireShotgun2}       },
    {"CheckReload",         {.p2 = A_CheckReload}        },
    {"OpenShotgun2",        {.p2 = A_OpenShotgun2}       },
    {"LoadShotgun2",        {.p2 = A_LoadShotgun2}       },
    {"CloseShotgun2",       {.p2 = A_CloseShotgun2}      },
    {"FireCGun",            {.p2 = A_FireCGun}           },
    {"GunFlash",            {.p2 = A_GunFlash}           },
    {"FireMissile",         {.p2 = A_FireMissile}        },
    {"Saw",                 {.p2 = A_Saw}                },
    {"FirePlasma",          {.p2 = A_FirePlasma}         },
    {"BFGsound",            {.p2 = A_BFGsound}           },
    {"FireBFG",             {.p2 = A_FireBFG}            },
    {"BFGSpray",            {.p1 = A_BFGSpray}           },
    {"Explode",             {.p1 = A_Explode}            },
    {"Pain",                {.p1 = A_Pain}               },
    {"PlayerScream",        {.p1 = A_PlayerScream}       },
    {"Fall",                {.p1 = A_Fall}               },
    {"XScream",             {.p1 = A_XScream}            },
    {"Look",                {.p1 = A_Look}               },
    {"Chase",               {.p1 = A_Chase}              },
    {"FaceTarget",          {.p1 = A_FaceTarget}         },
    {"PosAttack",           {.p1 = A_PosAttack}          },
    {"Scream",              {.p1 = A_Scream}             },
    {"SPosAttack",          {.p1 = A_SPosAttack}         },
    {"VileChase",           {.p1 = A_VileChase}          },
    {"VileStart",           {.p1 = A_VileStart}          },
    {"VileTarget",          {.p1 = A_VileTarget}         },
    {"VileAttack",          {.p1 = A_VileAttack}         },
    {"StartFire",           {.p1 = A_StartFire}          },
    {"Fire",                {.p1 = A_Fire}               },
    {"FireCrackle",         {.p1 = A_FireCrackle}        },
    {"Tracer",              {.p1 = A_Tracer}             },
    {"SkelWhoosh",          {.p1 = A_SkelWhoosh}         },
    {"SkelFist",            {.p1 = A_SkelFist}           },
    {"SkelMissile",         {.p1 = A_SkelMissile}        },
    {"FatRaise",            {.p1 = A_FatRaise}           },
    {"FatAttack1",          {.p1 = A_FatAttack1}         },
    {"FatAttack2",          {.p1 = A_FatAttack2}         },
    {"FatAttack3",          {.p1 = A_FatAttack3}         },
    {"BossDeath",           {.p1 = A_BossDeath}          },
    {"CPosAttack",          {.p1 = A_CPosAttack}         },
    {"CPosRefire",          {.p1 = A_CPosRefire}         },
    {"TroopAttack",         {.p1 = A_TroopAttack}        },
    {"SargAttack",          {.p1 = A_SargAttack}         },
    {"HeadAttack",          {.p1 = A_HeadAttack}         },
    {"BruisAttack",         {.p1 = A_BruisAttack}        },
    {"SkullAttack",         {.p1 = A_SkullAttack}        },
    {"Metal",               {.p1 = A_Metal}              },
    {"SpidRefire",          {.p1 = A_SpidRefire}         },
    {"BabyMetal",           {.p1 = A_BabyMetal}          },
    {"BspiAttack",          {.p1 = A_BspiAttack}         },
    {"Hoof",                {.p1 = A_Hoof}               },
    {"CyberAttack",         {.p1 = A_CyberAttack}        },
    {"PainAttack",          {.p1 = A_PainAttack}         },
    {"PainDie",             {.p1 = A_PainDie}            },
    {"KeenDie",             {.p1 = A_KeenDie}            },
    {"BrainPain",           {.p1 = A_BrainPain}          },
    {"BrainScream",         {.p1 = A_BrainScream}        },
    {"BrainDie",            {.p1 = A_BrainDie}           },
    {"BrainAwake",          {.p1 = A_BrainAwake}         },
    {"BrainSpit",           {.p1 = A_BrainSpit}          },
    {"SpawnSound",          {.p1 = A_SpawnSound}         },
    {"SpawnFly",            {.p1 = A_SpawnFly}           },
    {"BrainExplode",        {.p1 = A_BrainExplode}       },
    // MBF
    {"Detonate",            {.p1 = A_Detonate}           },
    {"Mushroom",            {.p1 = A_Mushroom}           },
    {"Die",                 {.p1 = A_Die}                },
    {"Spawn",               {.p1 = A_Spawn}              },
    {"Turn",                {.p1 = A_Turn}               },
    {"Face",                {.p1 = A_Face}               },
    {"Scratch",             {.p1 = A_Scratch}            },
    {"PlaySound",           {.p1 = A_PlaySound}          },
    {"RandomJump",          {.p1 = A_RandomJump}         },
    {"LineEffect",          {.p1 = A_LineEffect}         },
    {"FireOldBFG",          {.p2 = A_FireOldBFG}         },
    {"BetaSkullAttack",     {.p1 = A_BetaSkullAttack}    },
    {"Stop",                {.p1 = A_Stop}               },
    // MBF21
    {"SpawnObject",         {.p1 = A_SpawnObject}        },
    {"MonsterProjectile",   {.p1 = A_MonsterProjectile}  },
    {"MonsterBulletAttack", {.p1 = A_MonsterBulletAttack}},
    {"MonsterMeleeAttack",  {.p1 = A_MonsterMeleeAttack} },
    {"RadiusDamage",        {.p1 = A_RadiusDamage}       },
    {"NoiseAlert",          {.p1 = A_NoiseAlert}         },
    {"HealChase",           {.p1 = A_HealChase}          },
    {"SeekTracer",          {.p1 = A_SeekTracer}         },
    {"FindTracer",          {.p1 = A_FindTracer}         },
    {"ClearTracer",         {.p1 = A_ClearTracer}        },
    {"JumpIfHealthBelow",   {.p1 = A_JumpIfHealthBelow}  },
    {"JumpIfTargetInSight", {.p1 = A_JumpIfTargetInSight}},
    {"JumpIfTargetCloser",  {.p1 = A_JumpIfTargetCloser} },
    {"JumpIfTracerInSight", {.p1 = A_JumpIfTracerInSight}},
    {"JumpIfTracerCloser",  {.p1 = A_JumpIfTracerCloser} },
    {"JumpIfFlagsSet",      {.p1 = A_JumpIfFlagsSet}     },
    {"AddFlags",            {.p1 = A_AddFlags}           },
    {"RemoveFlags",         {.p1 = A_RemoveFlags}        },
    {"WeaponProjectile",    {.p2 = A_WeaponProjectile}   },
    {"WeaponBulletAttack",  {.p2 = A_WeaponBulletAttack} },
    {"WeaponMeleeAttack",   {.p2 = A_WeaponMeleeAttack}  },
    {"WeaponSound",         {.p2 = A_WeaponSound}        },
    {"WeaponAlert",         {.p2 = A_WeaponAlert}        },
    {"WeaponJump",          {.p2 = A_WeaponJump}         },
    {"ConsumeAmmo",         {.p2 = A_ConsumeAmmo}        },
    {"CheckAmmo",           {.p2 = A_CheckAmmo}          },
    {"RefireTo",            {.p2 = A_RefireTo}           },
    {"GunFlashTo",          {.p2 = A_GunFlashTo}         },
    {"NULL",                {NULL}                       },
};

extern actionf_t codeptrs[NUMSTATES];

static void *DEH_BEXPtrStart(deh_context_t *context, char *line)
{
    char s[10];

    if (sscanf(line, "%9s", s) == 0 || strcmp("[CODEPTR]", s))
    {
        DEH_Warning(context, "Parse error on section start");
    }

    return NULL;
}

static void DEH_BEXPtrParseLine(deh_context_t *context, char *line, void *tag)
{
    // parse "FRAME nn = mnemonic", where
    // variable_name = "FRAME nn" and value = "mnemonic"
    char *variable_name, *value;
    if (!DEH_ParseAssignment(line, &variable_name, &value))
    {
        DEH_Warning(context, "Failed to parse assignment: %s", line);
        return;
    }

    // parse "FRAME nn", where frame_number = "nn"
    int frame_number;
    char frame_str[6];
    if (sscanf(variable_name, "%5s %32d", frame_str, &frame_number) != 2 || strcasecmp(frame_str, "FRAME"))
    {
        DEH_Warning(context, "Failed to parse assignment: %s", variable_name);
        return;
    }

    if (frame_number < 0 || frame_number >= NUMSTATES)
    {
        DEH_Warning(context, "Invalid frame number: %i", frame_number);
        return;
    }

    state_t *state = (state_t *)&states[frame_number];

    for (int i = 0; i < arrlen(bex_pointer_table); i++)
    {
        if (!strcasecmp(bex_pointer_table[i].mnemonic, value))
        {
            state->action = bex_pointer_table[i].pointer;
            return;
        }
    }

    DEH_Warning(context, "Invalid mnemonic '%s'", value);
}

deh_section_t deh_section_bex_codepointers =
{
    "[CODEPTR]",
    NULL,
    DEH_BEXPtrStart,
    DEH_BEXPtrParseLine,
    NULL,
    NULL,
};

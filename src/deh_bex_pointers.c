//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2014 Fabian Greffrath
// Copyright(C) 2021 Roman Fomin
// Copyright(C) 2025 Guilherme Miranda
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
#include "deh_mapping.h"
#include "doomtype.h"
#include "i_system.h"
#include "info.h"
#include "m_fixed.h"
#include "p_action.h"

typedef struct
{
    const char *mnemonic;
    const actionf_t pointer;
    const int argcount;           // [XA] number of mbf21 args this action uses, if any
    const int args[MAXSTATEARGS]; // default values for mbf21 args
} bex_codepointer_t;

const bex_codepointer_t bex_pointer_null = {"(NULL)", {NULL}};

static const bex_codepointer_t bex_pointer_table[] =
{
    {"Light0",              {.p2 = A_Light0}                 },
    {"WeaponReady",         {.p2 = A_WeaponReady}            },
    {"Lower",               {.p2 = A_Lower}                  },
    {"Raise",               {.p2 = A_Raise}                  },
    {"Punch",               {.p2 = A_Punch}                  },
    {"ReFire",              {.p2 = A_ReFire}                 },
    {"FirePistol",          {.p2 = A_FirePistol}             },
    {"Light1",              {.p2 = A_Light1}                 },
    {"FireShotgun",         {.p2 = A_FireShotgun}            },
    {"Light2",              {.p2 = A_Light2}                 },
    {"FireShotgun2",        {.p2 = A_FireShotgun2}           },
    {"CheckReload",         {.p2 = A_CheckReload}            },
    {"OpenShotgun2",        {.p2 = A_OpenShotgun2}           },
    {"LoadShotgun2",        {.p2 = A_LoadShotgun2}           },
    {"CloseShotgun2",       {.p2 = A_CloseShotgun2}          },
    {"FireCGun",            {.p2 = A_FireCGun}               },
    {"GunFlash",            {.p2 = A_GunFlash}               },
    {"FireMissile",         {.p2 = A_FireMissile}            },
    {"Saw",                 {.p2 = A_Saw}                    },
    {"FirePlasma",          {.p2 = A_FirePlasma}             },
    {"BFGsound",            {.p2 = A_BFGsound}               },
    {"FireBFG",             {.p2 = A_FireBFG}                },
    {"BFGSpray",            {.p1 = A_BFGSpray}               },
    {"Explode",             {.p1 = A_Explode}                },
    {"Pain",                {.p1 = A_Pain}                   },
    {"PlayerScream",        {.p1 = A_PlayerScream}           },
    {"Fall",                {.p1 = A_Fall}                   },
    {"XScream",             {.p1 = A_XScream}                },
    {"Look",                {.p1 = A_Look}                   },
    {"Chase",               {.p1 = A_Chase}                  },
    {"FaceTarget",          {.p1 = A_FaceTarget}             },
    {"PosAttack",           {.p1 = A_PosAttack}              },
    {"Scream",              {.p1 = A_Scream}                 },
    {"SPosAttack",          {.p1 = A_SPosAttack}             },
    {"VileChase",           {.p1 = A_VileChase}              },
    {"VileStart",           {.p1 = A_VileStart}              },
    {"VileTarget",          {.p1 = A_VileTarget}             },
    {"VileAttack",          {.p1 = A_VileAttack}             },
    {"StartFire",           {.p1 = A_StartFire}              },
    {"Fire",                {.p1 = A_Fire}                   },
    {"FireCrackle",         {.p1 = A_FireCrackle}            },
    {"Tracer",              {.p1 = A_Tracer}                 },
    {"SkelWhoosh",          {.p1 = A_SkelWhoosh}             },
    {"SkelFist",            {.p1 = A_SkelFist}               },
    {"SkelMissile",         {.p1 = A_SkelMissile}            },
    {"FatRaise",            {.p1 = A_FatRaise}               },
    {"FatAttack1",          {.p1 = A_FatAttack1}             },
    {"FatAttack2",          {.p1 = A_FatAttack2}             },
    {"FatAttack3",          {.p1 = A_FatAttack3}             },
    {"BossDeath",           {.p1 = A_BossDeath}              },
    {"CPosAttack",          {.p1 = A_CPosAttack}             },
    {"CPosRefire",          {.p1 = A_CPosRefire}             },
    {"TroopAttack",         {.p1 = A_TroopAttack}            },
    {"SargAttack",          {.p1 = A_SargAttack}             },
    {"HeadAttack",          {.p1 = A_HeadAttack}             },
    {"BruisAttack",         {.p1 = A_BruisAttack}            },
    {"SkullAttack",         {.p1 = A_SkullAttack}            },
    {"Metal",               {.p1 = A_Metal}                  },
    {"SpidRefire",          {.p1 = A_SpidRefire}             },
    {"BabyMetal",           {.p1 = A_BabyMetal}              },
    {"BspiAttack",          {.p1 = A_BspiAttack}             },
    {"Hoof",                {.p1 = A_Hoof}                   },
    {"CyberAttack",         {.p1 = A_CyberAttack}            },
    {"PainAttack",          {.p1 = A_PainAttack}             },
    {"PainDie",             {.p1 = A_PainDie}                },
    {"KeenDie",             {.p1 = A_KeenDie}                },
    {"BrainPain",           {.p1 = A_BrainPain}              },
    {"BrainScream",         {.p1 = A_BrainScream}            },
    {"BrainDie",            {.p1 = A_BrainDie}               },
    {"BrainAwake",          {.p1 = A_BrainAwake}             },
    {"BrainSpit",           {.p1 = A_BrainSpit}              },
    {"SpawnSound",          {.p1 = A_SpawnSound}             },
    {"SpawnFly",            {.p1 = A_SpawnFly}               },
    {"BrainExplode",        {.p1 = A_BrainExplode}           },
    // MBF
    {"Detonate",            {.p1 = A_Detonate}               },
    {"Mushroom",            {.p1 = A_Mushroom}               },
    {"Die",                 {.p1 = A_Die}                    },
    {"Spawn",               {.p1 = A_Spawn}                  },
    {"Turn",                {.p1 = A_Turn}                   },
    {"Face",                {.p1 = A_Face}                   },
    {"Scratch",             {.p1 = A_Scratch}                },
    {"PlaySound",           {.p1 = A_PlaySound}              },
    {"RandomJump",          {.p1 = A_RandomJump}             },
    {"LineEffect",          {.p1 = A_LineEffect}             },
    {"FireOldBFG",          {.p2 = A_FireOldBFG}             },
    {"BetaSkullAttack",     {.p1 = A_BetaSkullAttack}        },
    {"Stop",                {.p1 = A_Stop}                   },
    // MBF21
    {"SpawnObject",         {.p1 = A_SpawnObject},         8 },
    {"MonsterProjectile",   {.p1 = A_MonsterProjectile},   5 },
    {"MonsterBulletAttack", {.p1 = A_MonsterBulletAttack}, 5, { 0, 0, 1, 3, 5 } },
    {"MonsterMeleeAttack",  {.p1 = A_MonsterMeleeAttack},  4, { 3, 8, 0, 0 } },
    {"RadiusDamage",        {.p1 = A_RadiusDamage},        2 },
    {"NoiseAlert",          {.p1 = A_NoiseAlert},          0 },
    {"HealChase",           {.p1 = A_HealChase},           2 },
    {"SeekTracer",          {.p1 = A_SeekTracer},          2 },
    {"FindTracer",          {.p1 = A_FindTracer},          2, { 0, 10 } },
    {"ClearTracer",         {.p1 = A_ClearTracer},         0 },
    {"JumpIfHealthBelow",   {.p1 = A_JumpIfHealthBelow},   2 },
    {"JumpIfTargetInSight", {.p1 = A_JumpIfTargetInSight}, 2 },
    {"JumpIfTargetCloser",  {.p1 = A_JumpIfTargetCloser},  2 },
    {"JumpIfTracerInSight", {.p1 = A_JumpIfTracerInSight}, 2 },
    {"JumpIfTracerCloser",  {.p1 = A_JumpIfTracerCloser},  2 },
    {"JumpIfFlagsSet",      {.p1 = A_JumpIfFlagsSet},      3 },
    {"AddFlags",            {.p1 = A_AddFlags},            2 },
    {"RemoveFlags",         {.p1 = A_RemoveFlags},         2 },
    {"WeaponProjectile",    {.p2 = A_WeaponProjectile},    5 },
    {"WeaponBulletAttack",  {.p2 = A_WeaponBulletAttack},  5, { 0, 0, 1, 5, 3 } },
    {"WeaponMeleeAttack",   {.p2 = A_WeaponMeleeAttack},   5, { 2, 10, 1 * FRACUNIT, 0, 0 } },
    {"WeaponSound",         {.p2 = A_WeaponSound},         2 },
    {"WeaponAlert",         {.p2 = A_WeaponAlert},         0 },
    {"WeaponJump",          {.p2 = A_WeaponJump},          2 },
    {"ConsumeAmmo",         {.p2 = A_ConsumeAmmo},         1 },
    {"CheckAmmo",           {.p2 = A_CheckAmmo},           2 },
    {"RefireTo",            {.p2 = A_RefireTo},            2 },
    {"GunFlashTo",          {.p2 = A_GunFlashTo},          2 },
    {"NULL",                {NULL}                           },
};

typedef enum
{
    arg_1,
    arg_1_inc,
    arg_2,
    arg_3,
    arg_4,
    arg_misc1,
    arg_misc1_inc
} arg_type_t;

typedef struct
{
    const actionf_t pointer;
    arg_type_t argtype;
} translate_args_t;

static translate_args_t translate_states[] = {
    { {.p1 = A_RandomJump },          arg_misc1 },
    { {.p1 = A_HealChase },           arg_1 },
    { {.p1 = A_JumpIfHealthBelow },   arg_1 },
    { {.p1 = A_JumpIfTargetInSight }, arg_1 },
    { {.p1 = A_JumpIfTargetCloser },  arg_1 },
    { {.p1 = A_JumpIfTracerInSight }, arg_1 },
    { {.p1 = A_JumpIfTracerCloser },  arg_1 },
    { {.p1 = A_JumpIfFlagsSet },      arg_1 },
    { {.p2 = A_WeaponJump },          arg_1 },
    { {.p2 = A_CheckAmmo },           arg_1 },
    { {.p2 = A_RefireTo },            arg_1 },
    { {.p2 = A_GunFlashTo },          arg_1 },
};

static translate_args_t translate_things[] = {
    { {.p1 = A_Spawn},              arg_misc1_inc },
    { {.p1 = A_SpawnObject},        arg_1_inc },
    { {.p1 = A_MonsterProjectile},  arg_1_inc },
    { {.p2 = A_WeaponProjectile},   arg_1_inc },
};

static translate_args_t translate_sounds[] = {
    { {.p1 = A_MonsterMeleeAttack}, arg_3 },
    { {.p1 = A_HealChase},          arg_2 },
    { {.p2 = A_WeaponMeleeAttack},  arg_4 },
    { {.p2 = A_WeaponSound},        arg_1 },
};

static void TranslateArgs(state_t *state,
                          translate_args_t *table, int size,
                          deh_translate_t translate)
{
    for (int i = 0; i < size; ++i)
    {
        if (state->action.v == table[i].pointer.v)
        {
            switch (table[i].argtype)
            {
                case arg_1:
                    state->args[0] = translate(state->args[0]);
                    break;
                case arg_1_inc:
                    state->args[0] = translate(state->args[0] - 1) + 1;
                    break;
                case arg_2:
                    state->args[1] = translate(state->args[1]);
                    break;
                case arg_3:
                    state->args[2] = translate(state->args[2]);
                    break;
                case arg_4:
                    state->args[3] = translate(state->args[3]);
                    break;
                case arg_misc1:
                    state->misc1 = translate(state->misc1);
                    break;
                case arg_misc1_inc:
                    state->misc1 = translate(state->misc1 - 1) + 1;
                    break;
            }
            return;
        }
    }
}

void DEH_ValidateStateArgs(void)
{
    const bex_codepointer_t *bex_pointer_match;

    for (int i = 0; i < num_states; i++)
    {
        state_t *state = &states[i];

        bex_pointer_match = &bex_pointer_null;

        for (int j = 0; bex_pointer_table[j].pointer.v != NULL; ++j)
        {
            if (state->action.v == bex_pointer_table[j].pointer.v)
            {
                bex_pointer_match = &bex_pointer_table[j];
                break;
            }
        }

        // ensure states don't use more mbf21 args than their
        // action pointer expects, for future-proofing's sake
        int k;
        for (k = MAXSTATEARGS - 1; k >= bex_pointer_match->argcount; k--)
        {
            if (state->args[k] != 0)
            {
                I_Error("Action %s on state %d expects no more than %d nonzero "
                        "args (%d found). Check your dehacked.",
                        bex_pointer_match->mnemonic, i,
                        bex_pointer_match->argcount, k + 1);
            }
        }

        byte defined_args = DEH_GetDefinedCodepointerArgs(i);

        // replace unset fields with default values
        for (; k >= 0; k--)
        {
            if (!(defined_args & (1 << k)))
            {
                state->args[k] = bex_pointer_match->args[k];
            }
        }

        TranslateArgs(state, translate_states, arrlen(translate_states),
                      DSDH_StateTranslate);
        TranslateArgs(state, translate_things, arrlen(translate_things),
                      DSDH_ThingTranslate);
        TranslateArgs(state, translate_sounds, arrlen(translate_sounds),
                      DSDH_SoundTranslate);
    }
}

boolean DEH_CheckSafeState(statenum_t state)
{
    int count = 0;

    for (statenum_t s = state; s != S_NULL; s = states[s].nextstate)
    {
        // [FG] recursive/nested states
        if (count++ >= 100)
        {
            return false;
        }

        // [crispy] a state with -1 tics never changes
        if (states[s].tics == -1)
        {
            break;
        }

        if (states[s].action.p2)
        {
            // [FG] A_Light*() considered harmless
            if (states[s].action.p2 == (actionf_p2)A_Light0
                || states[s].action.p2 == (actionf_p2)A_Light1
                || states[s].action.p2 == (actionf_p2)A_Light2)
            {
                continue;
            }
            else
            {
                return false;
            }
        }
    }

    return true;
}

static int DEH_BEXPointerStart(deh_context_t *context, char *line)
{
    char s[10];

    if (sscanf(line, "%9s", s) == 0 || strcmp("[CODEPTR]", s))
    {
        DEH_Warning(context, "Parse error on section start");
    }

    return 0;
}

static void DEH_BEXPointerParseLine(deh_context_t *context, char *line, int tag)
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
    int frame_number = -1;
    char frame_str[6];
    if (sscanf(variable_name, "%5s %32d", frame_str, &frame_number) != 2 || strcasecmp(frame_str, "FRAME"))
    {
        DEH_Warning(context, "Failed to parse assignment: %s", variable_name);
        return;
    }

    if (frame_number < 0)
    {
        DEH_Warning(context, "Invalid frame number: %i", frame_number);
        return;
    }

    // DSDHacked
    frame_number = DSDH_StateTranslate(frame_number);

    state_t *state = &states[frame_number];

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
    DEH_BEXPointerStart,
    DEH_BEXPointerParseLine,
    NULL,
    NULL,
};

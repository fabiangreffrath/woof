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
    const actionf_t action;
    const int argcount;           // [XA] number of mbf21 args this action uses, if any
    const int args[MAXSTATEARGS]; // default values for mbf21 args
} bex_codepointer_t;

const bex_codepointer_t bex_pointer_null = {"(NULL)", NULL};

static const bex_codepointer_t bex_pointer_table[] =
{
    {"Light0",              A_Light0                 },
    {"WeaponReady",         A_WeaponReady            },
    {"Lower",               A_Lower                  },
    {"Raise",               A_Raise                  },
    {"Punch",               A_Punch                  },
    {"ReFire",              A_ReFire                 },
    {"FirePistol",          A_FirePistol             },
    {"Light1",              A_Light1                 },
    {"FireShotgun",         A_FireShotgun            },
    {"Light2",              A_Light2                 },
    {"FireShotgun2",        A_FireShotgun2           },
    {"CheckReload",         A_CheckReload            },
    {"OpenShotgun2",        A_OpenShotgun2           },
    {"LoadShotgun2",        A_LoadShotgun2           },
    {"CloseShotgun2",       A_CloseShotgun2          },
    {"FireCGun",            A_FireCGun               },
    {"GunFlash",            A_GunFlash               },
    {"FireMissile",         A_FireMissile            },
    {"Saw",                 A_Saw                    },
    {"FirePlasma",          A_FirePlasma             },
    {"BFGsound",            A_BFGsound               },
    {"FireBFG",             A_FireBFG                },
    {"BFGSpray",            A_BFGSpray               },
    {"Explode",             A_Explode                },
    {"Pain",                A_Pain                   },
    {"PlayerScream",        A_PlayerScream           },
    {"Fall",                A_Fall                   },
    {"XScream",             A_XScream                },
    {"Look",                A_Look                   },
    {"Chase",               A_Chase                  },
    {"FaceTarget",          A_FaceTarget             },
    {"PosAttack",           A_PosAttack              },
    {"Scream",              A_Scream                 },
    {"SPosAttack",          A_SPosAttack             },
    {"VileChase",           A_VileChase              },
    {"VileStart",           A_VileStart              },
    {"VileTarget",          A_VileTarget             },
    {"VileAttack",          A_VileAttack             },
    {"StartFire",           A_StartFire              },
    {"Fire",                A_Fire                   },
    {"FireCrackle",         A_FireCrackle            },
    {"Tracer",              A_Tracer                 },
    {"SkelWhoosh",          A_SkelWhoosh             },
    {"SkelFist",            A_SkelFist               },
    {"SkelMissile",         A_SkelMissile            },
    {"FatRaise",            A_FatRaise               },
    {"FatAttack1",          A_FatAttack1             },
    {"FatAttack2",          A_FatAttack2             },
    {"FatAttack3",          A_FatAttack3             },
    {"BossDeath",           A_BossDeath              },
    {"CPosAttack",          A_CPosAttack             },
    {"CPosRefire",          A_CPosRefire             },
    {"TroopAttack",         A_TroopAttack            },
    {"SargAttack",          A_SargAttack             },
    {"HeadAttack",          A_HeadAttack             },
    {"BruisAttack",         A_BruisAttack            },
    {"SkullAttack",         A_SkullAttack            },
    {"Metal",               A_Metal                  },
    {"SpidRefire",          A_SpidRefire             },
    {"BabyMetal",           A_BabyMetal              },
    {"BspiAttack",          A_BspiAttack             },
    {"Hoof",                A_Hoof                   },
    {"CyberAttack",         A_CyberAttack            },
    {"PainAttack",          A_PainAttack             },
    {"PainDie",             A_PainDie                },
    {"KeenDie",             A_KeenDie                },
    {"BrainPain",           A_BrainPain              },
    {"BrainScream",         A_BrainScream            },
    {"BrainDie",            A_BrainDie               },
    {"BrainAwake",          A_BrainAwake             },
    {"BrainSpit",           A_BrainSpit              },
    {"SpawnSound",          A_SpawnSound             },
    {"SpawnFly",            A_SpawnFly               },
    {"BrainExplode",        A_BrainExplode           },
    // MBF
    {"Detonate",            A_Detonate               },
    {"Mushroom",            A_Mushroom               },
    {"Die",                 A_Die                    },
    {"Spawn",               A_Spawn                  },
    {"Turn",                A_Turn                   },
    {"Face",                A_Face                   },
    {"Scratch",             A_Scratch                },
    {"PlaySound",           A_PlaySound              },
    {"RandomJump",          A_RandomJump             },
    {"LineEffect",          A_LineEffect             },
    {"FireOldBFG",          A_FireOldBFG             },
    {"BetaSkullAttack",     A_BetaSkullAttack        },
    {"Stop",                A_Stop                   },
    // MBF21
    {"SpawnObject",         A_SpawnObject,         8 },
    {"MonsterProjectile",   A_MonsterProjectile,   5 },
    {"MonsterBulletAttack", A_MonsterBulletAttack, 5, { 0, 0, 1, 3, 5 } },
    {"MonsterMeleeAttack",  A_MonsterMeleeAttack,  4, { 3, 8, 0, 0 } },
    {"RadiusDamage",        A_RadiusDamage,        2 },
    {"NoiseAlert",          A_NoiseAlert,          0 },
    {"HealChase",           A_HealChase,           2 },
    {"SeekTracer",          A_SeekTracer,          2 },
    {"FindTracer",          A_FindTracer,          2, { 0, 10 } },
    {"ClearTracer",         A_ClearTracer,         0 },
    {"JumpIfHealthBelow",   A_JumpIfHealthBelow,   2 },
    {"JumpIfTargetInSight", A_JumpIfTargetInSight, 2 },
    {"JumpIfTargetCloser",  A_JumpIfTargetCloser,  2 },
    {"JumpIfTracerInSight", A_JumpIfTracerInSight, 2 },
    {"JumpIfTracerCloser",  A_JumpIfTracerCloser,  2 },
    {"JumpIfFlagsSet",      A_JumpIfFlagsSet,      3 },
    {"AddFlags",            A_AddFlags,            2 },
    {"RemoveFlags",         A_RemoveFlags,         2 },
    {"WeaponProjectile",    A_WeaponProjectile,    5 },
    {"WeaponBulletAttack",  A_WeaponBulletAttack,  5, { 0, 0, 1, 5, 3 } },
    {"WeaponMeleeAttack",   A_WeaponMeleeAttack,   5, { 2, 10, 1 * FRACUNIT, 0, 0 } },
    {"WeaponSound",         A_WeaponSound,         2 },
    {"WeaponAlert",         A_WeaponAlert,         0 },
    {"WeaponJump",          A_WeaponJump,          2 },
    {"ConsumeAmmo",         A_ConsumeAmmo,         1 },
    {"CheckAmmo",           A_CheckAmmo,           2 },
    {"RefireTo",            A_RefireTo,            2 },
    {"GunFlashTo",          A_GunFlashTo,          2 },
    {"NULL",                NULL                     },
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
    const actionf_t action;
    arg_type_t argtype;
} translate_args_t;

static translate_args_t translate_states[] = {
    { A_RandomJump,          arg_misc1 },
    { A_HealChase,           arg_1 },
    { A_JumpIfHealthBelow,   arg_1 },
    { A_JumpIfTargetInSight, arg_1 },
    { A_JumpIfTargetCloser,  arg_1 },
    { A_JumpIfTracerInSight, arg_1 },
    { A_JumpIfTracerCloser,  arg_1 },
    { A_JumpIfFlagsSet,      arg_1 },
    { A_WeaponJump,          arg_1 },
    { A_CheckAmmo,           arg_1 },
    { A_RefireTo,            arg_1 },
    { A_GunFlashTo,          arg_1 },
};

static translate_args_t translate_things[] = {
    { A_Spawn,             arg_misc1_inc },
    { A_SpawnObject,       arg_1_inc },
    { A_MonsterProjectile, arg_1_inc },
    { A_WeaponProjectile,  arg_1_inc },
};

static translate_args_t translate_sounds[] = {
    { A_PlaySound,          arg_misc1},
    { A_MonsterMeleeAttack, arg_3 },
    { A_HealChase,          arg_2 },
    { A_WeaponMeleeAttack,  arg_4 },
    { A_WeaponSound,        arg_1 },
};

static void TranslateArgs(state_t *state,
                          translate_args_t *table, int size,
                          deh_translate_t translate)
{
    for (int i = 0; i < size; ++i)
    {
        if (state->action == table[i].action)
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

        for (int j = 0; bex_pointer_table[j].action != NULL; ++j)
        {
            if (state->action == bex_pointer_table[j].action)
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

        // [FG] A_Light*() considered harmless
        actionf_t action = states[s].action;

        if (action == A_Light0 || action == A_Light1 || action == A_Light2)
        {
            continue;
        }
        else
        {
            return false;
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
            state->action = bex_pointer_table[i].action;
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

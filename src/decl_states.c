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

#include <stdlib.h>
#include <string.h>

#include "d_think.h"
#include "decl_defs.h"
#include "decl_misc.h"
#include "dsdh_main.h"
#include "doomtype.h"
#include "info.h"
#include "m_array.h"
#include "m_fixed.h"
#include "m_misc.h"
#include "m_scanner.h"

typedef struct
{
    char *name;
    actionf_t pointer;
    int argcount;
    arg_t misc1;
    arg_t misc2;
    arg_t args[8];
    actiontype_t type;
} action_t;

static action_t actions[] = {
#if 0 //weapons TODO
    {"A_Light0",          {.p2 = A_Light0}                 },
    {"A_WeaponReady",     {.p2 = A_WeaponReady}            },
    {"A_Lower",           {.p2 = A_Lower}                  },
    {"A_Raise",           {.p2 = A_Raise}                  },
    {"A_Punch",           {.p2 = A_Punch}                  },
    {"A_ReFire",          {.p2 = A_ReFire}                 },
    {"A_FirePistol",      {.p2 = A_FirePistol}             },
    {"A_Light1",          {.p2 = A_Light1}                 },
    {"A_FireShotgun",     {.p2 = A_FireShotgun}            },
    {"A_Light2",          {.p2 = A_Light2}                 },
    {"A_FireShotgun2",    {.p2 = A_FireShotgun2}           },
    {"A_CheckReload",     {.p2 = A_CheckReload}            },
    {"A_OpenShotgun2",    {.p2 = A_OpenShotgun2}           },
    {"A_LoadShotgun2",    {.p2 = A_LoadShotgun2}           },
    {"A_CloseShotgun2",   {.p2 = A_CloseShotgun2}          },
    {"A_FireCGun",        {.p2 = A_FireCGun}               },
    {"A_GunFlash",        {.p2 = A_GunFlash}               },
    {"A_FireMissile",     {.p2 = A_FireMissile}            },
    {"A_Saw",             {.p2 = A_Saw}                    },
    {"A_FirePlasma",      {.p2 = A_FirePlasma}             },
    {"A_BFGsound",        {.p2 = A_BFGsound}               },
    {"A_FireBFG",         {.p2 = A_FireBFG}                },
#endif
    {"A_BFGSpray",        {.p1 = A_BFGSpray}               },
    {"A_Explode",         {.p1 = A_Explode}                },
    {"A_Pain",            {.p1 = A_Pain}                   },
    {"A_PlayerScream",    {.p1 = A_PlayerScream}           },
    {"A_Fall",            {.p1 = A_Fall}                   },
    {"A_XScream",         {.p1 = A_XScream}                },
    {"A_Look",            {.p1 = A_Look}                   },
    {"A_Chase",           {.p1 = A_Chase}                  },
    {"A_FaceTarget",      {.p1 = A_FaceTarget}             },
    {"A_PosAttack",       {.p1 = A_PosAttack}              },
    {"A_Scream",          {.p1 = A_Scream}                 },
    {"A_SPosAttack",      {.p1 = A_SPosAttack}             },
    {"A_VileChase",       {.p1 = A_VileChase}              },
    {"A_VileStart",       {.p1 = A_VileStart}              },
    {"A_VileTarget",      {.p1 = A_VileTarget}             },
    {"A_VileAttack",      {.p1 = A_VileAttack}             },
    {"A_StartFire",       {.p1 = A_StartFire}              },
    {"A_Fire",            {.p1 = A_Fire}                   },
    {"A_FireCrackle",     {.p1 = A_FireCrackle}            },
    {"A_Tracer",          {.p1 = A_Tracer}                 },
    {"A_SkelWhoosh",      {.p1 = A_SkelWhoosh}             },
    {"A_SkelFist",        {.p1 = A_SkelFist}               },
    {"A_SkelMissile",     {.p1 = A_SkelMissile}            },
    {"A_FatRaise",        {.p1 = A_FatRaise}               },
    {"A_FatAttack1",      {.p1 = A_FatAttack1}             },
    {"A_FatAttack2",      {.p1 = A_FatAttack2}             },
    {"A_FatAttack3",      {.p1 = A_FatAttack3}             },
    {"A_BossDeath",       {.p1 = A_BossDeath}              },
    {"A_CPosAttack",      {.p1 = A_CPosAttack}             },
    {"A_CPosRefire",      {.p1 = A_CPosRefire}             },
    {"A_TroopAttack",     {.p1 = A_TroopAttack}            },
    {"A_SargAttack",      {.p1 = A_SargAttack}             },
    {"A_HeadAttack",      {.p1 = A_HeadAttack}             },
    {"A_BruisAttack",     {.p1 = A_BruisAttack}            },
    {"A_SkullAttack",     {.p1 = A_SkullAttack}            },
    {"A_Metal",           {.p1 = A_Metal}                  },
    {"A_SpidRefire",      {.p1 = A_SpidRefire}             },
    {"A_BabyMetal",       {.p1 = A_BabyMetal}              },
    {"A_BspiAttack",      {.p1 = A_BspiAttack}             },
    {"A_Hoof",            {.p1 = A_Hoof}                   },
    {"A_CyberAttack",     {.p1 = A_CyberAttack}            },
    {"A_PainAttack",      {.p1 = A_PainAttack}             },
    {"A_PainDie",         {.p1 = A_PainDie}                },
    {"A_KeenDie",         {.p1 = A_KeenDie}                },
    {"A_BrainPain",       {.p1 = A_BrainPain}              },
    {"A_BrainScream",     {.p1 = A_BrainScream}            },
    {"A_BrainDie",        {.p1 = A_BrainDie}               },
    {"A_BrainAwake",      {.p1 = A_BrainAwake}             },
    {"A_BrainSpit",       {.p1 = A_BrainSpit}              },
    {"A_SpawnSound",      {.p1 = A_SpawnSound}             },
    {"A_SpawnFly",        {.p1 = A_SpawnFly}               },
    {"A_BrainExplode",    {.p1 = A_BrainExplode}           },

    // MBF

    {"A_Detonate", {.p1 = A_Detonate}, .type = func_mbf},

    {"A_Mushroom", {.p1 = A_Mushroom}, .type = func_mbf,
      // (angle, speed)
     .argcount = 2, .misc1 = {arg_fixed}, .misc2 = {arg_fixed}
    },

    {"A_Die", {.p1 = A_Die}, .type = func_mbf},

    {"A_Spawn", {.p1 = A_Spawn}, .type = func_mbf,
      // (thing, z_pos)
     .argcount = 2, .misc1 = {arg_thing}, .misc2 = {arg_fixed}
    },

    {"A_Turn", {.p1 = A_Turn}, .type = func_mbf,
      // (deg)
     .argcount = 1, .misc1 = {arg_int}
    },

    {"A_Face", {.p1 = A_Face}, .type = func_mbf,
      // (deg)
     .argcount = 1, .misc1 = {arg_int}
    },

    {"A_Scratch", {.p1 = A_Scratch}, .type = func_mbf,
      // (damage, sound)
     .argcount = 2, .misc1 = {arg_int}, .misc2 = {arg_sound}
    },

    {"A_PlaySound", {.p1 = A_PlaySound}, .type = func_mbf,
      // (sound, fullvolume)
     .argcount = 2, .misc1 = {arg_sound}, .misc2 = {arg_int}
    },

    {"A_RandomJump", {.p1 = A_RandomJump}, .type = func_mbf,
     // (state, probabilty (0-255))
     .argcount = 2, .misc1 = {arg_state}, .misc2 = {arg_int}
    },

    {"A_LineEffect", {.p1 = A_LineEffect}, .type = func_mbf,
     // (boomspecial, tag)
     .argcount = 2, .misc1 = {arg_int}, .misc2 = {arg_int}
    },

    {"A_BetaSkullAttack", {.p1 = A_BetaSkullAttack}, .type = func_mbf},

    {"A_Stop", {.p1 = A_Stop}, .type = func_mbf},

#if 0 // weapons TODO
    {"A_FireOldBFG", {.p2 = A_FireOldBFG}},
#endif

    // MBF21

    {"A_SpawnObject", {.p1 = A_SpawnObject}, .type = func_mbf21,
     // (thing, angle, x_ofs, y_ofs, z_ofs, x_vel, y_vel, z_vel)
     .argcount = 8, .args = {{arg_thing}, {arg_fixed}, {arg_fixed}, {arg_fixed},
                          {arg_fixed}, {arg_fixed}, {arg_fixed}, {arg_fixed}}
    },

    {"A_MonsterProjectile", {.p1 = A_MonsterProjectile}, .type = func_mbf21,
      // (thing, angle, pitch, hoffset, voffset)
     .argcount = 5, .args= {{arg_thing}, {arg_fixed}, {arg_fixed}, {arg_fixed},
                         {arg_fixed}}
    },

    {"A_MonsterBulletAttack", {.p1 = A_MonsterBulletAttack}, .type = func_mbf21,
     // (hspread, vspread, numbullets, damagebase, damagedice)
     .argcount = 5, .args = {{arg_fixed}, {arg_fixed}, {arg_int, 1},
                          {arg_int, 3}, {arg_int, 5}}
    },

    {"A_MonsterMeleeAttack", {.p1 = A_MonsterMeleeAttack}, .type = func_mbf21,
     // (damagebase, damagedice, sound, range)
     .argcount = 4, .args = {{arg_int, 3}, {arg_int, 8}, {arg_sound}, {arg_fixed}}
    },

    {"A_RadiusDamage", {.p1 = A_RadiusDamage}, .type = func_mbf21,
      // (damage, radius)
     .argcount = 2, .args = {{arg_int}, {arg_int}}
    },

    {"A_NoiseAlert", {.p1 = A_NoiseAlert}, .type = func_mbf21},

    {"A_HealChase", {.p1 = A_HealChase}, .type = func_mbf21,
      // (state, sound)
     .argcount = 2, .args = {{arg_state}, {arg_sound}}
    },

    {"A_SeekTracer", {.p1 = A_SeekTracer}, .type = func_mbf21,
      // (threshold, maxturnangle)
     .argcount = 2, .args = {{arg_fixed}, {arg_fixed}}
    },

    {"A_FindTracer", {.p1 = A_FindTracer}, .type = func_mbf21,
      // (fov, rangeblocks)
     .argcount = 2, .args = {{arg_fixed}, {arg_int, 10}}
    },

    {"A_ClearTracer", {.p1 = A_ClearTracer}, .type = func_mbf21},

    {"A_JumpIfHealthBelow", {.p1 = A_JumpIfHealthBelow}, .type = func_mbf21,
      // (state, health)
     .argcount = 2, .args = {{arg_state}, {arg_int}}
    },

    {"A_JumpIfTargetInSight", {.p1 = A_JumpIfTargetInSight}, .type = func_mbf21,
      // (state, fov)
     .argcount = 2, .args = {{arg_state}, {arg_fixed}}
    },

    {"A_JumpIfTargetCloser", {.p1 = A_JumpIfTargetCloser}, .type = func_mbf21,
      // (state, distance)
     .argcount = 2, .args = {{arg_state}, {arg_fixed}}
    },

    {"A_JumpIfTracerInSight", {.p1 = A_JumpIfTracerInSight}, .type = func_mbf21,
      // (state, fov)
     .argcount = 2, .args = {{arg_state}, {arg_fixed}}
    },

    {"A_JumpIfTracerCloser", {.p1 = A_JumpIfTracerCloser}, .type = func_mbf21,
      // (state, distance)
     .argcount = 2, .args = {{arg_state}, {arg_fixed}}
    },

    {"A_JumpIfFlagsSet", {.p1 = A_JumpIfFlagsSet}, .type = func_mbf21,
      // (state, flags)
     .argcount = 2, .args = {{arg_state}, {arg_flags}}
    },

    {"A_AddFlags", {.p1 = A_AddFlags}, .type = func_mbf21,
      // (flags)
     .argcount = 1, .args = {{arg_flags}}
    },

    {"A_RemoveFlags", {.p1 = A_RemoveFlags}, .type = func_mbf21,
      // (flags)
     .argcount = 1, .args = {{arg_flags}}
    },

#if 0 // weapons TODO
    {"A_WeaponProjectile",    {.p2 = A_WeaponProjectile},    5 },
    {"A_WeaponBulletAttack",  {.p2 = A_WeaponBulletAttack},  5, { 0, 0, 1, 5, 3 } },
    {"A_WeaponMeleeAttack",   {.p2 = A_WeaponMeleeAttack},   5, { 2, 10, 1 * FRACUNIT, 0, 0 } },
    {"A_WeaponSound",         {.p2 = A_WeaponSound},         2 },
    {"A_WeaponAlert",         {.p2 = A_WeaponAlert},         0 },
    {"A_WeaponJump",          {.p2 = A_WeaponJump},          2 },
    {"A_ConsumeAmmo",         {.p2 = A_ConsumeAmmo},         1 },
    {"A_CheckAmmo",           {.p2 = A_CheckAmmo},           2 },
    {"A_RefireTo",            {.p2 = A_RefireTo},            2 },
    {"A_GunFlashTo",          {.p2 = A_GunFlashTo},          2 },
#endif
};

static void ParseStateJump(scanner_t *sc, dstate_t *state)
{
    SC_MustGetToken(sc, TK_Identifier);
    state->next.jumpstate = M_StringDuplicate(SC_GetString(sc));
    if (SC_CheckToken(sc, '+'))
    {
        SC_MustGetToken(sc, TK_IntConst);
        state->next.jumpoffset = SC_GetNumber(sc);
    }
}

static void ParseArg(scanner_t *sc, arg_t *arg)
{
    switch (arg->type)
    {
        case arg_fixed:
            arg->value = DoubleToFixed(DECL_GetNegativeDecimal(sc));
            break;
        case arg_int:
            SC_MustGetToken(sc, TK_IntConst);
            arg->value = SC_GetNumber(sc);
            break;
        case arg_thing:
        case arg_state:
            {
                SC_MustGetToken(sc, TK_Identifier);
                char *string = M_StringDuplicate(SC_GetString(sc));
                M_StringToLower(string);
                arg->data.string = string;
            }
            break;
        case arg_sound:
            arg->value = DECL_SoundMapping(sc);
            break;
        case arg_flags:
            do
            {
                DECL_ParseArgFlag(sc, arg);
            } while (SC_CheckToken(sc, '+') || SC_CheckToken(sc, '|'));
            break;
    }
}

static void ParseStateAction(scanner_t *sc, dstate_t *state)
{
    const char *fname = SC_GetString(sc);
    action_t *action = NULL;

    for (int i = 0; i < arrlen(actions); ++i)
    {
        if (strcasecmp(actions[i].name, fname) == 0)
        {
            action = &actions[i];
            state->action.pointer = action->pointer;
            break;
        }
    }
    if (!action)
    {
        SC_Error(sc, "Unknown action function %s.", fname);
        return;
    }

    state->action.type = action->type;
    state->action.argcount = action->argcount;

    if (action->type == func_vanilla)
    {
        if (SC_CheckToken(sc, '(')) // optional
        {
            SC_MustGetToken(sc, ')');
        }
    }
    else if (action->type == func_mbf)
    {
        state->action.misc1 = action->misc1;
        state->action.misc2 = action->misc2;

        if (SC_CheckToken(sc, '('))
        {
            if (action->argcount >= 1)
            {
                ParseArg(sc, &state->action.misc1);
                if (action->argcount == 2 && SC_CheckToken(sc, ','))
                {
                    ParseArg(sc, &state->action.misc2);
                }
            }
            SC_MustGetToken(sc, ')');
        }
    }
    else if (action->type == func_mbf21)
    {
        memcpy(state->action.args, action->args, sizeof(state->action.args));

        if (SC_CheckToken(sc, '('))
        {
            for (int i = 0; i < action->argcount; ++i)
            {
                if (i >= 1 && !SC_CheckToken(sc, ','))
                {
                    break;
                }
                ParseArg(sc, &state->action.args[i]);
            }
            SC_MustGetToken(sc, ')');
        }
    }
}

static void ParseState(scanner_t *sc, dstate_t *state)
{
    char *sprite = M_StringDuplicate(SC_GetString(sc));
    M_StringToUpper(sprite);

    int i;
    for (i = 0; i < array_size(sprnames); ++i)
    {
        if (strcmp(sprnames[i], sprite) == 0)
        {
            state->spritenum = i;
            free(sprite);
            break;
        }
    }
    if (i == array_size(sprnames))
    {
        i = DSDH_SpritesGetNewIndex();
        sprnames[i] = sprite;
        state->spritenum = i;
    }

    if (!SC_CheckToken(sc, TK_StringConst))
    {
        SC_MustGetToken(sc, TK_Identifier);
    }
    state->frames = M_StringDuplicate(SC_GetString(sc));
    M_StringToUpper(state->frames);

    state->duration = DECL_GetNegativeInteger(sc);

    if (SC_CheckToken(sc, TK_Identifier))
    {
        int keyword;
        do
        {
            keyword = DECL_CheckKeyword(sc, "bright", "fast", "offset");
            switch (keyword)
            {
                case 0:
                    state->bright = true;
                    break;
                case 1:
                    state->fast = true;
                    break;
                case 2:
                    SC_MustGetToken(sc, '(');
                    state->xoffset = DECL_GetNegativeInteger(sc);
                    SC_MustGetToken(sc, ',');
                    state->yoffset = DECL_GetNegativeInteger(sc);
                    SC_MustGetToken(sc, ')');
                    break;
                default:
                    break;
            }
            if (keyword < 0)
            {
                break;
            }
            if (!SC_CheckToken(sc, TK_Identifier))
            {
                break;
            }
        } while (true);

        // Action functions are technically ambiguous with starting the next
        // state, so we will just use a rule that all action functions must be
        // at least 5 characters long.
        if (strlen(SC_GetString(sc)) <= 4)
        {
            SC_Rewind(sc);
            return;
        }

        ParseStateAction(sc, state);
    }
}

void DECL_ParseActorStates(scanner_t *sc, actor_t *actor)
{
    char **labels = NULL;
    dstate_t *laststate = NULL;

    SC_MustGetToken(sc, '{');
    while (!SC_CheckToken(sc, '}'))
    {
        do
        {
            if (!SC_CheckToken(sc, TK_StringConst))
            {
                SC_MustGetToken(sc, TK_Identifier);
                char *label = M_StringDuplicate(SC_GetString(sc));
                if (DECL_CheckKeyword(sc, "loop", "goto", "stop", "wait") >= 0
                    || !SC_CheckToken(sc, ':'))
                {
                    free(label);
                    break;
                }
                else
                {
                    array_push(labels, label);
                }
            }
        } while (true);

        dstate_t state = {0};
        boolean usestate = false;

        enum {KEYWORD_Loop, KEYWORD_Goto, KEYWORD_Stop, KEYWORD_Wait};
        switch (DECL_CheckKeyword(sc, "loop", "goto", "stop", "wait"))
        {
            case KEYWORD_Loop:
                if (laststate)
                {
                    laststate->next.sequence = SEQ_Loop;
                }
                else
                {
                    SC_Error(sc, "Loop found with no previous state.");
                }
                break;
            case KEYWORD_Goto:
                if (laststate)
                {
                    laststate->next.sequence = SEQ_Goto;
                    ParseStateJump(sc, laststate);
                }
                else
                {
                    SC_Error(sc, "Goto found with no previous state.");
                }
                break;
            case KEYWORD_Stop:
                if (laststate)
                {
                    laststate->next.sequence = SEQ_Stop;
                }
                else
                {
                    SC_Error(sc, "Stop found with no previous state.");
                }
                break;
            case KEYWORD_Wait:
                if (laststate)
                {
                    laststate->next.sequence = SEQ_Wait;
                }
                else
                {
                    SC_Error(sc, "Wait found with no previous state.");
                }
                break;
            default:
                usestate = true;
                ParseState(sc, &state);
                break;
        }

        if (usestate)
        {
            array_push(actor->states, state);
            laststate = &actor->states[array_size(actor->states) - 1];

            for (int i = 0; i < array_size(labels); ++i)
            {
                label_t new_label = {
                    .label = labels[i],
                    .statenum = array_size(actor->states) - 1
                };
                array_push(actor->labels, new_label);
            }
            array_free(labels);
            labels = NULL;
        }
        else
        {
            laststate = NULL;
        }
    }

    array_free(labels);
}

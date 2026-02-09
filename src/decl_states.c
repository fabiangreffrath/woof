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
#include "m_misc.h"
#include "i_system.h"
#include "m_scanner.h"

typedef struct
{
    char *name;
    actionf_t pointer;
    int actor;
} action_t;

static action_t actions[] = {
    {"A_Light0",              {.p2 = A_Light0}                 },
    {"A_WeaponReady",         {.p2 = A_WeaponReady}            },
    {"A_Lower",               {.p2 = A_Lower}                  },
    {"A_Raise",               {.p2 = A_Raise}                  },
    {"A_Punch",               {.p2 = A_Punch}                  },
    {"A_ReFire",              {.p2 = A_ReFire}                 },
    {"A_FirePistol",          {.p2 = A_FirePistol}             },
    {"A_Light1",              {.p2 = A_Light1}                 },
    {"A_FireShotgun",         {.p2 = A_FireShotgun}            },
    {"A_Light2",              {.p2 = A_Light2}                 },
    {"A_FireShotgun2",        {.p2 = A_FireShotgun2}           },
    {"A_CheckReload",         {.p2 = A_CheckReload}            },
    {"A_OpenShotgun2",        {.p2 = A_OpenShotgun2}           },
    {"A_LoadShotgun2",        {.p2 = A_LoadShotgun2}           },
    {"A_CloseShotgun2",       {.p2 = A_CloseShotgun2}          },
    {"A_FireCGun",            {.p2 = A_FireCGun}               },
    {"A_GunFlash",            {.p2 = A_GunFlash}               },
    {"A_FireMissile",         {.p2 = A_FireMissile}            },
    {"A_Saw",                 {.p2 = A_Saw}                    },
    {"A_FirePlasma",          {.p2 = A_FirePlasma}             },
    {"A_BFGsound",            {.p2 = A_BFGsound}               },
    {"A_FireBFG",             {.p2 = A_FireBFG}                },
    {"A_BFGSpray",            {.p1 = A_BFGSpray}               },
    {"A_Explode",             {.p1 = A_Explode}                },
    {"A_Pain",                {.p1 = A_Pain}                   },
    {"A_PlayerScream",        {.p1 = A_PlayerScream}           },
    {"A_Fall",                {.p1 = A_Fall}                   },
    {"A_XScream",             {.p1 = A_XScream}                },
    {"A_Look",                {.p1 = A_Look}                   },
    {"A_Chase",               {.p1 = A_Chase}                  },
    {"A_FaceTarget",          {.p1 = A_FaceTarget}             },
    {"A_PosAttack",           {.p1 = A_PosAttack}              },
    {"A_Scream",              {.p1 = A_Scream}                 },
    {"A_SPosAttack",          {.p1 = A_SPosAttack}             },
    {"A_VileChase",           {.p1 = A_VileChase}              },
    {"A_VileStart",           {.p1 = A_VileStart}              },
    {"A_VileTarget",          {.p1 = A_VileTarget}             },
    {"A_VileAttack",          {.p1 = A_VileAttack}             },
    {"A_StartFire",           {.p1 = A_StartFire}              },
    {"A_Fire",                {.p1 = A_Fire}                   },
    {"A_FireCrackle",         {.p1 = A_FireCrackle}            },
    {"A_Tracer",              {.p1 = A_Tracer}                 },
    {"A_SkelWhoosh",          {.p1 = A_SkelWhoosh}             },
    {"A_SkelFist",            {.p1 = A_SkelFist}               },
    {"A_SkelMissile",         {.p1 = A_SkelMissile}            },
    {"A_FatRaise",            {.p1 = A_FatRaise}               },
    {"A_FatAttack1",          {.p1 = A_FatAttack1}             },
    {"A_FatAttack2",          {.p1 = A_FatAttack2}             },
    {"A_FatAttack3",          {.p1 = A_FatAttack3}             },
    {"A_BossDeath",           {.p1 = A_BossDeath}              },
    {"A_CPosAttack",          {.p1 = A_CPosAttack}             },
    {"A_CPosRefire",          {.p1 = A_CPosRefire}             },
    {"A_TroopAttack",         {.p1 = A_TroopAttack}            },
    {"A_SargAttack",          {.p1 = A_SargAttack}             },
    {"A_HeadAttack",          {.p1 = A_HeadAttack}             },
    {"A_BruisAttack",         {.p1 = A_BruisAttack}            },
    {"A_SkullAttack",         {.p1 = A_SkullAttack}            },
    {"A_Metal",               {.p1 = A_Metal}                  },
    {"A_SpidRefire",          {.p1 = A_SpidRefire}             },
    {"A_BabyMetal",           {.p1 = A_BabyMetal}              },
    {"A_BspiAttack",          {.p1 = A_BspiAttack}             },
    {"A_Hoof",                {.p1 = A_Hoof}                   },
    {"A_CyberAttack",         {.p1 = A_CyberAttack}            },
    {"A_PainAttack",          {.p1 = A_PainAttack}             },
    {"A_PainDie",             {.p1 = A_PainDie}                },
    {"A_KeenDie",             {.p1 = A_KeenDie}                },
    {"A_BrainPain",           {.p1 = A_BrainPain}              },
    {"A_BrainScream",         {.p1 = A_BrainScream}            },
    {"A_BrainDie",            {.p1 = A_BrainDie}               },
    {"A_BrainAwake",          {.p1 = A_BrainAwake}             },
    {"A_BrainSpit",           {.p1 = A_BrainSpit}              },
    {"A_SpawnSound",          {.p1 = A_SpawnSound}             },
    {"A_SpawnFly",            {.p1 = A_SpawnFly}               },
    {"A_BrainExplode",        {.p1 = A_BrainExplode}           },
    // MBF
    {"A_Detonate",            {.p1 = A_Detonate}               },
    {"A_Mushroom",            {.p1 = A_Mushroom}               },
    {"A_Die",                 {.p1 = A_Die}                    },
    {"A_Spawn",               {.p1 = A_Spawn}                  },
    {"A_Turn",                {.p1 = A_Turn}                   },
    {"A_Face",                {.p1 = A_Face}                   },
    {"A_Scratch",             {.p1 = A_Scratch}                },
    {"A_PlaySound",           {.p1 = A_PlaySound}              },
    {"A_RandomJump",          {.p1 = A_RandomJump}             },
    {"A_LineEffect",          {.p1 = A_LineEffect}             },
    {"A_FireOldBFG",          {.p2 = A_FireOldBFG}             },
    {"A_BetaSkullAttack",     {.p1 = A_BetaSkullAttack}        },
    {"A_Stop",                {.p1 = A_Stop}                   },
};

typedef struct
{
    int statenum;
    statelink_t link;
} resolve_t;

statetable_t *statetable = NULL;

static const statetable_t *FindState(const actor_t *actor, const statelink_t *link)
{
    const actor_t *start_actor = actor;

    if (link->jumpclass != NULL && link->jumpclass[0] != '\0')
    {
        if (strcasecmp(link->jumpclass, "Super") == 0)
        {
            if (actor->parent)
            {
                start_actor = actor->parent;
            }
            else
            {
                return NULL;
            }
        }
        else
        {
            I_Error("Only Super:: is supported with goto.");
        }
    }

    const char *lookup_label = link->jumpstate;

    const actor_t *check = start_actor;

    label_t *label;
    array_foreach(label, check->labels)
    {
        if (strcasecmp(label->label, lookup_label) == 0)
        {
            break;
        }
    }
    if (label == array_end(check->labels))
    {
        return NULL;
    }

    if (label->tablepos + link->jumpoffset < array_size(statetable))
    {
        return &statetable[label->tablepos + link->jumpoffset];
    }

    return NULL; // Not found
}


static void InstallStates(actor_t *actor)
{
    label_t *label = NULL;

    resolve_t *gotoresolves = NULL;

    for (int state_index = 0; state_index < array_size(actor->states); ++state_index)
    {
        dstate_t *state = &actor->states[state_index];

        // Find the label for this state
        array_foreach_type(alabel, actor->labels, label_t)
        {
            if (alabel->statenum == state_index)
            {
                label = alabel;
                alabel->tablepos = array_size(statetable);
                break;
            }
        }

        int label_start = array_size(statetable);

        statetable_t tstate = {
            .spritenum = state->spritenum,
            .duration = state->duration,
            .action = state->action,
            .xoffset = state->xoffset,
            .yoffset = state->yoffset,
            .bright = state->bright
        };

        int frameslen = strlen(state->frames);
        for (int j = 0; j < frameslen; ++j)
        {
            tstate.label = M_StringDuplicate(label->label);
            tstate.frame = state->frames[j] - 'A';
            if (j == frameslen - 1)
            {
                switch (state->next.sequence)
                {
                    case SEQ_Next:
                        tstate.next = array_size(statetable) + 1;
                        break;
                    case SEQ_Wait:
                        tstate.next = array_size(statetable);
                        break;
                    case SEQ_Stop:
                        tstate.next = 0;
                        break;
                    case SEQ_Loop:
                        tstate.next = label_start;
                        break;
                    case SEQ_Goto:
                        {
                            resolve_t res = {
                                .statenum = array_size(statetable),
                                .link = state->next
                            };
                            array_push(gotoresolves, res);
                            break;
                        }
                }
            }
            else
            {
                tstate.next = array_size(statetable) + 1;
            }

            array_push(statetable, tstate);
        }
    }

    array_foreach_type(resolve, gotoresolves, resolve_t)
    {
        statetable_t *state = &statetable[resolve->statenum];
        statelink_t *link = &resolve->link;

        const statetable_t *nextstate = FindState(actor, link);
        if (nextstate == NULL)
        {
            I_Error("Could not resolve goto %s::%s",
                    link->jumpclass ? link->jumpclass : "",
                    link->jumpstate);
        }

        state->next = nextstate - statetable;
        free(link->jumpclass);
        free(link->jumpstate);
    }
    array_free(gotoresolves);
}

static void ParseStateJump(scanner_t *sc, dstate_t *state)
{
    SC_MustGetToken(sc, TK_Identifier);
    state->next.jumpstate = M_StringDuplicate(SC_GetString(sc));

    if (SC_CheckToken(sc, TK_ScopeResolution))
    {
        state->next.jumpclass = state->next.jumpstate;
        SC_MustGetToken(sc, TK_Identifier);
        state->next.jumpstate = M_StringDuplicate(SC_GetString(sc));
    }

    if (SC_CheckToken(sc, '+'))
    {
        SC_MustGetToken(sc, TK_IntConst);
        state->next.jumpoffset = SC_GetNumber(sc);
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
                char *sprite = M_StringDuplicate(SC_GetString(sc));
                M_StringToUpper(sprite);

                int index;
                for (index = 0; index < array_size(sprnames); ++index)
                {
                    if (strcmp(sprnames[index], sprite) == 0)
                    {
                        state.spritenum = index;
                        free(sprite);
                        break;
                    }
                }
                if (index == array_size(sprnames))
                {
                    index = DSDH_SpritesGetNewIndex();
                    sprnames[index] = sprite;
                    state.spritenum = index;
                }

                if (!SC_CheckToken(sc, TK_StringConst))
                {
                    SC_MustGetToken(sc, TK_Identifier);
                }
                state.frames = M_StringDuplicate(SC_GetString(sc));
                M_StringToUpper(state.frames);

                state.duration = DECL_GetNegativeInteger(sc);

                if (SC_CheckToken(sc, TK_Identifier))
                {
                    int keyword;
                    do
                    {
                        keyword = DECL_CheckKeyword(sc, "bright", "offset");
                        switch (keyword)
                        {
                            case 0:
                                state.bright = true;
                                break;
                            case 1:
                                SC_MustGetToken(sc, '(');
                                state.xoffset = DECL_GetNegativeInteger(sc);
                                SC_MustGetToken(sc, ',');
                                state.yoffset = DECL_GetNegativeInteger(sc);
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

                    // Action functions are technically ambiguous with starting
                    // the next state, so we will just use a rule that all
                    // action functions must be at least 5 characters long.
                    if (strlen(SC_GetString(sc)) <= 4)
                    {
                        SC_Rewind(sc);
                        break;
                    }

                    const char *fname = SC_GetString(sc);
                    int action_idx;
                    for (action_idx = 0; action_idx < arrlen(actions); ++action_idx)
                    {
                        if (strcasecmp(actions[action_idx].name, fname) == 0)
                        {
                            state.action = actions[action_idx].pointer;
                            break;
                        }
                    }
                    if (action_idx == arrlen(actions))
                    {
                        SC_Warning(sc, "Unknown action function %s.", fname);
                    }

                    // Consume parameters of the unknown function
                    if (SC_CheckToken(sc, '('))
                    {
                        int paren_level = 1;
                        while (paren_level > 0 && SC_TokensLeft(sc))
                        {
                            if (SC_CheckToken(sc, ')'))
                            {
                                paren_level--;
                            }
                            else
                            {
                                SC_GetNextToken(sc, true); // Consume any token
                            }
                        }
                    }

                }
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

    InstallStates(actor);
}

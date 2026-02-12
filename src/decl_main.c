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

#include <string.h>

#include "decl_defs.h"
#include "decl_misc.h"
#include "doomtype.h"
#include "dsdh_main.h"
#include "info.h"
#include "m_array.h"
#include "m_misc.h"
#include "m_scanner.h"
#include "p_mobj.h"
#include "w_wad.h"
#include "z_zone.h"

static actor_t *actorclasses;

static void ParseActorBody(scanner_t *sc, actor_t *actor)
{
    SC_MustGetToken(sc, '{');
    while (!SC_CheckToken(sc, '}'))
    {
        if (SC_CheckToken(sc, '+'))
        {
            DECL_ParseActorFlag(sc, &actor->props, true);
        }
        else if (SC_CheckToken(sc, '-'))
        {
            DECL_ParseActorFlag(sc, &actor->props, false);
        }
        else
        {
            SC_MustGetToken(sc, TK_Identifier);
            if (strcasecmp(SC_GetString(sc), "MONSTER") == 0)
            {
                actor->props.flags |= MF_SHOOTABLE | MF_SOLID | MF_COUNTKILL;
            }
            else if (strcasecmp(SC_GetString(sc), "PROJECTILE") == 0)
            {
                actor->props.flags |=
                    MF_NOBLOCKMAP | MF_MISSILE | MF_DROPOFF | MF_NOGRAVITY;
            }
            else
            {
                switch (DECL_CheckKeyword(sc, "states"))
                {
                    case 0:
                        DECL_ParseActorStates(sc, actor);
                        break;
                    default:
                        DECL_ParseActorProperty(sc, &actor->props);
                        break;
                }
            }
        }
    }
}

static void ParseActor(scanner_t *sc)
{
    actor_t actor = {0};
    actor.classnum = array_size(actorclasses);
    actor.doomednum = -1;

    SC_MustGetToken(sc, TK_Identifier);
    actor.name = strdup(SC_GetString(sc));

    // TODO root class Actor

    if (SC_CheckToken(sc, ':'))
    {
        SC_MustGetToken(sc, TK_Identifier);
        const char *parent_name = SC_GetString(sc);
        actor_t *parent;
        array_foreach(parent, actorclasses)
        {
            if (strcasecmp(parent->name, parent_name))
            {
                break;
            }
        }
        if (parent == array_end(actorclasses))
        {
            SC_Error(sc, "Unknown parent actor '%s'.", parent_name);
        }

        actor.parent = parent;
        actor.props.flags = parent->props.flags;
        memcpy(actor.props.props, parent->props.props,
               prop_number * sizeof(property_t));
    }
    else if (actor.classnum != 0)
    {
        actor.props.flags = actorclasses[0].props.flags;
        actor.parent = NULL;
    }

    if (DECL_CheckKeyword(sc, "replaces") == 0)
    {
        SC_GetNextToken(sc, false); // consume "replaces"
        SC_MustGetToken(sc, TK_Identifier);
        actor.replaces = M_StringDuplicate(SC_GetString(sc));
    }

    if (SC_CheckToken(sc, TK_IntConst))
    {
        actor.doomednum = SC_GetNumber(sc);
    }

    array_push(actorclasses, actor);
    ParseActorBody(sc, &actorclasses[array_size(actorclasses) - 1]);
}

static void ParseDecorate(scanner_t *sc)
{
    while (SC_TokensLeft(sc))
    {
        SC_MustGetToken(sc, TK_Identifier);
        DECL_RequireKeyword(sc, "actor");
        ParseActor(sc);
    }
}

static void InstallMobjInfo(int start_mobjtype)
{
    mobjtype_t mobjtype = start_mobjtype;
    array_foreach_type(actor, actorclasses, actor_t)
    {
        mobjtype = DSDH_ThingTranslate(mobjtype);
        mobjinfo_t *mobj = &mobjinfo[mobjtype];

        mobj->doomednum = actor->doomednum;
        mobj->flags = actor->props.flags;
        mobj->flags2 = actor->props.flags2;

        for (proptype_t type = 0; type < prop_number; ++type)
        {
            propvalue_t value = actor->props.props[type].value;
            switch (type)
            {
                case prop_health:
                    mobj->spawnhealth = value.number;
                    break;
                case prop_seesound:
                    mobj->seesound = value.number;
                    break;
                case prop_reactiontime:
                    mobj->reactiontime = value.number;
                    break;
                case prop_attacksound:
                    mobj->attacksound = value.number;
                    break;
                case prop_painchance:
                    mobj->painchance = value.number;
                    break;
                case prop_painsound:
                    mobj->painsound = value.number;
                    break;
                case prop_deathsound:
                    mobj->deathsound = value.number;
                    break;
                case prop_speed:
                    mobj->speed = mobj->flags & MF_MISSILE
                                      ? IntToFixed(value.number)
                                      : value.number;
                    break;
                case prop_radius:
                    mobj->radius = IntToFixed(value.number);
                    break;
                case prop_height:
                    mobj->height = IntToFixed(value.number);
                    break;
                case prop_mass:
                    mobj->mass = value.number;
                    break;
                case prop_damage:
                    mobj->damage = value.number;
                    break;
                case prop_activesound:
                    mobj->activesound = value.number;
                    break;
                default:
                    break;
            }
        }

        ++mobjtype;
    }
}

static void ResolveMobjInfoStatePointers(int start_statenum, int start_mobjtype)
{
    array_foreach_type(actor, actorclasses, actor_t)
    {
        mobjtype_t mobjtype = start_mobjtype + actor->classnum;
        mobjinfo_t *mobj = &mobjinfo[mobjtype];

        array_foreach_type(label, actor->labels, label_t)
        {
            int statenum =
                start_statenum + actor->statetablepos + label->tablepos;
            if (!strcasecmp(label->label, "spawn"))
            {
                mobj->spawnstate = statenum;
            }
            else if (!strcasecmp(label->label, "see"))
            {
                mobj->seestate = statenum;
            }
            else if (!strcasecmp(label->label, "pain"))
            {
                mobj->painstate = statenum;
            }
            else if (!strcasecmp(label->label, "melee"))
            {
                mobj->meleestate = statenum;
            }
            else if (!strcasecmp(label->label, "missile"))
            {
                mobj->missilestate = statenum;
            }
            else if (!strcasecmp(label->label, "death"))
            {
                mobj->deathstate = statenum;
            }
            else if (!strcasecmp(label->label, "xdeath"))
            {
                mobj->xdeathstate = statenum;
            }
            else if (!strcasecmp(label->label, "raise"))
            {
                mobj->raisestate = statenum;
            }
        }
    }
}

static int ResolveActionArgs(arg_t *arg, int start_statenum, int start_mobjtype)
{
    switch (arg->type)
    {
        case arg_thing:
            array_foreach_type(target_actor, actorclasses, actor_t)
            {
                if (!strcasecmp(target_actor->name, arg->data.string))
                {
                    return start_mobjtype + target_actor->classnum + 1;
                }
            }
            I_Error("Not found actor '%s' for action parameter.",
                    arg->data.string);
            break;

        case arg_state:
            array_foreach_type(owner, actorclasses, actor_t)
            {
                array_foreach_type(label, owner->labels, label_t)
                {
                    if (!strcasecmp(label->label, arg->data.string))
                    {
                        return start_statenum + owner->statetablepos
                               + label->tablepos;
                    }
                }
            }
            I_Error("Not found state label for action parameter '%s'.",
                    arg->data.string);
            break;

        default:
            return arg->value;
            break;
    }
}

static void InstallStateAction(state_t *state, stateaction_t *action,
                               int start_statenum, int start_mobjtype)
{
    if (action->type == func_mbf)
    {
        state->misc1 = ResolveActionArgs(&action->misc1, start_statenum,
                                         start_mobjtype);
        state->misc2 = ResolveActionArgs(&action->misc2, start_statenum,
                                         start_mobjtype);
    }
    else if (action->type == func_mbf21)
    {
        for (int i = 0; i < action->argcount; ++i)
        {
            arg_t *arg = &action->args[i];
            if (arg->type == arg_flags)
            {
                state->args[i] = arg->value;
                state->args[i + 1] = arg->data.integer;
            }
            else
            {
                state->args[i] = ResolveActionArgs(arg, start_statenum,
                                                   start_mobjtype);
            }
        }
    }

    state->action = action->pointer;
}

static statenum_t ResolveGoto(const actor_t *current_actor,
                              const statelink_t *link, int start_statenum)
{
    const actor_t *target_actor = current_actor;
    if (link->jumpclass != NULL && link->jumpclass[0] != '\0')
    {
        if (strcasecmp(link->jumpclass, "Super") == 0)
        {
            if (current_actor->parent)
            {
                target_actor = current_actor->parent;
            }
            else
            {
                I_Error("Actor '%s' has no parent for Super:: goto.",
                        current_actor->name);
            }
        }
    }

    label_t *label;
    array_foreach(label, target_actor->labels)
    {
        if (strcasecmp(label->label, link->jumpstate) == 0)
        {
            break;
        }
    }
    if (label == array_end(target_actor->labels))
    {
        I_Error("Could not resolve goto %s::%s+%d",
                link->jumpclass ? link->jumpclass : "", link->jumpstate,
                link->jumpoffset);
    }

    return start_statenum + target_actor->statetablepos + label->tablepos
           + link->jumpoffset;
}

static void InstallStates(int start_statenum, int start_mobjtype)
{
    // Pass 1: Calculate state offsets and fill label->tablepos
    int total_states = 0;
    array_foreach_type(actor, actorclasses, actor_t)
    {
        actor->statetablepos = total_states;
        int offset = 0;
        array_foreach_type(dstate, actor->states, dstate_t)
        {
            array_foreach_type(label, actor->labels, label_t)
            {
                if (label->statenum == (dstate - actor->states))
                {
                    label->tablepos = offset;
                }
            }
            offset += strlen(dstate->frames);
        }
        total_states += offset;
    }

    // Pass 2: Install states
    statenum_t statenum = start_statenum;
    array_foreach_type(actor, actorclasses, actor_t)
    {
        int actor_state_offset = 0;
        array_foreach_type(dstate, actor->states, dstate_t)
        {
            int label_start_offset = actor_state_offset;
            int frameslen = strlen(dstate->frames);

            for (int i = 0; i < frameslen; ++i)
            {
                statenum = DSDH_StateTranslate(statenum);
                state_t *state = &states[statenum];

                state->sprite = dstate->spritenum;
                state->frame = dstate->frames[i] - 'A';
                if (dstate->bright)
                {
                    state->frame |= FF_FULLBRIGHT;
                }
                state->tics = dstate->duration;

                InstallStateAction(state, &dstate->action, start_statenum,
                                   start_mobjtype);

                // nextstate handling
                if (i == frameslen - 1)
                {
                    switch (dstate->next.sequence)
                    {
                        case SEQ_Next:
                            state->nextstate = statenum + 1;
                            break;
                        case SEQ_Wait:
                            state->nextstate = statenum;
                            break;
                        case SEQ_Stop:
                            state->nextstate = S_NULL;
                            break;
                        case SEQ_Loop:
                            state->nextstate = start_statenum
                                               + actor->statetablepos
                                               + label_start_offset;
                            break;
                        case SEQ_Goto:
                            state->nextstate = ResolveGoto(actor, &dstate->next,
                                                           start_statenum);
                            break;
                    }
                }
                else
                {
                    state->nextstate = statenum + 1;
                }
                statenum++;
            }
            actor_state_offset += frameslen;
        }
    }
}

static void DECL_Integrate(void)
{
    int start_statenum = DSDH_StatesGetNewIndex();
    int start_mobjtype = DSDH_MobjInfoGetNewIndex();

    InstallMobjInfo(start_mobjtype);
    InstallStates(start_statenum, start_mobjtype);
    ResolveMobjInfoStatePointers(start_statenum, start_mobjtype);
}

void DECL_Parse(int lumpnum)
{
    scanner_t *sc = SC_Open("DECLARE", W_CacheLumpNum(lumpnum, PU_CACHE),
                            W_LumpLength(lumpnum));

    ParseDecorate(sc);

    SC_Close(sc);

    DECL_Integrate();
}

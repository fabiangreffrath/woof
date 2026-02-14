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
#include "decl_main.h"
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

static int ReplaceActor(scanner_t *sc, const char *name)
{
    actor_t *actor;
    array_foreach(actor, actorclasses)
    {
        if (strcasecmp(actor->name, name) == 0)
        {
            break;
        }
    }
    if (actor == array_end(actorclasses))
    {
        SC_Error(sc, "Actor '%s' is not found.", name);
    }

    for (mobjtype_t type = 0; type < num_mobj_types; ++type)
    {
        if (mobjinfo[type].doomednum == actor->doomednum)
        {
            mobjinfo[type].doomednum = -1;
            break;
        }
    }
    return actor->doomednum;
}

static void ParseActor(scanner_t *sc)
{
    actor_t actor = {0};
    actor.installnum = -1;
    actor.doomednum = -1;

    SC_MustGetToken(sc, TK_Identifier);
    actor.name = M_StringDuplicate(SC_GetString(sc));

    if (SC_CheckToken(sc, ':'))
    {
        SC_MustGetToken(sc, TK_Identifier);
        const char *parent_name = SC_GetString(sc);
        actor_t *parent;
        array_foreach(parent, actorclasses)
        {
            if (!strcasecmp(parent->name, parent_name))
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
        actor.props.flags2 = parent->props.flags2;
        array_copy(actor.props.props, parent->props.props);
        array_copy(actor.states, parent->states);
        array_copy(actor.labels, parent->labels);
        actor.numstates = parent->numstates;
    }

    if (SC_CheckToken(sc, TK_Identifier))
    {
        if (DECL_CheckKeyword(sc, "replaces") == 0)
        {
            SC_MustGetToken(sc, TK_Identifier);
            actor.doomednum = ReplaceActor(sc, SC_GetString(sc));
        }
        else
        {
            SC_Rewind(sc);
        }
    }

    if (SC_CheckToken(sc, TK_IntConst))
    {
        if (actor.doomednum == -1)
        {
            actor.doomednum = SC_GetNumber(sc);
        }
    }

    if (SC_CheckToken(sc, TK_Identifier))
    {
        if (DECL_CheckKeyword(sc, "native") == 0)
        {
            actor.native = true;
        }
    }

    ParseActorBody(sc, &actor);
    array_push(actorclasses, actor);
}

static void ParseDecorate(scanner_t *sc)
{
    while (SC_TokensLeft(sc))
    {
        if (SC_CheckToken(sc, '#'))
        {
            SC_MustGetToken(sc, TK_Identifier);
            DECL_RequireKeyword(sc, "include");
            SC_MustGetToken(sc, TK_StringConst);
            const char *lump = SC_GetString(sc);
            int lumpnum = W_CheckNumForName(lump);
            if (lumpnum < 0)
            {
                SC_Error(sc, "Lump '%s' is not found.", lump);
            }
            DECL_Parse(lumpnum);
        }
        else
        {
            SC_MustGetToken(sc, TK_Identifier);
            DECL_RequireKeyword(sc, "actor");
            ParseActor(sc);
        }
    }
}

static int InstallMobjInfo(void)
{
    int start_mobjtype = -1;
    array_foreach_type(actor, actorclasses, actor_t)
    {
        if (actor->native)
        {
            continue;
        }

        mobjtype_t mobjtype = DSDH_MobjInfoGetNewIndex();
        if (start_mobjtype == -1)
        {
            start_mobjtype = mobjtype;
        }

        mobjinfo_t *mobj = &mobjinfo[mobjtype];

        mobj->doomednum = actor->doomednum;
        mobj->flags = actor->props.flags;
        mobj->flags2 = actor->props.flags2;

        array_foreach_type(prop, actor->props.props, property_t)
        {
            propvalue_t value = prop->value;
            switch (prop->type)
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
                    mobj->radius = value.number;
                    break;
                case prop_height:
                    mobj->height = value.number;
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

                case prop_dropitem:
                    mobj->droppeditem = value.number;
                    break;
                case prop_obituary:
                    mobj->obituary = value.string;
                    break;
                case prop_obituary_melee:
                    mobj->obituary_melee = value.string;
                    break;
                case prop_obituary_self:
                    mobj->obituary_self = value.string;
                    break;
                default:
                    break;
            }
        }
    }
    return start_mobjtype;
}

static void ResolveMobjInfoStatePointers(int start_statenum, int start_mobjtype)
{
    if (start_statenum == -1 || start_mobjtype == -1)
    {
        return;
    }

    array_foreach_type(actor, actorclasses, actor_t)
    {
        if (actor->native)
        {
            continue;
        }

        mobjtype_t mobjtype = start_mobjtype + actor->installnum;
        mobjinfo_t *mobj = &mobjinfo[mobjtype];

        array_foreach_type(label, actor->labels, label_t)
        {
            int statenum =
                start_statenum + actor->tablepos + label->tablepos;
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

static int ResolveActionArgs(const actor_t *owner, const arg_t *arg,
                             int start_statenum, int start_mobjtype)
{
    switch (arg->type)
    {
        case arg_thing:
            array_foreach_type(actor, actorclasses, actor_t)
            {
                if (actor->native)
                {
                    continue;
                }
                if (!strcasecmp(actor->name, arg->data.string))
                {
                    return start_mobjtype + actor->installnum + 1;
                }
            }
            I_Error("Not found actor '%s' for action parameter.",
                    arg->data.string);
            break;
        case arg_state:
            array_foreach_type(label, owner->labels, label_t)
            {
                if (!strcasecmp(label->label, arg->data.string))
                {
                    return start_statenum + owner->tablepos
                           + label->tablepos;
                }
            }
            I_Error("Not found state label '%s' for action parameter.",
                    arg->data.string);
            break;
        default:
            return arg->value;
            break;
    }
    return 0;
}

static void InstallStateAction(const actor_t *owner, state_t *state,
                               stateaction_t *action, int start_statenum,
                               int start_mobjtype)
{
    if (action->type == func_mbf)
    {
        state->misc1 = ResolveActionArgs(owner, &action->misc1, start_statenum,
                                         start_mobjtype);
        state->misc2 = ResolveActionArgs(owner, &action->misc2, start_statenum,
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
                state->args[i] = ResolveActionArgs(owner, arg, start_statenum,
                                                   start_mobjtype);
            }
        }
    }

    state->action = action->pointer;
}

static statenum_t ResolveGoto(const actor_t *actor, const statelink_t *link,
                              int start_statenum)
{
    label_t *label;
    array_foreach(label, actor->labels)
    {
        if (strcasecmp(label->label, link->jumpstate) == 0)
        {
            break;
        }
    }
    if (label == array_end(actor->labels))
    {
        I_Error("Could not resolve goto %s+%d", link->jumpstate,
                link->jumpoffset);
    }

    if (label->tablepos + link->jumpoffset >= actor->numstates)
    {
        I_Error("Goto %s+%d jumps out of actor states.",
                link->jumpstate, link->jumpoffset);
    }

    return start_statenum + actor->tablepos + label->tablepos
           + link->jumpoffset;
}

static int InstallStates(int start_mobjtype)
{
    if (start_mobjtype == -1)
    {
        return -1;
    }

    // Pass 1: Calculate state offsets and fill label->tablepos
    int total_states = 0;
    array_foreach_type(actor, actorclasses, actor_t)
    {
        if (actor->native)
        {
            actor->tablepos = -1;
            continue;
        }

        actor->tablepos = total_states;

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
        actor->numstates = offset;
        total_states += offset;
    }

    // Pass 2: Install states
    int start_statenum = -1;
    array_foreach_type(actor, actorclasses, actor_t)
    {
        if (actor->native)
        {
            continue;
        }

        int actor_state_offset = 0;
        array_foreach_type(dstate, actor->states, dstate_t)
        {
            int label_start_offset = actor_state_offset;
            int frameslen = strlen(dstate->frames);

            for (int i = 0; i < frameslen; ++i)
            {
                statenum_t statenum = DSDH_StatesGetNewIndex();
                if (start_statenum == -1)
                {
                    start_statenum = statenum;
                }

                state_t *state = &states[statenum];

                state->sprite = dstate->spritenum;
                state->frame = dstate->frames[i] - 'A';
                if (dstate->bright)
                {
                    state->frame |= FF_FULLBRIGHT;
                }
                if (dstate->fast)
                {
                    state->flags |= STATEF_SKILL5FAST;
                }
                state->tics = dstate->duration;

                InstallStateAction(actor, state, &dstate->action, start_statenum,
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
                                               + actor->tablepos
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
            }
            actor_state_offset += frameslen;
        }
    }
    return start_statenum;
}

void DECL_Integrate(void)
{
    int install_count = 0;
    array_foreach_type(actor, actorclasses, actor_t)
    {
        if (!actor->native)
        {
            actor->installnum = install_count++;
        }
    }
    int start_mobjtype = InstallMobjInfo();
    int start_statenum = InstallStates(start_mobjtype);
    ResolveMobjInfoStatePointers(start_statenum, start_mobjtype);
}

void DECL_Parse(int lumpnum)
{
    char lumpname[9] = {0};
    M_CopyLumpName(lumpname, lumpinfo[lumpnum].name);
    scanner_t *sc = SC_Open(lumpname, W_CacheLumpNum(lumpnum, PU_CACHE),
                            W_LumpLength(lumpnum));
    ParseDecorate(sc);
    SC_Close(sc);
}

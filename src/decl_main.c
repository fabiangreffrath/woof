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
                    MF_NOBLOCKMAP | MF_MISSILE | MF_DROPOFF | MF_NOCLIP;
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
        memcpy(actor.props.props, parent->props.props, prop_number * sizeof(property_t));
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

void ParseDecorate(scanner_t *sc)
{
    while (SC_TokensLeft(sc))
    {
        SC_MustGetToken(sc, TK_Identifier);
        DECL_RequireKeyword(sc, "actor");
        ParseActor(sc);
    }
}

static void DECL_Integrate(void)
{
    statenum_t basenum = -1;

    array_foreach_type(decl, statetable, statetable_t)
    {
        statenum_t num = DSDH_StatesGetNewIndex();
        if (basenum == -1)
        {
            basenum = num;
        }

        state_t *state = &states[num];

        state->sprite = decl->spritenum;
        state->frame = decl->frame;
        if (decl->bright)
        {
            state->frame |= FF_FULLBRIGHT;
        }
        state->tics = decl->duration;
        state->action = decl->action;
        state->nextstate = basenum + decl->next;
    }

    array_foreach_type(actor, actorclasses, actor_t)
    {
        mobjtype_t mobjtype = DSDH_MobjInfoGetNewIndex();

        mobjinfo_t *mobj = &mobjinfo[mobjtype];

        mobj->doomednum = actor->doomednum;
        mobj->flags = actor->props.flags;

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
                    mobj->speed = value.number;
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
                default:
                    break;
            }
        }

        array_foreach_type(label, actor->labels, label_t)
        {
            int statenum = basenum + label->tablepos;
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

void DECL_Parse(int lumpnum)
{
    scanner_t *sc = SC_Open("DECLARE", W_CacheLumpNum(lumpnum, PU_CACHE),
                            W_LumpLength(lumpnum));

    ParseDecorate(sc);

    SC_Close(sc);

    DECL_Integrate();
}

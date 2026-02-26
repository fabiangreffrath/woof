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
#include "decl_main.h"
#include "doomtype.h"
#include "info.h"
#include "m_array.h"
#include "m_hashmap.h"
#include "m_misc.h"
#include "m_scanner.h"
#include "p_mobj.h"
#include "w_wad.h"
#include "z_zone.h"

hashmap_t *actors;

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
                switch (SC_CheckKeyword(sc, "states"))
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
    actor_t *actor = hashmap_get_str(actors, name);
    if (!actor)
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
    actor.doomednum = -1;

    SC_MustGetToken(sc, TK_Identifier);
    actor.name = M_StringDuplicate(SC_GetString(sc));
    M_StringToLower(actor.name);

    if (SC_CheckToken(sc, ':'))
    {
        SC_MustGetToken(sc, TK_Identifier);
        char *parent_name = M_StringDuplicate(SC_GetString(sc));
        M_StringToLower(parent_name);
        actor_t *parent = hashmap_get_str(actors, parent_name);
        if (!parent)
        {
            SC_Error(sc, "Unknown parent actor '%s'.", parent_name);
        }
        free(parent_name);

        actor.parent = parent;
        actor.props.flags = parent->props.flags;
        actor.props.flags2 = parent->props.flags2;
        array_copy(actor.props.props, parent->props.props);
        array_copy(actor.states, parent->states);
        array_copy(actor.labels, parent->labels);
    }

    if (SC_CheckToken(sc, TK_Identifier))
    {
        if (SC_CheckKeyword(sc, "replaces") == 0)
        {
            SC_MustGetToken(sc, TK_Identifier);
            char *name = M_StringDuplicate(SC_GetString(sc));
            M_StringToLower(name);
            actor.doomednum = ReplaceActor(sc, name);
            free(name);
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
        if (SC_CheckKeyword(sc, "native") == 0)
        {
            actor.native = true;
        }
    }

    ParseActorBody(sc, &actor);

    if (!actors)
    {
        actors = hashmap_init_str(16, sizeof(actor_t));
    }
    hashmap_put_str(actors, actor.name, &actor);
}

static void ParseDeclarate(scanner_t *sc)
{
    while (SC_TokensLeft(sc))
    {
        if (SC_CheckToken(sc, '#'))
        {
            SC_MustGetToken(sc, TK_Identifier);
            SC_RequireKeyword(sc, "include");
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
            enum {KEYWORD_Thing, KEYWORD_Sound, KEYWORD_Ambient};
            switch (SC_RequireKeyword(sc, "thing", "sound", "ambient"))
            {
                case KEYWORD_Thing:
                    ParseActor(sc);
                    break;
                case KEYWORD_Sound:
                    DECL_ParseSound(sc);
                    break;
                case KEYWORD_Ambient:
                    DECL_ParseAmbient(sc);
                    break;
                default:
                    break;
            }
        }
    }
}

void DECL_Install(void)
{
    DECL_InstallSounds();
    DECL_InstallAmbient();
    DECL_InstallMobjInfo();
    DECL_InstallStates();
    DECL_ResolveMobjInfoStatePointers();
}

void DECL_Parse(int lumpnum)
{
    char lumpname[9] = {0};
    M_CopyLumpName(lumpname, lumpinfo[lumpnum].name);
    scanner_t *sc = SC_OpenOptions("declarate", (version_t){1, 0, 0}, lumpname,
                                   W_CacheLumpNum(lumpnum, PU_CACHE),
                                   W_LumpLength(lumpnum));
    ParseDeclarate(sc);
    SC_Close(sc);
}

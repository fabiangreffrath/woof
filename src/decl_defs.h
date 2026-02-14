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

#ifndef DECL_DEFS_H
#define DECL_DEFS_H

#include <stdint.h>

#include "d_think.h"
#include "doomtype.h"
#include "m_scanner.h"

typedef enum
{
    prop_health,
    prop_seesound,
    prop_reactiontime,
    prop_attacksound,
    prop_painchance,
    prop_painsound,
    prop_deathsound,
    prop_speed,
    prop_radius,
    prop_height,
    prop_mass,
    prop_damage,
    prop_activesound,
    prop_dropitem,

    prop_renderstyle,
    prop_translation,

    prop_obituary,
    prop_obituary_melee,
    prop_obituary_self,

    prop_number
} proptype_t;

typedef struct
{
    char *string;
    int number;
} propvalue_t;

typedef struct
{
    proptype_t type;
    propvalue_t value;
} property_t;

typedef struct
{
    property_t *props;
    uint32_t flags;
    uint32_t flags2;
} proplist_t;

int DECL_SoundMapping(scanner_t *sc);
void DECL_ParseActorProperty(scanner_t *sc, proplist_t *proplist);
void DECL_ParseActorFlag(scanner_t *sc, proplist_t *proplist, boolean set);

typedef struct
{
    char *label;
    int statenum;
    int tablepos;
} label_t;

typedef enum
{
    SEQ_Next,
    SEQ_Goto,
    SEQ_Stop,
    SEQ_Wait,
    SEQ_Loop
} sequence_t;

typedef struct
{
    sequence_t sequence;
    char *jumpstate;
    int jumpoffset;
} statelink_t;

typedef enum
{
    func_vanilla,
    func_mbf,
    func_mbf21
} actiontype_t;

typedef enum
{
    arg_fixed,
    arg_int,
    arg_thing,
    arg_state,
    arg_sound,
    arg_flags
} argtype_t;

typedef struct
{
    argtype_t type;
    int value;
    union
    {
        const char *string;
        int integer;
    } data;
} arg_t;

typedef struct
{
    actiontype_t type;
    actionf_t pointer;
    arg_t misc1;
    arg_t misc2;
    int argcount;
    arg_t args[8];
} stateaction_t;

typedef struct
{
    int spritenum;
    char *frames;
    statelink_t next;
    stateaction_t action;
    int duration;
    int xoffset;
    int yoffset;
    boolean bright;
    boolean fast;
} dstate_t;

typedef struct actor_s actor_t;

struct actor_s
{
    char *name;
    char *replaces;
    boolean native;
    int doomednum;
    int classnum;
    int installnum;
    actor_t *parent;
    proplist_t props;
    dstate_t *states;
    label_t *labels;
    int statetablepos;
    int numstates;
};

void DECL_ParseArgFlag(scanner_t *sc, arg_t *arg);
void DECL_ParseActorStates(scanner_t *sc, actor_t *actor);

#endif

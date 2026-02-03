
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doomtype.h"
#include "m_array.h"
#include "m_fixed.h"
#include "m_misc.h"
#include "m_scanner.h"
#include "p_mobj.h"
#include "w_wad.h"
#include "z_zone.h"

#include "dsdh_main.h"
#include "info.h"

typedef struct
{
    const char *name;
    uint32_t flag;
} flagdef_t;

// Global arrays - defined here
flagdef_t Flags[] = {
    {"SPECIAL",      MF_SPECIAL     },
    {"SOLID",        MF_SOLID       },
    {"SHOOTABLE",    MF_SHOOTABLE   },
    {"NOSECTOR",     MF_NOSECTOR    },
    {"NOBLOCKMAP",   MF_NOBLOCKMAP  },
    {"AMBUSH",       MF_AMBUSH      },
    {"JUSTHIT",      MF_JUSTHIT     },
    {"JUSTATTACKED", MF_JUSTATTACKED},
    {"SPAWNCEILING", MF_SPAWNCEILING},
    {"NOGRAVITY",    MF_NOGRAVITY   },
    {"DROPOFF",      MF_DROPOFF     },
    {"PICKUP",       MF_PICKUP      },
    {"NOCLIP",       MF_NOCLIP      },
    {"SLIDE",        MF_SLIDE       },
    {"FLOAT",        MF_FLOAT       },
    {"TELEPORT",     MF_TELEPORT    },
    {"MISSILE",      MF_MISSILE     },
    {"DROPPED",      MF_DROPPED     },
    {"SHADOW",       MF_SHADOW      },
    {"NOBLOOD",      MF_NOBLOOD     },
    {"CORPSE",       MF_CORPSE      },
    {"INFLOAT",      MF_INFLOAT     },
    {"COUNTKILL",    MF_COUNTKILL   },
    {"COUNTITEM",    MF_COUNTITEM   },
    {"SKULLFLY",     MF_SKULLFLY    },
    {"NOTDMATCH",    MF_NOTDMATCH   },
    {"TRANSLATION1", MF_TRANSLATION1},
    {"TRANSLATION2", MF_TRANSLATION2}
};

typedef struct
{
    char *codename;
    char *logical_name;
} sound_mapping_t;

static sound_mapping_t *soundtable;
static char **spritenames;

typedef struct
{
    char *function;
    int actor;
} action_t;

static action_t *action_functions;

typedef struct
{
    char *label;
    int state;
    bool deleted;
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
    char *jumpclass;
    int jumpoffset;
} statelink_t;

typedef struct
{
    char *sprite;
    char *frames;
    statelink_t next;
    int action;
    int duration;
    int xoffset;
    int yoffset;
    boolean bright;
} dstate_t;

typedef struct
{
    enum
    {
        TYPE_Int,
        TYPE_Fixed,
        TYPE_Sound,
        TYPE_Flags,
        TYPE_State
    } type;

    char *name;
    char *codename;
} property_t;

typedef struct
{
    char *string;
    int number;
} propvalue_t;

#define M_HASHMAP_KEY_STRING
#define M_HASHMAP_VALUE_T propvalue_t
#include "m_hashmap.h"

typedef struct
{
    hashmap_t *props;
    uint32_t flags;
} proplist;

typedef struct
{
    char *name;
    char *codename;
    char *replaces;
    int classNum;
    int parent;
    int doomednum;
    bool native;
    proplist props;
    dstate_t *states;
    label_t *labels;
    property_t *properties;
} actor_t;

static actor_t *actorclasses;

typedef struct
{
    char *label;
    int stemend;
    int frame;
} framelabel_t;

typedef struct
{
    char *label;
    char *sprite;
    unsigned frame;
    int duration;
    int action;
    int next;
    int xoffset;
    int yoffset;
    boolean bright;
} State;

static State *StateTable;

typedef struct
{
    int statenum;
    statelink_t link;
} resolve_t;

static int CheckKeywordInternal(scanner_t *sc,
                                const char *keywords[], int count)
{
    const char *string = SC_GetString(sc);
    for (int i = 0; i < count; ++i)
    {
        if (strcasecmp(keywords[i], string) == 0)
        {
            return i;
        }
    }
    return -1;
}

#define CheckKeyword(sc, keywords) \
    CheckKeywordInternal(sc, keywords, arrlen(keywords))

static int RequireKeywordInternal(scanner_t *sc, const char *keywords[],
                                  int count)
{
    int result = CheckKeywordInternal(sc, keywords, count);
    if (result >= 0)
    {
        return result;
    }
    SC_Error(sc, "Invalid keyword at this point.");
    return -1;
}

#define RequireKeyword(sc, keywords) \
    RequireKeywordInternal(sc, keywords, arrlen(keywords))

// Helper function for when we need to parse a signed integer.
inline static int GetNegativeInteger(scanner_t *sc)
{
    bool neg = SC_CheckToken(sc, '-');
    SC_MustGetToken(sc, TK_IntConst);
    return neg ? -SC_GetNumber(sc) : SC_GetNumber(sc);
}

static int LookupSound(const char *sound)
{
    for (int i = 0; i < array_size(soundtable); ++i)
    {
        if (strcasecmp(soundtable[i].logical_name, sound) == 0)
        {
            return i + 1;
        }
    }
    return 0;
}

static char *FrameLabel_Assign(framelabel_t *framelabel)
{
    char *ret = M_StringDuplicate(framelabel->label);

    framelabel->label[framelabel->stemend] = '\0';

    ++framelabel->frame;
    char num[10];
    snprintf(num, 10, "%d", framelabel->frame);

    char *tmp = framelabel->label;
    framelabel->label = M_StringJoin(tmp, num);
    free(tmp);

    return ret;
}

static void FrameLabel_Reset(framelabel_t *framelabel, const actor_t *actor,
                            const label_t *label)
{
    free(framelabel->label);
    
    framelabel->label = M_StringJoin(actor->name, "_", label->label);
    M_StringToUpper(framelabel->label);

    framelabel->stemend = strlen(framelabel->label);
    while (framelabel->stemend > 0
           && isdigit(framelabel->label[framelabel->stemend - 1]))
    {
        --framelabel->stemend;
    }

    if (framelabel->stemend != (int)strlen(framelabel->label))
    {
        framelabel->frame = atoi(framelabel->label + framelabel->stemend);
    }
    else
    {
        framelabel->frame = 1;
    }
}

static const State *FindState(const actor_t *actor, const statelink_t *link)
{
    const actor_t *start_actor = actor;

    if (link->jumpclass != NULL && link->jumpclass[0] != '\0')
    {
        if (strcasecmp(link->jumpclass, "Super") == 0)
        {
            if (actor->parent != -1)
            {
                start_actor = &actorclasses[actor->parent];
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

    char *lookup_label = strdup(link->jumpstate);
    if (!lookup_label)
    {
        I_Error("FindState: strdup failed");
    }

    while (true) // This loop is for shortening the label
    {
        const actor_t *check = start_actor;
        do // This loop is for traversing the hierarchy
        {
            int label_idx = -1;
            for (int i = 0; i < array_size(check->labels); ++i)
            {
                if (strcasecmp(check->labels[i].label, lookup_label) == 0)
                {
                    label_idx = i;
                    break;
                }
            }

            if (label_idx != -1)
            {
                label_t *state = &check->labels[label_idx];
                if (state->deleted)
                {
                    free(lookup_label);
                    return NULL;
                }

                if (state->tablepos + link->jumpoffset
                    >= array_size(StateTable))
                {
                    break;
                }
                free(lookup_label);
                return &StateTable[state->tablepos + link->jumpoffset];
            }

            if (check->parent == -1)
            {
                break;
            }
            check = &actorclasses[check->parent];
        } while (true);

        char *dot = strrchr(lookup_label, '.');
        if (dot)
        {
            *dot = '\0';
        }
        else
        {
            free(lookup_label);
            return NULL; // Not found
        }
    }
}

static void InstallStates(actor_t *actor)
{
    label_t *label = NULL;
    framelabel_t framelabel = {0};

    resolve_t *gotoresolves = NULL;

    int state_index = 0;
    while (state_index < array_size(actor->states))
    {
        dstate_t *state = &actor->states[state_index];
        bool new_label_found = false;

        // Find the label for this state
        array_foreach_type(alabel, actor->labels, label_t)
        {
            if (alabel->state == state_index)
            {
                label = alabel;
                FrameLabel_Reset(&framelabel, actor, alabel);
                alabel->tablepos = array_size(StateTable);
                new_label_found = true;
                break;
            }
        }

        int lblStart = array_size(StateTable);

        State tstate = {
            .sprite = strdup(state->sprite),
            .duration = state->duration,
            .action = state->action,
            .xoffset = state->xoffset,
            .yoffset = state->yoffset,
            .bright = state->bright
        };

        int frameslen = strlen(state->frames);
        for (int j = 0; j < frameslen; ++j)
        {
            tstate.label = FrameLabel_Assign(&framelabel);
            tstate.frame = state->frames[j] - 'A';
            if (j == frameslen - 1)
            {
                switch (state->next.sequence)
                {
                    case SEQ_Next:
                        tstate.next = array_size(StateTable) + 1;
                        break;
                    case SEQ_Wait:
                        tstate.next = array_size(StateTable);
                        break;
                    case SEQ_Stop:
                        tstate.next = 0;
                        break;
                    case SEQ_Loop:
                        tstate.next = lblStart;
                        break;
                    case SEQ_Goto:
                        {
                            resolve_t res = {
                                .statenum = array_size(StateTable),
                                .link = state->next
                            };
                            array_push(gotoresolves, res);
                            break;
                        }
                }
            }
            else
            {
                tstate.next = array_size(StateTable) + 1;
            }

            array_push(StateTable, tstate);
        }
        state_index++;
    }
    free(framelabel.label);

    array_foreach_type(resolve, gotoresolves, resolve_t)
    {
        State *state = &StateTable[resolve->statenum];
        statelink_t *link = &resolve->link;

        const State *nextstate = FindState(actor, link);
        if (nextstate == NULL)
        {
            I_Error("Could not resolve goto %s::%s+%d",
                    link->jumpclass ? link->jumpclass : "",
                    link->jumpstate, link->jumpoffset);
        }

        state->next = nextstate - StateTable;
        free(link->jumpclass);
        free(link->jumpstate);
    }
    array_free(gotoresolves);
}

static const State *FindStateByLabel(const actor_t *actor, const char *label)
{
    statelink_t link = {0};
    link.jumpstate = (char *)label;
    return FindState(actor, &link);
}

static void ParseStateJump(scanner_t *sc, dstate_t *state)
{
    SC_MustGetToken(sc, TK_Identifier);
    state->next.jumpstate = strdup(SC_GetString(sc));
    if (SC_CheckToken(sc, TK_ScopeResolution))
    {
        state->next.jumpclass = state->next.jumpstate;
        SC_MustGetToken(sc, TK_Identifier);
        state->next.jumpstate = strdup(SC_GetString(sc));
    }

    if (SC_CheckToken(sc, '+'))
    {
        SC_MustGetToken(sc, TK_IntConst);
        state->next.jumpoffset = SC_GetNumber(sc);
    }
}

static void ParseActorStates(scanner_t *sc, actor_t *actor)
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
                const char *label = SC_GetString(sc);
                const char *keywords[] = {"loop", "goto", "stop", "wait"};
                if (CheckKeyword(sc, keywords) >= 0 || !SC_CheckToken(sc, ':'))
                {
                    break;
                }
                else
                {
                    array_push(labels, strdup(label));
                }
            }
        } while (true);

        dstate_t state = {0};
        bool usestate = false;

        const char *keywords[] = {"loop", "goto", "stop", "wait"};

        enum
        {
            KEYWORD_Loop,
            KEYWORD_Goto,
            KEYWORD_Stop,
            KEYWORD_Wait
        };

        const char *stop_kwd[] = {"stop"};
        if (CheckKeyword(sc, stop_kwd) == 0)
        {
            SC_GetNextToken(sc, false); // Consume "stop"
            for (int i = 0; i < array_size(labels); ++i)
            {
                label_t new_label = {
                    .label = labels[i],
                    .deleted = true
                };
                array_push(actor->labels, new_label);
            }
            array_free(labels);
            continue;
        }

        switch (CheckKeyword(sc, keywords))
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
                // Not fully implemented as in original, complex logic with
                // labels
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
                state.sprite = strdup(SC_GetString(sc));
                M_StringToUpper(state.sprite);

                bool found = false;
                for (int i = 0; i < array_size(spritenames); ++i)
                {
                    if (strcmp(spritenames[i], state.sprite) == 0)
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    array_push(spritenames, strdup(state.sprite));
                }

                if (!SC_CheckToken(sc, TK_StringConst))
                {
                    SC_MustGetToken(sc, TK_Identifier);
                }
                state.frames = strdup(SC_GetString(sc));
                M_StringToUpper(state.frames);

                state.duration = GetNegativeInteger(sc);

                if (SC_CheckToken(sc, TK_Identifier))
                {
                    const char *state_keywords[] = {"bright", "offset"};
                    int keyword;
                    do
                    {
                        keyword = CheckKeyword(sc, state_keywords);
                        switch (keyword)
                        {
                            case 0:
                                state.bright = true;
                                break;
                            case 1:
                                SC_MustGetToken(sc, '(');
                                state.xoffset = GetNegativeInteger(sc);
                                SC_MustGetToken(sc, ',');
                                state.yoffset = GetNegativeInteger(sc);
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

                    if (strlen(SC_GetString(sc)) <= 4)
                    {
                        SC_Rewind(sc);
                        break;
                    }

                    const char *fname = SC_GetString(sc);
                    int func_idx = -1;
                    for (int i = 0; i < array_size(action_functions); ++i)
                    {
                        if (strcasecmp(action_functions[i].function, fname) == 0)
                        {
                            func_idx = i;
                            break;
                        }
                    }
                    if (func_idx == -1)
                    {
                        SC_Error(sc, "Unknown action function %s.", fname);
                    }
                    else
                    {
                        state.action = func_idx;
                    }

                    if (SC_CheckToken(sc, '('))
                    {
                        SC_MustGetToken(sc, ')');
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
                    new_label.label = labels[i],
                    new_label.state = array_size(actor->states) - 1
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

void ParseActorProperty(scanner_t *sc, actor_t *actor)
{
actor_t *check = actor;
const char *keywords[] = {"renderstyle", "translation"};
if (CheckKeyword(sc, keywords) >= 0)
{
    SC_Rewind(sc);
    SC_MustGetToken(sc, TK_Identifier);
}
switch (CheckKeyword(sc, keywords))
{
    case 0: // PROP_RenderStyle
        SC_MustGetToken(sc, TK_StringConst);
        const char *rstyle_keywords[] = {"none", "fuzzy"};
        switch (CheckKeyword(sc, rstyle_keywords))
        {
            case 0:
                actor->props.flags &= ~MF_SHADOW;
                break;
            case 1:
                actor->props.flags |= MF_SHADOW;
                break;
            default:
                SC_Error(sc, "Unknown render style '%s'.",
                         SC_GetString(sc));
                break;
        }
        break;
    case 1: // PROP_Translation
        SC_MustGetToken(sc, TK_IntConst);
        if (SC_GetNumber(sc) > 3)
        {
            SC_Error(sc, "Translation out of range.");
        }
        actor->props.flags = (actor->props.flags
                              & (~(MF_TRANSLATION1 | MF_TRANSLATION2)))
                             | (SC_GetNumber(sc) * MF_TRANSLATION1);
        break;
    default:
        do
        {
            const char *propname = SC_GetString(sc);
            int prop_idx = -1;
            for (int i = 0; i < array_size(check->properties); ++i)
            {
                if (strcasecmp(check->properties[i].name, propname)
                    == 0)
                {
                    prop_idx = i;
                    break;
                }
            }

            if (prop_idx != -1)
            {
                property_t *prop = &check->properties[prop_idx];
                propvalue_t val = {0};
                switch (prop->type)
                {
                    case TYPE_Int:
                        val.number = GetNegativeInteger(sc);
                        hashmap_put(actor->props.props, prop->name,
                                    &val);
                        break;
                    case TYPE_Fixed:
                        SC_MustGetToken(sc, TK_FloatConst);
                        val.number = SC_GetDecimal(sc) * 0x10000;
                        hashmap_put(actor->props.props, prop->name,
                                    &val);
                        break;
                    case TYPE_Sound:
                        printf("TYPE_Sound");
                        SC_MustGetToken(sc, TK_StringConst);
                        val.number = LookupSound(SC_GetString(sc));
                        if (SC_GetString(sc)[0] != '\0'
                            && val.number == 0)
                        {
                            SC_Error(sc, "Sound '%s' is not defined.",
                                     SC_GetString(sc));
                        }
                        hashmap_put(actor->props.props, prop->name,
                                    &val);
                        break;
                    case TYPE_Flags:
                    case TYPE_State:
                        SC_Error(sc, "Property is automatically "
                                     "assigned by other means.");
                        break;
                }
               return;
            }

            if (check->parent == -1)
            {
                break;
            }
            check = &actorclasses[check->parent];
        } while (true);

        SC_Warning(sc, "Unknown property '%s'.", SC_GetString(sc));
        break;
    }
}

static void ParseActorFlag(scanner_t *sc, actor_t *actor, bool set)
{
    SC_MustGetToken(sc, TK_Identifier);
    char *part1 = strdup(SC_GetString(sc));
    M_StringToUpper(part1);

    for (unsigned int i = 0; Flags[i].name; ++i)
    {
        if (strcasecmp(part1, Flags[i].name) == 0)
        {
            if (set)
            {
                actor->props.flags |= Flags[i].flag;
            }
            else
            {
                actor->props.flags &= ~Flags[i].flag;
            }
            free(part1);
            return;
        }
    }
    SC_Warning(sc, "Unknown flag '%s'.", part1);
    free(part1);
}

void ParseActionFunction(scanner_t *sc, actor_t *actor)
{
    SC_MustGetToken(sc, TK_Identifier);
    const char *keywords[] = {"native"};
    RequireKeyword(sc, keywords);

    SC_MustGetToken(sc, TK_Identifier);
    action_t new_action = {strdup(SC_GetString(sc)), actor->classNum};
    array_push(action_functions, new_action);

    SC_MustGetToken(sc, '(');
    SC_MustGetToken(sc, ')');
    SC_MustGetToken(sc, ';');
}

void ParseActorAnnotation(scanner_t *sc, actor_t *actor)
{
    while (!SC_CheckToken(sc, TK_AnnotateEnd))
    {
        SC_MustGetToken(sc, TK_Identifier);
        const char *keywords[] = {"native"};
        RequireKeyword(sc, keywords);

        property_t prop = {0};
        int type;

        SC_MustGetToken(sc, TK_Identifier);
        const char *prop_keywords[] = {"property"};
        if (CheckKeyword(sc, prop_keywords) == 0)
        {
            SC_MustGetToken(sc, TK_Identifier);
            const char *type_keywords[] = {"int", "fixed", "sound", "flags",
                                           "state"};
            if ((type = CheckKeyword(sc, type_keywords)) >= 0)
            {
                prop.type = type;
            }
            else
            {
                SC_Error(sc, "Unknown type '%s'.", SC_GetString(sc));
            }

            SC_MustGetToken(sc, TK_Identifier);
            prop.codename = prop.name = strdup(SC_GetString(sc));
            if (SC_CheckToken(sc, '<'))
            {
                SC_MustGetToken(sc, TK_Identifier);
                free(prop.codename);
                prop.codename = strdup(SC_GetString(sc));
                SC_MustGetToken(sc, '>');
            }
            else if (type == TYPE_State)
            {
                char *old_codename = prop.codename;
                prop.codename = malloc(strlen(old_codename) + 6);
                sprintf(prop.codename, "%sstate", old_codename);
                free(old_codename);
            }
            SC_MustGetToken(sc, ';');

            array_push(actor->properties, prop);
        }
        else
        {
            SC_Error(sc, "Unknown data type '%s'.", SC_GetString(sc));
        }
    }
}

void ParseActorBody(scanner_t *sc, actor_t *actor)
{
    SC_MustGetToken(sc, '{');
    while (!SC_CheckToken(sc, '}'))
    {
        if (SC_CheckToken(sc, '+'))
        {
            ParseActorFlag(sc, actor, true);
        }
        else if (SC_CheckToken(sc, '-'))
        {
            ParseActorFlag(sc, actor, false);
        }
        else if (SC_CheckToken(sc, TK_AnnotateStart))
        {
            ParseActorAnnotation(sc, actor);
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
                const char *keywords[] = {"action", "states"};
                switch (CheckKeyword(sc, keywords))
                {
                    case 0:
                        ParseActionFunction(sc, actor);
                        break;
                    case 1:
                        ParseActorStates(sc, actor);
                        break;
                    default:
                        ParseActorProperty(sc, actor);
                        break;
                }
            }
        }
    }
}

// This is the root actor. Add base properties.
static property_t base_props[] = {
    {TYPE_Int, "health", "spawnhealth"},
    {TYPE_Sound, "seesound", "seesound"},
    {TYPE_Int, "reactiontime", "reactiontime"},
    {TYPE_Sound, "attacksound", "attacksound"},
    {TYPE_Int, "painchance", "painchance"},
    {TYPE_Sound, "painsound", "painsound"},
    {TYPE_Sound, "deathsound", "deathsound"},
    {TYPE_Int, "speed", "speed"},
    {TYPE_Fixed, "radius", "radius"},
    {TYPE_Fixed, "height", "height"},
    {TYPE_Int, "mass", "mass"},
    {TYPE_Int, "damage", "damage"},
    {TYPE_Sound, "activesound", "activesound"},
    {TYPE_State, "Spawn", "spawnstate"},
    {TYPE_State, "See", "seestate"},
    {TYPE_State, "Pain", "painstate"},
    {TYPE_State, "Melee", "meleestate"},
    {TYPE_State, "Missile", "missilestate"},
    {TYPE_State, "Death", "deathstate"},
    {TYPE_State, "XDeath", "xdeathstate"},
    {TYPE_State, "Raise", "raisestate"},
};

void ParseActor(scanner_t *sc)
{
    actor_t actor = {0};
    actor.classNum = array_size(actorclasses);
    actor.parent = -1;
    actor.doomednum = -1;
    actor.props.props = hashmap_init(16);

    SC_MustGetToken(sc, TK_Identifier);
    actor.name = strdup(SC_GetString(sc));

    if (actor.classNum == 0 && strcasecmp(actor.name, "Actor") == 0)
    {
        for (unsigned i = 0; i < arrlen(base_props); ++i)
        {
            property_t p = { base_props[i].type, strdup(base_props[i].name), strdup(base_props[i].codename) };
            array_push(actor.properties, p);
        }
    }

    if (SC_CheckToken(sc, ':'))
    {
        SC_MustGetToken(sc, TK_Identifier);
        const char *parentName = SC_GetString(sc);
        int parent_idx = -1;
        for (int i = 0; i < array_size(actorclasses); ++i)
        {
            if (strcasecmp(actorclasses[i].name, parentName) == 0)
            {
                parent_idx = i;
                break;
            }
        }
        if (parent_idx == -1)
        {
            SC_Error(sc, "Unknown parent actor '%s'.", parentName);
        }

        actor.parent = parent_idx;
        actor.props.flags = actorclasses[parent_idx].props.flags;
        // Deep copy of properties would be needed here if they were complex
    }
    else if (actor.classNum != 0)
    {
        actor.props.flags = actorclasses[0].props.flags;
        actor.parent = 0;
    }

    const char *replaces_kwd[] = {"replaces"};
    if (CheckKeyword(sc, replaces_kwd) == 0)
    {
        SC_GetNextToken(sc, false); // consume "replaces"
        SC_MustGetToken(sc, TK_Identifier);
        actor.replaces = strdup(SC_GetString(sc));
    }

    if (SC_CheckToken(sc, TK_IntConst))
    {
        actor.doomednum = SC_GetNumber(sc);
    }

    const char *native_kwd[] = {"native"};
    if (CheckKeyword(sc, native_kwd) == 0)
    {
        actor.native = true;
        SC_GetNextToken(sc, false);
    }

    if (SC_CheckToken(sc, TK_AnnotateStart))
    {
        if (SC_CheckToken(sc, TK_Identifier))
        {
            actor.codename = strdup(SC_GetString(sc));
        }
        SC_MustGetToken(sc, TK_AnnotateEnd);
    }
    else
    {
        actor.codename = strdup(actor.name);
    }
    M_StringToUpper(actor.codename);

    array_push(actorclasses, actor);
    ParseActorBody(sc, &actorclasses[array_size(actorclasses) - 1]);
}

void ParseDecorate(scanner_t *sc)
{
    while (SC_TokensLeft(sc))
    {
        if (SC_CheckToken(sc, TK_AnnotateStart))
        {
            SC_MustGetToken(sc, TK_Identifier);
            const char *keywords[] = {"soundmap"};
            RequireKeyword(sc, keywords);
            SC_MustGetToken(sc, '{');
            do
            {
                SC_MustGetToken(sc, TK_StringConst);
                char *logical = strdup(SC_GetString(sc));
                SC_MustGetToken(sc, '<');
                SC_MustGetToken(sc, TK_Identifier);
                char *codename = strdup(SC_GetString(sc));
                SC_MustGetToken(sc, '>');
                sound_mapping_t mapping = {codename, logical};
                array_push(soundtable, mapping);
            } while (SC_CheckToken(sc, ','));
            SC_MustGetToken(sc, '}');
            SC_MustGetToken(sc, TK_AnnotateEnd);
        }
        else
        {
            SC_MustGetToken(sc, TK_Identifier);
            const char *keywords[] = {"actor"};
            RequireKeyword(sc, keywords);
            ParseActor(sc);
        }
    }
}

static void PrintMobjInfo(mobjinfo_t *info)
{
    printf("\
    doomednum: %d\n\
    spawnstate: %d\n\
    spawnhealth: %d\n\
    seestate: %d\n\
    seesound: %d\n\
    reactiontime: %d\n\
    attacksound: %d\n\
    painstate: %d\n\
    painchance: %d\n\
    painsound: %d\n\
    meleestate: %d\n\
    missilestate: %d\n\
    deathstate: %d\n\
    xdeathstate: %d\n\
    deathsound: %d\n\
    speed: %d\n\
    radius: %d\n\
    height: %d\n\
    mass: %d\n\
    damage: %d\n\
    activesound: %d\n\
    flags: %d\n\
    raisestate: %d\n\
    droppeditem: %d\n\
    flags2: %d\n\
    infighting_group: %d\n\
    projectile_group: %d\n\
    splash_group: %d\n\
    ripsound: %d\n\
    altspeed: %d\n\
    meleerange: %d\n\
    flags_extra: %d\n\
    bloodcolor: %d\n",
    info->doomednum,
    info->spawnstate,
    info->spawnhealth,
    info->seestate,
    info->seesound,
    info->reactiontime,
    info->attacksound,
    info->painstate,
    info->painchance,
    info->painsound,
    info->meleestate,
    info->missilestate,
    info->deathstate,
    info->xdeathstate,
    info->deathsound,
    info->speed,
    info->radius,
    info->height,
    info->mass,
    info->damage,
    info->activesound,
    info->flags,
    info->raisestate,
    info->droppeditem,
    info->flags2,
    info->infighting_group,
    info->projectile_group,
    info->splash_group,
    info->ripsound,
    info->altspeed,
    info->meleerange,
    info->flags_extra,
    info->bloodcolor);

    // const char *obituary;
    // const char *obituary_melee;
    // const char *obituary_self;
}

static void PrintStates(dstate_t *dstates)
{
    array_foreach_type(dstate, dstates, dstate_t)
    {
        printf("\
            sprite: %s\n\
            frames: %s\n\
            action: %d\n\
            duration: %d\n\
            xoffset: %d\n\
            yoffset: %d\n\
            bright: %d\n\
            ",
            dstate->sprite,
            dstate->frames,
            dstate->action,
            dstate->duration,
            dstate->xoffset,
            dstate->yoffset,
            dstate->bright);
    }
}

static void DECL_Integrate(void)
{
    if (array_size(actorclasses) == 0)
    {
        return;
    }

    // A mapping from our classNum to the final mobjtype_t
    mobjtype_t *classMap =
        malloc(sizeof(mobjtype_t) * array_size(actorclasses));
    if (!classMap)
    {
        I_Error("Could not allocate classMap");
    }
    for (int i = 0; i < array_size(actorclasses); ++i)
    {
        classMap[i] = MT_NULL;
    }

    for (int i = 0; i < array_size(actorclasses); ++i)
    {
        actor_t *actor = &actorclasses[i];
        if (actor->native)
        {
            continue;
        }

        mobjtype_t new_mobjtype = DSDH_MobjInfoGetNewIndex();
        classMap[i] = new_mobjtype;

        mobjinfo_t *mobjinfo_ptr = &mobjinfo[new_mobjtype];
        memset(mobjinfo_ptr, 0, sizeof(*mobjinfo_ptr));

        // Inherit from parent
        if (actor->parent != -1)
        {
            mobjtype_t parent_mobjtype = classMap[actor->parent];
            if (parent_mobjtype != MT_NULL)
            {
                // This is shallow copy, which is what dsdh does.
                *mobjinfo_ptr = mobjinfo[parent_mobjtype];
            }
        }

        mobjinfo_ptr->doomednum = actor->doomednum;
        mobjinfo_ptr->flags = actor->props.flags;

        propvalue_t val;
        if (hashmap_get(actor->props.props, "health", &val))
        {
            mobjinfo_ptr->spawnhealth = val.number;
        }
        if (hashmap_get(actor->props.props, "radius", &val))
        {
            mobjinfo_ptr->radius = val.number;
        }
        if (hashmap_get(actor->props.props, "height", &val))
        {
            mobjinfo_ptr->height = val.number;
        }
        if (hashmap_get(actor->props.props, "mass", &val))
        {
            mobjinfo_ptr->mass = val.number;
        }
        if (hashmap_get(actor->props.props, "speed", &val))
        {
            mobjinfo_ptr->speed = val.number;
        }
        if (hashmap_get(actor->props.props, "damage", &val))
        {
            mobjinfo_ptr->damage = val.number;
        }

        printf("\n%s\n", actor->name);

        PrintMobjInfo(mobjinfo_ptr);

        printf("\nstates:\n");

        PrintStates(actor->states);
    }

    // TODO: States, sprites and sounds

    free(classMap);
}

void DECL_Parse(int lumpnum)
{
    scanner_t *sc = SC_Open("DECORATE", W_CacheLumpNum(lumpnum, PU_CACHE),
                            W_LumpLength(lumpnum));

    ParseDecorate(sc);

    SC_Close(sc);

    DECL_Integrate();
}

//
// Copyright(C) 2025 Roman Fomin
// Copyright(C) 2026 Fabian Greffrath
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

#include "am_map.h"
#include "d_player.h"
#include "d_think.h"
#include "doomdata.h"
#include "doomstat.h"
#include "doomtype.h"
#include "i_printf.h"
#include "i_system.h"
#include "info.h"
#include "m_arena.h"
#include "m_array.h"
#include "m_fixed.h"
#include "m_json.h"
#include "m_random.h"
#include "p_ambient.h"
#include "p_dirty.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "p_saveg.h"
#include "p_setup.h"
#include "p_spec.h"
#include "p_tick.h"
#include "r_defs.h"
#include "r_state.h"

#include "miniz.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static json_mut_doc_t *doc;
static json_mut_t *root_mut;
static json_t *root;

// Pointer fields (mobj_t*, thinker_t*, msecnode_t*, …) cannot be stored
// directly in a savegame.  Instead every live pointer is mapped to an integer
// index into one of the four pointer tables below.  Special negative sentinel
// values are reserved for pointers that do not belong to any table:
enum
{
    null_index = -1,       // NULL pointer
    head_index = -2,       // &thinkercap
    dummy_index = -3,      // P_SubstNullMobj(NULL)
    th_delete_index = -4,  // &thinkerclasscap[th_delete]
    th_misc_index = -5,    // &thinkerclasscap[th_misc]
    th_friends_index = -6, // &thinkerclasscap[th_friends]
    th_enemies_index = -7, // &thinkerclasscap[th_enemies]
};

// Maps each thinker class cap (th_delete, th_misc, …) to its sentinel index.
static int tclass_to_index[] = {
    [th_delete] = th_delete_index,
    [th_misc] = th_misc_index,
    [th_friends] = th_friends_index,
    [th_enemies] = th_enemies_index,
};

// Every concrete thinker type that can appear in a savegame.
// tc_none covers thinkers with unknown or NULL function pointers.
// The _del variants are thinkers already queued for removal.
typedef enum
{
    tc_mobj,
    tc_mobj_del,
    tc_ceiling,
    tc_ceiling_del,
    tc_door,
    tc_door_del,
    tc_floor,
    tc_floor_del,
    tc_plat,
    tc_plat_del,
    tc_flash,
    tc_strobe,
    tc_glow,
    tc_elevator,
    tc_elevator_del,
    tc_scroll,
    tc_pusher,
    tc_flicker,
    tc_friction,
    tc_ambient,
    tc_param_scroll_floor,
    tc_param_scroll_ceiling,
    tc_none
} thinker_class_t;

// Maps each thinker_class_t back to its tick function, used when restoring
// thinkers from a savegame (the function pointer is never written to disk).
static actionf_p1 actions[] = {
    [tc_mobj] = P_MobjThinker,
    [tc_mobj_del] = P_RemoveMobjThinkerDelayed,
    [tc_ceiling] = T_MoveCeilingAdapter,
    [tc_ceiling_del] = P_RemoveCeilingThinkerDelayed,
    [tc_door] = T_VerticalDoorAdapter,
    [tc_door_del] = P_RemoveDoorThinkerDelayed,
    [tc_floor] = T_MoveFloorAdapter,
    [tc_floor_del] = P_RemoveFloorThinkerDelayed,
    [tc_plat] = T_PlatRaiseAdapter,
    [tc_plat_del] = P_RemovePlatThinkerDelayed,
    [tc_flash] = T_LightFlashAdapter,
    [tc_strobe] = T_StrobeFlashAdapter,
    [tc_glow] = T_GlowAdapter,
    [tc_elevator] = T_MoveElevatorAdapter,
    [tc_elevator_del] = P_RemoveElevatorThinkerDelayed,
    [tc_scroll] = T_ScrollAdapter,
    [tc_pusher] = T_PusherAdapter,
    [tc_flicker] = T_FireFlickerAdapter,
    [tc_friction] = T_FrictionAdapter,
    [tc_ambient] = T_AmbientSoundAdapter,
    [tc_param_scroll_floor] = T_ParamScrollFloorAdapter,
    [tc_param_scroll_ceiling] = T_ParamScrollCeilingAdapter,
    [tc_none] = NULL};

// One entry per live thinker, built by PrepareArchiveThinkers() before saving
// and by PrepareUnArchiveThinkers() before loading.  The index of an entry in
// this array is what gets written to / read from the JSON for any field that
// holds a thinker pointer.
typedef struct
{
    thinker_class_t tc;

    union
    {
        thinker_t *thinker;
        mobj_t *mobj;
        ceiling_t *ceiling;
        vldoor_t *door;
        floormove_t *floor;
        plat_t *plat;
        lightflash_t *flash;
        strobe_t *strobe;
        glow_t *glow;
        elevator_t *elevator;
        scroll_t *scroll;
        pusher_t *pusher;
        fireflicker_t *flicker;
        friction_t *friction;
        ambient_t *ambient;
        uintptr_t integer;
    } p;
} thinker_pointer_t;

// Pointer tables populated during Prepare* and consumed during UnArchive*.
// Indices stored in the JSON refer into these arrays.
static thinker_pointer_t *thinker_pointers;
static uintptr_t *msecnode_pointers;
static uintptr_t *ceilinglist_pointers;
static uintptr_t *platlist_pointers;

// Convenience macros for pointer-as-array-index fields (subsector, sector,
// state, line, …).  The pointer is converted to/from an offset into its
// respective base array.  A null_index round-trips to NULL.
#define JS_GetIdx(ptr, base, obj, key)                         \
    do                                                         \
    {                                                          \
        int index = JS_GetIntegerValue(obj, key);              \
        (ptr) = (index != null_index) ? (base) + index : NULL; \
    } while (0)

#define JS_SetIdx(doc, obj, key, ptr, base)                             \
    do                                                                  \
    {                                                                   \
        JS_SetInt(doc, obj, key, ptr ? (int)(ptr - base) : null_index); \
    } while (0)

inline static thinker_t *readp_thinker_index(int index)
{
    if (index == null_index)
    {
        return NULL;
    }
    else if (index == head_index)
    {
        return &thinkercap;
    }
    else if (index == dummy_index)
    {
        return (thinker_t *)P_SubstNullMobj(NULL);
    }
    else
    {
        if (index < 0 || index >= array_size(thinker_pointers))
        {
            I_Error("Wrong index: %d", index);
        }
        return thinker_pointers[index].p.thinker;
    }
}

static thinker_t *readp_thinker(int index)
{
    return readp_thinker_index(index);
}

static int writep_thinker(const thinker_t *thinker)
{
    int index;
    if (!thinker)
    {
        index = null_index;
    }
    else if (thinker == &thinkercap)
    {
        index = head_index;
    }
    else if (thinker == (thinker_t *)P_SubstNullMobj(NULL))
    {
        index = dummy_index;
    }
    else
    {
        index = M_ArenaTableIndex(thinkers_arena, (uintptr_t)thinker);
    }
    return index;
}

static mobj_t *readp_mobj(int index)
{
    return (mobj_t *)readp_thinker(index);
}

static int writep_mobj(const mobj_t *mobj)
{
    return mobj ? writep_thinker(&mobj->thinker) : null_index;
}

static msecnode_t *readp_msecnode(int index)
{
    if (index == null_index)
    {
        return NULL;
    }
    if (index < 0 || index >= array_size(msecnode_pointers))
    {
        I_Error("Wrong index: %d", index);
    }
    return (msecnode_t *)msecnode_pointers[index];
}

static int writep_msecnode(const msecnode_t *node)
{
    int index;
    if (!node)
    {
        index = null_index;
    }
    else
    {
        index = M_ArenaTableIndex(msecnodes_arena, (uintptr_t)node);
    }
    return index;
}

static ceilinglist_t *readp_activeceilings(int index)
{
    if (index == null_index)
    {
        return NULL;
    }
    if (index < 0 || index >= array_size(ceilinglist_pointers))
    {
        I_Error("Wrong index: %d", index);
    }
    return (ceilinglist_t *)ceilinglist_pointers[index];
}

static int writep_activeceilings(const ceilinglist_t *cl)
{
    int index;
    if (!cl)
    {
        index = null_index;
    }
    else
    {
        index = M_ArenaTableIndex(activeceilings_arena, (uintptr_t)cl);
    }
    return index;
}

static platlist_t *readp_activeplats(int index)
{
    if (index == null_index)
    {
        return NULL;
    }
    if (index < 0 || index >= array_size(platlist_pointers))
    {
        I_Error("Wrong index: %d", index);
    }
    return (platlist_t *)platlist_pointers[index];
}

static int writep_activeplats(const platlist_t *pl)
{
    int index;
    if (!pl)
    {
        index = null_index;
    }
    else
    {
        index = M_ArenaTableIndex(activeplats_arena, (uintptr_t)pl);
    }
    return index;
}

static void read_mapthing_t(mapthing_t *str, json_t *obj)
{
    str->x = JS_GetIntegerValue(obj, "x");
    str->y = JS_GetIntegerValue(obj, "y");
    str->height = JS_GetIntegerValue(obj, "height");
    str->angle = JS_GetIntegerValue(obj, "angle");
    str->type = JS_GetIntegerValue(obj, "type");
    str->options = JS_GetIntegerValue(obj, "options");
}

static json_mut_t *write_mapthing_t(mapthing_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    JS_SetInt(doc, obj, "x", str->x);
    JS_SetInt(doc, obj, "y", str->y);
    JS_SetInt(doc, obj, "height", str->height);
    JS_SetInt(doc, obj, "angle", str->angle);
    JS_SetInt(doc, obj, "type", str->type);
    JS_SetInt(doc, obj, "options", str->options);

    return obj;
}

static thinker_t *readp_thclass(int index)
{
    for (int tclass = 0; tclass < NUMTHCLASS; ++tclass)
    {
        if (index == tclass_to_index[tclass])
        {
            return &thinkerclasscap[tclass];
        }
    }
    return readp_thinker_index(index);
}

static int writep_thclass(thinker_t *str)
{
    int tclass;
    for (tclass = 0; tclass < NUMTHCLASS; ++tclass)
    {
        if (str == &thinkerclasscap[tclass])
        {
            return tclass_to_index[tclass];
        }
    }
    return writep_thinker(str);
}

static void read_thinker_t(thinker_t *str, thinker_class_t tc, json_t *obj)
{
    str->prev = readp_thinker(JS_GetIntegerValue(obj, "prev"));
    str->next = readp_thinker(JS_GetIntegerValue(obj, "next"));
    str->function.p1 = actions[tc];
    str->cnext = readp_thclass(JS_GetIntegerValue(obj, "cnext"));
    str->cprev = readp_thclass(JS_GetIntegerValue(obj, "cprev"));
    str->references = JS_GetIntegerValue(obj, "references");
}

static json_mut_t *write_thinker_t(thinker_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    JS_SetInt(doc, obj, "prev", writep_thinker(str->prev));
    JS_SetInt(doc, obj, "next", writep_thinker(str->next));
    JS_SetInt(doc, obj, "cnext", writep_thclass(str->cnext));
    JS_SetInt(doc, obj, "cprev", writep_thclass(str->cprev));
    JS_SetInt(doc, obj, "references", str->references);

    return obj;
}

static void read_mobj_t(mobj_t *str, thinker_class_t tc, json_t *obj)
{
    read_thinker_t(&str->thinker, tc, JS_GetObject(obj, "thinker"));

    str->x = JS_GetIntegerValue(obj, "x");
    str->y = JS_GetIntegerValue(obj, "y");
    str->z = JS_GetIntegerValue(obj, "z");

    // Done in UnArchiveThingList
    // str->snext;
    // str->sprev;

    str->angle = JS_GetIntegerValue(obj, "angle");
    str->sprite = JS_GetIntegerValue(obj, "sprite");
    str->frame = JS_GetIntegerValue(obj, "frame");

    // Done in UnArchiveBlocklinks
    // str->bnext;
    // str->bprev;

    JS_GetIdx(str->subsector, subsectors, obj, "subsector");
    str->floorz = JS_GetIntegerValue(obj, "floorz");
    str->ceilingz = JS_GetIntegerValue(obj, "ceilingz");
    str->dropoffz = JS_GetIntegerValue(obj, "dropoffz");
    str->radius = JS_GetIntegerValue(obj, "radius");
    str->height = JS_GetIntegerValue(obj, "height");
    str->momx = JS_GetIntegerValue(obj, "momx");
    str->momy = JS_GetIntegerValue(obj, "momy");
    str->momz = JS_GetIntegerValue(obj, "momz");
    str->validcount = JS_GetIntegerValue(obj, "validcount");
    str->type = JS_GetIntegerValue(obj, "type");
    JS_GetIdx(str->info, mobjinfo, obj, "info");
    str->tics = JS_GetIntegerValue(obj, "tics");
    JS_GetIdx(str->state, states, obj, "state");
    str->flags = JS_GetIntegerValue(obj, "flags");
    str->flags2 = JS_GetIntegerValue(obj, "flags2");
    str->flags_extra = JS_GetIntegerValue(obj, "flags_extra");
    str->intflags = JS_GetIntegerValue(obj, "intflags");
    str->health = JS_GetIntegerValue(obj, "health");
    str->tid = JS_GetIntegerValue(obj, "tid");
    str->special = JS_GetIntegerValue(obj, "special");

    json_t *args_arr = JS_GetObject(obj, "args");
    for (int i = 0; i < 5; ++i)
    {
        str->args[i] = JS_GetInteger(JS_GetArrayItem(args_arr, i));
    }

    str->movedir = JS_GetIntegerValue(obj, "movedir");
    str->movecount = JS_GetIntegerValue(obj, "movecount");
    str->strafecount = JS_GetIntegerValue(obj, "strafecount");
    str->target = readp_mobj(JS_GetIntegerValue(obj, "target"));
    str->reactiontime = JS_GetIntegerValue(obj, "reactiontime");
    str->threshold = JS_GetIntegerValue(obj, "threshold");
    str->pursuecount = JS_GetIntegerValue(obj, "pursuecount");
    str->gear = JS_GetIntegerValue(obj, "gear");
    JS_GetIdx(str->player, players, obj, "player");
    str->lastlook = JS_GetIntegerValue(obj, "lastlook");

    read_mapthing_t(&str->spawnpoint, JS_GetObject(obj, "spawnpoint"));

    str->tracer = readp_mobj(JS_GetIntegerValue(obj, "tracer"));
    str->lastenemy = readp_mobj(JS_GetIntegerValue(obj, "lastenemy"));
    str->above_thing = readp_mobj(JS_GetIntegerValue(obj, "above_thing"));
    str->below_thing = readp_mobj(JS_GetIntegerValue(obj, "below_thing"));
    str->friction = JS_GetIntegerValue(obj, "friction");
    str->movefactor = JS_GetIntegerValue(obj, "movefactor");
    str->touching_sectorlist =
        readp_msecnode(JS_GetIntegerValue(obj, "touching_sectorlist"));
    str->interp = JS_GetIntegerValue(obj, "interp");
    str->oldx = JS_GetIntegerValue(obj, "oldx");
    str->oldy = JS_GetIntegerValue(obj, "oldy");
    str->oldz = JS_GetIntegerValue(obj, "oldz");
    str->oldangle = JS_GetIntegerValue(obj, "oldangle");
    str->bloodcolor = JS_GetIntegerValue(obj, "bloodcolor");

    // TODO
    // P_SetActualHeight(str);
}

static json_mut_t *write_mobj_t(mobj_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    JS_SetObject(doc, obj, "thinker", write_thinker_t(&str->thinker));

    JS_SetInt(doc, obj, "x", str->x);
    JS_SetInt(doc, obj, "y", str->y);
    JS_SetInt(doc, obj, "z", str->z);

    // Done in ArchiveThingList
    // str->snext;
    // str->sprev;

    JS_SetInt(doc, obj, "angle", str->angle);
    JS_SetInt(doc, obj, "sprite", str->sprite);
    JS_SetInt(doc, obj, "frame", str->frame);

    // Done in ArchiveBlocklinks
    // str->bnext;
    // str->bprev;

    JS_SetIdx(doc, obj, "subsector", str->subsector, subsectors);
    JS_SetInt(doc, obj, "floorz", str->floorz);
    JS_SetInt(doc, obj, "ceilingz", str->ceilingz);
    JS_SetInt(doc, obj, "dropoffz", str->dropoffz);
    JS_SetInt(doc, obj, "radius", str->radius);
    JS_SetInt(doc, obj, "height", str->height);
    JS_SetInt(doc, obj, "momx", str->momx);
    JS_SetInt(doc, obj, "momy", str->momy);
    JS_SetInt(doc, obj, "momz", str->momz);
    JS_SetInt(doc, obj, "validcount", str->validcount);
    JS_SetInt(doc, obj, "type", str->type);
    JS_SetIdx(doc, obj, "info", str->info, mobjinfo);
    JS_SetInt(doc, obj, "tics", str->tics);
    JS_SetIdx(doc, obj, "state", str->state, states);
    JS_SetInt(doc, obj, "flags", str->flags);
    JS_SetInt(doc, obj, "flags2", str->flags2);
    JS_SetInt(doc, obj, "flags_extra", str->flags_extra);
    JS_SetInt(doc, obj, "intflags", str->intflags);
    JS_SetInt(doc, obj, "health", str->health);
    JS_SetInt(doc, obj, "tid", str->tid);
    JS_SetInt(doc, obj, "special", str->special);

    json_mut_t *args_arr = JS_NewArray(doc);
    for (int i = 0; i < 5; ++i)
    {
        JS_ArrayAddInt(doc, args_arr, str->args[i]);
    }
    JS_SetArray(doc, obj, "args", args_arr);

    JS_SetInt(doc, obj, "movedir", str->movedir);
    JS_SetInt(doc, obj, "movecount", str->movecount);
    JS_SetInt(doc, obj, "strafecount", str->strafecount);
    JS_SetInt(doc, obj, "target", writep_mobj(str->target));
    JS_SetInt(doc, obj, "reactiontime", str->reactiontime);
    JS_SetInt(doc, obj, "threshold", str->threshold);
    JS_SetInt(doc, obj, "pursuecount", str->pursuecount);
    JS_SetInt(doc, obj, "gear", str->gear);
    JS_SetIdx(doc, obj, "player", str->player, players);
    JS_SetInt(doc, obj, "lastlook", str->lastlook);

    JS_SetObject(doc, obj, "spawnpoint", write_mapthing_t(&str->spawnpoint));

    JS_SetInt(doc, obj, "tracer", writep_mobj(str->tracer));
    JS_SetInt(doc, obj, "lastenemy", writep_mobj(str->lastenemy));
    JS_SetInt(doc, obj, "above_thing", writep_mobj(str->above_thing));
    JS_SetInt(doc, obj, "below_thing", writep_mobj(str->below_thing));
    JS_SetInt(doc, obj, "friction", str->friction);
    JS_SetInt(doc, obj, "movefactor", str->movefactor);
    JS_SetInt(doc, obj, "touching_sectorlist",
              writep_msecnode(str->touching_sectorlist));
    JS_SetInt(doc, obj, "interp", str->interp);
    JS_SetInt(doc, obj, "oldx", str->oldx);
    JS_SetInt(doc, obj, "oldy", str->oldy);
    JS_SetInt(doc, obj, "oldz", str->oldz);
    JS_SetInt(doc, obj, "oldangle", str->oldangle);
    JS_SetInt(doc, obj, "bloodcolor", str->bloodcolor);

    return obj;
}

static void read_ticcmd_t(ticcmd_t *str, json_t *obj)
{
    str->forwardmove = JS_GetIntegerValue(obj, "forwardmove");
    str->sidemove = JS_GetIntegerValue(obj, "sidemove");
    str->angleturn = JS_GetIntegerValue(obj, "angleturn");
    str->consistancy = JS_GetIntegerValue(obj, "consistancy");
    str->chatchar = JS_GetIntegerValue(obj, "chatchar");
    str->buttons = JS_GetIntegerValue(obj, "buttons");
}

static json_mut_t *write_ticcmd_t(ticcmd_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    JS_SetInt(doc, obj, "forwardmove", str->forwardmove);
    JS_SetInt(doc, obj, "sidemove", str->sidemove);
    JS_SetInt(doc, obj, "angleturn", str->angleturn);
    JS_SetInt(doc, obj, "consistancy", str->consistancy);
    JS_SetInt(doc, obj, "chatchar", str->chatchar);
    JS_SetInt(doc, obj, "buttons", str->buttons);

    return obj;
}

static void read_pspdef_t(pspdef_t *str, json_t *obj)
{
    JS_GetIdx(str->state, states, obj, "state");
    str->tics = JS_GetIntegerValue(obj, "tics");
    str->sx = JS_GetIntegerValue(obj, "sx");
    str->sy = JS_GetIntegerValue(obj, "sy");
    str->sx2 = JS_GetIntegerValue(obj, "sx2");
    str->sy2 = JS_GetIntegerValue(obj, "sy2");
    str->oldsx2 = JS_GetIntegerValue(obj, "oldsx2");
    str->oldsy2 = JS_GetIntegerValue(obj, "oldsy2");
    str->sxf = JS_GetIntegerValue(obj, "sxf");
    str->syf = JS_GetIntegerValue(obj, "syf");
}

static json_mut_t *write_pspdef_t(pspdef_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    JS_SetIdx(doc, obj, "state", str->state, states);
    JS_SetInt(doc, obj, "tics", str->tics);
    JS_SetInt(doc, obj, "sx", str->sx);
    JS_SetInt(doc, obj, "sy", str->sy);
    JS_SetInt(doc, obj, "sx2", str->sx2);
    JS_SetInt(doc, obj, "sy2", str->sy2);
    JS_SetInt(doc, obj, "oldsx2", str->oldsx2);
    JS_SetInt(doc, obj, "oldsy2", str->oldsy2);
    JS_SetInt(doc, obj, "sxf", str->sxf);
    JS_SetInt(doc, obj, "syf", str->syf);

    return obj;
}

static void read_player_t(player_t *str, json_t *obj)
{
    str->mo = readp_mobj(JS_GetIntegerValue(obj, "mo"));
    str->playerstate = JS_GetIntegerValue(obj, "playerstate");

    read_ticcmd_t(&str->cmd, JS_GetObject(obj, "ticcmd"));

    str->viewz = JS_GetIntegerValue(obj, "viewz");
    str->viewheight = JS_GetIntegerValue(obj, "viewheight");
    str->deltaviewheight = JS_GetIntegerValue(obj, "deltaviewheight");
    str->bob = JS_GetIntegerValue(obj, "bob");
    str->momx = JS_GetIntegerValue(obj, "momx");
    str->momy = JS_GetIntegerValue(obj, "momy");
    str->health = JS_GetIntegerValue(obj, "health");
    str->armorpoints = JS_GetIntegerValue(obj, "armorpoints");
    str->armortype = JS_GetIntegerValue(obj, "armortype");

    json_t *powers_arr = JS_GetObject(obj, "powers");
    for (int i = 0; i < NUMPOWERS; ++i)
    {
        str->powers[i] = JS_GetInteger(JS_GetArrayItem(powers_arr, i));
    }

    json_t *cards_arr = JS_GetObject(obj, "cards");
    for (int i = 0; i < NUMCARDS; ++i)
    {
        str->cards[i] = JS_GetInteger(JS_GetArrayItem(cards_arr, i));
    }

    str->backpack = JS_GetIntegerValue(obj, "backpack");

    json_t *frags_arr = JS_GetObject(obj, "frags");
    for (int i = 0; i < MAXPLAYERS; ++i)
    {
        str->frags[i] = JS_GetInteger(JS_GetArrayItem(frags_arr, i));
    }

    str->readyweapon = JS_GetIntegerValue(obj, "readyweapon");
    str->pendingweapon = JS_GetIntegerValue(obj, "pendingweapon");

    json_t *weapons_arr = JS_GetObject(obj, "weaponowned");
    for (int i = 0; i < NUMWEAPONS; ++i)
    {
        str->weaponowned[i] = JS_GetInteger(JS_GetArrayItem(weapons_arr, i));
    }

    json_t *ammo_arr = JS_GetObject(obj, "ammo");
    for (int i = 0; i < NUMAMMO; ++i)
    {
        str->ammo[i] = JS_GetInteger(JS_GetArrayItem(ammo_arr, i));
    }

    json_t *maxammo_arr = JS_GetObject(obj, "maxammo");
    for (int i = 0; i < NUMAMMO; ++i)
    {
        str->maxammo[i] = JS_GetInteger(JS_GetArrayItem(maxammo_arr, i));
    }

    str->attackdown = JS_GetIntegerValue(obj, "attackdown");
    str->usedown = JS_GetIntegerValue(obj, "usedown");
    str->cheats = JS_GetIntegerValue(obj, "cheats");
    str->refire = JS_GetIntegerValue(obj, "refire");
    str->killcount = JS_GetIntegerValue(obj, "killcount");
    str->itemcount = JS_GetIntegerValue(obj, "itemcount");
    str->secretcount = JS_GetIntegerValue(obj, "secretcount");
    // TODO
    str->message = NULL;
    str->damagecount = JS_GetIntegerValue(obj, "damagecount");
    str->bonuscount = JS_GetIntegerValue(obj, "bonuscount");
    str->attacker = readp_mobj(JS_GetIntegerValue(obj, "attacker"));
    str->extralight = JS_GetIntegerValue(obj, "extralight");
    str->fixedcolormap = JS_GetIntegerValue(obj, "fixedcolormap");
    str->colormap = JS_GetIntegerValue(obj, "colormap");

    json_t *psprites_arr = JS_GetObject(obj, "psprites");
    for (int i = 0; i < NUMPSPRITES; ++i)
    {
        read_pspdef_t(&str->psprites[i], JS_GetArrayItem(psprites_arr, i));
    }

    str->secretmessage = NULL;
    str->didsecret = JS_GetIntegerValue(obj, "didsecret");
    str->oldviewz = JS_GetIntegerValue(obj, "oldviewz");
    str->pitch = JS_GetIntegerValue(obj, "pitch");
    str->oldpitch = JS_GetIntegerValue(obj, "oldpitch");
    str->slope = JS_GetIntegerValue(obj, "slope");
    str->maxkilldiscount = JS_GetIntegerValue(obj, "maxkilldiscount");

    array_clear(str->visitedlevels);
    json_t *visitedlevels_arr = JS_GetObject(obj, "visitedlevels");
    str->num_visitedlevels = JS_GetArraySize(visitedlevels_arr);
    for (int i = 0; i < str->num_visitedlevels; ++i)
    {
        json_t *visitedlevel_obj = JS_GetArrayItem(visitedlevels_arr, i);
        level_t level = {0};
        level.episode = JS_GetIntegerValue(visitedlevel_obj, "episode");
        level.map = JS_GetIntegerValue(visitedlevel_obj, "map");
        array_push(str->visitedlevels, level);
    }

    str->lastweapon = JS_GetIntegerValue(obj, "lastweapon");
    str->nextweapon = JS_GetIntegerValue(obj, "nextweapon");
    str->switching = JS_GetIntegerValue(obj, "switching");
}

static json_mut_t *write_player_t(player_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    JS_SetInt(doc, obj, "mo", writep_mobj(str->mo));
    JS_SetInt(doc, obj, "playerstate", str->playerstate);

    JS_SetObject(doc, obj, "ticcmd", write_ticcmd_t(&str->cmd));

    JS_SetInt(doc, obj, "viewz", str->viewz);
    JS_SetInt(doc, obj, "viewheight", str->viewheight);
    JS_SetInt(doc, obj, "deltaviewheight", str->deltaviewheight);
    JS_SetInt(doc, obj, "bob", str->bob);
    JS_SetInt(doc, obj, "momx", str->momx);
    JS_SetInt(doc, obj, "momy", str->momy);
    JS_SetInt(doc, obj, "health", str->health);
    JS_SetInt(doc, obj, "armorpoints", str->armorpoints);
    JS_SetInt(doc, obj, "armortype", str->armortype);

    json_mut_t *powers_arr = JS_NewArray(doc);
    for (int i = 0; i < NUMPOWERS; ++i)
    {
        JS_ArrayAddInt(doc, powers_arr, str->powers[i]);
    }
    JS_SetArray(doc, obj, "powers", powers_arr);

    json_mut_t *cards_arr = JS_NewArray(doc);
    for (int i = 0; i < NUMCARDS; ++i)
    {
        JS_ArrayAddInt(doc, cards_arr, str->cards[i]);
    }
    JS_SetArray(doc, obj, "cards", cards_arr);

    JS_SetInt(doc, obj, "backpack", str->backpack);

    json_mut_t *frags_arr = JS_NewArray(doc);
    for (int i = 0; i < MAXPLAYERS; ++i)
    {
        JS_ArrayAddInt(doc, frags_arr, str->frags[i]);
    }
    JS_SetArray(doc, obj, "frags", frags_arr);

    JS_SetInt(doc, obj, "readyweapon", str->readyweapon);
    JS_SetInt(doc, obj, "pendingweapon", str->pendingweapon);

    json_mut_t *weaponowned_arr = JS_NewArray(doc);
    for (int i = 0; i < NUMWEAPONS; ++i)
    {
        JS_ArrayAddInt(doc, weaponowned_arr, str->weaponowned[i]);
    }
    JS_SetArray(doc, obj, "weaponowned", weaponowned_arr);

    json_mut_t *ammo_arr = JS_NewArray(doc);
    for (int i = 0; i < NUMAMMO; ++i)
    {
        JS_ArrayAddInt(doc, ammo_arr, str->ammo[i]);
    }
    JS_SetArray(doc, obj, "ammo", ammo_arr);

    json_mut_t *maxammo_arr = JS_NewArray(doc);
    for (int i = 0; i < NUMAMMO; ++i)
    {
        JS_ArrayAddInt(doc, maxammo_arr, str->maxammo[i]);
    }
    JS_SetArray(doc, obj, "maxammo", maxammo_arr);

    JS_SetInt(doc, obj, "attackdown", str->attackdown);
    JS_SetInt(doc, obj, "usedown", str->usedown);
    JS_SetInt(doc, obj, "cheats", str->cheats);
    JS_SetInt(doc, obj, "refire", str->refire);
    JS_SetInt(doc, obj, "killcount", str->killcount);
    JS_SetInt(doc, obj, "itemcount", str->itemcount);
    JS_SetInt(doc, obj, "secretcount", str->secretcount);
    // TODO
    // str->message;
    JS_SetInt(doc, obj, "damagecount", str->damagecount);
    JS_SetInt(doc, obj, "bonuscount", str->bonuscount);
    JS_SetInt(doc, obj, "attacker", writep_mobj(str->attacker));
    JS_SetInt(doc, obj, "extralight", str->extralight);
    JS_SetInt(doc, obj, "fixedcolormap", str->fixedcolormap);
    JS_SetInt(doc, obj, "colormap", str->colormap);

    json_mut_t *psprites_arr = JS_NewArray(doc);
    for (int i = 0; i < NUMPSPRITES; ++i)
    {
        JS_ArrayAddObject(doc, psprites_arr, write_pspdef_t(&str->psprites[i]));
    }
    JS_SetArray(doc, obj, "psprites", psprites_arr);

    JS_SetInt(doc, obj, "didsecret", str->didsecret);
    JS_SetInt(doc, obj, "oldviewz", str->oldviewz);
    JS_SetInt(doc, obj, "pitch", str->pitch);
    JS_SetInt(doc, obj, "oldpitch", str->oldpitch);
    JS_SetInt(doc, obj, "slope", str->slope);
    JS_SetInt(doc, obj, "maxkilldiscount", str->maxkilldiscount);

    json_mut_t *visitedlevels_arr = JS_NewArray(doc);
    array_foreach_type(level, str->visitedlevels, level_t)
    {
        json_mut_t *visitedlevel_obj = JS_NewObject(doc);
        JS_SetInt(doc, visitedlevel_obj, "episode", level->episode);
        JS_SetInt(doc, visitedlevel_obj, "map", level->map);
        JS_ArrayAddObject(doc, visitedlevels_arr, visitedlevel_obj);
    }
    JS_SetArray(doc, obj, "visitedlevels", visitedlevels_arr);

    JS_SetInt(doc, obj, "lastweapon", str->lastweapon);
    JS_SetInt(doc, obj, "nextweapon", str->nextweapon);
    JS_SetInt(doc, obj, "switching", str->switching);

    return obj;
}

static void read_ceiling_t(ceiling_t *str, thinker_class_t tc, json_t *obj)
{
    read_thinker_t(&str->thinker, tc, JS_GetObject(obj, "thinker"));

    str->type = JS_GetIntegerValue(obj, "type");
    JS_GetIdx(str->sector, sectors, obj, "sector");
    str->bottomheight = JS_GetIntegerValue(obj, "bottomheight");
    str->topheight = JS_GetIntegerValue(obj, "topheight");
    str->speed = JS_GetIntegerValue(obj, "speed");
    str->oldspeed = JS_GetIntegerValue(obj, "oldspeed");
    str->crush = JS_GetIntegerValue(obj, "crush");
    str->newspecial = JS_GetIntegerValue(obj, "newspecial");
    str->oldspecial = JS_GetIntegerValue(obj, "oldspecial");
    str->texture = JS_GetIntegerValue(obj, "texture");
    str->direction = JS_GetIntegerValue(obj, "direction");
    str->tag = JS_GetIntegerValue(obj, "tag");
    str->olddirection = JS_GetIntegerValue(obj, "olddirection");
    str->list = readp_activeceilings(JS_GetIntegerValue(obj, "list"));
}

static json_mut_t *write_ceiling_t(ceiling_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    JS_SetObject(doc, obj, "thinker", write_thinker_t(&str->thinker));

    JS_SetInt(doc, obj, "type", str->type);
    JS_SetIdx(doc, obj, "sector", str->sector, sectors);
    JS_SetInt(doc, obj, "bottomheight", str->bottomheight);
    JS_SetInt(doc, obj, "topheight", str->topheight);
    JS_SetInt(doc, obj, "speed", str->speed);
    JS_SetInt(doc, obj, "oldspeed", str->oldspeed);
    JS_SetInt(doc, obj, "crush", str->crush);
    JS_SetInt(doc, obj, "newspecial", str->newspecial);
    JS_SetInt(doc, obj, "oldspecial", str->oldspecial);
    JS_SetInt(doc, obj, "texture", str->texture);
    JS_SetInt(doc, obj, "direction", str->direction);
    JS_SetInt(doc, obj, "tag", str->tag);
    JS_SetInt(doc, obj, "olddirection", str->olddirection);
    JS_SetInt(doc, obj, "list", writep_activeceilings(str->list));

    return obj;
}

static void read_vldoor_t(vldoor_t *str, thinker_class_t tc, json_t *obj)
{
    read_thinker_t(&str->thinker, tc, JS_GetObject(obj, "thinker"));

    str->type = JS_GetIntegerValue(obj, "type");
    JS_GetIdx(str->sector, sectors, obj, "sector");
    str->topheight = JS_GetIntegerValue(obj, "topheight");
    str->speed = JS_GetIntegerValue(obj, "speed");
    str->direction = JS_GetIntegerValue(obj, "direction");
    str->topwait = JS_GetIntegerValue(obj, "topwait");
    str->topcountdown = JS_GetIntegerValue(obj, "topcountdown");
    JS_GetIdx(str->line, lines, obj, "line");
    str->lighttag = JS_GetIntegerValue(obj, "lighttag");
}

static json_mut_t *write_vldoor_t(vldoor_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    JS_SetObject(doc, obj, "thinker", write_thinker_t(&str->thinker));

    JS_SetInt(doc, obj, "type", str->type);
    JS_SetIdx(doc, obj, "sector", str->sector, sectors);
    JS_SetInt(doc, obj, "topheight", str->topheight);
    JS_SetInt(doc, obj, "speed", str->speed);
    JS_SetInt(doc, obj, "direction", str->direction);
    JS_SetInt(doc, obj, "topwait", str->topwait);
    JS_SetInt(doc, obj, "topcountdown", str->topcountdown);
    JS_SetIdx(doc, obj, "line", str->line, lines);
    JS_SetInt(doc, obj, "lighttag", str->lighttag);

    return obj;
}

static void read_floormove_t(floormove_t *str, thinker_class_t tc, json_t *obj)
{
    read_thinker_t(&str->thinker, tc, JS_GetObject(obj, "thinker"));

    str->type = JS_GetIntegerValue(obj, "type");
    str->crush = JS_GetIntegerValue(obj, "crush");
    JS_GetIdx(str->sector, sectors, obj, "sector");
    str->direction = JS_GetIntegerValue(obj, "direction");
    str->newspecial = JS_GetIntegerValue(obj, "newspecial");
    str->oldspecial = JS_GetIntegerValue(obj, "oldspecial");
    str->texture = JS_GetIntegerValue(obj, "texture");
    str->floordestheight = JS_GetIntegerValue(obj, "floordestheight");
    str->speed = JS_GetIntegerValue(obj, "speed");
}

static json_mut_t *write_floormove_t(floormove_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    JS_SetObject(doc, obj, "thinker", write_thinker_t(&str->thinker));

    JS_SetInt(doc, obj, "type", str->type);
    JS_SetInt(doc, obj, "crush", str->crush);
    JS_SetIdx(doc, obj, "sector", str->sector, sectors);
    JS_SetInt(doc, obj, "direction", str->direction);
    JS_SetInt(doc, obj, "newspecial", str->newspecial);
    JS_SetInt(doc, obj, "oldspecial", str->oldspecial);
    JS_SetInt(doc, obj, "texture", str->texture);
    JS_SetInt(doc, obj, "floordestheight", str->floordestheight);
    JS_SetInt(doc, obj, "speed", str->speed);

    return obj;
}

static void read_plat_t(plat_t *str, thinker_class_t tc, json_t *obj)
{
    read_thinker_t(&str->thinker, tc, JS_GetObject(obj, "thinker"));

    JS_GetIdx(str->sector, sectors, obj, "sector");
    str->speed = JS_GetIntegerValue(obj, "speed");
    str->low = JS_GetIntegerValue(obj, "low");
    str->high = JS_GetIntegerValue(obj, "high");
    str->wait = JS_GetIntegerValue(obj, "wait");
    str->count = JS_GetIntegerValue(obj, "count");
    str->status = JS_GetIntegerValue(obj, "status");
    str->oldstatus = JS_GetIntegerValue(obj, "oldstatus");
    str->crush = JS_GetIntegerValue(obj, "crush");
    str->tag = JS_GetIntegerValue(obj, "tag");
    str->type = JS_GetIntegerValue(obj, "type");
    str->list = readp_activeplats(JS_GetIntegerValue(obj, "list"));
}

static json_mut_t *write_plat_t(plat_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    JS_SetObject(doc, obj, "thinker", write_thinker_t(&str->thinker));

    JS_SetIdx(doc, obj, "sector", str->sector, sectors);
    JS_SetInt(doc, obj, "speed", str->speed);
    JS_SetInt(doc, obj, "low", str->low);
    JS_SetInt(doc, obj, "high", str->high);
    JS_SetInt(doc, obj, "wait", str->wait);
    JS_SetInt(doc, obj, "count", str->count);
    JS_SetInt(doc, obj, "status", str->status);
    JS_SetInt(doc, obj, "oldstatus", str->oldstatus);
    JS_SetInt(doc, obj, "crush", str->crush);
    JS_SetInt(doc, obj, "tag", str->tag);
    JS_SetInt(doc, obj, "type", str->type);
    JS_SetInt(doc, obj, "list", writep_activeplats(str->list));

    return obj;
}

static void read_lightflash_t(lightflash_t *str, json_t *obj)
{
    read_thinker_t(&str->thinker, tc_flash, JS_GetObject(obj, "thinker"));

    JS_GetIdx(str->sector, sectors, obj, "sector");
    str->count = JS_GetIntegerValue(obj, "count");
    str->maxlight = JS_GetIntegerValue(obj, "maxlight");
    str->minlight = JS_GetIntegerValue(obj, "minlight");
    str->maxtime = JS_GetIntegerValue(obj, "maxtime");
    str->mintime = JS_GetIntegerValue(obj, "mintime");
}

static json_mut_t *write_lightflash_t(lightflash_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    JS_SetObject(doc, obj, "thinker", write_thinker_t(&str->thinker));

    JS_SetIdx(doc, obj, "sector", str->sector, sectors);
    JS_SetInt(doc, obj, "count", str->count);
    JS_SetInt(doc, obj, "maxlight", str->maxlight);
    JS_SetInt(doc, obj, "minlight", str->minlight);
    JS_SetInt(doc, obj, "maxtime", str->maxtime);
    JS_SetInt(doc, obj, "mintime", str->mintime);

    return obj;
}

static void read_strobe_t(strobe_t *str, json_t *obj)
{
    read_thinker_t(&str->thinker, tc_strobe, JS_GetObject(obj, "thinker"));

    JS_GetIdx(str->sector, sectors, obj, "sector");
    str->count = JS_GetIntegerValue(obj, "count");
    str->minlight = JS_GetIntegerValue(obj, "minlight");
    str->maxlight = JS_GetIntegerValue(obj, "maxlight");
    str->darktime = JS_GetIntegerValue(obj, "darktime");
    str->brighttime = JS_GetIntegerValue(obj, "brighttime");
}

static json_mut_t *write_strobe_t(strobe_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    JS_SetObject(doc, obj, "thinker", write_thinker_t(&str->thinker));

    JS_SetIdx(doc, obj, "sector", str->sector, sectors);
    JS_SetInt(doc, obj, "count", str->count);
    JS_SetInt(doc, obj, "minlight", str->minlight);
    JS_SetInt(doc, obj, "maxlight", str->maxlight);
    JS_SetInt(doc, obj, "darktime", str->darktime);
    JS_SetInt(doc, obj, "brighttime", str->brighttime);

    return obj;
}

static void read_glow_t(glow_t *str, json_t *obj)
{
    read_thinker_t(&str->thinker, tc_glow, JS_GetObject(obj, "thinker"));

    JS_GetIdx(str->sector, sectors, obj, "sector");
    str->minlight = JS_GetIntegerValue(obj, "minlight");
    str->maxlight = JS_GetIntegerValue(obj, "maxlight");
    str->direction = JS_GetIntegerValue(obj, "direction");
}

static json_mut_t *write_glow_t(glow_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    JS_SetObject(doc, obj, "thinker", write_thinker_t(&str->thinker));

    JS_SetIdx(doc, obj, "sector", str->sector, sectors);
    JS_SetInt(doc, obj, "minlight", str->minlight);
    JS_SetInt(doc, obj, "maxlight", str->maxlight);
    JS_SetInt(doc, obj, "direction", str->direction);

    return obj;
}

static void read_fireflicker_t(fireflicker_t *str, json_t *obj)
{
    read_thinker_t(&str->thinker, tc_flicker, JS_GetObject(obj, "thinker"));

    JS_GetIdx(str->sector, sectors, obj, "sector");
    str->count = JS_GetIntegerValue(obj, "count");
    str->maxlight = JS_GetIntegerValue(obj, "maxlight");
    str->minlight = JS_GetIntegerValue(obj, "minlight");
}

static json_mut_t *write_fireflicker_t(fireflicker_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    JS_SetObject(doc, obj, "thinker", write_thinker_t(&str->thinker));

    JS_SetIdx(doc, obj, "sector", str->sector, sectors);
    JS_SetInt(doc, obj, "count", str->count);
    JS_SetInt(doc, obj, "maxlight", str->maxlight);
    JS_SetInt(doc, obj, "minlight", str->minlight);

    return obj;
}

static void read_elevator_t(elevator_t *str, thinker_class_t tc, json_t *obj)
{
    read_thinker_t(&str->thinker, tc, JS_GetObject(obj, "thinker"));

    str->type = JS_GetIntegerValue(obj, "type");
    JS_GetIdx(str->sector, sectors, obj, "sector");
    str->direction = JS_GetIntegerValue(obj, "direction");
    str->floordestheight = JS_GetIntegerValue(obj, "floordestheight");
    str->ceilingdestheight = JS_GetIntegerValue(obj, "ceilingdestheight");
    str->speed = JS_GetIntegerValue(obj, "speed");
}

static json_mut_t *write_elevator_t(elevator_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    JS_SetObject(doc, obj, "thinker", write_thinker_t(&str->thinker));

    JS_SetInt(doc, obj, "type", str->type);
    JS_SetIdx(doc, obj, "sector", str->sector, sectors);
    JS_SetInt(doc, obj, "direction", str->direction);
    JS_SetInt(doc, obj, "floordestheight", str->floordestheight);
    JS_SetInt(doc, obj, "ceilingdestheight", str->ceilingdestheight);
    JS_SetInt(doc, obj, "speed", str->speed);

    return obj;
}

static void read_scroll_t(scroll_t *str, thinker_class_t tc, json_t *obj)
{
    read_thinker_t(&str->thinker, tc, JS_GetObject(obj, "thinker"));

    str->dx = JS_GetIntegerValue(obj, "dx");
    str->dy = JS_GetIntegerValue(obj, "dy");
    str->affectee = JS_GetIntegerValue(obj, "affectee");
    str->control = JS_GetIntegerValue(obj, "control");
    str->last_height = JS_GetIntegerValue(obj, "last_height");
    str->vdx = JS_GetIntegerValue(obj, "vdx");
    str->vdy = JS_GetIntegerValue(obj, "vdy");
    str->accel = JS_GetIntegerValue(obj, "accel");
    str->type = JS_GetIntegerValue(obj, "type");
}

static json_mut_t *write_scroll_t(scroll_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    JS_SetObject(doc, obj, "thinker", write_thinker_t(&str->thinker));

    JS_SetInt(doc, obj, "dx", str->dx);
    JS_SetInt(doc, obj, "dy", str->dy);
    JS_SetInt(doc, obj, "affectee", str->affectee);
    JS_SetInt(doc, obj, "control", str->control);
    JS_SetInt(doc, obj, "last_height", str->last_height);
    JS_SetInt(doc, obj, "vdx", str->vdx);
    JS_SetInt(doc, obj, "vdy", str->vdy);
    JS_SetInt(doc, obj, "accel", str->accel);
    JS_SetInt(doc, obj, "type", str->type);

    return obj;
}

static void read_pusher_t(pusher_t *str, json_t *obj)
{
    read_thinker_t(&str->thinker, tc_pusher, JS_GetObject(obj, "thinker"));

    str->type = JS_GetIntegerValue(obj, "type");
    str->source = readp_mobj(JS_GetIntegerValue(obj, "source"));
    str->x_mag = JS_GetIntegerValue(obj, "x_mag");
    str->y_mag = JS_GetIntegerValue(obj, "y_mag");
    str->magnitude = JS_GetIntegerValue(obj, "magnitude");
    str->radius = JS_GetIntegerValue(obj, "radius");
    str->x = JS_GetIntegerValue(obj, "x");
    str->y = JS_GetIntegerValue(obj, "y");
    str->affectee = JS_GetIntegerValue(obj, "affectee");
}

static json_mut_t *write_pusher_t(pusher_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    JS_SetObject(doc, obj, "thinker", write_thinker_t(&str->thinker));

    JS_SetInt(doc, obj, "type", str->type);
    JS_SetInt(doc, obj, "source", writep_mobj(str->source));
    JS_SetInt(doc, obj, "x_mag", str->x_mag);
    JS_SetInt(doc, obj, "y_mag", str->y_mag);
    JS_SetInt(doc, obj, "magnitude", str->magnitude);
    JS_SetInt(doc, obj, "radius", str->radius);
    JS_SetInt(doc, obj, "x", str->x);
    JS_SetInt(doc, obj, "y", str->y);
    JS_SetInt(doc, obj, "affectee", str->affectee);

    return obj;
}

static void read_friction_t(friction_t *str, json_t *obj)
{
    read_thinker_t(&str->thinker, tc_friction, JS_GetObject(obj, "thinker"));

    str->friction = JS_GetIntegerValue(obj, "friction");
    str->movefactor = JS_GetIntegerValue(obj, "movefactor");
    str->affectee = JS_GetIntegerValue(obj, "affectee");
}

static json_mut_t *write_friction_t(friction_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    JS_SetObject(doc, obj, "thinker", write_thinker_t(&str->thinker));

    JS_SetInt(doc, obj, "friction", str->friction);
    JS_SetInt(doc, obj, "movefactor", str->movefactor);
    JS_SetInt(doc, obj, "affectee", str->affectee);

    return obj;
}

static void read_ambient_data_t(ambient_data_t *str, json_t *obj)
{
    str->type = JS_GetIntegerValue(obj, "type");
    str->mode = JS_GetIntegerValue(obj, "mode");
    str->close_dist = JS_GetIntegerValue(obj, "close_dist");
    str->clipping_dist = JS_GetIntegerValue(obj, "clipping_dist");
    str->min_tics = JS_GetIntegerValue(obj, "min_tics");
    str->max_tics = JS_GetIntegerValue(obj, "max_tics");
    str->volume_scale = JS_GetIntegerValue(obj, "volume_scale");
    str->sfx_id = JS_GetIntegerValue(obj, "sfx_id");
}

static json_mut_t *write_ambient_data_t(ambient_data_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    JS_SetInt(doc, obj, "type", str->type);
    JS_SetInt(doc, obj, "mode", str->mode);
    JS_SetInt(doc, obj, "close_dist", str->close_dist);
    JS_SetInt(doc, obj, "clipping_dist", str->clipping_dist);
    JS_SetInt(doc, obj, "min_tics", str->min_tics);
    JS_SetInt(doc, obj, "max_tics", str->max_tics);
    JS_SetInt(doc, obj, "volume_scale", str->volume_scale);
    JS_SetInt(doc, obj, "sfx_id", str->sfx_id);

    return obj;
}

static void read_ambient_t(ambient_t *str, json_t *obj)
{
    read_thinker_t(&str->thinker, tc_ambient, JS_GetObject(obj, "thinker"));

    str->source = readp_mobj(JS_GetIntegerValue(obj, "source"));
    str->origin = readp_mobj(JS_GetIntegerValue(obj, "origin"));

    read_ambient_data_t(&str->data, JS_GetObject(obj, "ambient_data"));

    str->update_tics = AMB_UPDATE_NOW;
    str->wait_tics = JS_GetIntegerValue(obj, "wait_tics");
    str->active = JS_GetIntegerValue(obj, "active");
    str->playing = false;
    str->offset = (float)FixedToDouble(JS_GetIntegerValue(obj, "offset"));
    str->last_offset =
        (float)FixedToDouble(JS_GetIntegerValue(obj, "last_offset"));
    str->last_leveltime = JS_GetIntegerValue(obj, "last_leveltime");
}

static json_mut_t *write_ambient_t(ambient_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    JS_SetObject(doc, obj, "thinker", write_thinker_t(&str->thinker));

    JS_SetInt(doc, obj, "source", writep_mobj(str->source));
    JS_SetInt(doc, obj, "origin", writep_mobj(str->origin));

    JS_SetObject(doc, obj, "ambient_data", write_ambient_data_t(&str->data));

    JS_SetInt(doc, obj, "wait_tics", str->wait_tics);
    JS_SetInt(doc, obj, "active", str->active);
    JS_SetInt(doc, obj, "offset", DoubleToFixed(str->offset));
    JS_SetInt(doc, obj, "last_offset", DoubleToFixed(str->last_offset));
    JS_SetInt(doc, obj, "last_leveltime", str->last_leveltime);

    return obj;
}

static void read_rng_t(rng_t *str, json_t *obj)
{
    json_t *seeds_arr = JS_GetObject(obj, "seeds");
    for (int i = 0; i < NUMPRCLASS; ++i)
    {
        json_t *seed_obj = JS_GetArrayItem(seeds_arr, i);
        str->seed[i] = JS_GetNumber(seed_obj);
    }
    str->rndindex = JS_GetIntegerValue(obj, "rndindex");
    str->prndindex = JS_GetIntegerValue(obj, "prndindex");
}

static json_mut_t *write_rng_t(rng_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    json_mut_t *seed_arr = JS_NewArray(doc);
    for (int i = 0; i < NUMPRCLASS; ++i)
    {
        JS_ArrayAddInt(doc, seed_arr, str->seed[i]);
    }
    JS_SetArray(doc, obj, "seeds", seed_arr);

    JS_SetInt(doc, obj, "rndindex", str->rndindex);
    JS_SetInt(doc, obj, "prndindex", str->prndindex);

    return obj;
}

static void read_button_t(button_t *str, json_t *obj)
{
    JS_GetIdx(str->line, lines, obj, "line");
    str->where = JS_GetIntegerValue(obj, "where");
    str->btexture = JS_GetIntegerValue(obj, "btexture");
    str->btimer = JS_GetIntegerValue(obj, "btimer");
}

static json_mut_t *write_button_t(button_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    JS_SetIdx(doc, obj, "line", str->line, lines);
    JS_SetInt(doc, obj, "where", str->where);
    JS_SetInt(doc, obj, "btexture", str->btexture);
    JS_SetInt(doc, obj, "btimer", str->btimer);

    return obj;
}

static void read_msecnode_t(msecnode_t *str, json_t *obj)
{
    JS_GetIdx(str->m_sector, sectors, obj, "m_sector");
    str->m_thing = readp_mobj(JS_GetIntegerValue(obj, "m_thing"));
    str->m_tprev = readp_msecnode(JS_GetIntegerValue(obj, "m_tprev"));
    str->m_tnext = readp_msecnode(JS_GetIntegerValue(obj, "m_tnext"));
    str->m_sprev = readp_msecnode(JS_GetIntegerValue(obj, "m_sprev"));
    str->m_snext = readp_msecnode(JS_GetIntegerValue(obj, "m_snext"));
    str->visited = JS_GetIntegerValue(obj, "visited");
}

static json_mut_t *write_msecnode_t(msecnode_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    JS_SetIdx(doc, obj, "m_sector", str->m_sector, sectors);
    JS_SetInt(doc, obj, "m_thing", writep_mobj(str->m_thing));
    JS_SetInt(doc, obj, "m_tprev", writep_msecnode(str->m_tprev));
    JS_SetInt(doc, obj, "m_tnext", writep_msecnode(str->m_tnext));
    JS_SetInt(doc, obj, "m_sprev", writep_msecnode(str->m_sprev));
    JS_SetInt(doc, obj, "m_snext", writep_msecnode(str->m_snext));
    JS_SetInt(doc, obj, "visited", str->visited);

    return obj;
}

static void read_divline_t(divline_t *str, json_t *obj)
{
    str->x = JS_GetIntegerValue(obj, "x");
    str->y = JS_GetIntegerValue(obj, "y");
    str->dx = JS_GetIntegerValue(obj, "dx");
    str->dy = JS_GetIntegerValue(obj, "dy");
}

static json_mut_t *write_divline_t(divline_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    JS_SetInt(doc, obj, "x", str->x);
    JS_SetInt(doc, obj, "y", str->y);
    JS_SetInt(doc, obj, "dx", str->dx);
    JS_SetInt(doc, obj, "dy", str->dy);

    return obj;
}

static void read_partial_side_t(partial_side_t *str, json_t *partial_side_obj)
{
    str->textureoffset = JS_GetIntegerValue(partial_side_obj, "textureoffset");
    str->rowoffset = JS_GetIntegerValue(partial_side_obj, "rowoffset");
    str->toptexture = JS_GetIntegerValue(partial_side_obj, "toptexture");
    str->bottomtexture = JS_GetIntegerValue(partial_side_obj, "bottomtexture");
    str->midtexture = JS_GetIntegerValue(partial_side_obj, "midtexture");
}

static json_mut_t *write_partial_side_t(partial_side_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    JS_SetInt(doc, obj, "textureoffset", str->textureoffset);
    JS_SetInt(doc, obj, "rowoffset", str->rowoffset);
    JS_SetInt(doc, obj, "toptexture", str->toptexture);
    JS_SetInt(doc, obj, "bottomtexture", str->bottomtexture);
    JS_SetInt(doc, obj, "midtexture", str->midtexture);

    return obj;
}

//
// World
//

static void ArchiveDirty(void)
{
    json_mut_t *lines_arr = JS_NewArray(doc);
    array_foreach_type(dl, dirty_lines, dirty_line_t)
    {
        json_mut_t *line_obj = JS_NewObject(doc);

        JS_SetIdx(doc, line_obj, "line", dl->line, lines);
        JS_SetInt(doc, line_obj, "special", dl->clean_line.special);

        JS_ArrayAddObject(doc, lines_arr, line_obj);
    }
    JS_SetArray(doc, root_mut, "dirty_lines", lines_arr);

    json_mut_t *sides_arr = JS_NewArray(doc);
    array_foreach_type(ds, dirty_sides, dirty_side_t)
    {
        json_mut_t *side_obj = JS_NewObject(doc);

        JS_SetIdx(doc, side_obj, "side", ds->side, sides);
        json_mut_t *partial_side = write_partial_side_t(&ds->clean_side);
        JS_SetObject(doc, side_obj, "partial_side", partial_side);

        JS_ArrayAddObject(doc, sides_arr, side_obj);
    }
    JS_SetArray(doc, root_mut, "dirty_sides", sides_arr);
}

static void UnArchiveDirty(void)
{
    json_t *lines_arr = JS_GetObject(root, "dirty_lines");
    int count = JS_GetArraySize(lines_arr);
    int oldcount = array_size(dirty_lines);
    for (int i = count; i < oldcount; ++i)
    {
        P_CleanLine(&dirty_lines[i]);
    }
    array_resize(dirty_lines, count);
    json_arr_iter_t *lines_iter = JS_ArrayIterator(lines_arr);
    array_foreach_type(dl, dirty_lines, dirty_line_t)
    {
        json_t *line_obj = JS_ArrayNext(lines_iter);
        dl->line = lines + JS_GetIntegerValue(line_obj, "line");
        dl->line->dirty = true;
        dl->clean_line.special = JS_GetIntegerValue(line_obj, "special");
    }

    json_t *sides_arr = JS_GetObject(root, "dirty_sides");
    count = JS_GetArraySize(sides_arr);
    oldcount = array_size(dirty_sides);
    for (int i = count; i < oldcount; ++i)
    {
        P_CleanSide(&dirty_sides[i]);
    }
    array_resize(dirty_sides, count);
    json_arr_iter_t *sides_iter = JS_ArrayIterator(sides_arr);
    array_foreach_type(ds, dirty_sides, dirty_side_t)
    {
        json_t *side_obj = JS_ArrayNext(sides_iter);
        ds->side = sides + JS_GetIntegerValue(side_obj, "side");
        ds->side->dirty = true;
        json_t *partial_side_obj = JS_GetObject(side_obj, "partial_side");
        read_partial_side_t(&ds->clean_side, partial_side_obj);
    }
}

inline static json_mut_t *ArchiveThingList(const sector_t *sector)
{
    json_mut_t *thinglist_arr = JS_NewArray(doc);

    for (mobj_t *mobj = sector->thinglist; mobj; mobj = mobj->snext)
    {
        JS_ArrayAddInt(doc, thinglist_arr, writep_mobj(mobj));
    }

    return thinglist_arr;
}

inline static void UnArchiveThingList(sector_t *sector, json_t *thinglist_arr)
{
    sector->thinglist = NULL;
    int count = JS_GetArraySize(thinglist_arr);
    if (!count)
    {
        return;
    }

    mobj_t *mobj, **sprev;
    sprev = &sector->thinglist;
    while (count--)
    {
        json_t *thing_obj = JS_GetArrayItem(thinglist_arr, count);
        mobj = readp_mobj(JS_GetInteger(thing_obj));

        *sprev = mobj;
        if (mobj)
        {
            mobj->sprev = sprev;
            mobj->snext = NULL;
            sprev = &mobj->snext;
        }
    }
}

static void ArchiveWorld(void)
{
    int i;
    const sector_t *sector;

    // do sectors
    json_mut_t *sectors_arr = JS_NewArray(doc);
    for (i = 0, sector = sectors; i < numsectors; i++, sector++)
    {
        json_mut_t *sector_obj = JS_NewObject(doc);

        // killough 10/98: save full floor & ceiling heights, including fraction
        JS_SetInt(doc, sector_obj, "floorheight", sector->floorheight);
        JS_SetInt(doc, sector_obj, "ceilingheight", sector->ceilingheight);
        JS_SetInt(doc, sector_obj, "floor_xoffs", sector->floor_xoffs);
        JS_SetInt(doc, sector_obj, "floor_yoffs", sector->floor_yoffs);
        JS_SetInt(doc, sector_obj, "ceiling_xoffs", sector->ceiling_xoffs);
        JS_SetInt(doc, sector_obj, "ceiling_yoffs", sector->ceiling_yoffs);
        JS_SetInt(doc, sector_obj, "floor_rotation", sector->floor_rotation);
        JS_SetInt(doc, sector_obj, "ceiling_rotation",
                  sector->ceiling_rotation);
        JS_SetInt(doc, sector_obj, "tint", sector->tint);

        JS_SetInt(doc, sector_obj, "floorpic", sector->floorpic);
        JS_SetInt(doc, sector_obj, "ceilingpic", sector->ceilingpic);
        JS_SetInt(doc, sector_obj, "lightlevel", sector->lightlevel);
        JS_SetInt(doc, sector_obj, "special", sector->special);
        JS_SetInt(doc, sector_obj, "tag", sector->tag);

        // Woof!
        JS_SetInt(doc, sector_obj, "soundtarget",
                  writep_mobj(sector->soundtarget));
        JS_SetInt(doc, sector_obj, "floordata",
                  writep_thinker(sector->floordata));
        JS_SetInt(doc, sector_obj, "ceilingdata",
                  writep_thinker(sector->ceilingdata));

        json_mut_t *thinglist = ArchiveThingList(sector);
        JS_SetArray(doc, sector_obj, "thinglist", thinglist);

        JS_SetInt(doc, sector_obj, "touching_thinglist",
                  writep_msecnode(sector->touching_thinglist));

        JS_ArrayAddObject(doc, sectors_arr, sector_obj);
    }
    JS_SetArray(doc, root_mut, "sectors", sectors_arr);

    const line_t *line;

    json_mut_t *lines_arr = JS_NewArray(doc);
    for (i = 0, line = lines; i < numlines; i++, line++)
    {
        json_mut_t *line_obj = JS_NewObject(doc);

        JS_SetInt(doc, line_obj, "flags", line->flags);
        JS_SetInt(doc, line_obj, "special", line->special);

        json_mut_t *sides_arr = JS_NewArray(doc);
        for (int j = 0; j < 2; j++)
        {
            json_mut_t *side_obj = JS_NewObject(doc);
            if (line->sidenum[j] != NO_INDEX)
            {
                side_t *side = &sides[line->sidenum[j]];

                JS_SetInt(doc, side_obj, "textureoffset", side->textureoffset);
                JS_SetInt(doc, side_obj, "rowoffset", side->rowoffset);

                JS_SetInt(doc, side_obj, "toptexture", side->toptexture);
                JS_SetInt(doc, side_obj, "bottomtexture", side->bottomtexture);
                JS_SetInt(doc, side_obj, "midtexture", side->midtexture);
            }
            JS_ArrayAddObject(doc, sides_arr, side_obj);
        }
        JS_SetArray(doc, line_obj, "sides", sides_arr);
        JS_ArrayAddObject(doc, lines_arr, line_obj);
    }
    JS_SetArray(doc, root_mut, "lines", lines_arr);
}

static void UnArchiveWorld(void)
{
    int i;
    sector_t *sector;

    json_t *sectors_arr = JS_GetObject(root, "sectors");

    // do sectors
    for (i = 0, sector = sectors; i < numsectors; i++, sector++)
    {
        json_t *sector_obj = JS_GetArrayItem(sectors_arr, i);

        sector->floorheight = JS_GetIntegerValue(sector_obj, "floorheight");
        sector->ceilingheight = JS_GetIntegerValue(sector_obj, "ceilingheight");
        sector->floor_xoffs = JS_GetIntegerValue(sector_obj, "floor_xoffs");
        sector->floor_yoffs = JS_GetIntegerValue(sector_obj, "floor_yoffs");
        sector->ceiling_xoffs = JS_GetIntegerValue(sector_obj, "ceiling_xoffs");
        sector->ceiling_yoffs = JS_GetIntegerValue(sector_obj, "ceiling_yoffs");
        sector->floor_rotation =
            JS_GetIntegerValue(sector_obj, "floor_rotation");
        sector->ceiling_rotation =
            JS_GetIntegerValue(sector_obj, "ceiling_rotation");
        sector->tint = JS_GetIntegerValue(sector_obj, "tint");

        sector->floorpic = JS_GetIntegerValue(sector_obj, "floorpic");
        sector->ceilingpic = JS_GetIntegerValue(sector_obj, "ceilingpic");
        sector->lightlevel = JS_GetIntegerValue(sector_obj, "lightlevel");
        sector->special = JS_GetIntegerValue(sector_obj, "special");
        sector->tag = JS_GetIntegerValue(sector_obj, "tag");

        // Woof!
        sector->soundtarget =
            readp_mobj(JS_GetIntegerValue(sector_obj, "soundtarget"));
        sector->floordata =
            readp_thinker(JS_GetIntegerValue(sector_obj, "floordata"));
        sector->ceilingdata =
            readp_thinker(JS_GetIntegerValue(sector_obj, "ceilingdata"));

        json_t *thinglist_obj = JS_GetObject(sector_obj, "thinglist");
        UnArchiveThingList(sector, thinglist_obj);

        sector->touching_thinglist = readp_msecnode(
            JS_GetIntegerValue(sector_obj, "touching_thinglist"));
    }

    line_t *line;

    json_t *lines_arr = JS_GetObject(root, "lines");

    for (i = 0, line = lines; i < numlines; i++, line++)
    {
        json_t *line_obj = JS_GetArrayItem(lines_arr, i);

        line->flags = JS_GetIntegerValue(line_obj, "flags");
        line->special = JS_GetIntegerValue(line_obj, "special");

        json_t *sides_arr = JS_GetObject(line_obj, "sides");
        for (int j = 0; j < 2; j++)
        {
            json_t *side_obj = JS_GetArrayItem(sides_arr, j);
            if (JS_GetObjectSize(side_obj) > 0)
            {
                side_t *side = &sides[line->sidenum[j]];

                // killough 10/98: load full sidedef offsets, including
                // fractions

                side->textureoffset =
                    JS_GetIntegerValue(side_obj, "textureoffset");
                side->rowoffset = JS_GetIntegerValue(side_obj, "rowoffset");

                side->toptexture = JS_GetIntegerValue(side_obj, "toptexture");
                side->bottomtexture =
                    JS_GetIntegerValue(side_obj, "bottomtexture");
                side->midtexture = JS_GetIntegerValue(side_obj, "midtexture");
            }
        }
    }
}

//
// Thinkers
//

static thinker_class_t GetThinkerClass(actionf_p1 func)
{
    thinker_class_t tc;
    for (tc = 0; tc < arrlen(actions); ++tc)
    {
        if (func == actions[tc])
        {
            return tc;
        }
    }
    return tc_none;
}

// Pass 1 of archiving: snapshot the arena table so that every live thinker
// pointer can later be converted to a stable integer index.  Must be called
// before any write_*_t function that references thinker pointers.
static void PrepareArchiveThinkers(void)
{
    uintptr_t *table = M_ArenaTable(thinkers_arena);

    int count = M_ArenaTableSize(thinkers_arena);

    array_resize(thinker_pointers, count);

    for (int i = 0; i < count; ++i)
    {
        thinker_t *thinker = (thinker_t *)table[i];
        thinker_class_t tc = GetThinkerClass(thinker->function.p1);
        if (tc == tc_none)
        {
            I_Printf(VB_WARNING,
                     "PrepareArchiveThinkers: Unknown thinker class");
        }

        thinker_pointer_t pointer = {.tc = tc, .p.integer = table[i]};
        thinker_pointers[i] = pointer;
    }

    free(table);
}

static void ArchiveThinkers(void)
{
    json_mut_t *arr = JS_NewArray(doc);

    array_foreach_type(pointer, thinker_pointers, thinker_pointer_t)
    {
        json_mut_t *data = NULL;

        switch (pointer->tc)
        {
            case tc_mobj:
            case tc_mobj_del:
                data = write_mobj_t(pointer->p.mobj);
                break;
            case tc_ceiling:
            case tc_ceiling_del:
                data = write_ceiling_t(pointer->p.ceiling);
                break;
            case tc_door:
            case tc_door_del:
                data = write_vldoor_t(pointer->p.door);
                break;
            case tc_floor:
            case tc_floor_del:
                data = write_floormove_t(pointer->p.floor);
                break;
            case tc_plat:
            case tc_plat_del:
                data = write_plat_t(pointer->p.plat);
                break;
            case tc_flash:
                data = write_lightflash_t(pointer->p.flash);
                break;
            case tc_strobe:
                data = write_strobe_t(pointer->p.strobe);
                break;
            case tc_glow:
                data = write_glow_t(pointer->p.glow);
                break;
            case tc_elevator:
            case tc_elevator_del:
                data = write_elevator_t(pointer->p.elevator);
                break;
            case tc_scroll:
            case tc_param_scroll_floor:
            case tc_param_scroll_ceiling:
                data = write_scroll_t(pointer->p.scroll);
                break;
            case tc_pusher:
                data = write_pusher_t(pointer->p.pusher);
                break;
            case tc_flicker:
                data = write_fireflicker_t(pointer->p.flicker);
                break;
            case tc_friction:
                data = write_friction_t(pointer->p.friction);
                break;
            case tc_ambient:
                data = write_ambient_t(pointer->p.ambient);
                break;
            case tc_none:
                data = write_thinker_t(pointer->p.thinker);
                break;
        }

        if (data)
        {
            json_mut_t *obj = JS_NewObject(doc);
            JS_SetInt(doc, obj, "class", pointer->tc);
            JS_SetObject(doc, obj, "thinker", data);
            JS_ArrayAddObject(doc, arr, obj);
        }
    }

    JS_SetArray(doc, root_mut, "thinkers", arr);
}

// Pass 1 of unarchiving: allocate memory for every thinker and build the
// index table so that cross-references between thinkers can be resolved in
// the second pass (UnArchiveThinkers).
static void PrepareUnArchiveThinkers(json_t *thinkers)
{
    int count = JS_GetArraySize(thinkers);
    for (int i = 0; i < count; ++i)
    {
        json_t *thinker = JS_GetArrayItem(thinkers, i);
        thinker_pointer_t pointer;
        pointer.tc = JS_GetIntegerValue(thinker, "class");
        switch (pointer.tc)
        {
            case tc_mobj:
            case tc_mobj_del:
                pointer.p.mobj = arena_alloc(thinkers_arena, mobj_t);
                break;
            case tc_ceiling:
            case tc_ceiling_del:
                pointer.p.ceiling = arena_alloc(thinkers_arena, ceiling_t);
                break;
            case tc_door:
            case tc_door_del:
                pointer.p.door = arena_alloc(thinkers_arena, vldoor_t);
                break;
            case tc_floor:
            case tc_floor_del:
                pointer.p.floor = arena_alloc(thinkers_arena, floormove_t);
                break;
            case tc_plat:
            case tc_plat_del:
                pointer.p.plat = arena_alloc(thinkers_arena, plat_t);
                break;
            case tc_flash:
                pointer.p.flash = arena_alloc(thinkers_arena, lightflash_t);
                break;
            case tc_strobe:
                pointer.p.strobe = arena_alloc(thinkers_arena, strobe_t);
                break;
            case tc_glow:
                pointer.p.glow = arena_alloc(thinkers_arena, glow_t);
                break;
            case tc_elevator:
            case tc_elevator_del:
                pointer.p.elevator = arena_alloc(thinkers_arena, elevator_t);
                break;
            case tc_scroll:
            case tc_param_scroll_floor:
            case tc_param_scroll_ceiling:
                pointer.p.scroll = arena_alloc(thinkers_arena, scroll_t);
                break;
            case tc_pusher:
                pointer.p.pusher = arena_alloc(thinkers_arena, pusher_t);
                break;
            case tc_flicker:
                pointer.p.flicker = arena_alloc(thinkers_arena, fireflicker_t);
                break;
            case tc_friction:
                pointer.p.friction = arena_alloc(thinkers_arena, friction_t);
                break;
            case tc_ambient:
                pointer.p.ambient = arena_alloc(thinkers_arena, ambient_t);
                break;
            case tc_none:
                pointer.p.thinker = arena_alloc(thinkers_arena, thinker_t);
                break;
            default:
                I_Error("Unknown class: %d", pointer.tc);
                break;
        }
        array_push(thinker_pointers, pointer);
    }
}

// Pass 2 of unarchiving: fill in all fields, including cross-thinker pointer
// fields (target, tracer, …) that could not be resolved until all thinkers
// were allocated in pass 1.
static void UnArchiveThinkers(json_t *thinkers)
{
    int count = array_size(thinker_pointers);

    for (int i = 0; i < count; i++)
    {
        thinker_pointer_t *pointer = &thinker_pointers[i];
        json_t *wrapper = JS_GetArrayItem(thinkers, i);
        json_t *data = JS_GetObject(wrapper, "thinker");
        switch (pointer->tc)
        {
            case tc_mobj:
            case tc_mobj_del:
                read_mobj_t(pointer->p.mobj, pointer->tc, data);
                P_AddThingTID(pointer->p.mobj, pointer->p.mobj->tid);
                break;
            case tc_ceiling:
            case tc_ceiling_del:
                read_ceiling_t(pointer->p.ceiling, pointer->tc, data);
                break;
            case tc_door:
            case tc_door_del:
                read_vldoor_t(pointer->p.door, pointer->tc, data);
                break;
            case tc_floor:
            case tc_floor_del:
                read_floormove_t(pointer->p.floor, pointer->tc, data);
                break;
            case tc_plat:
            case tc_plat_del:
                read_plat_t(pointer->p.plat, pointer->tc, data);
                break;
            case tc_flash:
                read_lightflash_t(pointer->p.flash, data);
                break;
            case tc_strobe:
                read_strobe_t(pointer->p.strobe, data);
                break;
            case tc_glow:
                read_glow_t(pointer->p.glow, data);
                break;
            case tc_elevator:
            case tc_elevator_del:
                read_elevator_t(pointer->p.elevator, pointer->tc, data);
                break;
            case tc_scroll:
            case tc_param_scroll_floor:
            case tc_param_scroll_ceiling:
                read_scroll_t(pointer->p.scroll, pointer->tc, data);
                break;
            case tc_pusher:
                read_pusher_t(pointer->p.pusher, data);
                break;
            case tc_flicker:
                read_fireflicker_t(pointer->p.flicker, data);
                break;
            case tc_friction:
                read_friction_t(pointer->p.friction, data);
                break;
            case tc_ambient:
                read_ambient_t(pointer->p.ambient, data);
                break;
            case tc_none:
                read_thinker_t(pointer->p.thinker, pointer->tc, data);
                break;
        }
    }
}

//
// MSecNodes
//

static void ArchiveMSecNodes(void)
{
    json_mut_t *msecnodes_arr = JS_NewArray(doc);

    uintptr_t *table = M_ArenaTable(msecnodes_arena);
    int count = M_ArenaTableSize(msecnodes_arena);
    for (int i = 0; i < count; ++i)
    {
        json_mut_t *msecnode_obj = write_msecnode_t((msecnode_t *)table[i]);
        JS_ArrayAddObject(doc, msecnodes_arr, msecnode_obj);
    }
    free(table);

    JS_SetArray(doc, root_mut, "msecnodes", msecnodes_arr);
}

static void PrepareUnArchiveMSecNodes(json_t *msecnodes_arr)
{
    int count = JS_GetArraySize(msecnodes_arr);

    array_resize(msecnode_pointers, count);
    for (int i = 0; i < count; ++i)
    {
        msecnode_t *node = arena_alloc(msecnodes_arena, msecnode_t);
        msecnode_pointers[i] = (uintptr_t)node;
    }
}

static void UnArchiveMSecNodes(json_t *msecnodes_arr)
{
    int count = JS_GetArraySize(msecnodes_arr);

    for (int i = 0; i < count; ++i)
    {
        json_t *msecnode_obj = JS_GetArrayItem(msecnodes_arr, i);

        read_msecnode_t((msecnode_t *)msecnode_pointers[i], msecnode_obj);
    }
}

//
// Players
//

static void ArchivePlayers(void)
{
    json_mut_t *players_arr = JS_NewArray(doc);

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        json_mut_t *player_obj = JS_NewObject(doc);
        if (playeringame[i])
        {
            player_obj = write_player_t(&players[i]);
        }
        JS_ArrayAddObject(doc, players_arr, player_obj);
    }

    JS_SetArray(doc, root_mut, "players", players_arr);
}

static void UnArchivePlayers(void)
{
    json_t *players_arr = JS_GetObject(root, "players");

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        json_t *player_obj = JS_GetArrayItem(players_arr, i);

        if (JS_GetObjectSize(player_obj) > 0)
        {
            read_player_t(&players[i], player_obj);
        }
    }
}

//
// Blocklinks
//

static void ArchiveBlocklinks(void)
{
    json_mut_t *bmap_arr = JS_NewArray(doc);

    int blocklinks_count = bmapwidth * bmapheight;
    for (int i = 0; i < blocklinks_count; ++i)
    {
        json_mut_t *blocklinks_arr = JS_NewArray(doc);
        for (mobj_t *mobj = blocklinks[i]; mobj; mobj = mobj->bnext)
        {
            JS_ArrayAddInt(doc, blocklinks_arr, writep_mobj(mobj));
        }
        JS_ArrayAddObject(doc, bmap_arr, blocklinks_arr);
    }

    JS_SetArray(doc, root_mut, "blocklinks", bmap_arr);
}

static void UnArchiveBlocklinks(void)
{
    json_t *bmap_arr = JS_GetObject(root, "blocklinks");

    int blocklinks_count = bmapwidth * bmapheight;
    for (int i = 0; i < blocklinks_count; ++i)
    {
        blocklinks[i] = NULL;

        json_t *blocklinks_arr = JS_GetArrayItem(bmap_arr, i);
        int count = JS_GetArraySize(blocklinks_arr);
        if (count)
        {
            mobj_t *mobj;
            mobj_t **bprev = &blocklinks[i];
            while (count--)
            {
                json_t *mobj_obj = JS_GetArrayItem(blocklinks_arr, count);

                mobj = readp_mobj(JS_GetInteger(mobj_obj));
                *bprev = mobj;
                if (mobj)
                {
                    mobj->bprev = bprev;
                    mobj->bnext = NULL;
                    bprev = &mobj->bnext;
                }
            }
        }
    }
}

//
// CeilingList
//

static void ArchiveCeilingList(void)
{
    json_mut_t *ceilinglist_arr = JS_NewArray(doc);

    uintptr_t *table = M_ArenaTable(activeceilings_arena);
    int count = M_ArenaTableSize(activeceilings_arena);
    for (int i = 0; i < count; ++i)
    {
        ceilinglist_t *cl = (ceilinglist_t *)table[i];
        JS_ArrayAddInt(doc, ceilinglist_arr,
                       writep_thinker(&cl->ceiling->thinker));
    }
    free(table);

    JS_SetArray(doc, root_mut, "ceilinglist", ceilinglist_arr);
}

static void PrepareUnArchiveCeilingList(json_t *ceilinglist_arr)
{
    int count = JS_GetArraySize(ceilinglist_arr);
    array_resize(ceilinglist_pointers, count);
    for (int i = 0; i < count; ++i)
    {
        ceilinglist_t *cl = arena_alloc(activeceilings_arena, ceilinglist_t);
        ceilinglist_pointers[i] = (uintptr_t)cl;
    }
}

static void UnArchiveCeilingList(json_t *ceilinglist_arr)
{
    int count = array_size(ceilinglist_pointers);
    ceilinglist_t *cl, **prev;
    prev = &activeceilings;
    for (int i = 0; i < count; ++i)
    {
        json_t *ceilinglist_obj = JS_GetArrayItem(ceilinglist_arr, i);

        cl = (ceilinglist_t *)ceilinglist_pointers[i];
        cl->ceiling =
            (ceiling_t *)readp_thinker(JS_GetInteger(ceilinglist_obj));
        *prev = cl;
        cl->prev = prev;
        cl->next = NULL;
        prev = &cl->next;
    }
}

//
// PlatList
//

static void ArchivePlatList(void)
{
    json_mut_t *platlist_arr = JS_NewArray(doc);

    uintptr_t *table = M_ArenaTable(activeplats_arena);
    int count = M_ArenaTableSize(activeplats_arena);
    for (int i = 0; i < count; ++i)
    {
        platlist_t *cl = (platlist_t *)table[i];
        JS_ArrayAddInt(doc, platlist_arr, writep_thinker(&cl->plat->thinker));
    }
    free(table);

    JS_SetArray(doc, root_mut, "platlist", platlist_arr);
}

static void PrepareUnArchivePlatList(json_t *platlist_arr)
{
    int count = JS_GetArraySize(platlist_arr);
    array_resize(platlist_pointers, count);
    for (int i = 0; i < count; ++i)
    {
        platlist_t *pl = arena_alloc(activeplats_arena, platlist_t);
        platlist_pointers[i] = (uintptr_t)pl;
    }
}

static void UnArchivePlatList(json_t *platlist_arr)
{
    int count = array_size(platlist_pointers);
    platlist_t *pl, **prev;
    prev = &activeplats;
    for (int i = 0; i < count; ++i)
    {
        json_t *platlist_obj = JS_GetArrayItem(platlist_arr, i);

        pl = (platlist_t *)platlist_pointers[i];
        pl->plat = (plat_t *)readp_thinker(JS_GetInteger(platlist_obj));
        *prev = pl;
        pl->prev = prev;
        pl->next = NULL;
        prev = &pl->next;
    }
}

//
// Buttons
//

static void ArchiveButtons(void)
{
    json_mut_t *buttonlists_arr = JS_NewArray(doc);

    for (int i = 0; i < MAXBUTTONS; i++)
    {
        json_mut_t *buttonlist_obj = write_button_t(&buttonlist[i]);
        JS_ArrayAddObject(doc, buttonlists_arr, buttonlist_obj);
    }

    JS_SetArray(doc, root_mut, "buttonlist", buttonlists_arr);
}

static void UnArchiveButtons(void)
{
    json_t *buttonlists_arr = JS_GetObject(root, "buttonlist");

    for (int i = 0; i < MAXBUTTONS; ++i)
    {
        json_t *buttonlist_obj = JS_GetArrayItem(buttonlists_arr, i);

        read_button_t(&buttonlist[i], buttonlist_obj);
    }
}

//
// Automap
//

static void ArchiveAutoMap(void)
{
    json_mut_t *automap_obj = JS_NewObject(doc);

    JS_SetInt(doc, automap_obj, "automapactive", automapactive);
    JS_SetInt(doc, automap_obj, "viewactive", viewactive);
    JS_SetInt(doc, automap_obj, "followplayer", followplayer);
    JS_SetInt(doc, automap_obj, "automap_grid", automap_grid);

    json_mut_t *markpoints_arr = JS_NewArray(doc);
    if (markpointnum)
    {
        for (int i = 0; i < markpointnum; ++i)
        {
            json_mut_t *markpoint_arr = JS_NewArray(doc);

            JS_ArrayAddInt(doc, markpoint_arr, markpoints[i].x);
            JS_ArrayAddInt(doc, markpoint_arr, markpoints[i].y);

            JS_ArrayAddObject(doc, markpoints_arr, markpoint_arr);
        }
    }
    JS_SetArray(doc, automap_obj, "markpoints", markpoints_arr);

    JS_SetObject(doc, root_mut, "automap", automap_obj);
}

static void UnArchiveAutoMap(void)
{
    json_t *automap_obj = JS_GetObject(root, "automap");

    automapactive = JS_GetIntegerValue(automap_obj, "automapactive");
    viewactive = JS_GetIntegerValue(automap_obj, "viewactive");
    followplayer = JS_GetIntegerValue(automap_obj, "followplayer");
    automap_grid = JS_GetIntegerValue(automap_obj, "automap_grid");

    if (automapactive)
    {
        AM_Start();
    }

    json_t *markpoints_arr = JS_GetObject(automap_obj, "markpoints");
    markpointnum = JS_GetArraySize(markpoints_arr);

    if (markpointnum)
    {
        while (markpointnum >= markpointnum_max)
        {
            markpointnum_max = markpointnum_max ? markpointnum_max * 2 : 16;
            markpoints =
                Z_Realloc(markpoints, sizeof(*markpoints) * markpointnum_max,
                          PU_STATIC, 0);
        }

        for (int i = 0; i < markpointnum; ++i)
        {
            json_t *markpoint_arr = JS_GetArrayItem(markpoints_arr, i);

            markpoints[i].x = JS_GetInteger(JS_GetArrayItem(markpoint_arr, 0));
            markpoints[i].y = JS_GetInteger(JS_GetArrayItem(markpoint_arr, 1));
        }
    }
}

static void EndArchive(void)
{
    array_free(thinker_pointers);
}

static void StartUnArchive(void)
{
    M_ArenaClear(thinkers_arena);
    M_ArenaClear(msecnodes_arena);
    M_ArenaClear(activeceilings_arena);
    M_ArenaClear(activeplats_arena);
}

static void EndUnArchive(void)
{
    array_free(thinker_pointers);
    array_free(msecnode_pointers);
    array_free(ceilinglist_pointers);
    array_free(platlist_pointers);
}

#define MAX_STREAM_LENGTH (1 << 28) // 256 MiB

static int CheckStreamLength(int32_t length)
{
    return length > 0 && length < MAX_STREAM_LENGTH;
}

void P_ArchiveKeyframe(void)
{
    doc = JS_NewDoc();
    root_mut = JS_NewObject(doc);
    JS_SetRoot(doc, root_mut);

    PrepareArchiveThinkers();

    json_mut_t *thinkercap_obj = write_thinker_t(&thinkercap);
    JS_SetObject(doc, root_mut, "thinkercap", thinkercap_obj);

    json_mut_t *thinkerclasscaps_arr = JS_NewArray(doc);
    for (int i = 0; i < NUMTHCLASS; ++i)
    {
        json_mut_t *thinkerclasscap_obj = write_thinker_t(&thinkerclasscap[i]);
        JS_ArrayAddObject(doc, thinkerclasscaps_arr, thinkerclasscap_obj);
    }
    JS_SetArray(doc, root_mut, "thinkerclasscaps", thinkerclasscaps_arr);
    JS_SetInt(doc, root_mut, "headsecnode", writep_msecnode(headsecnode));

    ArchiveDirty();
    ArchiveWorld();

    // p_map.h
    JS_SetInt(doc, root_mut, "floatok", floatok);
    JS_SetInt(doc, root_mut, "felldown", felldown);
    JS_SetInt(doc, root_mut, "tmfloorz", tmfloorz);
    JS_SetInt(doc, root_mut, "tmceilingz", tmceilingz);
    JS_SetInt(doc, root_mut, "hangsolid", hangsolid);

    JS_SetIdx(doc, root_mut, "ceilingline", ceilingline, lines);
    JS_SetIdx(doc, root_mut, "floorline", floorline, lines);
    JS_SetInt(doc, root_mut, "linetarget", writep_mobj(linetarget));
    JS_SetInt(doc, root_mut, "sector_list", writep_msecnode(sector_list));
    JS_SetIdx(doc, root_mut, "blockline", blockline, lines);

    json_mut_t *tmbbox_arr = JS_NewArray(doc);
    for (int i = 0; i < arrlen(tmbbox); ++i)
    {
        JS_ArrayAddInt(doc, tmbbox_arr, tmbbox[i]);
    }
    JS_SetArray(doc, root_mut, "tmbbox", tmbbox_arr);

    // p_maputil.h
    JS_SetInt(doc, root_mut, "opentop", opentop);
    JS_SetInt(doc, root_mut, "openbottom", openbottom);
    JS_SetInt(doc, root_mut, "openrange", openrange);
    JS_SetInt(doc, root_mut, "lowfloor", lowfloor);

    json_mut_t *trace_obj = write_divline_t(&trace);
    JS_SetObject(doc, root_mut, "trace", trace_obj);

    // p_setup.h
    ArchiveBlocklinks();

    // p_spec.h
    ArchivePlayers();
    ArchiveThinkers();
    ArchiveMSecNodes();

    JS_SetInt(doc, root_mut, "activeceilings",
              writep_activeceilings(activeceilings));
    ArchiveCeilingList();
    JS_SetInt(doc, root_mut, "activeplats", writep_activeplats(activeplats));
    ArchivePlatList();

    json_mut_t *rng_obj = write_rng_t(&rng);
    JS_SetObject(doc, root_mut, "rng", rng_obj);

    ArchiveButtons();
    ArchiveAutoMap();

    EndArchive();

    // Serialise the document to a JSON string, then free it – the string
    // owns its own memory and is independent of the JSON document.
    size_t json_len;
    char *json_str = JS_DocWriteString(doc, &json_len);
    JS_FreeDoc(doc);
    root_mut = NULL;
    doc = NULL;

    // Compress the JSON string with miniz and write the result to the save
    // buffer as: [uint32 original_len][uint32 compressed_len][zlib stream].
    // If compression fails or is disabled, fall back to plain JSON with a
    // null terminator so older code can still read it.
    unsigned char *compressed = NULL;

#ifndef KF_NO_COMPRESS
    mz_ulong compressed_len = mz_compressBound((mz_ulong)json_len);
    if ((compressed = malloc((size_t)compressed_len)))
    {
        int mz_ret =
            mz_compress(compressed, &compressed_len,
                        (const unsigned char *)json_str, (mz_ulong)json_len);

        if (mz_ret == MZ_OK && CheckStreamLength((int32_t)json_len)
            && CheckStreamLength((int32_t)compressed_len))
        {
            free(json_str);
            saveg_write32((int32_t)json_len);
            saveg_write32((int32_t)compressed_len);
            saveg_grow(compressed_len);
            memcpy(save_p, compressed, (size_t)compressed_len);
            save_p += compressed_len;
        }
        else
        {
            I_Printf(VB_ERROR, "P_ArchiveKeyframe: Compression error (%s)",
                     mz_error(mz_ret));

            free(compressed);
            compressed = NULL;
        }
    }
#endif

    if (!compressed)
    {
        I_Printf(VB_WARNING, "P_ArchiveKeyframe: Saving uncompressed keyframe");

        json_len++; // include null-terminator
        saveg_grow(json_len);
        M_StringCopy((char *)save_p, json_str, json_len);
        free(json_str);
        save_p += json_len;
    }
    else
    {
        free(compressed);
    }
}

#define ZLIB_MAGIC_BYTE 0x78

static int CheckZlibHeader(uint8_t *c)
{
    return c[0] == ZLIB_MAGIC_BYTE && ((c[0] << 8) + c[1]) % 31 == 0;
}

void P_UnArchiveKeyframe(void)
{
    if (!saveg_check_size(2 * sizeof(int32_t) + sizeof(int16_t)))
    {
        I_Error("Corrupt savegame file");
    }

    // Detect whether the keyframe is compressed or plain JSON.
    //
    // Compressed: [uint32 original_len][uint32 compressed_len][zlib stream]
    // Plain: [JSON text][NUL]
    //
    // A valid zlib stream always begins with the CMF byte 0x78 followed by a
    // FLG byte such that (CMF << 8 | FLG) % 31 == 0.  A JSON stream always
    // starts with ASCII whitespace or '{' / '[', so the first byte can never
    // be 0x78, making this an unambiguous discriminator.
    //
    // We speculatively read the two 32-bit header fields and peek at the two
    // bytes that follow.  If they look like a valid zlib header we proceed
    // with decompression; otherwise we rewind save_p and parse as plain JSON.
    byte *orig_save_p = save_p;
    mz_ulong json_len = (mz_ulong)saveg_read32();
    int32_t compressed_len = saveg_read32();

    if (CheckStreamLength((int32_t)json_len)
        && CheckStreamLength((int32_t)compressed_len)
        && CheckZlibHeader(save_p))
    {
        // Compressed path: decompress into a temporary buffer, parse it,
        // then free the buffer (JS_OpenString copies all strings internally).
        unsigned char *decomp = malloc((size_t)json_len);
        if (!decomp)
        {
            I_Error("Out of memory");
        }
        int mz_ret =
            mz_uncompress(decomp, &json_len, (const unsigned char *)save_p,
                          (mz_ulong)compressed_len);
        if (mz_ret != MZ_OK)
        {
            I_Error("Decompression error (%s)", mz_error(mz_ret));
        }
        root = JS_OpenString((char *)decomp, (size_t)json_len);
        free(decomp);
        if (!root)
        {
            I_Error("Error parsing JSON");
        }
        save_p += compressed_len;
    }
    else
    {
        // Plain JSON path: rewind to before the speculative header read
        // and parse the null-terminated string directly from save_p.
        save_p = orig_save_p;
        json_len = (mz_ulong)strlen((char *)save_p);
        root = JS_OpenString((char *)save_p, (size_t)json_len);
        if (!root)
        {
            I_Error("Error parsing JSON");
        }
        save_p += json_len + 1;
    }

    StartUnArchive();

    json_t *thinkers_obj = JS_GetObject(root, "thinkers");
    PrepareUnArchiveThinkers(thinkers_obj);

    json_t *thinkercap_obj = JS_GetObject(root, "thinkercap");
    read_thinker_t(&thinkercap, tc_none, thinkercap_obj);

    json_t *thinkerclasscaps_arr = JS_GetObject(root, "thinkerclasscaps");
    for (int i = 0; i < NUMTHCLASS; ++i)
    {
        json_t *thinkerclasscap_obj = JS_GetArrayItem(thinkerclasscaps_arr, i);

        read_thinker_t(&thinkerclasscap[i], tc_none, thinkerclasscap_obj);
    }

    json_t *msecnodes_arr = JS_GetObject(root, "msecnodes");
    PrepareUnArchiveMSecNodes(msecnodes_arr);

    json_t *headsecnode_obj = JS_GetObject(root, "headsecnode");
    headsecnode = readp_msecnode(JS_GetInteger(headsecnode_obj));

    UnArchiveDirty();

    UnArchiveWorld();

    // p_map.h
    floatok = JS_GetIntegerValue(root, "floatok");
    felldown = JS_GetIntegerValue(root, "felldown");
    tmfloorz = JS_GetIntegerValue(root, "tmfloorz");
    tmceilingz = JS_GetIntegerValue(root, "tmceilingz");
    hangsolid = JS_GetIntegerValue(root, "hangsolid");

    JS_GetIdx(ceilingline, lines, root, "ceilingline");
    JS_GetIdx(floorline, lines, root, "floorline");
    linetarget = readp_mobj(JS_GetIntegerValue(root, "linetarget"));
    sector_list = readp_msecnode(JS_GetIntegerValue(root, "sector_list"));
    JS_GetIdx(blockline, lines, root, "blockline");

    json_t *tmbbox_arr = JS_GetObject(root, "tmbbox");
    for (int i = 0; i < arrlen(tmbbox); ++i)
    {
        json_t *tmbbox_obj = JS_GetArrayItem(tmbbox_arr, i);
        tmbbox[i] = JS_GetInteger(tmbbox_obj);
    }

    // p_maputil.h
    opentop = JS_GetIntegerValue(root, "opentop");
    openbottom = JS_GetIntegerValue(root, "openbottom");
    openrange = JS_GetIntegerValue(root, "openrange");
    lowfloor = JS_GetIntegerValue(root, "lowfloor");

    json_t *trace_obj = JS_GetObject(root, "trace");
    read_divline_t(&trace, trace_obj);

    // p_setup.h
    UnArchiveBlocklinks();

    // p_spec.h
    json_t *ceilinglist_arr = JS_GetObject(root, "ceilinglist");
    PrepareUnArchiveCeilingList(ceilinglist_arr);
    json_t *platlist_arr = JS_GetObject(root, "platlist");
    PrepareUnArchivePlatList(platlist_arr);

    UnArchivePlayers();

    UnArchiveThinkers(thinkers_obj);

    UnArchiveMSecNodes(msecnodes_arr);

    json_t *activeceilings_obj = JS_GetObject(root, "activeceilings");
    activeceilings = readp_activeceilings(JS_GetInteger(activeceilings_obj));
    UnArchiveCeilingList(ceilinglist_arr);

    json_t *activeplats_obj = JS_GetObject(root, "activeplats");
    activeplats = readp_activeplats(JS_GetInteger(activeplats_obj));
    UnArchivePlatList(platlist_arr);

    json_t *rng_obj = JS_GetObject(root, "rng");
    read_rng_t(&rng, rng_obj);
    UnArchiveButtons();

    UnArchiveAutoMap();

    EndUnArchive();

    // JS_OpenString() registers the parsed document under lump index NO_INDEX
    // (-1) so that JS_CloseOptions() can find and free it here.
    JS_CloseOptions(NO_INDEX);
    root = NULL;
}

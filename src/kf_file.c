//
// Copyright(C) 2025 Roman Fomin
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

#include <stddef.h>
#include <stdint.h>

// temp
#include "yyjson.h"
#include <stdio.h>

static json_mut_doc_t *doc;
static json_mut_t *root;

inline static void write8_internal(const int8_t data[], int count)
{
    saveg_grow(sizeof(int8_t) * count);
    for (int i = 0; i < count; ++i)
    {
        savep_putbyte(data[i]);
    }
}

#define write8(...)                                       \
    write8_internal((const int8_t[]){__VA_ARGS__},        \
                    sizeof((const int8_t[]){__VA_ARGS__}) \
                        / sizeof(const int8_t))

inline static void write16_internal(const int16_t data[], int count)
{
    saveg_grow(sizeof(int16_t) * count);
    for (int i = 0; i < count; ++i)
    {
        savep_putbyte(data[i] & 0xff);
        savep_putbyte((data[i] >> 8) & 0xff);
    }
}

#define write16(...)                                        \
    write16_internal((const int16_t[]){__VA_ARGS__},        \
                     sizeof((const int16_t[]){__VA_ARGS__}) \
                         / sizeof(const int16_t))

inline static void write32_internal(const int32_t data[], int count)
{
    saveg_grow(sizeof(int32_t) * count);
    for (int i = 0; i < count; ++i)
    {
        savep_putbyte(data[i] & 0xff);
        savep_putbyte((data[i] >> 8) & 0xff);
        savep_putbyte((data[i] >> 16) & 0xff);
        savep_putbyte((data[i] >> 24) & 0xff);
    }
}

#define write32(...)                                        \
    write32_internal((const int32_t[]){__VA_ARGS__},        \
                     sizeof((const int32_t[]){__VA_ARGS__}) \
                         / sizeof(const int32_t))

#define read8 saveg_read8
#define read16 saveg_read16
#define read32 saveg_read32
#define read64 saveg_read64
#define write64 saveg_write64

#define read_enum read32
#define write_enum write32

enum
{
    null_index = -1,
    head_index = -2,
    dummy_index = -3,
    th_delete_index = -4,
    th_misc_index = -5,
    th_friends_index = -6,
    th_enemies_index = -7,
};

static int tclass_to_index[] = {
    [th_delete] = th_delete_index,
    [th_misc] = th_misc_index,
    [th_friends] = th_friends_index,
    [th_enemies] = th_enemies_index,
};

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
    [tc_none] = NULL
};

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

static thinker_pointer_t *thinker_pointers;
static uintptr_t *msecnode_pointers;
static uintptr_t *ceilinglist_pointers;
static uintptr_t *platlist_pointers;

#define readp_index(ptr, base)                                 \
    do                                                         \
    {                                                          \
        int index = read32();                                  \
        (ptr) = (index != null_index) ? (base) + index : NULL; \
    } while (0)

#define writep_index(ptr, base)                          \
    do                                                   \
    {                                                    \
        int index = (ptr) ? (ptr) - (base) : null_index; \
        write32(index);                                  \
    } while (0)

#define JS_SetIdx(doc, obj, key, ptr, base)              \
    do                                                   \
    {                                                    \
        JS_SetInt(doc, obj, key,                         \
                  ptr ? (int)(ptr - base) : null_index); \
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

static thinker_t *readp_thinker(void)
{
    int index = read32();
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

static mobj_t *readp_mobj(void)
{
    return (mobj_t *)readp_thinker();
}

static int writep_mobj(const mobj_t *mobj)
{
    return writep_thinker(&mobj->thinker);
}

static msecnode_t *readp_msecnode(void)
{
    int index = read32();
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

static ceilinglist_t *readp_activeceilings(void)
{
    int index = read32();
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

static platlist_t *readp_activeplats(void)
{
    int index = read32();
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

static void read_mapthing_t(mapthing_t *str)
{
    str->x = read32();
    str->y = read32();
    str->height = read32();
    str->angle = read16();
    str->type = read16();
    str->options = read32();
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

static thinker_t *readp_thclass()
{
    int index = read32();
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

static void read_thinker_t(thinker_t *str, thinker_class_t tc)
{
    str->prev = readp_thinker();
    str->next = readp_thinker();
    str->function.p1 = actions[tc];
    str->cnext = readp_thclass();
    str->cprev = readp_thclass();
    str->references = read32();
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

static void read_mobj_t(mobj_t *str, thinker_class_t tc)
{
    read_thinker_t(&str->thinker, tc);
    str->x = read32();
    str->y = read32();
    str->z = read32();

    // Done in UnArchiveThingList
    // str->snext;
    // str->sprev;

    str->angle = read32();
    str->sprite = read_enum();
    str->frame = read32();

    // Done in UnArchiveBlocklinks
    // str->bnext;
    // str->bprev;

    readp_index(str->subsector, subsectors);
    str->floorz = read32();
    str->ceilingz = read32();
    str->dropoffz = read32();
    str->radius = read32();
    str->height = read32();
    str->momx = read32();
    str->momy = read32();
    str->momz = read32();
    str->validcount = read32();
    str->type = read_enum();
    readp_index(str->info, mobjinfo);
    str->tics = read32();
    readp_index(str->state, states);
    str->flags = read32();
    str->flags2 = read32();
    str->flags_extra = read32();
    str->intflags = read32();
    str->health = read32();
    str->id = read32();
    str->special = read32();
    str->args[0] = read32();
    str->args[1] = read32();
    str->args[2] = read32();
    str->args[3] = read32();
    str->args[4] = read32();
    str->movedir = read16();
    str->movecount = read16();
    str->strafecount = read16();
    str->target = readp_mobj();
    str->reactiontime = read16();
    str->threshold = read16();
    str->pursuecount = read16();
    str->gear = read16();
    readp_index(str->player, players);
    str->lastlook = read16();
    read_mapthing_t(&str->spawnpoint);
    str->tracer = readp_mobj();
    str->lastenemy = readp_mobj();
    str->above_thing = readp_mobj();
    str->below_thing = readp_mobj();
    str->friction = read32();
    str->movefactor = read32();
    str->touching_sectorlist = readp_msecnode();
    str->interp = read32();
    str->oldx = read32();
    str->oldy = read32();
    str->oldz = read32();
    str->oldangle = read32();
    str->bloodcolor = read32();

    // TODO
    // P_SetActualHeight(str);
}

static json_mut_t *write_mobj_t(mobj_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    json_mut_t *thinker = write_thinker_t(&str->thinker);
    JS_SetObject(doc, obj, "thinker", thinker);

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
    JS_SetInt(doc, obj, "id", str->id);
    JS_SetInt(doc, obj, "special", str->special);

    json_mut_t *args_arr = JS_NewArray(doc);
    for (int i = 0; i < 5; ++i)
        JS_ArrayAddInt(doc, args_arr, str->args[i]);
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

    json_mut_t *spawnpoint = write_mapthing_t(&str->spawnpoint);
    JS_SetObject(doc, obj, "spawnpoint", spawnpoint);

    JS_SetInt(doc, obj, "tracer", writep_mobj(str->tracer));
    JS_SetInt(doc, obj, "lastenemy", writep_mobj(str->lastenemy));
    JS_SetInt(doc, obj, "above_thing", writep_mobj(str->above_thing));
    JS_SetInt(doc, obj, "below_thing", writep_mobj(str->below_thing));
    JS_SetInt(doc, obj, "friction", str->friction);
    JS_SetInt(doc, obj, "movefactor", str->movefactor);
    JS_SetInt(doc, obj, "touching_sectorlist", writep_msecnode(str->touching_sectorlist));
    JS_SetInt(doc, obj, "interp", str->interp);
    JS_SetInt(doc, obj, "oldx", str->oldx);
    JS_SetInt(doc, obj, "oldy", str->oldy);
    JS_SetInt(doc, obj, "oldz", str->oldz);
    JS_SetInt(doc, obj, "oldangle", str->oldangle);
    JS_SetInt(doc, obj, "bloodcolor", str->bloodcolor);

    return obj;
}

static void read_ticcmd_t(ticcmd_t *str)
{
    str->forwardmove = read8();
    str->sidemove = read8();
    str->angleturn = read16();
    str->consistancy = read16();
    str->chatchar = read8();
    str->buttons = read8();
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

static void read_pspdef_t(pspdef_t *str)
{
    readp_index(str->state, states);
    str->tics = read32();
    str->sx = read32();
    str->sy = read32();
    str->sx2 = read32();
    str->sy2 = read32();
    str->oldsx2 = read32();
    str->oldsy2 = read32();
    str->sxf = read32();
    str->syf = read32();
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

static void read_player_t(player_t *str)
{
    str->mo = readp_mobj();
    str->playerstate = read_enum();
    read_ticcmd_t(&str->cmd);
    str->viewz = read32();
    str->viewheight = read32();
    str->deltaviewheight = read32();
    str->bob = read32();
    str->momx = read32();
    str->momy = read32();
    str->health = read32();
    str->armorpoints = read32();
    str->armortype = read32();
    for (int i = 0; i < NUMPOWERS; ++i)
    {
        str->powers[i] = read32();
    }
    for (int i = 0; i < NUMCARDS; ++i)
    {
        str->cards[i] = read32();
    }
    str->backpack = read32();
    for (int i = 0; i < MAXPLAYERS; ++i)
    {
        str->frags[i] = read32();
    }
    str->readyweapon = read_enum();
    str->pendingweapon = read_enum();
    for (int i = 0; i < NUMWEAPONS; ++i)
    {
        str->weaponowned[i] = read32();
    }
    for (int i = 0; i < NUMAMMO; ++i)
    {
        str->ammo[i] = read32();
    }
    for (int i = 0; i < NUMAMMO; ++i)
    {
        str->maxammo[i] = read32();
    }
    str->attackdown = read32();
    str->usedown = read32();
    str->cheats = read32();
    str->refire = read32();
    str->killcount = read32();
    str->itemcount = read32();
    str->secretcount = read32();
    // TODO
    str->message = NULL;
    str->damagecount = read32();
    str->bonuscount = read32();
    str->attacker = readp_mobj();
    str->extralight = read32();
    str->fixedcolormap = read32();
    str->colormap = read32();
    for (int i = 0; i < NUMPSPRITES; ++i)
    {
        read_pspdef_t(&str->psprites[i]);
    }
    str->secretmessage = NULL;
    str->didsecret = read32();
    str->oldviewz = read32();
    str->pitch = read32();
    str->oldpitch = read32();
    str->slope = read32();
    str->maxkilldiscount = read32();

    str->num_visitedlevels = read32();
    array_clear(str->visitedlevels);
    for (int i = 0; i < str->num_visitedlevels; ++i)
    {
        level_t level = {0};
        level.episode = read32();
        level.map = read32();
        array_push(str->visitedlevels, level);
    }

    str->lastweapon = read_enum();
    str->nextweapon = read_enum();
    str->switching = read_enum();
}

static json_mut_t *write_player_t(player_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    JS_SetInt(doc, obj, "mo", writep_mobj(str->mo));
    JS_SetInt(doc, obj, "playerstate", str->playerstate);

    json_mut_t *cmd_obj = write_ticcmd_t(&str->cmd);
    JS_SetObject(doc, obj, "ticcmd", cmd_obj);

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
        json_mut_t *pspsprite_obj = write_pspdef_t(&str->psprites[i]);
        JS_ArrayAddObject(doc, psprites_arr, pspsprite_obj);
    }
    JS_SetArray(doc, obj, "psprites", psprites_arr);

    JS_SetInt(doc, obj, "didsecret", str->didsecret);
    JS_SetInt(doc, obj, "oldviewz", str->oldviewz);
    JS_SetInt(doc, obj, "pitch", str->pitch);
    JS_SetInt(doc, obj, "oldpitch", str->oldpitch);
    JS_SetInt(doc, obj, "slope", str->slope);
    JS_SetInt(doc, obj, "maxkilldiscount", str->maxkilldiscount);

    json_mut_t *visitedlevels_arr = JS_NewArray(doc);
    level_t *level;
    array_foreach(level, str->visitedlevels)
    {
        json_mut_t *visitedlevel_obj = JS_NewArray(doc);

        JS_SetInt(doc, visitedlevel_obj, "episode", level->episode);
        JS_SetInt(doc, visitedlevel_obj, "map", level->map);

        JS_ArrayAddObject(doc, visitedlevels_arr, visitedlevel_obj);
    }

    JS_SetInt(doc, obj, "lastweapon", str->lastweapon);
    JS_SetInt(doc, obj, "nextweapon", str->nextweapon);
    JS_SetInt(doc, obj, "switching", str->switching);

    return obj;
}

static void read_ceiling_t(ceiling_t *str, thinker_class_t tc)
{
    read_thinker_t(&str->thinker, tc);
    str->type = read_enum();
    readp_index(str->sector, sectors);
    str->bottomheight = read32();
    str->topheight = read32();
    str->speed = read32();
    str->oldspeed = read32();
    str->crush = read32();
    str->newspecial = read32();
    str->oldspecial = read32();
    str->texture = read16();
    str->direction = read32();
    str->tag = read32();
    str->olddirection = read32();
    str->list = readp_activeceilings();
}

static json_mut_t *write_ceiling_t(ceiling_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    json_mut_t *thinker = write_thinker_t(&str->thinker);
    JS_SetObject(doc, obj, "thinker", thinker);

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

static void read_vldoor_t(vldoor_t *str, thinker_class_t tc)
{
    read_thinker_t(&str->thinker, tc);
    str->type = read_enum();
    readp_index(str->sector, sectors);
    str->topheight = read32();
    str->speed = read32();
    str->direction = read32();
    str->topwait = read32();
    str->topcountdown = read32();
    readp_index(str->line, lines);
    str->lighttag = read32();
}

static json_mut_t *write_vldoor_t(vldoor_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    json_mut_t *thinker = write_thinker_t(&str->thinker);
    JS_SetObject(doc, obj, "thinker", thinker);

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

static void read_floormove_t(floormove_t *str, thinker_class_t tc)
{
    read_thinker_t(&str->thinker, tc);
    str->type = read_enum();
    str->crush = read32();
    readp_index(str->sector, sectors);
    str->direction = read32();
    str->newspecial = read32();
    str->oldspecial = read32();
    str->texture = read16();
    str->floordestheight = read32();
    str->speed = read32();
}

static json_mut_t *write_floormove_t(floormove_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    json_mut_t *thinker = write_thinker_t(&str->thinker);
    JS_SetObject(doc, obj, "thinker", thinker);

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

static void read_plat_t(plat_t *str, thinker_class_t tc)
{
    read_thinker_t(&str->thinker, tc);
    readp_index(str->sector, sectors);
    str->speed = read32();
    str->low = read32();
    str->high = read32();
    str->wait = read32();
    str->count = read32();
    str->status = read_enum();
    str->oldstatus = read_enum();
    str->crush = read32();
    str->tag = read32();
    str->type = read_enum();
    str->list = readp_activeplats();
}

static json_mut_t *write_plat_t(plat_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    json_mut_t *thinker = write_thinker_t(&str->thinker);
    JS_SetObject(doc, obj, "thinker", thinker);

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

static void read_lightflash_t(lightflash_t *str)
{
    read_thinker_t(&str->thinker, tc_flash);
    readp_index(str->sector, sectors);
    str->count = read32();
    str->maxlight = read32();
    str->minlight = read32();
    str->maxtime = read32();
    str->mintime = read32();
}

static json_mut_t *write_lightflash_t(lightflash_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    json_mut_t *thinker = write_thinker_t(&str->thinker);
    JS_SetObject(doc, obj, "thinker", thinker);

    JS_SetIdx(doc, obj, "sector", str->sector, sectors);
    JS_SetInt(doc, obj, "count", str->count);
    JS_SetInt(doc, obj, "maxlight", str->maxlight);
    JS_SetInt(doc, obj, "minlight", str->minlight);
    JS_SetInt(doc, obj, "maxtime", str->maxtime);
    JS_SetInt(doc, obj, "mintime", str->mintime);

    return obj;
}

static void read_strobe_t(strobe_t *str)
{
    read_thinker_t(&str->thinker, tc_strobe);
    readp_index(str->sector, sectors);
    str->count = read32();
    str->minlight = read32();
    str->maxlight = read32();
    str->darktime = read32();
    str->brighttime = read32();
}

static json_mut_t *write_strobe_t(strobe_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    json_mut_t *thinker = write_thinker_t(&str->thinker);
    JS_SetObject(doc, obj, "thinker", thinker);

    JS_SetIdx(doc, obj, "sector", str->sector, sectors);
    JS_SetInt(doc, obj, "count", str->count);
    JS_SetInt(doc, obj, "minlight", str->minlight);
    JS_SetInt(doc, obj, "maxlight", str->maxlight);
    JS_SetInt(doc, obj, "darktime", str->darktime);
    JS_SetInt(doc, obj, "brighttime", str->brighttime);

    return obj;
}

static void read_glow_t(glow_t *str)
{
    read_thinker_t(&str->thinker, tc_glow);
    readp_index(str->sector, sectors);
    str->minlight = read32();
    str->maxlight = read32();
    str->direction = read32();
}

static json_mut_t *write_glow_t(glow_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    json_mut_t *thinker = write_thinker_t(&str->thinker);
    JS_SetObject(doc, obj, "thinker", thinker);

    JS_SetIdx(doc, obj, "sector", str->sector, sectors);
    JS_SetInt(doc, obj, "minlight", str->minlight);
    JS_SetInt(doc, obj, "maxlight", str->maxlight);
    JS_SetInt(doc, obj, "direction", str->direction);

    return obj;
}

static void read_fireflicker_t(fireflicker_t *str)
{
    read_thinker_t(&str->thinker, tc_flicker);
    readp_index(str->sector, sectors);
    str->count = read32();
    str->maxlight = read32();
    str->minlight = read32();
}

static json_mut_t *write_fireflicker_t(fireflicker_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    json_mut_t *thinker = write_thinker_t(&str->thinker);
    JS_SetObject(doc, obj, "thinker", thinker);

    JS_SetIdx(doc, obj, "sector", str->sector, sectors);
    JS_SetInt(doc, obj, "count", str->count);
    JS_SetInt(doc, obj, "maxlight", str->maxlight);
    JS_SetInt(doc, obj, "minlight", str->minlight);

    return obj;
}

static void read_elevator_t(elevator_t *str, thinker_class_t tc)
{
    read_thinker_t(&str->thinker, tc);
    str->type = read_enum();
    readp_index(str->sector, sectors);
    str->direction = read32();
    str->floordestheight = read32();
    str->ceilingdestheight = read32();
    str->speed = read32();
}

static json_mut_t *write_elevator_t(elevator_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    json_mut_t *thinker = write_thinker_t(&str->thinker);
    JS_SetObject(doc, obj, "thinker", thinker);

    JS_SetInt(doc, obj, "type", str->type);
    JS_SetIdx(doc, obj, "sector", str->sector, sectors);
    JS_SetInt(doc, obj, "direction", str->direction);
    JS_SetInt(doc, obj, "floordestheight", str->floordestheight);
    JS_SetInt(doc, obj, "ceilingdestheight", str->ceilingdestheight);
    JS_SetInt(doc, obj, "speed", str->speed);

    return obj;
}

static void read_scroll_t(scroll_t *str, thinker_class_t tc)
{
    read_thinker_t(&str->thinker, tc);
    str->dx = read32();
    str->dy = read32();
    str->affectee = read32();
    str->control = read32();
    str->last_height = read32();
    str->vdx = read32();
    str->vdy = read32();
    str->accel = read32();
    str->type = read_enum();
}

static json_mut_t *write_scroll_t(scroll_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    json_mut_t *thinker = write_thinker_t(&str->thinker);
    JS_SetObject(doc, obj, "thinker", thinker);

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

static void read_pusher_t(pusher_t *str)
{
    read_thinker_t(&str->thinker, tc_pusher);
    str->type = read_enum();
    str->source = readp_mobj();
    str->x_mag = read32();
    str->y_mag = read32();
    str->magnitude = read32();
    str->radius = read32();
    str->x = read32();
    str->y = read32();
    str->affectee = read32();
}

static json_mut_t *write_pusher_t(pusher_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    json_mut_t *thinker = write_thinker_t(&str->thinker);
    JS_SetObject(doc, obj, "thinker", thinker);

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

static void read_friction_t(friction_t *str)
{
    read_thinker_t(&str->thinker, tc_friction);
    str->friction = read32();
    str->movefactor = read32();
    str->affectee = read32();
}

static json_mut_t *write_friction_t(friction_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    json_mut_t *thinker = write_thinker_t(&str->thinker);
    JS_SetObject(doc, obj, "thinker", thinker);

    JS_SetInt(doc, obj, "friction", str->friction);
    JS_SetInt(doc, obj, "movefactor", str->movefactor);
    JS_SetInt(doc, obj, "affectee", str->affectee);

    return obj;
}

static void read_ambient_data_t(ambient_data_t *str)
{
    str->type = read_enum();
    str->mode = read_enum();
    str->close_dist = read32();
    str->clipping_dist = read32();
    str->min_tics = read32();
    str->max_tics = read32();
    str->volume_scale = read32();
    str->sfx_id = read32();
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

static void read_ambient_t(ambient_t *str)
{
    read_thinker_t(&str->thinker, tc_ambient);
    str->source = readp_mobj();
    str->origin = readp_mobj();
    read_ambient_data_t(&str->data);
    str->update_tics = AMB_UPDATE_NOW;
    str->wait_tics = read32();
    str->active = read32();
    str->playing = false;
    str->offset = (float)FixedToDouble(read32());
    str->last_offset = (float)FixedToDouble(read32());
    str->last_leveltime = read32();
}

static json_mut_t *write_ambient_t(ambient_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    json_mut_t *thinker = write_thinker_t(&str->thinker);
    JS_SetObject(doc, obj, "thinker", thinker);

    JS_SetInt(doc, obj, "source", writep_mobj(str->source));
    JS_SetInt(doc, obj, "origin", writep_mobj(str->origin));

    json_mut_t *ambient_data = write_ambient_data_t(&str->data);
    JS_SetObject(doc, obj, "ambient_data", ambient_data);

    JS_SetInt(doc, obj, "wait_tics", str->wait_tics);
    JS_SetInt(doc, obj, "active", str->active);
    JS_SetInt(doc, obj, "offset", DoubleToFixed(str->offset));
    JS_SetInt(doc, obj, "last_offset", DoubleToFixed(str->last_offset));
    JS_SetInt(doc, obj, "last_leveltime", str->last_leveltime);

    return obj;
}

static void read_rng_t(rng_t *str)
{
    for (int i = 0; i < NUMPRCLASS; ++i)
    {
        str->seed[i] = read32();
    }
    str->rndindex = read32();
    str->prndindex = read32();
}

static void write_rng_t(rng_t *str)
{
    for (int i = 0; i < NUMPRCLASS; ++i)
    {
        write32(str->seed[i]);
    }
    write32(str->rndindex);
    write32(str->prndindex);
}

static void read_button_t(button_t *str)
{
    readp_index(str->line, lines);
    str->where = read_enum();
    str->btexture = read32();
    str->btimer = read32();
}

static void write_button_t(button_t *str)
{
    writep_index(str->line, lines);
    write_enum(str->where);
    write32(str->btexture);
    write32(str->btimer);
}


static void read_msecnode_t(msecnode_t *str)
{
    readp_index(str->m_sector, sectors);
    str->m_thing = readp_mobj();
    str->m_tprev = readp_msecnode();
    str->m_tnext = readp_msecnode();
    str->m_sprev = readp_msecnode();
    str->m_snext = readp_msecnode();
    str->visited = read32();
}

static json_mut_t *write_msecnode_t(msecnode_t *str)
{
    json_mut_t *obj = JS_NewObject(doc);

    JS_SetIdx(doc, obj, "sector", str->m_sector, sectors);
    JS_SetInt(doc, obj, "m_thing", writep_mobj(str->m_thing));
    JS_SetInt(doc, obj, "m_tprev", writep_msecnode(str->m_tprev));
    JS_SetInt(doc, obj, "m_tnext", writep_msecnode(str->m_tnext));
    JS_SetInt(doc, obj, "m_sprev", writep_msecnode(str->m_sprev));
    JS_SetInt(doc, obj, "m_snext", writep_msecnode(str->m_snext));
    JS_SetInt(doc, obj, "visited", str->visited);

    return obj;
}

static void read_divline_t(divline_t *str)
{
    str->x = read32();
    str->y = read32();
    str->dx = read32();
    str->dy = read32();
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

static void read_partial_side_t(partial_side_t *str)
{
    str->textureoffset = read32();
    str->rowoffset = read32();
    str->toptexture = read16();
    str->bottomtexture = read16();
    str->midtexture = read16();
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
    JS_SetArray(doc, root, "dirty_lines", lines_arr);

    json_mut_t *sides_arr = JS_NewArray(doc);
    array_foreach_type(ds, dirty_sides, dirty_side_t)
    {
        json_mut_t *side_obj = JS_NewObject(doc);

        JS_SetIdx(doc, side_obj, "side", ds->side, sides);
        json_mut_t *partial_side = write_partial_side_t(&ds->clean_side);
        JS_SetObject(doc, side_obj, "partial_side", partial_side);

        JS_ArrayAddObject(doc, sides_arr, side_obj);
    }
    JS_SetArray(doc, root, "dirty_sides", sides_arr);
}

static void UnArchiveDirty(void)
{
    int count = read32();
    int oldcount = array_size(dirty_lines);
    for (int i = count; i < oldcount; ++i)
    {
        P_CleanLine(&dirty_lines[i]);
    }
    array_resize(dirty_lines, count);
    array_foreach_type(dl, dirty_lines, dirty_line_t)
    {
        dl->line = lines + read32();
        dl->line->dirty = true;
        dl->clean_line.special = read16();
    }

    count = read32();
    oldcount = array_size(dirty_sides);
    for (int i = count; i < oldcount; ++i)
    {
        P_CleanSide(&dirty_sides[i]);
    }
    array_resize(dirty_sides, count);
    array_foreach_type(ds, dirty_sides, dirty_side_t)
    {
        ds->side = sides + read32();
        ds->side->dirty = true;
        read_partial_side_t(&ds->clean_side);
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

inline static void UnArchiveThingList(sector_t *sector)
{
    sector->thinglist = NULL;
    int count = read32();
    if (!count)
    {
        return;
    }

    mobj_t *mobj, **sprev;
    sprev = &sector->thinglist;
    while (count--)
    {
        mobj = readp_mobj();
        *sprev = mobj;
        mobj->sprev = sprev;
        mobj->snext = NULL;
        sprev = &mobj->snext;
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
        JS_SetInt(doc, sector_obj, "ceiling_rotation", sector->ceiling_rotation);
        JS_SetInt(doc, sector_obj, "tint", sector->tint);

        JS_SetInt(doc, sector_obj, "floorpic", sector->floorpic);
        JS_SetInt(doc, sector_obj, "ceilingpic", sector->ceilingpic);
        JS_SetInt(doc, sector_obj, "lightlevel", sector->lightlevel);
        JS_SetInt(doc, sector_obj, "special", sector->special); // needed?   yes -- transfer types
        JS_SetInt(doc, sector_obj, "tag", sector->tag);         // needed?   need them -- killough 

        // Woof!
        JS_SetInt(doc, sector_obj, "soundtarget", writep_mobj(sector->soundtarget));
        JS_SetInt(doc, sector_obj, "floordata", writep_thinker(sector->floordata));
        JS_SetInt(doc, sector_obj, "ceilingdata", writep_thinker(sector->ceilingdata));

        json_mut_t *thinglist = ArchiveThingList(sector);
        JS_SetArray(doc, sector_obj, "thinglist", thinglist);

        JS_SetInt(doc, sector_obj, "touching_thinglist", writep_msecnode(sector->touching_thinglist));

        JS_ArrayAddObject(doc, sectors_arr, sector_obj);
    }
    JS_SetArray(doc, root, "sectors", sectors_arr);

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
    JS_SetArray(doc, root, "lines", lines_arr);
}

static void UnArchiveWorld(void)
{
    int i;
    sector_t *sector;

    // do sectors
    for (i = 0, sector = sectors; i < numsectors; i++, sector++)
    {
        sector->floorheight = read32();
        sector->ceilingheight = read32();
        sector->floor_xoffs = read32();
        sector->floor_yoffs = read32();
        sector->ceiling_xoffs = read32();
        sector->ceiling_yoffs = read32();
        sector->floor_rotation = read32();
        sector->ceiling_rotation = read32();
        sector->tint = read32();

        sector->floorpic = read16();
        sector->ceilingpic = read16();
        sector->lightlevel = read16();
        sector->special = read16();
        sector->tag = read16();

        // Woof!
        sector->soundtarget = readp_mobj();
        sector->floordata = readp_thinker();
        sector->ceilingdata = readp_thinker();
        UnArchiveThingList(sector);
        sector->touching_thinglist = readp_msecnode();
    }

    line_t *line;

    for (i = 0, line = lines; i < numlines; i++, line++)
    {
        line->flags = read16();
        line->special = read16();

        for (int j = 0; j < 2; j++)
        {
            if (line->sidenum[j] != NO_INDEX)
            {
                side_t *side = &sides[line->sidenum[j]];

                // killough 10/98: load full sidedef offsets, including
                // fractions
                
                side->textureoffset = read32();
                side->rowoffset = read32();
                
                side->toptexture = read16();
                side->bottomtexture = read16();
                side->midtexture = read16();
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

        thinker_pointer_t pointer = {
            .tc = tc,
            .p.integer = table[i]
        };
        thinker_pointers[i] = pointer;
    }

    free(table);
}

static json_mut_t *ArchiveThinkers(void)
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

    return arr;
}

static void PrepareUnArchiveThinkers(void)
{
    int count = read32();
    while (count--)
    {
        thinker_pointer_t pointer;
        pointer.tc = read8();
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

static void UnArchiveThinkers(void)
{
    array_foreach_type(pointer, thinker_pointers, thinker_pointer_t)
    {
        switch (pointer->tc)
        {
            case tc_mobj:
            case tc_mobj_del:
                read_mobj_t(pointer->p.mobj, pointer->tc);
                break;
            case tc_ceiling:
            case tc_ceiling_del:
                read_ceiling_t(pointer->p.ceiling, pointer->tc);
                break;
            case tc_door:
            case tc_door_del:
                read_vldoor_t(pointer->p.door, pointer->tc);
                break;
            case tc_floor:
            case tc_floor_del:
                read_floormove_t(pointer->p.floor, pointer->tc);
                break;
            case tc_plat:
            case tc_plat_del:
                read_plat_t(pointer->p.plat, pointer->tc);
                break;
            case tc_flash:
                read_lightflash_t(pointer->p.flash);
                break;
            case tc_strobe:
                read_strobe_t(pointer->p.strobe);
                break;
            case tc_glow:
                read_glow_t(pointer->p.glow);
                break;
            case tc_elevator:
            case tc_elevator_del:
                read_elevator_t(pointer->p.elevator, pointer->tc);
                break;
            case tc_scroll:
            case tc_param_scroll_floor:
            case tc_param_scroll_ceiling:
                read_scroll_t(pointer->p.scroll, pointer->tc);
                break;
            case tc_pusher:
                read_pusher_t(pointer->p.pusher);
                break;
            case tc_flicker:
                read_fireflicker_t(pointer->p.flicker);
                break;
            case tc_friction:
                read_friction_t(pointer->p.friction);
                break;
            case tc_ambient:
                read_ambient_t(pointer->p.ambient);
                break;
            case tc_none:
                read_thinker_t(pointer->p.thinker, pointer->tc);
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

    JS_SetArray(doc, root, "msecnodes", msecnodes_arr);
}

static void PrepareUnArchiveMSecNodes(void)
{
    int count = read32();
    array_resize(msecnode_pointers, count);
    for (int i = 0; i < count; ++i)
    {
        msecnode_t *node = arena_alloc(msecnodes_arena, msecnode_t);
        msecnode_pointers[i] = (uintptr_t)node;
    }
}

static void UnArchiveMSecNodes(void)
{
    int count = array_size(msecnode_pointers);
    for (int i = 0; i < count; ++i)
    {
        read_msecnode_t((msecnode_t *)msecnode_pointers[i]);
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
    JS_SetArray(doc, root, "players", players_arr);
}

static void UnArchivePlayers(void)
{
    for (int i = 0; i < MAXPLAYERS; i++)
    {
        if (playeringame[i])
        {
            read_player_t(&players[i]);
        }
    }
}

//
// Blocklinks
//

static void ArchiveBlocklinks(void)
{
    int blocklinks_count = bmapwidth * bmapheight;
    json_mut_t *bmap_arr = JS_NewArray(doc);
    for (int i = 0; i < blocklinks_count; ++i)
    {
        json_mut_t *blocklinks_arr = JS_NewArray(doc);
        for (mobj_t *mobj = blocklinks[i]; mobj; mobj = mobj->bnext)
        {
            JS_ArrayAddInt(doc, blocklinks_arr, writep_mobj(mobj));
        }
        JS_ArrayAddObject(doc, bmap_arr, blocklinks_arr);
    }
    JS_SetArray(doc, root, "blocklinks", bmap_arr);
}

static void UnArchiveBlocklinks(void)
{
    int blocklinks_count = bmapwidth * bmapheight;
    for (int i = 0; i < blocklinks_count; ++i)
    {
        blocklinks[i] = NULL;

        int count = read32();
        if (count)
        {
            mobj_t *mobj;
            mobj_t **bprev = &blocklinks[i];
            while (count--)
            {
                mobj = readp_mobj();
                *bprev = mobj;
                mobj->bprev = bprev;
                mobj->bnext = NULL;
                bprev = &mobj->bnext;
            }
        }
    }
}

//
// CeilingList
//

static void ArchiveCeilingList(void)
{
    uintptr_t *table = M_ArenaTable(activeceilings_arena);
    int count = M_ArenaTableSize(activeceilings_arena);
    for (int i = 0; i < count; ++i)
    {
        ceilinglist_t *cl = (ceilinglist_t *)table[i];
        writep_thinker(&cl->ceiling->thinker);
    }
    free(table);
}

static void PrepareUnArchiveCeilingList(void)
{
    int count = read32();
    array_resize(ceilinglist_pointers, count);
    for (int i = 0; i < count; ++i)
    {
        ceilinglist_t *cl = arena_alloc(activeceilings_arena, ceilinglist_t);
        ceilinglist_pointers[i] = (uintptr_t)cl;
    }
}

static void UnArchiveCeilingList(void)
{
    int count = array_size(ceilinglist_pointers);
    ceilinglist_t *cl, **prev;
    prev = &activeceilings;
    for (int i = 0; i < count; ++i)
    {
        cl = (ceilinglist_t *)ceilinglist_pointers[i];
        cl->ceiling = (ceiling_t *)readp_thinker();
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
    uintptr_t *table = M_ArenaTable(activeplats_arena);
    int count = M_ArenaTableSize(activeplats_arena);
    for (int i = 0; i < count; ++i)
    {
        platlist_t *cl = (platlist_t *)table[i];
        writep_thinker(&cl->plat->thinker);
    }
    free(table);
}

static void PrepareUnArchivePlatList(void)
{
    int count = read32();
    array_resize(platlist_pointers, count);
    for (int i = 0; i < count; ++i)
    {
        platlist_t *pl = arena_alloc(activeplats_arena, platlist_t);
        platlist_pointers[i] = (uintptr_t)pl;
    }
}

static void UnArchivePlatList(void)
{
    int count = array_size(platlist_pointers);
    platlist_t *pl, **prev;
    prev = &activeplats;
    for (int i = 0; i < count; ++i)
    {
        pl = (platlist_t *)platlist_pointers[i];
        pl->plat = (plat_t *)readp_thinker();
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
    for (int i = 0; i < MAXBUTTONS; i++)
    {
        write_button_t(&buttonlist[i]);
    }
}

static void UnArchiveButtons(void)
{
    for (int i = 0; i < MAXBUTTONS; ++i)
    {
        read_button_t(&buttonlist[i]);
    }
}

//
// Automap
//

static void ArchiveAutoMap(void)
{
    write32(automapactive,
            viewactive,
            followplayer,
            automap_grid);

    write32(markpointnum);

    if (markpointnum)
    {
        for (int i = 0; i < markpointnum; ++i)
        {
            write64(markpoints[i].x);
            write64(markpoints[i].y);
        }
    }
}

static void UnArchiveAutoMap(void)
{
    automapactive = read32();
    viewactive = read32();
    followplayer = read32();
    automap_grid = read32();

    if (automapactive)
    {
        AM_Start();
    }

    markpointnum = read32();

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
            markpoints[i].x = read64();
            markpoints[i].y = read64();
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

void P_ArchiveKeyframe(void)
{
    doc = JS_NewDoc();
    root = JS_NewObject(doc);
    JS_SetRoot(doc, root);

    PrepareArchiveThinkers();

    json_mut_t *thinkercap_obj = write_thinker_t(&thinkercap);
    JS_SetObject(doc, root, "thinkercap", thinkercap_obj);

    json_mut_t *thinkerclasscaps_arr = JS_NewArray(doc);
    for (int i = 0; i < NUMTHCLASS; ++i)
    {
        json_mut_t *thinkerclasscap_obj = write_thinker_t(&thinkerclasscap[i]);
        JS_ArrayAddObject(doc, thinkerclasscaps_arr, thinkerclasscap_obj);
    }
    JS_SetArray(doc, root, "thinkerclasscaps", thinkerclasscaps_arr);

    JS_SetInt(doc, root, "headsecnode", writep_msecnode(headsecnode));

    ArchiveDirty();

    ArchiveWorld();

    // p_map.h
    JS_SetInt(doc, root, "floatok", floatok);
    JS_SetInt(doc, root, "felldown", felldown);
    JS_SetInt(doc, root, "tmfloorz", tmfloorz);
    JS_SetInt(doc, root, "tmceilingz", tmceilingz);
    JS_SetInt(doc, root, "hangsolid", hangsolid);

    JS_SetIdx(doc, root, "ceilingline", ceilingline, lines);
    JS_SetIdx(doc, root, "floorline", floorline, lines);
    JS_SetInt(doc, root, "linetarget", writep_mobj(linetarget));
    JS_SetInt(doc, root, "sector_list", writep_msecnode(sector_list));
    JS_SetIdx(doc, root, "blockline", blockline, lines);

    json_mut_t *tmbbox_arr = JS_NewArray(doc);
    for (int i = 0; i < arrlen(tmbbox); ++i)
        JS_ArrayAddInt(doc, tmbbox_arr, tmbbox[i]);
    JS_SetArray(doc, root, "tmbbox", tmbbox_arr);

    // p_maputil.h
    JS_SetInt(doc, root, "opentop", opentop);
    JS_SetInt(doc, root, "openbottom", openbottom);
    JS_SetInt(doc, root, "openrange", openrange);
    JS_SetInt(doc, root, "lowfloor", lowfloor);

    json_mut_t *trace_obj = write_divline_t(&trace);
    JS_SetObject(doc, root, "trace", trace_obj);

    // p_setup.h
    ArchiveBlocklinks();

    // p_spec.h
    ArchivePlayers();

    json_mut_t *thinker_arr = ArchiveThinkers();
    JS_SetObject(doc, root, "thinkers", thinker_arr);

    ArchiveMSecNodes();

    puts(yyjson_mut_write(doc, YYJSON_WRITE_PRETTY, NULL));
    return;

    writep_activeceilings(activeceilings);
    ArchiveCeilingList();
    writep_activeplats(activeplats);
    ArchivePlatList();

    write_rng_t(&rng);
    ArchiveButtons();

    ArchiveAutoMap();

    EndArchive();
}

void P_UnArchiveKeyframe(void)
{
    StartUnArchive();

    PrepareUnArchiveThinkers();
    read_thinker_t(&thinkercap, tc_none);
    for (int i = 0; i < NUMTHCLASS; ++i)
    {
        read_thinker_t(&thinkerclasscap[i], tc_none);
    }

    PrepareUnArchiveMSecNodes();
    headsecnode = readp_msecnode();

    UnArchiveDirty();

    UnArchiveWorld();

    // p_map.h
    floatok = read32();
    felldown = read32();
    tmfloorz = read32();
    tmceilingz = read32();
    hangsolid = read32();

    readp_index(ceilingline, lines);
    readp_index(floorline, lines);
    linetarget = readp_mobj();
    sector_list = readp_msecnode();
    readp_index(blockline, lines);

    for (int i = 0; i < arrlen(tmbbox); ++i)
    {
        tmbbox[i] = read32();
    }

    // p_maputil.h
    opentop = read32();
    openbottom = read32();
    openrange = read32();
    lowfloor = read32();

    read_divline_t(&trace);

    // p_setup.h
    UnArchiveBlocklinks();

    // p_spec.h
    PrepareUnArchiveCeilingList();
    PrepareUnArchivePlatList();

    UnArchivePlayers();

    UnArchiveThinkers();

    UnArchiveMSecNodes();

    activeceilings = readp_activeceilings();
    UnArchiveCeilingList();
    activeplats = readp_activeplats();
    UnArchivePlatList();

    read_rng_t(&rng);
    UnArchiveButtons();

    UnArchiveAutoMap();

    EndUnArchive();
}

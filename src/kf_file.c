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

#include "d_player.h"
#include "d_think.h"
#include "doomdata.h"
#include "doomstat.h"
#include "doomtype.h"
#include "i_system.h"
#include "info.h"
#include "kf_hashmap.h"
#include "m_arena.h"
#include "m_array.h"
#include "m_fixed.h"
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
#include "s_sndinfo.h"

#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>

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
    tc_none = -1,
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
    tc_ambient
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
    [tc_ambient] = T_AmbientSoundAdapter
};

typedef struct
{
    uintptr_t value;
    thinker_class_t tc;
} thinker_pointer_t;

static thinker_pointer_t *thinker_pointers;
static uintptr_t *msecnode_pointers;
static uintptr_t *ceilinglist_pointers;
static uintptr_t *platlist_pointers;

static hashmap_t *thinker_hashmap;
static hashmap_t *msecnode_hashmap;
static hashmap_t *ceilinglist_hashmap;
static hashmap_t *platlist_hashmap;

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
        return (thinker_t *)thinker_pointers[index].value;
    }
}

static thinker_t *readp_thinker(void)
{
    int index = read32();
    return readp_thinker_index(index);
}

static void writep_thinker(const thinker_t *thinker)
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
        index = hashmap_get(thinker_hashmap, (uintptr_t)thinker);
    }
    write32(index);
}

static mobj_t *readp_mobj(void)
{
    return (mobj_t *)readp_thinker();
}

static void writep_mobj(const mobj_t *mobj)
{
    writep_thinker(&mobj->thinker);
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

static void writep_msecnode(const msecnode_t *node)
{
    int index;
    if (!node)
    {
        index = null_index;
    }
    else
    {
        index = hashmap_get(msecnode_hashmap, (uintptr_t)node);
    }
    write32(index);
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

static void writep_activeceilings(const ceilinglist_t *cl)
{
    int index;
    if (!cl)
    {
        index = null_index;
    }
    else
    {
        index = hashmap_get(ceilinglist_hashmap, (uintptr_t)cl);
    }
    write32(index);
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

static void writep_activeplats(const platlist_t *pl)
{
    int index;
    if (!pl)
    {
        index = null_index;
    }
    else
    {
        index = hashmap_get(platlist_hashmap, (uintptr_t)pl);
    }
    write32(index);
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

static void write_mapthing_t(mapthing_t *str)
{
    write32(str->x);
    write32(str->y);
    write32(str->height);
    write16(str->angle);
    write16(str->type);
    write32(str->options);
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

static void writep_thclass(thinker_t *str)
{
    int tclass;
    for (tclass = 0; tclass < NUMTHCLASS; ++tclass)
    {
        if (str == &thinkerclasscap[tclass])
        {
            write32(tclass_to_index[tclass]);
            return;
        }
    }
    writep_thinker(str);
}

static void read_thinker_t(thinker_t *str, thinker_class_t tc)
{
    str->prev = readp_thinker();
    str->next = readp_thinker();
    str->function.p1 = tc != tc_none ? actions[tc] : NULL;
    str->cnext = readp_thclass();
    str->cprev = readp_thclass();
    str->references = read32();
}

static void write_thinker_t(thinker_t *str)
{
    writep_thinker(str->prev);
    writep_thinker(str->next);
    writep_thclass(str->cprev);
    writep_thclass(str->cprev);
    write32(str->references);
}

inline static void UnArchiveTouchingSectorList(mobj_t *mobj);
inline static void ArchiveTouchingSectorList(const mobj_t *mobj);

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
    UnArchiveTouchingSectorList(str);
    str->interp = read32();
    str->oldx = read32();
    str->oldy = read32();
    str->oldz = read32();
    str->oldangle = read32();
    str->bloodcolor = read32();

    // TODO
    // P_SetActualHeight(str);
}

static void write_mobj_t(mobj_t *str)
{
    write_thinker_t(&str->thinker);
    write32(str->x);
    write32(str->y);
    write32(str->z);

    // Done in ArchiveThingList
    // str->snext;
    // str->sprev;

    write32(str->angle);
    write_enum(str->sprite);
    write32(str->frame);

    // Done in ArchiveBlocklinks
    // str->bnext;
    // str->bprev;

    writep_index(str->subsector, subsectors);
    write32(str->floorz);
    write32(str->ceilingz);
    write32(str->dropoffz);
    write32(str->radius);
    write32(str->height);
    write32(str->momx);
    write32(str->momy);
    write32(str->momz);
    write32(str->validcount);
    write_enum(str->type);
    writep_index(str->info, mobjinfo);
    write32(str->tics);
    writep_index(str->state, states);
    write32(str->flags);
    write32(str->flags2);
    write32(str->flags_extra);
    write32(str->intflags);
    write32(str->health);
    write16(str->movedir);
    write16(str->movecount);
    write16(str->strafecount);
    writep_mobj(str->target);
    write16(str->reactiontime);
    write16(str->threshold);
    write16(str->pursuecount);
    write16(str->gear);
    writep_index(str->player, players);
    write16(str->lastlook);
    write_mapthing_t(&str->spawnpoint);
    writep_mobj(str->tracer);
    writep_mobj(str->lastenemy);
    writep_mobj(str->above_thing);
    writep_mobj(str->below_thing);
    write32(str->friction);
    write32(str->movefactor);
    ArchiveTouchingSectorList(str);
    write32(str->interp);
    write32(str->oldx);
    write32(str->oldy);
    write32(str->oldz);
    write32(str->oldangle);
    write32(str->bloodcolor);
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

static void write_ticcmd_t(ticcmd_t *str)
{
    write8(str->forwardmove);
    write8(str->sidemove);
    write16(str->angleturn);
    write16(str->consistancy);
    write8(str->chatchar);
    write8(str->buttons);
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
}

static void write_pspdef_t(pspdef_t *str)
{
    writep_index(str->state, states);
    write32(str->tics);
    write32(str->sx);
    write32(str->sy);
    write32(str->sx2);
    write32(str->sy2);
    write32(str->oldsx2);
    write32(str->oldsy2);
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
}

static void write_player_t(player_t *str)
{
    writep_mobj(str->mo);
    write_enum(str->playerstate);
    write_ticcmd_t(&str->cmd);
    write32(str->viewz);
    write32(str->viewheight);
    write32(str->deltaviewheight);
    write32(str->bob);
    write32(str->momx);
    write32(str->momy);
    write32(str->health);
    write32(str->armorpoints);
    write32(str->armortype);
    for (int i = 0; i < NUMPOWERS; ++i)
    {
        write32(str->powers[i]);
    }
    for (int i = 0; i < NUMCARDS; ++i)
    {
        write32(str->cards[i]);
    }
    write32(str->backpack);
    for (int i = 0; i < MAXPLAYERS; ++i)
    {
        write32(str->frags[i]);
    }
    write_enum(str->readyweapon);
    write_enum(str->pendingweapon);
    for (int i = 0; i < NUMWEAPONS; ++i)
    {
        write32(str->weaponowned[i]);
    }
    for (int i = 0; i < NUMAMMO; ++i)
    {
        write32(str->ammo[i]);
    }
    for (int i = 0; i < NUMAMMO; ++i)
    {
        write32(str->maxammo[i]);
    }
    write32(str->attackdown);
    write32(str->usedown);
    write32(str->cheats);
    write32(str->refire);
    write32(str->killcount);
    write32(str->itemcount);
    write32(str->secretcount);
    // TODO
    // str->message;
    write32(str->damagecount);
    write32(str->bonuscount);
    writep_mobj(str->attacker);
    write32(str->extralight);
    write32(str->fixedcolormap);
    write32(str->colormap);
    for (int i = 0; i < NUMPSPRITES; ++i)
    {
        write_pspdef_t(&str->psprites[i]);
    }
    write32(str->didsecret);
    write32(str->oldviewz);
    write32(str->pitch);
    write32(str->oldpitch);
    write32(str->slope);
    write32(str->maxkilldiscount);

    write32(str->num_visitedlevels);
    level_t *level;
    array_foreach(level, str->visitedlevels)
    {
        write32(level->episode);
        write32(level->map);
    }

    write_enum(str->lastweapon);
    write_enum(str->nextweapon);
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

static void write_ceiling_t(ceiling_t *str)
{
    write_thinker_t(&str->thinker);
    write_enum(str->type);
    writep_index(str->sector, sectors);
    write32(str->bottomheight);
    write32(str->topheight);
    write32(str->speed);
    write32(str->oldspeed);
    write32(str->crush);
    write32(str->newspecial);
    write32(str->oldspecial);
    write16(str->texture);
    write32(str->direction);
    write32(str->tag);
    write32(str->olddirection);
    writep_activeceilings(str->list);
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

static void write_vldoor_t(vldoor_t *str)
{
    write_thinker_t(&str->thinker);
    write_enum(str->type);
    write32(str->sector - sectors);
    write32(str->topheight);
    write32(str->speed);
    write32(str->direction);
    write32(str->topwait);
    write32(str->topcountdown);
    writep_index(str->line, lines);
    write32(str->lighttag);
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

static void write_floormove_t(floormove_t *str)
{
    write_thinker_t(&str->thinker);
    write_enum(str->type);
    write32(str->crush);
    writep_index(str->sector, sectors);
    write32(str->direction);
    write32(str->newspecial);
    write32(str->oldspecial);
    write16(str->texture);
    write32(str->floordestheight);
    write32(str->speed);
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

static void write_plat_t(plat_t *str)
{
    write_thinker_t(&str->thinker);
    writep_index(str->sector, sectors);
    write32(str->speed);
    write32(str->low);
    write32(str->high);
    write32(str->wait);
    write32(str->count);
    write_enum(str->status);
    write_enum(str->oldstatus);
    write32(str->crush);
    write32(str->tag);
    write_enum(str->type);
    writep_activeplats(str->list);
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

static void write_lightflash_t(lightflash_t *str)
{
    write_thinker_t(&str->thinker);
    writep_index(str->sector, sectors);
    write32(str->count);
    write32(str->maxlight);
    write32(str->minlight);
    write32(str->maxtime);
    write32(str->mintime);
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

static void write_strobe_t(strobe_t *str)
{
    write_thinker_t(&str->thinker);
    writep_index(str->sector, sectors);
    write32(str->count);
    write32(str->minlight);
    write32(str->maxlight);
    write32(str->darktime);
    write32(str->brighttime);
}

static void read_glow_t(glow_t *str)
{
    read_thinker_t(&str->thinker, tc_glow);
    readp_index(str->sector, sectors);
    str->minlight = read32();
    str->maxlight = read32();
    str->direction = read32();
}

static void write_glow_t(glow_t *str)
{
    write_thinker_t(&str->thinker);
    write32(str->sector - sectors);
    write32(str->minlight);
    write32(str->maxlight);
    write32(str->direction);
}

static void read_fireflicker_t(fireflicker_t *str)
{
    read_thinker_t(&str->thinker, tc_flicker);
    readp_index(str->sector, sectors);
    str->count = read32();
    str->maxlight = read32();
    str->minlight = read32();
}

static void write_fireflicker_t(fireflicker_t *str)
{
    write_thinker_t(&str->thinker);
    writep_index(str->sector, sectors);
    write32(str->count);
    write32(str->maxlight);
    write32(str->minlight);
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

static void write_elevator_t(elevator_t *str)
{
    write_thinker_t(&str->thinker);
    write_enum(str->type);
    writep_index(str->sector, sectors);
    write32(str->direction);
    write32(str->floordestheight);
    write32(str->ceilingdestheight);
    write32(str->speed);
}

static void read_scroll_t(scroll_t *str)
{
    read_thinker_t(&str->thinker, tc_scroll);
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

static void write_scroll_t(scroll_t *str)
{
    write_thinker_t(&str->thinker);
    write32(str->dx);
    write32(str->dy);
    write32(str->affectee);
    write32(str->control);
    write32(str->last_height);
    write32(str->vdx);
    write32(str->vdy);
    write32(str->accel);
    write_enum(str->type);
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

static void write_pusher_t(pusher_t *str)
{
    write_thinker_t(&str->thinker);
    write_enum(str->type);
    writep_mobj(str->source);
    write32(str->x_mag);
    write32(str->y_mag);
    write32(str->magnitude);
    write32(str->radius);
    write32(str->x);
    write32(str->y);
    write32(str->affectee);
}

static void read_friction_t(friction_t *str)
{
    read_thinker_t(&str->thinker, tc_friction);
    str->friction = read32();
    str->movefactor = read32();
    str->affectee = read32();
}

static void write_friction_t(friction_t *str)
{
    write_thinker_t(&str->thinker);
    write32(str->friction);
    write32(str->movefactor);
    write32(str->affectee);
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

static void write_ambient_data_t(ambient_data_t *str)
{
    write_enum(str->type);
    write_enum(str->mode);
    write32(str->close_dist);
    write32(str->clipping_dist);
    write32(str->min_tics);
    write32(str->max_tics);
    write32(str->volume_scale);
    write32(str->sfx_id);
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
    str->playing = false;// read32();
    str->offset = (float)FixedToDouble(read32());
    str->last_offset = (float)FixedToDouble(read32());
    str->last_leveltime = read32();
}

static void write_ambient_t(ambient_t *str)
{
    write_thinker_t(&str->thinker);
    writep_mobj(str->source);
    writep_mobj(str->origin);
    write_ambient_data_t(&str->data);
    write32(str->wait_tics);
    write32(str->active);
    write32(DoubleToFixed(str->offset));
    write32(DoubleToFixed(str->last_offset));
    write32(str->last_leveltime);
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

static void write_msecnode_t(msecnode_t *str)
{
    writep_index(str->m_sector, sectors);
    // Don't write pointers to deleted thinkers
    if (str->m_thing
        && str->m_thing->thinker.function.p1 == P_RemoveMobjThinkerDelayed)
    {
        write32(null_index);
    }
    else
    {
        writep_mobj(str->m_thing);
    }
    writep_msecnode(str->m_tprev);
    writep_msecnode(str->m_tnext);
    writep_msecnode(str->m_sprev);
    writep_msecnode(str->m_snext);
    write32(str->visited);
}

static void read_divline_t(divline_t *str)
{
    str->x = read32();
    str->y = read32();
    str->dx = read32();
    str->dy = read32();
}

static void write_divline_t(divline_t *str)
{
    write32(str->x);
    write32(str->y);
    write32(str->dx);
    write32(str->dy);
}

static void read_partial_side_t(partial_side_t *str)
{
    str->textureoffset = read32();
    str->rowoffset = read32();
    str->toptexture = read16();
    str->bottomtexture = read16();
    str->midtexture = read16();
}

static void write_partial_side_t(partial_side_t *str)
{
    write32(str->textureoffset);
    write32(str->rowoffset);
    write16(str->toptexture);
    write16(str->bottomtexture);
    write16(str->midtexture);
}

//
// World
//

static void ArchiveDirty(void)
{
    int count = array_size(dirty_lines);
    write32(count);
    array_foreach_type(dl, dirty_lines, dirty_line_t)
    {
        writep_index(dl->line, lines);
        write16(dl->clean_line.special);
    }

    count = array_size(dirty_sides);
    write32(count);
    array_foreach_type(ds, dirty_sides, dirty_side_t)
    {
        writep_index(ds->side, sides);
        write_partial_side_t(&ds->clean_side);
    }
}

static void UnArchiveDirty(void)
{
    int count = read32();
    array_resize(dirty_lines, count);
    array_foreach_type(dl, dirty_lines, dirty_line_t)
    {
        readp_index(dl->line, lines);
        dl->line->dirty = true;
        dl->clean_line.special = read16();
    }

    count = read32();
    array_resize(dirty_sides, count);
    array_foreach_type(ds, dirty_sides, dirty_side_t)
    {
        readp_index(ds->side, sides);
        ds->side->dirty = true;
        read_partial_side_t(&ds->clean_side);
    }
}

inline static void ArchiveThingList(const sector_t *sector)
{
    int count = 0;
    for (mobj_t *mobj = sector->thinglist; mobj; mobj = mobj->snext)
    {
        ++count;
    }
    write32(count);
    for (mobj_t *mobj = sector->thinglist; mobj; mobj = mobj->snext)
    {
        writep_mobj(mobj);
    }
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

inline static void UnArchiveTouchingThingList(sector_t *sector);
inline static void ArchiveTouchingThingList(const sector_t *sector);

static void ArchiveWorld(void)
{
    int i;
    const sector_t *sector;

    // do sectors
    for (i = 0, sector = sectors; i < numsectors; i++, sector++)
    {
        // killough 10/98: save full floor & ceiling heights, including fraction
        write32(sector->floorheight,
                sector->ceilingheight,
                sector->floor_xoffs,
                sector->floor_yoffs,
                sector->ceiling_xoffs,
                sector->ceiling_yoffs,
                sector->floor_rotation,
                sector->ceiling_rotation,
                sector->tint);

        write16(sector->floorpic,
                sector->ceilingpic,
                sector->lightlevel,
                sector->special, // needed?   yes -- transfer types
                sector->tag);    // needed?   need them -- killough 

        // Woof!
        writep_mobj(sector->soundtarget);
        writep_thinker(sector->floordata);
        writep_thinker(sector->ceilingdata);
        ArchiveThingList(sector);
        ArchiveTouchingThingList(sector);
    }

    const line_t *line;

    int size = array_size(dirty_lines);
    write32(size);
    for (i = 0; i < size; ++i)
    {
        line = dirty_lines[i].line;
        write16(line->special);
    }

    const side_t *side;

    size = array_size(dirty_sides);
    write32(size);
    for (i = 0; i < size; ++i)
    {
        side = dirty_sides[i].side;

        write16(side->toptexture,
                side->bottomtexture,
                side->midtexture);

        write32(side->textureoffset,
                side->rowoffset);
    }
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
        UnArchiveTouchingThingList(sector);
    }

    line_t *line;

    int oldsize = read32();
    int size = array_size(dirty_lines);
    for (i = 0; i < size; ++i)
    {
        line = dirty_lines[i].line;
        if (i < oldsize)
        {
            line->special = read16();
        }
        else
        {
            P_CleanLine(&dirty_lines[i]);
        }
    }

    side_t *side;

    oldsize = read32();
    size = array_size(dirty_sides);
    for (i = 0; i < size; ++i)
    {
        side = dirty_sides[i].side;
        if (i < oldsize)
        {
            side->toptexture = read16();
            side->bottomtexture = read16();
            side->midtexture = read16();    
            side->textureoffset = read32();
            side->rowoffset = read32(); 
        }
        else
        {
            P_CleanSide(&dirty_sides[i]);
        }
    }
}

//
// Thinkers
//

static void PrepareArchiveThinkers(void)
{
    for (thinker_t *th = thinkercap.next; th != &thinkercap; th = th->next)
    {
        thinker_pointer_t pointer;
        thinker_class_t tc;
        for (tc = 0; tc < arrlen(actions); ++tc)
        {
            if (th->function.p1 == actions[tc])
            {
                pointer.tc = tc;
                break;
            }
        }
        if (tc == arrlen(actions))
        {
            I_Error("Thinker class not found");
        }
        pointer.value = (uintptr_t)th;

        array_push(thinker_pointers, pointer);
        hashmap_put(thinker_hashmap, pointer.value);
    }

    int count = array_size(thinker_pointers);
    write32(count);

    array_foreach_type(pointer, thinker_pointers, thinker_pointer_t)
    {
        write8(pointer->tc);
    }
}

static void ArchiveThinkers(void)
{
    array_foreach_type(pointer, thinker_pointers, thinker_pointer_t)
    {
        switch (pointer->tc)
        {
            case tc_mobj:
            case tc_mobj_del:
                write_mobj_t((mobj_t *)pointer->value);
                break;
            case tc_ceiling:
            case tc_ceiling_del:
                write_ceiling_t((ceiling_t *)pointer->value);
                break;
            case tc_door:
            case tc_door_del:
                write_vldoor_t((vldoor_t *)pointer->value);
                break;
            case tc_floor:
            case tc_floor_del:
                write_floormove_t((floormove_t *)pointer->value);
                break;
            case tc_plat:
            case tc_plat_del:
                write_plat_t((plat_t *)pointer->value);
                break;
            case tc_flash:
                write_lightflash_t((lightflash_t *)pointer->value);
                break;
            case tc_strobe:
                write_strobe_t((strobe_t *)pointer->value);
                break;
            case tc_glow:
                write_glow_t((glow_t *)pointer->value);
                break;
            case tc_elevator:
            case tc_elevator_del:
                write_elevator_t((elevator_t *)pointer->value);
                break;
            case tc_scroll:
                write_scroll_t((scroll_t *)pointer->value);
                break;
            case tc_pusher:
                write_pusher_t((pusher_t *)pointer->value);
                break;
            case tc_flicker:
                write_fireflicker_t((fireflicker_t *)pointer->value);
                break;
            case tc_friction:
                write_friction_t((friction_t *)pointer->value);
                break;
            case tc_ambient:
                write_ambient_t((ambient_t *)pointer->value);
                break;
            case tc_none:
                break;
        }
    }
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
                pointer.value = (uintptr_t)arena_calloc(thinkers_arena, mobj_t);
                break;
            case tc_ceiling:
            case tc_ceiling_del:
                pointer.value = (uintptr_t)arena_calloc(thinkers_arena, ceiling_t);
                break;
            case tc_door:
            case tc_door_del:
                pointer.value = (uintptr_t)arena_calloc(thinkers_arena, vldoor_t);
                break;
            case tc_floor:
            case tc_floor_del:
                pointer.value = (uintptr_t)arena_calloc(thinkers_arena, floormove_t);
                break;
            case tc_plat:
            case tc_plat_del:
                pointer.value = (uintptr_t)arena_calloc(thinkers_arena, floormove_t);
                break;
            case tc_flash:
                pointer.value = (uintptr_t)arena_calloc(thinkers_arena, lightflash_t);
                break;
            case tc_strobe:
                pointer.value = (uintptr_t)arena_calloc(thinkers_arena, strobe_t);
                break;
            case tc_glow:
                pointer.value = (uintptr_t)arena_calloc(thinkers_arena, glow_t);
                break;
            case tc_elevator:
            case tc_elevator_del:
                pointer.value = (uintptr_t)arena_calloc(thinkers_arena, elevator_t);
                break;
            case tc_scroll:
                pointer.value = (uintptr_t)arena_calloc(thinkers_arena, scroll_t);
                break;
            case tc_pusher:
                pointer.value = (uintptr_t)arena_calloc(thinkers_arena, pusher_t);
                break;
            case tc_flicker:
                pointer.value = (uintptr_t)arena_calloc(thinkers_arena, fireflicker_t);
                break;
            case tc_friction:
                pointer.value = (uintptr_t)arena_calloc(thinkers_arena, friction_t);
                break;
            case tc_ambient:
                pointer.value = (uintptr_t)arena_calloc(thinkers_arena, ambient_t);
                break;
            case tc_none:
                pointer.value = (uintptr_t)arena_calloc(thinkers_arena, thinker_t);
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
                read_mobj_t((mobj_t *)pointer->value, pointer->tc);
                break;
            case tc_ceiling:
            case tc_ceiling_del:
                read_ceiling_t((ceiling_t *)pointer->value, pointer->tc);
                break;
            case tc_door:
            case tc_door_del:
                read_vldoor_t((vldoor_t *)pointer->value, pointer->tc);
                break;
            case tc_floor:
            case tc_floor_del:
                read_floormove_t((floormove_t *)pointer->value, pointer->tc);
                break;
            case tc_plat:
            case tc_plat_del:
                read_plat_t((plat_t *)pointer->value, pointer->tc);
                break;
            case tc_flash:
                read_lightflash_t((lightflash_t *)pointer->value);
                break;
            case tc_strobe:
                read_strobe_t((strobe_t *)pointer->value);
                break;
            case tc_glow:
                read_glow_t((glow_t *)pointer->value);
                break;
            case tc_elevator:
            case tc_elevator_del:
                read_elevator_t((elevator_t *)pointer->value, pointer->tc);
                break;
            case tc_scroll:
                read_scroll_t((scroll_t *)pointer->value);
                break;
            case tc_pusher:
                read_pusher_t((pusher_t *)pointer->value);
                break;
            case tc_flicker:
                read_fireflicker_t((fireflicker_t *)pointer->value);
                break;
            case tc_friction:
                read_friction_t((friction_t *)pointer->value);
                break;
            case tc_ambient:
                read_ambient_t((ambient_t *)pointer->value);
                break;
            case tc_none:
                read_thinker_t((thinker_t *)pointer->value, pointer->tc);
                break;
        }
    }
}

//
// MSecNodes
//

inline static void UnArchiveTouchingThingList(sector_t *sector)
{
    sector->touching_thinglist = NULL;
    int count = read32();
    while (count--)
    {
        msecnode_t *node = arena_calloc(msecnodes_arena, msecnode_t);
        if (!sector->touching_thinglist)
        {
            sector->touching_thinglist = node;
        }
        array_push(msecnode_pointers, (uintptr_t)node);
    }
}

inline static void ArchiveTouchingThingList(const sector_t *sector)
{
    int oldsize = msecnode_hashmap->size;
    for (msecnode_t *node = sector->touching_thinglist; node; node = node->m_snext)
    {
        hashmap_put(msecnode_hashmap, (uintptr_t)node);
    }
    int count = msecnode_hashmap->size - oldsize;
    write32(count);
}

inline static void UnArchiveTouchingSectorList(mobj_t *mobj)
{
    mobj->touching_sectorlist = NULL;
    int count = read32();
    while (count--)
    {
        msecnode_t *node = arena_calloc(msecnodes_arena, msecnode_t);
        if (!mobj->touching_sectorlist)
        {
            mobj->touching_sectorlist = node;
        }
        array_push(msecnode_pointers, (uintptr_t)node);
    }
}

inline static void ArchiveTouchingSectorList(const mobj_t *mobj)
{
    int oldsize = msecnode_hashmap->size;
    for (msecnode_t *node = mobj->touching_sectorlist; node; node = node->m_tnext)
    {
        hashmap_put(msecnode_hashmap, (uintptr_t)node);
    }
    int count = msecnode_hashmap->size - oldsize;
    write32(count);
}

static void ArchiveFreeList(void)
{
    int oldcount = msecnode_hashmap->size;
    for (msecnode_t *node = headsecnode; node; node = node->m_snext)
    {
        hashmap_put(msecnode_hashmap, (uintptr_t)node);
    }
    int count = msecnode_hashmap->size - oldcount; 
    write32(count);
}

static void UnArchiveFreeList(void)
{
    int count = read32();
    while (count--)
    {
        msecnode_t *node = arena_calloc(msecnodes_arena, msecnode_t);
        array_push(msecnode_pointers, (uintptr_t)node);
    }
}

static void ArchiveMSecNodes(void)
{
    int count = msecnode_hashmap->size;
    uintptr_t *table = malloc(count * sizeof(*table));

    hashmap_iterator_t iter = hashmap_iterator(msecnode_hashmap);
    uintptr_t pointer;
    int number;
    while (hashmap_next(&iter, &pointer, &number))
    {
        table[number] = pointer;
    }
    for (int i = 0; i < count; ++i)
    {
        write_msecnode_t((msecnode_t *)table[i]);
    }

    free(table);
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
    for (int i = 0; i < MAXPLAYERS; i++)
    {
        if (playeringame[i])
        {
            write_player_t(&players[i]);
        }
    }
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
    for (int i = 0; i < blocklinks_count; ++i)
    {
        int count = 0;
        for (mobj_t *mobj = blocklinks[i]; mobj; mobj = mobj->bnext)
        {
            ++count;
        }
        write32(count);
        for (mobj_t *mobj = blocklinks[i]; mobj; mobj = mobj->bnext)
        {
            writep_mobj(mobj);
        }
    }
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

static void PrepareArchiveCeilingList(void)
{
    for (ceilinglist_t *cl = activeceilings; cl; cl = cl->next)
    {
        hashmap_put(ceilinglist_hashmap, (uintptr_t)cl);
    }
    int count = ceilinglist_hashmap->size;
    write32(count);
}

static void ArchiveCeilingList(void)
{
    int count = ceilinglist_hashmap->size;
    uintptr_t *table = malloc(count * sizeof(*table));

    hashmap_iterator_t iter = hashmap_iterator(ceilinglist_hashmap);
    uintptr_t pointer;
    int number;
    while (hashmap_next(&iter, &pointer, &number))
    {
        table[number] = pointer;
    }
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
    for (int i = 0; i < count; ++i)
    {
        ceilinglist_t *cl = arena_calloc(activeceilings_arena, ceilinglist_t);
        array_push(ceilinglist_pointers, (uintptr_t)cl);
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

static void PrepareArchivePlatList(void)
{
    for (platlist_t *pl = activeplats; pl; pl = pl->next)
    {
        hashmap_put(platlist_hashmap, (uintptr_t)pl);
    }

    int count = platlist_hashmap->size;
    write32(count);
}

static void ArchivePlatList(void)
{
    int count = platlist_hashmap->size;
    uintptr_t *table = malloc(count * sizeof(*table));

    hashmap_iterator_t iter = hashmap_iterator(platlist_hashmap);
    uintptr_t pointer;
    int number;
    while (hashmap_next(&iter, &pointer, &number))
    {
        table[number] = pointer;
    }
    for (int i = 0; i < count; ++i)
    {
        platlist_t *pl = (platlist_t *)table[i];
        writep_thinker(&pl->plat->thinker);
    }

    free(table);
}

static void PrepareUnArchivePlatList(void)
{
    int count = read32();
    for (int i = 0; i < count; ++i)
    {
        platlist_t *pl = arena_calloc(activeplats_arena, platlist_t);
        array_push(platlist_pointers, (uintptr_t)pl);
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

static void StartArchive(void)
{
    thinker_hashmap = hashmap_create(4 * 1024);
    msecnode_hashmap = hashmap_create(128);
    ceilinglist_hashmap = hashmap_create(128);
    platlist_hashmap = hashmap_create(128);
}

static void EndArchive(void)
{
    array_free(thinker_pointers);

    hashmap_free(thinker_hashmap);
    hashmap_free(msecnode_hashmap);
    hashmap_free(ceilinglist_hashmap);
    hashmap_free(platlist_hashmap);
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
    StartArchive();

    PrepareArchiveThinkers();
    write_thinker_t(&thinkercap);
    for (int i = 0; i < NUMTHCLASS; ++i)
    {
        write_thinker_t(&thinkerclasscap[i]);
    }

    ArchiveDirty();
    ArchiveWorld();

    // p_map.h
    ArchiveFreeList();
    writep_msecnode(headsecnode);

    write32(floatok,
            felldown,
            tmfloorz,
            tmceilingz,
            hangsolid);

    writep_index(ceilingline, lines);
    writep_index(floorline, lines);
    writep_mobj(linetarget);
    writep_msecnode(sector_list);
    writep_index(blockline, lines);

    for (int i = 0; i < arrlen(tmbbox); ++i)
    {
        write32(tmbbox[i]);
    }

    // p_maputil.h
    write32(opentop,
            openbottom,
            openrange,
            lowfloor);

    write_divline_t(&trace);

    // p_setup.h
    ArchiveBlocklinks();

    // p_spec.h
    PrepareArchiveCeilingList();
    PrepareArchivePlatList();

    ArchivePlayers();

    ArchiveThinkers();

    ArchiveMSecNodes();

    ArchiveCeilingList();
    writep_activeceilings(activeceilings);
    ArchivePlatList();
    writep_activeplats(activeplats);

    write_rng_t(&rng);
    ArchiveButtons();

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

    UnArchiveDirty();
    UnArchiveWorld();

    // p_map.h
    UnArchiveFreeList();
    headsecnode = readp_msecnode();

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

    UnArchiveCeilingList();
    activeceilings = readp_activeceilings();
    UnArchivePlatList();
    activeplats = readp_activeplats();

    read_rng_t(&rng);
    UnArchiveButtons();

    EndUnArchive();
}

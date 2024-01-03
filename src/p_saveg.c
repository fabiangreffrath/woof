//
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
// DESCRIPTION:
//      Archiving: SaveGame I/O.
//
//-----------------------------------------------------------------------------

#include "doomstat.h"
#include "r_main.h"
#include "p_maputl.h"
#include "p_spec.h"
#include "p_tick.h"
#include "p_saveg.h"
#include "m_random.h"
#include "am_map.h"
#include "p_enemy.h"
#include "w_wad.h" // [FG] W_LumpLength()

byte *save_p;

saveg_compat_t saveg_compat = saveg_woof510;

// Endian-safe integer read/write functions

byte saveg_read8(void)
{
    return *save_p++;
}

void saveg_write8(byte value)
{
    *save_p++ = value;
}

static short saveg_read16(void)
{
    int result;

    result = saveg_read8();
    result |= saveg_read8() << 8;

    return result;
}

static void saveg_write16(short value)
{
    saveg_write8(value & 0xff);
    saveg_write8((value >> 8) & 0xff);
}

int saveg_read32(void)
{
    int result;

    result = saveg_read8();
    result |= saveg_read8() << 8;
    result |= saveg_read8() << 16;
    result |= saveg_read8() << 24;

    return result;
}

void saveg_write32(int value)
{
    saveg_write8(value & 0xff);
    saveg_write8((value >> 8) & 0xff);
    saveg_write8((value >> 16) & 0xff);
    saveg_write8((value >> 24) & 0xff);
}

int64_t saveg_read64(void)
{
    int64_t result;

    result =  (int64_t)(saveg_read8());
    result |= (int64_t)(saveg_read8()) << 8;
    result |= (int64_t)(saveg_read8()) << 16;
    result |= (int64_t)(saveg_read8()) << 24;
    result |= (int64_t)(saveg_read8()) << 32;
    result |= (int64_t)(saveg_read8()) << 40;
    result |= (int64_t)(saveg_read8()) << 48;
    result |= (int64_t)(saveg_read8()) << 56;

    return result;
}

void saveg_write64(int64_t value)
{
    saveg_write8(value & 0xff);
    saveg_write8((value >> 8) & 0xff);
    saveg_write8((value >> 16) & 0xff);
    saveg_write8((value >> 24) & 0xff);
    saveg_write8((value >> 32) & 0xff);
    saveg_write8((value >> 40) & 0xff);
    saveg_write8((value >> 48) & 0xff);
    saveg_write8((value >> 56) & 0xff);
}

// Pad to 4-byte boundaries

static void saveg_read_pad(void)
{
    int padding;
    int i;

    padding = (4 - ((intptr_t)save_p & 3)) & 3;

    for (i=0; i<padding; ++i)
    {
        saveg_read8();
    }
}

static void saveg_write_pad(void)
{
    int padding;
    int i;

    padding = (4 - ((intptr_t)save_p & 3)) & 3;

    for (i=0; i<padding; ++i)
    {
        saveg_write8(0);
    }
}


// Pointers

static void *saveg_readp(void)
{
    return (void *) (intptr_t) saveg_read32();
}

static void saveg_writep(const void *p)
{
    saveg_write32((intptr_t) p);
}

// Enum values are 32-bit integers.

#define saveg_read_enum saveg_read32
#define saveg_write_enum saveg_write32

// [crispy] enumerate all thinker pointers
static int P_ThinkerToIndex(thinker_t* thinker)
{
    thinker_t* th;
    int i;

    if (!thinker)
        return 0;

    for (th = thinkercap.next, i = 0; th != &thinkercap; th = th->next)
    {
        if (th->function.p1 == (actionf_p1)P_MobjThinker)
        {
            i++;
            if (th == thinker)
                return i;
        }
    }

    return 0;
}

// [crispy] replace indizes with corresponding pointers
static thinker_t* P_IndexToThinker(int index)
{
    thinker_t* th;
    int i;

    if (!index)
        return NULL;

    for (th = thinkercap.next, i = 0; th != &thinkercap; th = th->next)
    {
        if (th->function.p1 == (actionf_p1)P_MobjThinker)
        {
            i++;
            if (i == index)
                return th;
        }
    }

    return NULL;
}

//
// Structure read/write functions
//

//
// mapthing_t
//

static void saveg_read_mapthing_t(mapthing_t *str)
{
    // short x;
    str->x = saveg_read16();

    // short y;
    str->y = saveg_read16();

    // short angle;
    str->angle = saveg_read16();

    // short type;
    str->type = saveg_read16();

    // short options;
    str->options = saveg_read16();
}

static void saveg_write_mapthing_t(mapthing_t *str)
{
    // short x;
    saveg_write16(str->x);

    // short y;
    saveg_write16(str->y);

    // short angle;
    saveg_write16(str->angle);

    // short type;
    saveg_write16(str->type);

    // short options;
    saveg_write16(str->options);
}

//
// think_t
//
// This is just an actionf_t.
//

#define saveg_read_think_t saveg_read_actionf_t
#define saveg_write_think_t saveg_write_actionf_t

//
// thinker_t
//

static void saveg_read_thinker_t(thinker_t *str)
{
    intptr_t func;

    // struct thinker_s* prev;
    str->prev = saveg_readp();

    // struct thinker_s* next;
    str->next = saveg_readp();

    // think_t function;
    func = saveg_readp();
    str->function.v = (actionf_v)func;

    // struct thinker_s* cnext;
    str->cnext = saveg_readp();

    // struct thinker_s* cprev;
    str->cprev = saveg_readp();

    // unsigned references;
    str->references = saveg_read32();
}

static void saveg_write_thinker_t(thinker_t *str)
{
    // struct thinker_s* prev;
    saveg_writep(str->prev);

    // struct thinker_s* next;
    saveg_writep(str->next);

    // think_t function;
    saveg_writep(&str->function);

    // struct thinker_s* cnext;
    saveg_writep(str->cnext);

    // struct thinker_s* cprev;
    saveg_writep(str->cprev);

    // thinker_t references
    saveg_write32(str->references);
}

//
// mobj_t
//

static void saveg_read_mobj_t(mobj_t *str)
{
    // thinker_t thinker;
    saveg_read_thinker_t(&str->thinker);

    // fixed_t x;
    str->x = saveg_read32();

    // fixed_t y;
    str->y = saveg_read32();

    // fixed_t z;
    str->z = saveg_read32();

    // struct mobj_s* snext;
    str->snext = saveg_readp();

    // struct mobj_s** sprev;
    str->sprev = saveg_readp();

    // angle_t angle;
    str->angle = saveg_read32();

    // spritenum_t sprite;
    str->sprite = saveg_read_enum();

    // int frame;
    str->frame = saveg_read32();

    // struct mobj_s* bnext;
    str->bnext = saveg_readp();

    // struct mobj_s** bprev;
    str->bprev = saveg_readp();

    // struct subsector_s* subsector;
    str->subsector = saveg_readp();

    // fixed_t floorz;
    str->floorz = saveg_read32();

    // fixed_t ceilingz;
    str->ceilingz = saveg_read32();

    // fixed_t dropoffz
    str->dropoffz = saveg_read32();

    // fixed_t radius;
    str->radius = saveg_read32();

    // fixed_t height;
    str->height = saveg_read32();

    // fixed_t momx;
    str->momx = saveg_read32();

    // fixed_t momy;
    str->momy = saveg_read32();

    // fixed_t momz;
    str->momz = saveg_read32();

    // int validcount;
    str->validcount = saveg_read32();

    // mobjtype_t type;
    str->type = saveg_read_enum();

    // mobjinfo_t* info;
    str->info = saveg_readp();

    // int tics;
    str->tics = saveg_read32();

    // state_t* state;
    str->state = saveg_readp();

    // int flags;
    str->flags = saveg_read32();

    if (saveg_compat > saveg_woof510)
    {
    // [Woof!]: mbf21: int flags2;
    str->flags2 = saveg_read32();
    }
    else
    {
      str->flags2 = mobjinfo[str->type].flags2;
    }

    // int intflags
    str->intflags = saveg_read32();

    // int health;
    str->health = saveg_read32();

    // short movedir;
    str->movedir = saveg_read16();

    // short movecount;
    str->movecount = saveg_read16();

    // short strafecount;
    str->strafecount = saveg_read16();

    // pad
    saveg_read16();

    // struct mobj_s* target;
    str->target = saveg_readp();

    // short reactiontime;
    str->reactiontime = saveg_read16();

    // short threshold;
    str->threshold = saveg_read16();

    // short pursuecount;
    str->pursuecount = saveg_read16();

    // short gear;
    str->gear = saveg_read16();

    // struct player_s* player;
    str->player = saveg_readp();

    // short lastlook;
    str->lastlook = saveg_read16();

    // mapthing_t spawnpoint;
    saveg_read_mapthing_t(&str->spawnpoint);

    // struct mobj_s* tracer;
    str->tracer = saveg_readp();

    // struct mobj_s* lastenemy;
    str->lastenemy = saveg_readp();

    // struct mobj_s* above_thing;
    str->above_thing = saveg_readp();

    // struct mobj_s* below_thing;
    str->below_thing = saveg_readp();

    if (saveg_compat > saveg_mbf)
    {
    // [Woof!]: int friction;
    str->friction = saveg_read32();

    // [Woof!]: int movefactor;
    str->movefactor = saveg_read32();
    }
    else
    {
        str->friction = 0;
        str->movefactor = 0;
    }

    // struct msecnode_s* touching_sectorlist;
    str->touching_sectorlist = saveg_readp();

    if (saveg_compat > saveg_mbf)
    {
    // [Woof!]: int interp;
    str->interp = saveg_read32();

    // [Woof!]: fixed_t oldx;
    str->oldx = saveg_read32();

    // [Woof!]: fixed_t oldy;
    str->oldy = saveg_read32();

    // [Woof!]: fixed_t oldz;
    str->oldz = saveg_read32();

    // [Woof!]: angle_t oldangle;
    str->oldangle = saveg_read32();
    }
    else
    {
        str->interp = 0;
        str->oldx = 0;
        str->oldy = 0;
        str->oldz = 0;
        str->oldangle = 0;
    }

    if (saveg_compat > saveg_woof510)
    {
    // [Woof!]: int bloodcolor;
    str->bloodcolor = saveg_read32();
    }
    else
    {
      str->bloodcolor = 0;
    }

    // [FG] height of the sprite in pixels
    str->actualheight = spritetopoffset[sprites[str->sprite].spriteframes[str->frame & FF_FRAMEMASK].lump[0]];
}

static void saveg_write_mobj_t(mobj_t *str)
{
    // thinker_t thinker;
    saveg_write_thinker_t(&str->thinker);

    // fixed_t x;
    saveg_write32(str->x);

    // fixed_t y;
    saveg_write32(str->y);

    // fixed_t z;
    saveg_write32(str->z);

    // struct mobj_s* snext;
    saveg_writep(str->snext);

    // struct mobj_s* sprev;
    saveg_writep(str->sprev);

    // angle_t angle;
    saveg_write32(str->angle);

    // spritenum_t sprite;
    saveg_write_enum(str->sprite);

    // int frame;
    saveg_write32(str->frame);

    // struct mobj_s* bnext;
    saveg_writep(str->bnext);

    // struct mobj_s* bprev;
    saveg_writep(str->bprev);

    // struct subsector_s* subsector;
    saveg_writep(str->subsector);

    // fixed_t floorz;
    saveg_write32(str->floorz);

    // fixed_t ceilingz;
    saveg_write32(str->ceilingz);

    // fixed_t dropoffz;
    saveg_write32(str->dropoffz);

    // fixed_t radius;
    saveg_write32(str->radius);

    // fixed_t height;
    saveg_write32(str->height);

    // fixed_t momx;
    saveg_write32(str->momx);

    // fixed_t momy;
    saveg_write32(str->momy);

    // fixed_t momz;
    saveg_write32(str->momz);

    // int validcount;
    saveg_write32(str->validcount);

    // mobjtype_t type;
    saveg_write_enum(str->type);

    // mobjinfo_t* info;
    saveg_writep(str->info);

    // int tics;
    saveg_write32(str->tics);

    // state_t* state;
    saveg_writep(str->state);

    // int flags;
    saveg_write32(str->flags);

    if (saveg_compat > saveg_woof510)
    {
    // [Woof!]: mbf21: int flags2;
    saveg_write32(str->flags2);
    }

    // int intflags;
    saveg_write32(str->intflags);

    // int health;
    saveg_write32(str->health);

    // short movedir;
    saveg_write16(str->movedir);

    // short movecount;
    saveg_write16(str->movecount);

    // short strafecount;
    saveg_write16(str->strafecount);

    // pad
    saveg_write16(0);

    // struct mobj_s* target;
    saveg_writep(str->target);

    // short reactiontime;
    saveg_write16(str->reactiontime);

    // short threshold;
    saveg_write16(str->threshold);

    // short pursuecount;
    saveg_write16(str->pursuecount);

    // short gear;
    saveg_write16(str->gear);

    // struct player_s* player;
    saveg_writep(str->player);

    // short lastlook;
    saveg_write16(str->lastlook);

    // mapthing_t spawnpoint;
    saveg_write_mapthing_t(&str->spawnpoint);

    // struct mobj_s* tracer;
    saveg_writep(str->tracer);

    // struct mobj_s* lastenemy;
    saveg_writep(str->lastenemy);

    // struct mobj_s* above_thing;
    saveg_writep(str->above_thing);

    // struct mobj_s* below_thing;
    saveg_writep(str->below_thing);

    // [Woof!]: int friction;
    saveg_write32(str->friction);

    // [Woof!]: int movefactor;
    saveg_write32(str->movefactor);

    // struct msecnode_s* touching_sectorlist;
    saveg_writep(str->touching_sectorlist);

    // [Woof!]: int interp;
    saveg_write32(str->interp);

    // [Woof!]: int oldx;
    saveg_write32(str->oldx);

    // [Woof!]: int oldy;
    saveg_write32(str->oldy);

    // [Woof!]: int oldz;
    saveg_write32(str->oldz);

    // [Woof!]: int oldangle;
    saveg_write32(str->oldangle);

    if (saveg_compat > saveg_woof510)
    {
    // [Woof!]: int bloodcolor;
    saveg_write32(str->bloodcolor);
    }
}

//
// ticcmd_t
//

static void saveg_read_ticcmd_t(ticcmd_t *str)
{
    // signed char forwardmove;
    str->forwardmove = saveg_read8();

    // signed char sidemove;
    str->sidemove = saveg_read8();

    // short angleturn;
    str->angleturn = saveg_read16();

    // short consistancy;
    str->consistancy = saveg_read16();

    // byte chatchar;
    str->chatchar = saveg_read8();

    // byte buttons;
    str->buttons = saveg_read8();
}

static void saveg_write_ticcmd_t(ticcmd_t *str)
{

    // signed char forwardmove;
    saveg_write8(str->forwardmove);

    // signed char sidemove;
    saveg_write8(str->sidemove);

    // short angleturn;
    saveg_write16(str->angleturn);

    // short consistancy;
    saveg_write16(str->consistancy);

    // byte chatchar;
    saveg_write8(str->chatchar);

    // byte buttons;
    saveg_write8(str->buttons);
}

//
// pspdef_t
//

static void saveg_read_pspdef_t(pspdef_t *str)
{
    int state;

    // state_t* state;
    state = saveg_read32();

    if (state > 0)
    {
        str->state = &states[state];
    }
    else
    {
        str->state = NULL;
    }

    // int tics;
    str->tics = saveg_read32();

    // fixed_t sx;
    str->sx = saveg_read32();

    // fixed_t sy;
    str->sy = saveg_read32();

    if (saveg_compat > saveg_mbf)
    {
    // [Woof!]: fixed_t sx2;
    str->sx2 = saveg_read32();

    // [Woof!]: fixed_t sy2;
    str->sy2 = saveg_read32();
    }
    else
    {
        str->sx2 = str->sx;
        str->sy2 = str->sy;
    }
}

static void saveg_write_pspdef_t(pspdef_t *str)
{
    // state_t* state;
    if (str->state)
    {
        saveg_write32(str->state - states);
    }
    else
    {
        saveg_write32(0);
    }

    // int tics;
    saveg_write32(str->tics);

    // fixed_t sx;
    saveg_write32(str->sx);

    // fixed_t sy;
    saveg_write32(str->sy);

    // [Woof!]: fixed_t sx2;
    saveg_write32(str->sx2);

    // [Woof!]: fixed_t sy2;
    saveg_write32(str->sy2);
}

//
// player_t
//

static void saveg_read_player_t(player_t *str)
{
    int i;

    // mobj_t* mo;
    str->mo = saveg_readp();

    // playerstate_t playerstate;
    str->playerstate = saveg_read_enum();

    // ticcmd_t cmd;
    saveg_read_ticcmd_t(&str->cmd);

    // fixed_t viewz;
    str->viewz = saveg_read32();

    // fixed_t viewheight;
    str->viewheight = saveg_read32();

    // fixed_t deltaviewheight;
    str->deltaviewheight = saveg_read32();

    // fixed_t bob;
    str->bob = saveg_read32();
    str->bob2 = str->bob;

    // fixed_t momx;
    str->momx = saveg_read32();

    // fixed_t momy;
    str->momy = saveg_read32();

    // int health;
    str->health = saveg_read32();

    // int armorpoints;
    str->armorpoints = saveg_read32();

    // int armortype;
    str->armortype = saveg_read32();

    // int powers[NUMPOWERS];
    for (i=0; i<NUMPOWERS; ++i)
    {
        str->powers[i] = saveg_read32();
    }

    // boolean cards[NUMCARDS];
    for (i=0; i<NUMCARDS; ++i)
    {
        str->cards[i] = saveg_read32();
    }

    // boolean backpack;
    str->backpack = saveg_read32();

    // int frags[MAXPLAYERS];
    for (i=0; i<MAXPLAYERS; ++i)
    {
        str->frags[i] = saveg_read32();
    }

    // weapontype_t readyweapon;
    str->readyweapon = saveg_read_enum();

    // weapontype_t pendingweapon;
    str->pendingweapon = saveg_read_enum();

    // boolean weaponowned[NUMWEAPONS];
    for (i=0; i<NUMWEAPONS; ++i)
    {
        str->weaponowned[i] = saveg_read32();
    }

    // int ammo[NUMAMMO];
    for (i=0; i<NUMAMMO; ++i)
    {
        str->ammo[i] = saveg_read32();
    }

    // int maxammo[NUMAMMO];
    for (i=0; i<NUMAMMO; ++i)
    {
        str->maxammo[i] = saveg_read32();
    }

    // int attackdown;
    str->attackdown = saveg_read32();

    // int usedown;
    str->usedown = saveg_read32();

    // int cheats;
    str->cheats = saveg_read32();

    // int refire;
    str->refire = saveg_read32();

    // int killcount;
    str->killcount = saveg_read32();

    // int itemcount;
    str->itemcount = saveg_read32();

    // int secretcount;
    str->secretcount = saveg_read32();

    // char* message;
    str->message = saveg_readp();

    // int damagecount;
    str->damagecount = saveg_read32();

    // int bonuscount;
    str->bonuscount = saveg_read32();

    // mobj_t* attacker;
    str->attacker = saveg_readp();

    // int extralight;
    str->extralight = saveg_read32();

    // int fixedcolormap;
    str->fixedcolormap = saveg_read32();

    // int colormap;
    str->colormap = saveg_read32();

    // pspdef_t psprites[NUMPSPRITES];
    for (i=0; i<NUMPSPRITES; ++i)
    {
        saveg_read_pspdef_t(&str->psprites[i]);
    }

    // [Woof!] char* secretmessage;
    str->secretmessage = NULL;

    // boolean didsecret;
    str->didsecret = saveg_read32();

    if (saveg_compat > saveg_mbf)
    {
    // [Woof!]: angle_t oldviewz;
    str->oldviewz = saveg_read32();
    }
    else
    {
      str->oldviewz = 0;
    }
}

static void saveg_write_player_t(player_t *str)
{
    int i;

    // mobj_t* mo;
    saveg_writep(str->mo);

    // playerstate_t playerstate;
    saveg_write_enum(str->playerstate);

    // ticcmd_t cmd;
    saveg_write_ticcmd_t(&str->cmd);

    // fixed_t viewz;
    saveg_write32(str->viewz);

    // fixed_t viewheight;
    saveg_write32(str->viewheight);

    // fixed_t deltaviewheight;
    saveg_write32(str->deltaviewheight);

    // fixed_t bob;
    saveg_write32(str->bob);

    // fixed_t momx;
    saveg_write32(str->momx);

    // fixed_t momy;
    saveg_write32(str->momy);

    // int health;
    saveg_write32(str->health);

    // int armorpoints;
    saveg_write32(str->armorpoints);

    // int armortype;
    saveg_write32(str->armortype);

    // int powers[NUMPOWERS];
    for (i=0; i<NUMPOWERS; ++i)
    {
        saveg_write32(str->powers[i]);
    }

    // boolean cards[NUMCARDS];
    for (i=0; i<NUMCARDS; ++i)
    {
        saveg_write32(str->cards[i]);
    }

    // boolean backpack;
    saveg_write32(str->backpack);

    // int frags[MAXPLAYERS];
    for (i=0; i<MAXPLAYERS; ++i)
    {
        saveg_write32(str->frags[i]);
    }

    // weapontype_t readyweapon;
    saveg_write_enum(str->readyweapon);

    // weapontype_t pendingweapon;
    saveg_write_enum(str->pendingweapon);

    // boolean weaponowned[NUMWEAPONS];
    for (i=0; i<NUMWEAPONS; ++i)
    {
        saveg_write32(str->weaponowned[i]);
    }

    // int ammo[NUMAMMO];
    for (i=0; i<NUMAMMO; ++i)
    {
        saveg_write32(str->ammo[i]);
    }

    // int maxammo[NUMAMMO];
    for (i=0; i<NUMAMMO; ++i)
    {
        saveg_write32(str->maxammo[i]);
    }

    // int attackdown;
    saveg_write32(str->attackdown);

    // int usedown;
    saveg_write32(str->usedown);

    // int cheats;
    saveg_write32(str->cheats);

    // int refire;
    saveg_write32(str->refire);

    // int killcount;
    saveg_write32(str->killcount);

    // int itemcount;
    saveg_write32(str->itemcount);

    // int secretcount;
    saveg_write32(str->secretcount);

    // char* message;
    saveg_writep(str->message);

    // int damagecount;
    saveg_write32(str->damagecount);

    // int bonuscount;
    saveg_write32(str->bonuscount);

    // mobj_t* attacker;
    saveg_writep(str->attacker);

    // int extralight;
    saveg_write32(str->extralight);

    // int fixedcolormap;
    saveg_write32(str->fixedcolormap);

    // int colormap;
    saveg_write32(str->colormap);

    // pspdef_t psprites[NUMPSPRITES];
    for (i=0; i<NUMPSPRITES; ++i)
    {
        saveg_write_pspdef_t(&str->psprites[i]);
    }

    // boolean didsecret;
    saveg_write32(str->didsecret);

    // [Woof!]: angle_t oldviewz;
    saveg_write32(str->oldviewz);
}


//
// ceiling_t
//

static void saveg_read_ceiling_t(ceiling_t *str)
{
    int sector;

    // thinker_t thinker;
    saveg_read_thinker_t(&str->thinker);

    // ceiling_e type;
    str->type = saveg_read_enum();

    // sector_t* sector;
    sector = saveg_read32();
    str->sector = &sectors[sector];

    // fixed_t bottomheight;
    str->bottomheight = saveg_read32();

    // fixed_t topheight;
    str->topheight = saveg_read32();

    // fixed_t speed;
    str->speed = saveg_read32();

    // fixed_t oldspeed;
    str->oldspeed = saveg_read32();

    // boolean crush;
    str->crush = saveg_read32();

    // int newspecial;
    str->newspecial = saveg_read32();

    // int oldspecial;
    str->oldspecial = saveg_read32();

    // short texture;
    str->texture = saveg_read16();

    // pad
    saveg_read16();

    // int direction;
    str->direction = saveg_read32();

    // int tag;
    str->tag = saveg_read32();

    // int olddirection;
    str->olddirection = saveg_read32();

    // struct ceilinglist *list;
    str->list = saveg_readp();
}

static void saveg_write_ceiling_t(ceiling_t *str)
{
    // thinker_t thinker;
    saveg_write_thinker_t(&str->thinker);

    // ceiling_e type;
    saveg_write_enum(str->type);

    // sector_t* sector;
    saveg_write32(str->sector - sectors);

    // fixed_t bottomheight;
    saveg_write32(str->bottomheight);

    // fixed_t topheight;
    saveg_write32(str->topheight);

    // fixed_t speed;
    saveg_write32(str->speed);

    // fixed_t oldspeed;
    saveg_write32(str->oldspeed);

    // boolean crush;
    saveg_write32(str->crush);

    // int newspecial;
    saveg_write32(str->newspecial);

    // int oldspecial;
    saveg_write32(str->oldspecial);

    // short texture;
    saveg_write16(str->texture);

    // pad
    saveg_write16(0);

    // int direction;
    saveg_write32(str->direction);

    // int tag;
    saveg_write32(str->tag);

    // int olddirection;
    saveg_write32(str->olddirection);

    // struct ceilinglist *list;
    saveg_writep(str->list);
}

//
// vldoor_t
//

static void saveg_read_vldoor_t(vldoor_t *str)
{
    int sector;

    // thinker_t thinker;
    saveg_read_thinker_t(&str->thinker);

    // vldoor_e type;
    str->type = saveg_read_enum();

    // sector_t* sector;
    sector = saveg_read32();
    str->sector = &sectors[sector];

    // fixed_t topheight;
    str->topheight = saveg_read32();

    // fixed_t speed;
    str->speed = saveg_read32();

    // int direction;
    str->direction = saveg_read32();

    // int topwait;
    str->topwait = saveg_read32();

    // int topcountdown;
    str->topcountdown = saveg_read32();

    // line_t *line;
    str->line = saveg_readp();

    // int lighttag;
    str->lighttag = saveg_read32();
}

static void saveg_write_vldoor_t(vldoor_t *str)
{
    // thinker_t thinker;
    saveg_write_thinker_t(&str->thinker);

    // vldoor_e type;
    saveg_write_enum(str->type);

    // sector_t* sector;
    saveg_write32(str->sector - sectors);

    // fixed_t topheight;
    saveg_write32(str->topheight);

    // fixed_t speed;
    saveg_write32(str->speed);

    // int direction;
    saveg_write32(str->direction);

    // int topwait;
    saveg_write32(str->topwait);

    // int topcountdown;
    saveg_write32(str->topcountdown);

    // line_t *line;
    //jff 1/31/98 archive line remembered by door as well
    if (str->line)
      saveg_write32(str->line - lines);
    else
      saveg_write32(-1);

    // int lighttag;
    saveg_write32(str->lighttag);
}

//
// floormove_t
//

static void saveg_read_floormove_t(floormove_t *str)
{
    int sector;

    // thinker_t thinker;
    saveg_read_thinker_t(&str->thinker);

    // floor_e type;
    str->type = saveg_read_enum();

    // boolean crush;
    str->crush = saveg_read32();

    // sector_t* sector;
    sector = saveg_read32();
    str->sector = &sectors[sector];

    // int direction;
    str->direction = saveg_read32();

    // int newspecial;
    str->newspecial = saveg_read32();

    // int oldspecial;
    str->oldspecial = saveg_read32();

    // short texture;
    str->texture = saveg_read16();

    // pad
    saveg_read16();

    // fixed_t floordestheight;
    str->floordestheight = saveg_read32();

    // fixed_t speed;
    str->speed = saveg_read32();
}

static void saveg_write_floormove_t(floormove_t *str)
{
    // thinker_t thinker;
    saveg_write_thinker_t(&str->thinker);

    // floor_e type;
    saveg_write_enum(str->type);

    // boolean crush;
    saveg_write32(str->crush);

    // sector_t* sector;
    saveg_write32(str->sector - sectors);

    // int direction;
    saveg_write32(str->direction);

    // int newspecial;
    saveg_write32(str->newspecial);

    // int oldspecial;
    saveg_write32(str->oldspecial);

    // short texture;
    saveg_write16(str->texture);

    // pad
    saveg_write16(0);

    // fixed_t floordestheight;
    saveg_write32(str->floordestheight);

    // fixed_t speed;
    saveg_write32(str->speed);
}

//
// plat_t
//

static void saveg_read_plat_t(plat_t *str)
{
    int sector;

    // thinker_t thinker;
    saveg_read_thinker_t(&str->thinker);

    // sector_t* sector;
    sector = saveg_read32();
    str->sector = &sectors[sector];

    // fixed_t speed;
    str->speed = saveg_read32();

    // fixed_t low;
    str->low = saveg_read32();

    // fixed_t high;
    str->high = saveg_read32();

    // int wait;
    str->wait = saveg_read32();

    // int count;
    str->count = saveg_read32();

    // plat_e status;
    str->status = saveg_read_enum();

    // plat_e oldstatus;
    str->oldstatus = saveg_read_enum();

    // boolean crush;
    str->crush = saveg_read32();

    // int tag;
    str->tag = saveg_read32();

    // plattype_e type;
    str->type = saveg_read_enum();

    // struct platlist *list;
    str->list = saveg_readp();
}

static void saveg_write_plat_t(plat_t *str)
{
    // thinker_t thinker;
    saveg_write_thinker_t(&str->thinker);

    // sector_t* sector;
    saveg_write32(str->sector - sectors);

    // fixed_t speed;
    saveg_write32(str->speed);

    // fixed_t low;
    saveg_write32(str->low);

    // fixed_t high;
    saveg_write32(str->high);

    // int wait;
    saveg_write32(str->wait);

    // int count;
    saveg_write32(str->count);

    // plat_e status;
    saveg_write_enum(str->status);

    // plat_e oldstatus;
    saveg_write_enum(str->oldstatus);

    // boolean crush;
    saveg_write32(str->crush);

    // int tag;
    saveg_write32(str->tag);

    // plattype_e type;
    saveg_write_enum(str->type);

    // struct platlist *list;
    saveg_writep(str->list);
}

//
// lightflash_t
//

static void saveg_read_lightflash_t(lightflash_t *str)
{
    int sector;

    // thinker_t thinker;
    saveg_read_thinker_t(&str->thinker);

    // sector_t* sector;
    sector = saveg_read32();
    str->sector = &sectors[sector];

    // int count;
    str->count = saveg_read32();

    // int maxlight;
    str->maxlight = saveg_read32();

    // int minlight;
    str->minlight = saveg_read32();

    // int maxtime;
    str->maxtime = saveg_read32();

    // int mintime;
    str->mintime = saveg_read32();
}

static void saveg_write_lightflash_t(lightflash_t *str)
{
    // thinker_t thinker;
    saveg_write_thinker_t(&str->thinker);

    // sector_t* sector;
    saveg_write32(str->sector - sectors);

    // int count;
    saveg_write32(str->count);

    // int maxlight;
    saveg_write32(str->maxlight);

    // int minlight;
    saveg_write32(str->minlight);

    // int maxtime;
    saveg_write32(str->maxtime);

    // int mintime;
    saveg_write32(str->mintime);
}

//
// strobe_t
//

static void saveg_read_strobe_t(strobe_t *str)
{
    int sector;

    // thinker_t thinker;
    saveg_read_thinker_t(&str->thinker);

    // sector_t* sector;
    sector = saveg_read32();
    str->sector = &sectors[sector];

    // int count;
    str->count = saveg_read32();

    // int minlight;
    str->minlight = saveg_read32();

    // int maxlight;
    str->maxlight = saveg_read32();

    // int darktime;
    str->darktime = saveg_read32();

    // int brighttime;
    str->brighttime = saveg_read32();
}

static void saveg_write_strobe_t(strobe_t *str)
{
    // thinker_t thinker;
    saveg_write_thinker_t(&str->thinker);

    // sector_t* sector;
    saveg_write32(str->sector - sectors);

    // int count;
    saveg_write32(str->count);

    // int minlight;
    saveg_write32(str->minlight);

    // int maxlight;
    saveg_write32(str->maxlight);

    // int darktime;
    saveg_write32(str->darktime);

    // int brighttime;
    saveg_write32(str->brighttime);
}

//
// glow_t
//

static void saveg_read_glow_t(glow_t *str)
{
    int sector;

    // thinker_t thinker;
    saveg_read_thinker_t(&str->thinker);

    // sector_t* sector;
    sector = saveg_read32();
    str->sector = &sectors[sector];

    // int minlight;
    str->minlight = saveg_read32();

    // int maxlight;
    str->maxlight = saveg_read32();

    // int direction;
    str->direction = saveg_read32();
}

static void saveg_write_glow_t(glow_t *str)
{
    // thinker_t thinker;
    saveg_write_thinker_t(&str->thinker);

    // sector_t* sector;
    saveg_write32(str->sector - sectors);

    // int minlight;
    saveg_write32(str->minlight);

    // int maxlight;
    saveg_write32(str->maxlight);

    // int direction;
    saveg_write32(str->direction);
}

//
// fireflicker_t
//

static void saveg_read_fireflicker_t(fireflicker_t *str)
{
    int sector;

    // thinker_t thinker;
    saveg_read_thinker_t(&str->thinker);

    // sector_t* sector;
    sector = saveg_read32();
    str->sector = &sectors[sector];

    // int count;
    str->count = saveg_read32();

    // int maxlight;
    str->maxlight = saveg_read32();

    // int minlight;
    str->minlight = saveg_read32();
}

static void saveg_write_fireflicker_t(fireflicker_t *str)
{
    // thinker_t thinker;
    saveg_write_thinker_t(&str->thinker);

    // sector_t* sector;
    saveg_write32(str->sector - sectors);

    // int count;
    saveg_write32(str->count);

    // int maxlight;
    saveg_write32(str->maxlight);

    // int minlight;
    saveg_write32(str->minlight);
}

//
// elevator_t
//

static void saveg_read_elevator_t(elevator_t *str)
{
    int sector;

    // thinker_t thinker;
    saveg_read_thinker_t(&str->thinker);

    // elevator_e type;
    str->type = saveg_read_enum();

    // sector_t* sector;
    sector = saveg_read32();
    str->sector = &sectors[sector];

    // int direction;
    str->direction = saveg_read32();

    // fixed_t floordestheight;
    str->floordestheight = saveg_read32();

    // fixed_t ceilingdestheight;
    str->ceilingdestheight = saveg_read32();

    // fixed_t speed;
    str->speed = saveg_read32();
}

static void saveg_write_elevator_t(elevator_t *str)
{
    // thinker_t thinker;
    saveg_write_thinker_t(&str->thinker);

    // elevator_e type;
    saveg_write_enum(str->type);

    // sector_t* sector;
    saveg_write32(str->sector - sectors);

    // int direction;
    saveg_write32(str->direction);

    // fixed_t floordestheight;
    saveg_write32(str->floordestheight);

    // fixed_t ceilingdestheight;
    saveg_write32(str->ceilingdestheight);

    // fixed_t speed;
    saveg_write32(str->speed);
}

//
// scroll_t
//

static void saveg_read_scroll_t(scroll_t *str)
{
    // thinker_t thinker;
    saveg_read_thinker_t(&str->thinker);

    // fixed_t dx;
    str->dx = saveg_read32();

    // fixed_t dy;
    str->dy = saveg_read32();

    // int affectee;
    str->affectee = saveg_read32();

    // int control;
    str->control = saveg_read32();

    // fixed_t last_height;
    str->last_height = saveg_read32();

    // fixed_t vdx;
    str->vdx = saveg_read32();

    // fixed_t vdy;
    str->vdy = saveg_read32();

    // int accel;
    str->accel = saveg_read32();

    // enum type;
    str->type = saveg_read_enum();
}

static void saveg_write_scroll_t(scroll_t *str)
{
    // thinker_t thinker;
    saveg_write_thinker_t(&str->thinker);

    // fixed_t dx;
    saveg_write32(str->dx);

    // fixed_t dy;
    saveg_write32(str->dy);

    // int affectee;
    saveg_write32(str->affectee);

    // int control;
    saveg_write32(str->control);

    // fixed_t last_height;
    saveg_write32(str->last_height);

    // fixed_t vdx;
    saveg_write32(str->vdx);

    // fixed_t vdy;
    saveg_write32(str->vdy);

    // int accel;
    saveg_write32(str->accel);

    // enum type;
    saveg_write_enum(str->type);
}

//
// pusher_t
//

static void saveg_read_pusher_t(pusher_t *str)
{
    // thinker_t thinker;
    saveg_read_thinker_t(&str->thinker);

    // enum type;
    str->type = saveg_read_enum();

    // mobj_t *source;
    str->source = (mobj_t *)P_IndexToThinker(saveg_read32());

    // int x_mag;
    str->x_mag = saveg_read32();

    // int y_mag;
    str->y_mag = saveg_read32();

    // int magnitude;
    str->magnitude = saveg_read32();

    // int radius;
    str->radius = saveg_read32();

    // int x;
    str->x = saveg_read32();

    // int y;
    str->y = saveg_read32();

    // int affectee;
    str->affectee = saveg_read32();
}

static void saveg_write_pusher_t(pusher_t *str)
{
    // thinker_t thinker;
    saveg_write_thinker_t(&str->thinker);

    // elevator_e type;
    saveg_write_enum(str->type);

    // mobj_t *source;
    saveg_write32(P_ThinkerToIndex((thinker_t *) str->source));

    // int x_mag;
    saveg_write32(str->x_mag);

    // int y_mag;
    saveg_write32(str->y_mag);

    // int magnitude;
    saveg_write32(str->magnitude);

    // int radius;
    saveg_write32(str->radius);

    // int x;
    saveg_write32(str->x);

    // int y;
    saveg_write32(str->y);

    // int affectee;
    saveg_write32(str->affectee);
}

//
// friction_t
//

static void saveg_read_friction_t(friction_t *str)
{
    // thinker_t thinker;
    saveg_read_thinker_t(&str->thinker);

    // fixed_t friction;
    str->friction = saveg_read32();

    // fixed_t movefactor;
    str->movefactor = saveg_read32();

    // int affectee;
    str->affectee = saveg_read32();
}

static void saveg_write_friction_t(friction_t *str)
{
    // thinker_t thinker;
    saveg_write_thinker_t(&str->thinker);

    // int friction;
    saveg_write32(str->friction);

    // int movefactor;
    saveg_write32(str->movefactor);

    // int affectee;
    saveg_write32(str->affectee);
}

//
// rng_t
//

static void saveg_read_rng_t(rng_t *str)
{
    int i;

    // unsigned long seed[NUMPRCLASS];
    for (i = 0; i < NUMPRCLASS; ++i)
    {
        str->seed[i] = saveg_read32();
    }

    // int rndindex;
    str->rndindex = saveg_read32();

    // int prndindex;
    str->prndindex = saveg_read32();
}

static void saveg_write_rng_t(rng_t *str)
{
    int i;

    // unsigned long seed[NUMPRCLASS];
    for (i = 0; i < NUMPRCLASS; ++i)
    {
        saveg_write32(str->seed[i]);
    }

    // int rndindex;
    saveg_write32(str->rndindex);

    // int prndindex;
    saveg_write32(str->prndindex);
}

//
// P_ArchivePlayers
//
void P_ArchivePlayers (void)
{
  int i;

  CheckSaveGame(sizeof(player_t) * MAXPLAYERS); // killough
  for (i=0 ; i<MAXPLAYERS ; i++)
    if (playeringame[i])
      {
        saveg_write_pad();
        saveg_write_player_t(&players[i]);
      }
}

//
// P_UnArchivePlayers
//
void P_UnArchivePlayers (void)
{
  int i;

  for (i=0 ; i<MAXPLAYERS ; i++)
    if (playeringame[i])
      {
        saveg_read_pad();

        saveg_read_player_t(&players[i]);

        // will be set when unarc thinker
        players[i].mo = NULL;
        players[i].message = NULL;
        players[i].attacker = NULL;
      }
}


//
// P_ArchiveWorld
//
void P_ArchiveWorld (void)
{
  int            i;
  const sector_t *sec;
  const line_t   *li;
  const side_t   *si;

  // killough 3/22/98: fix bug caused by hoisting save_p too early
  // killough 10/98: adjust size for changes below
  size_t size = 
    (sizeof(short)*5 + sizeof sec->floorheight + sizeof sec->ceilingheight) 
    * numsectors + sizeof(short)*3*numlines + 4;

  for (i=0; i<numlines; i++)
    {
      if (lines[i].sidenum[0] != NO_INDEX)
        size +=
	  sizeof(short)*3 + sizeof si->textureoffset + sizeof si->rowoffset;
      if (lines[i].sidenum[1] != NO_INDEX)
	size +=
	  sizeof(short)*3 + sizeof si->textureoffset + sizeof si->rowoffset;
    }

  CheckSaveGame(size); // killough

  saveg_write_pad();                // killough 3/22/98

  // do sectors
  for (i=0, sec = sectors ; i<numsectors ; i++,sec++)
    {
      // killough 10/98: save full floor & ceiling heights, including fraction
      saveg_write32(sec->floorheight);
      saveg_write32(sec->ceilingheight);
      saveg_write16(sec->floorpic);
      saveg_write16(sec->ceilingpic);
      saveg_write16(sec->lightlevel);
      saveg_write16(sec->special);            // needed?   yes -- transfer types
      saveg_write16(sec->tag);                // needed?   need them -- killough
    }

  // do lines
  for (i=0, li = lines ; i<numlines ; i++,li++)
    {
      int j;

      saveg_write16(li->flags);
      saveg_write16(li->special);
      saveg_write16(li->tag);

      for (j=0; j<2; j++)
        if (li->sidenum[j] != NO_INDEX)
          {
	    si = &sides[li->sidenum[j]];

	    // killough 10/98: save full sidedef offsets,
	    // preserving fractional scroll offsets

	    saveg_write32(si->textureoffset);
	    saveg_write32(si->rowoffset);

            saveg_write16(si->toptexture);
            saveg_write16(si->bottomtexture);
            saveg_write16(si->midtexture);
          }
    }
}



//
// P_UnArchiveWorld
//
void P_UnArchiveWorld (void)
{
  int          i;
  sector_t     *sec;
  line_t       *li;

  saveg_read_pad();                // killough 3/22/98

  // do sectors
  for (i=0, sec = sectors ; i<numsectors ; i++,sec++)
    {
      // [crispy] add overflow guard for the flattranslation[] array
      short floorpic, ceilingpic;
      extern int numflats;

      // killough 10/98: load full floor & ceiling heights, including fractions

      sec->floorheight = saveg_read32();
      sec->ceilingheight = saveg_read32();

      floorpic = saveg_read16();
      ceilingpic = saveg_read16();
      sec->lightlevel = saveg_read16();
      sec->special = saveg_read16();
      sec->tag = saveg_read16();
      sec->ceilingdata = 0; //jff 2/22/98 now three thinker fields, not two
      sec->floordata = 0;
      sec->lightingdata = 0;
      sec->soundtarget = 0;

      // [crispy] add overflow guard for the flattranslation[] array
      if (floorpic >= 0 && floorpic < numflats &&
          W_LumpLength(firstflat + floorpic) >= 64*64)
      {
          sec->floorpic = floorpic;
      }
      if (ceilingpic >= 0 && ceilingpic < numflats &&
          W_LumpLength(firstflat + ceilingpic) >= 64*64)
      {
          sec->ceilingpic = ceilingpic;
      }
    }

  // do lines
  for (i=0, li = lines ; i<numlines ; i++,li++)
    {
      int j;

      li->flags = saveg_read16();
      li->special = saveg_read16();
      li->tag = saveg_read16();
      for (j=0 ; j<2 ; j++)
        if (li->sidenum[j] != NO_INDEX)
          {
            side_t *si = &sides[li->sidenum[j]];

	    // killough 10/98: load full sidedef offsets, including fractions

	    si->textureoffset = saveg_read32();
	    si->rowoffset = saveg_read32();
	    // [crispy] smooth texture scrolling
	    si->basetextureoffset = si->textureoffset;
	    si->baserowoffset = si->rowoffset;

            si->toptexture = saveg_read16();
            si->bottomtexture = saveg_read16();
            si->midtexture = saveg_read16();
          }
    }
}

//
// Thinkers
//

typedef enum {
  tc_end,
  tc_mobj
} thinkerclass_t;

//
// P_ArchiveThinkers
//
// 2/14/98 killough: substantially modified to fix savegame bugs

void P_ArchiveThinkers (void)
{
  thinker_t *th;
  size_t    size = 0;
  mobj_t    tmp;

  CheckSaveGame(sizeof brain);      // killough 3/26/98: Save boss brain state
  saveg_write32(brain.easy);
  saveg_write32(brain.targeton);

  // killough 2/14/98:
  // count the number of thinkers, and mark each one with its index, using
  // the prev field as a placeholder, since it can be restored later.

  for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
    if (th->function.p1 == (actionf_p1)P_MobjThinker)
      th->prev = (thinker_t *) ++size;

  // check that enough room is available in savegame buffer
  CheckSaveGame(size*(sizeof(mobj_t)+4));       // killough 2/14/98

  // save off the current thinkers

  for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
    if (th->function.p1 == (actionf_p1)P_MobjThinker)
      {
        mobj_t *mobj = &tmp;
        memcpy (mobj, th, sizeof(*mobj));
        mobj->state = (state_t *)(mobj->state - states);

        // killough 2/14/98: convert pointers into indices.
        // Fixes many savegame problems, by properly saving
        // target and tracer fields. Note: we store NULL if
        // the thinker pointed to by these fields is not a
        // mobj thinker.

        if (mobj->target)
          mobj->target = mobj->target->thinker.function.p1 ==
            (actionf_p1)P_MobjThinker ?
            (mobj_t *) mobj->target->thinker.prev : NULL;

        if (mobj->tracer)
          mobj->tracer = mobj->tracer->thinker.function.p1 ==
            (actionf_p1)P_MobjThinker ?
            (mobj_t *) mobj->tracer->thinker.prev : NULL;

        // killough 2/14/98: new field: save last known enemy. Prevents
        // monsters from going to sleep after killing monsters and not
        // seeing player anymore.

        if (mobj->lastenemy)
          mobj->lastenemy = mobj->lastenemy->thinker.function.p1 ==
            (actionf_p1)P_MobjThinker ?
            (mobj_t *) mobj->lastenemy->thinker.prev : NULL;

        // killough 2/14/98: end changes

        if (mobj->above_thing)                                      // phares
          mobj->above_thing = mobj->above_thing->thinker.function.p1 ==
            (actionf_p1)P_MobjThinker ?
            (mobj_t *) mobj->above_thing->thinker.prev : NULL;

        if (mobj->below_thing)
          mobj->below_thing = mobj->below_thing->thinker.function.p1 ==
            (actionf_p1)P_MobjThinker ?
            (mobj_t *) mobj->below_thing->thinker.prev : NULL;      // phares

        if (mobj->player)
          mobj->player = (player_t *)((mobj->player-players) + 1);

        saveg_write8(tc_mobj);
        saveg_write_pad();
        saveg_write_mobj_t(mobj);
      }

  // add a terminating marker
  saveg_write8(tc_end);

  // killough 9/14/98: save soundtargets
  {
     int i;
     CheckSaveGame(numsectors * sizeof(mobj_t *));       // killough 9/14/98
     for (i = 0; i < numsectors; i++)
     {
        mobj_t *target = sectors[i].soundtarget;
        if (target)
        {
            // haleyjd 03/23/09: Imported from Eternity:
            // haleyjd 11/03/06: We must check for P_MobjThinker here as well,
            // or player corpses waiting for deferred removal will be saved as
            // raw pointer values instead of twizzled numbers, causing a crash
            // on savegame load!
            target = target->thinker.function.p1 == (actionf_p1)P_MobjThinker ?
                        (mobj_t *)target->thinker.prev : NULL;

        }
        saveg_writep(target);
     }
  }
  
  // killough 2/14/98: restore prev pointers
  {
    thinker_t *prev = &thinkercap;
    for (th = thinkercap.next ; th != &thinkercap ; prev=th, th=th->next)
      th->prev = prev;
  }
  // killough 2/14/98: end changes
}

//
// killough 11/98
//
// Same as P_SetTarget() in p_tick.c, except that the target is nullified
// first, so that no old target's reference count is decreased (when loading
// savegames, old targets are indices, not really pointers to targets).
//

static void P_SetNewTarget(mobj_t **mop, mobj_t *targ)
{
  *mop = NULL;
  P_SetTarget(mop, targ);
}

//
// P_UnArchiveThinkers
//
// 2/14/98 killough: substantially modified to fix savegame bugs
//

void P_UnArchiveThinkers (void)
{
  thinker_t *th;
  mobj_t    **mobj_p;    // killough 2/14/98: Translation table
  size_t    size;        // killough 2/14/98: size of or index into table
  size_t    idx;         // haleyjd 11/03/06: separate index var

  // killough 3/26/98: Load boss brain state
  brain.easy = saveg_read32();
  brain.targeton = saveg_read32();

  // remove all the current thinkers
  for (th = thinkercap.next; th != &thinkercap; )
    {
      thinker_t *next = th->next;
      if (th->function.p1 == (actionf_p1)P_MobjThinker)
        P_RemoveMobj ((mobj_t *) th);
      else
        Z_Free (th);
      th = next;
    }
  P_InitThinkers ();

  // killough 2/14/98: count number of thinkers by skipping through them
  {
    byte *sp = save_p;     // save pointer and skip header
    mobj_t tmp;
    for (size = 1; *save_p++ == tc_mobj; size++)  // killough 2/14/98
      {                     // skip all entries, adding up count
        saveg_read_pad();
        saveg_read_mobj_t(&tmp);
      }

    if (*--save_p != tc_end)
      I_Error ("Unknown tclass %i in savegame", *save_p);

    // first table entry special: 0 maps to NULL
    *(mobj_p = Z_Malloc(size * sizeof *mobj_p, PU_STATIC, 0)) = 0;   // table of pointers
    save_p = sp;           // restore save pointer
  }

  // read in saved thinkers
  // haleyjd 11/03/06: use idx to save "size" for rangechecking
  for (idx = 1; *save_p++ == tc_mobj; idx++)    // killough 2/14/98
    {
      mobj_t *mobj = Z_Malloc(sizeof(mobj_t), PU_LEVEL, NULL);

      // killough 2/14/98 -- insert pointers to thinkers into table, in order:
      mobj_p[idx] = mobj;

      saveg_read_pad();
      saveg_read_mobj_t(mobj);
      mobj->state = states + (size_t) mobj->state;

      if (mobj->player)
        (mobj->player = &players[(size_t) mobj->player - 1]) -> mo = mobj;

      P_SetThingPosition (mobj);
      mobj->info = &mobjinfo[mobj->type];

      // killough 2/28/98:
      // Fix for falling down into a wall after savegame loaded:
      //      mobj->floorz = mobj->subsector->sector->floorheight;
      //      mobj->ceilingz = mobj->subsector->sector->ceilingheight;

      mobj->thinker.function.p1 = (actionf_p1)P_MobjThinker;
      P_AddThinker (&mobj->thinker);
    }

  // killough 2/14/98: adjust target and tracer fields, plus
  // lastenemy field, to correctly point to mobj thinkers.
  // NULL entries automatically handled by first table entry.
  //
  // killough 11/98: use P_SetNewTarget() to set fields

  for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
    {
      P_SetNewTarget(&((mobj_t *) th)->target,
        mobj_p[(size_t)((mobj_t *)th)->target]);

      P_SetNewTarget(&((mobj_t *) th)->tracer,
        mobj_p[(size_t)((mobj_t *)th)->tracer]);

      P_SetNewTarget(&((mobj_t *) th)->lastenemy,
        mobj_p[(size_t)((mobj_t *)th)->lastenemy]);

      // phares: added two new fields for Sprite Height problem

      P_SetNewTarget(&((mobj_t *) th)->above_thing,
        mobj_p[(size_t)((mobj_t *)th)->above_thing]);

      P_SetNewTarget(&((mobj_t *) th)->below_thing,
        mobj_p[(size_t)((mobj_t *)th)->below_thing]);
    }

  {  // killough 9/14/98: restore soundtargets
    int i;
    for (i = 0; i < numsectors; i++)
    {
       mobj_t *target = saveg_readp();

       // haleyjd 11/03/06: rangecheck for security
       if((size_t)target < size)
          P_SetNewTarget(&sectors[i].soundtarget, mobj_p[(size_t) target]);
       else
          sectors[i].soundtarget = NULL;
    }
  }

  Z_Free(mobj_p);    // free translation table

  // killough 3/26/98: Spawn icon landings:
  if (gamemode == commercial)
    P_SpawnBrainTargets();
}

//
// P_ArchiveSpecials
//
enum {
  tc_ceiling,
  tc_door,
  tc_floor,
  tc_plat,
  tc_flash,
  tc_strobe,
  tc_glow,
  tc_elevator,    //jff 2/22/98 new elevator type thinker
  tc_scroll,      // killough 3/7/98: new scroll effect thinker
  tc_pusher,      // phares 3/22/98:  new push/pull effect thinker
  tc_flicker,     // killough 10/4/98
  tc_endspecials,
  tc_friction     // store friction for complevel Boom
} specials_e;

//
// Things to handle:
//
// T_MoveCeiling, (ceiling_t: sector_t * swizzle), - active list
// T_VerticalDoor, (vldoor_t: sector_t * swizzle),
// T_MoveFloor, (floormove_t: sector_t * swizzle),
// T_LightFlash, (lightflash_t: sector_t * swizzle),
// T_StrobeFlash, (strobe_t: sector_t *),
// T_Glow, (glow_t: sector_t *),
// T_PlatRaise, (plat_t: sector_t *), - active list
// T_MoveElevator, (plat_t: sector_t *), - active list      // jff 2/22/98
// T_Scroll                                                 // killough 3/7/98
// T_Pusher                                                 // phares 3/22/98
// T_FireFlicker                                            // killough 10/4/98
//

void P_ArchiveSpecials (void)
{
  thinker_t *th;
  size_t    size = 0;          // killough

  // save off the current thinkers (memory size calculation -- killough)

  for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
    if (!th->function.v)
      {
        platlist_t *pl;
        ceilinglist_t *cl;     //jff 2/22/98 need this for ceilings too now
        for (pl=activeplats; pl; pl=pl->next)
          if (pl->plat == (plat_t *) th)   // killough 2/14/98
            {
              size += 4+sizeof(plat_t);
              goto end;
            }
        for (cl=activeceilings; cl; cl=cl->next) // search for activeceiling
          if (cl->ceiling == (ceiling_t *) th)   //jff 2/22/98
            {
              size += 4+sizeof(ceiling_t);
              goto end;
            }
      end:;
      }
    else
      size +=
        th->function.p1==(actionf_p1)T_MoveCeiling  ? 4+sizeof(ceiling_t) :
        th->function.p1==(actionf_p1)T_VerticalDoor ? 4+sizeof(vldoor_t)  :
        th->function.p1==(actionf_p1)T_MoveFloor    ? 4+sizeof(floormove_t):
        th->function.p1==(actionf_p1)T_PlatRaise    ? 4+sizeof(plat_t)    :
        th->function.p1==(actionf_p1)T_LightFlash   ? 4+sizeof(lightflash_t):
        th->function.p1==(actionf_p1)T_StrobeFlash  ? 4+sizeof(strobe_t)  :
        th->function.p1==(actionf_p1)T_Glow         ? 4+sizeof(glow_t)    :
        th->function.p1==(actionf_p1)T_MoveElevator ? 4+sizeof(elevator_t):
        th->function.p1==(actionf_p1)T_Scroll       ? 4+sizeof(scroll_t)  :
        th->function.p1==(actionf_p1)T_Pusher       ? 4+sizeof(pusher_t)  :
        th->function.p1==(actionf_p1)T_FireFlicker  ? 4+sizeof(fireflicker_t) :
        th->function.p1==(actionf_p1)T_Friction     ? 4+sizeof(friction_t) :
      0;

  CheckSaveGame(size);          // killough

  // save off the current thinkers
  for (th=thinkercap.next; th!=&thinkercap; th=th->next)
    {
      if (!th->function.v)
        {
          platlist_t *pl;
          ceilinglist_t *cl;    //jff 2/22/98 add iter variable for ceilings

          // killough 2/8/98: fix plat original height bug.
          // Since acv==NULL, this could be a plat in stasis.
          // so check the active plats list, and save this
          // plat (jff: or ceiling) even if it is in stasis.

          for (pl=activeplats; pl; pl=pl->next)
            if (pl->plat == (plat_t *) th)      // killough 2/14/98
              goto plat;

          for (cl=activeceilings; cl; cl=cl->next)
            if (cl->ceiling == (ceiling_t *) th)      //jff 2/22/98
              goto ceiling;

          continue;
        }

      if (th->function.p1 == (actionf_p1)T_MoveCeiling)
        {
        ceiling:                               // killough 2/14/98
          saveg_write8(tc_ceiling);
          saveg_write_pad();
          saveg_write_ceiling_t((ceiling_t *) th);
          continue;
        }

      if (th->function.p1 == (actionf_p1)T_VerticalDoor)
        {
          saveg_write8(tc_door);
          saveg_write_pad();
          saveg_write_vldoor_t((vldoor_t *) th);
          continue;
        }

      if (th->function.p1 == (actionf_p1)T_MoveFloor)
        {
          saveg_write8(tc_floor);
          saveg_write_pad();
          saveg_write_floormove_t((floormove_t *) th);
          continue;
        }

      if (th->function.p1 == (actionf_p1)T_PlatRaise)
        {
        plat:   // killough 2/14/98: added fix for original plat height above
          saveg_write8(tc_plat);
          saveg_write_pad();
          saveg_write_plat_t((plat_t *) th);
          continue;
        }

      if (th->function.p1 == (actionf_p1)T_LightFlash)
        {
          saveg_write8(tc_flash);
          saveg_write_pad();
          saveg_write_lightflash_t((lightflash_t *) th);
          continue;
        }

      if (th->function.p1 == (actionf_p1)T_StrobeFlash)
        {
          saveg_write8(tc_strobe);
          saveg_write_pad();
          saveg_write_strobe_t((strobe_t *) th);
          continue;
        }

      if (th->function.p1 == (actionf_p1)T_Glow)
        {
          saveg_write8(tc_glow);
          saveg_write_pad();
          saveg_write_glow_t((glow_t *) th);
          continue;
        }

      // killough 10/4/98: save flickers
      if (th->function.p1 == (actionf_p1)T_FireFlicker)
        {
          saveg_write8(tc_flicker);
          saveg_write_pad();
          saveg_write_fireflicker_t((fireflicker_t *) th);
          continue;
        }

      //jff 2/22/98 new case for elevators
      if (th->function.p1 == (actionf_p1)T_MoveElevator)
        {
          saveg_write8(tc_elevator);
          saveg_write_pad();
          saveg_write_elevator_t((elevator_t *) th);
          continue;
        }

      // killough 3/7/98: Scroll effect thinkers
      if (th->function.p1 == (actionf_p1)T_Scroll)
        {
          saveg_write8(tc_scroll);
          saveg_write_scroll_t((scroll_t *) th);
          continue;
        }

      // phares 3/22/98: Push/Pull effect thinkers

      if (th->function.p1 == (actionf_p1)T_Pusher)
        {
          saveg_write8(tc_pusher);
          saveg_write_pusher_t((pusher_t *) th);
          continue;
        }

      // store friction for complevel Boom
      if (th->function.p1 == (actionf_p1)T_Friction)
        {
          saveg_write8(tc_friction);
          saveg_write_pad();
          saveg_write_friction_t((friction_t *) th);
          continue;
        }
    }

  // add a terminating marker
  saveg_write8(tc_endspecials);
}


//
// P_UnArchiveSpecials
//
void P_UnArchiveSpecials (void)
{
  byte tclass;

  // read in saved thinkers
  while ((tclass = saveg_read8()) != tc_endspecials)  // killough 2/14/98
    switch (tclass)
      {
      case tc_ceiling:
        saveg_read_pad();
        {
          ceiling_t *ceiling = Z_Malloc (sizeof(*ceiling), PU_LEVEL, NULL);
          saveg_read_ceiling_t(ceiling);
          ceiling->sector->ceilingdata = ceiling; //jff 2/22/98

          if (ceiling->thinker.function.p1)
            ceiling->thinker.function.p1 = (actionf_p1)T_MoveCeiling;

          P_AddThinker (&ceiling->thinker);
          P_AddActiveCeiling(ceiling);
          break;
        }

      case tc_door:
        saveg_read_pad();
        {
          vldoor_t *door = Z_Malloc (sizeof(*door), PU_LEVEL, NULL);
          saveg_read_vldoor_t(door);

          //jff 1/31/98 unarchive line remembered by door as well
          door->line = (size_t)door->line!=-1? &lines[(size_t)door->line] : NULL;

          door->sector->ceilingdata = door;       //jff 2/22/98
          door->thinker.function.p1 = (actionf_p1)T_VerticalDoor;
          P_AddThinker (&door->thinker);
          break;
        }

      case tc_floor:
        saveg_read_pad();
        {
          floormove_t *floor = Z_Malloc (sizeof(*floor), PU_LEVEL, NULL);
          saveg_read_floormove_t(floor);
          floor->sector->floordata = floor; //jff 2/22/98
          floor->thinker.function.p1 = (actionf_p1)T_MoveFloor;
          P_AddThinker (&floor->thinker);
          break;
        }

      case tc_plat:
        saveg_read_pad();
        {
          plat_t *plat = Z_Malloc (sizeof(*plat), PU_LEVEL, NULL);
          saveg_read_plat_t(plat);
          plat->sector->floordata = plat; //jff 2/22/98

          if (plat->thinker.function.p1)
            plat->thinker.function.p1 = (actionf_p1)T_PlatRaise;

          P_AddThinker (&plat->thinker);
          P_AddActivePlat(plat);
          break;
        }

      case tc_flash:
        saveg_read_pad();
        {
          lightflash_t *flash = Z_Malloc (sizeof(*flash), PU_LEVEL, NULL);
          saveg_read_lightflash_t(flash);
          flash->thinker.function.p1 = (actionf_p1)T_LightFlash;
          P_AddThinker (&flash->thinker);
          break;
        }

      case tc_strobe:
        saveg_read_pad();
        {
          strobe_t *strobe = Z_Malloc (sizeof(*strobe), PU_LEVEL, NULL);
          saveg_read_strobe_t(strobe);
          strobe->thinker.function.p1 = (actionf_p1)T_StrobeFlash;
          P_AddThinker (&strobe->thinker);
          break;
        }

      case tc_glow:
        saveg_read_pad();
        {
          glow_t *glow = Z_Malloc (sizeof(*glow), PU_LEVEL, NULL);
          saveg_read_glow_t(glow);
          glow->thinker.function.p1 = (actionf_p1)T_Glow;
          P_AddThinker (&glow->thinker);
          break;
        }

      case tc_flicker:           // killough 10/4/98
        saveg_read_pad();
        {
          fireflicker_t *flicker = Z_Malloc (sizeof(*flicker), PU_LEVEL, NULL);
          saveg_read_fireflicker_t(flicker);
          flicker->thinker.function.p1 = (actionf_p1)T_FireFlicker;
          P_AddThinker (&flicker->thinker);
          break;
        }

        //jff 2/22/98 new case for elevators
      case tc_elevator:
        saveg_read_pad();
        {
          elevator_t *elevator = Z_Malloc (sizeof(*elevator), PU_LEVEL, NULL);
          saveg_read_elevator_t(elevator);
          elevator->sector->floordata = elevator; //jff 2/22/98
          elevator->sector->ceilingdata = elevator; //jff 2/22/98
          elevator->thinker.function.p1 = (actionf_p1)T_MoveElevator;
          P_AddThinker (&elevator->thinker);
          break;
        }

      case tc_scroll:       // killough 3/7/98: scroll effect thinkers
        {
          scroll_t *scroll = Z_Malloc (sizeof(scroll_t), PU_LEVEL, NULL);
          saveg_read_scroll_t(scroll);
          scroll->thinker.function.p1 = (actionf_p1)T_Scroll;
          P_AddThinker(&scroll->thinker);
          break;
        }

      case tc_pusher:   // phares 3/22/98: new Push/Pull effect thinkers
        {
          pusher_t *pusher = Z_Malloc (sizeof(pusher_t), PU_LEVEL, NULL);
          saveg_read_pusher_t(pusher);
          pusher->thinker.function.p1 = (actionf_p1)T_Pusher;
          // can't convert from index to pointer, old save version
          if (pusher->source == NULL)
          {
          pusher->source = P_GetPushThing(pusher->affectee);
            if (pusher->type == p_push && pusher->source == NULL)
              I_Error("P_UnArchiveSpecials: Pusher thinker without source in sector %d",
                      pusher->affectee);
          }
          P_AddThinker(&pusher->thinker);
          break;
        }

      // load friction for complevel Boom
      case tc_friction:
        saveg_read_pad();
        {
          friction_t *friction = Z_Malloc (sizeof(friction_t), PU_LEVEL, NULL);
          saveg_read_friction_t(friction);
          friction->thinker.function.p1 = (actionf_p1)T_Friction;
          P_AddThinker(&friction->thinker);
          break;
        }

      default:
        I_Error ("P_UnarchiveSpecials:Unknown tclass %i "
                 "in savegame",tclass);
      }
}

// killough 2/16/98: save/restore random number generator state information

void P_ArchiveRNG(void)
{
  CheckSaveGame(sizeof rng);
  saveg_write_rng_t(&rng);
}

void P_UnArchiveRNG(void)
{
  saveg_read_rng_t(&rng);
}

// killough 2/22/98: Save/restore automap state
void P_ArchiveMap(void)
{
  CheckSaveGame(sizeof followplayer + sizeof markpointnum +
                markpointnum * sizeof *markpoints +
                sizeof automapactive + sizeof viewactive);

  saveg_write32(automapactive);
  saveg_write32(viewactive);
  saveg_write32(followplayer);
  saveg_write32(automap_grid);
  saveg_write32(markpointnum);

  if (markpointnum)
    {
      int i;

      for (i = 0; i < markpointnum; ++i)
      {
        // [Woof!]: int64_t x,y;
        saveg_write64(markpoints[i].x);
        saveg_write64(markpoints[i].y);
      }
    }
}

void P_UnArchiveMap(void)
{
  automapactive = saveg_read32();
  viewactive = saveg_read32();
  followplayer = saveg_read32();
  automap_grid = saveg_read32();

  if (automapactive)
    AM_Start();

  markpointnum = saveg_read32();

  if (markpointnum)
    {
      int i;
      while (markpointnum >= markpointnum_max)
        markpoints = Z_Realloc(markpoints, sizeof *markpoints *
         (markpointnum_max = markpointnum_max ? markpointnum_max*2 : 16),
         PU_STATIC, 0);

      for (i = 0; i < markpointnum; ++i)
      {
        if (saveg_compat > saveg_mbf)
        {
          // [Woof!]: int64_t x,y;
          markpoints[i].x = saveg_read64();
          markpoints[i].y = saveg_read64();
        }
        else
        {
          // fixed_t x,y;
          markpoints[i].x = saveg_read32();
          markpoints[i].y = saveg_read32();
        }
      }
    }
}

//----------------------------------------------------------------------------
//
// $Log: p_saveg.c,v $
// Revision 1.17  1998/05/03  23:10:22  killough
// beautification
//
// Revision 1.16  1998/04/19  01:16:06  killough
// Fix boss brain spawn crashes after loadgames
//
// Revision 1.15  1998/03/28  18:02:17  killough
// Fix boss spawner savegame crash bug
//
// Revision 1.14  1998/03/23  15:24:36  phares
// Changed pushers to linedef control
//
// Revision 1.13  1998/03/23  03:29:54  killough
// Fix savegame crash caused in P_ArchiveWorld
//
// Revision 1.12  1998/03/20  00:30:12  phares
// Changed friction to linedef control
//
// Revision 1.11  1998/03/09  07:20:23  killough
// Add generalized scrollers
//
// Revision 1.10  1998/03/02  12:07:18  killough
// fix stuck-in wall loadgame bug, automap status
//
// Revision 1.9  1998/02/24  08:46:31  phares
// Pushers, recoil, new friction, and over/under work
//
// Revision 1.8  1998/02/23  04:49:42  killough
// Add automap marks and properties to saved state
//
// Revision 1.7  1998/02/23  01:02:13  jim
// fixed elevator size, comments
//
// Revision 1.4  1998/02/17  05:43:33  killough
// Fix savegame crashes and monster sleepiness
// Save new RNG info
// Fix original plats height bug
//
// Revision 1.3  1998/02/02  22:17:55  jim
// Extended linedef types
//
// Revision 1.2  1998/01/26  19:24:21  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:07  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------

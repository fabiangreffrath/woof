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

#include <string.h>

#include "am_map.h"
#include "d_player.h"
#include "d_think.h"
#include "d_ticcmd.h"
#include "doomdata.h"
#include "doomdef.h"
#include "doomstat.h"
#include "i_system.h"
#include "info.h"
#include "m_array.h"
#include "m_random.h"
#include "p_enemy.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "p_pspr.h"
#include "p_saveg.h"
#include "p_spec.h"
#include "p_tick.h"
#include "r_data.h"
#include "r_defs.h"
#include "r_state.h"
#include "w_wad.h" // [FG] W_LumpLength()
#include "z_zone.h"

byte *save_p;

saveg_compat_t saveg_compat;

// Pad to 4-byte boundaries

inline static void saveg_read_pad(void)
{
    int padding = (4 - ((intptr_t)save_p & 3)) & 3;

    for (int i = 0; i < padding; ++i)
    {
        saveg_read8();
    }
}

// Pointers

inline static void *saveg_readp(void)
{
    return (void *) (intptr_t) saveg_read32();
}

// Enum values are 32-bit integers.

#define saveg_read_enum saveg_read32

// [crispy] replace indizes with corresponding pointers
static thinker_t *P_IndexToThinker(int index)
{
    thinker_t *th;
    int i;

    if (!index)
    {
        return NULL;
    }

    for (th = thinkercap.next, i = 0; th != &thinkercap; th = th->next)
    {
        if (th->function.p1 == P_MobjThinker)
        {
            i++;
            if (i == index)
            {
                return th;
            }
        }
    }

    return NULL;
}

//
// Structure read/write functions
//

static void saveg_read_mapthing_doom_t(mapthing_doom_t *str)
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

static void saveg_read_thinker_t(thinker_t *str)
{
    // struct thinker_s* prev;
    str->prev = saveg_readp();

    // struct thinker_s* next;
    str->next = saveg_readp();

    // think_t function;
    str->function.v = (actionf_v)saveg_readp();

    // struct thinker_s* cnext;
    str->cnext = saveg_readp();

    // struct thinker_s* cprev;
    str->cprev = saveg_readp();

    // unsigned references;
    str->references = saveg_read32();
}

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
    saveg_read_mapthing_doom_t((mapthing_doom_t*)&str->spawnpoint);

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
    P_SetActualHeight(str); 
}

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

    str->oldsx2 = str->sx2;
    str->oldsy2 = str->sy2;

    str->sxf = 0;
    str->syf = 0;
}

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
    for (i = 0; i < NUMPOWERS; ++i)
    {
        str->powers[i] = saveg_read32();
    }

    // boolean cards[NUMCARDS];
    for (i = 0; i < NUMCARDS; ++i)
    {
        str->cards[i] = saveg_read32();
    }

    // boolean backpack;
    str->backpack = saveg_read32();

    // int frags[MAXPLAYERS];
    for (i = 0; i < MAXPLAYERS; ++i)
    {
        str->frags[i] = saveg_read32();
    }

    // weapontype_t readyweapon;
    str->readyweapon = saveg_read_enum();

    // weapontype_t pendingweapon;
    str->pendingweapon = saveg_read_enum();

    // boolean weaponowned[NUMWEAPONS];
    for (i = 0; i < NUMWEAPONS; ++i)
    {
        str->weaponowned[i] = saveg_read32();
    }

    // int ammo[NUMAMMO];
    for (i = 0; i < NUMAMMO; ++i)
    {
        str->ammo[i] = saveg_read32();
    }

    // int maxammo[NUMAMMO];
    for (i = 0; i < NUMAMMO; ++i)
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
    for (i = 0; i < NUMPSPRITES; ++i)
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

    if (saveg_compat > saveg_woof600)
    {
        // [Woof!]: fixed_t pitch;
        str->pitch = saveg_read32();

        // [Woof!]: fixed_t oldpitch;
        str->oldpitch = saveg_read32();

        // [Woof!]: fixed_t slope;
        str->slope = saveg_read32();

        // [Woof!]: int maxkilldiscount;
        str->maxkilldiscount = saveg_read32();
    }
    else
    {
        str->pitch = 0;
        str->oldpitch = 0;
        str->slope = 0;
        str->maxkilldiscount = 0;
    }

    if (saveg_compat > saveg_woof1300)
    {
        // [Woof!]: int num_visitedlevels;
        str->num_visitedlevels = saveg_read32();

        // [Woof!]: level_t *visitedlevels;
        array_clear(str->visitedlevels);
        for (int i = 0; i < str->num_visitedlevels; ++i)
        {
            level_t level = {0};
            level.episode = saveg_read32();
            level.map = saveg_read32();
            array_push(str->visitedlevels, level);
        }

        // [Woof!]: weapontype_t lastweapon;
        str->lastweapon = saveg_read_enum();
    }
    else
    {
        str->num_visitedlevels = 0;
        array_clear(str->visitedlevels);
        str->lastweapon = wp_nochange;
    }

    str->nextweapon = str->readyweapon;

    str->switching = weapswitch_none;
}

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

static void saveg_read_vldoor_t(vldoor_t *str)
{
    int sector, line;

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
    //jff 1/31/98 unarchive line remembered by door as well
    line = saveg_read32();
    if (line == -1)
    {
        str->line = NULL;
    }
    else
    {
        str->line = &lines[line];
    }

    // int lighttag;
    str->lighttag = saveg_read32();
}

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

static void saveg_read_button_t(button_t *str)
{
    // line_t *line
    int line = saveg_read32();
    str->line = &lines[line];

    // bwhere_e where
    str->where = (bwhere_e)saveg_read32();

    // int btexture
    str->btexture = saveg_read32();

    // int btimer
    str->btimer = saveg_read32();
}

//
// P_UnArchivePlayers
//

void P_UnArchivePlayers(void)
{
    for (int i = 0; i < MAXPLAYERS; i++)
    {
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
}

//
// P_UnArchiveWorld
//

void P_UnArchiveWorld(void)
{
    int i;
    sector_t *sec;
    line_t *li;

    saveg_read_pad(); // killough 3/22/98

    // do sectors
    for (i = 0, sec = sectors; i < numsectors; i++, sec++)
    {
        // [crispy] add overflow guard for the flattranslation[] array
        short floorpic, ceilingpic;

        // killough 10/98: load full floor & ceiling heights, including
        // fractions

        sec->floorheight = saveg_read32();
        sec->ceilingheight = saveg_read32();

        floorpic = saveg_read16();
        ceilingpic = saveg_read16();
        sec->lightlevel = saveg_read16();
        sec->special = saveg_read16();
        sec->tag = saveg_read16();
        sec->ceilingdata = 0; // jff 2/22/98 now three thinker fields, not two
        sec->floordata = 0;
        sec->lightingdata = 0;
        sec->soundtarget = 0;

        // [crispy] add overflow guard for the flattranslation[] array
        if (floorpic >= 0 && floorpic < numflats
            && W_LumpLength(firstflat + floorpic) >= 64 * 64)
        {
            sec->floorpic = floorpic;
        }
        if (ceilingpic >= 0 && ceilingpic < numflats
            && W_LumpLength(firstflat + ceilingpic) >= 64 * 64)
        {
            sec->ceilingpic = ceilingpic;
        }
    }

    // do lines
    for (i = 0, li = lines; i < numlines; i++, li++)
    {
        int j;

        li->flags = saveg_read16();
        li->special = saveg_read16();
        li->id = saveg_read16();

        for (j = 0; j < 2; j++)
        {
            if (li->sidenum[j] != NO_INDEX)
            {
                side_t *si = &sides[li->sidenum[j]];

                // killough 10/98: load full sidedef offsets, including
                // fractions

                si->textureoffset = saveg_read32();
                si->rowoffset = saveg_read32();
                si->oldtextureoffset = si->textureoffset;
                si->oldrowoffset = si->rowoffset;

                si->toptexture = saveg_read16();
                si->bottomtexture = saveg_read16();
                si->midtexture = saveg_read16();
            }
        }
    }
}

//
// Thinkers
//

typedef enum
{
    tc_end,
    tc_mobj,
    tc_ambient,
} thinkerclass_t;

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

void P_UnArchiveThinkers(void)
{
    thinker_t *th;
    mobj_t **mobj_p; // killough 2/14/98: Translation table
    size_t size;     // killough 2/14/98: size of or index into table
    size_t idx;      // haleyjd 11/03/06: separate index var

    // killough 3/26/98: Load boss brain state
    brain.easy = saveg_read32();
    brain.targeton = saveg_read32();

    // remove all the current thinkers
    for (th = thinkercap.next; th != &thinkercap;)
    {
        thinker_t *next = th->next;
        if (th->function.p1 == P_MobjThinker)
        {
            P_RemoveMobj((mobj_t *)th);
        }
        th = next;
    }
    P_InitThinkers();

    // killough 2/14/98: count number of thinkers by skipping through them
    {
        byte *sp = save_p; // save pointer and skip header
        mobj_t tmp;
        for (size = 1; *save_p++ == tc_mobj; size++) // killough 2/14/98
        { // skip all entries, adding up count
            saveg_read_pad();
            saveg_read_mobj_t(&tmp);
        }

        if (*--save_p != tc_end)
        {
            I_Error("Unknown tclass %i in savegame", *save_p);
        }

        // first table entry special: 0 maps to NULL
        *(mobj_p = Z_Malloc(size * sizeof *mobj_p, PU_STATIC, 0)) = 0; // table of pointers
        save_p = sp; // restore save pointer
    }

    // read in saved thinkers
    // haleyjd 11/03/06: use idx to save "size" for rangechecking
    for (idx = 1; *save_p++ == tc_mobj; idx++) // killough 2/14/98
    {
        mobj_t *mobj = arena_alloc(thinkers_arena, mobj_t);

        // killough 2/14/98 -- insert pointers to thinkers into table, in order:
        mobj_p[idx] = mobj;

        saveg_read_pad();
        saveg_read_mobj_t(mobj);
        mobj->state = states + (size_t)mobj->state;

        if (mobj->player)
        {
            (mobj->player = &players[(size_t)mobj->player - 1])->mo = mobj;
        }

        P_SetThingPosition(mobj);
        mobj->info = &mobjinfo[mobj->type];

        // killough 2/28/98:
        // Fix for falling down into a wall after savegame loaded:
        //      mobj->floorz = mobj->subsector->sector->floorheight;
        //      mobj->ceilingz = mobj->subsector->sector->ceilingheight;

        mobj->thinker.function.p1 = P_MobjThinker;
        P_AddThinker(&mobj->thinker);

        // pre Woof 16.0.0 hack
        if (mobj->type == MT_MUSICSOURCE)
        {
            mobj->args[0] = mobj->health - 1000;
        }
    }

    // killough 2/14/98: adjust target and tracer fields, plus
    // lastenemy field, to correctly point to mobj thinkers.
    // NULL entries automatically handled by first table entry.
    //
    // killough 11/98: use P_SetNewTarget() to set fields

    for (th = thinkercap.next; th != &thinkercap; th = th->next)
    {
        P_SetNewTarget(&((mobj_t *)th)->target,
                       mobj_p[(size_t)((mobj_t *)th)->target]);

        P_SetNewTarget(&((mobj_t *)th)->tracer,
                       mobj_p[(size_t)((mobj_t *)th)->tracer]);

        P_SetNewTarget(&((mobj_t *)th)->lastenemy,
                       mobj_p[(size_t)((mobj_t *)th)->lastenemy]);

        // phares: added two new fields for Sprite Height problem

        P_SetNewTarget(&((mobj_t *)th)->above_thing,
                       mobj_p[(size_t)((mobj_t *)th)->above_thing]);

        P_SetNewTarget(&((mobj_t *)th)->below_thing,
                       mobj_p[(size_t)((mobj_t *)th)->below_thing]);
    }

    // killough 9/14/98: restore soundtargets

    for (int i = 0; i < numsectors; i++)
    {
        mobj_t *target = saveg_readp();

        // haleyjd 11/03/06: rangecheck for security
        if ((size_t)target < size)
        {
            P_SetNewTarget(&sectors[i].soundtarget, mobj_p[(size_t)target]);
        }
        else
        {
            sectors[i].soundtarget = NULL;
        }
    }

    Z_Free(mobj_p); // free translation table

    // killough 3/26/98: Spawn icon landings:
    if (gamemode == commercial)
    {
        P_SpawnBrainTargets();
    }
}

//
// Specials
//

enum
{
    tc_ceiling,
    tc_door,
    tc_floor,
    tc_plat,
    tc_flash,
    tc_strobe,
    tc_glow,
    tc_elevator, // jff 2/22/98 new elevator type thinker
    tc_scroll,   // killough 3/7/98: new scroll effect thinker
    tc_pusher,   // phares 3/22/98:  new push/pull effect thinker
    tc_flicker,  // killough 10/4/98
    tc_endspecials,
    tc_friction, // store friction for complevel Boom
    tc_button,
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

//
// P_UnArchiveSpecials
//

void P_UnArchiveSpecials(void)
{
    byte tclass;

    // read in saved thinkers
    while ((tclass = saveg_read8()) != tc_endspecials) // killough 2/14/98
    {
        switch (tclass)
        {
            case tc_ceiling:
                saveg_read_pad();
                {
                    ceiling_t *ceiling = arena_alloc(thinkers_arena, ceiling_t);
                    saveg_read_ceiling_t(ceiling);
                    ceiling->sector->ceilingdata = ceiling; // jff 2/22/98

                    if (ceiling->thinker.function.p1)
                    {
                        ceiling->thinker.function.p1 = T_MoveCeilingAdapter;
                    }

                    P_AddThinker(&ceiling->thinker);
                    P_AddActiveCeiling(ceiling);
                    break;
                }

            case tc_door:
                saveg_read_pad();
                {
                    vldoor_t *door = arena_alloc(thinkers_arena, vldoor_t);
                    saveg_read_vldoor_t(door);
                    door->sector->ceilingdata = door; // jff 2/22/98
                    door->thinker.function.p1 = T_VerticalDoorAdapter;
                    P_AddThinker(&door->thinker);
                    break;
                }

            case tc_floor:
                saveg_read_pad();
                {
                    floormove_t *floor = arena_alloc(thinkers_arena, floormove_t);
                    saveg_read_floormove_t(floor);
                    floor->sector->floordata = floor; // jff 2/22/98
                    floor->thinker.function.p1 = T_MoveFloorAdapter;
                    P_AddThinker(&floor->thinker);
                    break;
                }

            case tc_plat:
                saveg_read_pad();
                {
                    plat_t *plat = arena_alloc(thinkers_arena, plat_t);
                    saveg_read_plat_t(plat);
                    plat->sector->floordata = plat; // jff 2/22/98

                    if (plat->thinker.function.p1)
                    {
                        plat->thinker.function.p1 = T_PlatRaiseAdapter;
                    }

                    P_AddThinker(&plat->thinker);
                    P_AddActivePlat(plat);
                    break;
                }

            case tc_flash:
                saveg_read_pad();
                {
                    lightflash_t *flash = arena_alloc(thinkers_arena, lightflash_t);
                    saveg_read_lightflash_t(flash);
                    flash->thinker.function.p1 = T_LightFlashAdapter;
                    P_AddThinker(&flash->thinker);
                    break;
                }

            case tc_strobe:
                saveg_read_pad();
                {
                    strobe_t *strobe = arena_alloc(thinkers_arena, strobe_t);
                    saveg_read_strobe_t(strobe);
                    strobe->thinker.function.p1 = T_StrobeFlashAdapter;
                    P_AddThinker(&strobe->thinker);
                    break;
                }

            case tc_glow:
                saveg_read_pad();
                {
                    glow_t *glow = arena_alloc(thinkers_arena, glow_t);
                    saveg_read_glow_t(glow);
                    glow->thinker.function.p1 = T_GlowAdapter;
                    P_AddThinker(&glow->thinker);
                    break;
                }

            case tc_flicker: // killough 10/4/98
                saveg_read_pad();
                {
                    fireflicker_t *flicker = arena_alloc(thinkers_arena, fireflicker_t);
                    saveg_read_fireflicker_t(flicker);
                    flicker->thinker.function.p1 = T_FireFlickerAdapter;
                    P_AddThinker(&flicker->thinker);
                    break;
                }

                // jff 2/22/98 new case for elevators
            case tc_elevator:
                saveg_read_pad();
                {
                    elevator_t *elevator = arena_alloc(thinkers_arena, elevator_t);
                    saveg_read_elevator_t(elevator);
                    elevator->sector->floordata = elevator;   // jff 2/22/98
                    elevator->sector->ceilingdata = elevator; // jff 2/22/98
                    elevator->thinker.function.p1 = T_MoveElevatorAdapter;
                    P_AddThinker(&elevator->thinker);
                    break;
                }

            case tc_scroll: // killough 3/7/98: scroll effect thinkers
                {
                    scroll_t *scroll = arena_alloc(thinkers_arena, scroll_t);
                    saveg_read_scroll_t(scroll);
                    scroll->thinker.function.p1 = T_ScrollAdapter;
                    P_AddThinker(&scroll->thinker);
                    break;
                }

            case tc_pusher: // phares 3/22/98: new Push/Pull effect thinkers
                {
                    pusher_t *pusher = arena_alloc(thinkers_arena, pusher_t);
                    saveg_read_pusher_t(pusher);
                    pusher->thinker.function.p1 = T_PusherAdapter;
                    // can't convert from index to pointer, old save version
                    if (pusher->source == NULL)
                    {
                        pusher->source = P_GetPushThing(pusher->affectee);
                        if (pusher->type == p_push && pusher->source == NULL)
                        {
                            I_Error("Pusher thinker without source in sector %d",
                                    pusher->affectee);
                        }
                    }
                    P_AddThinker(&pusher->thinker);
                    break;
                }

            // load friction for complevel Boom
            case tc_friction:
                saveg_read_pad();
                {
                    friction_t *friction = arena_alloc(thinkers_arena, friction_t);
                    saveg_read_friction_t(friction);
                    friction->thinker.function.p1 = T_FrictionAdapter;
                    P_AddThinker(&friction->thinker);
                    break;
                }

            case tc_button:
                saveg_read_pad();
                {
                    button_t button;
                    saveg_read_button_t(&button);
                    P_StartButton(button.line, button.where, button.btexture,
                                  button.btimer);
                    break;
                }

            default:
                I_Error("Unknown tclass %i in savegame", tclass);
        }
    }
}

// killough 2/16/98: save/restore random number generator state information

void P_UnArchiveRNG(void)
{
    saveg_read_rng_t(&rng);
}

// killough 2/22/98: Save/restore automap state
void P_UnArchiveMap(void)
{
    automapactive = saveg_read32();
    viewactive = saveg_read32();
    followplayer = saveg_read32();
    automap_grid = saveg_read32();

    if (automapactive)
    {
        AM_Start();
    }

    markpointnum = saveg_read32();

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

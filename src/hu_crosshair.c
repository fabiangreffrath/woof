//
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
//  Copyright (C) 2023-2024 Fabian Greffrath
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
// DESCRIPTION:  Crosshair HUD component
//
//-----------------------------------------------------------------------------

#include "hu_crosshair.h"
#include "d_items.h"
#include "doomstat.h"
#include "m_swap.h"
#include "p_map.h"
#include "p_mobj.h"
#include "r_data.h"
#include "r_main.h"
#include "r_state.h"
#include "st_stuff.h"
#include "v_patch.h"
#include "v_video.h"

static player_t *plr = players;

int hud_crosshair;

crosstarget_t hud_crosshair_target;
boolean hud_crosshair_health;
boolean hud_crosshair_lockon; // [Alaux] Crosshair locks on target
int hud_crosshair_color;
int hud_crosshair_target_color;

typedef struct
{
    patch_t *patch;
    int w, h, x, y;
    byte *cr;
} crosshair_t;

static crosshair_t crosshair;

const char *crosshair_lumps[HU_CROSSHAIRS] = {
    NULL,      "CROSS00", "CROSS01", "CROSS02", "CROSS03",
    "CROSS04", "CROSS05", "CROSS06", "CROSS07", "CROSS08"};

const char *crosshair_strings[HU_CROSSHAIRS] = {
    "Off",    "Cross",      "Angle",   "Dot",      "Big Cross",
    "Circle", "Big Circle", "Chevron", "Chevrons", "Arcs"};

void HU_InitCrosshair(void)
{
    for (int i = 1; i < HU_CROSSHAIRS; i++)
    {
        int lump = W_CheckNumForName(crosshair_lumps[i]);

        if (W_IsWADLump(lump))
        {
            if (V_LumpIsPatch(lump))
            {
                crosshair_strings[i] = crosshair_lumps[i];
            }
            else
            {
                crosshair_lumps[i] = NULL;
            }
        }
    }
}

void HU_StartCrosshair(void)
{
    if (crosshair_lumps[hud_crosshair])
    {
        crosshair.patch =
            V_CachePatchName(crosshair_lumps[hud_crosshair], PU_STATIC);

        crosshair.w = SHORT(crosshair.patch->width) / 2;
        crosshair.h = SHORT(crosshair.patch->height) / 2;
    }
    else
    {
        crosshair.patch = NULL;
    }
}

mobj_t *crosshair_target; // [Alaux] Lock crosshair on target

static crange_idx_e CRByHealth(int health, int maxhealth, boolean invul)
{
    if (invul)
    {
        return CR_GRAY;
    }

    health = 100 * health / maxhealth;

    if (health < health_red)
    {
        return CR_RED;
    }
    else if (health < health_yellow)
    {
        return CR_GOLD;
    }
    else if (health <= health_green)
    {
        return CR_GREEN;
    }
    else
    {
        return CR_BLUE1;
    }
}

void HU_UpdateCrosshair(void)
{
    plr = &players[displayplayer];

    crosshair.x = SCREENWIDTH / 2;
    crosshair.y = (screenblocks <= 10) ? (SCREENHEIGHT - st_height) / 2
                                       : SCREENHEIGHT / 2;

    boolean invul = ST_PlayerInvulnerable(plr);

    if (hud_crosshair_health)
    {
        crosshair.cr = colrngs[CRByHealth(plr->health, 100, invul)];
    }
    else
    {
        crosshair.cr = colrngs[hud_crosshair_color];
    }

    if (STRICTMODE(hud_crosshair_target || hud_crosshair_lockon))
    {
        angle_t an = plr->mo->angle;
        ammotype_t ammo = weaponinfo[plr->readyweapon].ammo;
        fixed_t range = (ammo == am_noammo) ? MELEERANGE : 16 * 64 * FRACUNIT;
        boolean intercepts_overflow_enabled = overflow[emu_intercepts].enabled;

        crosshair_target = linetarget = NULL;

        overflow[emu_intercepts].enabled = false;
        P_AimLineAttack(plr->mo, an, range, CROSSHAIR_AIM);
        if (!direct_vertical_aiming && (ammo == am_misl || ammo == am_cell))
        {
            if (!linetarget)
            {
                P_AimLineAttack(plr->mo, an + (1 << 26), range, CROSSHAIR_AIM);
            }
            if (!linetarget)
            {
                P_AimLineAttack(plr->mo, an - (1 << 26), range, CROSSHAIR_AIM);
            }
        }
        overflow[emu_intercepts].enabled = intercepts_overflow_enabled;

        if (linetarget && !(linetarget->flags & MF_SHADOW))
        {
            crosshair_target = linetarget;
        }

        if (hud_crosshair_target && crosshair_target)
        {
            // [Alaux] Color crosshair by target health
            if (hud_crosshair_target == crosstarget_health)
            {
                crosshair.cr = colrngs[CRByHealth(
                    crosshair_target->health,
                    crosshair_target->info->spawnhealth, false)];
            }
            else
            {
                crosshair.cr = colrngs[hud_crosshair_target_color];
            }
        }
    }
}

void HU_UpdateCrosshairLock(int x, int y)
{
    int w = (crosshair.w * video.xscale) >> FRACBITS;
    int h = (crosshair.h * video.yscale) >> FRACBITS;

    x = viewwindowx + clampi(x, w, viewwidth - w - 1);
    y = viewwindowy + clampi(y, h, viewheight - h - 1);

    crosshair.x = (x << FRACBITS) / video.xscale - video.deltaw;
    crosshair.y = (y << FRACBITS) / video.yscale;
}

void HU_DrawCrosshair(void)
{
    if (plr->playerstate != PST_LIVE || automapactive || menuactive || paused)
    {
        return;
    }

    if (crosshair.patch)
    {
        V_DrawPatchTranslated(crosshair.x - crosshair.w,
                              crosshair.y - crosshair.h, crosshair.patch,
                              crosshair.cr);
    }
}

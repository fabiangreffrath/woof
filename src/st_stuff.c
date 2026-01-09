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
//
// DESCRIPTION:
//      Status bar code.
//      Does the face/direction indicator animatin.
//      Does palette indicators as well (red pain/berserk, bright pickup)
//
//-----------------------------------------------------------------------------

#include "st_stuff.h"

#include <math.h>

#include "am_map.h"
#include "d_event.h"
#include "d_items.h"
#include "d_player.h"
#include "deh_misc.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "g_game.h"
#include "g_umapinfo.h"
#include "hu_command.h"
#include "hu_obituary.h"
#include "i_video.h"
#include "info.h"
#include "m_array.h"
#include "m_cheat.h"
#include "m_config.h"
#include "m_misc.h"
#include "m_random.h"
#include "m_swap.h"
#include "p_inter.h"
#include "p_mobj.h"
#include "p_user.h"
#include "hu_crosshair.h"
#include "r_data.h"
#include "r_defs.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_state.h"
#include "s_sound.h"
#include "st_carousel.h"
#include "st_stuff.h"
#include "st_sbardef.h"
#include "st_widgets.h"
#include "tables.h"
#include "v_patch.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

int st_height = 0, st_height_screenblocks10 = 0;

//
// STATUS BAR DATA
//

// Palette indices.
// For damage/bonus red-/gold-shifts
#define STARTREDPALS            1
#define STARTBONUSPALS          9
#define NUMREDPALS              8
#define NUMBONUSPALS            4
// Radiation suit, green shift.
#define RADIATIONPAL            13

// Number of status faces.
#define ST_NUMPAINFACES         5
#define ST_NUMSTRAIGHTFACES     3
#define ST_NUMTURNFACES         2
#define ST_NUMSPECIALFACES      3

#define ST_FACESTRIDE \
          (ST_NUMSTRAIGHTFACES+ST_NUMTURNFACES+ST_NUMSPECIALFACES)

#define ST_NUMEXTRAFACES        2
#define ST_NUMXDTHFACES         9

#define ST_NUMFACES \
    (ST_FACESTRIDE * ST_NUMPAINFACES + ST_NUMEXTRAFACES + ST_NUMXDTHFACES)

#define ST_TURNOFFSET        (ST_NUMSTRAIGHTFACES)
#define ST_OUCHOFFSET        (ST_TURNOFFSET + ST_NUMTURNFACES)
#define ST_EVILGRINOFFSET    (ST_OUCHOFFSET + 1)
#define ST_RAMPAGEOFFSET     (ST_EVILGRINOFFSET + 1)
#define ST_GODFACE           (ST_NUMPAINFACES * ST_FACESTRIDE)
#define ST_DEADFACE          (ST_GODFACE + 1)
#define ST_XDTHFACE          (ST_DEADFACE + 1)

#define ST_EVILGRINCOUNT     (2 * TICRATE)
#define ST_STRAIGHTFACECOUNT (TICRATE / 2)
#define ST_TURNCOUNT         (1 * TICRATE)
#define ST_OUCHCOUNT         (1 * TICRATE)
#define ST_RAMPAGEDELAY      (2 * TICRATE)

#define ST_MUCHPAIN          20

// graphics are drawn to a backing screen and blitted to the real screen
static pixel_t *st_backing_screen = NULL;

static boolean sts_colored_numbers;

static boolean sts_pct_always_gray;

//jff 2/16/98 status color change levels
static int ammo_red;      // ammo percent less than which status is red
static int ammo_yellow;   // ammo percent less is yellow more green
int health_red;           // health amount less than which status is red
int health_yellow;        // health amount less than which status is yellow
int health_green;         // health amount above is blue, below is green
static int armor_red;     // armor amount less than which status is red
static int armor_yellow;  // armor amount less than which status is yellow
static int armor_green;   // armor amount above is blue, below is green

static boolean hud_armor_type; // color of armor depends on type

static boolean weapon_carousel;

static sbardef_t *sbardef;

static statusbar_t *statusbar;

static int st_cmd_x, st_cmd_y;

hud_anchoring_t hud_anchoring;
int st_wide_shift;

static patch_t **facepatches = NULL;
static patch_t **facebackpatches = NULL;

static int have_xdthfaces;

boolean ST_PlayerInvulnerable(player_t *player)
{
    return (player->cheats & CF_GODMODE) ||
        (player->powers[pw_invulnerability] > 4 * 32) ||
        (player->powers[pw_invulnerability] & 8);
}

//
// STATUS BAR CODE
//

static patch_t *CachePatchName(const char *name)
{
    int lumpnum = W_CheckNumForName(name);
    if (lumpnum < 0)
    {
        lumpnum = (W_CheckNumForName)(name, ns_sprites);
        if (lumpnum < 0)
        {
            return NULL;
        }
    }
    return V_CachePatchNum(lumpnum, PU_STATIC);
}

static void LoadFacePatches(void)
{
    char lump[9] = {0};

    int count;

    for (count = 0; count < ST_NUMPAINFACES; ++count)
    {
        for (int straightface = 0; straightface < ST_NUMSTRAIGHTFACES;
             ++straightface)
        {
            M_snprintf(lump, sizeof(lump), "STFST%d%d", count, straightface);
            array_push(facepatches, V_CachePatchName(lump, PU_STATIC));
        }

        M_snprintf(lump, sizeof(lump), "STFTR%d0", count); // turn right
        array_push(facepatches, V_CachePatchName(lump, PU_STATIC));

        M_snprintf(lump, sizeof(lump), "STFTL%d0", count); // turn left
        array_push(facepatches, V_CachePatchName(lump, PU_STATIC));

        M_snprintf(lump, sizeof(lump), "STFOUCH%d", count); // ouch!
        array_push(facepatches, V_CachePatchName(lump, PU_STATIC));

        M_snprintf(lump, sizeof(lump), "STFEVL%d", count); // evil grin ;)
        array_push(facepatches, V_CachePatchName(lump, PU_STATIC));

        M_snprintf(lump, sizeof(lump), "STFKILL%d", count); // pissed off
        array_push(facepatches, V_CachePatchName(lump, PU_STATIC));
    }

    M_snprintf(lump, sizeof(lump), "STFGOD0");
    array_push(facepatches, V_CachePatchName(lump, PU_STATIC));

    M_snprintf(lump, sizeof(lump), "STFDEAD0");
    array_push(facepatches, V_CachePatchName(lump, PU_STATIC));

    // [FG] support face gib animations as in the 3DO/Jaguar/PSX ports
    for (count = 0; count < ST_NUMXDTHFACES; ++count)
    {
        M_snprintf(lump, sizeof(lump), "STFXDTH%d", count);

        if (W_CheckNumForName(lump) != -1)
        {
            array_push(facepatches, V_CachePatchName(lump, PU_STATIC));
        }
        else
        {
            break;
        }
    }
    have_xdthfaces = count;

    for (count = 0; count < MAXPLAYERS; ++count)
    {
        M_snprintf(lump, sizeof(lump), "STFB%d", count);
        array_push(facebackpatches, V_CachePatchName(lump, PU_STATIC));
    }
}

static boolean CheckWidgetState(widgetstate_t state)
{
    if ((state == HUD_WIDGET_AUTOMAP && automapactive)
        || (state == HUD_WIDGET_HUD && !automapactive)
        || (state == HUD_WIDGET_ALWAYS))
    {
        return true;
    }

    return false;
}

static sbarwidgettype_t GetWidgetType(const char *name)
{
    if (name)
    {
        for (sbarwidgettype_t type = 0; type < sbw_names_len; ++type)
        {
            if (!strcasecmp(sbw_names[type], name))
            {
                return type;
            }
        }
    }
    return sbw_none;
}

static int StatsPct(int count, int total)
{
    return total ? (count * 100 / total) : 100;
}

static boolean CheckConditions(sbarcondition_t *conditions, player_t *player)
{
    boolean result = true;
    int currsessiontype = netgame ? MIN(deathmatch + 1, 2) : 0;
    // TODO
    // boolean compacthud = frame_width < frame_adjusted_width;

    sbarcondition_t *cond;
    array_foreach(cond, conditions)
    {
        switch (cond->condition)
        {
            case sbc_weaponowned:
                if (cond->param >= 0 && cond->param < NUMWEAPONS)
                {
                    result &= !!player->weaponowned[cond->param];
                }
                break;

            case sbc_weaponnotowned:
                if (cond->param >= 0 && cond->param < NUMWEAPONS)
                {
                    result &= !player->weaponowned[cond->param];
                }
                break;

            case sbc_weaponselected:
                result &= player->readyweapon == cond->param;
                break;

            case sbc_weaponnotselected:
                result &= player->readyweapon != cond->param;
                break;

            case sbc_weaponhasammo:
                if (cond->param >= 0 && cond->param < NUMWEAPONS)
                {
                    result &= weaponinfo[cond->param].ammo != am_noammo;
                }
                break;

            case sbc_selectedweaponhasammo:
                result &= weaponinfo[player->readyweapon].ammo != am_noammo;
                break;

            case sbc_selectedweaponammotype:
                result &=
                    weaponinfo[player->readyweapon].ammo == cond->param;
                break;

            case sbc_weaponslotowned:
                {
                    boolean owned = false;
                    for (int i = 0; i < NUMWEAPONS; ++i)
                    {
                        if (weaponinfo[i].slot == cond->param
                            && player->weaponowned[i])
                        {
                            owned = true;
                            break;
                        }
                    }
                    result &= owned;
                }
                break;

            case sbc_weaponslotnotowned:
                {
                    boolean notowned = true;
                    for (int i = 0; i < NUMWEAPONS; ++i)
                    {
                        if (weaponinfo[i].slot == cond->param
                            && player->weaponowned[i])
                        {
                            notowned = false;
                            break;
                        }
                    }
                    result &= notowned;
                }
                break;

            case sbc_weaponslotselected:
                result &= weaponinfo[player->readyweapon].slot == cond->param;
                break;

            case sbc_weaponslotnotselected:
                result &= weaponinfo[player->readyweapon].slot != cond->param;
                break;

            case sbc_itemowned:
                result &=
                    !!P_EvaluateItemOwned((itemtype_t)cond->param, player);
                break;

            case sbc_itemnotowned:
                result &=
                    !P_EvaluateItemOwned((itemtype_t)cond->param, player);
                break;

            case sbc_featurelevelgreaterequal:
                // always MBF21
                result &= 7 >= cond->param;
                break;

            case sbc_featurelevelless:
                // always MBF21
                result &= 7 < cond->param;
                break;

            case sbc_sessiontypeequal:
                result &= currsessiontype == cond->param;
                break;

            case sbc_sessiontypenotequal:
                result &= currsessiontype != cond->param;
                break;

            case sbc_modeeequal:
                result &= gamemode == (cond->param + 1);
                break;

            case sbc_modenotequal:
                result &= gamemode != (cond->param + 1);
                break;

            case sbc_hudmodeequal:
                // TODO
                // result &= ( !!cond->param == compacthud );
                result &= (!!cond->param == false);
                break;

            case sbc_automapmode:
                {
                    int enabled = 0;
                    if (cond->param & sbc_mode_overlay)
                    {
                        enabled |= (automapactive && automapoverlay);
                    }
                    if (cond->param & sbc_mode_automap)
                    {
                        enabled |= (automapactive && !automapoverlay);
                    }
                    if (cond->param & sbc_mode_hud)
                    {
                        enabled |= !automapactive;
                    }
                    result &= (enabled > 0);
                }
                break;

            case sbc_widgetenabled:
                {
                    widgetstate_t state = HUD_WIDGET_OFF;
                    sbarwidgettype_t type = GetWidgetType(cond->param_string);
                    switch (type)
                    {
                        case sbw_monsec:
                            state = hud_level_stats;
                            break;
                        case sbw_time:
                            state = hud_level_time;
                            break;
                        case sbw_coord:
                            state = hud_player_coords;
                            break;
                        default:
                            break;
                    }
                    result &= CheckWidgetState(state);
                }
                break;

            case sbc_widgetdisabled:
                {
                    widgetstate_t state = HUD_WIDGET_OFF;
                    sbarwidgettype_t type = GetWidgetType(cond->param_string);
                    switch (type)
                    {
                        case sbw_monsec:
                            state = hud_level_stats;
                            break;
                        case sbw_time:
                            state = hud_level_time;
                            break;
                        case sbw_coord:
                            state = hud_player_coords;
                            break;
                        default:
                            break;
                    }
                    result &= !CheckWidgetState(state);
                }
                break;

            case sbc_healthgreaterequal:
                result &= (player->health >= cond->param);
                break;

            case sbc_healthless:
                result &= (player->health < cond->param);
                break;

            case sbc_healthgreaterequalpct:
                if (deh_max_health)
                {
                    result &=
                        ((player->health * 100 / deh_max_health) >= cond->param);
                }
                break;

            case sbc_healthlesspct:
                if (deh_max_health)
                {
                    result &=
                        ((player->health * 100 / deh_max_health) < cond->param);
                }
                break;

            case sbc_armorgreaterequal:
                result &= (player->armorpoints >= cond->param);
                break;

            case sbc_armorless:
                result &= (player->armorpoints < cond->param);
                break;

            case sbc_armorgreaterequalpct:
                if (deh_max_armor)
                {
                    result &= ((player->armorpoints * 100 / deh_max_armor)
                               >= cond->param);
                }
                break;

            case sbc_armorlesspct:
                if (deh_max_armor)
                {
                    result &=
                        ((player->armorpoints * 100 / deh_max_armor) < cond->param);
                }
                break;

            case sbc_ammogreaterequal:
                result &= (player->ammo[weaponinfo[player->readyweapon].ammo]
                           >= cond->param);
                break;

            case sbc_ammoless:
                result &= (player->ammo[weaponinfo[player->readyweapon].ammo]
                           < cond->param);
                break;

            case sbc_ammogreaterequalpct:
                {
                    ammotype_t type = weaponinfo[player->readyweapon].ammo;
                    int maxammo = player->maxammo[type];
                    if (maxammo)
                    {
                        result &= ((player->ammo[type] * 100 / maxammo)
                                   >= cond->param);
                    }
                }
                break;

            case sbc_ammolesspct:
                {
                    ammotype_t type = weaponinfo[player->readyweapon].ammo;
                    int maxammo = player->maxammo[type];
                    if (maxammo)
                    {
                        result &= ((player->ammo[type] * 100 / maxammo)
                                   < cond->param);
                    }
                }
                break;

            case sbc_ammotypegreaterequal:
                if (cond->param2 >= 0 && cond->param2 < NUMAMMO)
                {
                    result &= (player->ammo[cond->param2] >= cond->param);
                }
                break;

            case sbc_ammotypeless:
                if (cond->param2 >= 0 && cond->param2 < NUMAMMO)
                {
                    result &= (player->ammo[cond->param2] < cond->param);
                }
                break;

            case sbc_ammotypegreaterequalpct:
                if (cond->param2 >= 0 && cond->param2 < NUMAMMO)
                {
                    int maxammo = player->maxammo[cond->param2];
                    if (maxammo)
                    {
                        result &= ((player->ammo[cond->param2] * 100 / maxammo)
                                   >= cond->param);
                    }
                }
                break;

            case sbc_ammotypelesspct:
                if (cond->param2 >= 0 && cond->param2 < NUMAMMO)
                {
                    int maxammo = player->maxammo[cond->param2];
                    if (maxammo)
                    {
                        result &= ((player->ammo[cond->param2] * 100 / maxammo)
                                   < cond->param);
                    }
                }
                break;

            case sbc_widescreenequal:
                result &= ((cond->param == 1 && st_wide_shift)
                           || (cond->param == 0 && !st_wide_shift));
                break;

            case sbc_episodeequal:
                result &= (gameepisode == cond->param);
                break;

            case sbc_levelgreaterequal:
                result &= (gamemap >= cond->param);
                break;

            case sbc_levelless:
                result &= (gamemap < cond->param);
                break;

            case sbc_patchempty:
                if (cond->param_string)
                {
                    result &= V_PatchIsEmpty(W_GetNumForName(cond->param_string));
                }
                break;

            case sbc_patchnotempty:
                if (cond->param_string)
                {
                    result &= (!V_PatchIsEmpty(W_GetNumForName(cond->param_string)));
                }
                break;

            case sbc_killsless:
                result &= (player->killcount - player->maxkilldiscount < cond->param);
                break;
            case sbc_killsgreaterequal:
                result &= (player->killcount - player->maxkilldiscount >= cond->param);
                break;
            case sbc_itemsless:
                result &= (player->itemcount < cond->param);
                break;
            case sbc_itemsgreaterequal:
                result &= (player->itemcount >= cond->param);
                break;
            case sbc_secretsless:
                result &= (player->secretcount < cond->param);
                break;
            case sbc_secretsgreaterequal:
                result &= (player->secretcount >= cond->param);
                break;
            case sbc_killslesspct:
                result &= (StatsPct(player->killcount - player->maxkilldiscount,
                                    max_kill_requirement)
                           < cond->param);
                break;
            case sbc_killsgreaterequalpct:
                result &= (StatsPct(player->killcount - player->maxkilldiscount,
                                    max_kill_requirement)
                           >= cond->param);
                break;
            case sbc_itemslesspct:
                result &= (StatsPct(player->itemcount, totalitems) < cond->param);
                break;
            case sbc_itemsgreaterequalpct:
                result &= (StatsPct(player->itemcount, totalitems) >= cond->param);
                break;
            case sbc_secretslesspct:
                result &= (StatsPct(player->secretcount, totalsecret) < cond->param);
                break;
            case sbc_secretsgreaterequalpct:
                result &= (StatsPct(player->secretcount, totalsecret) >= cond->param);
                break;

            case sbc_powerless:
                if (cond->param2 >= 0 && cond->param2 < NUMPOWERS)
                {
                    result &=
                        (player->powers[cond->param2] < cond->param * TICRATE);
                }
                break;
            case sbc_powergreaterequal:
                if (cond->param2 >= 0 && cond->param2 < NUMPOWERS)
                {
                    result &=
                        (player->powers[cond->param2] >= cond->param * TICRATE);
                }
                break;
            case sbc_powerlesspct:
                if (cond->param2 >= 0 && cond->param2 < NUMPOWERS)
                {
                    result &= (player->powers[cond->param2] * 100
                                   / P_GetPowerDuration(cond->param2)
                               < cond->param);
                }
                break;
            case sbc_powergreaterequalpct:
                if (cond->param2 >= 0 && cond->param2 < NUMPOWERS)
                {
                    result &= (player->powers[cond->param2] * 100
                                   / P_GetPowerDuration(cond->param2)
                               >= cond->param);
                }
                break;

            case sbc_none:
            default:
                result = false;
                break;
        }
    }

    return result;
}

static int ResolveNumber(sbe_number_t *number, player_t *player)
{
    int result = 0;
    int param = number->param;

    switch (number->type)
    {
        case sbn_health:
            result = player->health;
            break;

        case sbn_armor:
            result = player->armorpoints;
            break;

        case sbn_frags:
            for (int p = 0; p < MAXPLAYERS; ++p)
            {
                if (player != &players[p])
                    result += player->frags[p];
                else
                    result -= player->frags[p];
            }
            break;

        case sbn_ammo:
            if (param >= 0 && param < NUMAMMO)
            {
                result = player->ammo[param];
            }
            break;

        case sbn_ammoselected:
            result = player->ammo[weaponinfo[player->readyweapon].ammo];
            break;

        case sbn_maxammo:
            if (param >= 0 && param < NUMAMMO)
            {
                result = player->maxammo[param];
            }
            break;

        case sbn_weaponammo:
            if (param >= 0 && param < NUMWEAPONS)
            {
                result = player->ammo[weaponinfo[param].ammo];
            }
            break;

        case sbn_weaponmaxammo:
            if (param >= 0 && param < NUMWEAPONS)
            {
                result = player->maxammo[weaponinfo[param].ammo];
            }
            break;

        case sbn_kills:
            result = player->killcount - player->maxkilldiscount;
            break;
        case sbn_items:
            result = player->itemcount;
            break;
        case sbn_secrets:
            result = player->secretcount;
            break;
        case sbn_killspct:
            result = StatsPct(player->killcount - player->maxkilldiscount,
                              max_kill_requirement);
            break;
        case sbn_itemspct:
            result = StatsPct(player->itemcount, totalitems);
            break;
        case sbn_secretspct:
            result = StatsPct(player->secretcount, totalsecret);
            break;
        case sbn_totalkills:
            result = max_kill_requirement;
            break;
        case sbn_totalitems:
            result = totalitems;
            break;
        case sbn_totalsecrets:
            result = totalsecret;
            break;

        case sbn_power:
            if (param >= 0 && param < NUMPOWERS)
            {
                result = (param == pw_strength) ? 0 : player->powers[param] / TICRATE;
            }
            break;

        case sbn_none:
        default:
            break;
    }

    return result;
}

static int CalcPainOffset(sbe_face_t *face, player_t *player)
{
    static int lasthealthcalc;
    static int oldhealth = -1;
    int health = player->health > 100 ? 100 : player->health;
    if (oldhealth != health)
    {
        lasthealthcalc =
            ST_FACESTRIDE * (((100 - health) * ST_NUMPAINFACES) / 101);
        oldhealth = health;
    }
    return lasthealthcalc;
}

static int DeadFace(player_t *player)
{
    const int state =
        (player->mo->state - states) - mobjinfo[player->mo->type].xdeathstate;

    // [FG] support face gib animations as in the 3DO/Jaguar/PSX ports
    if (have_xdthfaces && state >= 0)
    {
        return ST_XDTHFACE + MIN(state, have_xdthfaces - 1);
    }

    return ST_DEADFACE;
}

static void UpdateFace(sbe_face_t *face, player_t *player)
{
    static int priority;
    static int lastattackdown = -1;

    if (priority < 10)
    {
        // dead
        if (!player->health)
        {
            priority = 9;
            face->faceindex = DeadFace(player);
            face->facecount = 1;
        }
    }

    if (priority < 9)
    {
        if (player->bonuscount)
        {
            // picking up bonus
            boolean doevilgrin = false;

            for (int i = 0; i < NUMWEAPONS; ++i)
            {
                if (face->oldweaponsowned[i] != player->weaponowned[i])
                {
                    if (face->oldweaponsowned[i] < player->weaponowned[i])
                    {
                        doevilgrin = true;
                    }
                    face->oldweaponsowned[i] = player->weaponowned[i];
                }
            }

            if (doevilgrin)
            {
                // evil grin if just picked up weapon
                priority = 8;
                face->facecount = ST_EVILGRINCOUNT;
                face->faceindex = CalcPainOffset(face, player) + ST_EVILGRINOFFSET;
            }
        }
    }

    if (priority < 8)
    {
        if (player->damagecount && player->attacker
            && player->attacker != player->mo)
        {
            // being attacked
            priority = 7;

            angle_t diffangle = 0;
            boolean right = false;

            // [FG] show "Ouch Face" as intended
            if (face->oldhealth - player->health > ST_MUCHPAIN)
            {
                // [FG] raise "Ouch Face" priority
                priority = 8;
                face->facecount = ST_TURNCOUNT;
                face->faceindex = CalcPainOffset(face, player) + ST_OUCHOFFSET;
            }
            else
            {
                angle_t badguyangle =
                    R_PointToAngle2(player->mo->x, player->mo->y,
                                     player->attacker->x, player->attacker->y);

                if (badguyangle > player->mo->angle)
                {
                    // whether right or left
                    diffangle = badguyangle - player->mo->angle;
                    right = diffangle > ANG180;
                }
                else
                {
                    // whether left or right
                    diffangle = player->mo->angle - badguyangle;
                    right = diffangle <= ANG180;
                } // confusing, aint it?

                face->facecount = ST_TURNCOUNT;
                face->faceindex = CalcPainOffset(face, player);

                if (diffangle < ANG45)
                {
                    // head-on
                    face->faceindex += ST_RAMPAGEOFFSET;
                }
                else if (right)
                {
                    // turn face right
                    face->faceindex += ST_TURNOFFSET;
                }
                else
                {
                    // turn face left
                    face->faceindex += ST_TURNOFFSET + 1;
                }
            }
        }
    }

    if (priority < 7)
    {
        // getting hurt because of your own damn stupidity
        if (player->damagecount)
        {
            if (face->oldhealth - player->health > ST_MUCHPAIN)
            {
                priority = 7;
                face->facecount = ST_TURNCOUNT;
                face->faceindex = CalcPainOffset(face, player) + ST_OUCHOFFSET;
            }
            else
            {
                priority = 6;
                face->facecount = ST_TURNCOUNT;
                face->faceindex = CalcPainOffset(face, player) + ST_RAMPAGEOFFSET;
            }
        }
    }

    if (priority < 6)
    {
        // rapid firing
        if (player->attackdown)
        {
            if (lastattackdown == -1)
            {
                lastattackdown = ST_RAMPAGEDELAY;
            }
            else if (!--lastattackdown)
            {
                priority = 5;
                face->faceindex = CalcPainOffset(face, player) + ST_RAMPAGEOFFSET;
                face->facecount = 1;
                lastattackdown = 1;
            }
        }
        else
        {
            lastattackdown = -1;
        }
    }

    if (priority < 5)
    {
        // invulnerability
        if ((player->cheats & CF_GODMODE) || player->powers[pw_invulnerability])
        {
            priority = 4;
            face->faceindex = ST_GODFACE;
            face->facecount = 1;
        }
    }

    // look left or look right if the facecount has timed out
    if (!face->facecount)
    {
        face->faceindex = CalcPainOffset(face, player) + (M_Random() % 3);
        face->facecount = ST_STRAIGHTFACECOUNT;
        priority = 0;
    }

    --face->facecount;

    face->oldhealth = player->health;
}

static void UpdateNumber(sbarelem_t *elem, player_t *player)
{
    sbe_number_t *number = elem->subtype.number;
    numberfont_t *font = number->font;

    int value = ResolveNumber(number, player);
    int power = (value < 0 ? number->maxlength - 1 : number->maxlength);
    int max = (int)pow(10.0, power) - 1;
    int valglyphs = 0;
    int numvalues = 0;

    if (value < 0 && font->minus != NULL)
    {
        value = MAX(-max, value);
        numvalues = (int)log10(-value) + 1;
        valglyphs = numvalues + 1;
    }
    else
    {
        value = CLAMP(value, 0, max);
        numvalues = valglyphs = value != 0 ? ((int)log10(value) + 1) : 1;
    }

    if (elem->type == sbe_percent && font->percent != NULL)
    {
        ++valglyphs;
    }

    int totalwidth = font->monowidth * valglyphs;
    if (font->type == sbf_mono0)
    {
        if (elem->type == sbe_percent && font->percent != NULL)
        {
            totalwidth += SHORT(font->percent->width) - font->monowidth;
        }
    }
    else if (font->type == sbf_proportional)
    {
        totalwidth = 0;
        if (value < 0 && font->minus != NULL)
        {
            totalwidth += SHORT(font->minus->width);
        }
        int tempnum = value;
        while (tempnum > 0)
        {
            int workingnum = tempnum % 10;
            totalwidth = SHORT(font->numbers[workingnum]->width);
            tempnum /= 10;
        }
        if (elem->type == sbe_percent && font->percent != NULL)
        {
            totalwidth += SHORT(font->percent->width);
        }
    }

    number->xoffset = 0;
    if (elem->alignment & sbe_h_middle)
    {
        number->xoffset -= (totalwidth >> 1);
    }
    else if (elem->alignment & sbe_h_right)
    {
        number->xoffset -= totalwidth;
    }

    number->value = value;
    number->numvalues = numvalues;
}

static void UpdateLines(sbarelem_t *elem)
{
    sbe_widget_t *widget = elem->subtype.widget;
    hudfont_t *font = widget->font;

    stringline_t *line;
    array_foreach(line, widget->lines)
    {
        int totalwidth = 0;

        const char *str = line->string;
        while (*str)
        {
            int ch = *str++;
            if (ch == '\x1b' && *str)
            {
                ++str;
                continue;
            }

            if (font->type == sbf_proportional)
            {
                ch = M_ToUpper(ch) - HU_FONTSTART;
                if (ch < 0 || ch >= HU_FONTSIZE)
                {
                    totalwidth += SPACEWIDTH;
                    continue;
                }
                patch_t *patch = font->characters[ch];
                if (patch == NULL)
                {
                    totalwidth += SPACEWIDTH;
                    continue;
                }
                totalwidth += SHORT(patch->width);
            }
            else
            {
                totalwidth += font->monowidth;
            }
        }

        line->xoffset = 0;
        if (elem->alignment & sbe_h_middle)
        {
            line->xoffset -= (totalwidth >> 1);
        }
        else if (elem->alignment & sbe_h_right)
        {
            line->xoffset -= totalwidth;
        }
        line->totalwidth = totalwidth;
    }
}

static void UpdateAnimation(sbarelem_t *elem)
{
    sbe_animation_t *animation = elem->subtype.animation;

    if (animation->duration_left == 0)
    {
        ++animation->frame_index;
        if (animation->frame_index == array_size(animation->frames))
        {
            animation->frame_index = 0;
        }
        animation->duration_left = animation->frames[animation->frame_index].duration;
    }

    --animation->duration_left;
}

static void UpdateBoomColors(sbarelem_t *elem, player_t *player)
{
    if (!sts_colored_numbers)
    {
        elem->crboom = CR_NONE;
        return;
    }

    sbe_number_t *number = elem->subtype.number;

    boolean invul = ST_PlayerInvulnerable(player);

    crange_idx_e cr;

    switch (number->type)
    {
        case sbn_health:
            {
                int health = player->health;
                if (invul)
                    cr = CR_GRAY;
                else if (health < health_red)
                    cr = CR_RED;
                else if (health < health_yellow)
                    cr = CR_GOLD;
                else if (health <= health_green)
                    cr = CR_GREEN;
                else
                    cr = CR_BLUE2;
            }
            break;
        case sbn_armor:
            if (hud_armor_type)
            {
                if (invul)
                    cr = CR_GRAY;
                else if (!player->armortype)
                    cr = CR_RED;
                else if (player->armortype == 1)
                    cr = CR_GREEN;
                else
                    cr = CR_BLUE2;
            }
            else
            {
                int armor = player->armorpoints;
                if (invul)
                    cr = CR_GRAY;
                else if (armor < armor_red)
                    cr = CR_RED;
                else if (armor < armor_yellow)
                    cr = CR_GOLD;
                else if (armor <= armor_green)
                    cr = CR_GREEN;
                else
                    cr = CR_BLUE2;
            }
            break;
        case sbn_ammoselected:
            {
                ammotype_t type = weaponinfo[player->readyweapon].ammo;
                if (type == am_noammo)
                {
                    return;
                }

                int maxammo = player->maxammo[type];
                if (maxammo == 0)
                {
                    return;
                }

                int ammo = player->ammo[type];

                // backpack changes thresholds
                if (player->backpack)
                {
                    maxammo /= 2;
                }

                if (ammo * 100 < ammo_red * maxammo)
                    cr = CR_RED;
                else if (ammo * 100 < ammo_yellow * maxammo)
                    cr = CR_GOLD;
                else if (ammo > maxammo)
                    cr = CR_BLUE2;
                else
                    cr = CR_GREEN;
            }
            break;
        default:
            cr = CR_NONE;
            break;
    }

    elem->crboom = cr;
}

static void UpdateString(sbarelem_t *elem)
{
    sbe_string_t *string = elem->subtype.string;

    switch (string->type)
    {
        case sbstr_maptitle:
            string->line.string = G_GetLevelTitle();
            break;
        case sbstr_label:
            if (gamemapinfo && gamemapinfo->label)
            {
                string->line.string = gamemapinfo->label;
            }
            break;
        case sbstr_author:
            if (gamemapinfo && gamemapinfo->author)
            {
                string->line.string = gamemapinfo->author;
            }
            break;
        default:
            break;
    }
}

static void UpdateListOfElem(sbarelem_t *elem, player_t *player);

static void UpdateElem(sbarelem_t *elem, player_t *player)
{
    elem->enabled = CheckConditions(elem->conditions, player);
    if (!elem->enabled)
    {
        return;
    }

    switch (elem->type)
    {
        case sbe_list:
            UpdateListOfElem(elem, player);
            return;

        case sbe_face:
            UpdateFace(elem->subtype.face, player);
            break;

        case sbe_animation:
            UpdateAnimation(elem);
            break;

        case sbe_number:
        case sbe_percent:
            UpdateBoomColors(elem, player);
            UpdateNumber(elem, player);
            break;

        case sbe_widget:
            ST_UpdateWidget(elem, player);
            UpdateLines(elem);
            break;

        case sbe_carousel:
            if (weapon_carousel)
            {
                ST_UpdateCarousel(player);
            }
            break;

        case sbe_string:
            UpdateString(elem);
            break;

        default:
            break;
    }

    sbarelem_t *child;
    array_foreach(child, elem->children)
    {
        UpdateElem(child, player);
    }
}

static void UpdateStatusBar(player_t *player)
{
    static int oldbarindex = -1;

    int barindex = MAX(screenblocks - 10, 0);

    if (barindex >= array_size(sbardef->statusbars))
    {
        barindex = array_size(sbardef->statusbars) - 1;
    }

    if (automap_on)
    {
        barindex = 0;
    }

    if (oldbarindex != barindex)
    {
        st_time_elem = NULL;
        st_cmd_elem = NULL;
        st_msg_elem = NULL;
        oldbarindex = barindex;
    }

    statusbar = &sbardef->statusbars[barindex];

    sbarelem_t *child;
    array_foreach(child, statusbar->children)
    {
        UpdateElem(child, player);
    }
}

static void ResetElem(sbarelem_t *elem, player_t *player)
{
    switch (elem->type)
    {
        case sbe_graphic:
            {
                sbe_graphic_t *graphic = elem->subtype.graphic;
                graphic->patch = CachePatchName(graphic->patch_name);
            }
            break;

        case sbe_face:
            {
                sbe_face_t *face = elem->subtype.face;
                face->faceindex = 0;
                face->facecount = 0;
                face->oldhealth = -1;
                for (int i = 0; i < NUMWEAPONS; i++)
                {
                    face->oldweaponsowned[i] = player->weaponowned[i];
                }
            }
            break;

        case sbe_animation:
            {
                sbe_animation_t *animation = elem->subtype.animation;
                sbarframe_t *frame;
                array_foreach(frame, animation->frames)
                {
                    frame->patch = CachePatchName(frame->patch_name);
                }
                animation->frame_index = 0;
                animation->duration_left = 0;
            }
            break;

        case sbe_widget:
            elem->subtype.widget->duration_left = 0;
            break;

        case sbe_string:
            {
                sbe_string_t *string = elem->subtype.string;
                if (!string->line.string)
                {
                    string->line.string = "";
                }
            }
            break;

        default:
            break;
    }

    sbarelem_t *child;
    array_foreach(child, elem->children)
    {
        ResetElem(child, player);
    }
}

static void ResetStatusBar(void)
{
    player_t *player = &players[displayplayer];

    statusbar_t *local_statusbar;
    array_foreach(local_statusbar, sbardef->statusbars)
    {
        sbarelem_t *child;
        array_foreach(child, local_statusbar->children)
        {
            ResetElem(child, player);
        }
    }

    ST_ResetTitle();
}

static int AdjustX(int x, int width, sbaralignment_t alignment)
{
    if (alignment & sbe_h_middle)
    {
        x = x - width / 2;
    }
    else if (alignment & sbe_h_right)
    {
        x -= width;
    }
    return x;
}

static int WideShiftX(int x, sbaralignment_t alignment)
{
    if (alignment & sbe_wide_left)
    {
        x -= st_wide_shift;
    }
    else if (alignment & sbe_wide_right)
    {
        x += st_wide_shift;
    }
    return x;
}

static int AdjustY(int y, int height, sbaralignment_t alignment)
{
    if (alignment & sbe_v_middle)
    {
        y = y - height / 2;
    }
    else if (alignment & sbe_v_bottom)
    {
        y -= height;
    }
    return y;
}

static void DrawPatch(int x1, int y1, int *x2, int *y2, boolean dry,
                      crop_t crop, int maxheight, sbaralignment_t alignment,
                      patch_t *patch, crange_idx_e cr, const byte *tl)
{
    if (!patch)
    {
        return;
    }

    int width = crop.width ? crop.width : SHORT(patch->width);
    int height;
    if (maxheight)
    {
        height = maxheight;
    }
    else if (crop.height)
    {
        height = crop.height;
    }
    else
    {
        height = SHORT(patch->height);
    }

    int xoffset = 0, yoffset = 0;
    if (!(alignment & sbe_ignore_xoffset))
    {
        xoffset = SHORT(patch->leftoffset);
    }
    if (!(alignment & sbe_ignore_yoffset))
    {
        yoffset = SHORT(patch->topoffset);
    }

    x1 = AdjustX(x1, width, alignment);
    if (alignment & sbe_h_middle)
    {
        x1 += xoffset;
        if (crop.center)
        {
            x1 += width / 2 + crop.left;
        }
    }
    x1 = WideShiftX(x1, alignment);

    y1 = AdjustY(y1, height, alignment);
    if (alignment & sbe_v_middle)
    {
        y1 += yoffset;
    }

    if (x2)
    {
        *x2 = MAX(*x2, x1 + width);
    }
    if (y2)
    {
        *y2 = MAX(*y2, y1 + height);
    }

    if (dry)
    {
        return;
    }

    byte *outr = colrngs[cr];

    V_DrawPatchGeneral(x1, y1, xoffset, yoffset, tl, outr, patch, crop);
}

static void DrawGlyphNumber(int x1, int y1, int *x2, int *y2, boolean dry,
                            sbarelem_t *elem, patch_t *glyph)
{
    sbe_number_t *number = elem->subtype.number;
    numberfont_t *font = number->font;

    int width, widthdiff;

    if (font->type == sbf_proportional)
    {
        width = glyph ? SHORT(glyph->width) : SPACEWIDTH;
        widthdiff = 0;
    }
    else
    {
        width = font->monowidth;
        widthdiff = glyph ? SHORT(glyph->width) - width : SPACEWIDTH - width;
    }

    if (elem->alignment & sbe_h_middle)
    {
        number->xoffset += ((width + widthdiff) >> 1);
    }
    else if (elem->alignment & sbe_h_right)
    {
        number->xoffset += (width + widthdiff);
    }

    if (glyph)
    {
        DrawPatch(x1 + number->xoffset, y1, x2, y2, dry, zero_crop,
                  font->maxheight, elem->alignment, glyph,
                  elem->crboom == CR_NONE ? elem->cr : elem->crboom,
                  elem->tranmap);
    }

    if (elem->alignment & sbe_h_middle)
    {
        number->xoffset += (width - ((width - widthdiff) >> 1));
    }
    else if (elem->alignment & sbe_h_right)
    {
        number->xoffset += -widthdiff;
    }
    else
    {
        number->xoffset += width;
    }
}

static void DrawGlyphLine(int x1, int y1, int *x2, int *y2, boolean dry,
                          sbarelem_t *elem, hudfont_t *font, stringline_t *line,
                          patch_t *glyph)
{
    int width, widthdiff;

    if (font->type == sbf_proportional)
    {
        width = glyph ? SHORT(glyph->width) : SPACEWIDTH;
        widthdiff = 0;
    }
    else
    {
        width = font->monowidth;
        widthdiff = glyph ? SHORT(glyph->width) - width : 0;
    }

    if (elem->alignment & sbe_h_middle)
    {
        line->xoffset += ((width + widthdiff) >> 1);
    }
    else if (elem->alignment & sbe_h_right)
    {
        line->xoffset += (width + widthdiff);
    }

    if (glyph)
    {
        DrawPatch(x1 + line->xoffset, y1, x2, y2, dry, zero_crop,
                  font->maxheight, elem->alignment, glyph, elem->cr,
                  elem->tranmap);
    }

    if (elem->alignment & sbe_h_middle)
    {
        line->xoffset += (width - ((width - widthdiff) >> 1));
    }
    else if (elem->alignment & sbe_h_right)
    {
        line->xoffset += -widthdiff;
    }
    else
    {
        line->xoffset += width;
    }
}

static void DrawNumber(int x1, int y1, int *x2, int *y2, boolean dry,
                       sbarelem_t *elem)
{
    sbe_number_t *number = elem->subtype.number;

    int value = number->value;
    int base_xoffset = number->xoffset;
    numberfont_t *font = number->font;

    if (value < 0 && font->minus != NULL)
    {
        DrawGlyphNumber(x1, y1, x2, y2, dry, elem, font->minus);
        value = -value;
    }

    int glyphindex = number->numvalues;
    while (glyphindex > 0)
    {
        int glyphbase = (int)pow(10.0, --glyphindex);
        int workingnum = value / glyphbase;
        DrawGlyphNumber(x1, y1, x2, y2, dry, elem, font->numbers[workingnum]);
        value -= (workingnum * glyphbase);
    }

    if (elem->type == sbe_percent && font->percent != NULL)
    {
        crange_idx_e oldcr = elem->crboom;
        if (sts_pct_always_gray)
        {
            elem->crboom = CR_GRAY;
        }
        DrawGlyphNumber(x1, y1, x2, y2, dry, elem, font->percent);
        elem->crboom = oldcr;
    }

    number->xoffset = base_xoffset;
}

static void DrawStringLine(int x1, int y1, int *x2, int *y2, boolean dry,
                           stringline_t *line, sbarelem_t *elem,
                           hudfont_t *font)
{
    int base_xoffset = line->xoffset;

    int cr = elem->cr;

    const char *str = line->string;
    while (*str)
    {
        int ch = *str++;

        if (ch == '\x1b' && *str)
        {
            ch = *str++;
            if (ch >= '0' && ch <= '0' + CR_NONE)
            {
                elem->cr = ch - '0';
            }
            else if (ch == '0' + CR_ORIG)
            {
                elem->cr = cr;
            }
            continue;
        }

        ch = M_ToUpper(ch) - HU_FONTSTART;

        patch_t *glyph;
        if (ch < 0 || ch >= HU_FONTSIZE)
        {
            glyph = NULL;
        }
        else
        {
            glyph = font->characters[ch];
        }
        DrawGlyphLine(x1, y1, x2, y2, dry, elem, font, line, glyph);
    }

    line->xoffset = base_xoffset;
}

static void DrawWidget(int x1, int y1, int *x2, int *y2, boolean dry,
                       sbarelem_t *elem)
{
    sbe_widget_t *widget = elem->subtype.widget;
    hudfont_t *font = widget->font;

    stringline_t *line;
    array_foreach(line, widget->lines)
    {
        DrawStringLine(x1, y1, x2, y2, dry, line, elem, font);
        if (elem->alignment & sbe_v_bottom)
        {
            y1 -= font->maxheight;
        }
        else
        {
            y1 += font->maxheight;
        }
    }
}

static void DrawListOfElem(int x1, int y1, int *x2, int *y2, boolean dry,
                           sbarelem_t *elem);

static void DrawElem(int x1, int y1, int *x2, int *y2, boolean dry,
                     sbarelem_t *elem)
{
    if (!elem->enabled)
    {
        return;
    }

    x1 += elem->x_pos;
    y1 += elem->y_pos;

    switch (elem->type)
    {
        case sbe_list:
            DrawListOfElem(x1, y1, x2, y2, dry, elem);
            return;

        case sbe_graphic:
            {
                sbe_graphic_t *graphic = elem->subtype.graphic;
                DrawPatch(x1, y1, x2, y2, dry, graphic->crop, 0, elem->alignment,
                          graphic->patch, elem->cr, elem->tranmap);
            }
            break;

        case sbe_facebackground:
            {
                sbe_facebackground_t *facebackground = elem->subtype.facebackground;
                DrawPatch(x1, y1, x2, y2, dry, facebackground->crop, 0,
                          elem->alignment, facebackpatches[displayplayer],
                          elem->cr, elem->tranmap);
            }
            break;

        case sbe_face:
            {
                sbe_face_t *face = elem->subtype.face;
                DrawPatch(x1, y1, x2, y2, dry, face->crop, 0, elem->alignment,
                          facepatches[face->faceindex], elem->cr,
                          elem->tranmap);
            }
            break;

        case sbe_animation:
            {
                sbe_animation_t *animation = elem->subtype.animation;
                patch_t *patch =
                    animation->frames[animation->frame_index].patch;
                DrawPatch(x1, y1, x2, y2, dry, zero_crop, 0, elem->alignment,
                          patch, elem->cr, elem->tranmap);
            }
            break;

        case sbe_number:
        case sbe_percent:
            DrawNumber(x1, y1, x2, y2, dry, elem);
            break;

        case sbe_widget:
            if (elem == st_cmd_elem)
            {
                st_cmd_x = x1;
                st_cmd_y = y1;
            }
            if (message_centered && elem == st_msg_elem)
            {
                break;
            }
            DrawWidget(x1, y1, x2, y2, dry, elem);
            break;

        case sbe_carousel:
            if (weapon_carousel)
            {
                ST_DrawCarousel(x1, y1, elem);
            }
            break;

        case sbe_string:
            {
                sbe_string_t *string = elem->subtype.string;
                DrawStringLine(x1, y1, x2, y2, dry, &string->line, elem,
                               string->font);
            }
            break;

        default:
            break;
    }

    sbarelem_t *child;
    array_foreach(child, elem->children)
    {
        DrawElem(x1, y1, x2, y2, dry, child);
    }
}

static void UpdateListOfElem(sbarelem_t *elem, player_t *player)
{
    array_foreach_type(child, elem->children, sbarelem_t)
    {
        UpdateElem(child, player);

        int width = 0, height = 0;
        DrawElem(0, 0, &width, &height, true, child); // Dry run
        child->width = width;
        child->height = height;
    }
}

static void DrawListOfElem(int x1, int y1, int *x2, int *y2, boolean dry,
                           sbarelem_t *elem)
{
    sbe_list_t *list = elem->subtype.list;

    int listwidth = 0, listheight = 0;

    array_foreach_type(child, elem->children, sbarelem_t)
    {
        if (dry)
        {
            int width = 0, height = 0;
            DrawElem(0, 0, &width, &height, true, child);
            child->width = width;
            child->height = height;
        }

        if (list->horizontal && child->width)
        {
            listwidth += (child->width + list->spacing);
        }
        else if (child->height)
        {
            listheight += (child->height + list->spacing);
        }
    }

    if (list->horizontal)
    {
        x1 = AdjustX(x1, listwidth, elem->alignment);
    }
    else
    {
        y1 = AdjustY(y1, listheight, elem->alignment);
    }

    x1 = WideShiftX(x1, elem->alignment);

    array_foreach_type(child, elem->children, sbarelem_t)
    {
        int x1adj = x1, y1adj = y1;

        if (list->horizontal)
        {
            y1adj = AdjustY(y1, child->height, elem->alignment);
        }
        else
        {
            x1adj = AdjustX(x1, child->width, elem->alignment);
        }

        DrawElem(x1adj, y1adj, x2, y2, dry, child);

        if (list->horizontal && child->width)
        {
            x1 += (child->width + list->spacing);
        }
        else if (child->height)
        {
            y1 += (child->height + list->spacing);
        }
    }
}

static boolean st_solidbackground;

static void DrawSolidBackground(void)
{
    // [FG] calculate average color of the 16px left and right of the status bar
    const int vstep[][2] = { {0, 1}, {1, 2}, {2, st_height} };

    patch_t *sbar = V_CachePatchName(W_CheckWidescreenPatch("STBAR"), PU_CACHE);
    // [FG] temporarily draw status bar to background buffer
    crop_t crop = {.width = SHORT(sbar->width), .height = st_height};
    V_DrawPatchCropped(-video.deltaw, 0, sbar, crop);

    byte *pal = W_CacheLumpName("PLAYPAL", PU_CACHE);

    const int width = MIN(SHORT(sbar->width), video.unscaledw);
    const int depth = 16;
    int v;

    // [FG] separate colors for the top rows
    for (v = 0; v < arrlen(vstep); v++)
    {
        int x, y;
        const int v0 = vstep[v][0], v1 = vstep[v][1];
        unsigned r = 0, g = 0, b = 0;
        byte col;

        for (y = v0; y < v1; y++)
        {
            int line = V_ScaleY(y) * video.width;
            for (x = 0; x < depth; x++)
            {
                pixel_t *c = st_backing_screen + line + V_ScaleX(x);
                r += pal[3 * c[0] + 0];
                g += pal[3 * c[0] + 1];
                b += pal[3 * c[0] + 2];

                c += V_ScaleX(width - 2 * x - 1);
                r += pal[3 * c[0] + 0];
                g += pal[3 * c[0] + 1];
                b += pal[3 * c[0] + 2];
            }
        }

        r /= 2 * depth * (v1 - v0);
        g /= 2 * depth * (v1 - v0);
        b /= 2 * depth * (v1 - v0);

        // [FG] tune down to half saturation (for empiric reasons)
        col = I_GetNearestColor(pal, r / 2, g / 2, b / 2);

        V_FillRect(0, v0, video.unscaledw, v1 - v0, col);
    }
}

boolean st_refresh_background = true;

static void DrawBackground(const char *name)
{
    if (st_refresh_background)
    {
        static int old_st_height;

        if (old_st_height < st_height)
        {
            old_st_height = st_height;
            if (st_backing_screen)
            {
                Z_Free(st_backing_screen);
            }
            ST_InitRes();
        }

        V_UseBuffer(st_backing_screen);

        if (st_solidbackground && st_height > 3)
        {
            DrawSolidBackground();
        }
        else
        {
            if (!name)
            {
                name = (gamemode == commercial) ? "GRNROCK" : "FLOOR7_2";
            }

            byte *flat =
                V_CacheFlatNum(firstflat + R_FlatNumForName(name), PU_CACHE);

            V_TileBlock64(ST_Y, video.unscaledw, st_height, flat);

            if (screenblocks == 10
                || (!statusbar->fullscreenrender && screenblocks > 10)
                || automap_on)
            {
                patch_t *patch = V_CachePatchName("brdr_b", PU_CACHE);
                crop_t crop = {.width = SHORT(patch->width), .height = st_height};
                for (int x = 0; x < video.unscaledw; x += 8)
                {
                    V_DrawPatchCropped(x - video.deltaw, 0, patch, crop);
                }
            }
        }

        V_RestoreBuffer();

        st_refresh_background = false;
    }

    V_CopyRect(0, 0, st_backing_screen, video.unscaledw, st_height, 0, ST_Y);
}

static void DrawCenteredMessage(void)
{
    if (message_centered && st_msg_elem)
    {
        DrawWidget(SCREENWIDTH / 2, 0, NULL, NULL, false, st_msg_elem);
    }
}

static void DrawStatusBar(void)
{
    if (!statusbar->fullscreenrender)
    {
        st_height = CLAMP(statusbar->height, 0, SCREENHEIGHT) & ~1;
    }
    else
    {
        st_height = 0;
    }

    if (st_height && (screenblocks <= 10 || automap_on))
    {
        DrawBackground(statusbar->fillflat);
    }

    sbarelem_t *child;
    int y1 = statusbar->fullscreenrender ? 0 : SCREENHEIGHT - statusbar->height;
    array_foreach(child, statusbar->children)
    {
        DrawElem(0, y1, NULL, NULL, false, child);
    }

    DrawCenteredMessage();
}

void ST_Erase(void)
{
    if (!sbardef || screenblocks >= 10)
    {
        return;
    }

    R_DrawViewBorder();
}

// Respond to keyboard input events,
//  intercept cheats.
boolean ST_Responder(event_t *ev)
{
    // Filter automap on/off.
    if (ev->type == ev_keyup && (ev->data1.i & 0xffff0000) == AM_MSGHEADER)
    {
        return false;
    }
    else if (ST_MessagesResponder(ev))
    {
        return true;
    }
    else // if a user keypress...
    {
        return M_CheatResponder(ev); // Try cheat responder in m_cheat.c
    }
}

pal_change_t palette_changes = PAL_CHANGE_ON;

static void DoPaletteStuff(player_t *player)
{
    static int oldpalette = 0;
    int palette;

    int damagecount = player->damagecount;

    // killough 7/14/98: beta version did not cause red berserk palette
    if (!beta_emulation)
    {

        if (player->powers[pw_strength])
        {
            // slowly fade the berzerk out
            int berzerkcount = 12 - (player->powers[pw_strength] >> 6);
            if (berzerkcount > damagecount)
            {
                damagecount = berzerkcount;
            }
        }
    }

    if (STRICTMODE(palette_changes == PAL_CHANGE_OFF))
    {
        palette = 0;
    }
    else if (damagecount)
    {
        // In Chex Quest, the player never sees red. Instead, the radiation suit
        // palette is used to tint the screen green, as though the player is
        // being covered in goo by an attacking flemoid.
        if (gameversion == exe_chex)
        {
            palette = RADIATIONPAL;
        }
        else
        {
            palette = (damagecount + 7) >> 3;
            if (palette >= NUMREDPALS)
            {
                palette = NUMREDPALS - 1;
            }
            // tune down a bit so the menu remains legible
            if (menuactive || paused || STRICTMODE(palette_changes == PAL_CHANGE_REDUCED))
            {
                palette /= 2;
            }
            palette += STARTREDPALS;
        }
    }
    else if (player->bonuscount)
    {
        palette = (player->bonuscount + 7) >> 3;
        if (palette >= NUMBONUSPALS)
        {
            palette = NUMBONUSPALS - 1;
        }
        if (STRICTMODE(palette_changes == PAL_CHANGE_REDUCED))
        {
            palette /= 2;
        }
        palette += STARTBONUSPALS;
    }
    // killough 7/14/98: beta version did not cause green palette
    else if (beta_emulation)
    {
        palette = 0;
    }
    else if (player->powers[pw_ironfeet] > 4 * 32
             || player->powers[pw_ironfeet] & 8)
    {
        palette = RADIATIONPAL;
    }
    else
    {
        palette = 0;
    }

    if (palette != oldpalette)
    {
        oldpalette = palette;
        // haleyjd: must cast to byte *, arith. on void pointer is
        // a GNU C extension
        I_SetPalette((byte *)W_CacheLumpName("PLAYPAL", PU_CACHE)
                     + palette * 768);
    }
}

void ST_Ticker(void)
{
    if (!sbardef)
    {
        return;
    }

    // check for incoming chat characters
    if (netgame)
    {
        ST_UpdateChatMessage();
    }

    player_t *player = &players[displayplayer];

    UpdateStatusBar(player);

    if (hud_crosshair)
    {
        HU_UpdateCrosshair();
    }

    if (!nodrawers)
    {
        DoPaletteStuff(player); // Do red-/gold-shifts from damage/items
    }
}

void ST_Drawer(void)
{
    if (!sbardef)
    {
        return;
    }

    DrawStatusBar();
}

void ST_Start(void)
{
    if (!sbardef)
    {
        return;
    }

    ResetStatusBar();
    ST_ResetCarousel();

    HU_StartCrosshair();
}

hudfont_t *stcfnt;
patch_t **hu_font = NULL;

void ST_Init(void)
{
    sbardef = ST_ParseSbarDef();

    stcfnt = LoadSTCFN();
    hu_font = stcfnt->characters;

    if (!sbardef)
    {
        return;
    }

    if (array_size(sbardef->statusbars))
    {
        statusbar_t *sb = &sbardef->statusbars[0];
        if (!sb->fullscreenrender)
        {
            st_height_screenblocks10 = CLAMP(sb->height, 0, SCREENHEIGHT) & ~1;
            st_height = st_height_screenblocks10;
        }
    }

    LoadFacePatches();

    HU_InitCrosshair();
    HU_InitCommandHistory();
    HU_InitObituaries();

    ST_InitWidgets();
}

void ST_InitRes(void)
{
    if (!st_height)
    {
        return;
    }
    // killough 11/98: allocate enough for hires
    st_backing_screen =
        Z_Malloc(video.width * V_ScaleY(st_height) * sizeof(*st_backing_screen),
                 PU_RENDERER, 0);
}

const char **ST_StatusbarList(void)
{
    if (!sbardef)
    {
        return NULL;
    }

    static const char **strings;

    if (array_size(strings))
    {
        return strings;
    }

    statusbar_t *item;
    array_foreach(item, sbardef->statusbars)
    {
        if (item->fullscreenrender)
        {
            array_push(strings, "Fullscreen");
        }
        else
        {
            array_push(strings, "Status Bar");
        }
    }
    return strings;
}

void ST_ResetPalette(void)
{
    I_SetPalette(W_CacheLumpName("PLAYPAL", PU_CACHE));
}

// [FG] draw Time widget on intermission screen

void WI_UpdateWidgets(void)
{
    if (st_cmd_elem && STRICTMODE(hud_command_history))
    {
        ST_UpdateWidget(st_cmd_elem, &players[displayplayer]);
        UpdateLines(st_cmd_elem);
    }
}

void WI_DrawWidgets(void)
{
    if (st_time_elem && hud_level_time & HUD_WIDGET_HUD)
    {
        sbarelem_t time = *st_time_elem;
        time.alignment = sbe_wide_left;
        // leveltime is already added to totalleveltimes before WI_Start()
        DrawWidget(0, 0, NULL, NULL, false, &time);
    }

    if (st_cmd_elem && STRICTMODE(hud_command_history))
    {
        DrawWidget(st_cmd_x, st_cmd_y, NULL, NULL, false, st_cmd_elem);
    }
}

void ST_BindSTSVariables(void)
{
  M_BindNum("hud_anchoring", &hud_anchoring, NULL, HUD_ANCHORING_16_9,
            HUD_ANCHORING_WIDE, HUD_ANCHORING_21_9, ss_stat, wad_no,
            "HUD anchoring (0 = Wide; 1 = 4:3; 2 = 16:9; 3 = 21:9)");
  M_BindBool("sts_colored_numbers", &sts_colored_numbers, NULL,
             false, ss_stat, wad_yes, "Colored numbers on the status bar");
  M_BindBool("sts_pct_always_gray", &sts_pct_always_gray, NULL,
             false, ss_none, wad_yes,
             "Percent signs on the status bar are always gray");
  M_BindBool("st_solidbackground", &st_solidbackground, NULL,
             false, ss_stat, wad_no,
             "Use solid-color borders for the status bar in widescreen mode");
  M_BindBool("hud_armor_type", &hud_armor_type, NULL, true, ss_none, wad_no,
             "Armor count is colored based on armor type");
  M_BindNum("health_red", &health_red, NULL, 25, 0, 200, ss_none, wad_yes,
            "Amount of health for red-to-yellow transition");
  M_BindNum("health_yellow", &health_yellow, NULL, 50, 0, 200, ss_none, wad_yes,
            "Amount of health for yellow-to-green transition");
  M_BindNum("health_green", &health_green, NULL, 100, 0, 200, ss_none, wad_yes,
            "Amount of health for green-to-blue transition");
  M_BindNum("armor_red", &armor_red, NULL, 25, 0, 200, ss_none, wad_yes,
            "Amount of armor for red-to-yellow transition");
  M_BindNum("armor_yellow", &armor_yellow, NULL, 50, 0, 200, ss_none, wad_yes,
            "Amount of armor for yellow-to-green transition");
  M_BindNum("armor_green", &armor_green, NULL, 100, 0, 200, ss_none, wad_yes,
            "Amount of armor for green-to-blue transition");
  M_BindNum("ammo_red", &ammo_red, NULL, 25, 0, 100, ss_none, wad_yes,
            "Percent of ammo for red-to-yellow transition");
  M_BindNum("ammo_yellow", &ammo_yellow, NULL, 50, 0, 100, ss_none, wad_yes,
            "Percent of ammo for yellow-to-green transition");

  M_BindNum("hud_crosshair", &hud_crosshair, NULL, 0, 0, 10 - 1, ss_stat, wad_no,
            "Crosshair");
  M_BindBool("hud_crosshair_health", &hud_crosshair_health, NULL,
             false, ss_stat, wad_no, "Change crosshair color based on player health");
  M_BindNum("hud_crosshair_target", &hud_crosshair_target, NULL,
            0, 0, 2, ss_stat, wad_no,
            "Change crosshair color when locking on target (1 = Highlight; 2 = Health)");
  M_BindBool("hud_crosshair_lockon", &hud_crosshair_lockon, NULL,
             false, ss_stat, wad_no, "Lock crosshair on target");
  M_BindNum("hud_crosshair_color", &hud_crosshair_color, NULL,
            CR_GRAY, CR_BRICK, CR_NONE, ss_stat, wad_no,
            "Default crosshair color");
  M_BindNum("hud_crosshair_target_color", &hud_crosshair_target_color, NULL,
            CR_YELLOW, CR_BRICK, CR_NONE, ss_stat, wad_no,
            "Crosshair color when aiming at target");

  M_BindBool("weapon_carousel", &weapon_carousel, NULL,
             true, ss_weap, wad_no, "Show weapon carousel");
}

//----------------------------------------------------------------------------
//
// $Log: st_stuff.c,v $
// Revision 1.46  1998/05/06  16:05:40  jim
// formatting and documenting
//
// Revision 1.45  1998/05/03  22:50:58  killough
// beautification, move external declarations, remove cheats
//
// Revision 1.44  1998/04/27  17:30:39  jim
// Fix DM demo/newgame status, remove IDK (again)
//
// Revision 1.43  1998/04/27  02:30:12  killough
// fuck you
//
// Revision 1.42  1998/04/24  23:52:31  thldrmn
// Removed idk cheat
//
// Revision 1.41  1998/04/24  11:39:23  killough
// Fix cheats while demo is played back
//
// Revision 1.40  1998/04/19  01:10:19  killough
// Generalize cheat engine to add deh support
//
// Revision 1.39  1998/04/16  06:26:06  killough
// Prevent cheats from working inside menu
//
// Revision 1.38  1998/04/12  10:58:24  jim
// IDMUSxy for DOOM 1 fix
//
// Revision 1.37  1998/04/12  10:23:52  jim
// IDMUS00 ok in DOOM 1
//
// Revision 1.36  1998/04/12  02:00:39  killough
// Change tranmap to main_tranmap
//
// Revision 1.35  1998/04/12  01:08:51  jim
// Fixed IDMUS00 crash
//
// Revision 1.34  1998/04/11  14:48:11  thldrmn
// Replaced IDK with TNTKA cheat
//
// Revision 1.33  1998/04/10  06:36:45  killough
// Fix -fast parameter bugs
//
// Revision 1.32  1998/03/31  10:37:17  killough
// comment clarification
//
// Revision 1.31  1998/03/28  18:09:19  killough
// Fix deh-cheat self-annihilation bug, make iddt closer to Doom
//
// Revision 1.30  1998/03/28  05:33:02  jim
// Text enabling changes for DEH
//
// Revision 1.29  1998/03/23  15:24:54  phares
// Changed pushers to linedef control
//
// Revision 1.28  1998/03/23  06:43:26  jim
// linedefs reference initial version
//
// Revision 1.27  1998/03/23  03:40:46  killough
// Fix idclip bug, make monster kills message smart
//
// Revision 1.26  1998/03/20  00:30:37  phares
// Changed friction to linedef control
//
// Revision 1.25  1998/03/17  20:44:32  jim
// fixed idmus non-restore, space bug
//
// Revision 1.24  1998/03/12  14:35:01  phares
// New cheat codes
//
// Revision 1.23  1998/03/10  07:14:38  jim
// Initial DEH support added, minus text
//
// Revision 1.22  1998/03/09  07:31:48  killough
// Fix spy mode to display player correctly, add TNTFAST
//
// Revision 1.21  1998/03/06  05:31:02  killough
// PEst control, from the TNT'EM man
//
// Revision 1.20  1998/03/02  15:35:03  jim
// Enabled Lee's status changes, added new types to common.cfg
//
// Revision 1.19  1998/03/02  12:09:18  killough
// blue status bar color, monsters_remember, traditional_keys
//
// Revision 1.18  1998/02/27  11:00:58  phares
// Can't own weapons that don't exist
//
// Revision 1.17  1998/02/26  22:57:45  jim
// Added message review display to HUD
//
// Revision 1.16  1998/02/24  08:46:45  phares
// Pushers, recoil, new friction, and over/under work
//
// Revision 1.15  1998/02/24  04:14:19  jim
// Added double keys to status
//
// Revision 1.14  1998/02/23  04:57:29  killough
// Fix TNTEM cheat again, add new cheats
//
// Revision 1.13  1998/02/20  21:57:07  phares
// Preliminarey sprite translucency
//
// Revision 1.12  1998/02/19  23:15:52  killough
// Add TNTAMMO in addition to TNTAMO
//
// Revision 1.11  1998/02/19  16:55:22  jim
// Optimized HUD and made more configurable
//
// Revision 1.10  1998/02/18  00:59:20  jim
// Addition of HUD
//
// Revision 1.9  1998/02/17  06:15:48  killough
// Add TNTKEYxx, TNTAMOx, TNTWEAPx cheats, and cheat engine support for them.
//
// Revision 1.8  1998/02/15  02:48:01  phares
// User-defined keys
//
// Revision 1.7  1998/02/09  03:19:04  killough
// Rewrite cheat code engine, add IDK and TNTHOM
//
// Revision 1.6  1998/02/02  22:19:01  jim
// Added TNTEM cheat to kill every monster alive
//
// Revision 1.5  1998/01/30  18:48:10  phares
// Changed textspeed and textwait to functions
//
// Revision 1.4  1998/01/30  16:09:03  phares
// Faster end-mission text display
//
// Revision 1.3  1998/01/28  12:23:05  phares
// TNTCOMP cheat code added
//
// Revision 1.2  1998/01/26  19:24:58  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:03  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------

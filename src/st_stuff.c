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
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
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
#include "v_fmt.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

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
                    switch ((sbarwidgettype_t)cond->param)
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
                    switch ((sbarwidgettype_t)cond->param)
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
                if (maxhealth)
                {
                    result &=
                        ((player->health * 100 / maxhealth) >= cond->param);
                }
                break;

            case sbc_healthlesspct:
                if (maxhealth)
                {
                    result &=
                        ((player->health * 100 / maxhealth) < cond->param);
                }
                break;

            case sbc_armorgreaterequal:
                result &= (player->armorpoints >= cond->param);
                break;

            case sbc_armorless:
                result &= (player->armorpoints < cond->param);
                break;

            case sbc_armorgreaterequalpct:
                if (max_armor)
                {
                    result &= ((player->armorpoints * 100 / max_armor)
                               >= cond->param);
                }
                break;

            case sbc_armorlesspct:
                if (max_armor)
                {
                    result &=
                        ((player->armorpoints * 100 / max_armor) < cond->param);
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
                result &=
                    ((cond->param == 1 && video.unscaledw > SCREENWIDTH)
                     || (cond->param == 0 && video.unscaledw == SCREENWIDTH));
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
    if (font->type == sbf_proportional)
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

    widgetline_t *line;
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

static void UpdateElem(sbarelem_t *elem, player_t *player)
{
    if (!CheckConditions(elem->conditions, player))
    {
        return;
    }

    switch (elem->type)
    {
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

    if (automapactive && automapoverlay == AM_OVERLAY_OFF)
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

static void DrawPatch(int x, int y, crop_t crop, int maxheight,
                      sbaralignment_t alignment, patch_t *patch,
                      crange_idx_e cr, byte *tl)
{
    if (!patch)
    {
        return;
    }

    int width = SHORT(patch->width);
    int height = maxheight ? maxheight : SHORT(patch->height);

    if (alignment & sbe_h_middle)
    {
        x = x - width / 2 + SHORT(patch->leftoffset);
        if (crop.midoffset)
        {
            x += width / 2 + crop.midoffset;
        }
    }
    else if (alignment & sbe_h_right)
    {
        x -= width;
    }

    if (alignment & sbe_v_middle)
    {
        y = y - height / 2 + SHORT(patch->topoffset);
    }
    else if (alignment & sbe_v_bottom)
    {
        y -= height;
    }

    if (alignment & sbe_wide_left)
    {
        x -= st_wide_shift;
    }
    if (alignment & sbe_wide_right)
    {
        x += st_wide_shift;
    }

    byte *outr = colrngs[cr];

    if (outr && tl)
    {
        V_DrawPatchTRTL(x, y, crop, patch, outr, tl);
    }
    else if (tl)
    {
        V_DrawPatchTL(x, y, crop, patch, tl);
    }
    else
    {
        V_DrawPatchTR(x, y, crop, patch, outr);
    }
}

static void DrawGlyphNumber(int x, int y, sbarelem_t *elem, patch_t *glyph)
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
        DrawPatch(x + number->xoffset, y, (crop_t){0}, font->maxheight,
                  elem->alignment, glyph,
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

static void DrawGlyphLine(int x, int y, sbarelem_t *elem, widgetline_t *line,
                          patch_t *glyph)
{
    sbe_widget_t *widget = elem->subtype.widget;
    hudfont_t *font = widget->font;

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
        DrawPatch(x + line->xoffset, y, (crop_t){0}, font->maxheight,
                  elem->alignment, glyph, elem->cr, elem->tranmap);
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

static void DrawNumber(int x, int y, sbarelem_t *elem)
{
    sbe_number_t *number = elem->subtype.number;

    int value = number->value;
    int base_xoffset = number->xoffset;
    numberfont_t *font = number->font;

    if (value < 0 && font->minus != NULL)
    {
        DrawGlyphNumber(x, y, elem, font->minus);
        value = -value;
    }

    int glyphindex = number->numvalues;
    while (glyphindex > 0)
    {
        int glyphbase = (int)pow(10.0, --glyphindex);
        int workingnum = value / glyphbase;
        DrawGlyphNumber(x, y, elem, font->numbers[workingnum]);
        value -= (workingnum * glyphbase);
    }

    if (elem->type == sbe_percent && font->percent != NULL)
    {
        crange_idx_e oldcr = elem->crboom;
        if (sts_pct_always_gray)
        {
            elem->crboom = CR_GRAY;
        }
        DrawGlyphNumber(x, y, elem, font->percent);
        elem->crboom = oldcr;
    }

    number->xoffset = base_xoffset;
}

static void DrawLines(int x, int y, sbarelem_t *elem)
{
    sbe_widget_t *widget = elem->subtype.widget;

    int cr = elem->cr;

    widgetline_t *line;
    array_foreach(line, widget->lines)
    {
        int base_xoffset = line->xoffset;
        hudfont_t *font = widget->font;

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
            DrawGlyphLine(x, y, elem, line, glyph);
        }

        if (elem->alignment & sbe_v_bottom)
        {
            y -= font->maxheight;
        }
        else
        {
            y += font->maxheight;
        }

        line->xoffset = base_xoffset;
    }
}

static void DrawElem(int x, int y, sbarelem_t *elem, player_t *player)
{
    if (!CheckConditions(elem->conditions, player))
    {
        return;
    }

    x += elem->x_pos;
    y += elem->y_pos;

    switch (elem->type)
    {
        case sbe_graphic:
            {
                sbe_graphic_t *graphic = elem->subtype.graphic;
                DrawPatch(x, y, graphic->crop, 0, elem->alignment,
                          graphic->patch, elem->cr, elem->tranmap);
            }
            break;

        case sbe_facebackground:
            {
                sbe_facebackground_t *facebackground = elem->subtype.facebackground;
                DrawPatch(x, y, facebackground->crop, 0, elem->alignment,
                          facebackpatches[displayplayer], elem->cr,
                          elem->tranmap);
            }
            break;

        case sbe_face:
            {
                sbe_face_t *face = elem->subtype.face;
                DrawPatch(x, y, face->crop, 0, elem->alignment,
                          facepatches[face->faceindex], elem->cr,
                          elem->tranmap);
            }
            break;

        case sbe_animation:
            {
                sbe_animation_t *animation = elem->subtype.animation;
                patch_t *patch =
                    animation->frames[animation->frame_index].patch;
                DrawPatch(x, y, (crop_t){0}, 0, elem->alignment, patch,
                          elem->cr, elem->tranmap);
            }
            break;

        case sbe_number:
        case sbe_percent:
            DrawNumber(x, y, elem);
            break;

        case sbe_widget:
            if (elem == st_cmd_elem)
            {
                st_cmd_x = x;
                st_cmd_y = y;
            }
            if (message_centered && elem == st_msg_elem)
            {
                break;
            }
            DrawLines(x, y, elem);
            break;

        case sbe_carousel:
            if (weapon_carousel)
            {
                ST_DrawCarousel(x, y, elem);
            }
            break;

        default:
            break;
    }

    sbarelem_t *child;
    array_foreach(child, elem->children)
    {
        DrawElem(x, y, child, player);
    }
}

static boolean st_solidbackground;

static void DrawSolidBackground(void)
{
    // [FG] calculate average color of the 16px left and right of the status bar
    const int vstep[][2] = { {0, 1}, {1, 2}, {2, ST_HEIGHT} };

    patch_t *sbar = V_CachePatchName(W_CheckWidescreenPatch("STBAR"), PU_CACHE);
    // [FG] temporarily draw status bar to background buffer
    V_DrawPatch(-video.deltaw, 0, sbar);

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
            for (x = 0; x < depth; x++)
            {
                pixel_t *c = st_backing_screen + V_ScaleY(y) * video.pitch
                          + V_ScaleX(x);
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
        V_UseBuffer(st_backing_screen);

        if (st_solidbackground)
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

            V_TileBlock64(ST_Y, video.unscaledw, ST_HEIGHT, flat);

            if (screenblocks == 10)
            {
                patch_t *patch = V_CachePatchName("brdr_b", PU_CACHE);
                for (int x = 0; x < video.unscaledw; x += 8)
                {
                    V_DrawPatch(x - video.deltaw, 0, patch);
                }
            }
        }

        V_RestoreBuffer();

        st_refresh_background = false;
    }

    V_CopyRect(0, 0, st_backing_screen, video.unscaledw, ST_HEIGHT, 0, ST_Y);
}

static void DrawCenteredMessage(void)
{
    if (message_centered && st_msg_elem)
    {
        DrawLines(SCREENWIDTH / 2, 0, st_msg_elem);
    }
}

static void DrawStatusBar(void)
{
    player_t *player = &players[displayplayer];

    if (!statusbar->fullscreenrender)
    {
        DrawBackground(statusbar->fillflat);
    }

    sbarelem_t *child;
    array_foreach(child, statusbar->children)
    {
        DrawElem(0, SCREENHEIGHT - statusbar->height, child, player);
    }

    DrawCenteredMessage();
}

static void EraseBackground(int y, int height)
{
    if (y > scaledviewy && y < scaledviewy + scaledviewheight - height)
    {
        R_VideoErase(0, y, scaledviewx, height);
        R_VideoErase(scaledviewx + scaledviewwidth, y, scaledviewx, height);
    }
    else
    {
        R_VideoErase(0, y, video.unscaledw, height);
    }
}

static void EraseElem(int x, int y, sbarelem_t *elem, player_t *player)
{
    if (!CheckConditions(elem->conditions, player))
    {
        return;
    }

    x += elem->x_pos;
    y += elem->y_pos;

    if (elem->type == sbe_widget)
    {
        sbe_widget_t *widget = elem->subtype.widget;
        hudfont_t *font = widget->font;

        int height = 0;
        widgetline_t *line;
        array_foreach(line, widget->lines)
        {
            if (elem->alignment & sbe_v_bottom)
            {
                y -= font->maxheight;
            }
            height += font->maxheight;
        }

        if (height > 0)
        {
            EraseBackground(y, height);
            widget->height = height;
        }
        else if (widget->height)
        {
            EraseBackground(y, widget->height);
            widget->height = 0;
        }
    }
    else if (elem->type == sbe_carousel)
    {
        ST_EraseCarousel(y);
    }

    sbarelem_t *child;
    array_foreach(child, elem->children)
    {
        EraseElem(x, y, child, player);
    }
}

void ST_Erase(void)
{
    if (!sbardef || screenblocks >= 10)
    {
        return;
    }

    player_t *player = &players[displayplayer];

    sbarelem_t *child;
    array_foreach(child, statusbar->children)
    {
        EraseElem(0, SCREENHEIGHT - statusbar->height, child, player);
    }
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

boolean palette_changes = true;

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

    if (STRICTMODE(!palette_changes))
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
            // [crispy] tune down a bit so the menu remains legible
            if (menuactive || paused)
            {
                palette >>= 1;
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

    LoadFacePatches();

    HU_InitCrosshair();
    HU_InitCommandHistory();
    HU_InitObituaries();

    ST_InitWidgets();
}

void ST_InitRes(void)
{
    // killough 11/98: allocate enough for hires
    st_backing_screen =
        Z_Malloc(video.pitch * V_ScaleY(ST_HEIGHT) * sizeof(*st_backing_screen),
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
        DrawLines(0, 0, &time);
    }

    if (st_cmd_elem && STRICTMODE(hud_command_history))
    {
        DrawLines(st_cmd_x, st_cmd_y, st_cmd_elem);
    }
}

void ST_BindSTSVariables(void)
{
  M_BindNum("st_wide_shift", &st_wide_shift,
            NULL, -1, -1, UL, ss_stat, wad_no, "HUD widescreen shift (-1 = Default)");
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

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

#include <stdlib.h>

#include "am_map.h"
#include "d_event.h"
#include "d_items.h"
#include "d_player.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "i_video.h"
#include "info.h"
#include "m_array.h"
#include "m_cheat.h"
#include "m_config.h"
#include "m_misc.h"
#include "m_random.h"
#include "m_swap.h"
#include "p_mobj.h"
#include "p_user.h"
#include "r_data.h"
#include "r_defs.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_state.h"
#include "s_sound.h"
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

// [Alaux]
static boolean hud_animated_counts;

static boolean sts_colored_numbers;

static boolean sts_pct_always_gray;

//jff 2/16/98 status color change levels
int ammo_red;      // ammo percent less than which status is red
int ammo_yellow;   // ammo percent less is yellow more green
int health_red;    // health amount less than which status is red
int health_yellow; // health amount less than which status is yellow
int health_green;  // health amount above is blue, below is green
int armor_red;     // armor amount less than which status is red
int armor_yellow;  // armor amount less than which status is yellow
int armor_green;   // armor amount above is blue, below is green

boolean hud_backpack_thresholds; // backpack changes thresholds
boolean hud_armor_type; // color of armor depends on type

// used for evil grin
static boolean  oldweaponsowned[NUMWEAPONS];

// [crispy] blinking key or skull in the status bar
int st_keyorskull[3];

static sbardef_t *sbardef;

typedef enum
{
    st_original,
    st_wide
} st_layout_t;

static st_layout_t st_layout;

static patch_t **facepatches = NULL;

static int have_xdthfaces;

//
// STATUS BAR CODE
//

patch_t *CachePatchName(const char *name)
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

    for (int painface = 0; painface < ST_NUMPAINFACES; ++painface)
    {
        for (int straightface = 0; straightface < ST_NUMSTRAIGHTFACES;
             ++straightface)
        {
            M_snprintf(lump, sizeof(lump), "STFST%d%d", painface, straightface);
            array_push(facepatches, V_CachePatchName(lump, PU_STATIC));
        }

        M_snprintf(lump, sizeof(lump), "STFTR%d0", painface); // turn right
        array_push(facepatches, V_CachePatchName(lump, PU_STATIC));

        M_snprintf(lump, sizeof(lump), "STFTL%d0", painface); // turn left
        array_push(facepatches, V_CachePatchName(lump, PU_STATIC));

        M_snprintf(lump, sizeof(lump), "STFOUCH%d", painface); // ouch!
        array_push(facepatches, V_CachePatchName(lump, PU_STATIC));

        M_snprintf(lump, sizeof(lump), "STFEVL%d", painface); // evil grin ;)
        array_push(facepatches, V_CachePatchName(lump, PU_STATIC));

        M_snprintf(lump, sizeof(lump), "STFKILL%d", painface); // pissed off
        array_push(facepatches, V_CachePatchName(lump, PU_STATIC));
    }

    M_snprintf(lump, sizeof(lump), "STFGOD0");
    array_push(facepatches, V_CachePatchName(lump, PU_STATIC));

    M_snprintf(lump, sizeof(lump), "STFDEAD0");
    array_push(facepatches, V_CachePatchName(lump, PU_STATIC));

    // [FG] support face gib animations as in the 3DO/Jaguar/PSX ports
    int painface;
    for (painface = 0; painface < ST_NUMXDTHFACES; ++painface)
    {
        M_snprintf(lump, sizeof(lump), "STFXDTH%d", painface);

        if (W_CheckNumForName(lump) != -1)
        {
            array_push(facepatches, V_CachePatchName(lump, PU_STATIC));
        }
        else
        {
            break;
        }
    }
    have_xdthfaces = painface;
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
                // ignore
                break;

            case sbc_featurelevelless:
                // ignore
                break;

            case sbc_sessiontypeeequal:
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

            case sbc_none:
            default:
                result = false;
                break;
        }
    }

    return result;
}

// [Alaux]
static int SmoothCount(int shownval, int realval)
{
    int step = realval - shownval;

    if (!hud_animated_counts || !step)
    {
        return realval;
    }
    else
    {
        int sign = step / abs(step);
        step = BETWEEN(1, 7, abs(step) / 20);
        shownval += (step + 1) * sign;

        if ((sign > 0 && shownval > realval)
            || (sign < 0 && shownval < realval))
        {
            shownval = realval;
        }

        return shownval;
    }
}

static int ResolveNumber(sbe_number_t *number, player_t *player)
{
    int result = 0;
    int param = number->param;

    switch (number->type)
    {
        case sbn_health:
            if (number->oldvalue == -1)
            {
                number->oldvalue = player->health;
            }
            result = SmoothCount(number->oldvalue, player->health);
            number->oldvalue = result;
            break;

        case sbn_armor:
            if (number->oldvalue == -1)
            {
                number->oldvalue = player->armorpoints;
            }
            result = SmoothCount(number->oldvalue, player->armorpoints);
            number->oldvalue = result;
            break;

        case sbn_frags:
            for (int p = 0; p < MAXPLAYERS; ++p)
            {
                result += player->frags[p];
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
    int health = player->health > 100 ? 100 : player->health;
    int lasthealthcalc =
        ST_FACESTRIDE * (((100 - health) * ST_NUMPAINFACES) / 101);
    face->oldhealth = health;
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
                if (oldweaponsowned[i] != player->weaponowned[i])
                {
                    doevilgrin = true;
                    oldweaponsowned[i] = player->weaponowned[i];
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
            if (player->health - face->oldhealth > ST_MUCHPAIN)
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
            if (player->health - face->oldhealth > ST_MUCHPAIN)
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
}

static void UpdateNumber(sbarelem_t *elem, player_t *player)
{
    sbe_number_t *number = elem->pointer.number;

    int value = ResolveNumber(number, player);
    int power = (value < 0 ? number->maxlength - 1 : number->maxlength);
    int max = (int)pow(10.0, power) - 1;
    int valglyphs = 0;
    int numvalues = 0;

    numberfont_t *font = number->font;
    if (font == NULL)
    {
        array_foreach(font, sbardef->numberfonts)
        {
            if (!strcmp(font->name, number->font_name))
            {
                break;
            }
        }
    }

    if (value < 0 && font->minus != NULL)
    {
        value = MAX(-max, value);
        numvalues = (int)log10(-value) + 1;
        valglyphs = numvalues + 1;
    }
    else
    {
        value = BETWEEN(0, max, value);
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

    elem->xoffset = 0;
    if (elem->alignment & sbe_h_middle)
    {
        elem->xoffset -= (totalwidth >> 1);
    }
    else if (elem->alignment & sbe_h_right)
    {
        elem->xoffset -= totalwidth;
    }

    number->font = font;
    number->value = value;
    number->numvalues = numvalues;
}

static void UpdateString(sbarelem_t *elem)
{
    sbe_widget_t *widget = elem->pointer.widget;

    int numglyphs = 0;

    hudfont_t *font = widget->font;
    if (font == NULL)
    {
        array_foreach(font, sbardef->hudfonts)
        {
            if (!strcmp(font->name, widget->font_name))
            {
                break;
            }
        }
    }

    numglyphs = strlen(widget->string);

    int totalwidth = font->monowidth * numglyphs;
    if (font->type == sbf_proportional)
    {
        totalwidth = 0;
        for (int i = 0; i < numglyphs; ++i)
        {
            int ch = widget->string[i];
            ch = M_ToUpper(ch) - HU_FONTSTART;
            if (ch < 0 || ch > HU_FONTSIZE)
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
    }

    elem->xoffset = 0;
    if (elem->alignment & sbe_h_middle)
    {
        elem->xoffset -= (totalwidth >> 1);
    }
    else if (elem->alignment & sbe_h_right)
    {
        elem->xoffset -= totalwidth;
    }

    widget->font = font;
    widget->totalwidth = totalwidth;
}

static void UpdateAnimation(sbarelem_t *elem)
{
    sbe_animation_t *animation = elem->pointer.animation;

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

    sbe_number_t *number = elem->pointer.number;

    boolean invul = (player->powers[pw_invulnerability]
                     || player->cheats & CF_GODMODE);

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
                if (player->backpack && !hud_backpack_thresholds)
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

static void UpdateWidget(sbarelem_t *elem, player_t *player)
{
    sbe_widget_t *widget = elem->pointer.widget;

    switch (widget->type)
    {
        case sbw_message:
            UpdateMessage(widget, player);
            break;
        case sbw_chat:
            UpdateChat(widget);
            break;
        case sbw_secret:
            UpdateSecretMessage(widget, player);
            break;
        case sbw_monsec:
            UpdateMonSec(widget);
            break;
        case sbw_time:
            UpdateStTime(widget, player);
            break;
        case sbw_coord:
        case sbw_fps:
        case sbw_speed:
            break;
        default:
            break;
    }

    UpdateString(elem);
}

static void UpdateElem(sbarelem_t *elem, player_t *player)
{
    switch (elem->type)
    {
        case sbe_face:
            UpdateFace(elem->pointer.face, player);
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
            UpdateWidget(elem, player);
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
    statusbar_t *statusbar;
    array_foreach(statusbar, sbardef->statusbars)
    {
        sbarelem_t *child;
        array_foreach(child, statusbar->children)
        {
            UpdateElem(child, player);
        }
    }
}

static void ResetElem(sbarelem_t *elem)
{
    switch (elem->type)
    {
        case sbe_graphic:
            {
                sbe_graphic_t *graphic = elem->pointer.graphic;
                graphic->patch = CachePatchName(graphic->patch_name);
            }
            break;

        case sbe_face:
            {
                sbe_face_t *face = elem->pointer.face;
                face->faceindex = 0;
                face->facecount = 0;
                face->oldhealth = -1;
            }
            break;

        case sbe_animation:
            {
                sbe_animation_t *animation = elem->pointer.animation;
                sbarframe_t *frame;
                array_foreach(frame, animation->frames)
                {
                    frame->patch = CachePatchName(frame->patch_name);
                }
                animation->frame_index = 0;
                animation->duration_left = 0;
            }
            break;

        case sbe_number:
        case sbe_percent:
            elem->pointer.number->oldvalue = -1;
            break;

        default:
            break;
    }

    sbarelem_t *child;
    array_foreach(child, elem->children)
    {
        ResetElem(child);
    }
}

static void ResetStatusBar(void)
{
    statusbar_t *statusbar;
    array_foreach(statusbar, sbardef->statusbars)
    {
        sbarelem_t *child;
        array_foreach(child, statusbar->children)
        {
            ResetElem(child);
        }
    }
}

static void DrawPatch(int x, int y, sbaralignment_t alignment, patch_t *patch,
                      crange_idx_e cr)
{
    if (!patch)
    {
        return;
    }

    int width = SHORT(patch->width);
    int height = SHORT(patch->height);

    if (alignment & sbe_h_middle)
    {
        x -= (width >> 1);
    }
    else if (alignment & sbe_h_right)
    {
        x -= width;
    }

    if (alignment & sbe_v_middle)
    {
        y -= (height >> 1);
    }
    else if (alignment & sbe_v_bottom)
    {
        y -= height;
    }

    if (st_layout == st_wide)
    {
        if (alignment & sbe_wide_left)
        {
            x -= video.deltaw;
        }
        if (alignment & sbe_wide_right)
        {
            x += video.deltaw;
        }
    }

    V_DrawPatchTranslated(x, y, patch, colrngs[cr]);
}

static void DrawGlyph(int x, int y, sbarelem_t *elem, fonttype_t fonttype,
                      int monowidth, patch_t *glyph)
{
    int width, widthdiff;

    if (fonttype == sbf_proportional)
    {
        width = glyph ? SHORT(glyph->width) : SPACEWIDTH;
        widthdiff = 0;
    }
    else
    {
        width = monowidth;
        widthdiff = glyph ? SHORT(glyph->width) - width : SPACEWIDTH - width;
    }

    if (elem->alignment & sbe_h_middle)
    {
        elem->xoffset += ((width + widthdiff) >> 1);
    }
    else if (elem->alignment & sbe_h_right)
    {
        elem->xoffset += (width + widthdiff);
    }

    if (glyph)
    {
        DrawPatch(x + elem->xoffset, y, elem->alignment, glyph,
                  elem->crboom == CR_NONE ? elem->cr : elem->crboom);
    }

    if (elem->alignment & sbe_h_middle)
    {
        elem->xoffset += (width - ((width - widthdiff) >> 1));
    }
    else if (elem->alignment & sbe_h_right)
    {
        elem->xoffset += -widthdiff;
    }
    else
    {
        elem->xoffset += width;
    }
}

static void DrawNumber(int x, int y, sbarelem_t *elem)
{
    sbe_number_t *number = elem->pointer.number;

    int value = number->value;
    int base_xoffset = elem->xoffset;
    numberfont_t *font = number->font;

    if (value < 0 && font->minus != NULL)
    {
        DrawGlyph(x, y, elem, font->type, font->monowidth, font->minus);
        value = -value;
    }

    int glyphindex = number->numvalues;
    while (glyphindex > 0)
    {
        int glyphbase = (int)pow(10.0, --glyphindex);
        int workingnum = value / glyphbase;
        DrawGlyph(x, y, elem, font->type, font->monowidth, font->numbers[workingnum]);
        value -= (workingnum * glyphbase);
    }

    if (elem->type == sbe_percent && font->percent != NULL)
    {
        crange_idx_e oldcr = elem->crboom;
        if (sts_pct_always_gray)
        {
            elem->crboom = CR_GRAY;
        }
        DrawGlyph(x, y, elem, font->type, font->monowidth, font->percent);
        elem->crboom = oldcr;
    }

    elem->xoffset = base_xoffset;
}

static void DrawString(int x, int y, sbarelem_t *elem)
{
    sbe_widget_t *widget = elem->pointer.widget;

    int base_xoffset = elem->xoffset;
    hudfont_t *font = widget->font;

    const char *str = widget->string;
    while (*str)
    {
        int ch = *str++;

        if (ch == '\x1b')
        {
            if (str)
            {
                ch = *str++;
                if (ch >= '0' && ch <= '0' + CR_NONE)
                {
                    elem->cr = ch - '0';
                }
                continue;
            }
        }

        ch = M_ToUpper(ch) - HU_FONTSTART;

        patch_t *glyph;
        if (ch < 0 || ch > HU_FONTSIZE)
        {
            glyph = NULL;
        }
        else
        {
            glyph = font->characters[ch];
        }
        DrawGlyph(x, y, elem, font->type, font->monowidth, glyph);
    }

    elem->xoffset = base_xoffset;
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
                sbe_graphic_t *graphic = elem->pointer.graphic;
                DrawPatch(x, y, elem->alignment, graphic->patch, elem->cr);
            }
            break;

        case sbe_face:
            {
                sbe_face_t *face = elem->pointer.face;
                DrawPatch(x, y, elem->alignment, facepatches[face->faceindex],
                          elem->cr);
            }
            break;

        case sbe_animation:
            {
                sbe_animation_t *animation = elem->pointer.animation;
                patch_t *patch = animation->frames[animation->frame_index].patch;
                DrawPatch(x, y, elem->alignment, patch, elem->cr);
            }
            break;

        case sbe_number:
        case sbe_percent:
            DrawNumber(x, y, elem);
            break;

        case sbe_widget:
            DrawString(x, y, elem);
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

boolean st_refresh_background = true;

static void DrawBackground(const char *name)
{
    if (!st_refresh_background)
    {
        V_CopyRect(0, 0, st_backing_screen, video.unscaledw, ST_HEIGHT, 0,
                   ST_Y);
        return;
    }

    V_UseBuffer(st_backing_screen);

    if (!name)
    {
        name = (gamemode == commercial) ? "GRNROCK" : "FLOOR7_2";
    }

    byte *flat = V_CacheFlatNum(firstflat + R_FlatNumForName(name), PU_CACHE);

    V_TileBlock64(ST_Y, video.unscaledw, ST_HEIGHT, flat);

    if (screenblocks == 10)
    {
        patch_t *patch = V_CachePatchName("brdr_b", PU_CACHE);
        for (int x = 0; x < video.unscaledw; x += 8)
        {
            V_DrawPatch(x - video.deltaw, 0, patch);
        }
    }

    V_RestoreBuffer();

    V_CopyRect(0, 0, st_backing_screen, video.unscaledw, ST_HEIGHT, 0, ST_Y);

    st_refresh_background = false;
}

static int current_barindex;

static void DrawStatusBar(void)
{
    player_t *player = &players[displayplayer];

    int barindex = MAX(screenblocks - 10, 0);

    if (automapactive && automapoverlay == AM_OVERLAY_OFF)
    {
        barindex = 0;
    }

    statusbar_t *statusbar = &sbardef->statusbars[barindex];

    if (!statusbar->fullscreenrender)
    {
        DrawBackground(statusbar->fillflat);
    }

    sbarelem_t *child;
    array_foreach(child, statusbar->children)
    {
        DrawElem(0, SCREENHEIGHT - statusbar->height, child, player);
    }

    current_barindex = barindex;
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
        hudfont_t *font = elem->pointer.widget->font;
        int height = font->maxheight;

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

    sbarelem_t *child;
    array_foreach(child, elem->children)
    {
        EraseElem(x, y, child, player);
    }
}

void ST_Erase(void)
{
    if (!sbardef)
    {
        return;
    }

    player_t *player = &players[displayplayer];
    statusbar_t *statusbar = &sbardef->statusbars[current_barindex];

    sbarelem_t *child;
    array_foreach(child, statusbar->children)
    {
        EraseElem(0, SCREENHEIGHT - statusbar->height, child, player);
    }
}

static boolean st_solidbackground;

/*
static void ST_DrawSolidBackground(int st_x)
{
  // [FG] calculate average color of the 16px left and right of the status bar
  const int vstep[][2] = {{0, 1}, {1, 2}, {2, ST_HEIGHT}};

  byte *pal = W_CacheLumpName("PLAYPAL", PU_STATIC);

  // [FG] temporarily draw status bar to background buffer
  V_DrawPatch(st_x, 0, sbar);

  const int offset = MAX(st_x + video.deltaw - SHORT(sbar->leftoffset), 0);
  const int width  = MIN(SHORT(sbar->width), video.unscaledw);
  const int depth  = 16;
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
        byte *c = st_backing_screen + V_ScaleY(y) * video.pitch + V_ScaleX(x + offset);
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
    col = I_GetNearestColor(pal, r/2, g/2, b/2);

    V_FillRect(0, v0, video.unscaledw, v1 - v0, col);
  }

  Z_ChangeTag (pal, PU_CACHE);
}
*/

// Respond to keyboard input events,
//  intercept cheats.
boolean ST_Responder(event_t *ev)
{
    // Filter automap on/off.
    if (ev->type == ev_keyup && (ev->data1.i & 0xffff0000) == AM_MSGHEADER)
    {
        return false;
    }
    else if (MessagesResponder(ev))
    {
        return true;
    }
    else // if a user keypress...
    {
        return M_CheatResponder(ev); // Try cheat responder in m_cheat.c
    }
}

static boolean sts_traditional_keys; // killough 2/28/98: traditional status bar keys
static boolean hud_blink_keys; // [crispy] blinking key or skull in the status bar

void ST_SetKeyBlink(player_t* player, int blue, int yellow, int red)
{
  int i;
  // Init array with args to iterate through
  const int keys[3] = { blue, yellow, red };

  player->keyblinktics = KEYBLINKTICS;

  for (i = 0; i < 3; i++)
  {
    if (   ((keys[i] == KEYBLINK_EITHER) && !(player->cards[i] || player->cards[i+3]))
        || ((keys[i] == KEYBLINK_CARD)   && !(player->cards[i]))
        || ((keys[i] == KEYBLINK_SKULL)  && !(player->cards[i+3]))
        || ((keys[i] == KEYBLINK_BOTH)   && !(player->cards[i] && player->cards[i+3])))
    {
      player->keyblinkkeys[i] = keys[i];
    }
    else
    {
      player->keyblinkkeys[i] = KEYBLINK_NONE;
    }
  }
}

int ST_BlinkKey(player_t* player, int index)
{
  const keyblink_t keyblink = player->keyblinkkeys[index];

  if (!keyblink)
    return KEYBLINK_NONE;

  if (player->keyblinktics & KEYBLINKMASK)
  {
    if (keyblink == KEYBLINK_EITHER)
    {
      if (st_keyorskull[index] && st_keyorskull[index] != KEYBLINK_BOTH)
      {
        return st_keyorskull[index];
      }
      else if ( (player->keyblinktics & (2*KEYBLINKMASK)) &&
               !(player->keyblinktics & (4*KEYBLINKMASK)))
      {
        return KEYBLINK_SKULL;
      }
      else
      {
        return KEYBLINK_CARD;
      }
    }
    else
    {
      return keyblink;
    }
  }

  return -1;
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
        UpdateChatMessage();
    }

    player_t *player = &players[displayplayer];

    UpdateStatusBar(player);

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
}

void ST_Init(void)
{
    sbardef = ST_ParseSbarDef();
    if (sbardef)
    {
        LoadFacePatches();
    }
}

void ST_InitRes(void)
{
    // killough 11/98: allocate enough for hires
    st_backing_screen =
        Z_Malloc(video.pitch * V_ScaleY(ST_HEIGHT) * sizeof(*st_backing_screen),
                 PU_RENDERER, 0);
}

void ST_ResetPalette(void)
{
    I_SetPalette(W_CacheLumpName("PLAYPAL", PU_CACHE));
}

void ST_BindSTSVariables(void)
{
  M_BindNum("st_layout", &st_layout, NULL,  st_wide, st_original, st_wide,
             ss_stat, wad_no, "HUD layout");
  M_BindBool("sts_colored_numbers", &sts_colored_numbers, NULL,
             false, ss_stat, wad_yes, "Colored numbers on the status bar");
  M_BindBool("sts_pct_always_gray", &sts_pct_always_gray, NULL,
             false, ss_stat, wad_yes,
             "Percent signs on the status bar are always gray");
  M_BindBool("sts_traditional_keys", &sts_traditional_keys, NULL,
             false, ss_stat, wad_yes,
             "Show last picked-up key on each key slot on the status bar");
  M_BindBool("hud_blink_keys", &hud_blink_keys, NULL,
             false, ss_stat, wad_no,
             "Make missing keys blink when trying to trigger linedef actions");
  M_BindBool("st_solidbackground", &st_solidbackground, NULL,
             false, ss_stat, wad_no,
             "Use solid-color borders for the status bar in widescreen mode");
  M_BindBool("hud_animated_counts", &hud_animated_counts, NULL,
            false, ss_stat, wad_no, "Animated health/armor counts");
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

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

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "hu_command.h"
#include "mn_internal.h"

#include "am_map.h"
#include "d_deh.h"
#include "d_main.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "g_game.h"
#include "hu_crosshair.h"
#include "i_gamepad.h"
#include "i_gyro.h"
#include "i_input.h"
#include "i_oalequalizer.h"
#include "i_oalsound.h"
#include "i_rumble.h"
#include "i_sound.h"
#include "i_video.h"
#include "m_argv.h"
#include "m_array.h"
#include "m_config.h"
#include "m_fixed.h"
#include "m_input.h"
#include "m_misc.h"
#include "m_swap.h"
#include "mn_font.h"
#include "mn_menu.h"
#include "p_mobj.h"
#include "r_bmaps.h"
#include "r_data.h"
#include "r_defs.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_plane.h" // [FG] R_InitPlanes()
#include "r_sky.h"   // [FG] R_UpdateStretchSkies()
#include "r_voxel.h"
#include "s_sound.h"
#include "s_trakinfo.h"
#include "st_sbardef.h"
#include "st_stuff.h"
#include "sounds.h"
#include "st_widgets.h"
#include "v_fmt.h"
#include "v_video.h"
#include "w_wad.h"
#include "ws_stuff.h"
#include "z_zone.h"

static int M_GetKeyString(int c, int offset);
static void DrawMenuString(int cx, int cy, int color);
static void DrawMenuStringBuffer(int flags, int x, int y, int color,
                                 const char *buffer);
static void DrawMenuStringEx(int flags, int x, int y, int color);

int warning_about_changes, print_warning_about_changes;

// killough 8/15/98: warn about changes not being committed until next game
#define warn_about_changes(x) \
    (warning_about_changes = (x), print_warning_about_changes = 2)

static boolean default_reset;

/////////////////////////////////////////////////////////////////////////////
//
// SETUP MENU (phares)
//
// We've added a set of Setup Screens from which you can configure a number
// of variables w/o having to restart the game. There are 7 screens:
//
//    Key Bindings
//    Weapons
//    Status Bar / HUD
//    Automap
//    Enemies
//    Messages
//    Chat Strings
//
// killough 10/98: added Compatibility and General menus
//

// phares 4/16/98:
// X,Y position of reset button. This is the same for every screen, and is
// only defined once here.
#define X_BUTTON          301
#define Y_BUTTON          (SCREENHEIGHT - 15 - 3)

#define CNTR_X            162
#define OFF_CNTR_X        (CNTR_X + 53)

#define M_WRAP            (SCREENWIDTH - CNTR_X - 8)
#define M_SPC             9
#define M_X               240
#define M_Y               (29 + M_SPC)
#define M_Y_WARN          (SCREENHEIGHT - 15)
#define M_Y_TITLE         2

#define M_THRM_STEP       8
#define M_THRM_HEIGHT     13
#define M_THRM_SIZE4      4
#define M_THRM_SIZE8      8
#define M_THRM_SIZE11     11
#define M_X_THRM4         (M_X - (M_THRM_SIZE4 + 3) * M_THRM_STEP)
#define M_X_THRM8         (M_X - (M_THRM_SIZE8 + 3) * M_THRM_STEP)
#define M_X_THRM11        (M_X - (M_THRM_SIZE11 + 3) * M_THRM_STEP)
#define M_THRM_TXT_OFFSET 3
#define M_THRM_SPC        (M_THRM_HEIGHT + 1)
#define M_THRM_UL_VAL     50

#define SPACEWIDTH        4

// Final entry
#define MI_END \
    {NULL, S_SKIP | S_END}

// Button for resetting to defaults
#define MI_RESET \
    {NULL, S_RESET, X_BUTTON, Y_BUTTON}

#define MI_GAP \
    {NULL, S_SKIP, 0, M_SPC}

#define MI_GAP_Y(y) \
    {NULL, S_SKIP, 0, (y)}

static setup_menu_t *GetMenuItem(setup_menu_t *menu, const char *item)
{
    while (!(menu->m_flags & S_END))
    {
        if (!(menu->m_flags & (S_SKIP | S_RESET)))
        {
            if (((menu->m_flags & S_HASDEFPTR)
                 && !strcasecmp(menu->var.def->name, item))
                || !strcasecmp(menu->m_text, item))
            {
                return menu;
            }
        }

        menu++;
    }

    I_Error("Item \"%s\" not found in menu", item);
}

static void DisableItem(boolean condition, setup_menu_t *menu, const char *item)
{
    setup_menu_t *menu_item = GetMenuItem(menu, item);

    if (condition)
    {
        menu_item->m_flags |= S_DISABLE;
    }
    else
    {
        menu_item->m_flags &= ~S_DISABLE;
    }
}

static void SetItemLimit(setup_menu_t *menu, const char *item, int min, int max)
{
    setup_menu_t *menu_item = GetMenuItem(menu, item);

    default_t *def = menu_item->var.def;
    def->limit.min = min;
    def->limit.max = max;
}

static void DisableItemsInternal(boolean condition, setup_menu_t *menu,
                                 const char *items[], int size)
{
    for (int i = 0; i < size; ++i)
    {
        DisableItem(condition, menu, items[i]);
    }
}

#define DisableItems(condition, menu, ...)                                   \
    DisableItemsInternal((condition), (menu), (const char *[]){__VA_ARGS__}, \
                         sizeof((const char *[]){__VA_ARGS__})               \
                             / sizeof(const char *))

/////////////////////////////
//
// booleans for setup screens
// these tell you what state the setup screens are in, and whether any of
// the overlay screens (automap colors, reset button message) should be
// displayed

boolean setup_active = false;             // in one of the setup screens
static boolean set_keybnd_active = false; // in key binding setup screens
static boolean set_weapon_active = false; // in weapons setup screen
static boolean setup_select = false;      // changing an item
static boolean setup_gather = false;      // gathering keys for value
boolean default_verify = false;           // verify reset defaults decision
static boolean block_input;
boolean setup_active_secondary;

/////////////////////////////
//
// set_menu_itemon is an index that starts at zero, and tells you which
// item on the current screen the cursor is sitting on.
//
// current_setup_menu is a pointer to the current setup menu table.

static int highlight_item;
static int set_item_on; // which setup item is selected?   // phares 3/98
static setup_menu_t *current_menu; // points to current setup menu table
static int current_page;           // the index of the current screen in a set

typedef struct
{
    const char *text;
    mrect_t rect;
    int flags;
} setup_tab_t;

static setup_tab_t *current_tabs;
static int highlight_tab;

// [FG] save the setup menu's itemon value in the S_END element's x coordinate

static int GetItemOn(void)
{
    const setup_menu_t *menu = current_menu;

    if (menu)
    {
        while (!(menu->m_flags & S_END))
        {
            menu++;
        }

        return menu->m_x;
    }

    return 0;
}

static void SetItemOn(const int x)
{
    setup_menu_t *menu = current_menu;

    if (menu)
    {
        while (!(menu->m_flags & S_END))
        {
            menu++;
        }

        menu->m_x = x;
    }
}

static int GetPageIndex(setup_menu_t *const *const pages);
static void SetPageIndex(const int y);

/////////////////////////////
//
// The menu_buffer is used to construct strings for display on the screen.

static char menu_buffer[66];

/////////////////////////////
//
// The setup_e enum is used to provide a unique number for each group of Setup
// Screens.

static ss_types setup_screen; // the current setup screen.

/////////////////////////////
//
// M_DrawBackground tiles a 64x64 patch over the entire screen, providing the
// background for the Help and Setup screens.
//
// killough 11/98: rewritten to support hires

static void DrawBackground(char *patchname)
{
    if (setup_active && menu_backdrop != MENU_BG_TEXTURE)
    {
        return;
    }

    V_DrawBackground(patchname);
}

/////////////////////////////
//
// Data that's used by the Setup screen code
//
// Establish the message colors to be used

#define CR_TITLE     CR_GOLD
#define CR_SET       CR_GREEN
#define CR_ITEM      CR_NONE
#define CR_HILITE    CR_NONE // CR_ORANGE
#define CR_SELECT    CR_GRAY

// chat strings must fit in this screen space
// killough 10/98: reduced, for more general uses
#define MAXCHATWIDTH 272

/////////////////////////////
//
// phares 4/17/98:
// Added 'Reset to Defaults' Button to Setup Menu screens
// This is a small button that sits in the upper-right-hand corner of
// the first screen for each group. It blinks when selected, thus the
// two patches, which it toggles back and forth.

static char reset_button_name[2][8] = {"M_BUTT1", "M_BUTT2"};

/////////////////////////////
//
// phares 4/18/98:
// Consolidate Item drawing code
//
// M_DrawItem draws the description of the provided item (the left-hand
// part). A different color is used for the text depending on whether the
// item is selected or not, or whether it's about to change.

enum
{
    str_empty,
    str_layout,
    str_flick_snap,
    str_ms_time,
    str_movement_sensitivity,
    str_movement_type,
    str_percent,
    str_curve,
    str_center_weapon,
    str_screensize,
    str_show_widgets,
    str_show_adv_widgets,
    str_stats_format,
    str_crosshair,
    str_crosshair_target,
    str_hudcolor,
    str_secretmessage,
    str_overlay,
    str_automap_preset,
    str_automap_keyed_door,
    str_fuzzmode,
    str_weapon_slots_activation,
    str_weapon_slots_selection,
    str_weapon_slots,

    str_resolution_scale,
    str_midi_player,

    str_gamma,
    str_sound_module,
    str_extra_music,
    str_resampler,
    str_equalizer_preset,

    str_mouse_accel,

    str_gamepad_device,
    str_gyro_space,
    str_gyro_action,
    str_gyro_sens,
    str_gyro_accel,

    str_default_complevel,
    str_endoom,
    str_death_use_action,
    str_widescreen,
    str_bobbing_pct,
    str_screen_melt,
    str_invul_mode,
    str_skill,
    str_freelook
};

static const char **GetStrings(int id);

static boolean ItemDisabled(int flags)
{
    complevel_t complevel =
        force_complevel != CL_NONE ? force_complevel : default_complevel;

    if ((flags & S_DISABLE)
        || (flags & S_STRICT && strictmode)
        || (flags & S_BOOM && complevel < CL_BOOM)
        || (flags & S_MBF && complevel < CL_MBF)
        || (flags & S_VANILLA && complevel != CL_VANILLA))
    {
        return true;
    }

    return false;
}

static boolean ItemSelected(setup_menu_t *s)
{
    if (s == current_menu + set_item_on && whichSkull)
    {
        return true;
    }

    return false;
}

static boolean PrevItemAvailable(setup_menu_t *s)
{
    int value = *s->var.def->location.i;
    int min = s->var.def->limit.min;

    return value > min;
}

static boolean NextItemAvailable(setup_menu_t *s)
{
    int value = *s->var.def->location.i;
    int max = s->var.def->limit.max;

    if (max == UL)
    {
        const char **strings = GetStrings(s->strings_id);
        if (strings)
        {
            max = array_size(strings) - 1;
        }
    }

    return value < max;
}

static void BlinkingArrowLeft(setup_menu_t *s)
{
    int flags = s->m_flags;

    if (!ItemSelected(s))
    {
        if (flags & S_CENTER)
        {
            strcpy(menu_buffer, "  ");    
        }
        return;
    }

    if (menu_input == mouse_mode)
    {
        return;
    }
    else if (flags & (S_CHOICE | S_CRITEM | S_THERMO))
    {
        if (setup_select && PrevItemAvailable(s))
        {
            strcpy(menu_buffer, "< ");
        }
        else if (!setup_select)
        {
            strcpy(menu_buffer, "> ");
        }
    }
    else
    {
        strcpy(menu_buffer, "> ");
    }
}

static void BlinkingArrowRight(setup_menu_t *s)
{
    int flags = s->m_flags;

    if (!ItemSelected(s))
    {
        if (flags & S_CENTER)
        {
            strcat(menu_buffer, "  ");    
        }
        return;
    }

    if (menu_input == mouse_mode)
    {
        return;
    }
    else if (flags & (S_CHOICE | S_CRITEM | S_THERMO))
    {
        if (setup_select && NextItemAvailable(s))
        {
            strcat(menu_buffer, " >");
        }
        else if (!setup_select)
        {
            strcat(menu_buffer, " <");
        }
    }
    else if (!setup_select)
    {
        strcat(menu_buffer, " <");
    }
}

#define M_TAB_Y      22
#define M_TAB_OFFSET 8

static void DrawTabs(void)
{
    setup_tab_t *tabs = current_tabs;

    int width = 0;

    for (int i = 0; tabs[i].text; ++i)
    {
        if (i)
        {
            width += M_TAB_OFFSET;
        }

        mrect_t *rect = &tabs[i].rect;
        if (!rect->w)
        {
            rect->w = MN_GetPixelWidth(tabs[i].text);
            rect->y = M_TAB_Y;
            rect->h = M_SPC;
        }
        width += rect->w;
    }

    int x = (SCREENWIDTH - width) / 2;

    for (int i = 0; tabs[i].text; ++i)
    {
        mrect_t *rect = &tabs[i].rect;

        if (i)
        {
            x += M_TAB_OFFSET;
        }

        menu_buffer[0] = '\0';
        strcpy(menu_buffer, tabs[i].text);
        if (i == current_page)
        {
            DrawMenuStringEx(tabs[i].flags, x, rect->y, CR_TITLE);
            V_FillRect(x + video.deltaw, rect->y + M_SPC, rect->w, 1,
                       colrngs[CR_TITLE][cr_shaded[v_lightest_color]]);
        }
        else
        {
            DrawMenuStringEx(tabs[i].flags, x, rect->y, CR_GRAY);
        }

        rect->x = x;

        x += rect->w;
    }
}

static int GetItemColor(int flags)
{
    return (flags & S_TITLE    ? CR_TITLE
            : flags & S_SELECT ? CR_SELECT
            : flags & S_HILITE ? CR_HILITE
                               : CR_ITEM); // killough 10/98
}

static void DrawItem(setup_menu_t *s, int accum_y)
{
    int x = s->m_x;
    int y = s->m_y;
    int flags = s->m_flags;
    mrect_t *rect = &s->rect;

    if (flags & S_RESET)
    {
        // This item is the reset button
        // Draw the 'off' version if this isn't the current menu item
        // Draw the blinking version in tune with the blinking skull otherwise

        const int index = (flags & (S_HILITE | S_SELECT)) ? whichSkull : 0;
        patch_t *patch = V_CachePatchName(reset_button_name[index], PU_CACHE);
        rect->x = x;
        rect->y = y;
        rect->w = SHORT(patch->width);
        rect->h = SHORT(patch->height);
        V_DrawPatch(x, y, patch);
        return;
    }

    if (!(flags & S_DIRECT))
    {
        y = accum_y;
    }

    // Draw the item string

    menu_buffer[0] = '\0';

    const char *text = s->m_text;
    const int color = GetItemColor(flags);

    BlinkingArrowLeft(s);

    // killough 10/98: support left-justification:
    strcat(menu_buffer, text);
    if (!(flags & (S_LEFTJUST | S_CENTER)))
    {
        x -= (MN_GetPixelWidth(menu_buffer) + 4);
    }

    rect->x = 0;
    rect->y = y;
    rect->w = SCREENWIDTH;
    rect->h = M_SPC * MAX(1, s->lines);

    if (flags & S_THERMO)
    {
        y += M_THRM_TXT_OFFSET;
    }

    if (flags & S_CENTER)
    {
        BlinkingArrowRight(s);
        x = (SCREENWIDTH  - MN_GetPixelWidth(menu_buffer)) / 2;
    }

    DrawMenuStringEx(flags, x, y, color);
}

// If a number item is being changed, allow up to N keystrokes to 'gather'
// the value. Gather_count tells you how many you have so far. The legality
// of what is gathered is determined by the low/high settings for the item.

#define MAXGATHER 5
static int gather_count;
static char
    gather_buffer[MAXGATHER + 1]; // killough 10/98: make input character-based

/////////////////////////////
//
// phares 4/18/98:
// Consolidate Item Setting drawing code
//
// M_DrawSetting draws the setting of the provided item (the right-hand
// part. It determines the text color based on whether the item is
// selected or being changed. Then, depending on the type of item, it
// displays the appropriate setting value: yes/no, a key binding, a number,
// a paint chip, etc.

static void (*DrawIndicator)(const setup_menu_t *s, int x, int y, int width);

static void DrawIndicator_Meter(const setup_menu_t *s, int x, int y, int width)
{
    const int flags = s->m_flags;

    if ((flags & S_HILITE) && !(flags & (S_END | S_SKIP | S_RESET | S_FUNC)))
    {
        const char *name = s->var.def->name;
        float scale = 0.0f;
        float limit = 0.0f;

        if (!strcasecmp(name, "joy_movement_inner_deadzone"))
        {
            I_GetRawAxesScaleMenu(true, &scale, &limit);
        }
        else if (!strcasecmp(name, "joy_camera_inner_deadzone"))
        {
            I_GetRawAxesScaleMenu(false, &scale, &limit);
        }
        else if (!strcasecmp(name, "gyro_smooth_threshold"))
        {
            I_GetRawGyroScaleMenu(&scale, &limit);
        }

        if (scale > 0.0f)
        {
            const byte shade = cr_shaded[v_lightest_color];
            const byte color = scale < limit    ? cr_green[shade]
                               : scale >= 0.99f ? cr_red[shade]
                                                : cr_gold[shade];
            V_FillRect(x, y, lroundf(width * scale), 1, color);
        }
    }
}

static void DrawSetupThermo(const setup_menu_t *s, int x, int y, int width,
                            int size, int dot, byte *cr)
{
    int xx;
    int i;

    xx = x;
    V_DrawPatchTranslated(xx, y, V_CachePatchName("M_THERML", PU_CACHE), cr);
    xx += M_THRM_STEP;

    patch_t *patch = V_CachePatchName("M_THERMM", PU_CACHE);
    for (i = 0; i < width + 1; i++)
    {
        V_DrawPatchTranslated(xx, y, patch, cr);
        xx += M_THRM_STEP;
    }
    V_DrawPatchTranslated(xx, y, V_CachePatchName("M_THERMR", PU_CACHE), cr);

    if (dot > size)
    {
        dot = size;
    }

    int step;
    if (size)
    {
        step = width * M_THRM_STEP * FRACUNIT / size;
    }
    else
    {
        step = 1;
    }

    if (DrawIndicator)
    {
        DrawIndicator(s, x + M_THRM_STEP + video.deltaw, y + patch->height / 2,
                      xx - x - M_THRM_STEP);
    }

    V_DrawPatchTranslated(x + M_THRM_STEP + dot * step / FRACUNIT, y,
                          V_CachePatchName("M_THERMO", PU_CACHE), cr);
}

static void WrapSettingString(setup_menu_t *s, int x, int y, int color)
{
    const int flags = s->m_flags;
    s->rect.y = y;

    if (MN_GetPixelWidth(menu_buffer) <= M_WRAP)
    {
        BlinkingArrowRight(s);
        DrawMenuStringEx(flags, x, y, color);
        return;
    }

    int index = 0;
    int line_pixel_width = 0;

    while (menu_buffer[index] != '\0')
    {
        char c[2] = {0};
        c[0] = menu_buffer[index];
        int pixel_width = MN_GetPixelWidth(c);

        if (menu_buffer[index] == ' ')
        {
            char *ptr = &menu_buffer[index + 1];
            int ptr_index = 0;

            while (ptr[ptr_index] != '\0' && ptr[ptr_index] != ' ')
            {
                ptr_index++;
            }

            if (ptr_index)
            {
                char old_c = ptr[ptr_index];
                ptr[ptr_index] = '\0';
                const int word_pixel_width = MN_GetPixelWidth(ptr);
                ptr[ptr_index] = old_c;

                if (line_pixel_width + pixel_width + word_pixel_width > M_WRAP)
                {
                    menu_buffer[index] = '\n';
                    line_pixel_width = 0;
                    pixel_width = 0;
                }
            }
        }

        index++;
        line_pixel_width += pixel_width;
    }

    BlinkingArrowRight(s);
    const int length = index;
    index = 0;
    s->lines = 0;

    while (index < length)
    {
        int offset = 0;

        while (menu_buffer[index + offset] != '\0'
               && menu_buffer[index + offset] != '\n')
        {
            offset++;
        }

        if (offset)
        {
            if (menu_buffer[index + offset] == '\n')
            {
                menu_buffer[index + offset] = '\0';
                offset++;
            }

            DrawMenuStringBuffer(flags, x, y, color, &menu_buffer[index]);
            y += M_SPC;
            s->lines++;

            if (s->lines > 1)
            {
                break;
            }
        }
        else
        {
            break;
        }

        index += offset;
    }
}

static void DrawSetting(setup_menu_t *s, int accum_y)
{
    int x = s->m_x, y = s->m_y, flags = s->m_flags, color;

    if (!(flags & S_DIRECT))
    {
        y = accum_y;
    }

    if (flags & S_FUNC)
    {
        // A menu item with the S_FUNC flag has no setting, so draw an
        // ellipsis to the right of the item with the same color.
        const int color = GetItemColor(flags);
        sprintf(menu_buffer, ". . .");
        BlinkingArrowRight(s);
        DrawMenuStringEx(flags, x, y, color);
        return;
    }

    // Determine color of the text. This may or may not be used
    // later, depending on whether the item is a text string or not.

    color = flags & S_SELECT ? CR_SELECT : CR_SET;

    // Is the item a YES/NO item?

    if (flags & S_ONOFF)
    {
        int value = *s->var.def->location.i;
        strcpy(menu_buffer, value ? "ON" : "OFF");
        BlinkingArrowRight(s);
        DrawMenuStringEx(flags, x, y, color);
        return;
    }

    // Is the item a simple number?

    if (flags & S_NUM)
    {
        // killough 10/98: We must draw differently for items being gathered.
        if (flags & (S_HILITE | S_SELECT) && setup_gather)
        {
            gather_buffer[gather_count] = 0;
            strcpy(menu_buffer, gather_buffer);
        }
        else
        {
            int value = *s->var.def->location.i;
            if (flags & S_PCT)
            {
                M_snprintf(menu_buffer, sizeof(menu_buffer), "%d%%", value);
            }
            else if (s->append)
            {
                M_snprintf(menu_buffer, sizeof(menu_buffer), "%d %s", value,
                           s->append);
            }
            else
            {
                M_snprintf(menu_buffer, sizeof(menu_buffer), "%d", value);
            }
        }

        BlinkingArrowRight(s);
        DrawMenuStringEx(flags, x, y, color);
        return;
    }

    // Is the item a key binding?

    if (flags & S_INPUT)
    {
        int i;
        int offset = 0;

        const input_t *inputs = M_Input(s->input_id);

        // Draw the input bound to the action
        menu_buffer[0] = '\0';

        for (i = 0; i < array_size(inputs); ++i)
        {
            if (i > 0)
            {
                menu_buffer[offset++] = ' ';
                menu_buffer[offset++] = '+';
                menu_buffer[offset++] = ' ';
                menu_buffer[offset] = '\0';
            }

            switch (inputs[i].type)
            {
                case INPUT_KEY:
                    offset = M_GetKeyString(inputs[i].value, offset);
                    break;
                case INPUT_MOUSEB:
                    offset += sprintf(menu_buffer + offset, "%s",
                                      M_GetNameForMouseB(inputs[i].value));
                    break;
                case INPUT_JOYB:
                    offset += sprintf(menu_buffer + offset, "%s",
                                      M_GetPlatformName(inputs[i].value));
                    break;
                default:
                    break;
            }
        }

        // "NONE"
        if (i == 0)
        {
            M_GetKeyString(0, 0);
        }

        BlinkingArrowRight(s);
        DrawMenuStringEx(flags, x, y, color);
    }

    // Is the item a weapon number?
    // OR, Is the item a colored text string from the Automap?
    //
    // killough 10/98: removed special code, since the rest of the engine
    // already takes care of it, and this code prevented the user from setting
    // their overall weapons preferences while playing Doom 1.
    //
    // killough 11/98: consolidated weapons code with color range code

    if (flags & S_WEAP) // weapon number
    {
        int value = *s->var.def->location.i;
        sprintf(menu_buffer, "%d", value);
        BlinkingArrowRight(s);
        DrawMenuStringEx(flags, x, y, color);
        return;
    }

    // [FG] selection of choices

    if (flags & (S_CHOICE | S_CRITEM))
    {
        int i = *s->var.def->location.i;
        const char **strings = GetStrings(s->strings_id);

        menu_buffer[0] = '\0';

        if (i >= 0 && strings)
        {
            strcat(menu_buffer, strings[i]);
        }

        if (flags & S_WRAP_LINE)
        {
            WrapSettingString(s, x, y, color);
            return;
        }

        BlinkingArrowRight(s);
        DrawMenuStringEx(flags, x, y, flags & S_CRITEM ? i : color);
        return;
    }

    if (flags & S_THERMO)
    {
        int value = *s->var.def->location.i;
        int min = s->var.def->limit.min;
        int max = s->var.def->limit.max;
        const char **strings = GetStrings(s->strings_id);

        int width;
        if (flags & S_THRM_SIZE11)
        {
            width = M_THRM_SIZE11;
        }
        else if (flags & S_THRM_SIZE4)
        {
            width = M_THRM_SIZE4;
        }
        else
        {
            width = M_THRM_SIZE8;
        }

        if (max == UL)
        {
            if (strings)
            {
                max = array_size(strings) - 1;
            }
            else
            {
                max = M_THRM_UL_VAL;
            }
        }

        int thrm_val = CLAMP(value, min, max);

        byte *cr;
        if (ItemDisabled(flags))
        {
            cr = cr_dark;
        }
        else if (flags & S_HILITE)
        {
            cr = cr_bright;
        }
        else
        {
            cr = NULL;
        }

        mrect_t *rect = &s->rect;
        rect->x = x;
        rect->y = y;
        rect->w = (width + 2) * M_THRM_STEP;
        rect->h = M_THRM_HEIGHT;
        DrawSetupThermo(s, x, y, width, max - min, thrm_val - min, cr);

        if (strings)
        {
            strcpy(menu_buffer, strings[value]);
        }
        else if (flags & S_PCT)
        {
            M_snprintf(menu_buffer, sizeof(menu_buffer), "%d%%", value);
        }
        else if (s->append)
        {
            M_snprintf(menu_buffer, sizeof(menu_buffer), "%d %s", value,
                       s->append);
        }
        else
        {
            M_snprintf(menu_buffer, sizeof(menu_buffer), "%d", value);
        }

        BlinkingArrowRight(s);
        DrawMenuStringEx(flags, x + M_THRM_STEP + rect->w,
                         y + M_THRM_TXT_OFFSET, color);
    }
}

/////////////////////////////
//
// M_DrawScreenItems takes the data for each menu item and gives it to
// the drawing routines above.

static void DrawScreenItems(setup_menu_t *src)
{
    if (print_warning_about_changes > 0) // killough 8/15/98: print warning
    {
        int x_warn;

        if (warning_about_changes & S_BADVAL)
        {
            strcpy(menu_buffer, "Value out of Range");
        }
        else if (warning_about_changes & S_PRGWARN)
        {
            strcpy(menu_buffer, "Warning: Restart to see changes");
        }
        else
        {
            strcpy(menu_buffer, "Warning: Changes apply to next game");
        }

        x_warn = SCREENWIDTH / 2 - MN_GetPixelWidth(menu_buffer) / 2;
        DrawMenuString(x_warn, M_Y_WARN, CR_RED);
    }

    int accum_y = M_Y;

    while (!(src->m_flags & S_END))
    {
        // See if we're to draw the item description (left-hand part)

        if (src->m_flags & S_SHOWDESC)
        {
            DrawItem(src, accum_y);
        }

        // See if we're to draw the setting (right-hand part)

        if (src->m_flags & S_SHOWSET)
        {
            DrawSetting(src, accum_y);
        }

        if (!(src->m_flags & S_DIRECT))
        {
            accum_y += src->m_y;
        }

        src++;
    }
}

/////////////////////////////
//
// Data used to draw the "are you sure?" dialogue box when resetting
// to defaults.

static void DrawDefVerify(void)
{
    patch_t *patch = V_CachePatchName("M_VBOX", PU_CACHE);
    int x = (SCREENWIDTH - patch->width) / 2;
    int y = (SCREENHEIGHT - patch->height) / 2;
    V_DrawPatch(x, y, patch);

    // The blinking messages is keyed off of the blinking of the
    // cursor skull.

    if (whichSkull) // blink the text
    {
        const char *text = "Restore defaults? (Y/N)";
        strcpy(menu_buffer, text);
        x = (SCREENWIDTH - MN_GetPixelWidth(text)) / 2;
        y = (SCREENHEIGHT - MN_StringHeight(text)) / 2;
        DrawMenuString(x, y, CR_RED);
    }
}

static void DrawNotification(const char *text, int color, boolean blink)
{
    patch_t *patch = V_CachePatchName("M_VBOX", PU_CACHE);
    int x = (SCREENWIDTH - patch->width) / 2;
    int y = (SCREENHEIGHT - patch->height) / 2;
    V_DrawPatch(x, y, patch);

    if (!blink || whichSkull)
    {
        x = (SCREENWIDTH - MN_GetPixelWidth(text)) / 2;
        y = (SCREENHEIGHT - MN_StringHeight(text)) / 2;
        MN_DrawString(x, y, color, text);
    }
}

void MN_DrawDelVerify(void)
{
    DrawNotification("Delete savegame? (Y/N)", CR_RED, true);
}

static void DrawGyroCalibration(void)
{
    switch (I_GetGyroCalibrationState())
    {
        case GYRO_CALIBRATION_INACTIVE:
            break;

        case GYRO_CALIBRATION_STARTING:
            block_input = true;
            DrawNotification("Starting calibration...", CR_GRAY, false);
            I_UpdateGyroCalibrationState();
            if (I_GetGyroCalibrationState() == GYRO_CALIBRATION_ACTIVE)
            {
                M_StartSound(sfx_pstop);
            }
            break;

        case GYRO_CALIBRATION_ACTIVE:
            DrawNotification("Calibrating, please wait...", CR_GRAY, false);
            I_UpdateGyroCalibrationState();
            if (I_GetGyroCalibrationState() == GYRO_CALIBRATION_COMPLETE)
            {
                M_StartSound(sfx_pstop);
            }
            break;

        case GYRO_CALIBRATION_COMPLETE:
            DrawNotification("Calibration complete!", CR_GREEN, false);
            I_UpdateGyroCalibrationState();
            if (I_GetGyroCalibrationState() == GYRO_CALIBRATION_INACTIVE)
            {
                M_StartSound(sfx_swtchx);
                block_input = false;
            }
            break;
    }
}

/////////////////////////////
//
// phares 4/18/98:
// M_DrawInstructions writes the instruction text just below the screen title
//
// killough 8/15/98: rewritten

typedef enum
{
    MENU_HELP_OFF,
    MENU_HELP_AUTO,
    MENU_HELP_KEY,
    MENU_HELP_PAD
} menu_help_t;

static menu_help_t menu_help;

static void DrawInstructions(void)
{
    int index = (menu_input == mouse_mode ? highlight_item : set_item_on);
    const setup_menu_t *item = &current_menu[index];
    const int flags = item->m_flags;

    if (menu_help == MENU_HELP_OFF || print_warning_about_changes > 0)
    {
        return;
    }

    // There are different instruction messages depending on whether you
    // are changing an item or just sitting on it.

    char s[80];
    s[0] = '\0';
    const char *first, *second;
    const boolean pad = ((menu_help == MENU_HELP_AUTO && help_input == pad_mode)
                         || menu_help == MENU_HELP_PAD);

    if (ItemDisabled(flags))
    {
        if (pad)
        {
            second = M_GetPlatformName(gamepad_cancel);
        }
        else
        {
            second = M_GetNameForKey(KEY_BACKSPACE);
        }

        M_snprintf(s, sizeof(s), "[ %s ] Back", second);
    }
    else if (item->desc)
    {
        M_snprintf(s, sizeof(s), "%s", item->desc);
    }
    else if (setup_select)
    {
        if (flags & S_INPUT)
        {
            M_snprintf(s, sizeof(s), "Press key or button to bind/unbind");
        }
        else if (flags & S_ONOFF)
        {
            if (pad)
            {
                first = M_GetPlatformName(gamepad_confirm);
                second = M_GetPlatformName(gamepad_cancel);
            }
            else
            {
                first = M_GetNameForKey(KEY_ENTER);
                second = M_GetNameForKey(KEY_ESCAPE);
            }

            M_snprintf(s, sizeof(s), "[ %s ] Toggle, [ %s ] Cancel", first,
                       second);
        }
        else if (flags & (S_CHOICE | S_CRITEM | S_THERMO))
        {
            if (pad)
            {
                second = M_GetPlatformName(gamepad_cancel);
            }
            else
            {
                second = M_GetNameForKey(KEY_ESCAPE);
            }

            M_snprintf(s, sizeof(s), "[ Left/Right ] Choose, [ %s ] Cancel",
                       second);
        }
        else if (flags & S_NUM)
        {
            M_snprintf(s, sizeof(s), "Enter value");
        }
        else if (flags & S_WEAP)
        {
            M_snprintf(s, sizeof(s), "Enter weapon number");
        }
        else if (flags & S_RESET)
        {
            if (pad)
            {
                first = M_GetPlatformName(gamepad_confirm);
                second = M_GetPlatformName(gamepad_cancel);
            }
            else
            {
                first = M_GetNameForKey(KEY_ENTER);
                second = M_GetNameForKey(KEY_ESCAPE);
            }

            M_snprintf(s, sizeof(s), "[ %s ] OK, [ %s ] Cancel", first, second);
        }
    }
    else
    {
        if (flags & S_INPUT)
        {
            if (pad)
            {
                first = M_GetPlatformName(gamepad_confirm);
                second = M_GetPlatformName(GAMEPAD_NORTH);
            }
            else
            {
                first = M_GetNameForKey(KEY_ENTER);
                second = M_GetNameForKey(KEY_DEL);
            }

            M_snprintf(s, sizeof(s), "[ %s ] Change, [ %s ] Clear", first,
                       second);
        }
        else if (flags & S_RESET)
        {
            if (pad)
            {
                first = M_GetPlatformName(gamepad_confirm);
            }
            else
            {
                first = M_GetNameForKey(KEY_ENTER);
            }

            M_snprintf(s, sizeof(s), "[ %s ] Restore defaults", first);
        }
        else
        {
            if (pad)
            {
                first = M_GetPlatformName(gamepad_confirm);
                second = M_GetPlatformName(gamepad_cancel);
            }
            else
            {
                first = M_GetNameForKey(KEY_ENTER);
                second = M_GetNameForKey(KEY_BACKSPACE);
            }

            M_snprintf(s, sizeof(s), "[ %s ] Change, [ %s ] Back", first,
                       second);
        }
    }

    MN_DrawStringCR((SCREENWIDTH - MN_GetPixelWidth(s)) / 2, M_Y_WARN,
                    cr_shaded, NULL, s);
}

static void SetupMenu(void)
{
    setup_active = true;
    setup_select = false;
    default_verify = false;
    setup_gather = false;
    highlight_tab = 0;
    highlight_item = 0;
    set_item_on = GetItemOn();
    while (current_menu[set_item_on++].m_flags & S_SKIP)
        ;
    current_menu[--set_item_on].m_flags |= S_HILITE;
}

static void SetupMenuSecondary(void)
{
    setup_active_secondary = true;
    SetupMenu();
}

/////////////////////////////
//
// The Key Binding Screen tables.

#define KB_X 130

static setup_tab_t keys_tabs[] = {
    {"action"},
    {"weapon"},
    {"misc"},
    {"func"},
    {"automap"},
    {"cheats"},
    {NULL}
};

// Note also that the first screen of each set has a line for the reset
// button. If there is more than one screen in a set, the others don't get
// the reset button.

static setup_menu_t keys_settings1[] = {
    {"Fire",         S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_fire},
    {"Forward",      S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_forward},
    {"Backward",     S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_backward},
    {"Strafe Left",  S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_strafeleft},
    {"Strafe Right", S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_straferight},
    {"Use",          S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_use},
    {"Run",          S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_speed},
    {"Strafe",       S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_strafe},
    {"Turn Left",    S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_turnleft},
    {"Turn Right",   S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_turnright},
    {"180 Turn",     S_INPUT | S_STRICT, KB_X, M_SPC, {0}, m_scrn, input_reverse},
    {"Gyro",         S_INPUT, KB_X, M_SPC, {0}, m_gyro, input_gyro},
    MI_GAP,
    {"Autorun",   S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_autorun},
    {"Free Look", S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_freelook},
    {"Vertmouse", S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_novert},
    MI_RESET,
    MI_END
};

static setup_menu_t keys_settings2[] = {
    {"Fist",     S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_weapon1},
    {"Pistol",   S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_weapon2},
    {"Shotgun",  S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_weapon3},
    {"Chaingun", S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_weapon4},
    {"Rocket",   S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_weapon5},
    {"Plasma",   S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_weapon6},
    {"BFG",      S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_weapon7},
    {"Chainsaw", S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_weapon8},
    {"SSG",      S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_weapon9},
    {"Best",     S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_weapontoggle},
    {"Last",     S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_lastweapon},
    MI_GAP,
    // [FG] prev/next weapon keys and buttons
    {"Prev", S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_prevweapon},
    {"Next", S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_nextweapon},
    MI_END
};

static setup_menu_t keys_settings3[] = {
    // [FG] reload current level / go to next level
    {"Rewind",          S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_rewind},
    {"Reload Map/Demo", S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_menu_reloadlevel},
    {"Next Map",        S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_menu_nextlevel},
    {"Previous Map",    S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_menu_prevlevel},
    {"Show Stats/Time", S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_hud_timestats},
    MI_GAP,
    {"Fast-FWD Demo",   S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_demo_fforward},
    {"Finish Demo",     S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_demo_quit},
    {"Join Demo",       S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_demo_join},
    {"Increase Speed",  S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_speed_up},
    {"Decrease Speed",  S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_speed_down},
    {"Default Speed",   S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_speed_default},
    MI_GAP,
    {"Begin Chat",      S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_chat},
    MI_END
};

static setup_menu_t keys_settings4[] = {
    {"Pause",        S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_pause},
    {"Save",         S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_savegame},
    {"Load",         S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_loadgame},
    {"Volume",       S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_soundvolume},
    {"Hud",          S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_hud},
    {"Quicksave",    S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_quicksave},
    {"End Game",     S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_endgame},
    {"Messages",     S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_messages},
    {"Quickload",    S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_quickload},
    {"Quit",         S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_quit},
    {"Gamma Fix",    S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_gamma},
    {"Spy",          S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_spy},
    {"Screenshot",   S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_screenshot},
    {"Clean Screenshot", S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_clean_screenshot},
    {"Larger View",  S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_zoomin},
    {"Smaller View", S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_zoomout},
    MI_END
};

static setup_menu_t keys_settings5[] = {
    {"Toggle Automap",  S_INPUT, KB_X, M_SPC, {0}, m_map, input_map},
    {"Follow",          S_INPUT, KB_X, M_SPC, {0}, m_map, input_map_follow},
    {"Overlay",         S_INPUT, KB_X, M_SPC, {0}, m_map, input_map_overlay},
    {"Rotate",          S_INPUT, KB_X, M_SPC, {0}, m_map, input_map_rotate},
    MI_GAP,
    {"Zoom In",         S_INPUT, KB_X, M_SPC, {0}, m_map, input_map_zoomin},
    {"Zoom Out",        S_INPUT, KB_X, M_SPC, {0}, m_map, input_map_zoomout},
    {"Shift Up",        S_INPUT, KB_X, M_SPC, {0}, m_map, input_map_up},
    {"Shift Down",      S_INPUT, KB_X, M_SPC, {0}, m_map, input_map_down},
    {"Shift Left",      S_INPUT, KB_X, M_SPC, {0}, m_map, input_map_left},
    {"Shift Right",     S_INPUT, KB_X, M_SPC, {0}, m_map, input_map_right},
    {"Mark Place",      S_INPUT, KB_X, M_SPC, {0}, m_map, input_map_mark},
    {"Clear Last Mark", S_INPUT, KB_X, M_SPC, {0}, m_map, input_map_clear},
    {"Full/Zoom",       S_INPUT, KB_X, M_SPC, {0}, m_map, input_map_gobig},
    {"Grid",            S_INPUT, KB_X, M_SPC, {0}, m_map, input_map_grid},
    MI_END
};

#define CHEAT_X 160

static setup_menu_t keys_settings6[] = {
    {"Fake Archvile Jump",   S_INPUT, CHEAT_X, M_SPC, {0}, m_scrn, input_avj      },
    {"God mode/Resurrect",   S_INPUT, CHEAT_X, M_SPC, {0}, m_scrn, input_iddqd    },
    {"Ammo & Keys",          S_INPUT, CHEAT_X, M_SPC, {0}, m_scrn, input_idkfa    },
    {"Ammo",                 S_INPUT, CHEAT_X, M_SPC, {0}, m_scrn, input_idfa     },
    {"No Clipping",          S_INPUT, CHEAT_X, M_SPC, {0}, m_scrn, input_idclip   },
    {"Health",               S_INPUT, CHEAT_X, M_SPC, {0}, m_scrn, input_idbeholdh},
    {"Armor",                S_INPUT, CHEAT_X, M_SPC, {0}, m_scrn, input_idbeholdm},
    {"Invulnerability",      S_INPUT, CHEAT_X, M_SPC, {0}, m_scrn, input_idbeholdv},
    {"Berserk",              S_INPUT, CHEAT_X, M_SPC, {0}, m_scrn, input_idbeholds},
    {"Partial Invisibility", S_INPUT, CHEAT_X, M_SPC, {0}, m_scrn, input_idbeholdi},
    {"Radiation Suit",       S_INPUT, CHEAT_X, M_SPC, {0}, m_scrn, input_idbeholdr},
    {"Reveal Map",           S_INPUT, CHEAT_X, M_SPC, {0}, m_scrn, input_iddt     },
    {"Light Amplification",  S_INPUT, CHEAT_X, M_SPC, {0}, m_scrn, input_idbeholdl},
    {"No Target",            S_INPUT, CHEAT_X, M_SPC, {0}, m_scrn, input_notarget },
    {"Freeze",               S_INPUT, CHEAT_X, M_SPC, {0}, m_scrn, input_freeze   },
    MI_END
};

static setup_menu_t *keys_settings[] = {
    keys_settings1,
    keys_settings2,
    keys_settings3,
    keys_settings4,
    keys_settings5,
    keys_settings6,
    NULL,
};

// Setting up for the Key Binding screen. Turn on flags, set pointers,
// locate the first item on the screen where the cursor is allowed to
// land.

void MN_KeyBindings(int choice)
{
    MN_SetNextMenuAlt(ss_keys);
    setup_screen = ss_keys;
    set_keybnd_active = true;
    current_page = GetPageIndex(keys_settings);
    current_menu = keys_settings[current_page];
    current_tabs = keys_tabs;
    SetupMenu();
}

// The drawing part of the Key Bindings Setup initialization. Draw the
// background, title, instruction line, and items.

void MN_DrawKeybnd(void)
{
    // Set up the Key Binding screen

    DrawBackground("FLOOR4_6"); // Draw background
    MN_DrawTitle(M_X_CENTER, M_Y_TITLE, "M_KEYBND", "Key Bindings");
    DrawTabs();
    DrawInstructions();
    DrawScreenItems(current_menu);

    // If the Reset Button has been selected, an "Are you sure?" message
    // is overlayed across everything else.

    if (default_verify)
    {
        DrawDefVerify();
    }
}

/////////////////////////////
//
// The Weapon Screen tables.

static setup_tab_t weap_tabs[] = {
    {"Prefs"},
    {"Slots"},
    {"Priority"},
    {NULL}
};

// [FG] centered or bobbing weapon sprite
static const char *center_weapon_strings[] = {"Off", "Centered", "Bobbing"};

static void UpdateCenteredWeaponItem(void);

static const char *bobbing_pct_strings[] = {"0%", "25%", "50%", "75%", "100%"};

static setup_menu_t weap_settings1[] = {

    {"Gameplay", S_SKIP | S_TITLE, CNTR_X, M_SPC},

    {"Weapon Carousel", S_ONOFF, CNTR_X, M_SPC, {"weapon_carousel"}},

    {"Vanilla Weapon Cycle", S_ONOFF | S_BOOM, CNTR_X, M_SPC,
     {"doom_weapon_cycle"}},

    {"Use Weapon Toggles", S_ONOFF | S_BOOM, CNTR_X, M_SPC,
     {"doom_weapon_toggles"}},

    // killough 8/8/98
    {"Pre-Beta BFG", S_ONOFF | S_STRICT, CNTR_X, M_SPC, {"classic_bfg"}},

    MI_GAP,

    {"Cosmetic", S_SKIP | S_TITLE, CNTR_X, M_SPC},

    {"View Bob", S_THERMO, CNTR_X, M_THRM_SPC, {"view_bobbing_pct"},
     .strings_id = str_bobbing_pct},

    {"Weapon Bob", S_THERMO, CNTR_X, M_THRM_SPC, {"weapon_bobbing_pct"},
     .strings_id = str_bobbing_pct, .action = UpdateCenteredWeaponItem},

    // [FG] centered or bobbing weapon sprite
    {"Weapon Alignment", S_CHOICE | S_STRICT, CNTR_X, M_SPC, {"center_weapon"},
     .strings_id = str_center_weapon},

    {"Hide Weapon", S_ONOFF | S_STRICT, CNTR_X, M_SPC, {"hide_weapon"}},

    {"Weapon Recoil", S_ONOFF, CNTR_X, M_SPC, {"weapon_recoilpitch"}},

    MI_RESET,

    MI_END
};

static const char *weapon_slots_activation_strings[] = {
    "Off", "Hold \"Last\"", "Always On"
};

static const char *weapon_slots_selection_strings[] = {
    "D-Pad", "Face Buttons", "1-4 Keys"
};

static const char **GetWeaponSlotStrings(void)
{
    static const char *vanilla_doom_strings[] = {
        "--", "Chainsaw/Fist", "Pistol", "Shotgun", "Chaingun",
        "Rocket", "Plasma", "BFG", "Chainsaw/Fist", "Shotgun"
    };
    static const char *vanilla_doom2_strings[] = {
        "--", "Chainsaw/Fist", "Pistol", "SSG/Shotgun", "Chaingun",
        "Rocket", "Plasma", "BFG", "Chainsaw/Fist", "SSG/Shotgun"
    };
    static const char *full_doom2_strings[] = {
        "--", "Fist", "Pistol", "Shotgun", "Chaingun",
        "Rocket", "Plasma", "BFG", "Chainsaw", "SSG"
    };

    if (force_complevel == CL_VANILLA || default_complevel == CL_VANILLA)
    {
        return (ALLOW_SSG ? vanilla_doom2_strings : vanilla_doom_strings);
    }
    else
    {
        return full_doom2_strings;
    }
}

#define WS_BUF_SiZE 80
static char slot_labels[NUM_WS_SLOTS * NUM_WS_WEAPS][WS_BUF_SiZE];

static void UpdateWeaponSlotLabels(void)
{
    const char *keys[NUM_WS_SLOTS];

    switch (WS_Selection())
    {
        case WS_SELECT_DPAD:
            keys[0] = M_GetPlatformName(GAMEPAD_DPAD_UP);
            keys[1] = M_GetPlatformName(GAMEPAD_DPAD_DOWN);
            keys[2] = M_GetPlatformName(GAMEPAD_DPAD_LEFT);
            keys[3] = M_GetPlatformName(GAMEPAD_DPAD_RIGHT);
            break;

        case WS_SELECT_FACE_BUTTONS:
            keys[0] = M_GetPlatformName(GAMEPAD_NORTH);
            keys[1] = M_GetPlatformName(GAMEPAD_SOUTH);
            keys[2] = M_GetPlatformName(GAMEPAD_WEST);
            keys[3] = M_GetPlatformName(GAMEPAD_EAST);
            break;

        default: // WS_SELECT_1234
            keys[0] = "1-Key";
            keys[1] = "2-Key";
            keys[2] = "3-Key";
            keys[3] = "4-Key";
            break;
    }

    const char *pos[NUM_WS_WEAPS] = {"1st", "2nd", "3rd"};
    int num = 0;

    for (int i = 0; i < NUM_WS_SLOTS; i++)
    {
        M_snprintf(slot_labels[num++], WS_BUF_SiZE, "%s %s", keys[i], pos[0]);

        for (int j = 1; j < NUM_WS_WEAPS; j++)
        {
            M_snprintf(slot_labels[num++], WS_BUF_SiZE, "%s", pos[j]);
        }
    }
}

static void UpdateWeaponSlotItems(void);

static void UpdateWeaponSlotActivation(void)
{
    WS_Reset();
    UpdateWeaponSlotItems();
}

static void UpdateWeaponSlotSelection(void)
{
    WS_UpdateSelection();
    WS_Reset();
    UpdateWeaponSlotLabels();
}

static void UpdateWeaponSlots(void)
{
    WS_UpdateSlots();
    WS_Reset();
}

#define MI_WEAPON_SLOT(i, s)                                      \
    {slot_labels[i], S_CHOICE, CNTR_X, M_SPC, {s},                \
     .strings_id = str_weapon_slots, .action = UpdateWeaponSlots}

static setup_menu_t weap_settings2[] = {

    {"Enable Slots", S_CHOICE, CNTR_X, M_SPC, {"weapon_slots_activation"},
     .strings_id = str_weapon_slots_activation,
     .action = UpdateWeaponSlotActivation},

    {"Select Slots", S_CHOICE, CNTR_X, M_SPC, {"weapon_slots_selection"},
     .strings_id = str_weapon_slots_selection,
     .action = UpdateWeaponSlotSelection},

    MI_GAP_Y(4),
    MI_WEAPON_SLOT(0, "weapon_slots_1_1"),
    MI_WEAPON_SLOT(1, "weapon_slots_1_2"),
    MI_WEAPON_SLOT(2, "weapon_slots_1_3"),
    MI_GAP_Y(4),
    MI_WEAPON_SLOT(3, "weapon_slots_2_1"),
    MI_WEAPON_SLOT(4, "weapon_slots_2_2"),
    MI_WEAPON_SLOT(5, "weapon_slots_2_3"),
    MI_GAP_Y(4),
    MI_WEAPON_SLOT(6, "weapon_slots_3_1"),
    MI_WEAPON_SLOT(7, "weapon_slots_3_2"),
    MI_WEAPON_SLOT(8, "weapon_slots_3_3"),
    MI_GAP_Y(4),
    MI_WEAPON_SLOT(9, "weapon_slots_4_1"),
    MI_WEAPON_SLOT(10, "weapon_slots_4_2"),
    MI_WEAPON_SLOT(11, "weapon_slots_4_3"),
    MI_END
};

static void UpdateWeaponSlotItems(void)
{
    const boolean condition = !WS_Enabled();

    DisableItem(condition, weap_settings2, "weapon_slots_selection");
    DisableItem(condition, weap_settings2, "weapon_slots_1_1");
    DisableItem(condition, weap_settings2, "weapon_slots_1_2");
    DisableItem(condition, weap_settings2, "weapon_slots_1_3");
    DisableItem(condition, weap_settings2, "weapon_slots_2_1");
    DisableItem(condition, weap_settings2, "weapon_slots_2_2");
    DisableItem(condition, weap_settings2, "weapon_slots_2_3");
    DisableItem(condition, weap_settings2, "weapon_slots_3_1");
    DisableItem(condition, weap_settings2, "weapon_slots_3_2");
    DisableItem(condition, weap_settings2, "weapon_slots_3_3");
    DisableItem(condition, weap_settings2, "weapon_slots_4_1");
    DisableItem(condition, weap_settings2, "weapon_slots_4_2");
    DisableItem(condition, weap_settings2, "weapon_slots_4_3");
}

static setup_menu_t weap_settings3[] = {
    {"1st Choice Weapon", S_WEAP | S_BOOM, OFF_CNTR_X, M_SPC, {"weapon_choice_1"}},
    {"2nd Choice Weapon", S_WEAP | S_BOOM, OFF_CNTR_X, M_SPC, {"weapon_choice_2"}},
    {"3rd Choice Weapon", S_WEAP | S_BOOM, OFF_CNTR_X, M_SPC, {"weapon_choice_3"}},
    {"4th Choice Weapon", S_WEAP | S_BOOM, OFF_CNTR_X, M_SPC, {"weapon_choice_4"}},
    {"5th Choice Weapon", S_WEAP | S_BOOM, OFF_CNTR_X, M_SPC, {"weapon_choice_5"}},
    {"6th Choice Weapon", S_WEAP | S_BOOM, OFF_CNTR_X, M_SPC, {"weapon_choice_6"}},
    {"7th Choice Weapon", S_WEAP | S_BOOM, OFF_CNTR_X, M_SPC, {"weapon_choice_7"}},
    {"8th Choice Weapon", S_WEAP | S_BOOM, OFF_CNTR_X, M_SPC, {"weapon_choice_8"}},
    {"9th Choice Weapon", S_WEAP | S_BOOM, OFF_CNTR_X, M_SPC, {"weapon_choice_9"}},
    MI_END
};

static setup_menu_t *weap_settings[] = {
    weap_settings1, weap_settings2, weap_settings3, NULL
};

static void UpdateCenteredWeaponItem(void)
{
    DisableItem(!weapon_bobbing_pct, weap_settings1, "center_weapon");
}

// Setting up for the Weapons screen. Turn on flags, set pointers,
// locate the first item on the screen where the cursor is allowed to
// land.

void MN_Weapons(int choice)
{
    MN_SetNextMenuAlt(ss_weap);
    setup_screen = ss_weap;
    set_weapon_active = true;
    current_page = GetPageIndex(weap_settings);
    current_menu = weap_settings[current_page];
    current_tabs = weap_tabs;
    SetupMenu();
}

// The drawing part of the Weapons Setup initialization. Draw the
// background, title, instruction line, and items.

void MN_DrawWeapons(void)
{
    DrawBackground("FLOOR4_6"); // Draw background
    MN_DrawTitle(M_X_CENTER, M_Y_TITLE, "M_WEAP", "Weapons");
    DrawTabs();
    DrawInstructions();
    DrawScreenItems(current_menu);

    // If the Reset Button has been selected, an "Are you sure?" message
    // is overlayed across everything else.

    if (default_verify)
    {
        DrawDefVerify();
    }
}

/////////////////////////////
//
// The Status Bar / HUD tables.

static setup_tab_t stat_tabs[] = {
    {"HUD"},
    {"Widgets"},
    {"Crosshair"},
    {"Messages"},
    {NULL}
};

static void SizeDisplayAlt(void)
{
    R_SetViewSize(screenblocks);
}

static void RefreshSolidBackground(void)
{
    st_refresh_background = true;
}

#define H_X_THRM8 (M_X_THRM8 - 14)
#define H_X       (M_X - 14)

static setup_menu_t stat_settings1[] = {

    {"Screen Size", S_THERMO, H_X_THRM8, M_THRM_SPC, {"screenblocks"},
     .strings_id = str_screensize, .action = SizeDisplayAlt},

    MI_GAP,

    {"Wide Shift", S_THERMO, H_X_THRM8, M_THRM_SPC, {"st_wide_shift"},
     .append = "px"},

    MI_GAP,

    {"Status Bar", S_SKIP | S_TITLE, H_X, M_SPC},

    {"Colored Numbers", S_ONOFF | S_COSMETIC, H_X, M_SPC, {"sts_colored_numbers"}},

    {"Solid Background Color", S_ONOFF, H_X, M_SPC, {"st_solidbackground"},
     .action = RefreshSolidBackground},

    MI_RESET,

    MI_END
};

void MN_UpdateWideShiftItem(boolean reset)
{
    DisableItem(!video.deltaw, stat_settings1, "st_wide_shift");
    SetItemLimit(stat_settings1, "st_wide_shift", 0, video.deltaw);
    if (reset || st_wide_shift == -1)
    {
        st_wide_shift = video.deltaw;
    }
    st_wide_shift = CLAMP(st_wide_shift, 0, video.deltaw);
}

static void UpdateStatsFormatItem(void);

static const char *show_widgets_strings[] = {"Off", "Automap", "HUD", "Always"};
static const char *show_adv_widgets_strings[] = {"Off", "Automap", "HUD",
                                                 "Always", "Advanced"};

static const char *stats_format_strings[] = {
  "Ratio", "Boolean", "Percent", "Remaining", "Count"
};

static setup_menu_t stat_settings2[] = {

    {"Widget Types", S_SKIP | S_TITLE, H_X, M_SPC},

    {"Show Level Stats", S_CHOICE, H_X, M_SPC, {"hud_level_stats"},
     .strings_id = str_show_widgets, .action = UpdateStatsFormatItem},

    {"Show Level Time", S_CHOICE, H_X, M_SPC, {"hud_level_time"},
     .strings_id = str_show_widgets},

    {"Show Player Coords", S_CHOICE | S_STRICT, H_X, M_SPC,
     {"hud_player_coords"}, .strings_id = str_show_adv_widgets},

    {"Show Command History", S_ONOFF | S_STRICT, H_X, M_SPC,
     {"hud_command_history"}, .action = HU_ResetCommandHistory},

    {"Show Use-Button Timer", S_ONOFF, H_X, M_SPC, {"hud_time_use"}},

    MI_GAP,

    {"Widget Appearance", S_SKIP | S_TITLE, H_X, M_SPC},

    {"Use Doom Font", S_CHOICE, H_X, M_SPC, {"hud_widget_font"},
     .strings_id = str_show_widgets},

    {"Level Stats Format", S_CHOICE, H_X, M_SPC, {"hud_stats_format"},
     .strings_id = str_stats_format},

    MI_END
};

static void UpdateCrosshairItems(void);

static const char *crosshair_target_strings[] = {"Off", "Highlight", "Health"};

static const char *hudcolor_strings[] = {
    "BRICK",  "TAN",    "GRAY",  "GREEN", "BROWN",  "GOLD",  "RED", "BLUE",
    "ORANGE", "YELLOW", "BLUE2", "BLACK", "PURPLE", "WHITE", "NONE"
};

#define XH_X (H_X - 33)

static setup_menu_t stat_settings3[] = {

    {"Crosshair", S_CHOICE, XH_X, M_SPC, {"hud_crosshair"},
     .strings_id = str_crosshair, .action = UpdateCrosshairItems},

    {"Color By Player Health", S_ONOFF | S_STRICT, XH_X, M_SPC, {"hud_crosshair_health"}},

    {"Color By Target", S_CHOICE | S_STRICT, XH_X, M_SPC, {"hud_crosshair_target"},
     .strings_id = str_crosshair_target, .action = UpdateCrosshairItems},

    {"Lock On Target", S_ONOFF | S_STRICT, XH_X, M_SPC, {"hud_crosshair_lockon"}},

    {"Default Color", S_CRITEM, XH_X, M_SPC, {"hud_crosshair_color"},
     .strings_id = str_hudcolor},

    {"Highlight Color", S_CRITEM | S_STRICT, XH_X, M_SPC,
     {"hud_crosshair_target_color"}, .strings_id = str_hudcolor},

    MI_END
};

static const char *secretmessage_strings[] = {
    "Off", "On", "Count",
};

static setup_menu_t stat_settings4[] = {
    {"Announce Revealed Secrets", S_CHOICE, H_X, M_SPC, {"hud_secret_message"},
     .strings_id = str_secretmessage},
    {"Announce Map Titles",  S_ONOFF, H_X, M_SPC, {"hud_map_announce"}},
    {"Show Toggle Messages", S_ONOFF, H_X, M_SPC, {"show_toggle_messages"}},
    {"Show Pickup Messages", S_ONOFF, H_X, M_SPC, {"show_pickup_messages"}},
    {"Show Obituaries",      S_ONOFF, H_X, M_SPC, {"show_obituary_messages"}},
    {"Center Messages",      S_ONOFF, H_X, M_SPC, {"message_centered"}},
    {"Colorize Messages",    S_ONOFF, H_X, M_SPC, {"message_colorized"},
     .action = ST_ResetMessageColors},
    MI_END
};

static setup_menu_t *stat_settings[] = {stat_settings1, stat_settings2,
                                        stat_settings3, stat_settings4, NULL};

static void UpdateStatsFormatItem(void)
{
  DisableItem(!hud_level_stats, stat_settings2, "hud_stats_format");
}

static void UpdateCrosshairItems(void)
{
    DisableItem(!hud_crosshair, stat_settings3, "hud_crosshair_health");
    DisableItem(!hud_crosshair, stat_settings3, "hud_crosshair_target");
    DisableItem(!hud_crosshair, stat_settings3, "hud_crosshair_lockon");
    DisableItem(!hud_crosshair, stat_settings3, "hud_crosshair_color");
    DisableItem(
        !(hud_crosshair && hud_crosshair_target == crosstarget_highlight),
        stat_settings3, "hud_crosshair_target_color");

    HU_StartCrosshair();
}

// Setting up for the Status Bar / HUD screen. Turn on flags, set pointers,
// locate the first item on the screen where the cursor is allowed to
// land.

void MN_StatusBar(int choice)
{
    MN_SetNextMenuAlt(ss_stat);
    setup_screen = ss_stat;
    current_page = GetPageIndex(stat_settings);
    current_menu = stat_settings[current_page];
    current_tabs = stat_tabs;
    SetupMenu();
}

// The drawing part of the Status Bar / HUD Setup initialization. Draw the
// background, title, instruction line, and items.

void MN_DrawStatusHUD(void)
{
    DrawBackground("FLOOR4_6"); // Draw background
    MN_DrawTitle(M_X_CENTER, M_Y_TITLE, "M_STAT", "Status Bar/HUD");
    DrawTabs();
    DrawInstructions();
    DrawScreenItems(current_menu);

    if (hud_crosshair && current_page == 2)
    {
        patch_t *patch =
            V_CachePatchName(crosshair_lumps[hud_crosshair], PU_CACHE);

        int x = XH_X + 85 - SHORT(patch->width) / 2;
        int y = M_Y + M_SPC / 2 - SHORT(patch->height) / 2 - 1;

        V_DrawPatchTranslated(x, y, patch, colrngs[hud_crosshair_color]);
    }

    // If the Reset Button has been selected, an "Are you sure?" message
    // is overlayed across everything else.

    if (default_verify)
    {
        DrawDefVerify();
    }
}

/////////////////////////////
//
// The Automap tables.

static const char *overlay_strings[] = {"Off", "On", "Dark"};

static const char *automap_preset_strings[] = {"Vanilla", "Crispy", "Boom", "ZDoom"};

static const char *automap_keyed_door_strings[] = {"Off", "On", "Flashing"};

static setup_menu_t auto_settings1[] = {

    {"Modes", S_SKIP | S_TITLE, H_X, M_SPC},

    {"Follow Player",   S_ONOFF,  H_X, M_SPC, {"followplayer"}},
    {"Rotate Automap",  S_ONOFF,  H_X, M_SPC, {"automaprotate"}},
    {"Overlay Automap", S_CHOICE, H_X, M_SPC, {"automapoverlay"},
     .strings_id = str_overlay},

    MI_GAP,

    {"Miscellaneous", S_SKIP | S_TITLE, H_X, M_SPC},

    {"Color Preset", S_CHOICE | S_COSMETIC, H_X, M_SPC, {"mapcolor_preset"},
     .strings_id = str_automap_preset, .action = AM_ColorPreset},

    {"Show Found Secrets Only", S_ONOFF, H_X, M_SPC, {"map_secret_after"}},

    {"Color Keyed Doors", S_CHOICE, H_X, M_SPC, {"map_keyed_door"},
     .strings_id = str_automap_keyed_door},

    MI_RESET,

    MI_END
};

static setup_menu_t *auto_settings[] = {auto_settings1, NULL};

// Setting up for the Automap screen. Turn on flags, set pointers,
// locate the first item on the screen where the cursor is allowed to
// land.

void MN_Automap(int choice)
{
    MN_SetNextMenuAlt(ss_auto);
    setup_screen = ss_auto;
    current_page = GetPageIndex(auto_settings);
    current_menu = auto_settings[current_page];
    current_tabs = NULL;
    SetupMenu();
}

// The drawing part of the Automap Setup initialization. Draw the
// background, title, instruction line, and items.

void MN_DrawAutoMap(void)
{
    DrawBackground("FLOOR4_6"); // Draw background
    MN_DrawTitle(M_X_CENTER, M_Y_TITLE, "M_AUTO", "Automap");
    DrawInstructions();
    DrawScreenItems(current_menu);

    // If the Reset Button has been selected, an "Are you sure?" message
    // is overlayed across everything else.

    if (default_verify)
    {
        DrawDefVerify();
    }
}

/////////////////////////////
//
// The Enemies table.

static void BarkSound(void)
{
    if (default_dogs)
    {
        M_StartSound(sfx_dgact);
    }
}

static setup_menu_t enem_settings1[] = {

    {"Helper Dogs", S_MBF | S_THERMO | S_THRM_SIZE4 | S_LEVWARN | S_ACTION,
     M_X_THRM4, M_THRM_SPC, {"player_helpers"}, .action = BarkSound},

    MI_GAP,

    {"Cosmetic", S_SKIP | S_TITLE, M_X, M_SPC},

    // [FG] colored blood and gibs
    {"Colored Blood", S_ONOFF | S_STRICT, M_X, M_SPC, {"colored_blood"},
     .action = D_SetBloodColor},

    // [crispy] randomly flip corpse, blood and death animation sprites
    {"Randomly Mirrored Corpses", S_ONOFF | S_STRICT, M_X, M_SPC, {"flipcorpses"}},

    // [crispy] resurrected pools of gore ("ghost monsters") are translucent
    {"Translucent Ghost Monsters", S_ONOFF | S_STRICT | S_VANILLA, M_X, M_SPC,
     {"ghost_monsters"}},

    MI_RESET,

    MI_END
};

static setup_menu_t *enem_settings[] = {enem_settings1, NULL};

/////////////////////////////

// Setting up for the Enemies screen. Turn on flags, set pointers,
// locate the first item on the screen where the cursor is allowed to
// land.

void MN_Enemy(int choice)
{
    MN_SetNextMenuAlt(ss_enem);
    setup_screen = ss_enem;
    current_page = GetPageIndex(enem_settings);
    current_menu = enem_settings[current_page];
    current_tabs = NULL;
    SetupMenu();
}

// The drawing part of the Enemies Setup initialization. Draw the
// background, title, instruction line, and items.

void MN_DrawEnemy(void)
{
    DrawBackground("FLOOR4_6"); // Draw background
    MN_DrawTitle(M_X_CENTER, M_Y_TITLE, "M_ENEM", "Enemies");
    DrawInstructions();
    DrawScreenItems(current_menu);

    // If the Reset Button has been selected, an "Are you sure?" message
    // is overlayed across everything else.

    if (default_verify)
    {
        DrawDefVerify();
    }
}

/////////////////////////////
//
// The Compatibility table.
// killough 10/10/98

static const char *default_complevel_strings[] = {
    "Vanilla", "Boom", "MBF", "MBF21"
};

static void UpdateInterceptsEmuItem(void);
static void UpdateWeaponSlotStrings(void);

static void UpdateDefaultCompatibilityLevel(void)
{
    UpdateInterceptsEmuItem();
    UpdateWeaponSlotStrings();
}

setup_menu_t comp_settings1[] = {

    {"Default Compatibility Level", S_CHOICE | S_LEVWARN, M_X, M_SPC,
     {"default_complevel"}, .strings_id = str_default_complevel,
     .action = UpdateDefaultCompatibilityLevel},

    MI_GAP,

    {"Auto Strafe 50", S_ONOFF | S_STRICT, M_X, M_SPC, {"autostrafe50"},
     .action = G_UpdateSideMove},

    {"Improved Hit Detection", S_ONOFF | S_STRICT, M_X, M_SPC,
     {"blockmapfix"}},

    {"Fast Line-of-Sight Calculation", S_ONOFF | S_STRICT, M_X, M_SPC,
     {"checksight12"}, .action = P_UpdateCheckSight},

    {"Walk Under Solid Hanging Bodies", S_ONOFF | S_STRICT, M_X, M_SPC,
     {"hangsolid"}},

    {"Emulate INTERCEPTS overflow", S_ONOFF | S_VANILLA, M_X, M_SPC,
     {"emu_intercepts"}, .action = UpdateInterceptsEmuItem},

    MI_RESET,

    MI_END
};

static void UpdateInterceptsEmuItem(void)
{
    DisableItem((force_complevel == CL_VANILLA || default_complevel == CL_VANILLA)
                    && overflow[emu_intercepts].enabled,
                comp_settings1, "blockmapfix");
}

static setup_menu_t *comp_settings[] = {comp_settings1, NULL};

// Setting up for the Compatibility screen. Turn on flags, set pointers,
// locate the first item on the screen where the cursor is allowed to
// land.

void MN_Compat(int choice)
{
    MN_SetNextMenuAlt(ss_comp);
    setup_screen = ss_comp;
    current_page = GetPageIndex(comp_settings);
    current_menu = comp_settings[current_page];
    current_tabs = NULL;
    SetupMenu();
}

// The drawing part of the Compatibility Setup initialization. Draw the
// background, title, instruction line, and items.

void MN_DrawCompat(void)
{
    DrawBackground("FLOOR4_6"); // Draw background
    MN_DrawTitle(M_X_CENTER, M_Y_TITLE, "M_COMPAT", "Compatibility");
    DrawInstructions();
    DrawScreenItems(current_menu);

    // If the Reset Button has been selected, an "Are you sure?" message
    // is overlayed across everything else.

    if (default_verify)
    {
        DrawDefVerify();
    }
}

/////////////////////////////
//
// The General table.
// killough 10/10/98

static setup_tab_t gen_tabs[] = {
    {"video"},
    {"audio"},
    {"mouse"},
    {"gamepad"},
    {"display"},
    {"misc"},
    {NULL}
};

static int resolution_scale;

static const char **GetResolutionScaleStrings(void)
{
    const char **strings = NULL;
    resolution_scaling_t rs = I_GetResolutionScaling();

    array_push(strings, "100%");

    resolution_scale = 0;
    int val = SCREENHEIGHT * 2;
    char buf[8];
    int i;

    for (i = 1; val < rs.max; ++i)
    {
        if (val == current_video_height)
        {
            resolution_scale = i;
        }

        int pct = val * 100 / SCREENHEIGHT;
        M_snprintf(buf, sizeof(buf), "%d%%", pct);
        array_push(strings, M_StringDuplicate(buf));

        val += rs.step;
    }

    if (current_video_height == rs.max)
    {
        resolution_scale = i;
    }

    array_push(strings, "max");

    return strings;
}

static void ResetVideoHeight(void)
{
    const char **strings = GetStrings(str_resolution_scale);
    resolution_scaling_t rs = I_GetResolutionScaling();

    if (default_reset)
    {
        current_video_height = 600;
        int val = SCREENHEIGHT * 2;
        for (int i = 1; val < rs.max; ++i)
        {
            if (val == current_video_height)
            {
                resolution_scale = i;
                break;
            }

            val += rs.step;
        }
    }
    else
    {
        if (resolution_scale == array_size(strings))
        {
            current_video_height = rs.max;
        }
        else if (resolution_scale == 0)
        {
            current_video_height = SCREENHEIGHT;
        }
        else
        {
            current_video_height =
                SCREENHEIGHT * 2 + (resolution_scale - 1) * rs.step;
        }
    }

    if (!dynamic_resolution)
    {
        VX_ResetMaxDist();
    }

    MN_UpdateDynamicResolutionItem();

    resetneeded = true;
}

const char *widescreen_strings[] = {"Off", "Auto", "16:10", "16:9",
                                    "21:9", "32:9"};

static void ResetVideo(void)
{
    resetneeded = true;
}

static void UpdateFOV(void)
{
    setsizeneeded = true; // run R_ExecuteSetViewSize;
}

static void ToggleFullScreen(void)
{
    toggle_fullscreen = true;
}

static void UpdateFPSLimit(void)
{
    setrefreshneeded = true;
}

const char *gamma_strings[] = {
    // Darker
    "-4", "-3.6", "-3.2", "-2.8", "-2.4", "-2.0", "-1.6", "-1.2", "-0.8",

    // No gamma correction
    "0",

    // Lighter
    "0.5", "1", "1.5", "2", "2.5", "3", "3.5", "4"
};

void MN_ResetGamma(void)
{
    I_SetPalette(W_CacheLumpName("PLAYPAL", PU_CACHE));
}

static setup_menu_t gen_settings1[] = {

    {"Resolution Scale", S_THERMO | S_THRM_SIZE11 | S_ACTION, CNTR_X,
     M_THRM_SPC, {"resolution_scale"}, .strings_id = str_resolution_scale,
     .action = ResetVideoHeight},

    {"Dynamic Resolution", S_ONOFF, CNTR_X, M_SPC, {"dynamic_resolution"},
     .action = ResetVideoHeight},

    {"Widescreen", S_CHOICE, CNTR_X, M_SPC, {"widescreen"},
     .strings_id = str_widescreen, .action = ResetVideo},

    {"Fullscreen", S_ONOFF, CNTR_X, M_SPC, {"fullscreen"},
     .action = ToggleFullScreen},

    MI_GAP_Y(6),

    {"Uncapped FPS", S_ONOFF, CNTR_X, M_SPC, {"uncapped"},
     .action = UpdateFPSLimit},

    {"Target FPS", S_NUM, CNTR_X, M_SPC, {"fpslimit"},
     .action = UpdateFPSLimit},

    {"VSync", S_ONOFF, CNTR_X, M_SPC, {"use_vsync"},
     .action = I_ToggleVsync},

    MI_GAP_Y(5),

    {"FOV", S_THERMO | S_THRM_SIZE11, CNTR_X, M_THRM_SPC, {"fov"},
     .action = UpdateFOV},

    {"Gamma Correction", S_THERMO, CNTR_X, M_THRM_SPC, {"gamma2"},
     .strings_id = str_gamma, .action = MN_ResetGamma},

    {"Extra Lighting", S_THERMO | S_STRICT, CNTR_X,
     M_THRM_SPC, {"extra_level_brightness"}},

    MI_RESET,

    MI_END
};

void MN_DisableResolutionScaleItem(void)
{
    DisableItem(true, gen_settings1, "resolution_scale");
}

static void UpdateSfxVolume(void)
{
    S_SetSfxVolume(snd_SfxVolume);
}

static void UpdateMusicVolume(void)
{
    S_SetMusicVolume(snd_MusicVolume);
}

static const char *sound_module_strings[] = {
    "Standard", "OpenAL 3D",
#if defined(HAVE_AL_BUFFER_CALLBACK)
    "PC Speaker"
#endif
};

static void SetSoundModule(void)
{
    if (!I_AllowReinitSound())
    {
        // The OpenAL implementation doesn't support the ALC_SOFT_HRTF extension
        // which is required for alcResetDeviceSOFT(). Warn the user to restart.
        warn_about_changes(S_PRGWARN);
        return;
    }

    S_EvictChannels();
    I_SetSoundModule();
}

static void SetMidiPlayer(void)
{
    S_StopMusic();
    I_SetMidiPlayer();
    S_SetMusicVolume(snd_MusicVolume);
    S_RestartMusic();
}

static void SetMidiPlayerOpl(void)
{
    if (I_MidiPlayerType() == midiplayer_opl)
    {
        SetMidiPlayer();
    }
}

#if defined(HAVE_FLUIDSYNTH)
static void SetMidiPlayerFluidSynth(void)
{
    if (I_MidiPlayerType() == midiplayer_fluidsynth)
    {
        SetMidiPlayer();
    }
}
#endif

static void RestartMusic(void)
{
    S_StopMusic();
    S_SetMusicVolume(snd_MusicVolume);
    S_RestartMusic();
}

static const char *extra_music_strings[] = {
    "Off", "Remix", "Original"
};

static void MN_Sfx(void);
static void MN_Music(void);
static void MN_Equalizer(void);

static setup_menu_t gen_settings2[] = {

    {"Sound Volume", S_THERMO, CNTR_X, M_THRM_SPC, {"sfx_volume"},
     .action = UpdateSfxVolume},

    {"Music Volume", S_THERMO, CNTR_X, M_THRM_SPC, {"music_volume"},
     .action = UpdateMusicVolume},

    MI_GAP,

    {"Sound Module", S_CHOICE, CNTR_X, M_SPC, {"snd_module"},
     .strings_id = str_sound_module, .action = SetSoundModule},

    {"Headphones Mode", S_ONOFF, CNTR_X, M_SPC, {"snd_hrtf"}, 
     .action = SetSoundModule},

    MI_GAP,

    {"Extra Soundtrack", S_CHOICE | S_ACTION, CNTR_X, M_SPC, {"extra_music"},
      .strings_id = str_extra_music, .action = RestartMusic},

    // [FG] music backend
    {"MIDI Player", S_CHOICE | S_ACTION | S_WRAP_LINE, CNTR_X, M_SPC * 2,
     {"midi_player_menu"}, .strings_id = str_midi_player,
     .action = SetMidiPlayer},

    MI_GAP,

    {"Sound Options", S_FUNC, CNTR_X, M_SPC, .action = MN_Sfx},

    {"Music Options", S_FUNC, CNTR_X, M_SPC, .action = MN_Music},

    {"Equalizer Options", S_FUNC, CNTR_X, M_SPC, .action = MN_Equalizer},

    MI_END
};

static setup_menu_t sfx_settings1[] = {

    {"SFX Channels", S_THERMO, CNTR_X, M_THRM_SPC, {"snd_channels"},
     .action = S_EvictChannels},

    {"Output Limiter", S_ONOFF, CNTR_X, M_SPC, {"snd_limiter"},
     .action = SetSoundModule},

    MI_GAP,

    {"Pitch-Shifting", S_ONOFF, CNTR_X, M_SPC, {"pitched_sounds"}},

    // [FG] play sounds in full length
    {"Disable Cutoffs", S_ONOFF, CNTR_X, M_SPC, {"full_sounds"}},

    {"Resampler", S_CHOICE, CNTR_X, M_SPC, {"snd_resampler"},
     .strings_id = str_resampler, .action = I_OAL_SetResampler},

    MI_GAP,

    {"Doppler Effect", S_THERMO | S_ACTION, CNTR_X, M_THRM_SPC, {"snd_doppler"},
     .strings_id = str_percent, .action = SetSoundModule},

    MI_END
};

static const char **GetResamplerStrings(void)
{
    const char **strings = I_OAL_GetResamplerStrings();
    DisableItem(!strings, sfx_settings1, "snd_resampler");
    return strings;
}

static setup_menu_t *sfx_settings[] = {sfx_settings1, NULL};

static setup_tab_t sfx_tabs[] = {{"Sound"}, {NULL}};

static void MN_Sfx(void)
{
    SetItemOn(set_item_on);
    SetPageIndex(current_page);

    MN_SetNextMenuAlt(ss_sfx);
    setup_screen = ss_sfx;
    current_page = GetPageIndex(sfx_settings);
    current_menu = sfx_settings[current_page];
    current_tabs = sfx_tabs;
    SetupMenuSecondary();
}

void MN_DrawSfx(void)
{
    DrawBackground("FLOOR4_6");
    MN_DrawTitle(M_X_CENTER, M_Y_TITLE, "M_GENERL", "General");
    DrawTabs();
    DrawInstructions();
    DrawScreenItems(current_menu);
}

static void UpdateGainItems(void);

static void ResetAutoGain(void)
{
    RestartMusic();
    UpdateGainItems();
}

static setup_menu_t music_settings1[] = {

    {"Auto Gain", S_ONOFF, CNTR_X, M_SPC, {"auto_gain"},
      .action = ResetAutoGain},

    MI_GAP,

#if defined (HAVE_FLUIDSYNTH)
    {"FluidSynth Gain", S_THERMO, CNTR_X, M_THRM_SPC, {"fl_gain"},
     .action = UpdateMusicVolume, .append = "dB"},

    {"FluidSynth Reverb", S_ONOFF, CNTR_X, M_SPC, {"fl_reverb"},
     .action = SetMidiPlayerFluidSynth},

    {"FluidSynth Chorus", S_ONOFF, CNTR_X, M_SPC, {"fl_chorus"},
     .action = SetMidiPlayerFluidSynth},

    MI_GAP,
#endif

    {"OPL3 Gain", S_THERMO, CNTR_X, M_THRM_SPC, {"opl_gain"},
     .action = UpdateMusicVolume, .append = "dB"},

    {"OPL3 Number of Chips", S_THERMO | S_THRM_SIZE4 | S_ACTION, CNTR_X,
     M_THRM_SPC, {"num_opl_chips"}, .action = SetMidiPlayerOpl},

    {"OPL3 Reverse Stereo", S_ONOFF, CNTR_X, M_SPC,
     {"opl_stereo_correct"}, .action = SetMidiPlayerOpl},

    MI_END
};

static void UpdateGainItems(void)
{
#if defined (HAVE_FLUIDSYNTH)
    DisableItem(auto_gain, music_settings1, "fl_gain");
#endif

    DisableItem(auto_gain, music_settings1, "opl_gain");
}

static setup_menu_t *music_settings[] = {music_settings1, NULL};

static setup_tab_t midi_tabs[] = {{"Music"}, {NULL}};

static void MN_Music(void)
{
    SetItemOn(set_item_on);
    SetPageIndex(current_page);

    MN_SetNextMenuAlt(ss_music);
    setup_screen = ss_music;
    current_page = GetPageIndex(music_settings);
    current_menu = music_settings[current_page];
    current_tabs = midi_tabs;
    SetupMenuSecondary();
}

void MN_DrawMidi(void)
{
    DrawBackground("FLOOR4_6");
    MN_DrawTitle(M_X_CENTER, M_Y_TITLE, "M_GENERL", "General");
    DrawTabs();
    DrawInstructions();
    DrawScreenItems(current_menu);
}

static const char *equalizer_preset_strings[] = {
    "Off", "Classical", "Rock", "Vocal", "Custom"
};

static setup_menu_t eq_settings1[] = {
    {"Preset", S_CHOICE, CNTR_X, M_SPC, {"snd_equalizer"},
     .strings_id = str_equalizer_preset, .action = I_OAL_EqualizerPreset},

    MI_GAP_Y(4),

    {"Preamp", S_THERMO, CNTR_X, M_THRM_SPC,
     {"snd_eq_preamp"}, .action = I_OAL_EqualizerPreset, .append = "dB"},

    MI_GAP_Y(4),

    {"Low Gain", S_THERMO, CNTR_X, M_THRM_SPC,
     {"snd_eq_low_gain"}, .action = I_OAL_EqualizerPreset, .append = "dB"},

    {"Mid 1 Gain", S_THERMO, CNTR_X, M_THRM_SPC,
     {"snd_eq_mid1_gain"}, .action = I_OAL_EqualizerPreset, .append = "dB"},

    {"Mid 2 Gain", S_THERMO, CNTR_X, M_THRM_SPC,
     {"snd_eq_mid2_gain"}, .action = I_OAL_EqualizerPreset, .append = "dB"},

    {"High Gain", S_THERMO, CNTR_X, M_THRM_SPC,
     {"snd_eq_high_gain"}, .action = I_OAL_EqualizerPreset, .append = "dB"},

    MI_GAP_Y(4),

    {"Low Cutoff", S_NUM, CNTR_X, M_SPC,
     {"snd_eq_low_cutoff"}, .action = I_OAL_EqualizerPreset, .append = "Hz"},

    {"Mid 1 Center", S_NUM, CNTR_X, M_SPC,
     {"snd_eq_mid1_center"}, .action = I_OAL_EqualizerPreset, .append = "Hz"},

    {"Mid 2 Center", S_NUM, CNTR_X, M_SPC,
     {"snd_eq_mid2_center"}, .action = I_OAL_EqualizerPreset, .append = "Hz"},

    {"High Cutoff", S_NUM, CNTR_X, M_SPC,
     {"snd_eq_high_cutoff"}, .action = I_OAL_EqualizerPreset, .append = "Hz"},

    MI_END
};

static setup_menu_t *eq_settings[] = {eq_settings1, NULL};

void MN_UpdateEqualizerItems(void)
{
    const boolean condition = !I_OAL_CustomEqualizer();

    DisableItem(!I_OAL_EqualizerInitialized(), gen_settings2, "Equalizer Options");
    DisableItem(!I_OAL_EqualizerInitialized(), eq_settings1, "snd_equalizer");
    DisableItem(condition, eq_settings1, "snd_eq_preamp");
    DisableItem(condition, eq_settings1, "snd_eq_low_gain");
    DisableItem(condition, eq_settings1, "snd_eq_low_cutoff");
    DisableItem(condition, eq_settings1, "snd_eq_mid1_gain");
    DisableItem(condition, eq_settings1, "snd_eq_mid1_center");
    DisableItem(condition, eq_settings1, "snd_eq_mid2_gain");
    DisableItem(condition, eq_settings1, "snd_eq_mid2_center");
    DisableItem(condition, eq_settings1, "snd_eq_high_gain");
    DisableItem(condition, eq_settings1, "snd_eq_high_cutoff");
}

static setup_tab_t equalizer_tabs[] = {{"Equalizer"}, {NULL}};

static void MN_Equalizer(void)
{
    SetItemOn(set_item_on);
    SetPageIndex(current_page);

    MN_SetNextMenuAlt(ss_eq);
    setup_screen = ss_eq;
    current_page = GetPageIndex(eq_settings);
    current_menu = eq_settings[current_page];
    current_tabs = equalizer_tabs;
    SetupMenuSecondary();
}

void MN_DrawEqualizer(void)
{
    DrawBackground("FLOOR4_6");
    MN_DrawTitle(M_X_CENTER, M_Y_TITLE, "M_GENERL", "General");
    DrawTabs();
    DrawInstructions();
    DrawScreenItems(current_menu);
}

typedef enum
{
  FREELOOK_OFF,         // Free look disabled.
  FREELOOK_AUTO_AIM,    // Free look enabled with traditional auto-aiming.
  FREELOOK_DIRECT_AIM,  // Free look enabled with direct vertical aiming.
} freelook_mode_t;

static freelook_mode_t freelook_mode;

static const char *free_look_strings[] = {"Off", "Auto-Aim", "Direct Aim"};

void MN_UpdateFreeLook(void)
{
    P_UpdateDirectVerticalAiming();

    if (!freelook)
    {
        for (int i = 0; i < MAXPLAYERS; ++i)
        {
            if (playeringame[i])
            {
                players[i].centering = true;
            }
        }
    }
}

static void UpdateFreeLookMode(void)
{
    switch (freelook_mode)
    {
        case FREELOOK_OFF:
            freelook = false;
            default_direct_vertical_aiming = false;
            break;
        case FREELOOK_AUTO_AIM:
            freelook = true;
            default_direct_vertical_aiming = false;
            break;
        case FREELOOK_DIRECT_AIM:
            freelook = true;
            default_direct_vertical_aiming = true;
            break;
    }

    MN_UpdateFreeLook();
}

#define MOUSE_ACCEL_STRINGS_SIZE (40 + 1)

static const char **GetMouseAccelStrings(void)
{
    static const char *strings[MOUSE_ACCEL_STRINGS_SIZE];
    char buf[8];

    strings[0] = "Off";

    for (int i = 1; i < MOUSE_ACCEL_STRINGS_SIZE; ++i)
    {
        int val = i + 10;
        M_snprintf(buf, sizeof(buf), "%1d.%1d", val / 10, val % 10);
        strings[i] = M_StringDuplicate(buf);
    }
    return strings;
}

static setup_menu_t gen_settings3[] = {
    {"Turn Sensitivity", S_THERMO | S_THRM_SIZE11, CNTR_X, M_THRM_SPC,
     {"mouse_sensitivity"}, .action = G_UpdateMouseVariables},

    {"Look Sensitivity", S_THERMO | S_THRM_SIZE11, CNTR_X, M_THRM_SPC,
     {"mouse_sensitivity_y_look"}, .action = G_UpdateMouseVariables},

    {"Move Sensitivity", S_THERMO | S_THRM_SIZE11, CNTR_X, M_THRM_SPC,
     {"mouse_sensitivity_y"}, .action = G_UpdateMouseVariables},

    {"Strafe Sensitivity", S_THERMO | S_THRM_SIZE11, CNTR_X, M_THRM_SPC,
     {"mouse_sensitivity_strafe"}, .action = G_UpdateMouseVariables},

    MI_GAP,

    {"Acceleration", S_THERMO, CNTR_X, M_THRM_SPC, {"mouse_acceleration"},
     .strings_id = str_mouse_accel, .action = G_UpdateMouseVariables},

    MI_GAP,

    // [FG] double click to "use"
    {"Double-Click to \"Use\"", S_ONOFF, CNTR_X, M_SPC, {"dclick_use"}},

    {"Free Look", S_CHOICE, CNTR_X, M_SPC, {"freelook_mode"},
     .strings_id = str_freelook, .action = UpdateFreeLookMode},

    // [FG] invert vertical axis
    {"Invert Look", S_ONOFF, CNTR_X, M_SPC, {"mouse_y_invert"},
     .action = G_UpdateMouseVariables},

    MI_END
};

static void UpdateGamepadItems(void);

static void UpdateGamepad(void)
{
    UpdateGamepadItems();
    I_ResetGamepad();
}

static const char *layout_strings[] = {
    "Off",
    "Default",
    "Southpaw",
    "Legacy",
    "Legacy Southpaw",
    "Flick Stick",
    "Flick Stick Southpaw",
};

static void UpdateRumble(void)
{
    I_UpdateRumbleEnabled();
    I_RumbleMenuFeedback();
}

static const char *percent_strings[] = {
    "Off", "10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%", "100%"
};

static const char *curve_strings[] = {
    "",       "",    "",    "",        "",    "",    "",
    "",       "",    "", // Dummy values, start at 1.0.
    "Linear", "1.1", "1.2", "1.3",     "1.4", "1.5", "1.6",
    "1.7",    "1.8", "1.9", "Squared", "2.1", "2.2", "2.3",
    "2.4",    "2.5", "2.6", "2.7",     "2.8", "2.9", "Cubed"
};

static void MN_PadAdv(void);
static void MN_Gyro(void);

static void UpdateGamepadDevice(void)
{
    I_UpdateGamepadDevice(menu_input == pad_mode);
}

static setup_menu_t gen_settings4[] = {

    {"Device", S_CHOICE | S_ACTION | S_WRAP_LINE, CNTR_X, M_SPC * 2,
     {"joy_device"}, .strings_id = str_gamepad_device,
     .action = UpdateGamepadDevice},

    MI_GAP_Y(1),

    {"Turn Speed", S_THERMO | S_THRM_SIZE11, CNTR_X, M_THRM_SPC,
     {"joy_turn_speed"}, .action = I_ResetGamepad},

    {"Look Speed", S_THERMO | S_THRM_SIZE11, CNTR_X, M_THRM_SPC,
     {"joy_look_speed"}, .action = I_ResetGamepad},

    MI_GAP_Y(2),

    {"Free Look", S_CHOICE, CNTR_X, M_SPC, {"freelook_mode"},
     .strings_id = str_freelook, .action = UpdateFreeLookMode},

    {"Invert Look", S_ONOFF, CNTR_X, M_SPC, {"joy_invert_look"},
     .action = I_ResetGamepad},

    MI_GAP_Y(2),

    {"Movement Deadzone", S_THERMO | S_PCT, CNTR_X, M_THRM_SPC,
     {"joy_movement_inner_deadzone"}, .action = I_ResetGamepad},

    {"Camera Deadzone", S_THERMO | S_PCT, CNTR_X, M_THRM_SPC,
     {"joy_camera_inner_deadzone"}, .action = I_ResetGamepad},

    {"Rumble", S_THERMO, CNTR_X, M_THRM_SPC, {"joy_rumble"},
     .strings_id = str_percent, .action = UpdateRumble},

    MI_GAP_Y(2),

    {"Advanced Options", S_FUNC, CNTR_X, M_SPC, .action = MN_PadAdv},

    {"Gyro Options", S_FUNC, CNTR_X, M_SPC, .action = MN_Gyro},

    MI_END
};

void MN_InitFreeLook(void)
{
    if (strictmode)
    {
        DisableItem(true, gen_settings3, "freelook_mode");
        DisableItem(true, gen_settings4, "freelook_mode");
    }
    if (demorecording || (netgame && !solonet))
    {
        SetItemLimit(gen_settings3, "freelook_mode", FREELOOK_OFF,
                     FREELOOK_AUTO_AIM);
        SetItemLimit(gen_settings4, "freelook_mode", FREELOOK_OFF,
                     FREELOOK_AUTO_AIM);
    }
}

static const char *movement_type_strings[] = {
    "Normalized", "Faster Diagonals"
};

#define MOVEMENT_SENSITIVITY_STRINGS_SIZE (40 + 1)

static const char **GetMovementSensitivityStrings(void)
{
    static const char *strings[MOVEMENT_SENSITIVITY_STRINGS_SIZE];
    char buf[8];

    for (int i = 0; i < MOVEMENT_SENSITIVITY_STRINGS_SIZE; i++)
    {
        M_snprintf(buf, sizeof(buf), "%1d.%1d", i / 10, i % 10);
        strings[i] = M_StringDuplicate(buf);
    }
    return strings;
}

#define MS_TIME_STRINGS_SIZE (100 + 1)

static const char **GetMsTimeStrings(void)
{
    static const char *strings[MS_TIME_STRINGS_SIZE];
    char buf[8];

    for (int i = 0; i < MS_TIME_STRINGS_SIZE; ++i)
    {
        M_snprintf(buf, sizeof(buf), "%d ms", i * 10);
        strings[i] = M_StringDuplicate(buf);
    }
    return strings;
}

static const char *flick_snap_strings[] = {"Off", "4-Way", "8-Way"};

static setup_menu_t padadv_settings1[] = {

    {"Stick Layout", S_CHOICE, CNTR_X, M_SPC, {"joy_stick_layout"},
     .strings_id = str_layout, .action = UpdateGamepad},

    {"Flick Time", S_THERMO, CNTR_X, M_THRM_SPC, {"joy_flick_time"},
     .strings_id = str_ms_time, .action = I_ResetGamepad},

    MI_GAP,

    {"Movement Type", S_CHOICE, CNTR_X, M_SPC, {"joy_movement_type"},
     .strings_id = str_movement_type, .action = I_ResetGamepad},

    {"Forward Sensitivity", S_THERMO, CNTR_X, M_THRM_SPC,
     {"joy_forward_sensitivity"}, .strings_id = str_movement_sensitivity,
     .action = I_ResetGamepad},

    {"Strafe Sensitivity", S_THERMO, CNTR_X, M_THRM_SPC,
     {"joy_strafe_sensitivity"}, .strings_id = str_movement_sensitivity,
     .action = I_ResetGamepad},

    MI_GAP,

    {"Extra Turn Speed", S_THERMO | S_THRM_SIZE11, CNTR_X, M_THRM_SPC,
     {"joy_outer_turn_speed"}, .action = UpdateGamepad},

    {"Extra Ramp Time", S_THERMO, CNTR_X, M_THRM_SPC, {"joy_outer_ramp_time"},
     .strings_id = str_ms_time, .action = I_ResetGamepad},

    {"Response Curve", S_THERMO, CNTR_X, M_THRM_SPC, {"joy_camera_curve"},
     .strings_id = str_curve, .action = I_ResetGamepad},

    MI_END
};

static setup_menu_t *padadv_settings[] = {padadv_settings1, NULL};

static setup_tab_t padadv_tabs[] = {{"Advanced"}, {NULL}};

static void MN_PadAdv(void)
{
    SetItemOn(set_item_on);
    SetPageIndex(current_page);

    MN_SetNextMenuAlt(ss_padadv);
    setup_screen = ss_padadv;
    current_page = GetPageIndex(padadv_settings);
    current_menu = padadv_settings[current_page];
    current_tabs = padadv_tabs;
    SetupMenuSecondary();
}

void MN_DrawPadAdv(void)
{
    DrawBackground("FLOOR4_6");
    MN_DrawTitle(M_X_CENTER, M_Y_TITLE, "M_GENERL", "General");
    DrawTabs();
    DrawInstructions();
    DrawScreenItems(current_menu);
}

static void UpdateGamepadItems(void)
{
    const boolean devices = (I_GamepadEnabled() && I_GamepadDevices());
    const boolean gamepad = (I_UseGamepad() && devices);
    const boolean gyro = (I_GyroEnabled() && I_GyroSupported());
    const boolean sticks = I_UseStickLayout();
    const boolean flick = (gamepad && sticks && !I_StandardLayout());
    const boolean ramp = (gamepad && sticks && I_RampTimeEnabled());
    const boolean condition = (!gamepad || !sticks);

    DisableItem(!devices, gen_settings4, "joy_device");
    DisableItem(!gamepad, gen_settings4, "Advanced Options");
    DisableItem(!gamepad || !I_GyroSupported(), gen_settings4, "Gyro Options");
    DisableItem(!gamepad || !I_RumbleSupported(), gen_settings4, "joy_rumble");
    DisableItem(!gamepad || (!sticks && !gyro) || strictmode, gen_settings4,
        "freelook_mode");
    DisableItems(condition, gen_settings4, "joy_invert_look",
        "joy_movement_inner_deadzone", "joy_camera_inner_deadzone",
        "joy_turn_speed", "joy_look_speed");

    DisableItem(!gamepad, padadv_settings1, "joy_stick_layout");
    DisableItem(!flick, padadv_settings1, "joy_flick_time");
    DisableItems(condition, padadv_settings1, "joy_movement_type",
        "joy_forward_sensitivity", "joy_strafe_sensitivity",
        "joy_outer_turn_speed");
    DisableItem(!ramp, padadv_settings1, "joy_outer_ramp_time");
    DisableItem(condition, padadv_settings1, "joy_camera_curve");
}

static void UpdateGyroItems(void);

static void UpdateGyroAiming(void)
{
    UpdateGamepadItems(); // Update "Gyro Options" and freelook.
    UpdateGyroItems();
    I_SetSensorsEnabled(I_GyroEnabled());
    I_ResetGamepad();
}

static const char *gyro_space_strings[] = {"Local", "Player"};

static const char *gyro_action_strings[] = {
    "None",
    "Disable Gyro",
    "Enable Gyro",
    "Invert Gyro",
    "Reset Camera",
    "Reset / Disable Gyro",
    "Reset / Enable Gyro",
    "Reset / Invert Gyro"
};

#define GYRO_SENS_STRINGS_SIZE (500 + 1)

static const char **GetGyroSensitivityStrings(void)
{
    static const char *strings[GYRO_SENS_STRINGS_SIZE];
    char buf[8];

    for (int i = 0; i < GYRO_SENS_STRINGS_SIZE; i++)
    {
        M_snprintf(buf, sizeof(buf), "%1d.%1d", i / 10, i % 10);
        strings[i] = M_StringDuplicate(buf);
    }
    return strings;
}

#define GYRO_ACCEL_STRINGS_SIZE (200 + 1)

static const char **GetGyroAccelStrings(void)
{
    static const char *strings[GYRO_ACCEL_STRINGS_SIZE] = {
        "", "", "", "", "", "", "", "", "", "", "Off"
    };
    char buf[8];

    for (int i = 11; i < GYRO_ACCEL_STRINGS_SIZE; i++)
    {
        M_snprintf(buf, sizeof(buf), "%1d.%1d", i / 10, i % 10);
        strings[i] = M_StringDuplicate(buf);
    }
    return strings;
}

static void UpdateGyroAcceleration(void)
{
    UpdateGyroItems();
    I_ResetGamepad();
}

static void UpdateGyroSteadying(void)
{
    I_UpdateGyroSteadying();
    I_ResetGamepad();
}

static setup_menu_t gyro_settings1[] = {

    {"Gyro Aiming", S_ONOFF, CNTR_X, M_SPC, {"gyro_enable"},
     .action = UpdateGyroAiming},

    {"Gyro Space", S_CHOICE, CNTR_X, M_SPC, {"gyro_space"},
     .strings_id = str_gyro_space, .action = I_ResetGamepad},

    {"Gyro Button Action", S_CHOICE, CNTR_X, M_SPC, {"gyro_button_action"},
     .strings_id = str_gyro_action, .action = I_ResetGamepad},

    {"Camera Stick Action", S_CHOICE, CNTR_X, M_SPC, {"gyro_stick_action"},
     .strings_id = str_gyro_action, .action = I_ResetGamepad},

    {"Turn Sensitivity", S_THERMO | S_THRM_SIZE11, CNTR_X, M_THRM_SPC,
     {"gyro_turn_sensitivity"}, .strings_id = str_gyro_sens,
     .action = I_ResetGamepad},

    {"Look Sensitivity", S_THERMO | S_THRM_SIZE11, CNTR_X, M_THRM_SPC,
     {"gyro_look_sensitivity"}, .strings_id = str_gyro_sens,
     .action = I_ResetGamepad},

    {"Acceleration", S_THERMO | S_THRM_SIZE11, CNTR_X, M_THRM_SPC,
     {"gyro_acceleration"}, .strings_id = str_gyro_accel,
     .action = UpdateGyroAcceleration},

    {"Lower Threshold", S_THERMO | S_THRM_SIZE11, CNTR_X, M_THRM_SPC,
     {"gyro_accel_min_threshold"}, .action = I_ResetGamepad},

    {"Upper Threshold", S_THERMO | S_THRM_SIZE11, CNTR_X, M_THRM_SPC,
     {"gyro_accel_max_threshold"}, .action = I_ResetGamepad},

    {"Steadying", S_THERMO | S_THRM_SIZE11, CNTR_X, M_THRM_SPC,
     {"gyro_smooth_threshold"}, .strings_id = str_gyro_sens,
     .action = UpdateGyroSteadying},

    MI_GAP_Y(2),

    {"Calibrate", S_FUNC, CNTR_X, M_SPC,
     .action = I_UpdateGyroCalibrationState,
     .desc = "Place gamepad on a flat surface"},

    MI_END
};

static setup_menu_t *gyro_settings[] = {gyro_settings1, NULL};

static void UpdateGyroItems(void)
{
    const boolean gamepad = (I_UseGamepad() && I_GamepadEnabled());
    const boolean gyro = (I_GyroEnabled() && I_GyroSupported());
    const boolean acceleration = (gamepad && gyro && I_GyroAcceleration());
    const boolean condition = (!gamepad || !gyro);

    DisableItem(!gamepad || !I_GyroSupported(), gyro_settings1, "gyro_enable");
    DisableItem(condition, gyro_settings1, "gyro_space");
    DisableItem(condition, gyro_settings1, "gyro_button_action");
    DisableItem(condition, gyro_settings1, "gyro_stick_action");
    DisableItem(condition, gyro_settings1, "gyro_turn_sensitivity");
    DisableItem(condition, gyro_settings1, "gyro_look_sensitivity");
    DisableItem(condition, gyro_settings1, "gyro_acceleration");
    DisableItem(!acceleration, gyro_settings1, "gyro_accel_min_threshold");
    DisableItem(!acceleration, gyro_settings1, "gyro_accel_max_threshold");
    DisableItem(condition, gyro_settings1, "gyro_smooth_threshold");
    DisableItem(condition, gyro_settings1, "Calibrate");
}

static setup_tab_t gyro_tabs[] = {{"Gyro"}, {NULL}};

static void MN_Gyro(void)
{
    SetItemOn(set_item_on);
    SetPageIndex(current_page);

    MN_SetNextMenuAlt(ss_gyro);
    setup_screen = ss_gyro;
    current_page = GetPageIndex(gyro_settings);
    current_menu = gyro_settings[current_page];
    current_tabs = gyro_tabs;
    SetupMenuSecondary();
}

void MN_DrawGyro(void)
{
    DrawBackground("FLOOR4_6");
    MN_DrawTitle(M_X_CENTER, M_Y_TITLE, "M_GENERL", "General");
    DrawTabs();
    DrawInstructions();

    if (I_UseGamepad() && I_GyroEnabled())
    {
        DrawIndicator = DrawIndicator_Meter;
    }
    else
    {
        DrawIndicator = NULL;
    }

    DrawScreenItems(current_menu);
    DrawGyroCalibration();
}

static void SmoothLight(void)
{
    setsmoothlight = true;
    setsizeneeded = true; // run R_ExecuteSetViewSize
}

static const char *fuzzmode_strings[] = {
    "Blocky", "Refraction", "Shadow", "Original"
};

static setup_menu_t gen_settings5[] = {

    {"Smooth Pixel Scaling", S_ONOFF, OFF_CNTR_X, M_SPC, {"smooth_scaling"},
     .action = ResetVideo},

    {"Sprite Translucency", S_ONOFF | S_STRICT, OFF_CNTR_X, M_SPC,
     {"translucency"}},

    {"Partial Invisibility", S_CHOICE | S_STRICT, OFF_CNTR_X, M_SPC, {"fuzzmode"},
     .strings_id = str_fuzzmode, .action = R_SetFuzzColumnMode},

    MI_GAP,

    {"Voxels", S_ONOFF | S_STRICT, OFF_CNTR_X, M_SPC, {"voxels_rendering"}},

    {"Brightmaps", S_ONOFF | S_STRICT, OFF_CNTR_X, M_SPC, {"brightmaps"}},

    {"Stretch Short Skies", S_ONOFF, OFF_CNTR_X, M_SPC, {"stretchsky"},
     .action = R_UpdateStretchSkies},

    {"Linear Sky Scrolling", S_ONOFF, OFF_CNTR_X, M_SPC, {"linearsky"},
     .action = R_InitPlanes},

    {"Swirling Flats", S_ONOFF, OFF_CNTR_X, M_SPC, {"r_swirl"}},

    {"Smooth Diminishing Lighting", S_ONOFF, OFF_CNTR_X, M_SPC, {"smoothlight"},
     .action = SmoothLight},

    MI_END
};

static const char *death_use_action_strings[] = {"default", "last save",
                                                 "nothing"};

static const char *screen_melt_strings[] = {"Off", "Melt", "Crossfade", "Fizzle"};

static const char *invul_mode_strings[] = {"Vanilla", "MBF", "Gray"};

static const char *endoom_strings[] = {"Off", "PWAD Only", "Always"};

static setup_menu_t gen_settings6[] = {

    {"Quality of life", S_SKIP | S_TITLE, OFF_CNTR_X, M_SPC},

    {"Screen wipe effect", S_CHOICE | S_STRICT, OFF_CNTR_X, M_SPC,
     {"screen_melt"}, .strings_id = str_screen_melt},

    {"Pain/Pickup/Powerup flashes", S_ONOFF | S_STRICT, OFF_CNTR_X, M_SPC,
     {"palette_changes"}},

    {"Invulnerability effect", S_CHOICE | S_STRICT, OFF_CNTR_X, M_SPC,
     {"invul_mode"}, .strings_id = str_invul_mode, .action = R_InvulMode},

    {"Pause Demos in Menu", S_ONOFF, OFF_CNTR_X, M_SPC, {"menu_pause_demos"}},

    {"Demo progress bar", S_ONOFF, OFF_CNTR_X, M_SPC, {"demobar"}},

    {"On death action", S_CHOICE, OFF_CNTR_X, M_SPC, {"death_use_action"},
     .strings_id = str_death_use_action},

    {"Auto save", S_ONOFF, OFF_CNTR_X, M_SPC, {"autosave"},
     .action = M_ResetAutoSave},

    {"Organize save files", S_ONOFF | S_PRGWARN, OFF_CNTR_X, M_SPC,
     {"organize_savefiles"}, .action = D_SetSavegameDirectory},

    MI_GAP,

    {"Miscellaneous", S_SKIP | S_TITLE, OFF_CNTR_X, M_SPC},

    {"Game speed", S_NUM | S_STRICT | S_PCT, OFF_CNTR_X, M_SPC,
     {"realtic_clock_rate"}, .action = G_SetTimeScale},

    {"Show Quit Prompt", S_ONOFF, OFF_CNTR_X, M_SPC, {"quit_prompt"}},

    {"Play Quit Sound", S_ONOFF, OFF_CNTR_X, M_SPC, {"quit_sound"}},

    {"Show ENDOOM Screen", S_CHOICE, OFF_CNTR_X, M_SPC, {"show_endoom"},
     .strings_id = str_endoom},

    MI_END
};

static setup_menu_t *gen_settings[] = {
    gen_settings1, gen_settings2, gen_settings3, gen_settings4,
    gen_settings5, gen_settings6, NULL
};

void MN_UpdateDynamicResolutionItem(void)
{
    DisableItem(current_video_height <= DRS_MIN_HEIGHT, gen_settings1,
                "dynamic_resolution");
}

void MN_UpdateAdvancedSoundItems(boolean toggle)
{
    DisableItem(toggle, gen_settings2, "snd_hrtf");
    DisableItem(toggle, sfx_settings1, "snd_doppler");
}

void MN_UpdateFpsLimitItem(void)
{
    DisableItem(!uncapped, gen_settings1, "fpslimit");
    G_ClearInput();
    G_UpdateAngleFunctions();
}

void MN_DisableVoxelsRenderingItem(void)
{
    DisableItem(true, gen_settings5, "voxels_rendering");
}

void MN_Trans(void) // To reset translucency after setting it in menu
{
    R_InitTranMap(0);
}

// Setting up for the General screen. Turn on flags, set pointers,
// locate the first item on the screen where the cursor is allowed to
// land.

void MN_General(int choice)
{
    MN_SetNextMenuAlt(ss_gen);
    setup_screen = ss_gen;
    current_page = GetPageIndex(gen_settings);
    current_menu = gen_settings[current_page];
    current_tabs = gen_tabs;
    SetupMenu();

    if (freelook && direct_vertical_aiming)
    {
        freelook_mode = FREELOOK_DIRECT_AIM;
    }
    else if (freelook && !direct_vertical_aiming)
    {
        freelook_mode = FREELOOK_AUTO_AIM;
    }
    else
    {
        freelook_mode = FREELOOK_OFF;
    }
}

// The drawing part of the General Setup initialization. Draw the
// background, title, instruction line, and items.

void MN_DrawGeneral(void)
{
    DrawBackground("FLOOR4_6"); // Draw background
    MN_DrawTitle(M_X_CENTER, M_Y_TITLE, "M_GENERL", "General");
    DrawTabs();
    DrawInstructions();

    if (I_UseGamepad() && current_menu == gen_settings4 && I_UseStickLayout())
    {
        DrawIndicator = DrawIndicator_Meter;
    }
    else
    {
        DrawIndicator = NULL;
    }

    DrawScreenItems(current_menu);

    // If the Reset Button has been selected, an "Are you sure?" message
    // is overlayed across everything else.

    if (default_verify)
    {
        DrawDefVerify();
    }
}

static boolean csmenu_skill;

static struct
{
    boolean fastparm;
    boolean respawnparm;
    boolean nomonsters;
    boolean coopspawns;
    boolean pistolstart;
    boolean halfplayerdamage;
    boolean doubleammo;
    boolean aggromonsters;    
} csmenu;

const char *skill_strings[] = {
    "I'm too young to die", "Hey, not too rough", "Hurt me plenty",
    "Ultra-Violence", "NIGHTMARE!",
};

static void SelectSkillLevel(void);

static void StartGame(void)
{
    clfastparm = csmenu.fastparm;
    clrespawnparm = csmenu.respawnparm;
    clnomonsters = csmenu.nomonsters;
    clcoopspawns = csmenu.coopspawns;
    clpistolstart = csmenu.pistolstart;
    cshalfplayerdamage = csmenu.halfplayerdamage;
    csdoubleammo = csmenu.doubleammo;
    csaggromonsters = csmenu.aggromonsters;

    M_ChooseSkill(csmenu_skill);
    setup_active = false;
}

static setup_menu_t customskill_settings1[] = {
    MI_GAP_Y(10),
    {"Skill level", S_CHOICE, CNTR_X, M_SPC, {"csmenu_skill"},
     .strings_id = str_skill, .action = SelectSkillLevel},
    {"Half damage", S_ONOFF, CNTR_X, M_SPC, {"csmenu.halfplayerdamage"}},
    {"Double ammo", S_ONOFF, CNTR_X, M_SPC, {"csmenu.doubleammo"}},
    {"Fast monsters", S_ONOFF, CNTR_X, M_SPC, {"csmenu.fastparm"}},
    {"Aggressive monsters", S_ONOFF, CNTR_X, M_SPC, {"csmenu.aggromonsters"}},
    {"Respawn monsters", S_ONOFF, CNTR_X, M_SPC, {"csmenu.respawnparm"}},
    MI_GAP,
    {"No monsters", S_ONOFF, CNTR_X, M_SPC, {"csmenu.nomonsters"}},
    {"Co-op spawns", S_ONOFF, CNTR_X, M_SPC, {"csmenu.coopspawns"}},
    {"Pistol start", S_ONOFF, CNTR_X, M_SPC, {"csmenu.pistolstart"}},
    MI_GAP,
    {"Start Game", S_CENTER, 0, M_SPC, .action = StartGame},
    MI_END
};

static void SelectSkillLevel(void)
{
    memset(&csmenu, 0, sizeof(csmenu));

    switch (csmenu_skill)
    {
        case sk_baby:
            csmenu.halfplayerdamage = true;
            csmenu.doubleammo = true;
            break;
        case sk_nightmare:
            csmenu.doubleammo = true;
            csmenu.aggromonsters = true;
            csmenu.fastparm = true;
            csmenu.respawnparm = true;
            break;
        default:
            break;
    }

    DisableItem(csmenu_skill == sk_baby, customskill_settings1,
                "csmenu.halfplayerdamage");
    DisableItem(csmenu_skill == sk_baby || csmenu_skill == sk_nightmare,
                customskill_settings1, "csmenu.doubleammo");
    DisableItems(csmenu_skill == sk_nightmare, customskill_settings1,
                 "csmenu.aggromonsters", "csmenu.fastparm",
                 "csmenu.respawnparm");
}

static setup_menu_t *customskill_settings[] = {customskill_settings1, NULL};

void MN_CustomSkill(void)
{
    MN_SetNextMenuAlt(ss_cskill);
    setup_screen = ss_cskill;
    current_page = 0;
    current_menu = customskill_settings[current_page];
    current_tabs = NULL;
    SetupMenu();

    csmenu_skill = default_skill - 1;
    SelectSkillLevel();
}

void MN_DrawCustomSkill(void)
{
    const char *title = "Custom Skill";
    MN_DrawString((SCREENWIDTH - MN_GetPixelWidth(title)) / 2, 30, CR_GOLD,
                  title);
    DrawInstructions();
    DrawScreenItems(current_menu);
}

/////////////////////////////
//
// General routines used by the Setup screens.
//

// phares 4/17/98:
// M_SelectDone() gets called when you have finished entering your
// Setup Menu item change.

static void SelectDone(setup_menu_t *ptr)
{
    ptr->m_flags &= ~S_SELECT;
    ptr->m_flags |= S_HILITE;
    M_StartSound(sfx_itemup);
    setup_select = false;
    if (print_warning_about_changes) // killough 8/15/98
    {
        print_warning_about_changes--;
    }
}

// phares 4/21/98:
// Array of setup screens used by M_ResetDefaults()

static setup_menu_t **setup_screens[] = {
    keys_settings,
    weap_settings,
    stat_settings,
    auto_settings,
    enem_settings,
    gen_settings, // killough 10/98
    comp_settings,
    sfx_settings,
    music_settings,
    eq_settings,
    padadv_settings,
    gyro_settings,
    customskill_settings,
};

// [FG] save the index of the current screen in the first page's S_END element's
// y coordinate

static int GetPageIndex(setup_menu_t *const *const pages)
{
    if (pages)
    {
        const setup_menu_t *menu = pages[0];

        if (menu)
        {
            while (!(menu->m_flags & S_END))
            {
                menu++;
            }

            return menu->m_y;
        }
    }

    return 0;
}

static void SetPageIndex(const int y)
{
    setup_menu_t *menu = setup_screens[setup_screen][0];

    while (!(menu->m_flags & S_END))
    {
        menu++;
    }

    menu->m_y = y;
}

// phares 4/19/98:
// M_ResetDefaults() resets all values for a setup screen to default values
//
// killough 10/98: rewritten to fix bugs and warn about pending changes

static void ResetDefaults(ss_types reset_screen)
{
    default_t *dp;
    int warn = 0;

    default_reset = true; // needed to propely reset some dynamic items

    // Look through the defaults table and reset every variable that
    // belongs to the group we're interested in.
    //
    // killough: However, only reset variables whose field in the
    // current setup screen is the same as in the defaults table.
    // i.e. only reset variables really in the current setup screen.

    for (dp = defaults; dp->name; dp++)
    {
        if (dp->setupscreen != reset_screen)
        {
            continue;
        }

        setup_menu_t **screens = setup_screens[reset_screen];

        for (; *screens; screens++)
        {
            setup_menu_t *current_item = *screens;

            for (; !(current_item->m_flags & S_END); current_item++)
            {
                int flags = current_item->m_flags;

                if (flags & S_HASDEFPTR && current_item->var.def == dp)
                {
                    if (dp->type == string)
                    {
                        free(*dp->location.s);
                        *dp->location.s = strdup(dp->defaultvalue.string);
                    }
                    else if (dp->type == number)
                    {
                        *dp->location.i = dp->defaultvalue.number;
                    }

                    if (flags & (S_LEVWARN | S_PRGWARN))
                    {
                        warn |= flags & (S_LEVWARN | S_PRGWARN);
                    }
                    else if (dp->type == string && dp->current.s)
                    {
                        *dp->current.s = *dp->location.s;
                    }
                    else if (dp->type == number && dp->current.i)
                    {
                        *dp->current.i = *dp->location.i;
                    }

                    if (current_item->action)
                    {
                        current_item->action();
                    }
                }
                else if (current_item->input_id == dp->input_id)
                {
                    M_InputSetDefault(dp->input_id);
                }
            }
        }
    }

    default_reset = false;

    if (warn)
    {
        warn_about_changes(warn);
    }
}

static void ResetDefaultsSecondary(void)
{
    if (setup_screen == ss_gen)
    {
        ResetDefaults(ss_sfx);
        ResetDefaults(ss_music);
        ResetDefaults(ss_eq);
        ResetDefaults(ss_padadv);
        ResetDefaults(ss_gyro);
    }
}

//
// M_InitDefaults()
//
// killough 11/98:
//
// This function converts all setup menu entries consisting of cfg
// variable names, into pointers to the corresponding default[]
// array entry. var.name becomes converted to var.def.
//

void MN_InitDefaults(void)
{
    for (int i = 0; i < ss_max; i++)
    {
        setup_menu_t **screens = setup_screens[i];
        for (; *screens; screens++)
        {
            setup_menu_t *current_item = *screens;
            for (; !(current_item->m_flags & S_END); current_item++)
            {
                if (current_item->m_flags & S_HASDEFPTR)
                {
                    default_t *dp = M_LookupDefault(current_item->var.name);
                    if (!dp)
                    {
                        I_Error("Could not find config variable \"%s\"",
                                current_item->var.name);
                    }
                    else
                    {
                        current_item->var.def = dp;
                        current_item->var.def->setup_menu = current_item;
                    }
                }
            }
        }
    }
}

//
// End of Setup Screens.
//
/////////////////////////////////////////////////////////////////////////////

static int M_GetKeyString(int c, int offset)
{
    const char *s;

    if (c >= 33 && c <= 126)
    {
        // The '=', ',', and '.' keys originally meant the shifted
        // versions of those keys, but w/o having to shift them in
        // the game. Any actions that are mapped to these keys will
        // still mean their shifted versions. Could be changed later
        // if someone can come up with a better way to deal with them.

        if (c == '=') // probably means the '+' key?
        {
            c = '+';
        }
        else if (c == ',') // probably means the '<' key?
        {
            c = '<';
        }
        else if (c == '.') // probably means the '>' key?
        {
            c = '>';
        }
        menu_buffer[offset++] = c; // Just insert the ascii key
        menu_buffer[offset] = 0;
    }
    else
    {
        s = M_GetNameForKey(c);
        if (!s)
        {
            s = "JUNK";
        }
        strcpy(&menu_buffer[offset], s); // string to display
        offset += strlen(s);
    }

    return offset;
}

static int kerning = 0;

void MN_DrawStringCR(int cx, int cy, byte *cr1, byte *cr2, const char *ch)
{
    int w;
    int c;

    byte *cr = cr1;

    while (*ch)
    {
        c = *ch++; // get next char

        if (c == '\x1b' && *ch)
        {
            c = *ch++;
            if (c >= '0' && c <= '0' + CR_NONE)
            {
                cr = colrngs[c - '0'];
            }
            else if (c == '0' + CR_ORIG)
            {
                cr = cr1;
            }
            continue;
        }

        c = M_ToUpper(c) - HU_FONTSTART;
        if (c < 0 || c >= HU_FONTSIZE || hu_font[c] == NULL)
        {
            cx += SPACEWIDTH; // space
            continue;
        }

        w = SHORT(hu_font[c]->width);
        if (cx + w > SCREENWIDTH)
        {
            break;
        }

        // V_DrawpatchTranslated() will draw the string in the
        // desired color, colrngs[color]
        if (cr && cr2)
        {
            V_DrawPatchTRTR(cx, cy, (crop_t){0}, hu_font[c], cr, cr2);
        }
        else
        {
            V_DrawPatchTranslated(cx, cy, hu_font[c], cr);
        }

        // The screen is cramped, so trim one unit from each
        // character so they butt up against each other.
        cx += w + kerning;
    }
}

// cph 2006/08/06 - M_DrawString() is the old M_DrawMenuString, except that it
// is not tied to menu_buffer

void MN_DrawString(int cx, int cy, int color, const char *ch)
{
    MN_DrawStringCR(cx, cy, colrngs[color], NULL, ch);
}

static void DrawMenuString(int cx, int cy, int color)
{
    MN_DrawString(cx, cy, color, menu_buffer);
}

static void DrawMenuStringBuffer(int flags, int x, int y, int color,
                                 const char *buffer)
{
    if (ItemDisabled(flags))
    {
        MN_DrawStringCR(x, y, cr_dark, NULL, buffer);
    }
    else if (flags & S_HILITE)
    {
        if (color == CR_NONE)
        {
            MN_DrawStringCR(x, y, cr_bright, NULL, buffer);
        }
        else
        {
            MN_DrawStringCR(x, y, colrngs[color], cr_bright, buffer);
        }
    }
    else
    {
        MN_DrawString(x, y, color, buffer);
    }
}

static void DrawMenuStringEx(int flags, int x, int y, int color)
{
    DrawMenuStringBuffer(flags, x, y, color, menu_buffer);
}

// M_GetPixelWidth() returns the number of pixels in the width of
// the string, NOT the number of chars in the string.

int MN_GetPixelWidth(const char *ch)
{
    int len = 0;
    int c;

    while (*ch)
    {
        c = *ch++; // pick up next char

        if (c == '\x1b') // skip color
        {
            if (*ch)
            {
                ch++;
            }
            continue;
        }

        c = M_ToUpper(c) - HU_FONTSTART;
        if (c < 0 || c >= HU_FONTSIZE || hu_font[c] == NULL)
        {
            len += SPACEWIDTH; // space
            continue;
        }

        len += SHORT(hu_font[c]->width);
        len += kerning; // adjust so everything fits
    }
    len -= kerning; // replace what you took away on the last char only
    return len;
}

boolean MN_SetupCursorPostion(int x, int y)
{
    if (!setup_active || setup_select)
    {
        return false;
    }

    if (block_input)
    {
        return true;
    }

    if (current_tabs)
    {
        for (int i = 0; current_tabs[i].text; ++i)
        {
            setup_tab_t *tab = &current_tabs[i];

            tab->flags &= ~S_HILITE;

            if (MN_PointInsideRect(&tab->rect, x, y))
            {
                tab->flags |= S_HILITE;

                if (highlight_tab != i)
                {
                    highlight_tab = i;
                    M_StartSound(sfx_itemup);
                }
            }
        }
    }

    for (int i = 0; !(current_menu[i].m_flags & S_END); i++)
    {
        setup_menu_t *item = &current_menu[i];
        int flags = item->m_flags;

        if (flags & S_SKIP)
        {
            continue;
        }

        item->m_flags &= ~S_HILITE;

        mrect_t rect = item->rect;

        if (item->m_flags & S_THERMO)
        {
            rect.x = 0;
            rect.w = SCREENWIDTH;
        }

        if (MN_PointInsideRect(&rect, x, y))
        {
            item->m_flags |= S_HILITE;

            if (highlight_item != i)
            {
                print_warning_about_changes = false;
                highlight_item = i;
                M_StartSound(sfx_itemup);
            }
        }
    }

    return true;
}

static int setup_cancel = -1;

static void OnOff(void)
{
    setup_menu_t *current_item = current_menu + set_item_on;
    int flags = current_item->m_flags;
    default_t *def = current_item->var.def;

    // def->location->i = !def->location->i; // killough 8/15/98
    int value = *def->location.i;
    value = !value;
    *def->location.i = value;

    // killough 8/15/98: add warning messages

    if (flags & (S_LEVWARN | S_PRGWARN))
    {
        warn_about_changes(flags);
    }
    else if (def->current.i)
    {
        *def->current.i = *def->location.i;
    }

    if (current_item->action) // killough 10/98
    {
        current_item->action();
    }
}

static void Choice(menu_action_t action)
{
    setup_menu_t *current_item = current_menu + set_item_on;
    int flags = current_item->m_flags;
    default_t *def = current_item->var.def;
    int value = *def->location.i;

    if (flags & S_ACTION && setup_cancel == -1)
    {
        setup_cancel = value;
    }

    if (action == MENU_LEFT)
    {
        value--;

        if (def->limit.min != UL && value < def->limit.min)
        {
            value = def->limit.min;
        }

        if (*def->location.i != value)
        {
            M_StartSound(sfx_stnmov);
        }
        *def->location.i = value;

        if (!(flags & S_ACTION) && current_item->action)
        {
            current_item->action();
        }
    }

    if (action == MENU_RIGHT)
    {
        int max = def->limit.max;

        value++;

        if (max == UL)
        {
            const char **strings = GetStrings(current_item->strings_id);
            if (strings && value == array_size(strings))
            {
                value--;
            }
        }
        else if (value > max)
        {
            value = max;
        }

        if (*def->location.i != value)
        {
            M_StartSound(sfx_stnmov);
        }
        *def->location.i = value;

        if (!(flags & S_ACTION) && current_item->action)
        {
            current_item->action();
        }
    }

    if (action == MENU_ENTER)
    {
        if (flags & (S_LEVWARN | S_PRGWARN))
        {
            warn_about_changes(flags);
        }
        else if (def->current.i)
        {
            *def->current.i = *def->location.i;
        }

        if (current_item->action)
        {
            current_item->action();
        }
        SelectDone(current_item);
        setup_cancel = -1;
    }
}

static boolean ChangeEntry(menu_action_t action, int ch)
{
    if (!setup_select)
    {
        return false;
    }

    setup_menu_t *current_item = current_menu + set_item_on;
    int flags = current_item->m_flags;
    default_t *def = current_item->var.def;

    if (action == MENU_ESCAPE  // Exit key = no change
        || (action == MENU_BACKSPACE && !(flags & S_INPUT)))
    {
        if (flags & (S_CHOICE | S_CRITEM | S_THERMO) && setup_cancel != -1)
        {
            *def->location.i = setup_cancel;
            setup_cancel = -1;
        }

        SelectDone(current_item); // phares 4/17/98
        setup_gather = false;     // finished gathering keys, if any
        help_input = old_help_input;
        menu_input = old_menu_input;
        MN_ResetMouseCursor();
        return true;
    }

    if (flags & S_ONOFF) // yes or no setting?
    {
        if (action == MENU_ENTER)
        {
            OnOff();
        }
        SelectDone(current_item); // phares 4/17/98
        return true;
    }

    // [FG] selection of choices

    if (flags & (S_CHOICE | S_CRITEM | S_THERMO))
    {
        Choice(action);
        return true;
    }

    if (flags & S_NUM) // number?
    {
        if (!setup_gather) // gathering keys for a value?
        {
            return false;
        }
        // killough 10/98: Allow negatives, and use a more
        // friendly input method (e.g. don't clear value early,
        // allow backspace, and return to original value if bad
        // value is entered).

        help_input = old_help_input;
        menu_input = old_menu_input;
        MN_ResetMouseCursor();

        if (action == MENU_ENTER)
        {
            if (gather_count) // Any input?
            {
                gather_buffer[gather_count] = 0;

                int value = atoi(gather_buffer); // Integer value

                int min = def->limit.min;
                int max = def->limit.max;

                if ((min != UL && value < min) || (max != UL && value > max))
                {
                    warn_about_changes(S_BADVAL);
                    value = CLAMP(value, min, max);
                }

                *def->location.i = value;

                // killough 8/9/98: fix numeric vars
                // killough 8/15/98: add warning message

                if (flags & (S_LEVWARN | S_PRGWARN))
                {
                    warn_about_changes(flags);
                }
                else if (def->current.i)
                {
                    *def->current.i = value;
                }

                if (current_item->action) // killough 10/98
                {
                    current_item->action();
                }
            }
            SelectDone(current_item); // phares 4/17/98
            setup_gather = false;     // finished gathering keys
            return true;
        }

        if (action == MENU_BACKSPACE && gather_count)
        {
            gather_count--;
            return true;
        }

        if (gather_count >= MAXGATHER)
        {
            return true;
        }

        if (!isdigit(ch) && ch != '-')
        {
            return true; // ignore
        }

        // killough 10/98: character-based numerical input
        gather_buffer[gather_count++] = ch;
        return true;
    }

    return false;
}

static boolean BindInput(void)
{
    if (!set_keybnd_active || !setup_select) // on a key binding setup screen
    { // incoming key or button gets bound
        return false;
    }

    help_input = old_help_input;
    menu_input = old_menu_input;
    MN_ResetMouseCursor();

    setup_menu_t *current_item = current_menu + set_item_on;

    int input_id =
        (current_item->m_flags & S_INPUT) ? current_item->input_id : 0;

    if (!input_id)
    {
        return true; // not a legal action here (yet)
    }

    // see if the button is already bound elsewhere. if so, you
    // have to swap bindings so the action where it's currently
    // bound doesn't go dead. Since there is more than one
    // keybinding screen, you have to search all of them for
    // any duplicates. You're only interested in the items
    // that belong to the same group as the one you're changing.

    // if you find that you're trying to swap with an action
    // that has S_KEEP set, you can't bind ch; it's already
    // bound to that S_KEEP action, and that action has to
    // keep that key.

    if (M_InputActivated(input_id))
    {
        M_InputRemoveActivated(input_id);
        SelectDone(current_item);
        return true;
    }

    boolean search = true;

    for (int i = 0; keys_settings[i] && search; i++)
    {
        for (setup_menu_t *p = keys_settings[i]; !(p->m_flags & S_END); p++)
        {
            if (p->m_group == current_item->m_group && p != current_item
                && (p->m_flags & (S_INPUT | S_KEEP))
                && M_InputActivated(p->input_id))
            {
                if (p->m_flags & S_KEEP)
                {
                    return true; // can't have it!
                }
                M_InputRemoveActivated(p->input_id);
                search = false;
                break;
            }
        }
    }

    if (!M_InputAddActivated(input_id))
    {
        return true;
    }

    SelectDone(current_item); // phares 4/17/98
    return true;
}

static boolean NextPage(int inc)
{
    // Some setup screens may have multiple screens.
    // When there are multiple screens, m_prev and m_next items need to
    // be placed on the appropriate screen tables so the user can
    // move among the screens using the left and right arrow keys.
    // The m_var1 field contains a pointer to the appropriate screen
    // to move to.

    if (!current_tabs)
    {
        return false;
    }

    int i = current_page + inc;

    if (i < 0 || current_tabs[i].text == NULL)
    {
        return false;
    }

    current_page += inc;

    setup_menu_t *current_item = current_menu + set_item_on;
    current_item->m_flags &= ~S_HILITE;

    SetItemOn(set_item_on);
    highlight_tab = current_page;
    current_menu = setup_screens[setup_screen][current_page];
    set_item_on = GetItemOn();

    highlight_item = 0;

    print_warning_about_changes = false; // killough 10/98
    while (current_menu[set_item_on++].m_flags & S_SKIP)
        ;
    --set_item_on;
    if (menu_input != mouse_mode)
    {
        current_menu[set_item_on].m_flags |= S_HILITE;
    }

    M_StartSound(sfx_pstop); // killough 10/98
    return true;
}

static setup_menu_t *active_thermo = NULL;

boolean MN_SetupResponder(menu_action_t action, int ch)
{
    // phares 3/26/98 - 4/11/98:
    // Setup screen key processing

    if (!setup_active)
    {
        return false;
    }

    if (block_input)
    {
        return true;
    }

    if (menu_input != mouse_mode && current_tabs)
    {
        current_tabs[highlight_tab].flags &= ~S_HILITE;
    }

    setup_menu_t *current_item = current_menu + set_item_on;

    if (menu_input != mouse_mode)
    {
       current_item->m_flags |= S_HILITE;
    }

    if ((current_item->m_flags & (S_FUNC | S_CENTER)) && action == MENU_ENTER)
    {
        if (ItemDisabled(current_item->m_flags))
        {
            M_StartSound(sfx_oof);
            return true;
        }
        else if (current_item->action)
        {
            current_item->action();
        }

        M_StartSound(sfx_pistol);
        return true;
    }

    // phares 4/19/98:
    // Catch the response to the 'reset to default?' verification
    // screen

    if (default_verify)
    {
        if (M_ToUpper(ch) == 'Y' || action == MENU_ENTER)
        {
            ResetDefaults(setup_screen);
            ResetDefaultsSecondary();
            default_verify = false;
            SelectDone(current_item);
        }
        else if (M_ToUpper(ch) == 'N' || action == MENU_BACKSPACE
                 || action == MENU_ESCAPE)
        {
            default_verify = false;
            SelectDone(current_item);
        }
        return true;
    }

    if (ChangeEntry(action, ch))
    {
        return true;
    }

    if (BindInput())
    {
        return true;
    }

    // Weapons

    if (set_weapon_active && // on the weapons setup screen
        setup_select)        // changing an entry
    {
        if (action != MENU_ENTER)
        {
            ch -= '0'; // out of ascii
            if (ch < 1 || ch > 9)
            {
                return true; // ignore
            }

            // Plasma and BFG don't exist in shareware
            // killough 10/98: allow it anyway, since this
            // isn't the game itself, just setting preferences

            // see if 'ch' is already assigned elsewhere. if so,
            // you have to swap assignments.

            // killough 11/98: simplified

            for (int i = 0; weap_settings[i]; i++)
            {
                setup_menu_t *p = weap_settings[i];
                for (; !(p->m_flags & S_END); p++)
                {
                    if (p->m_flags & S_WEAP
                        && *p->var.def->location.i == ch
                        && p != current_item)
                    {
                        *p->var.def->location.i = *current_item->var.def->location.i;
                        goto end;
                    }
                }
            }
        end:
            *current_item->var.def->location.i = ch;
        }

        SelectDone(current_item); // phares 4/17/98
        help_input = old_help_input;
        menu_input = old_menu_input;
        MN_ResetMouseCursor();
        return true;
    }

    // [FG] clear key bindings with the DEL key
    if (action == MENU_CLEAR)
    {
        int index = (old_menu_input == mouse_mode ? highlight_item : set_item_on);
        current_item = current_menu + index;

        if (current_item->m_flags & S_INPUT)
        {
            M_InputReset(current_item->input_id);
        }
        help_input = old_help_input;
        menu_input = old_menu_input;
        MN_ResetMouseCursor();
        return true;
    }

    if (highlight_item != set_item_on)
    {
        current_menu[highlight_item].m_flags &= ~S_HILITE;
    }

    // Not changing any items on the Setup screens. See if we're
    // navigating the Setup menus or selecting an item to change.

    if (action == MENU_DOWN)
    {
        current_item->m_flags &= ~S_HILITE; // phares 4/17/98
        do
        {
            if (current_item->m_flags & S_END)
            {
                set_item_on = 0;
                current_item = current_menu;
            }
            else
            {
                set_item_on++;
                current_item++;
            }
        } while (current_item->m_flags & S_SKIP);

        SelectDone(current_item); // phares 4/17/98
        return true;
    }

    if (action == MENU_UP)
    {
        current_item->m_flags &= ~S_HILITE; // phares 4/17/98
        do
        {
            if (set_item_on == 0)
            {
                do
                {
                    set_item_on++;
                    current_item++;
                } while (!(current_item->m_flags & S_END));
            }
            set_item_on--;
            current_item--;
        } while (current_item->m_flags & S_SKIP);

        SelectDone(current_item); // phares 4/17/98
        return true;
    }

    if (action == MENU_ENTER)
    {
        int flags = current_item->m_flags;

        // You've selected an item to change. Highlight it, post a new
        // message about what to do, and get ready to process the
        // change.

        if (ItemDisabled(flags))
        {
            M_StartSound(sfx_oof);
            return true;
        }
        else if (flags & S_NUM)
        {
            setup_gather = true;
            print_warning_about_changes = false;
            gather_count = 0;
        }
        else if (flags & S_RESET)
        {
            default_verify = true;
        }

        current_item->m_flags |= S_SELECT;
        setup_select = true;
        M_StartSound(sfx_itemup);
        return true;
    }

    if (action == MENU_ESCAPE || action == MENU_BACKSPACE)
    {
        if (active_thermo && setup_cancel != -1)
        {
            default_t *def = active_thermo->var.def;
            *def->location.i = setup_cancel;
            setup_cancel = -1;
        }

        SetItemOn(set_item_on);
        SetPageIndex(current_page);

        if (action == MENU_ESCAPE) // Clear all menus
        {
            MN_ClearMenus();
            setup_active = false;
            setup_active_secondary = false;
        }
        else
        {
            if (setup_active_secondary)
            {
                MN_BackSecondary();
                setup_active_secondary = false;
            }
            else
            {
                MN_Back();
                setup_active = false;
            }
        }

        current_item->m_flags &= ~(S_HILITE | S_SELECT); // phares 4/19/98
        set_keybnd_active = false;
        set_weapon_active = false;
        default_verify = false;              // phares 4/19/98
        print_warning_about_changes = false; // [FG] reset
        active_thermo = NULL;
        M_StartSound(sfx_swtchx);
        return true;
    }

    if (action == MENU_LEFT)
    {
        if (NextPage(-1))
        {
            return true;
        }
    }

    if (action == MENU_RIGHT)
    {
        if (NextPage(1))
        {
            return true;
        }
    }

    return false;

} // End of Setup Screen processing

static boolean SetupTab(void)
{
    if (!current_tabs)
    {
        return false;
    }

    setup_tab_t *tab = current_tabs + highlight_tab;

    if (!(M_InputActivated(input_menu_enter) && tab->flags & S_HILITE))
    {
        return false;
    }

    current_page = highlight_tab;
    current_menu = setup_screens[setup_screen][current_page];
    set_item_on = 0;
    while (current_menu[set_item_on++].m_flags & S_SKIP)
        ;
    set_item_on--;

    M_StartSound(sfx_pstop);
    return true;
}

boolean MN_SetupMouseResponder(int x, int y)
{
    if (!setup_active || setup_select)
    {
        return false;
    }

    if (block_input)
    {
        return true;
    }

    if (SetupTab())
    {
        return true;
    }

    if (M_InputDeactivated(input_menu_enter) && active_thermo)
    {
        int flags = active_thermo->m_flags;
        default_t *def = active_thermo->var.def;

        if (flags & S_ACTION)
        {
            if (flags & (S_LEVWARN | S_PRGWARN))
            {
                warn_about_changes(flags);
            }
            else if (def->current.i)
            {
                *def->current.i = *def->location.i;
            }

            if (active_thermo->action)
            {
                active_thermo->action();
            }
        }
        active_thermo = NULL;
        setup_cancel = -1;
    }

    if (M_InputActivated(input_menu_enter))
    {
        set_item_on = highlight_item;
    }

    setup_menu_t *current_item = current_menu + set_item_on;
    int flags = current_item->m_flags;
    default_t *def = current_item->var.def;
    mrect_t *rect = &current_item->rect;

    if (ItemDisabled(flags))
    {
        return false;
    }

    if (M_InputActivated(input_menu_enter) && !MN_PointInsideRect(rect, x, y))
    {
        return true; // eat event
    }

    if (flags & S_THERMO)
    {
        if (active_thermo && active_thermo != current_item)
        {
            active_thermo = NULL;
        }

        if (M_InputActivated(input_menu_enter))
        {
            active_thermo = current_item;

            if (flags & S_ACTION && setup_cancel == -1)
            {
                setup_cancel = *def->location.i;
            }
        }
    }

    if (flags & S_THERMO && active_thermo)
    {
        int dot = x - (rect->x + M_THRM_STEP + video.deltaw);

        int min = def->limit.min;
        int max = def->limit.max;

        if (max == UL)
        {
            const char **strings = GetStrings(active_thermo->strings_id);
            if (strings)
            {
                max = array_size(strings) - 1;
            }
            else
            {
                max = M_THRM_UL_VAL;
            }
        }

        int step = (max - min) * FRACUNIT / (rect->w - M_THRM_STEP * 2);
        int value = dot * step / FRACUNIT + min;
        value = CLAMP(value, min, max);

        if (value != *def->location.i)
        {
            *def->location.i = value;

            if (!(flags & S_ACTION) && active_thermo->action)
            {
                active_thermo->action();
            }
            M_StartSound(sfx_stnmov);
        }
        return true;
    }

    if (!M_InputActivated(input_menu_enter))
    {
        return false;
    }

    if (flags & S_ONOFF) // yes or no setting?
    {
        OnOff();
        M_StartSound(sfx_itemup);
        return true;
    }

    if (flags & (S_CRITEM | S_CHOICE))
    {
        default_t *def = current_item->var.def;
        int value = *def->location.i;

        if (NextItemAvailable(current_item))
        {
            value++;
        }
        else if (def->limit.min != UL)
        {
            value = def->limit.min;
        }

        if (*def->location.i != value)
        {
            M_StartSound(sfx_stnmov);
        }
        *def->location.i = value;

        if (current_item->action)
        {
            current_item->action();
        }

        if (flags & (S_LEVWARN | S_PRGWARN))
        {
            warn_about_changes(flags);
        }
        else if (def->current.i)
        {
            *def->current.i = *def->location.i;
        }

        return true;
    }

    return false;
}

//
// Find string width from hu_font chars
//

int MN_StringWidth(const char *string)
{
    int c, w = 0;

    while (*string)
    {
        c = *string++;
        if (c == '\x1b') // skip code for color change
        {
            if (*string)
            {
                string++;
            }
            continue;
        }
        c = M_ToUpper(c) - HU_FONTSTART;
        if (c < 0 || c >= HU_FONTSIZE || hu_font[c] == NULL)
        {
            w += SPACEWIDTH;
            continue;
        }
        w += SHORT(hu_font[c]->width);
    }

    return w;
}

void MN_SetHUFontKerning(void)
{
    if (MN_StringWidth("abcdefghijklmnopqrstuvwxyz01234") > 230)
    {
        kerning = -1;
    }
}

//
//    Find string height from hu_font chars
//

int MN_StringHeight(const char *string)
{
    int height = SHORT(hu_font[0]->height);
    for (int i = 0; string[i]; ++i)
    {
        if (string[i] == '\n')
        {
            height += SHORT(hu_font[0]->height);
        }
    }
    return height;
}

// [FG] alternative text for missing menu graphics lumps

void MN_DrawTitle(int x, int y, const char *patch, const char *alttext)
{
    int patch_lump = W_CheckNumForName(patch);
    int patch_priority = -1;

    if (patch_lump >= 0)
    {
        patch_priority = lumpinfo[patch_lump].handle.priority;
    }

    if (patch_lump >= 0 && patch_priority >= bigfont_priority)
    {
        patch_t *patch = V_CachePatchNum(patch_lump, PU_CACHE);
        V_DrawPatch(x == M_X_CENTER ? SCREENWIDTH / 2 - patch->width / 2 : x,
                    y, patch);
    }
    else
    {
        // patch doesn't exist, draw some text in place of it
        if (!MN_DrawFon2String(
                (SCREENWIDTH - MN_GetFon2PixelWidth(alttext)) / 2,
                y, NULL, alttext))
        {
            M_snprintf(menu_buffer, sizeof(menu_buffer), "%s", alttext);
            DrawMenuString(
                (SCREENWIDTH - MN_GetPixelWidth(alttext)) / 2,
                y + 8 - MN_StringHeight(alttext) / 2, // assumes patch height 16
                CR_TITLE);
        }
    }
}

static const char **selectstrings[] = {
    [str_empty] = NULL,
    [str_layout] = layout_strings,
    [str_flick_snap] = flick_snap_strings,
    [str_ms_time] = NULL,
    [str_movement_sensitivity] = NULL,
    [str_movement_type] = movement_type_strings,
    [str_percent] = percent_strings,
    [str_curve] = curve_strings,
    [str_center_weapon] = center_weapon_strings,
    [str_screensize] = NULL,
    [str_show_widgets] = show_widgets_strings,
    [str_show_adv_widgets] = show_adv_widgets_strings,
    [str_stats_format] = stats_format_strings,
    [str_crosshair] = crosshair_strings,
    [str_crosshair_target] = crosshair_target_strings,
    [str_hudcolor] = hudcolor_strings,
    [str_secretmessage] = secretmessage_strings,
    [str_overlay] = overlay_strings,
    [str_automap_preset] = automap_preset_strings,
    [str_automap_keyed_door] = automap_keyed_door_strings,
    [str_fuzzmode] = fuzzmode_strings,
    [str_weapon_slots_activation] = weapon_slots_activation_strings,
    [str_weapon_slots_selection] = weapon_slots_selection_strings,
    [str_weapon_slots] = NULL,
    [str_resolution_scale] = NULL,
    [str_midi_player] = NULL,
    [str_gamma] = gamma_strings,
    [str_sound_module] = sound_module_strings,
    [str_extra_music] = extra_music_strings,
    [str_resampler] = NULL,
    [str_equalizer_preset] = equalizer_preset_strings,
    [str_mouse_accel] = NULL,
    [str_gamepad_device] = NULL,
    [str_gyro_space] = gyro_space_strings,
    [str_gyro_action] = gyro_action_strings,
    [str_gyro_sens] = NULL,
    [str_gyro_accel] = NULL,
    [str_default_complevel] = default_complevel_strings,
    [str_endoom] = endoom_strings,
    [str_death_use_action] = death_use_action_strings,
    [str_widescreen] = widescreen_strings,
    [str_bobbing_pct] = bobbing_pct_strings,
    [str_screen_melt] = screen_melt_strings,
    [str_invul_mode] = invul_mode_strings,
    [str_skill] = skill_strings,
    [str_freelook] = free_look_strings,
};

static const char **GetStrings(int id)
{
    if (id > str_empty && id < arrlen(selectstrings))
    {
        return selectstrings[id];
    }

    return NULL;
}

static const char **GetGamepadDeviceStrings(void)
{
    return I_GamepadDeviceList();
}

void MN_UpdateAllGamepadItems(void)
{
    selectstrings[str_gamepad_device] = GetGamepadDeviceStrings();
    UpdateWeaponSlotSelection();
    UpdateGamepadItems();
    UpdateGyroItems();
}

static void UpdateWeaponSlotStrings(void)
{
    selectstrings[str_weapon_slots] = GetWeaponSlotStrings();
}

static const char **GetMidiPlayerStrings(void)
{
    return I_DeviceList();
}

static const char **GetScreenSizeStrings(void)
{
    const char **strings = NULL;

    for (int i = 0; i < 3; ++i)
    {
        array_push(strings, "");
    }
    for (int i = 3; i < 10; ++i)
    {
        array_push(strings, "Status Bar");
    }

    const char **st_strings = ST_StatusbarList();
    for (int i = 0; i < array_size(st_strings); ++i)
    {
        array_push(strings, st_strings[i]);
    }

    maxscreenblocks = array_size(strings) - 1;
    screenblocks = MIN(screenblocks, maxscreenblocks);

    return strings;
}

void MN_InitMenuStrings(void)
{
    UpdateWeaponSlotLabels();
    UpdateWeaponSlotStrings();
    selectstrings[str_resolution_scale] = GetResolutionScaleStrings();
    selectstrings[str_midi_player] = GetMidiPlayerStrings();
    selectstrings[str_mouse_accel] = GetMouseAccelStrings();
    selectstrings[str_ms_time] = GetMsTimeStrings();
    selectstrings[str_movement_sensitivity] = GetMovementSensitivityStrings();
    selectstrings[str_gamepad_device] = GetGamepadDeviceStrings();
    selectstrings[str_gyro_sens] = GetGyroSensitivityStrings();
    selectstrings[str_gyro_accel] = GetGyroAccelStrings();
    selectstrings[str_resampler] = GetResamplerStrings();
    selectstrings[str_screensize] = GetScreenSizeStrings();
}

void MN_SetupResetMenu(void)
{
    DisableItem(force_complevel != CL_NONE, comp_settings1, "default_complevel");
    DisableItem(M_ParmExists("-uncapped") || M_ParmExists("-nouncapped"),
                gen_settings1, "uncapped");
    DisableItem(deh_set_blood_color, enem_settings1, "colored_blood");
    DisableItem(!brightmaps_found || force_brightmaps, gen_settings5,
                "brightmaps");
    DisableItem(!trakinfo_found, gen_settings2, "extra_music");
    DisableItem(M_ParmExists("-save"), gen_settings6, "organize_savefiles");
    UpdateInterceptsEmuItem();
    UpdateStatsFormatItem();
    UpdateCrosshairItems();
    UpdateCenteredWeaponItem();
    UpdateGamepadItems();
    UpdateGyroItems();
    UpdateWeaponSlotItems();
    MN_UpdateEqualizerItems();
    UpdateGainItems();
}

void MN_BindMenuVariables(void)
{
    BIND_NUM_MENU(resolution_scale, 0, UL);
    BIND_BOOL_GENERAL(menu_pause_demos, false,
        "Pause demo loop while menu is open");
    BIND_NUM_GENERAL(menu_backdrop, MENU_BG_DARK, MENU_BG_OFF, MENU_BG_TEXTURE,
        "Menu backdrop (0 = Off; 1 = Dark; 2 = Texture)");
    BIND_NUM_GENERAL(menu_help, MENU_HELP_AUTO, MENU_HELP_OFF, MENU_HELP_PAD,
        "Menu help (0 = Off; 1 = Auto; 2 = Always Keyboard; 3 = Always Gamepad)");
    BIND_NUM_MENU(csmenu_skill, sk_baby, sk_nightmare);
    BIND_BOOL_MENU(csmenu.nomonsters);
    BIND_BOOL_MENU(csmenu.fastparm);
    BIND_BOOL_MENU(csmenu.respawnparm);
    BIND_BOOL_MENU(csmenu.pistolstart);
    BIND_BOOL_MENU(csmenu.coopspawns);
    BIND_BOOL_MENU(csmenu.doubleammo);
    BIND_BOOL_MENU(csmenu.halfplayerdamage);
    BIND_BOOL_MENU(csmenu.aggromonsters);
    BIND_NUM_MENU(freelook_mode, FREELOOK_OFF, FREELOOK_DIRECT_AIM);
}

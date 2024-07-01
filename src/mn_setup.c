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

#include "mn_internal.h"

#include "am_map.h"
#include "d_deh.h"
#include "d_main.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "g_game.h"
#include "hu_lib.h"
#include "hu_stuff.h"
#include "i_gamepad.h"
#include "i_oalequalizer.h"
#include "i_oalsound.h"
#include "i_sound.h"
#include "i_timer.h"
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
#include "r_sky.h"   // [FG] R_InitSkyMap()
#include "r_voxel.h"
#include "s_sound.h"
#include "sounds.h"
#include "v_fmt.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

static int M_GetKeyString(int c, int offset);
static void DrawMenuString(int cx, int cy, int color);
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

#define M_SPC             9
#define M_X               240
#define M_Y               (29 + M_SPC)
#define M_Y_WARN          (SCREENHEIGHT - 15)

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
    {0, S_SKIP | S_END}

// Button for resetting to defaults
#define MI_RESET \
    {0, S_RESET, X_BUTTON, Y_BUTTON}

#define MI_GAP \
    {"", S_SKIP, 0, M_SPC}

static void DisableItem(boolean condition, setup_menu_t *menu, const char *item)
{
    while (!(menu->m_flags & S_END))
    {
        if (!(menu->m_flags & (S_SKIP | S_RESET)))
        {
            if (strcasecmp(menu->var.def->name, item) == 0)
            {
                if (condition)
                {
                    menu->m_flags |= S_DISABLE;
                }
                else
                {
                    menu->m_flags &= ~S_DISABLE;
                }

                return;
            }
        }

        menu++;
    }

    I_Error("Item \"%s\" not found in menu", item);
}

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
    str_curve,
    str_center_weapon,
    str_screensize,
    str_hudtype,
    str_hudmode,
    str_show_widgets,
    str_show_adv_widgets,
    str_crosshair,
    str_crosshair_target,
    str_hudcolor,
    str_overlay,
    str_automap_preset,
    str_automap_keyed_door,

    str_resolution_scale,
    str_midi_player,

    str_gamma,
    str_sound_module,
    str_resampler,
    str_equalizer_preset,

    str_mouse_accel,

    str_default_skill,
    str_default_complevel,
    str_endoom,
    str_death_use_action,
    str_menu_backdrop,
    str_widescreen,
    str_bobbing_pct,
    str_screen_melt,
    str_invul_mode,
};

static const char **GetStrings(int id);

static boolean ItemDisabled(int flags)
{
    complevel_t complevel =
        force_complevel != CL_NONE ? force_complevel : default_complevel;

    if ((flags & S_DISABLE)
        || (flags & S_STRICT && (default_strictmode || force_strictmode))
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
    int value = s->var.def->location->i;
    int min = s->var.def->limit.min;

    return value > min;
}

static boolean NextItemAvailable(setup_menu_t *s)
{
    int value = s->var.def->location->i;
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
    if (!ItemSelected(s))
    {
        return;
    }

    int flags = s->m_flags;

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
    if (!ItemSelected(s))
    {
        return;
    }

    int flags = s->m_flags;

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
        DrawMenuStringEx(tabs[i].flags, x, rect->y, CR_GOLD);
        if (i == current_page)
        {
            V_FillRect(x + video.deltaw, rect->y + M_SPC, rect->w, 1,
                       cr_gold[cr_shaded[v_lightest_color]]);
        }

        rect->x = x;

        x += rect->w;
    }
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

    int w = 0;
    const char *text = s->m_text;
    int color = flags & S_TITLE    ? CR_TITLE
                : flags & S_SELECT ? CR_SELECT
                : flags & S_HILITE ? CR_HILITE
                                   : CR_ITEM; // killough 10/98

    if (!(flags & S_NEXT_LINE))
    {
        BlinkingArrowLeft(s);
    }

    // killough 10/98: support left-justification:
    strcat(menu_buffer, text);
    w = MN_GetPixelWidth(menu_buffer);
    if (!(flags & S_LEFTJUST))
    {
        x -= (w + 4);
    }

    rect->x = 0;
    rect->y = y;
    rect->w = SCREENWIDTH;
    rect->h = M_SPC;

    if (flags & S_THERMO)
    {
        y += M_THRM_TXT_OFFSET;
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

static void DrawSetupThermo(int x, int y, int width, int size, int dot,
                            byte *cr)
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

    int step = width * M_THRM_STEP * FRACUNIT / size;

    V_DrawPatchTranslated(x + M_THRM_STEP + dot * step / FRACUNIT, y,
                          V_CachePatchName("M_THERMO", PU_CACHE), cr);
}

static void DrawSetting(setup_menu_t *s, int accum_y)
{
    int x = s->m_x, y = s->m_y, flags = s->m_flags, color;

    if (!(flags & S_DIRECT))
    {
        y = accum_y;
    }

    // Determine color of the text. This may or may not be used
    // later, depending on whether the item is a text string or not.

    color = flags & S_SELECT ? CR_SELECT : CR_SET;

    // Is the item a YES/NO item?

    if (flags & S_ONOFF)
    {
        strcpy(menu_buffer, s->var.def->location->i ? "ON" : "OFF");
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
        else if (flags & S_PCT)
        {
            M_snprintf(menu_buffer, sizeof(menu_buffer), "%d%%",
                       s->var.def->location->i);
        }
        else
        {
            M_snprintf(menu_buffer, sizeof(menu_buffer), "%d",
                       s->var.def->location->i);
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
                                      M_GetNameForJoyB(inputs[i].value));
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
        sprintf(menu_buffer, "%d", s->var.def->location->i);
        BlinkingArrowRight(s);
        DrawMenuStringEx(flags, x, y, color);
        return;
    }

    // [FG] selection of choices

    if (flags & (S_CHOICE | S_CRITEM))
    {
        int i = s->var.def->location->i;
        mrect_t *rect = &s->rect;
        int width;
        const char **strings = GetStrings(s->strings_id);

        menu_buffer[0] = '\0';

        if (flags & S_NEXT_LINE)
        {
            BlinkingArrowLeft(s);
        }

        if (i >= 0 && strings)
        {
            strcat(menu_buffer, strings[i]);
        }
        width = MN_GetPixelWidth(menu_buffer);
        if (flags & S_NEXT_LINE)
        {
            y += M_SPC;
            x = M_X - width - 4;
            rect->y = y;
        }

        BlinkingArrowRight(s);
        DrawMenuStringEx(flags, x, y, flags & S_CRITEM ? i : color);
        return;
    }

    if (flags & S_THERMO)
    {
        int value = s->var.def->location->i;
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

        int thrm_val = BETWEEN(min, max, value);

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
        DrawSetupThermo(x, y, width, max - min, thrm_val - min, cr);

        if (strings)
        {
            strcpy(menu_buffer, strings[value]);
        }
        else if (flags & S_PCT)
        {
            M_snprintf(menu_buffer, sizeof(menu_buffer), "%d%%", value);
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

#define VERIFYBOXXORG 66
#define VERIFYBOXYORG 88

static void DrawDefVerify()
{
    V_DrawPatch(VERIFYBOXXORG, VERIFYBOXYORG,
                V_CachePatchName("M_VBOX", PU_CACHE));

    // The blinking messages is keyed off of the blinking of the
    // cursor skull.

    if (whichSkull) // blink the text
    {
        strcpy(menu_buffer, "Restore defaults? (Y or N)");
        DrawMenuString(VERIFYBOXXORG + 8, VERIFYBOXYORG + 8, CR_RED);
    }
}

void MN_DrawDelVerify(void)
{
    V_DrawPatch(VERIFYBOXXORG, VERIFYBOXYORG,
                V_CachePatchName("M_VBOX", PU_CACHE));

    if (whichSkull)
    {
        MN_DrawString(VERIFYBOXXORG + 8, VERIFYBOXYORG + 8, CR_RED,
                      "Delete savegame? (Y or N)");
    }
}

/////////////////////////////
//
// phares 4/18/98:
// M_DrawInstructions writes the instruction text just below the screen title
//
// killough 8/15/98: rewritten

static void DrawInstructions()
{
    int index = (menu_input == mouse_mode ? highlight_item : set_item_on);
    int flags = current_menu[index].m_flags;

    if (ItemDisabled(flags) || print_warning_about_changes > 0)
    {
        return;
    }

    // There are different instruction messages depending on whether you
    // are changing an item or just sitting on it.

    const char *s = "";

    if (setup_select)
    {
        if (flags & S_INPUT)
        {
            s = "Press key or button to bind/unbind";
        }
        else if (flags & S_ONOFF)
        {
            if (menu_input == pad_mode)
            {
                s = "[ PadA ] to toggle";
            }
            else
            {
                s = "[ Enter ] to toggle, [ Esc ] to cancel";
            }
        }
        else if (flags & (S_CHOICE | S_CRITEM | S_THERMO))
        {
            if (menu_input == pad_mode)
            {
                s = "[ Left/Right ] to choose, [ PadB ] to cancel";
            }
            else
            {
                s = "[ Left/Right ] to choose, [ Esc ] to cancel";
            }
        }
        else if (flags & S_NUM)
        {
            s = "Enter value";
        }
        else if (flags & S_WEAP)
        {
            s = "Enter weapon number";
        }
        else if (flags & S_RESET)
        {
            s = "Restore defaults";
        }
    }
    else
    {
        if (flags & S_INPUT)
        {
            switch (menu_input)
            {
                case mouse_mode:
                    s = "[ Del ] to clear";
                    break;
                case pad_mode:
                    s = "[ PadA ] to change, [ PadY ] to clear";
                    break;
                default:
                case key_mode:
                    s = "[ Enter ] to change, [ Del ] to clear";
                    break;
            }
        }
        else if (flags & S_RESET)
        {
            s = "Restore defaults";
        }
        else
        {
            switch (menu_input)
            {
                case pad_mode:
                    s = "[ PadA ] to change, [ PadB ] to return";
                    break;
                case key_mode:
                    s = "[ Enter ] to change";
                    break;
                default:
                    break;
            }
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
    MI_GAP,
    {"Toggles",   S_SKIP | S_TITLE, KB_X, M_SPC},
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
    MI_GAP,
    // [FG] prev/next weapon keys and buttons
    {"Prev", S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_prevweapon},
    {"Next", S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_nextweapon},
    MI_END
};

static setup_menu_t keys_settings3[] = {
    // [FG] reload current level / go to next level
    {"Reload Map/Demo", S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_menu_reloadlevel},
    {"Next Map",        S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_menu_nextlevel},
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
    {"Player 1",        S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_chat_dest0},
    {"Player 2",        S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_chat_dest1},
    {"Player 3",        S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_chat_dest2},
    {"Player 4",        S_INPUT, KB_X, M_SPC, {0}, m_scrn, input_chat_dest3},
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
    inhelpscreens = true; // killough 4/6/98: Force status bar redraw

    // Set up the Key Binding screen

    DrawBackground("FLOOR4_6"); // Draw background
    MN_DrawTitle(84, 2, "M_KEYBND", "Key Bindings");
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
    {"cosmetic"},
    {"preferences"},
    {NULL}
};

// [FG] centered or bobbing weapon sprite
static const char *center_weapon_strings[] = {"Off", "Centered", "Bobbing"};

static void UpdateCenteredWeaponItem(void);

static const char *bobbing_pct_strings[] = {"0%", "25%", "50%", "75%", "100%"};

static setup_menu_t weap_settings1[] = {

    {"View Bob", S_THERMO | S_THRM_SIZE4, M_X_THRM4, M_THRM_SPC,
     {"view_bobbing_pct"}, m_null, input_null, str_bobbing_pct},

    {"Weapon Bob", S_THERMO | S_THRM_SIZE4, M_X_THRM4, M_THRM_SPC,
     {"weapon_bobbing_pct"}, m_null, input_null, str_bobbing_pct,
     UpdateCenteredWeaponItem},

    // [FG] centered or bobbing weapon sprite
    {"Weapon Alignment", S_CHOICE | S_STRICT, M_X, M_SPC,
     {"center_weapon"}, m_null, input_null, str_center_weapon},

    {"Hide Weapon", S_ONOFF | S_STRICT, M_X, M_SPC, {"hide_weapon"}},

    {"Weapon Recoil", S_ONOFF, M_X, M_SPC, {"weapon_recoilpitch"}},

    MI_RESET,

    MI_END
};

static setup_menu_t weap_settings2[] = {
    {"1St Choice Weapon", S_WEAP | S_BOOM, M_X, M_SPC, {"weapon_choice_1"}},
    {"2Nd Choice Weapon", S_WEAP | S_BOOM, M_X, M_SPC, {"weapon_choice_2"}},
    {"3Rd Choice Weapon", S_WEAP | S_BOOM, M_X, M_SPC, {"weapon_choice_3"}},
    {"4Th Choice Weapon", S_WEAP | S_BOOM, M_X, M_SPC, {"weapon_choice_4"}},
    {"5Th Choice Weapon", S_WEAP | S_BOOM, M_X, M_SPC, {"weapon_choice_5"}},
    {"6Th Choice Weapon", S_WEAP | S_BOOM, M_X, M_SPC, {"weapon_choice_6"}},
    {"7Th Choice Weapon", S_WEAP | S_BOOM, M_X, M_SPC, {"weapon_choice_7"}},
    {"8Th Choice Weapon", S_WEAP | S_BOOM, M_X, M_SPC, {"weapon_choice_8"}},
    {"9Th Choice Weapon", S_WEAP | S_BOOM, M_X, M_SPC, {"weapon_choice_9"}},
    MI_GAP,
    {"Use Weapon Toggles", S_ONOFF | S_BOOM, M_X, M_SPC, {"doom_weapon_toggles"}},
    MI_GAP,
    // killough 8/8/98
    {"Pre-Beta BFG", S_ONOFF | S_STRICT, M_X, M_SPC, {"classic_bfg"}},
    MI_END
};

static setup_menu_t *weap_settings[] = {weap_settings1, weap_settings2, NULL};

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
    inhelpscreens = true; // killough 4/6/98: Force status bar redraw

    DrawBackground("FLOOR4_6"); // Draw background
    MN_DrawTitle(109, 2, "M_WEAP", "Weapons");
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
    int choice = -1;

    if (screenblocks > saved_screenblocks)
    {
        choice = 1;
    }
    else if (screenblocks < saved_screenblocks)
    {
        choice = 0;
    }

    screenblocks = saved_screenblocks;

    if (choice != -1)
    {
        MN_SizeDisplay(choice);
    }

    hud_displayed = (screenblocks == 11);
}

static const char *screensize_strings[] = {
    "",           "",           "",           "Status Bar",
    "Status Bar", "Status Bar", "Status Bar", "Status Bar",
    "Status Bar", "Status Bar", "Status Bar", "Fullscreen"
};

static const char *hudtype_strings[] = {"Crispy", "Boom No Bars", "Boom"};

static const char **GetHUDModeStrings(void)
{
    static const char *crispy_strings[] = {"Off", "Original", "Widescreen"};
    static const char *boom_strings[] = {"Minimal", "Compact", "Distributed"};
    return hud_type ? boom_strings : crispy_strings;
}

static void UpdateHUDModeStrings(void);

#define H_X_THRM8 (M_X_THRM8 - 14)
#define H_X       (M_X - 14)

static setup_menu_t stat_settings1[] = {

    {"Screen Size", S_THERMO, H_X_THRM8, M_THRM_SPC, {"screenblocks"},
     m_null, input_null, str_screensize, SizeDisplayAlt},

    MI_GAP,

    {"Status Bar", S_SKIP | S_TITLE, H_X, M_SPC},

    {"Colored Numbers", S_ONOFF | S_COSMETIC, H_X, M_SPC, {"sts_colored_numbers"}},

    {"Gray Percent Sign", S_ONOFF | S_COSMETIC, H_X, M_SPC, {"sts_pct_always_gray"}},

    {"Solid Background Color", S_ONOFF, H_X, M_SPC, {"st_solidbackground"}},

    MI_GAP,

    {"Fullscreen HUD", S_SKIP | S_TITLE, H_X, M_SPC},

    {"HUD Type", S_CHOICE, H_X, M_SPC, {"hud_type"}, m_null, input_null,
     str_hudtype, UpdateHUDModeStrings},

    {"HUD Mode", S_CHOICE, H_X, M_SPC, {"hud_active"}, m_null, input_null,
     str_hudmode},

    MI_GAP,

    {"Backpack Shifts Ammo Color", S_ONOFF, H_X, M_SPC, {"hud_backpack_thresholds"}},

    {"Armor Color Matches Type", S_ONOFF, H_X, M_SPC, {"hud_armor_type"}},

    {"Animated Health/Armor Count", S_ONOFF, H_X, M_SPC, {"hud_animated_counts"}},

    {"Blink Missing Keys", S_ONOFF, H_X, M_SPC, {"hud_blink_keys"}},

    MI_RESET,

    MI_END
};

static const char *show_widgets_strings[] = {"Off", "Automap", "HUD", "Always"};
static const char *show_adv_widgets_strings[] = {"Off", "Automap", "HUD",
                                                 "Always", "Advanced"};

static setup_menu_t stat_settings2[] = {

    {"Widget Types", S_SKIP | S_TITLE, M_X, M_SPC},

    {"Show Level Stats", S_CHOICE, M_X, M_SPC, {"hud_level_stats"},
     m_null, input_null, str_show_widgets},

    {"Show Level Time", S_CHOICE, M_X, M_SPC, {"hud_level_time"},
     m_null, input_null, str_show_widgets},

    {"Show Player Coords", S_CHOICE | S_STRICT, M_X, M_SPC,
     {"hud_player_coords"}, m_null, input_null, str_show_adv_widgets, HU_Start},

    {"Show Command History", S_ONOFF | S_STRICT, M_X, M_SPC,
     {"hud_command_history"}, m_null, input_null, str_empty,
     HU_ResetCommandHistory},

    {"Use-Button Timer", S_ONOFF, M_X, M_SPC, {"hud_time_use"}},

    MI_GAP,

    {"Widget Appearance", S_SKIP | S_TITLE, M_X, M_SPC},

    {"Use Doom Font", S_CHOICE, M_X, M_SPC, {"hud_widget_font"},
     m_null, input_null, str_show_widgets},

    {"Widescreen Alignment", S_ONOFF, M_X, M_SPC, {"hud_widescreen_widgets"},
     m_null, input_null, str_empty, HU_Start},

    {"Vertical Layout", S_ONOFF, M_X, M_SPC, {"hud_widget_layout"},
     m_null, input_null, str_empty, HU_Start},

    MI_END
};

static void UpdateCrosshairItems(void);

static const char *crosshair_target_strings[] = {"Off", "Highlight", "Health"};

static const char *hudcolor_strings[] = {
    "BRICK",  "TAN",    "GRAY",  "GREEN", "BROWN",  "GOLD",  "RED", "BLUE",
    "ORANGE", "YELLOW", "BLUE2", "BLACK", "PURPLE", "WHITE", "NONE"
};

#define XH_X (M_X - 33)

static setup_menu_t stat_settings3[] = {

    {"Crosshair", S_CHOICE, XH_X, M_SPC, {"hud_crosshair"},
     m_null, input_null, str_crosshair, UpdateCrosshairItems},

    {"Color By Player Health", S_ONOFF | S_STRICT, XH_X, M_SPC, {"hud_crosshair_health"}},

    {"Color By Target", S_CHOICE | S_STRICT, XH_X, M_SPC,
     {"hud_crosshair_target"}, m_null, input_null, str_crosshair_target,
     UpdateCrosshairItems},

    {"Lock On Target", S_ONOFF | S_STRICT, XH_X, M_SPC, {"hud_crosshair_lockon"}},

    {"Default Color", S_CRITEM, XH_X, M_SPC, {"hud_crosshair_color"},
     m_null, input_null, str_hudcolor},

    {"Highlight Color", S_CRITEM | S_STRICT, XH_X, M_SPC,
     {"hud_crosshair_target_color"}, m_null, input_null, str_hudcolor},

    MI_END
};

static setup_menu_t stat_settings4[] = {
    {"Announce Revealed Secrets", S_ONOFF, M_X, M_SPC, {"hud_secret_message"}},
    {"Announce Map Titles",  S_ONOFF, M_X, M_SPC, {"hud_map_announce"}},
    {"Show Toggle Messages", S_ONOFF, M_X, M_SPC, {"show_toggle_messages"}},
    {"Show Pickup Messages", S_ONOFF, M_X, M_SPC, {"show_pickup_messages"}},
    {"Show Obituaries",      S_ONOFF, M_X, M_SPC, {"show_obituary_messages"}},
    {"Center Messages",      S_ONOFF, M_X, M_SPC, {"message_centered"}},
    {"Colorize Messages",    S_ONOFF, M_X, M_SPC, {"message_colorized"},
     m_null, input_null, str_empty, HU_ResetMessageColors},
    MI_END
};

static setup_menu_t *stat_settings[] = {stat_settings1, stat_settings2,
                                        stat_settings3, stat_settings4, NULL};

static void UpdateCrosshairItems(void)
{
    DisableItem(!hud_crosshair, stat_settings3, "hud_crosshair_health");
    DisableItem(!hud_crosshair, stat_settings3, "hud_crosshair_target");
    DisableItem(!hud_crosshair, stat_settings3, "hud_crosshair_lockon");
    DisableItem(!hud_crosshair, stat_settings3, "hud_crosshair_color");
    DisableItem(
        !(hud_crosshair && hud_crosshair_target == crosstarget_highlight),
        stat_settings3, "hud_crosshair_target_color");
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
    inhelpscreens = true; // killough 4/6/98: Force status bar redraw

    DrawBackground("FLOOR4_6"); // Draw background
    MN_DrawTitle(59, 2, "M_STAT", "Status Bar/HUD");
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

    {"Modes", S_SKIP | S_TITLE, M_X, M_SPC},

    {"Follow Player",   S_ONOFF,  M_X, M_SPC, {"followplayer"}},
    {"Rotate Automap",  S_ONOFF,  M_X, M_SPC, {"automaprotate"}},
    {"Overlay Automap", S_CHOICE, M_X, M_SPC, {"automapoverlay"},
     m_null, input_null, str_overlay},

    // killough 10/98
    {"Coords Follow Pointer", S_ONOFF, M_X, M_SPC, {"map_point_coord"}},

    MI_GAP,

    {"Miscellaneous", S_SKIP | S_TITLE, M_X, M_SPC},

    {"Color Preset", S_CHOICE | S_COSMETIC, M_X, M_SPC, {"mapcolor_preset"},
     m_null , input_null, str_automap_preset, AM_ColorPreset},

    {"Smooth automap lines", S_ONOFF, M_X, M_SPC, {"map_smooth_lines"},
     m_null, input_null, str_empty, AM_EnableSmoothLines},

    {"Show Found Secrets Only", S_ONOFF, M_X, M_SPC, {"map_secret_after"}},

    {"Color Keyed Doors", S_CHOICE, M_X, M_SPC, {"map_keyed_door"},
     m_null, input_null, str_automap_keyed_door},

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
    inhelpscreens = true; // killough 4/6/98: Force status bar redraw

    DrawBackground("FLOOR4_6"); // Draw background
    MN_DrawTitle(109, 2, "M_AUTO", "Automap");
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
     M_X_THRM4, M_THRM_SPC, {"player_helpers"}, m_null, input_null,
     str_empty, BarkSound},

    MI_GAP,

    {"Cosmetic", S_SKIP | S_TITLE, M_X, M_SPC},

    // [FG] colored blood and gibs
    {"Colored Blood", S_ONOFF | S_STRICT, M_X, M_SPC,
     {"colored_blood"}, m_null, input_null, str_overlay, D_SetBloodColor},

    // [crispy] randomly flip corpse, blood and death animation sprites
    {"Randomly Mirrored Corpses", S_ONOFF | S_STRICT, M_X, M_SPC,
     {"flipcorpses"}},

    // [crispy] resurrected pools of gore ("ghost monsters") are translucent
    {"Translucent Ghost Monsters", S_ONOFF | S_STRICT | S_VANILLA, M_X, M_SPC,
     {"ghost_monsters"}},

    // [FG] spectre drawing mode
    {"Blocky Spectre Drawing", S_ONOFF, M_X, M_SPC, {"fuzzcolumn_mode"},
     m_null, input_null, str_overlay, R_SetFuzzColumnMode},

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
    inhelpscreens = true;

    DrawBackground("FLOOR4_6"); // Draw background
    MN_DrawTitle(114, 2, "M_ENEM", "Enemies");
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

setup_menu_t comp_settings1[] = {

    {"Default Compatibility Level", S_CHOICE | S_LEVWARN, M_X, M_SPC,
     {"default_complevel"}, m_null, input_null, str_default_complevel,
     UpdateInterceptsEmuItem},

    {"Strict Mode", S_ONOFF | S_LEVWARN, M_X, M_SPC, {"strictmode"}},

    MI_GAP,

    {"Compatibility-breaking Features", S_SKIP | S_TITLE, M_X, M_SPC},

    {"Direct Vertical Aiming", S_ONOFF | S_STRICT, M_X, M_SPC,
     {"direct_vertical_aiming"}},

    {"Auto Strafe 50", S_ONOFF | S_STRICT, M_X, M_SPC,
     {"autostrafe50"}, m_null, input_null, str_empty, G_UpdateSideMove},

    {"Pistol Start", S_ONOFF | S_STRICT, M_X, M_SPC, {"pistolstart"}},

    MI_GAP,

    {"Improved Hit Detection", S_ONOFF | S_STRICT, M_X, M_SPC,
     {"blockmapfix"}},

    {"Fast Line-of-Sight Calculation", S_ONOFF | S_STRICT, M_X, M_SPC,
     {"checksight12"}, m_null, input_null, str_empty, P_UpdateCheckSight},

    {"Walk Under Solid Hanging Bodies", S_ONOFF | S_STRICT, M_X, M_SPC,
     {"hangsolid"}},

    {"Emulate INTERCEPTS overflow", S_ONOFF | S_VANILLA, M_X, M_SPC,
     {"emu_intercepts"}, m_null, input_null, str_empty, UpdateInterceptsEmuItem},

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
    inhelpscreens = true;

    DrawBackground("FLOOR4_6"); // Draw background
    MN_DrawTitle(52, 2, "M_COMPAT", "Compatibility");
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
    resolution_scaling_t rs;
    I_GetResolutionScaling(&rs);

    array_push(strings, "100%");

    if (current_video_height == SCREENHEIGHT)
    {
        resolution_scale = 0;
    }

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

    resolution_scale = BETWEEN(0, i, resolution_scale);

    array_push(strings, "native");

    return strings;
}

static void ResetVideoHeight(void)
{
    const char **strings = GetStrings(str_resolution_scale);
    resolution_scaling_t rs;
    I_GetResolutionScaling(&rs);

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

static const char *widescreen_strings[] = {"Off", "Auto", "16:10", "16:9",
                                           "21:9"};

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

static void ToggleExclusiveFullScreen(void)
{
    toggle_exclusive_fullscreen = true;
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

    {"Resolution Scale", S_THERMO | S_THRM_SIZE11 | S_ACTION, M_X_THRM11,
     M_THRM_SPC, {"resolution_scale"}, m_null, input_null, str_resolution_scale,
     ResetVideoHeight},

    {"Dynamic Resolution", S_ONOFF, M_X, M_SPC, {"dynamic_resolution"},
     m_null, input_null, str_empty, ResetVideoHeight},

    {"Widescreen", S_CHOICE, M_X, M_SPC, {"widescreen"}, m_null, input_null,
     str_widescreen, ResetVideo},

    {"FOV", S_THERMO, M_X_THRM8, M_THRM_SPC, {"fov"}, m_null, input_null,
     str_empty, UpdateFOV},

    {"Fullscreen", S_ONOFF, M_X, M_SPC, {"fullscreen"}, m_null, input_null,
     str_empty, ToggleFullScreen},

    {"Exclusive Fullscreen", S_ONOFF, M_X, M_SPC, {"exclusive_fullscreen"},
     m_null, input_null, str_empty, ToggleExclusiveFullScreen},

    MI_GAP,

    {"Uncapped Framerate", S_ONOFF, M_X, M_SPC, {"uncapped"}, m_null, input_null,
     str_empty, UpdateFPSLimit},

    {"Framerate Limit", S_NUM, M_X, M_SPC, {"fpslimit"}, m_null, input_null,
     str_empty, UpdateFPSLimit},

    {"VSync", S_ONOFF, M_X, M_SPC, {"use_vsync"}, m_null, input_null, str_empty,
     I_ToggleVsync},

    MI_GAP,

    {"Gamma Correction", S_THERMO, M_X_THRM8, M_THRM_SPC, {"gamma2"},
     m_null, input_null, str_gamma, MN_ResetGamma},

    {"Level Brightness", S_THERMO | S_THRM_SIZE4 | S_STRICT, M_X_THRM4,
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

    I_SetSoundModule();
}

static void SetMidiPlayer(void)
{
    S_StopMusic();
    I_SetMidiPlayer();
    S_SetMusicVolume(snd_MusicVolume);
    S_RestartMusic();
}

static const char *equalizer_preset_strings[] = {"Off", "Classical", "Rock", "Vocal"};

static setup_menu_t gen_settings2[] = {

    {"Sound Volume", S_THERMO, M_X_THRM8, M_THRM_SPC, {"sfx_volume"},
     m_null, input_null, str_empty, UpdateSfxVolume},

    {"Music Volume", S_THERMO, M_X_THRM8, M_THRM_SPC, {"music_volume"},
     m_null, input_null, str_empty, UpdateMusicVolume},

    MI_GAP,

    {"Sound Module", S_CHOICE, M_X, M_SPC, {"snd_module"}, m_null, input_null,
     str_sound_module, SetSoundModule},

    {"Headphones Mode", S_ONOFF, M_X, M_SPC, {"snd_hrtf"}, m_null, input_null,
     str_empty, SetSoundModule},

    {"Pitch-Shifted Sounds", S_ONOFF, M_X, M_SPC, {"pitched_sounds"}},

    // [FG] play sounds in full length
    {"Disable Sound Cutoffs", S_ONOFF, M_X, M_SPC, {"full_sounds"}},

    {"Equalizer Preset", S_CHOICE, M_X, M_SPC, {"snd_equalizer"}, m_null, input_null,
     str_equalizer_preset, I_OAL_EqualizerPreset},

    {"Resampler", S_CHOICE | S_NEXT_LINE, M_X, M_SPC, {"snd_resampler"}, m_null,
     input_null, str_resampler, I_OAL_SetResampler},

    MI_GAP,
    MI_GAP,

    // [FG] music backend
    {"MIDI player", S_CHOICE | S_ACTION | S_NEXT_LINE, M_X, M_SPC,
     {"midi_player_menu"}, m_null, input_null, str_midi_player, SetMidiPlayer},

    MI_END
};

/*
static setup_menu_t gen_settings_eq[] = {
    {"Preamp dB", S_THERMO | S_THRM_SIZE11, M_X_THRM11, M_THRM_SPC,
     {"snd_eq_preamp"}, m_null, input_null, str_empty, I_OAL_SetEqualizer},

    {"Low Gain dB", S_THERMO | S_THRM_SIZE11, M_X_THRM11, M_THRM_SPC,
     {"snd_eq_low_gain"}, m_null, input_null, str_empty, I_OAL_SetEqualizer},

    {"Mid 1 Gain dB", S_THERMO | S_THRM_SIZE11, M_X_THRM11, M_THRM_SPC,
     {"snd_eq_mid1_gain"}, m_null, input_null, str_empty, I_OAL_SetEqualizer},

    {"Mid 2 Gain dB", S_THERMO | S_THRM_SIZE11, M_X_THRM11, M_THRM_SPC,
     {"snd_eq_mid2_gain"}, m_null, input_null, str_empty, I_OAL_SetEqualizer},

    {"High Gain dB", S_THERMO | S_THRM_SIZE11, M_X_THRM11, M_THRM_SPC,
     {"snd_eq_high_gain"}, m_null, input_null, str_empty, I_OAL_SetEqualizer},


    {"Low Cutoff Hz", S_THERMO | S_THRM_SIZE11, M_X_THRM11, M_THRM_SPC,
     {"snd_eq_low_cutoff"}, m_null, input_null, str_empty, I_OAL_SetEqualizer},

    {"Mid 1 Center Hz", S_THERMO | S_THRM_SIZE11, M_X_THRM11, M_THRM_SPC,
     {"snd_eq_mid1_center"}, m_null, input_null, str_empty, I_OAL_SetEqualizer},

    {"Mid 2 Center Hz", S_THERMO | S_THRM_SIZE11, M_X_THRM11, M_THRM_SPC,
     {"snd_eq_mid2_center"}, m_null, input_null, str_empty, I_OAL_SetEqualizer},

    {"High Cutoff Hz", S_THERMO | S_THRM_SIZE11, M_X_THRM11, M_THRM_SPC,
     {"snd_eq_high_cutoff"}, m_null, input_null, str_empty, I_OAL_SetEqualizer},


    {"Mid 1 Width Oct", S_THERMO | S_THRM_SIZE11, M_X_THRM11, M_THRM_SPC,
     {"snd_eq_mid1_width"}, m_null, input_null, str_empty, I_OAL_SetEqualizer},

    {"Mid 2 Width Oct", S_THERMO | S_THRM_SIZE11, M_X_THRM11, M_THRM_SPC,
     {"snd_eq_mid2_width"}, m_null, input_null, str_empty, I_OAL_SetEqualizer},

    MI_END
};
*/

static const char **GetResamplerStrings(void)
{
    const char **strings = I_OAL_GetResamplerStrings();
    DisableItem(!strings, gen_settings2, "snd_resampler");
    return strings;
}

void MN_UpdateFreeLook(void)
{
    P_UpdateDirectVerticalAiming();

    if (!mouselook && !padlook)
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

#define CNTR_X 162

static setup_menu_t gen_settings3[] = {
    // [FG] double click to "use"
    {"Double-Click to \"Use\"", S_ONOFF, CNTR_X, M_SPC, {"dclick_use"}},

    {"Free Look", S_ONOFF, CNTR_X, M_SPC, {"mouselook"}, m_null, input_null,
     str_empty, MN_UpdateFreeLook},

    // [FG] invert vertical axis
    {"Invert Look", S_ONOFF, CNTR_X, M_SPC,
     {"mouse_y_invert"}, m_null, input_null, str_empty,
     G_UpdateMouseVariables},

    MI_GAP,

    {"Turn Sensitivity", S_THERMO | S_THRM_SIZE11, CNTR_X, M_THRM_SPC,
     {"mouse_sensitivity"}, m_null, input_null, str_empty,
     G_UpdateMouseVariables},

    {"Look Sensitivity", S_THERMO | S_THRM_SIZE11, CNTR_X, M_THRM_SPC,
     {"mouse_sensitivity_y_look"}, m_null, input_null, str_empty,
     G_UpdateMouseVariables},

    {"Move Sensitivity", S_THERMO | S_THRM_SIZE11, CNTR_X, M_THRM_SPC,
     {"mouse_sensitivity_y"}, m_null, input_null, str_empty,
     G_UpdateMouseVariables},

    {"Strafe Sensitivity", S_THERMO | S_THRM_SIZE11, CNTR_X, M_THRM_SPC,
     {"mouse_sensitivity_strafe"}, m_null, input_null, str_empty,
     G_UpdateMouseVariables},

    MI_GAP,

    {"Mouse acceleration", S_THERMO, CNTR_X, M_THRM_SPC,
     {"mouse_acceleration"}, m_null, input_null, str_mouse_accel,
     G_UpdateMouseVariables},

    MI_END
};

static const char *layout_strings[] = {"Default", "Swap", "Legacy",
                                       "Legacy Swap"};

static const char *curve_strings[] = {
    "",       "",    "",    "",        "",    "",    "",
    "",       "",    "", // Dummy values, start at 1.0.
    "Linear", "1.1", "1.2", "1.3",     "1.4", "1.5", "1.6",
    "1.7",    "1.8", "1.9", "Squared", "2.1", "2.2", "2.3",
    "2.4",    "2.5", "2.6", "2.7",     "2.8", "2.9", "Cubed"
};

static setup_menu_t gen_settings4[] = {

    {"Stick Layout",S_CHOICE, CNTR_X, M_SPC, {"joy_layout"}, m_null, input_null,
     str_layout, I_ResetController},

    {"Free Look", S_ONOFF, CNTR_X, M_SPC, {"padlook"}, m_null, input_null,
     str_empty, MN_UpdateFreeLook},

    {"Invert Look", S_ONOFF, CNTR_X, M_SPC, {"joy_invert_look"},
     m_null, input_null, str_empty, G_UpdateControllerVariables},

    MI_GAP,

    {"Turn Sensitivity", S_THERMO | S_THRM_SIZE11, CNTR_X, M_THRM_SPC,
     {"joy_sensitivity_turn"}, m_null, input_null, str_empty, I_ResetController},

    {"Look Sensitivity", S_THERMO | S_THRM_SIZE11, CNTR_X, M_THRM_SPC,
     {"joy_sensitivity_look"}, m_null, input_null, str_empty, I_ResetController},

    {"Extra Turn Sensitivity", S_THERMO | S_THRM_SIZE11, CNTR_X, M_THRM_SPC,
     {"joy_extra_sensitivity_turn"}, m_null, input_null, str_empty,
     I_ResetController},

    MI_GAP,

    {"Movement Curve", S_THERMO, CNTR_X, M_THRM_SPC,
     {"joy_response_curve_movement"}, m_null, input_null, str_curve,
     I_ResetController},

    {"Camera Curve", S_THERMO, CNTR_X, M_THRM_SPC, {"joy_response_curve_camera"},
     m_null, input_null, str_curve, I_ResetController},

    {"Movement Deadzone", S_THERMO | S_PCT, CNTR_X, M_THRM_SPC,
     {"joy_deadzone_movement"}, m_null, input_null, str_empty, I_ResetController},

    {"Camera Deadzone", S_THERMO | S_PCT, CNTR_X, M_THRM_SPC,
     {"joy_deadzone_camera"}, m_null, input_null, str_empty, I_ResetController},

    MI_END
};

static void SmoothLight(void)
{
    setsmoothlight = true;
    setsizeneeded = true; // run R_ExecuteSetViewSize
}

static const char *menu_backdrop_strings[] = {"Off", "Dark", "Texture"};

static const char *endoom_strings[] = {"off", "on", "PWAD only"};

static setup_menu_t gen_settings5[] = {

    {"Smooth Pixel Scaling", S_ONOFF, M_X, M_SPC, {"smooth_scaling"},
     m_null, input_null, str_empty, ResetVideo},

    {"Sprite Translucency", S_ONOFF | S_STRICT, M_X, M_SPC, {"translucency"}},

    {"Translucency Filter", S_NUM | S_ACTION | S_PCT, M_X, M_SPC,
     {"tran_filter_pct"}, m_null, input_null, str_empty, MN_Trans},

    MI_GAP,

    {"Voxels", S_ONOFF | S_STRICT, M_X, M_SPC, {"voxels_rendering"}},

    {"Brightmaps", S_ONOFF | S_STRICT, M_X, M_SPC, {"brightmaps"},
     m_null, input_null, str_empty, R_InitDrawFunctions},

    {"Stretch Short Skies", S_ONOFF, M_X, M_SPC, {"stretchsky"},
     m_null, input_null, str_empty, R_InitSkyMap},

    {"Linear Sky Scrolling", S_ONOFF, M_X, M_SPC, {"linearsky"},
     m_null, input_null, str_empty, R_InitPlanes},

    {"Swirling Flats", S_ONOFF, M_X, M_SPC, {"r_swirl"}},

    {"Smooth Diminishing Lighting", S_ONOFF, M_X, M_SPC, {"smoothlight"},
     m_null, input_null, str_empty, SmoothLight},

    MI_GAP,

    {"Menu Backdrop", S_CHOICE, M_X, M_SPC, {"menu_backdrop"},
     m_null, input_null, str_menu_backdrop},

    {"Show ENDOOM Screen", S_CHOICE, M_X, M_SPC, {"show_endoom"},
     m_null, input_null, str_endoom},

    MI_END
};

const char *default_skill_strings[] = {
    // dummy first option because defaultskill is 1-based
    "", "ITYTD", "HNTR", "HMP", "UV", "NM"
};

static const char *death_use_action_strings[] = {"default", "last save",
                                                 "nothing"};

static const char *screen_melt_strings[] = {"Off", "Melt", "Crossfade", "Fizzle"};

static const char *invul_mode_strings[] = {"Vanilla", "MBF", "Gray"};

void MN_ResetTimeScale(void)
{
    if (strictmode || D_CheckNetConnect())
    {
        I_SetTimeScale(100);
        return;
    }

    int time_scale = realtic_clock_rate;

    //!
    // @arg <n>
    // @category game
    //
    // Increase or decrease game speed, percentage of normal.
    //

    int p = M_CheckParmWithArgs("-speed", 1);

    if (p)
    {
        time_scale = M_ParmArgToInt(p);
        if (time_scale < 10 || time_scale > 1000)
        {
            I_Error(
                "Invalid parameter '%d' for -speed, valid values are 10-1000.",
                time_scale);
        }
    }

    I_SetTimeScale(time_scale);
}

static setup_menu_t gen_settings6[] = {

    {"Quality of life", S_SKIP | S_TITLE, M_X, M_SPC},

    {"Screen wipe effect", S_CHOICE | S_STRICT, M_X, M_SPC, {"screen_melt"},
     m_null, input_null, str_screen_melt},

    {"On death action", S_CHOICE, M_X, M_SPC, {"death_use_action"},
     m_null, input_null, str_death_use_action},

    {"Demo progress bar", S_ONOFF, M_X, M_SPC, {"demobar"}},

    {"Screen flashes", S_ONOFF | S_STRICT, M_X, M_SPC, {"palette_changes"}},

    {"Invulnerability effect", S_CHOICE | S_STRICT, M_X, M_SPC, {"invul_mode"},
     m_null, input_null, str_invul_mode, R_InvulMode},

    {"Organize save files", S_ONOFF | S_PRGWARN, M_X, M_SPC,
     {"organize_savefiles"}},

    MI_GAP,

    {"Miscellaneous", S_SKIP | S_TITLE, M_X, M_SPC},

    {"Game speed", S_NUM | S_STRICT | S_PCT, M_X, M_SPC, {"realtic_clock_rate"},
     m_null, input_null, str_empty, MN_ResetTimeScale},

    {"Default Skill", S_CHOICE | S_LEVWARN, M_X, M_SPC, {"default_skill"},
     m_null, input_null, str_default_skill},

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
}

// The drawing part of the General Setup initialization. Draw the
// background, title, instruction line, and items.

void MN_DrawGeneral(void)
{
    inhelpscreens = true;

    DrawBackground("FLOOR4_6"); // Draw background
    MN_DrawTitle(114, 2, "M_GENERL", "General");
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

static void ResetDefaults()
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
        if (dp->setupscreen != setup_screen)
        {
            continue;
        }

        setup_menu_t **screens = setup_screens[setup_screen];

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
                        free(dp->location->s);
                        dp->location->s = strdup(dp->defaultvalue.s);
                    }
                    else if (dp->type == number)
                    {
                        dp->location->i = dp->defaultvalue.i;
                    }

                    if (flags & (S_LEVWARN | S_PRGWARN))
                    {
                        warn |= flags & (S_LEVWARN | S_PRGWARN);
                    }
                    else if (dp->current)
                    {
                        if (dp->type == string)
                        {
                            dp->current->s = dp->location->s;
                        }
                        else if (dp->type == number)
                        {
                            dp->current->i = dp->location->i;
                        }
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

        if (c == '\x1b')
        {
            if (ch)
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
        }

        c = M_ToUpper(c) - HU_FONTSTART;
        if (c < 0 || c > HU_FONTSIZE)
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
            V_DrawPatchTRTR(cx, cy, hu_font[c], cr, cr2);
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

static void DrawMenuStringEx(int flags, int x, int y, int color)
{
    if (ItemDisabled(flags))
    {
        MN_DrawStringCR(x, y, cr_dark, NULL, menu_buffer);
    }
    else if (flags & S_HILITE)
    {
        if (color == CR_NONE)
        {
            MN_DrawStringCR(x, y, cr_bright, NULL, menu_buffer);
        }
        else
        {
            MN_DrawStringCR(x, y, colrngs[color], cr_bright, menu_buffer);
        }
    }
    else
    {
        DrawMenuString(x, y, color);
    }
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
        if (c < 0 || c > HU_FONTSIZE)
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

enum
{
    prog,
    art,
    test,
    test_stub,
    test_stub2,
    canine,
    musicsfx, /*musicsfx_stub,*/
    woof,     // [FG] shamelessly add myself to the Credits page ;)
    adcr,
    adcr_stub,
    special,
    special_stub,
    special_stub2,
};

enum
{
    cr_prog = 1,
    cr_art,
    cr_test,
    cr_canine,
    cr_musicsfx,
    cr_woof, // [FG] shamelessly add myself to the Credits page ;)
    cr_adcr,
    cr_special,
};

#define CR_S  9
#define CR_X  152
#define CR_X2 (CR_X + 8)
#define CR_Y  31
#define CR_SH 2

static setup_menu_t cred_settings[] = {

    {"Programmer", S_SKIP | S_CREDIT, CR_X, CR_Y + CR_S * prog + CR_SH * cr_prog},
    {"Lee Killough", S_SKIP | S_CREDIT | S_LEFTJUST, CR_X2,
     CR_Y + CR_S *prog + CR_SH * cr_prog},

    {"Artist", S_SKIP | S_CREDIT, CR_X, CR_Y + CR_S * art + CR_SH * cr_art},
    {"Len Pitre", S_SKIP | S_CREDIT | S_LEFTJUST, CR_X2,
     CR_Y + CR_S *art + CR_SH * cr_art},

    {"PlayTesters", S_SKIP | S_CREDIT, CR_X, CR_Y + CR_S * test + CR_SH * cr_test},
    {"Ky (Rez) Moffet", S_SKIP | S_CREDIT | S_LEFTJUST, CR_X2,
     CR_Y + CR_S * test + CR_SH * cr_test},
    {"Len Pitre", S_SKIP | S_CREDIT | S_LEFTJUST, CR_X2,
     CR_Y + CR_S * (test + 1) + CR_SH * cr_test},
    {"James (Quasar) Haley", S_SKIP | S_CREDIT | S_LEFTJUST, CR_X2,
     CR_Y + CR_S *(test + 2) + CR_SH * cr_test},

    {"Canine Consulting", S_SKIP | S_CREDIT, CR_X,
     CR_Y + CR_S * canine + CR_SH * cr_canine},
    {"Longplain Kennels, Reg'd", S_SKIP | S_CREDIT | S_LEFTJUST, CR_X2,
     CR_Y + CR_S * canine + CR_SH * cr_canine},

    // haleyjd 05/12/09: changed Allegro credits to Team Eternity
    {"SDL Port By", S_SKIP | S_CREDIT, CR_X,
     CR_Y + CR_S * musicsfx + CR_SH * cr_musicsfx},
    {"Team Eternity", S_SKIP | S_CREDIT | S_LEFTJUST, CR_X2,
     CR_Y + CR_S * musicsfx + CR_SH * cr_musicsfx},

    // [FG] shamelessly add myself to the Credits page ;)
    {"Woof! by", S_SKIP | S_CREDIT, CR_X, CR_Y + CR_S * woof + CR_SH * cr_woof},
    {"Fabian Greffrath", S_SKIP | S_CREDIT | S_LEFTJUST, CR_X2,
     CR_Y + CR_S * woof + CR_SH * cr_woof},

    {"Additional Credit To", S_SKIP | S_CREDIT, CR_X,
     CR_Y + CR_S * adcr + CR_SH * cr_adcr},
    {"id Software", S_SKIP | S_CREDIT | S_LEFTJUST, CR_X2,
     CR_Y + CR_S * adcr + CR_SH * cr_adcr},
    {"TeamTNT", S_SKIP | S_CREDIT | S_LEFTJUST, CR_X2,
     CR_Y + CR_S * (adcr + 1) + CR_SH * cr_adcr},

    {"Special Thanks To", S_SKIP | S_CREDIT, CR_X,
     CR_Y + CR_S * special + CR_SH * cr_special},
    {"John Romero", S_SKIP | S_CREDIT | S_LEFTJUST, CR_X2,
     CR_Y + CR_S * (special + 0) + CR_SH * cr_special},
    {"Joel Murdoch", S_SKIP | S_CREDIT | S_LEFTJUST, CR_X2,
     CR_Y + CR_S * (special + 1) + CR_SH * cr_special},

    {0, S_SKIP | S_END, m_null}
};

void MN_DrawCredits(void) // killough 10/98: credit screen
{
    char mbftext_s[32];
    M_snprintf(mbftext_s, sizeof(mbftext_s), PROJECT_STRING);
    inhelpscreens = true;
    DrawBackground(gamemode == shareware ? "CEIL5_1" : "MFLR8_4");
    MN_DrawTitle(42, 9, "MBFTEXT", mbftext_s);
    DrawScreenItems(cred_settings);
}

boolean MN_SetupCursorPostion(int x, int y)
{
    if (!setup_active || setup_select)
    {
        return false;
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

        if (MN_PointInsideRect(&item->rect, x, y))
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

    def->location->i = !def->location->i; // killough 8/15/98

    // killough 8/15/98: add warning messages

    if (flags & (S_LEVWARN | S_PRGWARN))
    {
        warn_about_changes(flags);
    }
    else if (def->current)
    {
        def->current->i = def->location->i;
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
    int value = def->location->i;

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

        if (def->location->i != value)
        {
            M_StartSound(sfx_stnmov);
        }
        def->location->i = value;

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

        if (def->location->i != value)
        {
            M_StartSound(sfx_stnmov);
        }
        def->location->i = value;

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
        else if (def->current)
        {
            def->current->i = def->location->i;
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

    if (action == MENU_ESCAPE) // Exit key = no change
    {
        if (flags & (S_CHOICE | S_CRITEM | S_THERMO) && setup_cancel != -1)
        {
            def->location->i = setup_cancel;
            setup_cancel = -1;
        }

        SelectDone(current_item); // phares 4/17/98
        setup_gather = false;     // finished gathering keys, if any
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

        menu_input = old_menu_input;
        MN_ResetMouseCursor();

        if (action == MENU_ENTER)
        {
            if (gather_count) // Any input?
            {
                int value;

                gather_buffer[gather_count] = 0;
                value = atoi(gather_buffer); // Integer value

                if ((def->limit.min != UL && value < def->limit.min)
                    || (def->limit.max != UL && value > def->limit.max))
                {
                    warn_about_changes(S_BADVAL);
                }
                else
                {
                    def->location->i = value;

                    // killough 8/9/98: fix numeric vars
                    // killough 8/15/98: add warning message

                    if (flags & (S_LEVWARN | S_PRGWARN))
                    {
                        warn_about_changes(flags);
                    }
                    else if (def->current)
                    {
                        def->current->i = value;
                    }

                    if (current_item->action) // killough 10/98
                    {
                        current_item->action();
                    }
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

    print_warning_about_changes = false; // killough 10/98
    while (current_menu[set_item_on++].m_flags & S_SKIP)
        ;
    current_menu[--set_item_on].m_flags |= S_HILITE;

    M_StartSound(sfx_pstop); // killough 10/98
    return true;
}

boolean MN_SetupResponder(menu_action_t action, int ch)
{
    // phares 3/26/98 - 4/11/98:
    // Setup screen key processing

    if (!setup_active)
    {
        return false;
    }

    if (menu_input != mouse_mode && current_tabs)
    {
        current_tabs[highlight_tab].flags &= ~S_HILITE;
    }

    setup_menu_t *current_item = current_menu + set_item_on;

    // phares 4/19/98:
    // Catch the response to the 'reset to default?' verification
    // screen

    if (default_verify)
    {
        if (M_ToUpper(ch) == 'Y')
        {
            ResetDefaults();
            default_verify = false;
            SelectDone(current_item);
        }
        else if (M_ToUpper(ch) == 'N')
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
                    if (p->m_flags & S_WEAP && p->var.def->location->i == ch
                        && p != current_item)
                    {
                        p->var.def->location->i =
                            current_item->var.def->location->i;
                        goto end;
                    }
                }
            }
        end:
            current_item->var.def->location->i = ch;
        }

        SelectDone(current_item); // phares 4/17/98
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
        SetItemOn(set_item_on);
        SetPageIndex(current_page);

        if (action == MENU_ESCAPE) // Clear all menus
        {
            MN_ClearMenus();
        }
        else if (action == MENU_BACKSPACE)
        {
            MN_Back();
        }

        current_item->m_flags &= ~(S_HILITE | S_SELECT); // phares 4/19/98
        setup_active = false;
        set_keybnd_active = false;
        set_weapon_active = false;
        default_verify = false;              // phares 4/19/98
        print_warning_about_changes = false; // [FG] reset
        HU_Start(); // catch any message changes // phares 4/19/98
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

    if (SetupTab())
    {
        return true;
    }

    static setup_menu_t *active_thermo = NULL;

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
            else if (def->current)
            {
                def->current->i = def->location->i;
            }

            if (active_thermo->action)
            {
                active_thermo->action();
            }
        }
        active_thermo = NULL;
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
        value = BETWEEN(min, max, value);

        if (value != def->location->i)
        {
            def->location->i = value;

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

        int value = def->location->i;

        if (NextItemAvailable(current_item))
        {
            value++;
        }
        else if (def->limit.min != UL)
        {
            value = def->limit.min;
        }

        if (def->location->i != value)
        {
            M_StartSound(sfx_stnmov);
        }
        def->location->i = value;

        if (current_item->action)
        {
            current_item->action();
        }

        if (flags & (S_LEVWARN | S_PRGWARN))
        {
            warn_about_changes(flags);
        }
        else if (def->current)
        {
            def->current->i = def->location->i;
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
        if (c < 0 || c > HU_FONTSIZE)
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

    if (patch_lump >= 0)
    {
        V_DrawPatch(x, y, V_CachePatchNum(patch_lump, PU_CACHE));
    }
    else
    {
        // patch doesn't exist, draw some text in place of it
        if (!MN_DrawFon2String(
                SCREENWIDTH / 2 - MN_GetFon2PixelWidth(alttext) / 2,
                y, NULL, alttext))
        {
            M_snprintf(menu_buffer, sizeof(menu_buffer), "%s", alttext);
            DrawMenuString(
                SCREENWIDTH / 2 - MN_StringWidth(alttext) / 2,
                y + 8 - MN_StringHeight(alttext) / 2, // assumes patch height 16
                CR_TITLE);
        }
    }
}

static const char **selectstrings[] = {
    NULL, // str_empty
    layout_strings,
    curve_strings,
    center_weapon_strings,
    screensize_strings,
    hudtype_strings,
    NULL, // str_hudmode
    show_widgets_strings,
    show_adv_widgets_strings,
    crosshair_strings,
    crosshair_target_strings,
    hudcolor_strings,
    overlay_strings,
    automap_preset_strings,
    automap_keyed_door_strings,
    NULL, // str_resolution_scale
    NULL, // str_midi_player
    gamma_strings,
    sound_module_strings,
    NULL, // str_resampler
    equalizer_preset_strings,
    NULL, // str_mouse_accel
    default_skill_strings,
    default_complevel_strings,
    endoom_strings,
    death_use_action_strings,
    menu_backdrop_strings,
    widescreen_strings,
    bobbing_pct_strings,
    screen_melt_strings,
    invul_mode_strings,
};

static const char **GetStrings(int id)
{
    if (id > str_empty && id < arrlen(selectstrings))
    {
        return selectstrings[id];
    }

    return NULL;
}

static void UpdateHUDModeStrings(void)
{
    selectstrings[str_hudmode] = GetHUDModeStrings();
}

static const char **GetMidiPlayerStrings(void)
{
    return I_DeviceList();
}

void MN_InitMenuStrings(void)
{
    UpdateHUDModeStrings();
    selectstrings[str_resolution_scale] = GetResolutionScaleStrings();
    selectstrings[str_midi_player] = GetMidiPlayerStrings();
    selectstrings[str_mouse_accel] = GetMouseAccelStrings();
    selectstrings[str_resampler] = GetResamplerStrings();
}

void MN_SetupResetMenu(void)
{
    DisableItem(force_strictmode, comp_settings1, "strictmode");
    DisableItem(force_complevel != CL_NONE, comp_settings1, "default_complevel");
    DisableItem(M_ParmExists("-pistolstart"), comp_settings1, "pistolstart");
    DisableItem(M_ParmExists("-uncapped") || M_ParmExists("-nouncapped"),
                gen_settings1, "uncapped");
    DisableItem(deh_set_blood_color, enem_settings1, "colored_blood");
    DisableItem(!brightmaps_found || force_brightmaps, gen_settings5,
                "brightmaps");
    UpdateInterceptsEmuItem();
    UpdateCrosshairItems();
    UpdateCenteredWeaponItem();
}

void MN_BindMenuVariables(void)
{
    BIND_NUM(resolution_scale, 0, 0, UL, "Position of resolution scale slider (do not modify)");
    BIND_NUM_GENERAL(menu_backdrop, MENU_BG_DARK, MENU_BG_OFF, MENU_BG_TEXTURE,
        "Menu backdrop (0 = Off; 1 = Dark; 2 = Texture)");
}

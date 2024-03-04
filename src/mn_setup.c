

#include "d_main.h"
#include "doomstat.h"
#include "mn_menu.h"
#include "mn_setup.h"

#include "doomtype.h"
#include "doomdef.h"
#include "am_map.h"
#include "d_event.h"
#include "g_game.h"
#include "hu_lib.h"
#include "hu_stuff.h"
#include "i_gamepad.h"
#include "i_sound.h"
#include "i_video.h"
#include "i_timer.h"
#include "m_argv.h"
#include "m_array.h"
#include "m_fixed.h"
#include "m_input.h"
#include "m_swap.h"
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
#include "v_video.h"
#include "m_misc.h"
#include "m_misc2.h"
#include "w_wad.h"

static int M_GetKeyString(int c, int offset);
static void M_DrawMenuString(int cx, int cy, int color);
static void M_DrawMenuStringEx(int flags, int x, int y, int color);

int warning_about_changes, print_warning_about_changes;

// killough 8/15/98: warn about changes not being committed until next game
#define warn_about_changes(x) (warning_about_changes = (x), \
                               print_warning_about_changes = 2)

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

#define M_SPC        9
#define M_X          240
#define M_Y          (29 + M_SPC)
#define M_Y_WARN     (SCREENHEIGHT - 15)

#define M_THRM_STEP   8
#define M_THRM_HEIGHT 13
#define M_THRM_SIZE4  4
#define M_THRM_SIZE8  8
#define M_THRM_SIZE11 11
#define M_X_THRM4     (M_X - (M_THRM_SIZE4 + 3) * M_THRM_STEP)
#define M_X_THRM8     (M_X - (M_THRM_SIZE8 + 3) * M_THRM_STEP)
#define M_X_THRM11    (M_X - (M_THRM_SIZE11 + 3) * M_THRM_STEP)
#define M_THRM_TXT_OFFSET 3
#define M_THRM_SPC    (M_THRM_HEIGHT + 1)
#define M_THRM_UL_VAL 50

// Final entry
#define MI_END \
  {0, S_SKIP|S_END, m_null}

// Button for resetting to defaults
#define MI_RESET \
  {0, S_RESET, m_null, X_BUTTON, Y_BUTTON}

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

boolean setup_active             = false; // in one of the setup screens
static boolean set_keybnd_active = false; // in key binding setup screens
static boolean set_weapon_active = false; // in weapons setup screen
static boolean setup_select      = false; // changing an item
static boolean setup_gather      = false; // gathering keys for value
boolean default_verify           = false; // verify reset defaults decision

/////////////////////////////
//
// set_menu_itemon is an index that starts at zero, and tells you which
// item on the current screen the cursor is sitting on.
//
// current_setup_menu is a pointer to the current setup menu table.

static int set_menu_itemon; // which setup item is selected?   // phares 3/98
static setup_menu_t *current_setup_menu; // points to current setup menu table
static int mult_screens_index; // the index of the current screen in a set

typedef struct
{
  const char *text;
  setup_menu_t *page;
  mrect_t rect;
  int flags;
} setup_tab_t;

static setup_tab_t *current_setup_tabs;
static int set_tab_on;

// [FG] save the setup menu's itemon value in the S_END element's x coordinate

static int M_GetSetupMenuItemOn (void)
{
  const setup_menu_t* menu = current_setup_menu;

  if (menu)
  {
    while (!(menu->m_flags & S_END))
      menu++;

    return menu->m_x;
  }

  return 0;
}

static void M_SetSetupMenuItemOn (const int x)
{
  setup_menu_t* menu = current_setup_menu;

  if (menu)
  {
    while (!(menu->m_flags & S_END))
      menu++;

    menu->m_x = x;
  }
}

// [FG] save the index of the current screen in the first page's S_END element's y coordinate

static int M_GetMultScreenIndex (setup_menu_t *const *const pages)
{
  if (pages)
  {
    const setup_menu_t* menu = pages[0];

    if (menu)
    {
      while (!(menu->m_flags & S_END))
        menu++;

      return menu->m_y;
    }
  }

  return 0;
}

static void M_SetMultScreenIndex (const int y)
{
    if (!current_setup_tabs)
    {
        return;
    }

    setup_menu_t * menu = current_setup_tabs[0].page;

    while (!(menu->m_flags & S_END))
        menu++;

    menu->m_y = y;
}

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

static void M_DrawBackground(char *patchname)
{
  if (setup_active && menu_backdrop != MENU_BG_TEXTURE)
    return;

  V_DrawBackground(patchname);
}

/////////////////////////////
//
// Data that's used by the Setup screen code
//
// Establish the message colors to be used

#define CR_TITLE  CR_GOLD
#define CR_SET    CR_GREEN
#define CR_ITEM   CR_NONE
#define CR_HILITE CR_NONE //CR_ORANGE
#define CR_SELECT CR_GRAY

// chat strings must fit in this screen space
// killough 10/98: reduced, for more general uses
#define MAXCHATWIDTH         272

/////////////////////////////
//
// phares 4/17/98:
// Added 'Reset to Defaults' Button to Setup Menu screens
// This is a small button that sits in the upper-right-hand corner of
// the first screen for each group. It blinks when selected, thus the
// two patches, which it toggles back and forth.

static char ResetButtonName[2][8] = {"M_BUTT1","M_BUTT2"};

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

    str_mouse_accel,

    str_default_skill,
    str_default_complevel,
    str_endoom,
    str_death_use_action,
    str_menu_backdrop,
    str_widescreen,
    str_bobbing_pct,
    str_invul_mode,
};

static const char **GetStrings(int id);

static void M_DrawMenuStringEx(int flags, int x, int y, int color);

static boolean ItemDisabled(int flags)
{
    if ((flags & S_DISABLE) ||
        (flags & S_STRICT && default_strictmode) ||
        (flags & S_BOOM && default_complevel < CL_BOOM) ||
        (flags & S_MBF && default_complevel < CL_MBF) ||
        (flags & S_VANILLA && default_complevel != CL_VANILLA))
    {
        return true;
    }

    return false;
}

static boolean ItemSelected(setup_menu_t *s)
{
  if (s == current_setup_menu + set_menu_itemon && whichSkull)
    return true;

  return false;
}

static boolean PrevItemAvailable(setup_menu_t *s)
{
    int value = s->var.def->location->i;
    int min   = s->var.def->limit.min;

    return value > min;
}

static boolean NextItemAvailable(setup_menu_t *s)
{
    int value = s->var.def->location->i;
    int max   = s->var.def->limit.max;

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
        if (flags & S_HILITE)
            strcpy(menu_buffer, "< ");
    }
    else if (flags & (S_CHOICE|S_CRITEM|S_THERMO))
    {
        if (setup_select && PrevItemAvailable(s))
            strcpy(menu_buffer, "< ");
        else if (!setup_select)
            strcpy(menu_buffer, "> ");
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
        if (flags & S_HILITE)
            strcat(menu_buffer, " >");
    }
    else if (flags & (S_CHOICE|S_CRITEM|S_THERMO))
    {
        if (setup_select && NextItemAvailable(s))
            strcat(menu_buffer, " >");
        else if (!setup_select)
            strcat(menu_buffer, " <");
    }
    else if (!setup_select)
    {
        strcat(menu_buffer, " <");
    }
}

#define M_TAB_Y      22
#define M_TAB_OFFSET 8

static void M_DrawTabs(void)
{
    setup_tab_t *tabs = current_setup_tabs;

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
            rect->w = M_GetPixelWidth(tabs[i].text);
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
        M_DrawMenuStringEx(tabs[i].flags, x, rect->y, CR_GOLD);
        if (i == mult_screens_index)
        {
            V_FillRect(x + video.deltaw, rect->y + M_SPC, rect->w, 1,
                       cr_gold[cr_shaded[v_lightest_color]]);
        }

        rect->x = x;

        x += rect->w;
    }
}

static void M_DrawItem(setup_menu_t* s, int accum_y)
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

        const int index = (flags & (S_HILITE|S_SELECT)) ? whichSkull : 0;
        patch_t *patch = W_CacheLumpName(ResetButtonName[index], PU_CACHE);
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
    int color =
          flags & S_TITLE ? CR_TITLE :
          flags & S_SELECT ? CR_SELECT :
          flags & S_HILITE ? CR_HILITE : CR_ITEM; // killough 10/98

    if (!(flags & S_NEXT_LINE))
    {
        BlinkingArrowLeft(s);
    }

    // killough 10/98: support left-justification:
    strcat(menu_buffer, text);
    w = M_GetPixelWidth(menu_buffer);
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

    M_DrawMenuStringEx(flags, x, y, color);
}

// If a number item is being changed, allow up to N keystrokes to 'gather'
// the value. Gather_count tells you how many you have so far. The legality
// of what is gathered is determined by the low/high settings for the item.

#define MAXGATHER 5
static int  gather_count;
static char gather_buffer[MAXGATHER+1];  // killough 10/98: make input character-based

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

static void M_DrawSetupThermo(int x, int y, int width, int size, int dot, byte *cr)
{
  int xx;
  int  i;

  xx = x;
  V_DrawPatchTranslated(xx, y, W_CacheLumpName("M_THERML", PU_CACHE), cr);
  xx += M_THRM_STEP;

  patch_t *patch = W_CacheLumpName("M_THERMM", PU_CACHE);
  for (i = 0; i < width + 1; i++)
  {
    V_DrawPatchTranslated(xx, y, patch, cr);
    xx += M_THRM_STEP;
  }
  V_DrawPatchTranslated(xx, y, W_CacheLumpName("M_THERMR", PU_CACHE), cr);

  if (dot > size)
    dot = size;

  int step = width * M_THRM_STEP * FRACUNIT / size;

  V_DrawPatchTranslated(x + M_THRM_STEP + dot * step / FRACUNIT, y,
                        W_CacheLumpName("M_THERMO", PU_CACHE), cr);
}

static void M_DrawSetting(setup_menu_t *s, int accum_y)
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

  if (flags & S_YESNO)
    {
      strcpy(menu_buffer, s->var.def->location->i ? "YES" : "NO");
      BlinkingArrowRight(s);
      M_DrawMenuStringEx(flags, x, y, color);
      return;
    }

  if (flags & S_ONOFF)
    {
      strcpy(menu_buffer, s->var.def->location->i ? "ON" : "OFF");
      BlinkingArrowRight(s);
      M_DrawMenuStringEx(flags, x, y, color);
      return;
    }

  // Is the item a simple number?

  if (flags & S_NUM)
    {
      // killough 10/98: We must draw differently for items being gathered.
      if (flags & (S_HILITE|S_SELECT) && setup_gather)
      {
        gather_buffer[gather_count] = 0;
        strcpy(menu_buffer, gather_buffer);
      }
      else if (flags & S_PCT)
        M_snprintf(menu_buffer, sizeof(menu_buffer), "%d%%", s->var.def->location->i);
      else
        M_snprintf(menu_buffer, sizeof(menu_buffer), "%d", s->var.def->location->i);

      BlinkingArrowRight(s);
      M_DrawMenuStringEx(flags, x, y, color);
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
          offset += sprintf(menu_buffer + offset, "%s", M_GetNameForMouseB(inputs[i].value));
          break;
        case INPUT_JOYB:
          offset += sprintf(menu_buffer + offset, "%s", M_GetNameForJoyB(inputs[i].value));
          break;
        default:
          break;
      }
    }

    // "NONE"
    if (i == 0)
      M_GetKeyString(0, 0);

    BlinkingArrowRight(s);
    M_DrawMenuStringEx(flags, x, y, color);
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
      sprintf(menu_buffer,"%d", s->var.def->location->i);
      BlinkingArrowRight(s);
      M_DrawMenuStringEx(flags, x, y, color);
      return;
    }

  // [FG] selection of choices

  if (flags & (S_CHOICE|S_CRITEM))
    {
      int i = s->var.def->location->i;
      mrect_t *rect = &s->rect;
      int width;
      const char **strings = GetStrings(s->strings_id);

      menu_buffer[0] = '\0';

      if (flags & S_NEXT_LINE)
        BlinkingArrowLeft(s);

      if (i >= 0 && strings)
        strcat(menu_buffer, strings[i]);
      width = M_GetPixelWidth(menu_buffer);
      if (flags & S_NEXT_LINE)
      {
        y += M_SPC;
        x = M_X - width - 4;
        rect->y = y;
      }

      BlinkingArrowRight(s);
      M_DrawMenuStringEx(flags, x, y, flags & S_CRITEM ? i : color);
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
        width = M_THRM_SIZE11;
      else if (flags & S_THRM_SIZE4)
        width = M_THRM_SIZE4;
      else
        width = M_THRM_SIZE8;

      if (max == UL)
      {
        if (strings)
          max = array_size(strings) - 1;
        else
          max = M_THRM_UL_VAL;
      }

      int thrm_val = BETWEEN(min, max, value);

      byte *cr;
      if (ItemDisabled(flags))
        cr = cr_dark;
      else if (flags & S_HILITE)
        cr = cr_bright;
      else
        cr = NULL;

      mrect_t *rect = &s->rect;
      rect->x = x;
      rect->y = y;
      rect->w = (width + 2) * M_THRM_STEP;
      rect->h = M_THRM_HEIGHT;
      M_DrawSetupThermo(x, y, width, max - min, thrm_val - min, cr);

      if (strings)
        strcpy(menu_buffer, strings[value]);
      else if (flags & S_PCT)
        M_snprintf(menu_buffer, sizeof(menu_buffer), "%d%%", value);
      else
        M_snprintf(menu_buffer, sizeof(menu_buffer), "%d", value);

      BlinkingArrowRight(s);
      M_DrawMenuStringEx(flags, x + M_THRM_STEP + rect->w, y + M_THRM_TXT_OFFSET, color);
    }
}

/////////////////////////////
//
// M_DrawScreenItems takes the data for each menu item and gives it to
// the drawing routines above.

static void M_DrawScreenItems(setup_menu_t* src)
{
  if (print_warning_about_changes > 0)   // killough 8/15/98: print warning
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

    x_warn = SCREENWIDTH/2 - M_GetPixelWidth(menu_buffer)/2;
    M_DrawMenuString(x_warn, M_Y_WARN, CR_RED);
  }

  int accum_y = 0;

  while (!(src->m_flags & S_END))
  {
      if (!(src->m_flags & S_DIRECT))
      {
          if (!accum_y)
              accum_y = src->m_y;
          else
              accum_y += src->m_y;
      }

      // See if we're to draw the item description (left-hand part)

      if (src->m_flags & S_SHOWDESC)
      {
          M_DrawItem(src, accum_y);
      }

      // See if we're to draw the setting (right-hand part)

      if (src->m_flags & S_SHOWSET)
      {
          M_DrawSetting(src, accum_y);
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

static void M_DrawDefVerify()
{
  V_DrawPatch(VERIFYBOXXORG, VERIFYBOXYORG, W_CacheLumpName("M_VBOX", PU_CACHE));

  // The blinking messages is keyed off of the blinking of the
  // cursor skull.

  if (whichSkull) // blink the text
    {
      strcpy(menu_buffer, "Restore defaults? (Y or N)");
      M_DrawMenuString(VERIFYBOXXORG+8,VERIFYBOXYORG+8,CR_RED);
    }
}

void M_DrawDelVerify(void)
{
    V_DrawPatch(VERIFYBOXXORG, VERIFYBOXYORG,
                W_CacheLumpName("M_VBOX", PU_CACHE));

    if (whichSkull)
    {
        M_DrawString(VERIFYBOXXORG + 8, VERIFYBOXYORG + 8, CR_RED,
                     "Delete savegame? (Y or N)");
    }
}

/////////////////////////////
//
// phares 4/18/98:
// M_DrawInstructions writes the instruction text just below the screen title
//
// killough 8/15/98: rewritten

static void M_DrawInstructions()
{
    int flags = current_setup_menu[set_menu_itemon].m_flags;

    if (ItemDisabled(flags) || print_warning_about_changes > 0)
    {
        return;
    }

    if (menu_input == mouse_mode && !(flags & S_HILITE))
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
        else if (flags & (S_YESNO|S_ONOFF))
        {
            if (menu_input == pad_mode)
                s = "[ PadA ] to toggle";
            else
                s = "[ Enter ] to toggle, [ Esc ] to cancel";
        }
        else if (flags & (S_CHOICE|S_CRITEM|S_THERMO))
        {
            if (menu_input == pad_mode)
                s = "[ Left/Right ] to choose, [ PadB ] to cancel";
            else
                s = "[ Left/Right ] to choose, [ Esc ] to cancel";
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

    M_DrawStringCR((SCREENWIDTH - M_GetPixelWidth(s)) / 2, M_Y_WARN, cr_shaded, NULL, s);
}

/////////////////////////////
//
// The Key Binding Screen tables.

#define KB_X  130

// phares 4/16/98:
// X,Y position of reset button. This is the same for every screen, and is
// only defined once here.

#define X_BUTTON 301
#define Y_BUTTON (SCREENHEIGHT - 15 - 3)

// Definitions of the (in this case) five key binding screens.

setup_menu_t keys_settings1[];
setup_menu_t keys_settings2[];
setup_menu_t keys_settings3[];
setup_menu_t keys_settings4[];
setup_menu_t keys_settings5[];
setup_menu_t keys_settings6[];

// The table which gets you from one screen table to the next.

setup_menu_t* keys_settings[] =
{
  keys_settings1,
  keys_settings2,
  keys_settings3,
  keys_settings4,
  keys_settings5,
  keys_settings6,
  NULL
};

static setup_tab_t keys_tabs[] =
{
   { "action", keys_settings1 },
   { "weapon", keys_settings2 },
   { "misc", keys_settings3 },
   { "func", keys_settings4 },
   { "automap", keys_settings5 },
   { "cheats", keys_settings6 },
   { NULL }
};

// Here's an example from this first screen, with explanations.
//
//  {
//  "STRAFE",      // The description of the item ('strafe' key)
//  S_KEY,         // This is a key binding item
//  m_scrn,        // It belongs to the m_scrn group. Its key cannot be
//                 // bound to two items in this group.
//  KB_X,          // The X offset of the start of the right-hand side
//  KB_Y+ 8*8,     // The Y offset of the start of the right-hand side.
//                 // Always given in multiples off a baseline.
//  &key_strafe,   // The variable that holds the key value bound to this
//                    OR a string that holds the config variable name.
//                    OR a pointer to another setup_menu
//  &mousebstrafe, // The variable that holds the mouse button bound to
                   // this. If zero, no mouse button can be bound here.
//  &joybstrafe,   // The variable that holds the joystick button bound to
                   // this. If zero, no mouse button can be bound here.
//  }

// The first Key Binding screen table.
// Note that the Y values are ascending. If you need to add something to
// this table, (well, this one's not a good example, because it's full)
// you need to make sure the Y values still make sense so everything gets
// displayed.
//
// Note also that the first screen of each set has a line for the reset
// button. If there is more than one screen in a set, the others don't get
// the reset button.
//
// Note also that this screen has a "NEXT ->" line. This acts like an
// item, in that 'activating' it moves you along to the next screen. If
// there's a "<- PREV" item on a screen, it behaves similarly, moving you
// to the previous screen. If you leave these off, you can't move from
// screen to screen.

setup_menu_t keys_settings1[] =  // Key Binding screen strings
{
  {"Fire"        , S_INPUT,      m_scrn, KB_X, M_Y, {0}, input_fire},
  {"Forward"     , S_INPUT,      m_scrn, KB_X, M_SPC, {0}, input_forward},
  {"Backward"    , S_INPUT,      m_scrn, KB_X, M_SPC, {0}, input_backward},
  {"Strafe Left" , S_INPUT,      m_scrn, KB_X, M_SPC, {0}, input_strafeleft},
  {"Strafe Right", S_INPUT,      m_scrn, KB_X, M_SPC, {0}, input_straferight},
  {"Use"         , S_INPUT,      m_scrn, KB_X, M_SPC, {0}, input_use},
  {"Run"         , S_INPUT,      m_scrn, KB_X, M_SPC, {0}, input_speed},
  {"Strafe"      , S_INPUT,      m_scrn, KB_X, M_SPC, {0}, input_strafe},
  {"Turn Left"   , S_INPUT,      m_scrn, KB_X, M_SPC, {0}, input_turnleft},
  {"Turn Right"  , S_INPUT,      m_scrn, KB_X, M_SPC, {0}, input_turnright},
  {"180 Turn", S_INPUT|S_STRICT, m_scrn, KB_X, M_SPC, {0}, input_reverse},

  {"", S_SKIP, m_null, KB_X, M_SPC},

  {"Toggles", S_SKIP|S_TITLE,    m_null, KB_X, M_SPC},
  {"Autorun"     , S_INPUT,      m_scrn, KB_X, M_SPC, {0}, input_autorun},
  {"Free Look"   , S_INPUT,      m_scrn, KB_X, M_SPC, {0}, input_freelook},
  {"Vertmouse"   , S_INPUT,      m_scrn, KB_X, M_SPC, {0}, input_novert},

  MI_RESET,

  MI_END
};

setup_menu_t keys_settings2[] =
{
  {"Fist"    , S_INPUT,       m_scrn, KB_X, M_Y,   {0}, input_weapon1},
  {"Pistol"  , S_INPUT,       m_scrn, KB_X, M_SPC, {0}, input_weapon2},
  {"Shotgun" , S_INPUT,       m_scrn, KB_X, M_SPC, {0}, input_weapon3},
  {"Chaingun", S_INPUT,       m_scrn, KB_X, M_SPC, {0}, input_weapon4},
  {"Rocket"  , S_INPUT,       m_scrn, KB_X, M_SPC, {0}, input_weapon5},
  {"Plasma"  , S_INPUT,       m_scrn, KB_X, M_SPC, {0}, input_weapon6},
  {"BFG",      S_INPUT,       m_scrn, KB_X, M_SPC, {0}, input_weapon7},
  {"Chainsaw", S_INPUT,       m_scrn, KB_X, M_SPC, {0}, input_weapon8},
  {"SSG"     , S_INPUT,       m_scrn, KB_X, M_SPC, {0}, input_weapon9},
  {"Best"    , S_INPUT,       m_scrn, KB_X, M_SPC, {0}, input_weapontoggle},

  {"", S_SKIP, m_null, KB_X, M_SPC},

  // [FG] prev/next weapon keys and buttons
  {"Prev"    , S_INPUT,       m_scrn, KB_X, M_SPC, {0}, input_prevweapon},
  {"Next"    , S_INPUT,       m_scrn, KB_X, M_SPC, {0}, input_nextweapon},

  MI_END
};

setup_menu_t keys_settings3[] =
{
  // [FG] reload current level / go to next level
  {"Reload Map/Demo",  S_INPUT, m_scrn, KB_X, M_Y,   {0}, input_menu_reloadlevel},
  {"Next Map",         S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_menu_nextlevel},
  {"Show Stats/Time",  S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_hud_timestats},

  {"", S_SKIP, m_null, KB_X, M_SPC},

  {"Fast-FWD Demo",     S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_demo_fforward},
  {"Finish Demo",      S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_demo_quit},
  {"Join Demo",        S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_demo_join},
  {"Increase Speed",   S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_speed_up},
  {"Decrease Speed",   S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_speed_down},
  {"Default Speed",    S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_speed_default},

  {"", S_SKIP, m_null, KB_X, M_SPC},

  {"Begin Chat",       S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_chat},
  {"Player 1",         S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_chat_dest0},
  {"Player 2",         S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_chat_dest1},
  {"Player 3",         S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_chat_dest2},
  {"Player 4",         S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_chat_dest3},

  MI_END
};

setup_menu_t keys_settings4[] =
{
  // phares 4/13/98:
  // key_help and key_escape can no longer be rebound. This keeps the
  // player from getting themselves in a bind where they can't remember how
  // to get to the menus, and can't remember how to get to the help screen
  // to give them a clue as to how to get to the menus. :)

  // Also, the keys assigned to these functions cannot be bound to other
  // functions. Introduce an S_KEEP flag to show that you cannot swap this
  // key with other keys in the same 'group'. (m_scrn, etc.)

  {"Help",       S_SKIP|S_KEEP, m_scrn, 0,    0,     {&key_help}},
  {"Menu",       S_SKIP|S_KEEP, m_scrn, 0,    0,     {&key_escape}},
  {"Pause",            S_INPUT, m_scrn, KB_X, M_Y,   {0}, input_pause},
  {"Save",             S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_savegame},
  {"Load",             S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_loadgame},
  {"Volume",           S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_soundvolume},
  {"Hud",              S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_hud},
  {"Quicksave",        S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_quicksave},
  {"End Game",         S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_endgame},
  {"Messages",         S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_messages},
  {"Quickload",        S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_quickload},
  {"Quit",             S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_quit},
  {"Gamma Fix",        S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_gamma},
  {"Spy",              S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_spy},
  {"Screenshot",       S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_screenshot},
  {"Clean Screenshot", S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_clean_screenshot},
  {"Larger View",      S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_zoomin},
  {"Smaller View",     S_INPUT, m_scrn, KB_X, M_SPC, {0}, input_zoomout},
  MI_END
};

setup_menu_t keys_settings5[] =  // Key Binding screen strings       
{
  {"Toggle Automap",  S_INPUT, m_map, KB_X, M_Y,   {0}, input_map},
  {"Follow",          S_INPUT, m_map, KB_X, M_SPC, {0}, input_map_follow},
  {"Overlay",         S_INPUT, m_map, KB_X, M_SPC, {0}, input_map_overlay},
  {"Rotate",          S_INPUT, m_map, KB_X, M_SPC, {0}, input_map_rotate},

  {"", S_SKIP, m_null, KB_X, M_SPC},

  {"Zoom In",         S_INPUT, m_map, KB_X, M_SPC, {0}, input_map_zoomin},
  {"Zoom Out",        S_INPUT, m_map, KB_X, M_SPC, {0}, input_map_zoomout},
  {"Shift Up",        S_INPUT, m_map, KB_X, M_SPC, {0}, input_map_up},
  {"Shift Down",      S_INPUT, m_map, KB_X, M_SPC, {0}, input_map_down},
  {"Shift Left",      S_INPUT, m_map, KB_X, M_SPC, {0}, input_map_left},
  {"Shift Right",     S_INPUT, m_map, KB_X, M_SPC, {0}, input_map_right},
  {"Mark Place",      S_INPUT, m_map, KB_X, M_SPC, {0}, input_map_mark},
  {"Clear Last Mark", S_INPUT, m_map ,KB_X, M_SPC, {0}, input_map_clear},
  {"Full/Zoom",       S_INPUT, m_map ,KB_X, M_SPC, {0}, input_map_gobig},
  {"Grid",            S_INPUT, m_map ,KB_X, M_SPC, {0}, input_map_grid},

  MI_END
};

#define CHEAT_X 160

setup_menu_t keys_settings6[] =
{
  {"Fake Archvile Jump",   S_INPUT, m_scrn, CHEAT_X, M_Y,   {0}, input_avj},
  {"God mode/Resurrect",   S_INPUT, m_scrn, CHEAT_X, M_SPC, {0}, input_iddqd},
  {"Ammo & Keys",          S_INPUT, m_scrn, CHEAT_X, M_SPC, {0}, input_idkfa},
  {"Ammo",                 S_INPUT, m_scrn, CHEAT_X, M_SPC, {0}, input_idfa},
  {"No Clipping",          S_INPUT, m_scrn, CHEAT_X, M_SPC, {0}, input_idclip},
  {"Health",               S_INPUT, m_scrn, CHEAT_X, M_SPC, {0}, input_idbeholdh},
  {"Armor",                S_INPUT, m_scrn, CHEAT_X, M_SPC, {0}, input_idbeholdm},
  {"Invulnerability",      S_INPUT, m_scrn, CHEAT_X, M_SPC, {0}, input_idbeholdv},
  {"Berserk",              S_INPUT, m_scrn, CHEAT_X, M_SPC, {0}, input_idbeholds},
  {"Partial Invisibility", S_INPUT, m_scrn, CHEAT_X, M_SPC, {0}, input_idbeholdi},
  {"Radiation Suit",       S_INPUT, m_scrn, CHEAT_X, M_SPC, {0}, input_idbeholdr},
  {"Reveal Map",           S_INPUT, m_scrn, CHEAT_X, M_SPC, {0}, input_iddt},
  {"Light Amplification",  S_INPUT, m_scrn, CHEAT_X, M_SPC, {0}, input_idbeholdl},
  {"No Target",            S_INPUT, m_scrn, CHEAT_X, M_SPC, {0}, input_notarget},
  {"Freeze",               S_INPUT, m_scrn, CHEAT_X, M_SPC, {0}, input_freeze},

  MI_END
};

// Setting up for the Key Binding screen. Turn on flags, set pointers,
// locate the first item on the screen where the cursor is allowed to
// land.

void M_KeyBindings(int choice)
{
  M_SetupNextMenuAlt(ss_keys);

  setup_active = true;
  setup_screen = ss_keys;
  set_keybnd_active = true;
  setup_select = false;
  default_verify = false;
  setup_gather = false;
  mult_screens_index = M_GetMultScreenIndex(keys_settings);
  set_tab_on = mult_screens_index;
  current_setup_menu = keys_settings[mult_screens_index];
  current_setup_tabs = keys_tabs;
  set_menu_itemon = M_GetSetupMenuItemOn();
  while (current_setup_menu[set_menu_itemon++].m_flags & S_SKIP);
  current_setup_menu[--set_menu_itemon].m_flags |= S_HILITE;
}

// The drawing part of the Key Bindings Setup initialization. Draw the
// background, title, instruction line, and items.

void M_DrawKeybnd(void)
{
  inhelpscreens = true;    // killough 4/6/98: Force status bar redraw

  // Set up the Key Binding screen 

  M_DrawBackground("FLOOR4_6"); // Draw background
  M_DrawTitle(84, 2, "M_KEYBND", "KEY BINDINGS");
  M_DrawTabs();
  M_DrawInstructions();
  M_DrawScreenItems(current_setup_menu);

  // If the Reset Button has been selected, an "Are you sure?" message
  // is overlayed across everything else.

  if (default_verify)
    M_DrawDefVerify();
}

/////////////////////////////
//
// The Weapon Screen tables.

// There's only one weapon settings screen (for now). But since we're
// trying to fit a common description for screens, it gets a setup_menu_t,
// which only has one screen definition in it.
//
// Note that this screen has no PREV or NEXT items, since there are no
// neighboring screens.

setup_menu_t weap_settings1[], weap_settings2[];

static setup_menu_t* weap_settings[] =
{
  weap_settings1,
  weap_settings2,
  NULL
};

static setup_tab_t weap_tabs[] =
{
   { "cosmetic",    weap_settings1 },
   { "preferences", weap_settings2 },
   { NULL }
};

// [FG] centered or bobbing weapon sprite
static const char *center_weapon_strings[] = {
    "Off", "Centered", "Bobbing"
};

static void M_UpdateCenteredWeaponItem(void)
{
  DisableItem(!weapon_bobbing_pct, weap_settings1, "center_weapon");
}

static const char *bobbing_pct_strings[] = {
  "0%", "25%", "50%", "75%", "100%"
};

setup_menu_t weap_settings1[] =
{
  {"View Bob", S_THERMO|S_THRM_SIZE4, m_null, M_X_THRM4, M_Y,
   {"view_bobbing_pct"}, 0, NULL, str_bobbing_pct},

  {"Weapon Bob", S_THERMO|S_THRM_SIZE4, m_null, M_X_THRM4, M_THRM_SPC,
   {"weapon_bobbing_pct"}, 0, M_UpdateCenteredWeaponItem, str_bobbing_pct},

  // [FG] centered or bobbing weapon sprite
  {"Weapon Alignment", S_CHOICE|S_STRICT, m_null, M_X, M_THRM_SPC,
   {"center_weapon"}, 0, NULL, str_center_weapon},

  {"Hide Weapon", S_ONOFF|S_STRICT, m_null, M_X, M_SPC, {"hide_weapon"}},

  {"Weapon Recoil", S_ONOFF, m_null, M_X, M_SPC, {"weapon_recoilpitch"}},

  MI_RESET,

  MI_END
};

setup_menu_t weap_settings2[] =  // Weapons Settings screen
{
  {"1St Choice Weapon", S_WEAP|S_BOOM, m_null, M_X, M_Y,   {"weapon_choice_1"}},
  {"2Nd Choice Weapon", S_WEAP|S_BOOM, m_null, M_X, M_SPC, {"weapon_choice_2"}},
  {"3Rd Choice Weapon", S_WEAP|S_BOOM, m_null, M_X, M_SPC, {"weapon_choice_3"}},
  {"4Th Choice Weapon", S_WEAP|S_BOOM, m_null, M_X, M_SPC, {"weapon_choice_4"}},
  {"5Th Choice Weapon", S_WEAP|S_BOOM, m_null, M_X, M_SPC, {"weapon_choice_5"}},
  {"6Th Choice Weapon", S_WEAP|S_BOOM, m_null, M_X, M_SPC, {"weapon_choice_6"}},
  {"7Th Choice Weapon", S_WEAP|S_BOOM, m_null, M_X, M_SPC, {"weapon_choice_7"}},
  {"8Th Choice Weapon", S_WEAP|S_BOOM, m_null, M_X, M_SPC, {"weapon_choice_8"}},
  {"9Th Choice Weapon", S_WEAP|S_BOOM, m_null, M_X, M_SPC, {"weapon_choice_9"}},

  {"", S_SKIP, m_null, M_X, M_SPC},

  {"Use Weapon Toggles", S_ONOFF|S_BOOM, m_null, M_X, M_SPC,
   {"doom_weapon_toggles"}},

  {"", S_SKIP, m_null, M_X, M_SPC},

  {"Pre-Beta BFG", S_ONOFF, m_null, M_X,  // killough 8/8/98
   M_SPC, {"classic_bfg"}},

  MI_END
};

// Setting up for the Weapons screen. Turn on flags, set pointers,
// locate the first item on the screen where the cursor is allowed to
// land.

void M_Weapons(int choice)
{
  M_SetupNextMenuAlt(ss_weap);

  setup_active = true;
  setup_screen = ss_weap;
  set_weapon_active = true;
  setup_select = false;
  default_verify = false;
  setup_gather = false;
  mult_screens_index = M_GetMultScreenIndex(weap_settings);
  set_tab_on = mult_screens_index;
  current_setup_menu = weap_settings[mult_screens_index];
  current_setup_tabs = weap_tabs;
  set_menu_itemon = M_GetSetupMenuItemOn();
  while (current_setup_menu[set_menu_itemon++].m_flags & S_SKIP);
  current_setup_menu[--set_menu_itemon].m_flags |= S_HILITE;
}


// The drawing part of the Weapons Setup initialization. Draw the
// background, title, instruction line, and items.

void M_DrawWeapons(void)
{
  inhelpscreens = true;    // killough 4/6/98: Force status bar redraw

  M_DrawBackground("FLOOR4_6"); // Draw background
  M_DrawTitle(109, 2, "M_WEAP", "WEAPONS");
  M_DrawTabs();
  M_DrawInstructions();
  M_DrawScreenItems(current_setup_menu);

  // If the Reset Button has been selected, an "Are you sure?" message
  // is overlayed across everything else.

  if (default_verify)
    M_DrawDefVerify();
}

/////////////////////////////
//
// The Status Bar / HUD tables.

// Screen table definitions

setup_menu_t stat_settings1[];
setup_menu_t stat_settings2[];
setup_menu_t stat_settings3[];
setup_menu_t stat_settings4[];

static setup_menu_t* stat_settings[] =
{
  stat_settings1,
  stat_settings2,
  stat_settings3,
  stat_settings4,
  NULL
};

static setup_tab_t stat_tabs[] =
{
   { "HUD",       stat_settings1 },
   { "Widgets",   stat_settings2 },
   { "Crosshair", stat_settings3 },
   { "Messages",  stat_settings4 },
   { NULL }
};

static void M_SizeDisplayAlt(void)
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
        M_SizeDisplay(choice);
    }

    hud_displayed = (screenblocks == 11);
}

static const char *screensize_strings[] = {
    "", "", "", "Status Bar", "Status Bar", "Status Bar", "Status Bar",
    "Status Bar", "Status Bar", "Status Bar", "Status Bar", "Fullscreen"
};

static const char *hudtype_strings[] = {
    "Crispy", "Boom No Bars", "Boom"
};

static const char **M_GetHUDModeStrings(void)
{
    static const char *crispy_strings[] = {"Off", "Original", "Widescreen"};
    static const char *boom_strings[] = {"Minimal", "Compact", "Distributed"};
    return hud_type ? boom_strings : crispy_strings;
}

static void M_UpdateHUDModeStrings(void);

#define H_X_THRM8 (M_X_THRM8 - 14)
#define H_X (M_X - 14)

setup_menu_t stat_settings1[] =  // Status Bar and HUD Settings screen
{
  {"Screen Size", S_THERMO, m_null, H_X_THRM8, M_Y, {"screenblocks"}, 0, M_SizeDisplayAlt, str_screensize},

  {"", S_SKIP, m_null, H_X, M_THRM_SPC},

  {"Status Bar", S_SKIP|S_TITLE, m_null, H_X, M_SPC},
  {"Colored Numbers", S_ONOFF|S_COSMETIC, m_null, H_X, M_SPC, {"sts_colored_numbers"}},
  {"Gray Percent Sign", S_ONOFF|S_COSMETIC, m_null, H_X, M_SPC, {"sts_pct_always_gray"}},
  {"Solid Background Color", S_ONOFF, m_null, H_X, M_SPC, {"st_solidbackground"}},

  {"", S_SKIP, m_null, H_X, M_SPC},

  {"Fullscreen HUD", S_SKIP|S_TITLE, m_null, H_X, M_SPC},
  {"HUD Type", S_CHOICE, m_null, H_X, M_SPC, {"hud_type"}, 0, M_UpdateHUDModeStrings, str_hudtype},
  {"HUD Mode", S_CHOICE, m_null, H_X, M_SPC, {"hud_active"}, 0, NULL, str_hudmode},

  {"", S_SKIP, m_null, H_X, M_SPC},

  {"Backpack Shifts Ammo Color", S_ONOFF, m_null, H_X, M_SPC, {"hud_backpack_thresholds"}},
  {"Armor Color Matches Type", S_ONOFF, m_null, H_X, M_SPC, {"hud_armor_type"}},
  {"Animated Health/Armor Count", S_ONOFF, m_null, H_X, M_SPC, {"hud_animated_counts"}},
  {"Blink Missing Keys", S_ONOFF, m_null, H_X, M_SPC, {"hud_blink_keys"}},

  MI_RESET,

  MI_END
};

static const char *show_widgets_strings[] = {
    "Off", "Automap", "HUD", "Always"
};

setup_menu_t stat_settings2[] =
{
  {"Widget Types", S_SKIP|S_TITLE, m_null, M_X, M_Y},

  {"Show Level Stats", S_CHOICE, m_null, M_X, M_SPC,
   {"hud_level_stats"}, 0, NULL, str_show_widgets},
  {"Show Level Time", S_CHOICE, m_null, M_X, M_SPC,
   {"hud_level_time"}, 0, NULL, str_show_widgets},
  {"Show Player Coords", S_CHOICE|S_STRICT, m_null, M_X, M_SPC,
   {"hud_player_coords"}, 0, NULL, str_show_widgets},
  {"Use-Button Timer", S_ONOFF, m_null, M_X, M_SPC, {"hud_time_use"}},

  {"", S_SKIP, m_null, M_X, M_SPC},

  {"Widget Appearance", S_SKIP|S_TITLE, m_null, M_X, M_SPC},

  {"Use Doom Font", S_CHOICE, m_null, M_X, M_SPC,
   {"hud_widget_font"}, 0, NULL, str_show_widgets},
  {"Widescreen Alignment", S_ONOFF, m_null, M_X, M_SPC,
   {"hud_widescreen_widgets"}, 0, HU_Start},
  {"Vertical Layout", S_ONOFF, m_null, M_X, M_SPC,
   {"hud_widget_layout"}, 0, HU_Start},

  MI_END
};

static void M_UpdateCrosshairItems (void)
{
    DisableItem(!hud_crosshair, stat_settings3, "hud_crosshair_health");
    DisableItem(!hud_crosshair, stat_settings3, "hud_crosshair_target");
    DisableItem(!hud_crosshair, stat_settings3, "hud_crosshair_lockon");
    DisableItem(!hud_crosshair, stat_settings3, "hud_crosshair_color");
    DisableItem(!(hud_crosshair && hud_crosshair_target == crosstarget_highlight),
        stat_settings3, "hud_crosshair_target_color");
}

static const char *crosshair_target_strings[] = {
    "Off", "Highlight", "Health"
};

static const char *hudcolor_strings[] = {
    "BRICK", "TAN", "GRAY", "GREEN", "BROWN", "GOLD", "RED", "BLUE", "ORANGE",
    "YELLOW", "BLUE2", "BLACK", "PURPLE", "WHITE", "NONE"
};

#define XH_X (M_X - 33)

setup_menu_t stat_settings3[] =
{
  {"Crosshair", S_CHOICE, m_null, XH_X, M_Y,
   {"hud_crosshair"}, 0, M_UpdateCrosshairItems, str_crosshair},

  {"Color By Player Health",S_ONOFF|S_STRICT, m_null, XH_X, M_SPC, {"hud_crosshair_health"}},
  {"Color By Target", S_CHOICE|S_STRICT, m_null, XH_X, M_SPC,
   {"hud_crosshair_target"}, 0, M_UpdateCrosshairItems, str_crosshair_target},
  {"Lock On Target", S_ONOFF|S_STRICT, m_null, XH_X, M_SPC, {"hud_crosshair_lockon"}},
  {"Default Color", S_CRITEM, m_null, XH_X, M_SPC,
   {"hud_crosshair_color"}, 0, NULL, str_hudcolor},
  {"Highlight Color", S_CRITEM|S_STRICT, m_null, XH_X, M_SPC,
   {"hud_crosshair_target_color"}, 0, NULL, str_hudcolor},

  MI_END
};

setup_menu_t stat_settings4[] =
{
  {"\"A Secret is Revealed!\" Message", S_ONOFF, m_null, M_X, M_Y,
   {"hud_secret_message"}},

  {"Show Toggle Messages", S_ONOFF, m_null, M_X, M_SPC, {"show_toggle_messages"}},

  {"Show Pickup Messages", S_ONOFF, m_null, M_X, M_SPC, {"show_pickup_messages"}},

  {"Show Obituaries", S_ONOFF, m_null, M_X, M_SPC, {"show_obituary_messages"}},

  {"Center Messages", S_ONOFF, m_null, M_X, M_SPC, {"message_centered"}},

  {"Colorize Messages", S_ONOFF, m_null, M_X, M_SPC,
   {"message_colorized"}, 0, HU_ResetMessageColors},

  MI_END
};

// Setting up for the Status Bar / HUD screen. Turn on flags, set pointers,
// locate the first item on the screen where the cursor is allowed to
// land.

void M_StatusBar(int choice)
{
  M_SetupNextMenuAlt(ss_stat);

  setup_active = true;
  setup_screen = ss_stat;
  setup_select = false;
  default_verify = false;
  setup_gather = false;
  mult_screens_index = M_GetMultScreenIndex(stat_settings);
  set_tab_on = mult_screens_index;
  current_setup_menu = stat_settings[mult_screens_index];
  current_setup_tabs = stat_tabs;
  set_menu_itemon = M_GetSetupMenuItemOn();
  while (current_setup_menu[set_menu_itemon++].m_flags & S_SKIP);
  current_setup_menu[--set_menu_itemon].m_flags |= S_HILITE;
}


// The drawing part of the Status Bar / HUD Setup initialization. Draw the
// background, title, instruction line, and items.

void M_DrawStatusHUD(void)
{
  inhelpscreens = true;    // killough 4/6/98: Force status bar redraw

  M_DrawBackground("FLOOR4_6"); // Draw background
  M_DrawTitle(59, 2, "M_STAT", "STATUS BAR / HUD");
  M_DrawTabs();
  M_DrawInstructions();
  M_DrawScreenItems(current_setup_menu);

  if (hud_crosshair && mult_screens_index == 2)
  {
    patch_t *patch = W_CacheLumpName(crosshair_lumps[hud_crosshair], PU_CACHE);

    int x = XH_X + 85 - SHORT(patch->width) / 2;
    int y = M_Y + M_SPC / 2 - SHORT(patch->height) / 2 - 1;

    V_DrawPatchTranslated(x, y, patch, colrngs[hud_crosshair_color]);
  }

  // If the Reset Button has been selected, an "Are you sure?" message
  // is overlayed across everything else.

  if (default_verify)
    M_DrawDefVerify();
}


/////////////////////////////
//
// The Automap tables.

setup_menu_t auto_settings1[];

static setup_menu_t* auto_settings[] =
{
  auto_settings1,
  NULL
};

static const char *overlay_strings[] = {
    "Off", "On", "Dark"
};

static const char *automap_preset_strings[] = {
    "Vanilla", "Boom", "ZDoom"
};

static const char *automap_keyed_door_strings[] = {
    "Off", "On", "Flashing"
};

setup_menu_t auto_settings1[] =  // 1st AutoMap Settings screen       
{
  {"Modes", S_SKIP|S_TITLE, m_null, M_X, M_Y},
  {"Follow Player" ,        S_ONOFF,  m_null, M_X, M_SPC, {"followplayer"}},
  {"Rotate Automap",        S_ONOFF,  m_null, M_X, M_SPC, {"automaprotate"}},
  {"Overlay Automap",       S_CHOICE, m_null, M_X, M_SPC, {"automapoverlay"},
   0, NULL, str_overlay},
  {"Coords Follow Pointer", S_ONOFF,  m_null, M_X, M_SPC, {"map_point_coord"}},  // killough 10/98

  {"", S_SKIP, m_null, M_X, M_SPC},

  {"Miscellaneous", S_SKIP|S_TITLE, m_null, M_X, M_SPC},
  {"Color Preset", S_CHOICE|S_COSMETIC, m_null, M_X, M_SPC,
   {"mapcolor_preset"}, 0, AM_ColorPreset, str_automap_preset},
  {"Smooth automap lines", S_ONOFF, m_null, M_X, M_SPC,
   {"map_smooth_lines"}, 0, AM_EnableSmoothLines},
  {"Show Found Secrets Only", S_ONOFF, m_null, M_X, M_SPC, {"map_secret_after"}},
  {"Color Keyed Doors" , S_CHOICE, m_null, M_X, M_SPC,
   {"map_keyed_door"}, 0, NULL, str_automap_keyed_door},

  MI_RESET,

  MI_END
};

// Setting up for the Automap screen. Turn on flags, set pointers,
// locate the first item on the screen where the cursor is allowed to
// land.

void M_Automap(int choice)
{
  M_SetupNextMenuAlt(ss_auto);

  setup_active = true;
  setup_screen = ss_auto;
  setup_select = false;
  default_verify = false;
  setup_gather = false;
  mult_screens_index = M_GetMultScreenIndex(auto_settings);
  set_tab_on = mult_screens_index;
  current_setup_menu = auto_settings[mult_screens_index];
  current_setup_tabs = NULL;
  set_menu_itemon = M_GetSetupMenuItemOn();
  while (current_setup_menu[set_menu_itemon++].m_flags & S_SKIP);
  current_setup_menu[--set_menu_itemon].m_flags |= S_HILITE;
}

// The drawing part of the Automap Setup initialization. Draw the
// background, title, instruction line, and items.

void M_DrawAutoMap(void)
{
  inhelpscreens = true;    // killough 4/6/98: Force status bar redraw

  M_DrawBackground("FLOOR4_6"); // Draw background
  M_DrawTitle(109, 2, "M_AUTO", "AUTOMAP");
  M_DrawInstructions();
  M_DrawScreenItems(current_setup_menu);

  // If the Reset Button has been selected, an "Are you sure?" message
  // is overlayed across everything else.

  if (default_verify)
    M_DrawDefVerify();
}


/////////////////////////////
//
// The Enemies table.

setup_menu_t enem_settings1[];

static setup_menu_t* enem_settings[] =
{
  enem_settings1,
  NULL
};

static void M_BarkSound(void)
{
    if (default_dogs)
    {
        S_StartSound(NULL, sfx_dgact);
    }
}

setup_menu_t enem_settings1[] =  // Enemy Settings screen
{
  {"Helper Dogs", S_MBF|S_THERMO|S_THRM_SIZE4|S_LEVWARN|S_ACTION,
   m_null, M_X_THRM4, M_Y, {"player_helpers"}, 0, M_BarkSound},

  {"", S_SKIP, m_null, M_X, M_THRM_SPC},

  {"Cosmetic", S_SKIP|S_TITLE, m_null, M_X, M_SPC},

  // [FG] colored blood and gibs
  {"Colored Blood", S_ONOFF|S_STRICT, m_null, M_X, M_SPC,
   {"colored_blood"}, 0, D_SetBloodColor},

  // [crispy] randomly flip corpse, blood and death animation sprites
  {"Randomly Mirrored Corpses", S_ONOFF|S_STRICT, m_null, M_X, M_SPC,
   {"flipcorpses"}},

  // [crispy] resurrected pools of gore ("ghost monsters") are translucent
  {"Translucent Ghost Monsters", S_ONOFF|S_STRICT|S_VANILLA, m_null, M_X, M_SPC,
   {"ghost_monsters"}},

  // [FG] spectre drawing mode
  {"Blocky Spectre Drawing", S_ONOFF, m_null, M_X, M_SPC,
   {"fuzzcolumn_mode"}, 0, R_SetFuzzColumnMode},

  MI_RESET,

  MI_END

};

/////////////////////////////

// Setting up for the Enemies screen. Turn on flags, set pointers,
// locate the first item on the screen where the cursor is allowed to
// land.

void M_Enemy(int choice)
{
  M_SetupNextMenuAlt(ss_enem);

  setup_active = true;
  setup_screen = ss_enem;
  setup_select = false;
  default_verify = false;
  setup_gather = false;
  mult_screens_index = M_GetMultScreenIndex(enem_settings);
  set_tab_on = mult_screens_index;
  current_setup_menu = enem_settings[mult_screens_index];
  current_setup_tabs = NULL;
  set_menu_itemon = M_GetSetupMenuItemOn();
  while (current_setup_menu[set_menu_itemon++].m_flags & S_SKIP);
  current_setup_menu[--set_menu_itemon].m_flags |= S_HILITE;
}

// The drawing part of the Enemies Setup initialization. Draw the
// background, title, instruction line, and items.

void M_DrawEnemy(void)
{
  inhelpscreens = true;

  M_DrawBackground("FLOOR4_6"); // Draw background
  M_DrawTitle(114, 2, "M_ENEM", "ENEMIES");
  M_DrawInstructions();
  M_DrawScreenItems(current_setup_menu);

  // If the Reset Button has been selected, an "Are you sure?" message
  // is overlayed across everything else.

  if (default_verify)
    M_DrawDefVerify();
}

/////////////////////////////
//
// The Compatibility table.
// killough 10/10/98

setup_menu_t comp_settings1[];

static setup_menu_t* comp_settings[] =
{
  comp_settings1,
  NULL
};

static const char *default_complevel_strings[] = {
  "Vanilla", "Boom", "MBF", "MBF21"
};

setup_menu_t comp_settings1[] =  // Compatibility Settings screen #1
{
  {"Compatibility", S_SKIP|S_TITLE, m_null, M_X, M_Y},

  {"Default Compatibility Level", S_CHOICE|S_LEVWARN, m_null, M_X, M_SPC,
   {"default_complevel"}, 0, NULL, str_default_complevel},

  {"Strict Mode", S_ONOFF|S_LEVWARN, m_null, M_X, M_SPC, {"strictmode"}},

  {"", S_SKIP, m_null, M_X, M_SPC},

  {"Compatibility-breaking Features", S_SKIP|S_TITLE, m_null, M_X, M_SPC},

  {"Direct Vertical Aiming", S_ONOFF|S_STRICT, m_null, M_X, M_SPC,
   {"direct_vertical_aiming"}},

  {"Auto Strafe 50", S_ONOFF|S_STRICT, m_null, M_X, M_SPC,
   {"autostrafe50"}, 0, G_UpdateSideMove},

  {"Pistol Start", S_ONOFF|S_STRICT, m_null, M_X, M_SPC,
   {"pistolstart"}},

  {"", S_SKIP, m_null, M_X, M_SPC},

  {"Improved Hit Detection", S_ONOFF|S_STRICT|S_BOOM, m_null, M_X,
   M_SPC, {"blockmapfix"}},

  {"Fast Line-of-Sight Calculation", S_ONOFF|S_STRICT, m_null, M_X,
   M_SPC, {"checksight12"}, 0, P_UpdateCheckSight},

  {"Walk Under Solid Hanging Bodies", S_ONOFF|S_STRICT, m_null, M_X,
   M_SPC, {"hangsolid"}},

  {"Emulate INTERCEPTS overflow", S_ONOFF|S_VANILLA, m_null, M_X,
   M_SPC, {"emu_intercepts"}},


  MI_RESET,

  MI_END
};

// Setting up for the Compatibility screen. Turn on flags, set pointers,
// locate the first item on the screen where the cursor is allowed to
// land.

void M_Compat(int choice)
{
  M_SetupNextMenuAlt(ss_comp);

  setup_active = true;
  setup_screen = ss_comp;
  setup_select = false;
  default_verify = false;
  setup_gather = false;
  mult_screens_index = M_GetMultScreenIndex(comp_settings);
  set_tab_on = mult_screens_index;
  current_setup_menu = comp_settings[mult_screens_index];
  current_setup_tabs = NULL;
  set_menu_itemon = M_GetSetupMenuItemOn();
  while (current_setup_menu[set_menu_itemon++].m_flags & S_SKIP);
  current_setup_menu[--set_menu_itemon].m_flags |= S_HILITE;
}

// The drawing part of the Compatibility Setup initialization. Draw the
// background, title, instruction line, and items.

void M_DrawCompat(void)
{
  inhelpscreens = true;

  M_DrawBackground("FLOOR4_6"); // Draw background
  M_DrawTitle(52, 2, "M_COMPAT", "DOOM COMPATIBILITY");
  M_DrawInstructions();
  M_DrawScreenItems(current_setup_menu);

  // If the Reset Button has been selected, an "Are you sure?" message
  // is overlayed across everything else.

  if (default_verify)
    M_DrawDefVerify();
}

/////////////////////////////
//
// The General table.
// killough 10/10/98

extern int tran_filter_pct;

setup_menu_t gen_settings1[];
setup_menu_t gen_settings2[];
setup_menu_t gen_settings3[];
setup_menu_t gen_settings4[];
setup_menu_t gen_settings5[];
setup_menu_t gen_settings6[];

static setup_menu_t* gen_settings[] =
{
  gen_settings1,
  gen_settings2,
  gen_settings3,
  gen_settings4,
  gen_settings5,
  gen_settings6,
  NULL
};

static setup_tab_t gen_tabs[] =
{
   { "video",   gen_settings1 },
   { "audio",   gen_settings2 },
   { "mouse",   gen_settings3 },
   { "gamepad", gen_settings4 },
   { "display", gen_settings5 },
   { "misc",    gen_settings6 },
   { NULL }
};

int resolution_scale;

static const char **M_GetResolutionScaleStrings(void)
{
    const char **strings = NULL;

    resolution_scaling_t rs;
    I_GetResolutionScaling(&rs);

    array_push(strings, "100%");

    int val = SCREENHEIGHT * 2;
    char buf[8];

    for (int i = 1; val < rs.max; ++i)
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

    array_push(strings, "native");

    return strings;
}

static void M_ResetVideoHeight(void)
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
            current_video_height = SCREENHEIGHT * 2 + (resolution_scale - 1) * rs.step;
        }
    }

    if (!dynamic_resolution)
    {
        VX_ResetMaxDist();
    }

    DisableItem(current_video_height <= DRS_MIN_HEIGHT,
                 gen_settings1, "dynamic_resolution");

    resetneeded = true;
}

static const char *widescreen_strings[] = {
    "Off", "Auto", "16:10", "16:9", "21:9"
};

int midi_player_menu;

static const char **M_GetMidiDevicesStrings(void)
{
    return I_DeviceList(&midi_player_menu);
}

static void M_SmoothLight(void)
{
  setsmoothlight = true;
  setsizeneeded = true; // run R_ExecuteSetViewSize
}

const char *gamma_strings[] = {
    // Darker
    "-4", "-3.6", "-3.2", "-2.8", "-2.4", "-2.0", "-1.6", "-1.2", "-0.8",

    // No gamma correction
    "0",

    // Lighter
    "0.5", "1", "1.5", "2", "2.5", "3", "3.5", "4"
};

void M_ResetGamma(void)
{
  I_SetPalette(W_CacheLumpName("PLAYPAL",PU_CACHE));
}

static const char *sound_module_strings[] = {
    "Standard", "OpenAL 3D",
#if defined(HAVE_AL_BUFFER_CALLBACK)
    "PC Speaker"
#endif
};

static void M_UpdateAdvancedSoundItems(void)
{
  DisableItem(snd_module != SND_MODULE_3D, gen_settings2, "snd_hrtf");
}

static void M_SetSoundModule(void)
{
  M_UpdateAdvancedSoundItems();

  if (!I_AllowReinitSound())
  {
    // The OpenAL implementation doesn't support the ALC_SOFT_HRTF extension
    // which is required for alcResetDeviceSOFT(). Warn the user to restart.
    warn_about_changes(S_PRGWARN);
    return;
  }

  I_SetSoundModule(snd_module);
}

static void M_SetMidiPlayer(void)
{
  S_StopMusic();
  I_SetMidiPlayer(midi_player_menu);
  S_SetMusicVolume(snd_MusicVolume);
  S_RestartMusic();
}

static void M_ToggleUncapped(void)
{
  DisableItem(!default_uncapped, gen_settings1, "fpslimit");
  setrefreshneeded = true;
}

static void M_ToggleFullScreen(void)
{
  toggle_fullscreen = true;
}

static void M_ToggleExclusiveFullScreen(void)
{
  toggle_exclusive_fullscreen = true;
}

static void M_CoerceFPSLimit(void)
{
  if (fpslimit < TICRATE)
    fpslimit = 0;
  setrefreshneeded = true;
}

static void M_UpdateFOV(void)
{
  setsizeneeded = true; // run R_ExecuteSetViewSize;
}

static void M_ResetScreen(void)
{
  resetneeded = true;
}

static void M_UpdateSfxVolume(void)
{
  S_SetSfxVolume(snd_SfxVolume);
}

static void M_UpdateMusicVolume(void)
{
  S_SetMusicVolume(snd_MusicVolume);
}

setup_menu_t gen_settings1[] = { // General Settings screen1

  {"Resolution Scale", S_THERMO|S_THRM_SIZE11|S_ACTION, m_null, M_X_THRM11, M_Y,
   {"resolution_scale"}, 0, M_ResetVideoHeight, str_resolution_scale},

  {"Dynamic Resolution", S_ONOFF, m_null, M_X, M_THRM_SPC,
   {"dynamic_resolution"}, 0, M_ResetVideoHeight},

  {"Widescreen", S_CHOICE, m_null, M_X, M_SPC,
   {"widescreen"}, 0, M_ResetScreen, str_widescreen},

  {"FOV", S_THERMO, m_null, M_X_THRM8, M_SPC, {"fov"}, 0, M_UpdateFOV},

  {"Fullscreen", S_ONOFF, m_null, M_X, M_THRM_SPC,
   {"fullscreen"}, 0, M_ToggleFullScreen},

  {"Exclusive Fullscreen", S_ONOFF, m_null, M_X, M_SPC,
   {"exclusive_fullscreen"}, 0, M_ToggleExclusiveFullScreen},

  {"", S_SKIP, m_null, M_X, M_SPC},

  {"Uncapped Framerate", S_ONOFF, m_null, M_X, M_SPC,
   {"uncapped"}, 0, M_ToggleUncapped},

  {"Framerate Limit", S_NUM, m_null, M_X, M_SPC,
   {"fpslimit"}, 0, M_CoerceFPSLimit},

  {"VSync", S_ONOFF, m_null, M_X, M_SPC, {"use_vsync"}, 0, I_ToggleVsync},

  {"", S_SKIP, m_null, M_X, M_SPC},

  {"Gamma Correction", S_THERMO, m_null, M_X_THRM8, M_SPC,
   {"gamma2"}, 0, M_ResetGamma, str_gamma},

  {"Level Brightness", S_THERMO|S_THRM_SIZE4|S_STRICT, m_null, M_X_THRM4, M_THRM_SPC,
   {"extra_level_brightness"}},

  MI_RESET,

  MI_END
};

setup_menu_t gen_settings2[] = { // General Settings screen2

  {"Sound Volume", S_THERMO, m_null, M_X_THRM8, M_Y,
   {"sfx_volume"}, 0, M_UpdateSfxVolume},

  {"Music Volume", S_THERMO, m_null, M_X_THRM8, M_THRM_SPC,
   {"music_volume"}, 0, M_UpdateMusicVolume},

  {"", S_SKIP, m_null, M_X, M_THRM_SPC},

  {"Sound Module", S_CHOICE, m_null, M_X, M_SPC,
   {"snd_module"}, 0, M_SetSoundModule, str_sound_module},

  {"Headphones Mode", S_ONOFF, m_null, M_X, M_SPC, {"snd_hrtf"}, 0, M_SetSoundModule},

  {"Pitch-Shifted Sounds", S_ONOFF, m_null, M_X, M_SPC, {"pitched_sounds"}},

  // [FG] play sounds in full length
  {"Disable Sound Cutoffs", S_ONOFF, m_null, M_X, M_SPC, {"full_sounds"}},

  {"", S_SKIP, m_null, M_X, M_SPC},

  // [FG] music backend
  {"MIDI player", S_CHOICE|S_ACTION|S_NEXT_LINE, m_null, M_X, M_SPC,
   {"midi_player_menu"}, 0, M_SetMidiPlayer, str_midi_player},

  MI_END
};

#define MOUSE_ACCEL_STRINGS_SIZE (40 + 1)

static const char **M_GetMouseAccelStrings(void)
{
    static const char *strings[MOUSE_ACCEL_STRINGS_SIZE];
    char buf[8];

    for (int i = 0; i < MOUSE_ACCEL_STRINGS_SIZE; ++i)
    {
        int val = i + 10;
        M_snprintf(buf, sizeof(buf), "%1d.%1d", val / 10, val % 10);
        strings[i] = M_StringDuplicate(buf);
    }
    return strings;
}

void M_ResetTimeScale(void)
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
            I_Error("Invalid parameter '%d' for -speed, valid values are 10-1000.", time_scale);
        }
    }

    I_SetTimeScale(time_scale);
}

void M_UpdateFreeLook(void)
{
  P_UpdateDirectVerticalAiming();

  if (!mouselook || !padlook)
  {
    int i;
    for (i = 0; i < MAXPLAYERS; ++i)
      if (playeringame[i])
        players[i].centering = true;
  }
}

static const char *layout_strings[] = {
  "Default", "Swap", "Legacy", "Legacy Swap"
};

static const char *curve_strings[] = {
  "", "", "", "", "", "", "", "", "", "", // Dummy values, start at 1.0.
  "Linear", "1.1", "1.2", "1.3", "1.4", "1.5", "1.6", "1.7", "1.8", "1.9",
  "Squared", "2.1", "2.2", "2.3", "2.4", "2.5", "2.6", "2.7", "2.8", "2.9",
  "Cubed"
};

const char *default_skill_strings[] = {
  // dummy first option because defaultskill is 1-based
  "", "ITYTD", "HNTR", "HMP", "UV", "NM"
};

static const char *endoom_strings[] = {
  "off", "on", "PWAD only"
};

static const char *death_use_action_strings[] = {
  "default", "last save", "nothing"
};

static const char *menu_backdrop_strings[] = {
  "Off", "Dark", "Texture"
};

static const char *invul_mode_strings[] = {
  "Vanilla", "MBF", "Gray"
};

void M_DisableVoxelsRenderingItem(void)
{
    DisableItem(true, gen_settings5, "voxels_rendering");
}

#define CNTR_X 162

setup_menu_t gen_settings3[] = {

  // [FG] double click to "use"
  {"Double-Click to \"Use\"", S_ONOFF, m_null, CNTR_X, M_Y, {"dclick_use"}},

  {"Free Look", S_ONOFF, m_null, CNTR_X, M_SPC,
   {"mouselook"}, 0, M_UpdateFreeLook},

  // [FG] invert vertical axis
  {"Invert Look", S_ONOFF, m_null, CNTR_X, M_SPC,
   {"mouse_y_invert"}},

  {"", S_SKIP, m_null, CNTR_X, M_SPC},

  {"Turn Sensitivity", S_THERMO|S_THRM_SIZE11, m_null, CNTR_X, M_SPC,
   {"mouse_sensitivity"}},

  {"Look Sensitivity", S_THERMO|S_THRM_SIZE11, m_null, CNTR_X, M_THRM_SPC,
   {"mouse_sensitivity_y_look"}},

  {"Move Sensitivity", S_THERMO|S_THRM_SIZE11, m_null, CNTR_X, M_THRM_SPC,
   {"mouse_sensitivity_y"}},

  {"Strafe Sensitivity", S_THERMO|S_THRM_SIZE11, m_null, CNTR_X, M_THRM_SPC,
   {"mouse_sensitivity_strafe"}},

  {"", S_SKIP, m_null, CNTR_X, M_THRM_SPC},

  {"Mouse acceleration", S_THERMO, m_null, CNTR_X, M_SPC,
   {"mouse_acceleration"}, 0, NULL, str_mouse_accel},

  MI_END
};

setup_menu_t gen_settings4[] = {

  {"Stick Layout", S_CHOICE, m_scrn, CNTR_X, M_Y,
   {"joy_layout"}, 0, I_ResetController, str_layout},

  {"Free Look", S_ONOFF, m_null, CNTR_X, M_SPC,
   {"padlook"}, 0, M_UpdateFreeLook},

  {"Invert Look", S_ONOFF, m_scrn, CNTR_X, M_SPC, {"joy_invert_look"}},

  {"", S_SKIP, m_null, CNTR_X, M_SPC},

  {"Turn Sensitivity", S_THERMO|S_THRM_SIZE11, m_scrn, CNTR_X, M_SPC,
   {"joy_sensitivity_turn"}, 0, I_ResetController},

  {"Look Sensitivity", S_THERMO|S_THRM_SIZE11, m_scrn, CNTR_X, M_THRM_SPC,
   {"joy_sensitivity_look"}, 0, I_ResetController},

  {"Extra Turn Sensitivity", S_THERMO|S_THRM_SIZE11, m_scrn, CNTR_X, M_THRM_SPC,
   {"joy_extra_sensitivity_turn"}, 0, I_ResetController},

  {"", S_SKIP, m_null, CNTR_X, M_THRM_SPC},

  {"Movement Curve", S_THERMO, m_scrn, CNTR_X, M_SPC,
   {"joy_response_curve_movement"}, 0, I_ResetController, str_curve},

  {"Camera Curve", S_THERMO, m_scrn, CNTR_X, M_THRM_SPC,
   {"joy_response_curve_camera"}, 0, I_ResetController, str_curve},

  {"Movement Deadzone", S_THERMO|S_PCT, m_scrn, CNTR_X, M_THRM_SPC,
   {"joy_deadzone_movement"}, 0, I_ResetController},

  {"Camera Deadzone", S_THERMO|S_PCT, m_scrn, CNTR_X, M_THRM_SPC,
   {"joy_deadzone_camera"}, 0, I_ResetController},

  MI_END
};

setup_menu_t gen_settings5[] = {

  {"Smooth Pixel Scaling", S_ONOFF, m_null, M_X, M_Y,
   {"smooth_scaling"}, 0, M_ResetScreen},

  {"Sprite Translucency", S_ONOFF|S_STRICT, m_null, M_X, M_SPC, {"translucency"}},

  {"Translucency Filter", S_NUM|S_ACTION|S_PCT, m_null, M_X, M_SPC,
   {"tran_filter_pct"}, 0, M_Trans},

  {"", S_SKIP, m_null, M_X, M_SPC},

  {"Voxels", S_ONOFF|S_STRICT, m_null, M_X, M_SPC, {"voxels_rendering"}},

  {"Brightmaps", S_ONOFF|S_STRICT, m_null, M_X, M_SPC, {"brightmaps"}},

  {"Stretch Short Skies", S_ONOFF, m_null, M_X, M_SPC,
   {"stretchsky"}, 0, R_InitSkyMap},

  {"Linear Sky Scrolling", S_ONOFF, m_null, M_X, M_SPC,
   {"linearsky"}, 0, R_InitPlanes},

  {"Swirling Flats", S_ONOFF, m_null, M_X, M_SPC, {"r_swirl"}},

  {"Smooth Diminishing Lighting", S_ONOFF, m_null, M_X, M_SPC,
   {"smoothlight"}, 0, M_SmoothLight},

  {"", S_SKIP, m_null, M_X, M_SPC},

  {"Menu Backdrop", S_CHOICE, m_null, M_X, M_SPC,
   {"menu_backdrop"}, 0, NULL, str_menu_backdrop},

  {"Show ENDOOM Screen", S_CHOICE, m_null, M_X, M_SPC,
   {"show_endoom"}, 0, NULL, str_endoom},

  MI_END
};

setup_menu_t gen_settings6[] = {

  {"Quality of life", S_SKIP|S_TITLE, m_null, M_X, M_Y},

  {"Screen melt", S_ONOFF|S_STRICT, m_null, M_X, M_SPC, {"screen_melt"}},

  {"On death action", S_CHOICE, m_null, M_X, M_SPC,
   {"death_use_action"}, 0, NULL, str_death_use_action},

  {"Demo progress bar", S_ONOFF, m_null, M_X, M_SPC, {"demobar"}},

  {"Screen flashes", S_ONOFF|S_STRICT, m_null, M_X, M_SPC,
   {"palette_changes"}},

  {"Invulnerability effect", S_CHOICE|S_STRICT, m_null, M_X, M_SPC,
   {"invul_mode"}, 0, R_InvulMode, str_invul_mode},

  {"Organize save files", S_ONOFF|S_PRGWARN, m_null, M_X, M_SPC,
   {"organize_savefiles"}},

  {"", S_SKIP, m_null, M_X, M_SPC},

  {"Miscellaneous", S_SKIP|S_TITLE, m_null, M_X, M_SPC},

  {"Game speed", S_NUM|S_STRICT|S_PCT, m_null, M_X, M_SPC,
   {"realtic_clock_rate"}, 0, M_ResetTimeScale},

  {"Default Skill", S_CHOICE|S_LEVWARN, m_null, M_X, M_SPC,
   {"default_skill"}, 0, NULL, str_default_skill},

  MI_END
};

void M_Trans(void) // To reset translucency after setting it in menu
{
    R_InitTranMap(0);
}

// Setting up for the General screen. Turn on flags, set pointers,
// locate the first item on the screen where the cursor is allowed to
// land.

void M_General(int choice)
{
  M_SetupNextMenuAlt(ss_gen);

  setup_active = true;
  setup_screen = ss_gen;
  setup_select = false;
  default_verify = false;
  setup_gather = false;
  mult_screens_index = M_GetMultScreenIndex(gen_settings);
  set_tab_on = mult_screens_index;
  current_setup_menu = gen_settings[mult_screens_index];
  current_setup_tabs = gen_tabs;
  set_menu_itemon = M_GetSetupMenuItemOn();
  while (current_setup_menu[set_menu_itemon++].m_flags & S_SKIP);
  current_setup_menu[--set_menu_itemon].m_flags |= S_HILITE;
}

// The drawing part of the General Setup initialization. Draw the
// background, title, instruction line, and items.

void M_DrawGeneral(void)
{
  inhelpscreens = true;

  M_DrawBackground("FLOOR4_6"); // Draw background
  M_DrawTitle(114, 2, "M_GENERL", "GENERAL");
  M_DrawTabs();
  M_DrawInstructions();
  M_DrawScreenItems(current_setup_menu);

  // If the Reset Button has been selected, an "Are you sure?" message
  // is overlayed across everything else.

  if (default_verify)
    M_DrawDefVerify();
}

/////////////////////////////
//
// General routines used by the Setup screens.
//

// phares 4/17/98:
// M_SelectDone() gets called when you have finished entering your
// Setup Menu item change.

static void M_SelectDone(setup_menu_t* ptr)
{
  ptr->m_flags &= ~S_SELECT;
  ptr->m_flags |= S_HILITE;
  S_StartSound(NULL,sfx_itemup);
  setup_select = false;
  if (print_warning_about_changes)     // killough 8/15/98
    print_warning_about_changes--;
}

// phares 4/21/98:
// Array of setup screens used by M_ResetDefaults()

static setup_menu_t **setup_screens[] =
{
  keys_settings,
  weap_settings,
  stat_settings,
  auto_settings,
  enem_settings,
  gen_settings,      // killough 10/98
  comp_settings,
};

// phares 4/19/98:
// M_ResetDefaults() resets all values for a setup screen to default values
// 
// killough 10/98: rewritten to fix bugs and warn about pending changes

static void M_ResetDefaults()
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

        setup_menu_t **screens = setup_screens[setup_screen - 1];

        for ( ; *screens; screens++)
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
                            dp->current->s = dp->location->s;
                        else if (dp->type == number)
                            dp->current->i = dp->location->i;
                    }

                    if (current_item->action)
                    {
                        current_item->action();
                    }
                }
                else if (current_item->input_id == dp->input_id)
                {
                    M_InputSetDefault(dp->input_id, dp->inputs);
                }
            }
        }
    }

    default_reset = false;

    if (warn)
        warn_about_changes(warn);
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

void M_InitDefaults(void)
{
  setup_menu_t *const *p, *t;
  default_t *dp;
  int i;
  for (i = 0; i < ss_max-1; i++)
    for (p = setup_screens[i]; *p; p++)
      for (t = *p; !(t->m_flags & S_END); t++)
	if (t->m_flags & S_HASDEFPTR)
	{
	  if (!(dp = M_LookupDefault(t->var.name)))
	    I_Error("Could not find config variable \"%s\"", t->var.name);
	  else
	    (t->var.def = dp)->setup_menu = t;
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

      if (c == '=')      // probably means the '+' key?
          c = '+';
      else if (c == ',') // probably means the '<' key?
          c = '<';
      else if (c == '.') // probably means the '>' key?
          c = '>';
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

static int menu_font_spacing = 0;

// M_DrawMenuString() draws the string in menu_buffer[]

void M_DrawStringCR(int cx, int cy, byte *cr1, byte *cr2, const char *ch)
{
    int   w;
    int   c;

    byte *cr = cr1;

    while (*ch)
    {
        c = *ch++;         // get next char

        if (c == '\x1b')
        {
            if (ch)
            {
                c = *ch++;
                if (c >= '0' && c <= '0'+CR_NONE)
                    cr = colrngs[c - '0'];
                else if (c == '0'+CR_ORIG)
                    cr = cr1;
                continue;
            }
        }

        c = toupper(c) - HU_FONTSTART;
        if (c < 0 || c > HU_FONTSIZE)
        {
            cx += SPACEWIDTH;    // space
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
            V_DrawPatchTRTR(cx, cy, hu_font[c], cr, cr2);
        else
            V_DrawPatchTranslated(cx, cy, hu_font[c], cr);

        // The screen is cramped, so trim one unit from each
        // character so they butt up against each other.
        cx += w + menu_font_spacing;
    }
}

void M_DrawString(int cx, int cy, int color, const char *ch)
{
  M_DrawStringCR(cx, cy, colrngs[color], NULL, ch);
}

// cph 2006/08/06 - M_DrawString() is the old M_DrawMenuString, except that it is not tied to menu_buffer

static void M_DrawMenuString(int cx, int cy, int color)
{
  M_DrawString(cx, cy, color, menu_buffer);
}

static void M_DrawMenuStringEx(int flags, int x, int y, int color)
{
    if (ItemDisabled(flags))
    {
        M_DrawStringCR(x, y, cr_dark, NULL, menu_buffer);
    }
    else if (flags & S_HILITE)
    {
        if (color == CR_NONE)
            M_DrawStringCR(x, y, cr_bright, NULL, menu_buffer);
        else
            M_DrawStringCR(x, y, colrngs[color], cr_bright, menu_buffer);
    }
    else
    {
        M_DrawMenuString(x, y, color);
    }
}

// M_GetPixelWidth() returns the number of pixels in the width of
// the string, NOT the number of chars in the string.

int M_GetPixelWidth(const char *ch)
{
    int len = 0;
    int c;

    while (*ch)
    {
        c = *ch++;    // pick up next char

        if (c == '\x1b') // skip color
        {
            if (*ch)
                ch++;
            continue;
        }

        c = toupper(c) - HU_FONTSTART;
        if (c < 0 || c > HU_FONTSIZE)
        {
            len += SPACEWIDTH;   // space
            continue;
        }

        len += SHORT(hu_font[c]->width);
        len += menu_font_spacing; // adjust so everything fits
    }
    len -= menu_font_spacing; // replace what you took away on the last char only
    return len;
}

enum {
  prog,
  art,
  test, test_stub, test_stub2,
  canine,
  musicsfx, /*musicsfx_stub,*/
  woof, // [FG] shamelessly add myself to the Credits page ;)
  adcr, adcr_stub,
  special, special_stub, special_stub2,
};

enum {
  cr_prog=1,
  cr_art,
  cr_test,
  cr_canine,
  cr_musicsfx,
  cr_woof, // [FG] shamelessly add myself to the Credits page ;)
  cr_adcr,
  cr_special,
};

#define CR_S 9
#define CR_X 152
#define CR_X2 (CR_X+8)
#define CR_Y 31
#define CR_SH 2

static setup_menu_t cred_settings[] = {

  {"Programmer",S_SKIP|S_CREDIT,m_null, CR_X, CR_Y + CR_S*prog + CR_SH*cr_prog},
  {"Lee Killough",S_SKIP|S_CREDIT|S_LEFTJUST,m_null, CR_X2, CR_Y + CR_S*prog + CR_SH*cr_prog},

  {"Artist",S_SKIP|S_CREDIT,m_null, CR_X, CR_Y + CR_S*art + CR_SH*cr_art},
  {"Len Pitre",S_SKIP|S_CREDIT|S_LEFTJUST,m_null, CR_X2, CR_Y + CR_S*art + CR_SH*cr_art},

  {"PlayTesters",S_SKIP|S_CREDIT,m_null, CR_X, CR_Y + CR_S*test + CR_SH*cr_test},
  {"Ky (Rez) Moffet",S_SKIP|S_CREDIT|S_LEFTJUST,m_null, CR_X2, CR_Y + CR_S*test + CR_SH*cr_test},
  {"Len Pitre",S_SKIP|S_CREDIT|S_LEFTJUST,m_null, CR_X2, CR_Y + CR_S*(test+1) + CR_SH*cr_test},
  {"James (Quasar) Haley",S_SKIP|S_CREDIT|S_LEFTJUST,m_null, CR_X2, CR_Y + CR_S*(test+2) + CR_SH*cr_test},

  {"Canine Consulting",S_SKIP|S_CREDIT,m_null, CR_X, CR_Y + CR_S*canine + CR_SH*cr_canine},
  {"Longplain Kennels, Reg'd",S_SKIP|S_CREDIT|S_LEFTJUST,m_null, CR_X2, CR_Y + CR_S*canine + CR_SH*cr_canine},

  // haleyjd 05/12/09: changed Allegro credits to Team Eternity
  {"SDL Port By",S_SKIP|S_CREDIT,m_null, CR_X, CR_Y + CR_S*musicsfx + CR_SH*cr_musicsfx},
  {"Team Eternity",S_SKIP|S_CREDIT|S_LEFTJUST,m_null, CR_X2, CR_Y + CR_S*musicsfx + CR_SH*cr_musicsfx},

  // [FG] shamelessly add myself to the Credits page ;)
  {"Woof! by",S_SKIP|S_CREDIT,m_null, CR_X, CR_Y + CR_S*woof + CR_SH*cr_woof},
  {"Fabian Greffrath",S_SKIP|S_CREDIT|S_LEFTJUST,m_null, CR_X2, CR_Y + CR_S*woof + CR_SH*cr_woof},

  {"Additional Credit To",S_SKIP|S_CREDIT,m_null, CR_X, CR_Y + CR_S*adcr + CR_SH*cr_adcr},
  {"id Software",S_SKIP|S_CREDIT|S_LEFTJUST,m_null, CR_X2, CR_Y + CR_S*adcr+CR_SH*cr_adcr},
  {"TeamTNT",S_SKIP|S_CREDIT|S_LEFTJUST,m_null, CR_X2, CR_Y + CR_S*(adcr+1)+CR_SH*cr_adcr},

  {"Special Thanks To",S_SKIP|S_CREDIT,m_null, CR_X, CR_Y + CR_S*special + CR_SH*cr_special},
  {"John Romero",S_SKIP|S_CREDIT|S_LEFTJUST,m_null, CR_X2, CR_Y + CR_S*(special+0)+CR_SH*cr_special},
  {"Joel Murdoch",S_SKIP|S_CREDIT|S_LEFTJUST,m_null, CR_X2, CR_Y + CR_S*(special+1)+CR_SH*cr_special},

  {0,S_SKIP|S_END,m_null}
};

void M_DrawCredits(void)     // killough 10/98: credit screen
{
  char mbftext_s[32];
  sprintf(mbftext_s, PROJECT_STRING);
  inhelpscreens = true;
  M_DrawBackground(gamemode==shareware ? "CEIL5_1" : "MFLR8_4");
  M_DrawTitle(42, 9, "MBFTEXT", mbftext_s);
  M_DrawScreenItems(cred_settings);
}

boolean MN_CursorPostionSetup(int x, int y)
{
    if (!setup_active || setup_select)
    {
        return false;
    }

    if (current_setup_tabs)
    {
        for (int i = 0; current_setup_tabs[i].text; ++i)
        {
            setup_tab_t *tab = &current_setup_tabs[i];

            tab->flags &= ~S_HILITE;

            if (M_PointInsideRect(&tab->rect, x, y))
            {
                tab->flags |= S_HILITE;

                if (set_tab_on != i)
                {
                    set_tab_on = i;
                    S_StartSound(NULL, sfx_itemup);
                }
            }
        }
    }

    for (int i = 0; !(current_setup_menu[i].m_flags & S_END); i++)
    {
        setup_menu_t *item = &current_setup_menu[i];
        int flags = item->m_flags;

        if (flags & S_SKIP)
        {
            continue;
        }

        item->m_flags &= ~S_HILITE;

        if (M_PointInsideRect(&item->rect, x, y))
        {
            item->m_flags |= S_HILITE;

            if (set_menu_itemon != i)
            {
                print_warning_about_changes = false;
                set_menu_itemon = i;
                S_StartSound(NULL, sfx_itemup);
            }
        }
    }

    return true;
}

static int setup_cancel = -1;

static void OnOff(void)
{
    setup_menu_t *current_item = current_setup_menu + set_menu_itemon;
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

    if (current_item->action)      // killough 10/98
    {
        current_item->action();
    }
}

static void Choice(menu_action_t action)
{
    setup_menu_t *current_item = current_setup_menu + set_menu_itemon;
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
            S_StartSound(NULL, sfx_stnmov);
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
                value--;
        }
        else if (value > max)
        {
            value = max;
        }

        if (def->location->i != value)
        {
            S_StartSound(NULL, sfx_stnmov);
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
        M_SelectDone(current_item);
        setup_cancel = -1;
    }
}

static boolean ChangeEntry(menu_action_t action, int ch)
{
    if (!setup_select)
    {
        return false;
    }

    setup_menu_t *current_item = current_setup_menu + set_menu_itemon;
    int flags = current_item->m_flags;
    default_t *def = current_item->var.def;

    if (action == MENU_ESCAPE) // Exit key = no change
    {
        if (flags & (S_CHOICE|S_CRITEM|S_THERMO) && setup_cancel != -1)
        {
            def->location->i = setup_cancel;
            setup_cancel = -1;
        }

        M_SelectDone(current_item);                           // phares 4/17/98
        setup_gather = false;   // finished gathering keys, if any
        return true;
    }

    if (flags & (S_YESNO|S_ONOFF)) // yes or no setting?
    {
        if (action == MENU_ENTER)
        {
           OnOff();
        }
        M_SelectDone(current_item);        // phares 4/17/98
        return true;
    }

    // [FG] selection of choices

    if (flags & (S_CHOICE|S_CRITEM|S_THERMO))
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

        if (action == MENU_ENTER)
        {
            if (gather_count)      // Any input?
            {
                int value;

                gather_buffer[gather_count] = 0;
                value = atoi(gather_buffer);  // Integer value

                if ((def->limit.min != UL && value < def->limit.min) ||
                    (def->limit.max != UL && value > def->limit.max))
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

                    if (current_item->action)      // killough 10/98
                    {
                        current_item->action();
                    }
                }
            }
            M_SelectDone(current_item);     // phares 4/17/98
            setup_gather = false; // finished gathering keys
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
    {                                        // incoming key or button gets bound
        return false;
    }

    setup_menu_t *current_item = current_setup_menu + set_menu_itemon;

    int input_id = (current_item->m_flags & S_INPUT) ? current_item->input_id : 0;

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
        M_SelectDone(current_item);
        return true;
    }

    boolean search = true;

    for (int i = 0; keys_settings[i] && search; i++)
    {
        for (setup_menu_t *p = keys_settings[i]; !(p->m_flags & S_END); p++)
        {
            if (p->m_group == current_item->m_group &&
                p != current_item &&
                (p->m_flags & (S_INPUT|S_KEEP)) &&
                M_InputActivated(p->input_id))
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

    M_SelectDone(current_item);       // phares 4/17/98
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

    if (!current_setup_tabs)
    {
        return false;
    }

    int i = mult_screens_index + inc;

    if (i < 0 || current_setup_tabs[i].text == NULL)
    {
        return false;
    }

    mult_screens_index += inc;

    setup_menu_t *current_item = current_setup_menu + set_menu_itemon;
    current_item->m_flags &= ~S_HILITE;

    M_SetSetupMenuItemOn(set_menu_itemon);
    set_tab_on = mult_screens_index;
    current_setup_menu = current_setup_tabs[set_tab_on].page;
    set_menu_itemon = M_GetSetupMenuItemOn();

    print_warning_about_changes = false; // killough 10/98
    while (current_setup_menu[set_menu_itemon++].m_flags & S_SKIP);
    current_setup_menu[--set_menu_itemon].m_flags |= S_HILITE;

    S_StartSound(NULL, sfx_pstop);  // killough 10/98
    return true;

}

boolean MN_ResponderSetup(event_t *ev, menu_action_t action, int ch)
{
    // phares 3/26/98 - 4/11/98:
    // Setup screen key processing

    if (!setup_active)
    {
        return false;
    }

    setup_menu_t *current_item = current_setup_menu + set_menu_itemon;

    // phares 4/19/98:
    // Catch the response to the 'reset to default?' verification
    // screen

    if (default_verify)
    {
        if (toupper(ch) == 'Y')
        {
            M_ResetDefaults();
            default_verify = false;
            M_SelectDone(current_item);
        }
        else if (toupper(ch) == 'N')
        {
            default_verify = false;
            M_SelectDone(current_item);
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
        setup_select) // changing an entry
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
                    if (p->m_flags & S_WEAP &&
                        p->var.def->location->i == ch &&
                        p != current_item)
                    {
                        p->var.def->location->i = current_item->var.def->location->i;
                        goto end;
                    }
                }
            }
        end:
            current_item->var.def->location->i = ch;
        }

        M_SelectDone(current_item);       // phares 4/17/98
        return true;
    }

    // Not changing any items on the Setup screens. See if we're
    // navigating the Setup menus or selecting an item to change.

    if (action == MENU_DOWN)
    {
        current_item->m_flags &= ~S_HILITE;     // phares 4/17/98
        do
        {
            if (current_item->m_flags & S_END)
            {
                set_menu_itemon = 0;
                current_item = current_setup_menu;
            }
            else
            {
                set_menu_itemon++;
                current_item++;
            }
        } while (current_item->m_flags & S_SKIP);

        M_SelectDone(current_item);         // phares 4/17/98
        return true;
    }

    if (action == MENU_UP)
    {
        current_item->m_flags &= ~S_HILITE;     // phares 4/17/98
        do
        {
            if (set_menu_itemon == 0)
            {
                do
                {
                    set_menu_itemon++;
                    current_item++;
                } while(!(current_item->m_flags & S_END));
            }
            set_menu_itemon--;
            current_item--;
        } while(current_item->m_flags & S_SKIP);

        M_SelectDone(current_item);         // phares 4/17/98
        return true;
    }

    // [FG] clear key bindings with the DEL key
    if (action == MENU_CLEAR)
    {
        int flags = current_item->m_flags;

        if (flags & S_INPUT)
        {
            M_InputReset(current_item->input_id);
        }
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
            S_StartSound(NULL, sfx_oof);
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
        S_StartSound(NULL, sfx_itemup);
        return true;
    }

    if (action == MENU_ESCAPE || action == MENU_BACKSPACE)
    {
        M_SetSetupMenuItemOn(set_menu_itemon);
        M_SetMultScreenIndex(mult_screens_index);

        if (action == MENU_ESCAPE) // Clear all menus
        {
            M_ClearMenus();
        }
        else if (action == MENU_BACKSPACE)
        {
            M_Back();
        }

        current_item->m_flags &= ~(S_HILITE|S_SELECT);// phares 4/19/98
        setup_active = false;
        set_keybnd_active = false;
        set_weapon_active = false;
        default_verify = false;       // phares 4/19/98
        print_warning_about_changes = false; // [FG] reset
        HU_Start();    // catch any message changes // phares 4/19/98
        S_StartSound(NULL, sfx_swtchx);
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
    if (!current_setup_tabs)
    {
        return false;
    }

    setup_tab_t *tab = current_setup_tabs + set_tab_on;

    if (!(M_InputActivated(input_menu_enter) && tab->flags & S_HILITE))
    {
        return false;
    }

    current_setup_menu = tab->page;
    mult_screens_index = set_tab_on;
    set_menu_itemon = 0;
    while (current_setup_menu[set_menu_itemon++].m_flags & S_SKIP);
    set_menu_itemon--;

    S_StartSound(NULL, sfx_pstop);
    return true;
}

boolean MN_MouseResponderSetup(int x, int y)
{
    if (!setup_active || setup_select)
    {
        return false;
    }

    I_ShowMouseCursor(true);

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

    setup_menu_t *current_item = current_setup_menu + set_menu_itemon;
    int flags = current_item->m_flags;
    default_t *def = current_item->var.def;
    mrect_t *rect = &current_item->rect;

    if (ItemDisabled(flags))
    {
        return false;
    }

    if (M_InputActivated(input_menu_enter)
        && !M_PointInsideRect(rect, x, y))
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
                max = array_size(strings) - 1;
            else
                max = M_THRM_UL_VAL;
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
            S_StartSound(NULL, sfx_stnmov);
        }
        return true;
    }

    if (!M_InputActivated(input_menu_enter))
    {
        return false;
    }

    if (flags & (S_YESNO|S_ONOFF)) // yes or no setting?
    {
        OnOff();
        S_StartSound(NULL, sfx_itemup);
        return true;
    }

    if (flags & (S_CRITEM|S_CHOICE))
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
            S_StartSound(NULL, sfx_stnmov);
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

void M_SetMenuFontSpacing(void)
{
  if (M_StringWidth("abcdefghijklmnopqrstuvwxyz01234") > 230)
    menu_font_spacing = -1;
}

// [FG] alternative text for missing menu graphics lumps

void M_DrawTitle(int x, int y, const char *patch, const char *alttext)
{
  if (W_CheckNumForName(patch) >= 0)
    V_DrawPatchDirect(x,y,W_CacheLumpName(patch,PU_CACHE));
  else
  {
    // patch doesn't exist, draw some text in place of it
    M_snprintf(menu_buffer, sizeof(menu_buffer), "%s", alttext);
    M_DrawMenuString(SCREENWIDTH/2 - M_StringWidth(alttext)/2,
                     y + 8 - M_StringHeight(alttext)/2, // assumes patch height 16
                     CR_TITLE);
  }
}

/////////////////////////////
//
// Initialization Routines to take care of one-time setup
//

static const char **selectstrings[] = {
    NULL, // str_empty
    layout_strings,
    curve_strings,
    center_weapon_strings,
    screensize_strings,
    hudtype_strings,
    NULL, // str_hudmode
    show_widgets_strings,
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
    NULL, // str_mouse_accel
    default_skill_strings,
    default_complevel_strings,
    endoom_strings,
    death_use_action_strings,
    menu_backdrop_strings,
    widescreen_strings,
    bobbing_pct_strings,
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

static void M_UpdateHUDModeStrings(void)
{
    selectstrings[str_hudmode] = M_GetHUDModeStrings();
}

void M_InitMenuStrings(void)
{
    M_UpdateHUDModeStrings();
    selectstrings[str_resolution_scale] = M_GetResolutionScaleStrings();
    selectstrings[str_midi_player] = M_GetMidiDevicesStrings();
    selectstrings[str_mouse_accel] = M_GetMouseAccelStrings();
}

void M_ResetSetupMenu(void)
{
  extern boolean deh_set_blood_color;

  DisableItem(M_ParmExists("-strict"), comp_settings1, "strictmode");
  DisableItem(force_complevel, comp_settings1, "default_complevel");
  DisableItem(M_ParmExists("-pistolstart"), comp_settings1, "pistolstart");
  DisableItem(M_ParmExists("-uncapped") || M_ParmExists("-nouncapped"), gen_settings1, "uncapped");
  DisableItem(deh_set_blood_color, enem_settings1, "colored_blood");
  DisableItem(!brightmaps_found || force_brightmaps, gen_settings5, "brightmaps");
  DisableItem(current_video_height <= DRS_MIN_HEIGHT, gen_settings1, "dynamic_resolution");

  M_CoerceFPSLimit();
  M_UpdateCrosshairItems();
  M_UpdateCenteredWeaponItem();
  M_UpdateAdvancedSoundItems();
}

void M_ResetSetupMenuVideo(void)
{
  M_ToggleUncapped();
}

//
// End of General Routines
//
/////////////////////////////////////////////////////////////////////////////

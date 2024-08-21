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

#include "doomdef.h"
#include "doomtype.h"

typedef struct
{
    short x;
    short y;
    short w;
    short h;
} mrect_t;

typedef enum
{
    MENU_NULL,
    MENU_UP,
    MENU_DOWN,
    MENU_LEFT,
    MENU_RIGHT,
    MENU_BACKSPACE,
    MENU_ENTER,
    MENU_ESCAPE,
    MENU_CLEAR
} menu_action_t;

typedef enum
{
    null_mode,
    mouse_mode,
    pad_mode,
    key_mode
} menu_input_mode_t;

extern menu_input_mode_t menu_input, old_menu_input;
void MN_ResetMouseCursor(void);

extern boolean setup_active;
extern short whichSkull; // which skull to draw (he blinks)
extern int saved_screenblocks;

extern boolean default_verify;
extern int warning_about_changes, print_warning_about_changes;

void MN_InitDefaults(void);
extern const char *gamma_strings[];
void MN_ResetGamma(void);
void MN_DrawDelVerify(void);
void MN_SizeDisplay(int choice);

boolean MN_SetupCursorPostion(int x, int y);
boolean MN_SetupMouseResponder(int x, int y);
boolean MN_SetupResponder(menu_action_t action, int ch);

void MN_SetNextMenuAlt(ss_types type);
boolean MN_PointInsideRect(mrect_t *rect, int x, int y);
void MN_ClearMenus(void);
void MN_Back(void);

#define M_X_CENTER (-1)

// [FG] alternative text for missing menu graphics lumps
void MN_DrawTitle(int x, int y, const char *patch, const char *alttext);
void MN_DrawStringCR(int cx, int cy, byte *cr1, byte *cr2, const char *ch);
int MN_StringWidth(const char *string);
int MN_StringHeight(const char *string);

void MN_General(int choice);
void MN_KeyBindings(int choice);
void MN_Compat(int choice);
void MN_StatusBar(int choice);
void MN_Automap(int choice);
void MN_Weapons(int choice);
void MN_Enemy(int choice);

void MN_DrawGeneral(void);
void MN_DrawKeybnd(void);
void MN_DrawCompat(void);
void MN_DrawStatusHUD(void);
void MN_DrawAutoMap(void);
void MN_DrawWeapons(void);
void MN_DrawEnemy(void);

/////////////////////////////
//
// The following #defines are for the m_flags field of each item on every
// Setup Screen. They can be OR'ed together where appropriate

#define S_HILITE      0x00000001 // Cursor is sitting on this item
#define S_SELECT      0x00000002 // We're changing this item
#define S_TITLE       0x00000004 // Title item
#define S_FUNC        0x00000008 // Non-config item
#define S_CRITEM      0x00000010 // Message color
#define S_RESET       0x00000020 // Reset to Defaults Button
#define S_INPUT       0x00000040 // Composite input
#define S_WEAP        0x00000080 // Weapon #
#define S_NUM         0x00000100 // Numerical item
#define S_SKIP        0x00000200 // Cursor can't land here
#define S_KEEP        0x00000400 // Don't swap key out
#define S_END         0x00000800 // Last item in list (dummy)
#define S_LEVWARN     0x00001000 // killough 8/30/98: Always warn about pending change
#define S_PRGWARN     0x00002000 // killough 10/98: Warn about change until next run
#define S_BADVAL      0x00004000 // killough 10/98: Warn about bad value
#define S_LEFTJUST    0x00008000 // killough 10/98: items which are left-justified
#define S_CREDIT      0x00010000 // killough 10/98: credit
#define S_CHOICE      0x00020000 // [FG] selection of choices
#define S_DISABLE     0x00040000 // Disable item
#define S_COSMETIC    0x00080000 // Don't warn about change, always load from OPTIONS lump
#define S_THERMO      0x00100000 // Thermo bar (default size 8)
#define S_WRAP_LINE   0x00200000 // Wrap long menu items relative to M_WRAP
#define S_STRICT      0x00400000 // Disable in strict mode
#define S_BOOM        0x00800000 // Disable if complevel < boom
#define S_VANILLA     0x01000000 // Disable if complevel != vanilla
#define S_ACTION      0x02000000 // Run function call only when change is complete
#define S_THRM_SIZE11 0x04000000 // Thermo bar size 11
#define S_ONOFF       0x08000000 // Alias for S_YESNO
#define S_MBF         0x10000000 // Disable if complevel < mbf
#define S_THRM_SIZE4  0x20000000 // Thermo bar size 4
#define S_PCT         0x40000000 // Show % sign

// S_SHOWDESC  = the set of items whose description should be displayed
// S_SHOWSET   = the set of items whose setting should be displayed
// S_HASDEFPTR = the set of items whose var field points to default array
// S_DIRECT    = the set of items with direct coordinates

#define S_SHOWDESC                                                       \
    (S_TITLE | S_ONOFF | S_CRITEM | S_RESET | S_INPUT | S_WEAP | S_NUM   \
     | S_CREDIT | S_CHOICE | S_THERMO | S_FUNC)

#define S_SHOWSET \
    (S_ONOFF | S_CRITEM | S_INPUT | S_WEAP | S_NUM | S_CHOICE | S_THERMO \
     | S_FUNC)

#define S_HASDEFPTR \
    (S_ONOFF | S_NUM | S_WEAP | S_CRITEM | S_CHOICE | S_THERMO)

#define S_DIRECT (S_RESET | S_END | S_CREDIT)

/////////////////////////////
//
// The setup_group enum is used to show which 'groups' keys fall into so
// that you can bind a key differently in each 'group'.

typedef enum
{
    m_null, // Has no meaning; not applicable
    m_scrn, // A key can not be assigned to more than one action
    m_map,  // in the same group. A key can be assigned to one
    m_gyro, // action in one group, and another action in another.
} setup_group;

/////////////////////////////
//
// phares 4/17/98:
// State definition for each item.
// This is the definition of the structure for each setup item. Not all
// fields are used by all items.
//
// A setup screen is defined by an array of these items specific to
// that screen.
//
// killough 11/98:
//
// Restructured to allow simpler table entries,
// and to Xref with defaults[] array in m_misc.c.
// Moved from m_menu.c to m_menu.h so that m_misc.c can use it.

typedef struct setup_menu_s
{
    const char *m_text;  // text to display
    int m_flags;         // phares 4/17/98: flag bits S_* (defined above)
    short m_x;           // screen x position (left is 0)
    short m_y;           // screen y position (top is 0)

    union // killough 11/98: The first field is a union of several types
    {
        void *var;             // generic variable
        char *name;            // name
        struct default_s *def; // default[] table entry
    } var;

    setup_group m_group;  // group
    int input_id;         // composite input
    int strings_id;       // [FG] selection of choices
    void (*action)(void); // killough 10/98: function to call after changing
    mrect_t rect;
    int lines;            // number of lines for rect, always > 0
} setup_menu_t;

// phares 4/21/98: Moved from m_misc.c so m_menu.c could see it.
//
// killough 11/98: totally restructured

// [FG] use a union of integer and string pointer to store config values,
// instead of type-punning string pointers to integers which won't work on
// 64-bit systems anyway

typedef union config_u
{
  int i;
  char *s;
} config_t;

typedef struct default_s
{
  const char *name;                   // name
  config_t *location;                 // default variable
  config_t *current;                  // possible nondefault variable
  config_t  defaultvalue;             // built-in default value
  struct {int min, max;} limit;       // numerical limits
  enum {number, string, input} type;  // type
  ss_types setupscreen;               // setup screen this appears on
  wad_allowed_t wad_allowed;          // whether it's allowed in wads
  const char *help;                   // description of parameter

  int input_id;

  // internal fields (initialized implicitly to 0) follow

  struct default_s *first, *next;           // hash table pointers
  int modified;                             // Whether it's been modified
  config_t orig_default;                    // Original default, if modified
  struct setup_menu_s *setup_menu;          // Xref to setup menu item, if any
} default_t;

extern default_t *defaults;

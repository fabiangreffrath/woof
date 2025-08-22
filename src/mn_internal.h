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

void M_ChooseSkill(int choice);

extern int maxscreenblocks;

extern int bigfont_priority;

extern menu_input_mode_t help_input, old_help_input; // pad_mode or key_mode.
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

boolean MN_SetupCursorPostion(int x, int y);
boolean MN_SetupMouseResponder(int x, int y);
boolean MN_SetupResponder(menu_action_t action, int ch);

void MN_SetNextMenuAlt(ss_types type);
boolean MN_PointInsideRect(mrect_t *rect, int x, int y);
void MN_ClearMenus(void);
void MN_Back(void);
void MN_BackSecondary(void);

#define M_X_CENTER (-1)

// [FG] alternative text for missing menu graphics lumps
void MN_DrawTitle(int x, int y, const char *patch, const char *alttext);
void MN_DrawStringCR(int cx, int cy, byte *cr1, byte *cr2, const char *ch);
int MN_StringHeight(const char *string);

void MN_General(int choice);
void MN_KeyBindings(int choice);
void MN_Compat(int choice);
void MN_StatusBar(int choice);
void MN_Automap(int choice);
void MN_Weapons(int choice);
void MN_Enemy(int choice);
void MN_CustomSkill(void);

void MN_DrawGeneral(void);
void MN_DrawKeybnd(void);
void MN_DrawCompat(void);
void MN_DrawStatusHUD(void);
void MN_DrawAutoMap(void);
void MN_DrawWeapons(void);
void MN_DrawEnemy(void);
void MN_DrawSfx(void);
void MN_DrawMidi(void);
void MN_DrawEqualizer(void);
void MN_DrawPadAdv(void);
void MN_DrawGyro(void);
void MN_DrawCustomSkill(void);

/////////////////////////////
//
// The following #defines are for the m_flags field of each item on every
// Setup Screen. They can be OR'ed together where appropriate

enum
{
    S_HILITE =      (1u << 0), // Cursor is sitting on this item
    S_SELECT =      (1u << 1), // We're changing this item
    S_TITLE =       (1u << 2), // Title item
    S_FUNC =        (1u << 3), // Non-config item
    S_CRITEM =      (1u << 4), // Message color
    S_RESET =       (1u << 5), // Reset to Defaults Button
    S_INPUT =       (1u << 6), // Composite input
    S_WEAP =        (1u << 7), // Weapon #
    S_NUM =         (1u << 8), // Numerical item
    S_SKIP =        (1u << 9), // Cursor can't land here
    S_KEEP =        (1u << 10), // Don't swap key out
    S_END =         (1u << 11), // Last item in list (dummy)
    S_LEVWARN =     (1u << 12), // killough 8/30/98: Always warn about pending change
    S_PRGWARN =     (1u << 13), // killough 10/98: Warn about change until next run
    S_BADVAL =      (1u << 14), // killough 10/98: Warn about bad value
    S_LEFTJUST =    (1u << 15), // killough 10/98: items which are left-justified
    S_CHOICE =      (1u << 16), // [FG] selection of choices
    S_DISABLE =     (1u << 17), // Disable item
    S_COSMETIC =    (1u << 18), // Don't warn about change, always load from OPTIONS lump
    S_THERMO =      (1u << 19), // Thermo bar (default size 8)
    S_WRAP_LINE =   (1u << 20), // Wrap long menu items relative to M_WRAP
    S_STRICT =      (1u << 21), // Disable in strict mode
    S_BOOM =        (1u << 22), // Disable if complevel < boom
    S_VANILLA =     (1u << 23), // Disable if complevel != vanilla
    S_ACTION =      (1u << 24), // Run function call only when change is complete
    S_THRM_SIZE11 = (1u << 25), // Thermo bar size 11
    S_ONOFF =       (1u << 26), // Alias for S_YESNO
    S_MBF =         (1u << 27), // Disable if complevel < mbf
    S_THRM_SIZE4 =  (1u << 28), // Thermo bar size 4
    S_PCT =         (1u << 29), // Show % sign
    S_CENTER =      (1u << 30), // Centered
}; 

// S_SHOWDESC  = the set of items whose description should be displayed
// S_SHOWSET   = the set of items whose setting should be displayed
// S_HASDEFPTR = the set of items whose var field points to default array
// S_DIRECT    = the set of items with direct coordinates

#define S_SHOWDESC                                                       \
    (S_TITLE | S_ONOFF | S_CRITEM | S_RESET | S_INPUT | S_WEAP | S_NUM   \
     | S_CHOICE | S_THERMO | S_FUNC | S_CENTER)

#define S_SHOWSET \
    (S_ONOFF | S_CRITEM | S_INPUT | S_WEAP | S_NUM | S_CHOICE | S_THERMO \
     | S_FUNC)

#define S_HASDEFPTR \
    (S_ONOFF | S_NUM | S_WEAP | S_CRITEM | S_CHOICE | S_THERMO)

#define S_DIRECT (S_RESET | S_END)

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
        char *name;            // name
        struct default_s *def; // default[] table entry
    } var;

    setup_group m_group;  // group
    int input_id;         // composite input
    int strings_id;       // [FG] selection of choices
    void (*action)(void); // killough 10/98: function to call after changing
    mrect_t rect;
    int lines;            // number of lines for rect, always > 0
    const char *desc;     // overrides default description
    const char *append;   // string to append to value
} setup_menu_t;

// phares 4/21/98: Moved from m_misc.c so m_menu.c could see it.
//
// killough 11/98: totally restructured

// [FG] use a union of integer and string pointer to store config values,
// instead of type-punning string pointers to integers which won't work on
// 64-bit systems anyway

typedef union
{
    char **s;
    int *i;
    const char *string;
    int number;
} config_t;

typedef struct default_s
{
    const char *name;      // name
    config_t location;     // default variable
    config_t current;      // possible nondefault variable
    config_t defaultvalue; // built-in default value

    struct
    {
        int min;
        int max;
    } limit; // numerical limits

    enum
    {
        number,
        string,
        input,
        menu // menu only
    } type; // type

    ss_types setupscreen;      // setup screen this appears on
    wad_allowed_t wad_allowed; // whether it's allowed in wads
    const char *help;          // description of parameter

    int input_id;

    // internal fields (initialized implicitly to 0) follow

    struct default_s *first, *next;  // hash table pointers
    int modified;                    // Whether it's been modified
    config_t orig_default;           // Original default, if modified
    struct setup_menu_s *setup_menu; // Xref to setup menu item, if any
} default_t;

extern default_t *defaults;

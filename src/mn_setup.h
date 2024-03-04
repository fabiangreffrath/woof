
#include "doomtype.h"
#include "mn_menu.h"

struct event_s;

void M_InitDefaults(void);

// [FG] alternative text for missing menu graphics lumps
void M_DrawTitle(int x, int y, const char *patch, const char *alttext);

int M_GetPixelWidth(const char *ch);
void M_UpdateFreeLook(void);

extern const char *gamma_strings[];
void M_ResetGamma(void);

extern boolean setup_active;
boolean MN_CursorPostionSetup(int x, int y);
boolean MN_MouseResponderSetup(int x, int y);
boolean MN_ResponderSetup(struct event_s *ev, menu_action_t action, int ch);

extern boolean default_verify;
extern int warning_about_changes, print_warning_about_changes;

#define SPACEWIDTH 4

void M_DrawDelVerify(void);

void M_DrawString(int cx, int cy, int color, const char *ch);
void M_DrawStringCR(int cx, int cy, byte *cr1, byte *cr2, const char *ch);

typedef enum
{
  set_general, // killough 10/98
  set_compat,
  set_key_bindings,
  set_weapons,
  set_statbar,
  set_automap,
  set_enemy,
  set_setup_end
} set_setup_t;

void M_General(int choice);
void M_DrawGeneral(void);

void M_KeyBindings(int choice);
void M_DrawKeybnd(void);

void M_Compat(int choice);
void M_DrawCompat(void);

void M_StatusBar(int choice);
void M_DrawStatusHUD(void);

void M_Automap(int choice);
void M_DrawAutoMap(void);

void M_Weapons(int choice);
void M_DrawWeapons(void);

void M_Enemy(int choice);
void M_DrawEnemy(void);

/////////////////////////////
//
// The following #defines are for the m_flags field of each item on every
// Setup Screen. They can be OR'ed together where appropriate

#define S_HILITE       0x00000001 // Cursor is sitting on this item
#define S_SELECT       0x00000002 // We're changing this item
#define S_TITLE        0x00000004 // Title item
#define S_YESNO        0x00000008 // Yes or No item
#define S_CRITEM       0x00000010 // Message color
#define S_RESET        0x00000020 // Reset to Defaults Button
#define S_INPUT        0x00000040 // Composite input
#define S_WEAP         0x00000080 // Weapon #
#define S_NUM          0x00000100 // Numerical item
#define S_SKIP         0x00000200 // Cursor can't land here
#define S_KEEP         0x00000400 // Don't swap key out
#define S_END          0x00000800 // Last item in list (dummy)
#define S_LEVWARN      0x00001000 // killough 8/30/98: Always warn about pending change
#define S_PRGWARN      0x00002000 // killough 10/98: Warn about change until next run
#define S_BADVAL       0x00004000 // killough 10/98: Warn about bad value
#define S_LEFTJUST     0x00008000 // killough 10/98: items which are left-justified
#define S_CREDIT       0x00010000 // killough 10/98: credit
#define S_CHOICE       0x00020000 // [FG] selection of choices
#define S_DISABLE      0x00040000 // Disable item
#define S_COSMETIC     0x00080000 // Don't warn about change, always load from OPTIONS lump
#define S_THERMO       0x00100000 // Thermo bar (default size 8)
#define S_NEXT_LINE    0x00200000 // Two lines menu items
#define S_STRICT       0x00400000 // Disable in strict mode
#define S_BOOM         0x00800000 // Disable if complevel < boom
#define S_VANILLA      0x01000000 // Disable if complevel != vanilla
#define S_ACTION       0x02000000 // Run function call only when change is complete
#define S_THRM_SIZE11  0x04000000 // Thermo bar size 11
#define S_ONOFF        0x08000000 // Alias for S_YESNO
#define S_MBF          0x10000000 // Disable if complevel < mbf
#define S_THRM_SIZE4   0x20000000 // Thermo bar size 4
#define S_PCT          0x40000000 // Show % sign

// S_SHOWDESC  = the set of items whose description should be displayed
// S_SHOWSET   = the set of items whose setting should be displayed
// S_HASDEFPTR = the set of items whose var field points to default array
// S_DIRECT    = the set of items with direct coordinates

#define S_SHOWDESC (S_TITLE|S_YESNO|S_ONOFF|S_CRITEM|S_RESET|S_INPUT|S_WEAP| \
                    S_NUM|S_CREDIT|S_CHOICE|S_THERMO)

#define S_SHOWSET  (S_YESNO|S_ONOFF|S_CRITEM|S_INPUT|S_WEAP|S_NUM|S_CHOICE|S_THERMO)

#define S_HASDEFPTR (S_YESNO|S_ONOFF|S_NUM|S_WEAP|S_CRITEM|S_CHOICE|S_THERMO)

#define S_DIRECT   (S_RESET|S_END|S_CREDIT)

/////////////////////////////
//
// The setup_group enum is used to show which 'groups' keys fall into so
// that you can bind a key differently in each 'group'.

typedef enum {
  m_null,       // Has no meaning; not applicable
  m_scrn,       // A key can not be assigned to more than one action
  m_map,        // in the same group. A key can be assigned to one
                // action in one group, and another action in another.
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
  const char  *m_text;  // text to display
  int         m_flags;  // phares 4/17/98: flag bits S_* (defined above)
  setup_group m_group;  // Group
  short       m_x;      // screen x position (left is 0)
  short       m_y;      // screen y position (top is 0)

  union  // killough 11/98: The first field is a union of several types
   {
     void      *var;    // generic variable
     char      *name;   // name
     struct default_s *def;      // default[] table entry
  } var;

  int input_id; // composite input
  void (*action)(void); // killough 10/98: function to call after changing
  int strings_id; // [FG] selection of choices
  mrect_t rect;
} setup_menu_t;

//
// Copyright(C) 2024 ceski
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//      Weapon slots.
//

#include <SDL3/SDL.h>

#include "g_game.h"
#include "i_input.h"
#include "i_timer.h"
#include "m_config.h"
#include "m_input.h"
#include "r_main.h"
#include "ws_stuff.h"

typedef enum
{
    WS_OFF,
    WS_HOLD_LAST,
    WS_ALWAYS_ON,
    NUM_WS_ACTIVATION
} weapon_slots_activation_t;

typedef enum
{
    WS_TYPE_NONE,
    WS_TYPE_FIST,
    WS_TYPE_PISTOL,
    WS_TYPE_SHOTGUN,
    WS_TYPE_CHAINGUN,
    WS_TYPE_ROCKET,
    WS_TYPE_PLASMA,
    WS_TYPE_BFG,
    WS_TYPE_CHAINSAW,
    WS_TYPE_SSG,
    NUM_WS_TYPES
} weapon_slots_type_t;

static weapon_slots_activation_t weapon_slots_activation;
static weapon_slots_selection_t weapon_slots_selection;
static weapon_slots_type_t weapon_slots_1_1;
static weapon_slots_type_t weapon_slots_1_2;
static weapon_slots_type_t weapon_slots_1_3;
static weapon_slots_type_t weapon_slots_2_1;
static weapon_slots_type_t weapon_slots_2_2;
static weapon_slots_type_t weapon_slots_2_3;
static weapon_slots_type_t weapon_slots_3_1;
static weapon_slots_type_t weapon_slots_3_2;
static weapon_slots_type_t weapon_slots_3_3;
static weapon_slots_type_t weapon_slots_4_1;
static weapon_slots_type_t weapon_slots_4_2;
static weapon_slots_type_t weapon_slots_4_3;

typedef struct
{
    int time;         // Time when activator was pressed (tics).
    int input_key;    // Key that triggered this event (doomkeys.h).
    boolean *state;   // Pointer to input key's on/off state.
    boolean restored; // Was this event restored?
} shared_event_t;

typedef struct
{
    weapontype_t weapons[NUM_WS_WEAPS]; // Weapons for this slot.
    int num_weapons;                    // Number of weapons in this slot.
    int input_key;                      // Key that selects slot (doomkeys.h).
} weapon_slot_t;

typedef struct
{
    boolean enabled;       // State initially updated in WS_UpdateState.
    boolean key_match;     // Event input key matches any slot's input key?
    int index;             // Index of the slot that matches the event.
    boolean override;      // Give gamepad higher priority.
    boolean hold_override; // Give gamepad higher priority with hold "last".

    boolean activated; // Activated weapon slots?
    boolean pressed;   // Pressed a weapon slot while activated?
    boolean selected;  // Selected a weapon slot while activated?
    int input_key;     // Input key for selected slot, tracked separately.
    const weapon_slot_t *current_slot; // Slot selected while activated.
} weapon_slots_state_t;

static shared_event_t shared_event;       // Event shared with activator.
static weapon_slot_t slots[NUM_WS_SLOTS]; // Parameters for each slot.
static weapon_slots_state_t state;        // Weapon slots state.

static void ResetState(void)
{
    state.enabled = false;
    state.key_match = false;
    state.index = -1;
    state.override = false;
    state.hold_override = false;

    state.activated = false;
    state.pressed = false;
    state.selected = false;
    state.input_key = -1;
    state.current_slot = NULL;
}

static void ResetSharedEvent(void)
{
    if (shared_event.state != NULL)
    {
        *shared_event.state = false;
        shared_event.state = NULL;
    }

    shared_event.time = 0;
    shared_event.input_key = -1;
    shared_event.restored = false;
}

//
// WS_Reset
//
void WS_Reset(void)
{
    ResetSharedEvent();
    ResetState();
}

//
// WS_Enabled
//
boolean WS_Enabled(void)
{
    return (weapon_slots_activation != WS_OFF);
}

//
// WS_Selection
//
weapon_slots_selection_t WS_Selection(void)
{
    return weapon_slots_selection;
}

//
// WS_UpdateSelection
//
void WS_UpdateSelection(void)
{
    const int translate[] = SCANCODE_TO_KEYS_ARRAY;

    switch (weapon_slots_selection)
    {
        case WS_SELECT_DPAD:
            slots[0].input_key = GAMEPAD_DPAD_UP;
            slots[1].input_key = GAMEPAD_DPAD_DOWN;
            slots[2].input_key = GAMEPAD_DPAD_LEFT;
            slots[3].input_key = GAMEPAD_DPAD_RIGHT;
            break;

        case WS_SELECT_FACE_BUTTONS:
            slots[0].input_key = GAMEPAD_NORTH;
            slots[1].input_key = GAMEPAD_SOUTH;
            slots[2].input_key = GAMEPAD_WEST;
            slots[3].input_key = GAMEPAD_EAST;
            break;

        default: // WS_SELECT_1234
            slots[0].input_key = translate[SDL_SCANCODE_1];
            slots[1].input_key = translate[SDL_SCANCODE_2];
            slots[2].input_key = translate[SDL_SCANCODE_3];
            slots[3].input_key = translate[SDL_SCANCODE_4];
            break;
    }
}

//
// WS_UpdateSlots
//
void WS_UpdateSlots(void)
{
    const weapon_slots_type_t types[NUM_WS_SLOTS][NUM_WS_WEAPS] = {
        {weapon_slots_1_1, weapon_slots_1_2, weapon_slots_1_3},
        {weapon_slots_2_1, weapon_slots_2_2, weapon_slots_2_3},
        {weapon_slots_3_1, weapon_slots_3_2, weapon_slots_3_3},
        {weapon_slots_4_1, weapon_slots_4_2, weapon_slots_4_3},
    };

    for (int i = 0; i < NUM_WS_SLOTS; i++)
    {
        int count[NUMWEAPONS] = {0};
        slots[i].num_weapons = 0;

        for (int j = 0; j < NUM_WS_WEAPS; j++)
        {
            if (types[i][j] == WS_TYPE_NONE)
            {
                continue; // Skip "none" weapon type.
            }

            const weapontype_t weapon = types[i][j] - WS_TYPE_FIST;

            if (++count[weapon] > 1)
            {
                continue; // Skip duplicate weapons.
            }

            slots[i].weapons[slots[i].num_weapons++] = weapon;
        }
    }
}

//
// WS_Init
//
void WS_Init(void)
{
    WS_UpdateSelection();
    WS_UpdateSlots();
    WS_Reset();
}

//
// Weapon slots responder functions.
//

static boolean SearchSlotMatch(const event_t *ev, int *index)
{
    for (int i = 0; i < NUM_WS_SLOTS; i++)
    {
        if (ev->data1.i == slots[i].input_key)
        {
            *index = i;
            return true;
        }
    }

    *index = -1;
    return false;
}

static boolean InputKeyMatch(const event_t *ev, int *index)
{
    switch (ev->type)
    {
        case ev_joyb_down:
        case ev_joyb_up:
            if (weapon_slots_selection != WS_SELECT_1234)
            {
                return SearchSlotMatch(ev, index);
            }
            break;

        case ev_keydown:
        case ev_keyup:
            if (weapon_slots_selection == WS_SELECT_1234)
            {
                return SearchSlotMatch(ev, index);
            }
            break;

        default:
            break;
    }

    *index = -1;
    return false;
}

//
// WS_UpdateState
//
void WS_UpdateState(const event_t *ev)
{
    if (WS_Enabled()
        // If using gamepad weapon slots, then also using a gamepad.
        && (weapon_slots_selection == WS_SELECT_1234 || I_UseGamepad())
        // Playing the game.
        && (!menuactive && !demoplayback && gamestate == GS_LEVEL && !paused))
    {
        state.enabled = true;
        state.key_match = (InputKeyMatch(ev, &state.index)
                           && slots[state.index].num_weapons > 0);

        const boolean override = (
            // Only a gamepad can override other responders.
            (I_UseGamepad() && weapon_slots_selection != WS_SELECT_1234)
            // The current input key matches a weapon slot key.
            && state.key_match);

        state.override =
            (override
             // Weapon slots selected directly or hold "last" is activated.
             && (weapon_slots_activation == WS_ALWAYS_ON || state.activated));

        state.hold_override =
            (override
             // Weapon slots are activated using hold "last" only.
             && (weapon_slots_activation == WS_HOLD_LAST && state.activated));
    }
    else if (state.enabled)
    {
        WS_Reset();
    }
}

//
// WS_Override
//
boolean WS_Override(void)
{
    return state.override;
}

//
// WS_HoldOverride
//
boolean WS_HoldOverride(void)
{
    return state.hold_override;
}

//
// WS_UpdateStateTic
//
void WS_UpdateStateTic(void)
{
    if (shared_event.restored)
    {
        ResetSharedEvent();
    }

    state.current_slot = NULL;
}

static void RestoreSharedEvent(void)
{
// Restore the shared event if the activator was pressed and then quickly
// released without pressing a weapon slot.
#define MAX_RESTORE_TICS 21 // 600 ms

    if ((state.activated && !state.pressed)
        && (!shared_event.restored && shared_event.state != NULL)
        && (I_GetTime() - shared_event.time <= MAX_RESTORE_TICS))
    {
        shared_event.time = 0;
        shared_event.input_key = -1;
        *shared_event.state = true;
        shared_event.restored = true;
    }
    else
    {
        ResetSharedEvent();
    }
}

static void SetSharedEvent(const event_t *ev, boolean *buttons, int32_t size)
{
    if (ev->data1.i >= 0 && ev->data1.i < size)
    {
        shared_event.time = I_GetTime();
        shared_event.input_key = ev->data1.i;
        shared_event.state = &buttons[ev->data1.i];
        shared_event.restored = false;
    }
    else
    {
        ResetSharedEvent();
    }
}

static void BackupSharedEvent(const event_t *ev)
{
    switch (ev->type)
    {
        case ev_joyb_down:
            SetSharedEvent(ev, joybuttons, NUM_GAMEPAD_BUTTONS);
            break;

        case ev_keydown:
            SetSharedEvent(ev, gamekeydown, NUMKEYS);
            break;

        case ev_mouseb_down:
            SetSharedEvent(ev, mousebuttons, NUM_MOUSE_BUTTONS);
            break;

        default:
            ResetSharedEvent();
            break;
    }
}

static boolean UpdateSelection(const event_t *ev)
{
    if (!state.key_match)
    {
        return false;
    }

    switch (ev->type)
    {
        case ev_joyb_down:
        case ev_keydown:
            if (!state.selected)
            {
                state.pressed = true;
                state.selected = true;
                state.input_key = ev->data1.i;
                state.current_slot = &slots[state.index];
            }
            break;

        case ev_joyb_up:
        case ev_keyup:
            if (state.selected && ev->data1.i == state.input_key)
            {
                state.selected = false;
                state.input_key = -1;
            }
            break;

        default:
            break;
    }

    return true;
}

//
// WS_Responder
//
boolean WS_Responder(const event_t *ev)
{
    if (!state.enabled)
    {
        return false;
    }

    switch (weapon_slots_activation)
    {
        case WS_ALWAYS_ON:
            return UpdateSelection(ev);

        case WS_HOLD_LAST:
            if (M_InputActivated(input_lastweapon))
            {
                if (!state.activated)
                {
                    BackupSharedEvent(ev);
                    ResetState();
                    state.activated = true;
                }
                return true;
            }
            else if (M_InputDeactivated(input_lastweapon))
            {
                if (state.activated && ev->data1.i == shared_event.input_key)
                {
                    RestoreSharedEvent();
                    ResetState();
                    return true;
                }
                return false;
            }
            else if (state.activated)
            {
                return UpdateSelection(ev);
            }
            break;

        default:
            break;
    }

    return false;
}

//
// Weapon slots switching functions.
//

static weapontype_t FinalWeapon(const player_t *player,
                                weapontype_t current_weapon,
                                weapontype_t final_slot_weapon)
{
    switch (final_slot_weapon)
    {
        case wp_fist:
        case wp_chainsaw:
            if (player->weaponowned[wp_chainsaw]
                && current_weapon != wp_chainsaw && current_weapon == wp_fist)
            {
                return wp_chainsaw;
            }
            break;

        case wp_shotgun:
        case wp_supershotgun:
            if (ALLOW_SSG && player->weaponowned[wp_supershotgun]
                && player->weaponowned[wp_shotgun]
                && current_weapon == wp_shotgun)
            {
                return wp_shotgun; // wp_supershotgun
            }
            break;

        default:
            break;
    }

    return wp_nochange;
}

static boolean Selectable(const player_t *player, weapontype_t slot_weapon)
{
    if ((slot_weapon == wp_plasma || slot_weapon == wp_bfg)
        && gamemission == doom && gamemode == shareware)
    {
        return false;
    }

    if (!player->weaponowned[slot_weapon])
    {
        return false;
    }

    return true;
}

static boolean NextWeapon(const player_t *player, weapontype_t current_weapon,
                          weapontype_t slot_weapon, weapontype_t *next_weapon)
{
    switch (slot_weapon)
    {
        case wp_fist:
        case wp_chainsaw:
            if (player->weaponowned[wp_chainsaw]
                && current_weapon != wp_chainsaw && current_weapon != wp_fist)
            {
                *next_weapon = wp_chainsaw;
                return true;
            }
            else if (current_weapon != wp_fist
                     && (!player->weaponowned[wp_chainsaw]
                         || (current_weapon == wp_chainsaw
                             && player->powers[pw_strength])))
            {
                *next_weapon = wp_fist;
                return true;
            }
            break;

        case wp_shotgun:
        case wp_supershotgun:
            if (ALLOW_SSG && player->weaponowned[wp_supershotgun]
                && current_weapon != wp_supershotgun
                && current_weapon != wp_shotgun)
            {
                *next_weapon = wp_shotgun; // wp_supershotgun
                return true;
            }
            else if (player->weaponowned[wp_shotgun]
                     && current_weapon != wp_shotgun
                     && (current_weapon == wp_supershotgun || !ALLOW_SSG
                         || !player->weaponowned[wp_supershotgun]))
            {
                *next_weapon = wp_shotgun;
                return true;
            }
            break;

        default:
            if (Selectable(player, slot_weapon))
            {
                *next_weapon = slot_weapon;
                return true;
            }
            break;
    }

    return false;
}

static int CurrentPosition(weapontype_t current_weapon,
                           const weapontype_t *slot_weapons, int num_weapons)
{
    for (int pos = 0; pos < num_weapons; pos++)
    {
        const int next_pos = (pos + 1) % num_weapons;
        const weapontype_t next_weapon = slot_weapons[next_pos];

        switch (slot_weapons[pos])
        {
            case wp_fist:
            case wp_chainsaw:
                if (next_weapon == wp_fist || next_weapon == wp_chainsaw)
                {
                    continue; // Skip duplicates.
                }
                else if (current_weapon == wp_fist
                         || current_weapon == wp_chainsaw)
                {
                    return pos; // Start in current position.
                }
                break;

            case wp_shotgun:
            case wp_supershotgun:
                if (next_weapon == wp_shotgun || next_weapon == wp_supershotgun)
                {
                    continue; // Skip duplicates.
                }
                else if (current_weapon == wp_shotgun
                         || current_weapon == wp_supershotgun)
                {
                    return pos; // Start in current position.
                }
                break;

            default:
                if (current_weapon == slot_weapons[pos])
                {
                    return next_pos; // Start in next position.
                }
                break;
        }
    }

    return 0; // Start in first position.
}

static weapontype_t SlotWeaponVanilla(const player_t *player,
                                      weapontype_t current_weapon,
                                      const weapontype_t *slot_weapons,
                                      int num_weapons)
{
    int pos = CurrentPosition(current_weapon, slot_weapons, num_weapons) - 1;
    weapontype_t next_weapon;

    for (int i = 0; i < num_weapons; i++)
    {
        pos = (pos + 1) % num_weapons;

        if (NextWeapon(player, current_weapon, slot_weapons[pos], &next_weapon))
        {
            return next_weapon;
        }
    }

    return FinalWeapon(player, current_weapon, slot_weapons[num_weapons - 1]);
}

static weapontype_t SlotWeapon(const player_t *player,
                               weapontype_t current_weapon,
                               const weapontype_t *slot_weapons,
                               int num_weapons)
{
    int pos = -1;

    for (int i = 0; i < num_weapons; i++)
    {
        if (current_weapon == slot_weapons[i])
        {
            pos = i;
            break;
        }
    }

    for (int i = 0; i < num_weapons; i++)
    {
        pos = (pos + 1) % num_weapons;

        if (slot_weapons[pos] == wp_supershotgun && !ALLOW_SSG)
        {
            continue;
        }

        if (Selectable(player, slot_weapons[pos]))
        {
            return slot_weapons[pos];
        }
    }

    return wp_nochange;
}

static weapontype_t CurrentWeapon(const player_t *player)
{
    if (player->pendingweapon == wp_nochange)
    {
        return player->readyweapon;
    }
    else
    {
        return player->pendingweapon;
    }
}

//
// WS_SlotSelected
//
boolean WS_SlotSelected(void)
{
    return (state.current_slot != NULL);
}

//
// WS_SlotWeapon
//
weapontype_t WS_SlotWeapon(void)
{
    const player_t *player = &players[consoleplayer];
    const weapontype_t current_weapon = CurrentWeapon(player);
    const weapontype_t *slot_weapons = state.current_slot->weapons;
    const int num_weapons = state.current_slot->num_weapons;

    if (!demo_compatibility)
    {
        return SlotWeapon(player, current_weapon, slot_weapons, num_weapons);
    }
    else
    {
        return SlotWeaponVanilla(player, current_weapon, slot_weapons,
                                 num_weapons);
    }
}

//
// WS_BindVariables
//

#define BIND_NUM_WEAP(name, v, a, b, help) \
    M_BindNum(#name, &name, NULL, (v), (a), (b), ss_weap, wad_no, help)

#define BIND_SLOT(name, v, help) \
    BIND_NUM_WEAP(name, (v), WS_TYPE_NONE, NUM_WS_TYPES - 1, help)

void WS_BindVariables(void)
{
    BIND_NUM_WEAP(weapon_slots_activation,
        WS_OFF, WS_OFF, NUM_WS_ACTIVATION - 1,
        "Weapon slots activation (0 = Off; 1 = Hold \"Last\"; 2 = Always On)");

    BIND_NUM_WEAP(weapon_slots_selection,
        WS_SELECT_DPAD, WS_SELECT_DPAD, NUM_WS_SELECT - 1,
        "Weapon slots selection (0 = D-Pad; 1 = Face Buttons; 2 = 1-4 Keys)");

    BIND_SLOT(weapon_slots_1_1, WS_TYPE_SSG,      "Slot 1, weapon 1");
    BIND_SLOT(weapon_slots_1_2, WS_TYPE_SHOTGUN,  "Slot 1, weapon 2");
    BIND_SLOT(weapon_slots_1_3, WS_TYPE_NONE,     "Slot 1, weapon 3");

    BIND_SLOT(weapon_slots_2_1, WS_TYPE_ROCKET,   "Slot 2, weapon 1");
    BIND_SLOT(weapon_slots_2_2, WS_TYPE_CHAINSAW, "Slot 2, weapon 2");
    BIND_SLOT(weapon_slots_2_3, WS_TYPE_FIST,     "Slot 2, weapon 3");

    BIND_SLOT(weapon_slots_3_1, WS_TYPE_PLASMA,   "Slot 3, weapon 1");
    BIND_SLOT(weapon_slots_3_2, WS_TYPE_BFG,      "Slot 3, weapon 2");
    BIND_SLOT(weapon_slots_3_3, WS_TYPE_NONE,     "Slot 3, weapon 3");

    BIND_SLOT(weapon_slots_4_1, WS_TYPE_CHAINGUN, "Slot 4, weapon 1");
    BIND_SLOT(weapon_slots_4_2, WS_TYPE_PISTOL,   "Slot 4, weapon 2");
    BIND_SLOT(weapon_slots_4_3, WS_TYPE_NONE,     "Slot 4, weapon 3");
}

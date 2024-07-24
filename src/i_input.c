//
// Copyright(C) 2023 Roman Fomin
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
//

#include "SDL.h"

#include "d_event.h"
#include "d_main.h"
#include "doomkeys.h"
#include "doomtype.h"
#include "i_gamepad.h"
#include "i_printf.h"
#include "i_system.h"
#include "m_config.h"

#define AXIS_BUTTON_DEADZONE (SDL_JOYSTICK_AXIS_MAX / 3)

static SDL_GameController *gamepad;
static int gamepad_index = -1;

// [FG] adapt joystick button and axis handling from Chocolate Doom 3.0

static int GetAxisState(int axis)
{
    return SDL_GameControllerGetAxis(gamepad, axis);
}

static void AxisToButton(int value, int *state, int direction)
{
    int button = -1;

    if (value < AXIS_BUTTON_DEADZONE && value > -AXIS_BUTTON_DEADZONE)
    {
        value = 0;
    }

    if (value < 0)
    {
        button = direction;
    }
    else if (value > 0)
    {
        button = direction + 1;
    }

    if (button != *state)
    {
        if (*state != -1)
        {
            static event_t up;
            up.data1 = *state;
            up.type = ev_joyb_up;
            D_PostEvent(&up);
        }

        if (button != -1)
        {
            static event_t down;
            down.data1 = button;
            down.type = ev_joyb_down;
            D_PostEvent(&down);
        }

        *state = button;
    }
}

static int axisbuttons[] = {-1, -1, -1, -1};

static void AxisToButtons(event_t *ev)
{
    AxisToButton(ev->data1, &axisbuttons[0], GAMEPAD_LEFT_STICK_LEFT);
    AxisToButton(ev->data2, &axisbuttons[1], GAMEPAD_LEFT_STICK_UP);
    AxisToButton(ev->data3, &axisbuttons[2], GAMEPAD_RIGHT_STICK_LEFT);
    AxisToButton(ev->data4, &axisbuttons[3], GAMEPAD_RIGHT_STICK_UP);
}

static void TriggerToButton(int value, boolean *trigger_on, int trigger_type)
{
    static event_t ev;

    if (value > trigger_threshold && !*trigger_on)
    {
        *trigger_on = true;
        ev.type = ev_joyb_down;
    }
    else if (value <= trigger_threshold && *trigger_on)
    {
        *trigger_on = false;
        ev.type = ev_joyb_up;
    }
    else
    {
        return;
    }

    ev.data1 = trigger_type;
    D_PostEvent(&ev);
}

static void TriggerToButtons(void)
{
    static boolean left_trigger_on;
    static boolean right_trigger_on;

    TriggerToButton(GetAxisState(SDL_CONTROLLER_AXIS_TRIGGERLEFT),
                    &left_trigger_on, GAMEPAD_LEFT_TRIGGER);
    TriggerToButton(GetAxisState(SDL_CONTROLLER_AXIS_TRIGGERRIGHT),
                    &right_trigger_on, GAMEPAD_RIGHT_TRIGGER);
}

void I_UpdateGamepad(evtype_t type, boolean axis_buttons)
{
    static event_t ev;

    ev.type = type;
    ev.data1 = GetAxisState(SDL_CONTROLLER_AXIS_LEFTX);
    ev.data2 = GetAxisState(SDL_CONTROLLER_AXIS_LEFTY);
    ev.data3 = GetAxisState(SDL_CONTROLLER_AXIS_RIGHTX);
    ev.data4 = GetAxisState(SDL_CONTROLLER_AXIS_RIGHTY);

    D_PostEvent(&ev);

    if (axis_buttons)
    {
        AxisToButtons(&ev);
        TriggerToButtons();
    }
}

static void UpdateTouchState(boolean menu, boolean on)
{
    static event_t ev = {.data1 = GAMEPAD_TOUCHPAD_TOUCH};

    if (on)
    {
        if (!menu)
        {
            ev.type = ev_joyb_down;
            D_PostEvent(&ev);
        }
    }
    else if (menu) // Allow separate "touch" and "press" bindings.
    {
        ev.type = ev_joyb_down;
        D_PostEvent(&ev);
        ev.type = ev_joyb_up;
        D_PostEvent(&ev);
    }
    else
    {
        ev.type = ev_joyb_up;
        D_PostEvent(&ev);
    }
}

static void UpdateGamepadButtonState(unsigned int button, boolean on)
{
    static event_t event;
    if (on)
    {
        event.type = ev_joyb_down;
    }
    else
    {
        event.type = ev_joyb_up;
    }

    event.data1 = button;
    D_PostEvent(&event);
}

boolean I_UseGamepad(void)
{
    return (gamepad != NULL);
}

void I_FlushGamepadEvents(void)
{
    SDL_PumpEvents();
    SDL_FlushEvent(SDL_JOYHATMOTION);
    SDL_FlushEvent(SDL_JOYBUTTONDOWN);
    SDL_FlushEvent(SDL_JOYBUTTONUP);
    SDL_FlushEvent(SDL_CONTROLLERBUTTONDOWN);
    SDL_FlushEvent(SDL_CONTROLLERBUTTONUP);
    SDL_FlushEvent(SDL_CONTROLLERTOUCHPADDOWN);
    SDL_FlushEvent(SDL_CONTROLLERTOUCHPADUP);
}

static void SetTouchEventState(boolean condition)
{
    if (condition && gamepad && SDL_GameControllerGetNumTouchpads(gamepad) > 0)
    {
        SDL_EventState(SDL_CONTROLLERTOUCHPADDOWN, SDL_ENABLE);
        SDL_EventState(SDL_CONTROLLERTOUCHPADUP, SDL_ENABLE);
    }
    else
    {
        SDL_EventState(SDL_CONTROLLERTOUCHPADDOWN, SDL_IGNORE);
        SDL_EventState(SDL_CONTROLLERTOUCHPADUP, SDL_IGNORE);
    }
}

static void EnableGamepadEvents(void)
{
    SDL_EventState(SDL_JOYHATMOTION, SDL_ENABLE);
    SDL_EventState(SDL_JOYBUTTONDOWN, SDL_ENABLE);
    SDL_EventState(SDL_JOYBUTTONUP, SDL_ENABLE);
    SDL_EventState(SDL_CONTROLLERBUTTONDOWN, SDL_ENABLE);
    SDL_EventState(SDL_CONTROLLERBUTTONUP, SDL_ENABLE);
    SetTouchEventState(true);
}

static void DisableGamepadEvents(void)
{
    SDL_EventState(SDL_JOYHATMOTION, SDL_IGNORE);
    SDL_EventState(SDL_JOYBUTTONDOWN, SDL_IGNORE);
    SDL_EventState(SDL_JOYBUTTONUP, SDL_IGNORE);
    SDL_EventState(SDL_CONTROLLERBUTTONDOWN, SDL_IGNORE);
    SDL_EventState(SDL_CONTROLLERBUTTONUP, SDL_IGNORE);
    SetTouchEventState(false);

    // Always ignore unsupported gamepad events.
    SDL_EventState(SDL_JOYAXISMOTION, SDL_IGNORE);
    SDL_EventState(SDL_JOYBALLMOTION, SDL_IGNORE);
#if SDL_VERSION_ATLEAST(2, 24, 0)
    SDL_EventState(SDL_JOYBATTERYUPDATED, SDL_IGNORE);
#endif
    SDL_EventState(SDL_CONTROLLERAXISMOTION, SDL_IGNORE);
    SDL_EventState(SDL_CONTROLLERDEVICEREMAPPED, SDL_IGNORE);
    SDL_EventState(SDL_CONTROLLERTOUCHPADMOTION, SDL_IGNORE);
    SDL_EventState(SDL_CONTROLLERSENSORUPDATE, SDL_IGNORE);
}

static void I_ShutdownGamepad(void)
{
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
}

void I_InitGamepad(void)
{
    if (!joy_enable)
    {
        return;
    }

    if (SDL_Init(SDL_INIT_GAMECONTROLLER) < 0)
    {
        I_Printf(VB_WARNING,
                 "I_InitGamepad: Failed to initialize gamepad: %s",
                 SDL_GetError());
        return;
    }

    DisableGamepadEvents();

    I_Printf(VB_INFO, "I_InitGamepad: Initialize gamepad.");

    I_AtExit(I_ShutdownGamepad, true);
}

void I_OpenGamepad(int which)
{
    if (gamepad)
    {
        return;
    }

    if (SDL_IsGameController(which))
    {
        gamepad = SDL_GameControllerOpen(which);
        if (gamepad)
        {
            gamepad_index = which;
            I_Printf(VB_INFO,
                     "I_OpenGamepad: Found a valid gamepad, named: %s",
                     SDL_GameControllerName(gamepad));
        }
    }

    if (gamepad == NULL)
    {
        I_Printf(VB_ERROR,
                 "I_OpenGamepad: Could not open gamepad %i: %s",
                 which, SDL_GetError());
    }

    I_ResetGamepad();

    if (gamepad)
    {
        EnableGamepadEvents();
    }
}

void I_CloseGamepad(int which)
{
    if (gamepad != NULL && gamepad_index == which)
    {
        SDL_GameControllerClose(gamepad);
        gamepad = NULL;
        gamepad_index = -1;
    }

    DisableGamepadEvents();
    I_ResetGamepad();
}

void I_HandleGamepadEvent(SDL_Event *sdlevent, boolean menu)
{
    switch (sdlevent->type)
    {
        case SDL_CONTROLLERBUTTONDOWN:
            UpdateGamepadButtonState(sdlevent->cbutton.button, true);
            break;

        case SDL_CONTROLLERBUTTONUP:
            UpdateGamepadButtonState(sdlevent->cbutton.button, false);
            break;

        case SDL_CONTROLLERTOUCHPADDOWN:
            UpdateTouchState(menu, true);
            break;

        case SDL_CONTROLLERTOUCHPADUP:
            UpdateTouchState(menu, false);
            break;

        default:
            break;
    }
}

// [FG] updated to scancode-based approach from Chocolate Doom 3.0

static const int scancode_translate_table[] = SCANCODE_TO_KEYS_ARRAY;

static int TranslateKey(int scancode)
{
    switch (scancode)
    {
        case SDL_SCANCODE_LCTRL:
        case SDL_SCANCODE_RCTRL:
            return KEY_RCTRL;

        case SDL_SCANCODE_LSHIFT:
        case SDL_SCANCODE_RSHIFT:
            return KEY_RSHIFT;

        case SDL_SCANCODE_LALT:
            return KEY_LALT;

        case SDL_SCANCODE_RALT:
            return KEY_RALT;

        default:
            if (scancode >= 0 && scancode < arrlen(scancode_translate_table))
            {
                return scancode_translate_table[scancode];
            }
            else
            {
                return 0;
            }
    }
}

static void UpdateMouseButtonState(unsigned int button, boolean on,
                                   unsigned int dclick)
{
    static event_t event;

    if (button < SDL_BUTTON_LEFT || button > NUM_MOUSE_BUTTONS)
    {
        return;
    }

    // Note: button "0" is left, button "1" is right,
    // button "2" is middle for Doom.  This is different
    // to how SDL sees things.

    switch (button)
    {
        case SDL_BUTTON_LEFT:
            button = 0;
            break;

        case SDL_BUTTON_RIGHT:
            button = 1;
            break;

        case SDL_BUTTON_MIDDLE:
            button = 2;
            break;

        default:
            // SDL buttons are indexed from 1.
            --button;
            break;
    }

    // Turn bit representing this button on or off.

    if (on)
    {
        event.type = ev_mouseb_down;
    }
    else
    {
        event.type = ev_mouseb_up;
    }

    // Post an event with the new button state.

    event.data1 = button;
    event.data2 = dclick;
    D_PostEvent(&event);
}

static event_t delay_event;

static void MapMouseWheelToButtons(SDL_MouseWheelEvent wheel)
{
    static event_t down;
    int button;

    if (wheel.y < 0)
    {
        button = MOUSE_BUTTON_WHEELDOWN;
    }
    else if (wheel.y > 0)
    {
        button = MOUSE_BUTTON_WHEELUP;
    }
    else if (wheel.x < 0)
    {
        button = MOUSE_BUTTON_WHEELLEFT;
    }
    else if (wheel.x > 0)
    {
        button = MOUSE_BUTTON_WHEELRIGHT;
    }
    else
    {
        return;
    }

    // post a button down event
    down.type = ev_mouseb_down;
    down.data1 = button;
    D_PostEvent(&down);

    // hold button for one tic, required for checks in G_BuildTiccmd
    delay_event.type = ev_mouseb_up;
    delay_event.data1 = button;
}

void I_DelayEvent(void)
{
    if (delay_event.data1)
    {
        D_PostEvent(&delay_event);
        delay_event.data1 = 0;
    }
}

//
// Read the change in mouse state to generate mouse motion events
//

void I_ReadMouse(void)
{
    static event_t ev = {.type = ev_mouse};

    SDL_GetRelativeMouseState(&ev.data1, &ev.data2);

    if (ev.data1 || ev.data2)
    {
        D_PostEvent(&ev);
        ev.data1 = ev.data2 = 0;
    }
}

void I_HandleMouseEvent(SDL_Event *sdlevent)
{
    switch (sdlevent->type)
    {
        case SDL_MOUSEBUTTONDOWN:
            UpdateMouseButtonState(sdlevent->button.button, true,
                                   sdlevent->button.clicks);
            break;

        case SDL_MOUSEBUTTONUP:
            UpdateMouseButtonState(sdlevent->button.button, false, 0);
            break;

        case SDL_MOUSEWHEEL:
            MapMouseWheelToButtons(sdlevent->wheel);
            break;

        default:
            break;
    }
}

void I_HandleKeyboardEvent(SDL_Event *sdlevent)
{
    static event_t event;

    switch (sdlevent->type)
    {
        case SDL_KEYDOWN:
            event.type = ev_keydown;
            event.data1 = TranslateKey(sdlevent->key.keysym.scancode);

            if (event.data1 != 0)
            {
                D_PostEvent(&event);
            }
            break;

        case SDL_KEYUP:
            event.type = ev_keyup;
            event.data1 = TranslateKey(sdlevent->key.keysym.scancode);

            // data2/data3 are initialized to zero for ev_keyup.
            // For ev_keydown it's the shifted Unicode character
            // that was typed, but if something wants to detect
            // key releases it should do so based on data1
            // (key ID), not the printable char.

            if (event.data1 != 0)
            {
                D_PostEvent(&event);
            }
            break;

        default:
            break;
    }
}

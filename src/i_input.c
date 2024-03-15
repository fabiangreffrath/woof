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
#include "r_main.h"

#define AXIS_BUTTON_DEADZONE (SDL_JOYSTICK_AXIS_MAX / 3)

static SDL_GameController *controller;
static int controller_index = -1;

// [FG] adapt joystick button and axis handling from Chocolate Doom 3.0

static int GetAxisState(int axis)
{
    return SDL_GameControllerGetAxis(controller, axis);
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
    AxisToButton(ev->data1, &axisbuttons[0], CONTROLLER_LEFT_STICK_LEFT);
    AxisToButton(ev->data2, &axisbuttons[1], CONTROLLER_LEFT_STICK_UP);
    AxisToButton(ev->data3, &axisbuttons[2], CONTROLLER_RIGHT_STICK_LEFT);
    AxisToButton(ev->data4, &axisbuttons[3], CONTROLLER_RIGHT_STICK_UP);
}

void I_UpdateJoystick(boolean axis_buttons)
{
    static event_t ev;

    if (controller == NULL)
    {
        return;
    }

    ev.data1 = GetAxisState(SDL_CONTROLLER_AXIS_LEFTX);
    ev.data2 = GetAxisState(SDL_CONTROLLER_AXIS_LEFTY);
    ev.data3 = GetAxisState(SDL_CONTROLLER_AXIS_RIGHTX);
    ev.data4 = GetAxisState(SDL_CONTROLLER_AXIS_RIGHTY);

    if (axis_buttons)
    {
        AxisToButtons(&ev);
    }

    ev.type = ev_joystick;
    D_PostEvent(&ev);
}

void I_UpdateJoystickMenu(void)
{
    static event_t ev;

    if (controller == NULL)
    {
        return;
    }

    ev.data1 = GetAxisState(SDL_CONTROLLER_AXIS_LEFTX);
    ev.data2 = GetAxisState(SDL_CONTROLLER_AXIS_LEFTY);
    ev.data3 = GetAxisState(SDL_CONTROLLER_AXIS_RIGHTX);
    ev.data4 = GetAxisState(SDL_CONTROLLER_AXIS_RIGHTY);

    AxisToButtons(&ev);
    ev.type = ev_joystick_state;
    D_PostEvent(&ev);
}

static void UpdateJoystickButtonState(unsigned int button, boolean on)
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

static void UpdateControllerAxisState(unsigned int value, boolean left_trigger)
{
    int button;
    static event_t event;
    static boolean left_trigger_on;
    static boolean right_trigger_on;

    if (left_trigger)
    {
        if (value > trigger_threshold && !left_trigger_on)
        {
            left_trigger_on = true;
            event.type = ev_joyb_down;
        }
        else if (value <= trigger_threshold && left_trigger_on)
        {
            left_trigger_on = false;
            event.type = ev_joyb_up;
        }
        else
        {
            return;
        }

        button = CONTROLLER_LEFT_TRIGGER;
    }
    else
    {
        if (value > trigger_threshold && !right_trigger_on)
        {
            right_trigger_on = true;
            event.type = ev_joyb_down;
        }
        else if (value <= trigger_threshold && right_trigger_on)
        {
            right_trigger_on = false;
            event.type = ev_joyb_up;
        }
        else
        {
            return;
        }

        button = CONTROLLER_RIGHT_TRIGGER;
    }

    event.data1 = button;
    D_PostEvent(&event);
}

boolean I_UseController(void)
{
    return (controller && joy_enable);
}

static void EnableControllerEvents(void)
{
    SDL_EventState(SDL_CONTROLLERAXISMOTION, SDL_ENABLE);
    SDL_EventState(SDL_CONTROLLERBUTTONDOWN, SDL_ENABLE);
    SDL_EventState(SDL_CONTROLLERBUTTONUP, SDL_ENABLE);
}

static void DisableControllerEvents(void)
{
    SDL_EventState(SDL_CONTROLLERAXISMOTION, SDL_IGNORE);
    SDL_EventState(SDL_CONTROLLERBUTTONDOWN, SDL_IGNORE);
    SDL_EventState(SDL_CONTROLLERBUTTONUP, SDL_IGNORE);

    // Always ignore unsupported game controller events.
    SDL_EventState(SDL_CONTROLLERDEVICEREMAPPED, SDL_IGNORE);
    SDL_EventState(SDL_CONTROLLERTOUCHPADDOWN, SDL_IGNORE);
    SDL_EventState(SDL_CONTROLLERTOUCHPADMOTION, SDL_IGNORE);
    SDL_EventState(SDL_CONTROLLERTOUCHPADUP, SDL_IGNORE);
    SDL_EventState(SDL_CONTROLLERSENSORUPDATE, SDL_IGNORE);
}

static void I_ShutdownController(void)
{
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
}

void I_InitController(void)
{
    if (!joy_enable)
    {
        SDL_GameControllerEventState(SDL_IGNORE);
        return;
    }

    if (SDL_Init(SDL_INIT_GAMECONTROLLER) < 0)
    {
        I_Printf(VB_WARNING,
                 "I_InitController: Failed to initialize game controller: %s",
                 SDL_GetError());
        return;
    }

    SDL_GameControllerEventState(SDL_ENABLE);
    DisableControllerEvents();

    I_Printf(VB_INFO, "I_InitController: Initialize game controller.");

    I_AtExit(I_ShutdownController, true);
}

void I_OpenController(int which)
{
    if (controller)
    {
        return;
    }

    if (SDL_IsGameController(which))
    {
        controller = SDL_GameControllerOpen(which);
        if (controller)
        {
            controller_index = which;
            I_Printf(VB_INFO,
                     "I_OpenController: Found a valid game controller"
                     ", named: %s",
                     SDL_GameControllerName(controller));
        }
    }

    if (controller == NULL)
    {
        I_Printf(VB_ERROR,
                 "I_OpenController: Could not open game controller %i: %s",
                 which, SDL_GetError());
    }

    I_ResetController();

    if (controller)
    {
        EnableControllerEvents();
    }
}

void I_CloseController(int which)
{
    if (controller != NULL && controller_index == which)
    {
        SDL_GameControllerClose(controller);
        controller = NULL;
        controller_index = -1;
    }

    DisableControllerEvents();
    I_ResetController();
}

void I_HandleJoystickEvent(SDL_Event *sdlevent)
{
    switch (sdlevent->type)
    {
        case SDL_CONTROLLERBUTTONDOWN:
            UpdateJoystickButtonState(sdlevent->cbutton.button, true);
            break;

        case SDL_CONTROLLERBUTTONUP:
            UpdateJoystickButtonState(sdlevent->cbutton.button, false);
            break;

        case SDL_CONTROLLERAXISMOTION:
            if (sdlevent->caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT)
            {
                UpdateControllerAxisState(sdlevent->caxis.value, true);
            }
            else if (sdlevent->caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
            {
                UpdateControllerAxisState(sdlevent->caxis.value, false);
            }
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
// This is to combine all mouse movement for a tic into one mouse
// motion event.

// The mouse input values are input directly to the game, but when the values
// exceed the value of mouse_acceleration_threshold, they are multiplied by
// mouse_acceleration to increase the speed.

int mouse_acceleration;
int mouse_acceleration_threshold;

static double AccelerateMouse_Thresh(int val)
{
    if (val < 0)
    {
        return -AccelerateMouse_Thresh(-val);
    }

    if (val > mouse_acceleration_threshold)
    {
        return ((double)(val - mouse_acceleration_threshold)
                    * (mouse_acceleration + 10) / 10
                + mouse_acceleration_threshold);
    }
    else
    {
        return val;
    }
}

static double AccelerateMouse_NoThresh(int val)
{
    return ((double)val * (mouse_acceleration + 10) / 10);
}

static double AccelerateMouse_Skip(int val)
{
    return val;
}

double (*I_AccelerateMouse)(int val) = AccelerateMouse_NoThresh;

void I_UpdateAccelerateMouse(void)
{
    if (mouse_acceleration)
    {
        I_AccelerateMouse =
            raw_input ? AccelerateMouse_NoThresh : AccelerateMouse_Thresh;
    }
    else
    {
        I_AccelerateMouse = AccelerateMouse_Skip;
    }
}

void I_ReadMouse(void)
{
    int x, y;
    static event_t ev;

    SDL_GetRelativeMouseState(&x, &y);

    if (x != 0 || y != 0)
    {
        ev.type = ev_mouse;
        ev.data1 = 0;
        ev.data2 = x;
        ev.data3 = -y;

        D_PostEvent(&ev);
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

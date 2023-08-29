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

#include "doomtype.h"
#include "doomkeys.h"
#include "i_printf.h"
#include "d_event.h"
#include "d_main.h"
#include "m_fixed.h"
#include "i_timer.h"
#include "i_video.h"

static SDL_GameController *controller;
static int controller_index = -1;

// When an axis is within the dead zone, it is set to zero.
#define DEAD_ZONE (32768 / 3)

#define TRIGGER_THRESHOLD 30 // from xinput.h

// [FG] adapt joystick button and axis handling from Chocolate Doom 3.0

static int GetAxisState(int axis)
{
    int result;

    result = SDL_GameControllerGetAxis(controller, axis);

    if (result < DEAD_ZONE && result > -DEAD_ZONE)
    {
        result = 0;
    }

    return result;
}

static void AxisToButton(int value, int *state, int direction)
{
  int button = -1;

  if (value < 0)
    button = direction;
  else if (value > 0)
    button = direction + 1;

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

static int axisbuttons[] = { -1, -1, -1, -1 };

void I_UpdateJoystick(void)
{
    static event_t ev;

    if (controller == NULL)
    {
        return;
    }

    ev.type = ev_joystick;
    ev.data1 = GetAxisState(SDL_CONTROLLER_AXIS_LEFTX);
    ev.data2 = GetAxisState(SDL_CONTROLLER_AXIS_LEFTY);
    ev.data3 = GetAxisState(SDL_CONTROLLER_AXIS_RIGHTX);
    ev.data4 = GetAxisState(SDL_CONTROLLER_AXIS_RIGHTY);

    AxisToButton(ev.data1, &axisbuttons[0], CONTROLLER_LEFT_STICK_LEFT);
    AxisToButton(ev.data2, &axisbuttons[1], CONTROLLER_LEFT_STICK_UP);
    AxisToButton(ev.data3, &axisbuttons[2], CONTROLLER_RIGHT_STICK_LEFT);
    AxisToButton(ev.data4, &axisbuttons[3], CONTROLLER_RIGHT_STICK_UP);

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
        if (value > TRIGGER_THRESHOLD && !left_trigger_on)
        {
            left_trigger_on = true;
            event.type = ev_joyb_down;
        }
        else if (value <= TRIGGER_THRESHOLD && left_trigger_on)
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
        if (value > TRIGGER_THRESHOLD && !right_trigger_on)
        {
            right_trigger_on = true;
            event.type = ev_joyb_down;
        }
        else if (value <= TRIGGER_THRESHOLD && right_trigger_on)
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
            I_Printf(VB_INFO, "I_OpenController: Found a valid game controller, named: %s",
                    SDL_GameControllerName(controller));
        }
    }

    if (controller == NULL)
    {
        I_Printf(VB_WARNING, "I_OpenController: Could not open game controller %i: %s",
                which, SDL_GetError());
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

static void UpdateMouseButtonState(unsigned int button, boolean on, unsigned int dclick)
{
    static event_t event;

    if (button < SDL_BUTTON_LEFT || button > MAX_MB)
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
    {   // scroll down
        button = MOUSE_BUTTON_WHEELDOWN;
    }
    else if (wheel.y >0)
    {   // scroll up
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

static int AccelerateMouse(int val)
{
    if (val < 0)
        return -AccelerateMouse(-val);

    if (val > mouse_acceleration_threshold)
    {
        return (val - mouse_acceleration_threshold) * (mouse_acceleration + 10) / 10 + mouse_acceleration_threshold;
    }
    else
    {
        return val;
    }
}

// [crispy] Distribute the mouse movement between the current tic and the next
// based on how far we are into the current tic. Compensates for mouse sampling
// jitter.

static void SmoothMouse(int* x, int* y)
{
    static int x_remainder_old = 0;
    static int y_remainder_old = 0;
    int x_remainder, y_remainder;
    fixed_t correction_factor;
    fixed_t fractic;

    *x += x_remainder_old;
    *y += y_remainder_old;

    fractic = I_GetFracTime();
    correction_factor = FixedDiv(fractic, FRACUNIT + fractic);

    x_remainder = FixedMul(*x, correction_factor);
    *x -= x_remainder;
    x_remainder_old = x_remainder;

    y_remainder = FixedMul(*y, correction_factor);
    *y -= y_remainder;
    y_remainder_old = y_remainder;
}

void I_ReadMouse(void)
{
    int x, y;
    static event_t ev;

    SDL_GetRelativeMouseState(&x, &y);

    if (uncapped)
    {
        SmoothMouse(&x, &y);
    }

    if (x != 0 || y != 0)
    {
        ev.type = ev_mouse;
        ev.data1 = 0;
        ev.data2 = AccelerateMouse(x);
        ev.data3 = -AccelerateMouse(y);

        D_PostEvent(&ev);
    }
}

void I_HandleMouseEvent(SDL_Event *sdlevent)
{
    switch (sdlevent->type)
    {
        case SDL_MOUSEBUTTONDOWN:
            UpdateMouseButtonState(sdlevent->button.button, true, sdlevent->button.clicks);
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

//
// Copyright(C) 2005-2014 Simon Howard
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

#include <string.h>

#include "d_event.h"
#include "d_main.h"
#include "doomkeys.h"
#include "doomstat.h"
#include "doomtype.h"
#include "i_gamepad.h"
#include "i_gyro.h"
#include "i_printf.h"
#include "i_rumble.h"
#include "i_system.h"
#include "i_timer.h"
#include "m_array.h"
#include "m_config.h"
#include "m_input.h"
#include "m_misc.h"
#include "mn_menu.h"

#define AXIS_BUTTON_DEADZONE (SDL_JOYSTICK_AXIS_MAX / 3)

static const char **gamepad_strings;
static SDL_GameController *gamepad;
static SDL_JoystickID gamepad_instance_id = -1;
static boolean gyro_supported;
static joy_platform_t platform;

// [FG] adapt joystick button and axis handling from Chocolate Doom 3.0

int I_GetAxisState(int axis)
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
            up.data1.i = *state;
            up.type = ev_joyb_up;
            D_PostEvent(&up);
        }

        if (button != -1)
        {
            static event_t down;
            down.data1.i = button;
            down.type = ev_joyb_down;
            D_PostEvent(&down);
        }

        *state = button;
    }
}

static int axisbuttons[] = {-1, -1, -1, -1};

static void AxisToButtons(event_t *ev)
{
    AxisToButton(ev->data1.i, &axisbuttons[0], GAMEPAD_LEFT_STICK_LEFT);
    AxisToButton(ev->data2.i, &axisbuttons[1], GAMEPAD_LEFT_STICK_UP);
    AxisToButton(ev->data3.i, &axisbuttons[2], GAMEPAD_RIGHT_STICK_LEFT);
    AxisToButton(ev->data4.i, &axisbuttons[3], GAMEPAD_RIGHT_STICK_UP);
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

    ev.data1.i = trigger_type;
    D_PostEvent(&ev);
}

static void TriggerToButtons(void)
{
    static boolean left_trigger_on;
    static boolean right_trigger_on;

    TriggerToButton(I_GetAxisState(SDL_CONTROLLER_AXIS_TRIGGERLEFT),
                    &left_trigger_on, GAMEPAD_LEFT_TRIGGER);
    TriggerToButton(I_GetAxisState(SDL_CONTROLLER_AXIS_TRIGGERRIGHT),
                    &right_trigger_on, GAMEPAD_RIGHT_TRIGGER);
}

void I_ReadGyro(void)
{
    if (I_GyroEnabled() && gyro_supported)
    {
        static event_t ev = {.type = ev_gyro};
        static float data[3];

        SDL_GameControllerGetSensorData(gamepad, SDL_SENSOR_ACCEL, data, 3);
        data[0] /= SDL_STANDARD_GRAVITY;
        data[1] /= SDL_STANDARD_GRAVITY;
        data[2] /= SDL_STANDARD_GRAVITY;
        I_UpdateAccelData(data);

        SDL_GameControllerGetSensorData(gamepad, SDL_SENSOR_GYRO, data, 3);
        ev.data1.f = data[0];
        ev.data2.f = data[1];
        ev.data3.f = data[2];
        D_PostEvent(&ev);
    }
}

void I_UpdateGamepad(evtype_t type, boolean axis_buttons)
{
    static event_t ev;

    ev.type = type;

    if (I_UseStickLayout() || type == ev_joystick_state)
    {
        ev.data1.i = I_GetAxisState(SDL_CONTROLLER_AXIS_LEFTX);
        ev.data2.i = I_GetAxisState(SDL_CONTROLLER_AXIS_LEFTY);
        ev.data3.i = I_GetAxisState(SDL_CONTROLLER_AXIS_RIGHTX);
        ev.data4.i = I_GetAxisState(SDL_CONTROLLER_AXIS_RIGHTY);
        D_PostEvent(&ev);

        if (axis_buttons)
        {
            AxisToButtons(&ev);
        }
    }

    if (axis_buttons)
    {
        TriggerToButtons();
    }
}

static void UpdateTouchState(boolean menu, boolean on)
{
    static event_t ev = {.data1.i = GAMEPAD_TOUCHPAD_TOUCH};
    static int touch_time;

    if (on)
    {
        if (menu)
        {
            touch_time = I_GetTimeMS();
        }
        else
        {
            ev.type = ev_joyb_down;
            D_PostEvent(&ev);
        }
    }
    else
    {
        if (menu)
        {
            // Allow separate "touch" and "press" bindings. Touch binding is
            // registered after 100 ms to prevent accidental activation.
            if (I_GetTimeMS() - touch_time > 100)
            {

                ev.type = ev_joyb_down;
                D_PostEvent(&ev);
                ev.type = ev_joyb_up;
                D_PostEvent(&ev);
            }
        }
        else
        {
            ev.type = ev_joyb_up;
            D_PostEvent(&ev);
        }
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

    event.data1.i = button;
    D_PostEvent(&event);
}

boolean I_UseGamepad(void)
{
    return (gamepad != NULL);
}

boolean I_GyroSupported(void)
{
    return gyro_supported;
}

static joy_platform_t GetSwitchSubPlatform(void)
{
#if SDL_VERSION_ATLEAST(2, 24, 0)
    if (gamepad != NULL)
    {
        switch (SDL_GameControllerGetType(gamepad))
        {
            case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
                return PLATFORM_SWITCH_JOYCON_LEFT;

            case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
                return PLATFORM_SWITCH_JOYCON_RIGHT;

            case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
                return PLATFORM_SWITCH_JOYCON_PAIR;

            default:
                break;
        }
    }
#endif

    return PLATFORM_SWITCH_PRO;
}

void I_GetFaceButtons(int *buttons)
{
    if (platform < PLATFORM_SWITCH)
    {
        buttons[0] = GAMEPAD_Y;
        buttons[1] = GAMEPAD_A;
        buttons[2] = GAMEPAD_X;
        buttons[3] = GAMEPAD_B;
    }
    else
    {
        buttons[0] = GAMEPAD_X;
        buttons[1] = GAMEPAD_B;
        buttons[2] = GAMEPAD_Y;
        buttons[3] = GAMEPAD_A;
    }
}

static void UpdatePlatform(void)
{
    platform = joy_platform;

    if (platform == PLATFORM_AUTO)
    {
        if (gamepad != NULL)
        {
            switch ((int)SDL_GameControllerGetType(gamepad))
            {
                case SDL_CONTROLLER_TYPE_XBOXONE:
                    platform = PLATFORM_XBOXONE;
                    break;

                case SDL_CONTROLLER_TYPE_PS3:
                    platform = PLATFORM_PS3;
                    break;

                case SDL_CONTROLLER_TYPE_PS4:
                    platform = PLATFORM_PS4;
                    break;

                case SDL_CONTROLLER_TYPE_PS5:
                    platform = PLATFORM_PS5;
                    break;

                case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO:
#if SDL_VERSION_ATLEAST(2, 24, 0)
                case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
                case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
                case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
#endif
                    platform = GetSwitchSubPlatform();
                    break;
            }
        }
    }
    else if (platform == PLATFORM_SWITCH)
    {
        platform = GetSwitchSubPlatform();
    }

    M_UpdatePlatform(platform);
}

void I_FlushGamepadSensorEvents(void)
{
    SDL_PumpEvents();
    SDL_FlushEvent(SDL_CONTROLLERSENSORUPDATE);
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
    I_FlushGamepadSensorEvents();
}

void I_SetSensorEventState(boolean condition)
{
    if (I_GamepadEnabled())
    {
        SDL_EventState(SDL_CONTROLLERSENSORUPDATE,
                       condition && gyro_supported ? SDL_ENABLE : SDL_IGNORE);
        I_FlushGamepadSensorEvents();
    }
}

void I_SetSensorsEnabled(boolean condition)
{
    gyro_supported =
        (gamepad && SDL_GameControllerHasSensor(gamepad, SDL_SENSOR_ACCEL)
         && SDL_GameControllerHasSensor(gamepad, SDL_SENSOR_GYRO));

    if (condition && gyro_supported)
    {
        SDL_GameControllerSetSensorEnabled(gamepad, SDL_SENSOR_ACCEL, SDL_TRUE);
        SDL_GameControllerSetSensorEnabled(gamepad, SDL_SENSOR_GYRO, SDL_TRUE);
    }
    else if (gamepad)
    {
        SDL_GameControllerSetSensorEnabled(gamepad, SDL_SENSOR_ACCEL, SDL_FALSE);
        SDL_GameControllerSetSensorEnabled(gamepad, SDL_SENSOR_GYRO, SDL_FALSE);
    }

    I_SetSensorEventState(condition && menuactive);
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
    I_SetSensorsEnabled(I_GyroEnabled());
}

static void DisableGamepadEvents(void)
{
    SDL_EventState(SDL_JOYHATMOTION, SDL_IGNORE);
    SDL_EventState(SDL_JOYBUTTONDOWN, SDL_IGNORE);
    SDL_EventState(SDL_JOYBUTTONUP, SDL_IGNORE);
    SDL_EventState(SDL_CONTROLLERBUTTONDOWN, SDL_IGNORE);
    SDL_EventState(SDL_CONTROLLERBUTTONUP, SDL_IGNORE);
    SetTouchEventState(false);
    I_SetSensorsEnabled(false);

    // Always ignore unsupported gamepad events.
    SDL_EventState(SDL_JOYAXISMOTION, SDL_IGNORE);
    SDL_EventState(SDL_JOYBALLMOTION, SDL_IGNORE);
#if SDL_VERSION_ATLEAST(2, 24, 0)
    SDL_EventState(SDL_JOYBATTERYUPDATED, SDL_IGNORE);
#endif
    SDL_EventState(SDL_CONTROLLERAXISMOTION, SDL_IGNORE);
    SDL_EventState(SDL_CONTROLLERDEVICEREMAPPED, SDL_IGNORE);
    SDL_EventState(SDL_CONTROLLERTOUCHPADMOTION, SDL_IGNORE);
}

static void I_ShutdownGamepad(void)
{
    I_ShutdownRumble();

    if (gamepad)
    {
        SDL_GameControllerSetPlayerIndex(gamepad, -1);
    }

    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
}

static int NumJoysticks(void)
{
    const int num_joysticks = SDL_NumJoysticks();
    return MAX(0, num_joysticks);
}

void I_OpenGamepad(int device_index);

void I_InitGamepad(void)
{
    UpdatePlatform();

    if (!I_GamepadEnabled())
    {
        return;
    }

    // Enable bluetooth gyro support for DualShock 4 and DualSense.
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, "1");

    if (SDL_Init(SDL_INIT_GAMECONTROLLER) < 0)
    {
        I_Printf(VB_WARNING,
                 "I_InitGamepad: Failed to initialize gamepad: %s",
                 SDL_GetError());
        return;
    }

    DisableGamepadEvents();
    I_InitRumble();

    I_Printf(VB_INFO, "I_InitGamepad: Initialize gamepad.");

    I_AtExit(I_ShutdownGamepad, true);

    if (joy_device > 0)
    {
        const int num_joysticks = NumJoysticks();

        if (num_joysticks && SDL_IsGameController(joy_device - 1))
        {
            I_OpenGamepad(joy_device - 1);
        }
        else
        {
            joy_device = 1;

            if (num_joysticks && SDL_IsGameController(0))
            {
                I_OpenGamepad(0);
            }
            else
            {
                MN_UpdateAllGamepadItems();
            }
        }
    }
    else
    {
        MN_UpdateAllGamepadItems();
    }

    last_joy_device = joy_device;
    SDL_FlushEvents(SDL_CONTROLLERDEVICEADDED, SDL_CONTROLLERDEVICEREMOVED);
}

static boolean CheckActiveGamepad(void)
{
    if (gamepad_instance_id != -1)
    {
        const int num_joysticks = NumJoysticks();

        for (int i = 0; i < num_joysticks; i++)
        {
            if (SDL_JoystickGetDeviceInstanceID(i) == gamepad_instance_id)
            {
                joy_device = i + 1;
                last_joy_device = joy_device;
                MN_UpdateAllGamepadItems();
                return true;
            }
        }
    }

    return false;
}

static void CloseGamepad(void)
{
    if (gamepad_instance_id != -1)
    {
        I_ResetAllRumbleChannels();
        I_SetRumbleSupported(NULL);
        SDL_GameControllerSetPlayerIndex(gamepad, -1);
        SDL_GameControllerClose(gamepad);
        gamepad = NULL;
        gamepad_instance_id = -1;
        joy_device = 0;
        last_joy_device = joy_device;
        DisableGamepadEvents();
        UpdatePlatform();
        I_ResetGamepad();
    }
}

void I_OpenGamepad(int device_index)
{
    if (CheckActiveGamepad())
    {
        // Ignore when already using a gamepad.
        return;
    }

    CloseGamepad();
    gamepad = SDL_GameControllerOpen(device_index);

    if (gamepad)
    {
        I_Printf(VB_INFO, "I_OpenGamepad: Found a valid gamepad, named: %s",
                 SDL_GameControllerName(gamepad));

        gamepad_instance_id =
            SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gamepad));
        joy_device = device_index + 1;
        last_joy_device = joy_device;
        I_SetRumbleSupported(gamepad);
        I_ResetAllRumbleChannels();
        I_ResetGamepad();
        UpdatePlatform();
        EnableGamepadEvents();
        SDL_GameControllerSetPlayerIndex(gamepad, 0);

        if (gyro_supported)
        {
            I_LoadGyroCalibration();
        }
    }
    else
    {
        I_Printf(VB_ERROR, "I_OpenGamepad: Could not open gamepad %d: %s",
                 device_index, SDL_GetError());
    }

    MN_UpdateAllGamepadItems();
}

void I_CloseGamepad(SDL_JoystickID instance_id)
{
    if (gamepad_instance_id != -1)
    {
        if (instance_id == gamepad_instance_id)
        {
            CloseGamepad();

            if (NumJoysticks() && SDL_IsGameController(0))
            {
                // Fall back to another detected gamepad.
                I_OpenGamepad(0);
                return;
            }
        }
        else if (CheckActiveGamepad())
        {
            // Ignore when already using a gamepad.
            return;
        }
    }

    MN_UpdateAllGamepadItems();
}

void I_UpdateGamepadDevice(boolean gamepad_input)
{
    if (gamepad_input && joy_device == 0)
    {
        // Prevent accidentally disabling gamepad when it's being used.
        joy_device = last_joy_device;
        return;
    }

    last_joy_device = joy_device;
    const int device_index = joy_device - 1;
    CloseGamepad();

    if (device_index >= 0)
    {
        I_OpenGamepad(device_index);
    }
    else
    {
        MN_UpdateAllGamepadItems();
    }
}

const char **I_GamepadDeviceList(void)
{
    if (!I_GamepadEnabled())
    {
        return NULL;
    }

    array_free(gamepad_strings);
    array_push(gamepad_strings, "None");

    const int num_joysticks = NumJoysticks();
    int num_gamepads = 0;

    for (int i = 0; i < num_joysticks; i++)
    {
        if (SDL_IsGameController(i))
        {
            char *name = M_StringDuplicate(SDL_GameControllerNameForIndex(i));
            array_push(gamepad_strings, name);
            num_gamepads++;
        }
    }

    if (joy_device > num_gamepads)
    {
        for (int i = num_gamepads; i < joy_device; i++)
        {
            array_push(gamepad_strings, "None");
        }
    }

    return gamepad_strings;
}

boolean I_GamepadDevices(void)
{
    return (array_size(gamepad_strings) > 1
            && strncmp(gamepad_strings[1], "None", strlen(gamepad_strings[1])));
}

static uint64_t GetSensorTimeUS(const SDL_ControllerSensorEvent *csensor)
{
#if SDL_VERSION_ATLEAST(2, 26, 0)
    if (csensor->timestamp_us)
    {
        return csensor->timestamp_us;
    }
    else
#endif
    {
        return (uint64_t)csensor->timestamp * 1000;
    }
}

static float GetDeltaTime(const SDL_ControllerSensorEvent *csensor,
                          uint64_t *last_time)
{
    const uint64_t sens_time = GetSensorTimeUS(csensor);
    const float dt = (sens_time - *last_time) * 1.0e-6f;
    *last_time = sens_time;
    return BETWEEN(0.0f, 0.05f, dt);
}

static void UpdateGyroState(const SDL_ControllerSensorEvent *csensor)
{
    static event_t ev = {.type = ev_gyro};
    static uint64_t last_time;

    ev.data1.f = csensor->data[0];
    ev.data2.f = csensor->data[1];
    ev.data3.f = csensor->data[2];
    ev.data4.f = GetDeltaTime(csensor, &last_time);

    D_PostEvent(&ev);
}

static void UpdateAccelState(const SDL_ControllerSensorEvent *csensor)
{
    static float data[3];
    data[0] = csensor->data[0] / SDL_STANDARD_GRAVITY;
    data[1] = csensor->data[1] / SDL_STANDARD_GRAVITY;
    data[2] = csensor->data[2] / SDL_STANDARD_GRAVITY;

    I_UpdateAccelData(data);
}

void I_HandleSensorEvent(SDL_Event *sdlevent)
{
    switch (sdlevent->csensor.sensor)
    {
        case SDL_SENSOR_ACCEL:
            UpdateAccelState(&sdlevent->csensor);
            break;

        case SDL_SENSOR_GYRO:
            UpdateGyroState(&sdlevent->csensor);
            break;

        default:
            break;
    }
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

// If true, I_StartTextInput() has been called, and we are populating
// the data3 field of ev_keydown events.

static boolean text_input_enabled;

// key tables
// jff 5/10/98 french support removed, 
// as it was not being used and couldn't be easily tested
//

static const char shiftxform[] =
{
    0,
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
    31,
    ' ', '!', '"', '#', '$', '%', '&',
    '"', // shift-'
    '(', ')', '*', '+',
    '<', // shift-,
    '_', // shift--
    '>', // shift-.
    '?', // shift-/
    ')', // shift-0
    '!', // shift-1
    '@', // shift-2
    '#', // shift-3
    '$', // shift-4
    '%', // shift-5
    '^', // shift-6
    '&', // shift-7
    '*', // shift-8
    '(', // shift-9
    ':',
    ':', // shift-;
    '<',
    '+', // shift-=
    '>', '?', '@',
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
    'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    '[', // shift-[
    '!', // shift-backslash - OH MY GOD DOES WATCOM SUCK
    ']', // shift-]
    '"', '_',
    '\'', // shift-`
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
    'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    '{', '|', '}', '~', 127
};

static boolean vanilla_keyboard_mapping;

static int TranslateKey(SDL_Keysym *sym)
{
    int scancode = sym->scancode;

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


// Get the localized version of the key press. This takes into account the
// keyboard layout, but does not apply any changes due to modifiers, (eg.
// shift-, alt-, etc.)

static int GetLocalizedKey(SDL_Keysym *sym)
{
    // When using Vanilla mapping, we just base everything off the scancode
    // and always pretend the user is using a US layout keyboard.
    if (vanilla_keyboard_mapping)
    {
        return TranslateKey(sym);
    }
    else
    {
        int result = sym->sym;

        if (result < 0 || result >= 128)
        {
            result = 0;
        }

        return result;
    }
}

static int GetTypedChar(SDL_Keysym *sym)
{
    int result = TranslateKey(sym);

    // If shift is held down, apply the original uppercase
    // translation table used under DOS.
    if ((SDL_GetModState() & KMOD_SHIFT) != 0 && result >= 0
        && result < arrlen(shiftxform))
    {
        result = shiftxform[result];
    }

    return result;
}

void I_StartTextInput(void)
{
    text_input_enabled = true;

    if (!vanilla_keyboard_mapping)
    {
        SDL_StartTextInput();
    }
}

void I_StopTextInput(void)
{
    text_input_enabled = false;

    if (!vanilla_keyboard_mapping)
    {
        SDL_StopTextInput();
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

    event.data1.i = button;
    event.data2.i = dclick;
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
    down.data1.i = button;
    D_PostEvent(&down);

    // hold button for one tic, required for checks in G_BuildTiccmd
    delay_event.type = ev_mouseb_up;
    delay_event.data1.i = button;
}

void I_DelayEvent(void)
{
    if (delay_event.data1.i)
    {
        D_PostEvent(&delay_event);
        delay_event.data1.i = 0;
    }
}

//
// Read the change in mouse state to generate mouse motion events
//

void I_ReadMouse(void)
{
    static event_t ev = {.type = ev_mouse};

    SDL_GetRelativeMouseState(&ev.data1.i, &ev.data2.i);

    if (ev.data1.i || ev.data2.i)
    {
        D_PostEvent(&ev);
        ev.data1.i = ev.data2.i = 0;
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
            event.data1.i = TranslateKey(&sdlevent->key.keysym);
            event.data2.i = GetLocalizedKey(&sdlevent->key.keysym);

            if (event.data1.i != 0)
            {
                D_PostEvent(&event);
            }

            if (text_input_enabled && vanilla_keyboard_mapping)
            {
                event.type = ev_text;
                event.data1.i = GetTypedChar(&sdlevent->key.keysym);

                if (event.data1.i != 0)
                {
                    D_PostEvent(&event);
                }
            }
            break;

        case SDL_KEYUP:
            event.type = ev_keyup;
            event.data1.i = TranslateKey(&sdlevent->key.keysym);

            // data2 is initialized to zero for ev_keyup. For ev_keydown it's
            // the shifted Unicode character that was typed, but if something
            // wants to detect key releases it should do so based on data1
            // (key ID), not the printable char.

            event.data2.i = 0;

            if (event.data1.i != 0)
            {
                D_PostEvent(&event);
            }
            break;

        case SDL_TEXTINPUT:
            // SDL_TEXTINPUT are fired only after SDL_StartTextInput().
            if (strlen(sdlevent->text.text) == 1
                && (sdlevent->text.text[0] & 0x80) == 0)
            {
                event.type = ev_text;
                event.data1.i = sdlevent->text.text[0];
                D_PostEvent(&event);
            }
            break;

        default:
            break;
    }
}

void I_InitKeyboard(void)
{
    // On desktop platforms, SDL_StartTextInput() is implicitly called on SDL
    // window creation
    SDL_StopTextInput();
}

void I_BindKeyboardVariables(void)
{
    BIND_BOOL(vanilla_keyboard_mapping, false, "Vanilla keyboard mapping");
}

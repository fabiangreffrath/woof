//
//  Copyright (C) 2026 Roman Fomin
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
// DESCRIPTION:
//  Configuration-backed items for the new options menu.
//

#ifndef MN_OPTIONS_H
#define MN_OPTIONS_H

#include "mn_container.h"

// The setup_group enum is used to show which 'groups' keys fall into so
// that you can bind a key differently in each 'group'.

typedef enum
{
    m_null, // Has no meaning; not applicable
    m_scrn, // A key can not be assigned to more than one action
    m_map,  // in the same group. A key can be assigned to one
    m_gyro, // action in one group, and another action in another.
} setup_group;

extern mn_menu_t video_menu;
extern mn_menu_t display_menu;
extern mn_menu_t audio_menu;
extern mn_menu_t equalizer_menu;
extern mn_menu_t controls_menu;
extern mn_menu_t advanced_gamepad_menu;
extern mn_menu_t advanced_gyro_menu;
extern mn_menu_t input_bindings_menu;
extern mn_menu_t interface_menu;
extern mn_menu_t gameplay_menu;
extern mn_menu_t options_menu;

void MN_BindMenu(mn_menu_t *menu);

void MN_OptionsOpen(void);

void MN_GetResolutionScaleStrings(mn_item_t *item);
void MN_OptionsUpdateResolutionScale(void);
void MN_OptionsUpdateFullscreen(void);
void MN_OptionsUpdateFPSLimit(void);
void MN_OptionsUpdateFPSLimitItem(void);
void MN_OptionsUpdateFOV(void);
void MN_OptionsResetVideo(void);

void MN_OptionsUpdateSfxVolume(void);
void MN_OptionsUpdateMusicVolume(void);
void MN_OptionsSetSoundModule(void);
void MN_OptionsRestartMusic(void);
void MN_OptionsEqualizerPreset(void);
void MN_GetMidiPlayerStrings(mn_item_t *item);
void MN_OptionsSetMidiPlayer(void);
void MN_GetResamplerStrings(mn_item_t *item);
void MN_OptionsResetAutoGain(void);
void MN_OptionsUpdateAdvancedSoundItems(boolean toggle);

void MN_OptionsInitFreeLook(void);
void MN_OptionsUpdateFreeLook(void);
void MN_OptionsUpdateFreeLookMode(void);
void MN_GetGamepadDeviceStrings(mn_item_t *item);
void MN_OptionsUpdateGamepadDevice(void);
void MN_OptionsUpdateGamepad(void);
void MN_GetMsTimeStrings(mn_item_t *item);
void MN_OptionsUpdateRumble(void);
void MN_GetMovementSensitivityStrings(mn_item_t *item);
void MN_OptionsUpdateGyroAiming(void);
void MN_GetGyroSensitivityStrings(mn_item_t *item);
void MN_GetGyroAccelStrings(mn_item_t *item);
void MN_OptionsUpdateGyroSteadying(void);
void MN_OptionsUpdateGyroAcceleration(void);
void MN_OptionsUpdateAllGamepadItems(void);

#include "ws_stuff.h"
#define WS_BUF_SIZE 80
extern char slot_labels[NUM_WS_SLOTS * NUM_WS_WEAPS][WS_BUF_SIZE];
void MN_OptionsUpdateWeaponSlotLabels(void);
void MN_OptionsUpdateWeaponSlotActivation(void);
void MN_OptionsUpdateWeaponSlotSelection(void);
void MN_OptionsUpdateWeaponSlots(void);
void MN_GetWeaponSlotStrings(mn_item_t *item);

void MN_GetScreenSizeStrings(mn_item_t *item);
void MN_OptionsViewSize(void);
void MN_OptionsUpdateStatsFormatItem(void);
void MN_OptionsUpdateCrosshairItems(void);

void MN_OptionsUpdateCenteredWeaponItem(void);

void MN_BindOptionsVariables(void);

#endif

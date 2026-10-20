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

extern mn_menu_t options_menu;

void MN_OptionsBind(void);
void MN_OptionsRefresh(void);
void MN_OptionsOpen(void);
void MN_OptionsOpenClassic(void);
void MN_OptionsResetPage(void);

void MN_OptionsUpdateSfxVolume(void);
void MN_OptionsUpdateMusicVolume(void);
void MN_OptionsUpdateFullscreen(void);
void MN_OptionsUpdateVSync(void);
void MN_OptionsUpdateFPSLimit(void);
void MN_OptionsUpdateFOV(void);
void MN_OptionsResetVideo(void);
void MN_OptionsResetPalette(void);
void MN_OptionsSetSoundModule(void);
void MN_OptionsRestartMusic(void);
void MN_OptionsEqualizerPreset(void);

boolean MN_OptionEnabled(const mn_item_t *item);
int MN_OptionValue(const mn_item_t *item);
void MN_OptionSetValue(mn_item_t *item, int value);
void MN_OptionSliderSetFromMouse(mn_item_t *item, int mouse_x);

#endif

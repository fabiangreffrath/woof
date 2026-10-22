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

extern mn_menu_t display_menu;
extern mn_menu_t video_menu;
extern mn_menu_t audio_menu;
extern mn_menu_t equalizer_menu;
extern mn_menu_t options_root_menu;

void MN_OptionsBind(void);
void MN_OptionsRefresh(void);
void MN_OptionsOpen(void);

void MN_GetResolutionScaleStrings(mn_item_t *item);
void MN_OptionsUpdateResolutionScale(void);

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

void MN_GetMidiPlayerStrings(mn_item_t *item);
void MN_OptionsSetMidiPlayer(void);

void MN_GetResamplerStrings(mn_item_t *item);

#endif

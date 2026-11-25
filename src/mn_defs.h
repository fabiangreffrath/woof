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

#ifndef MN_DEFS_H
#define MN_DEFS_H

typedef enum
{
    null_mode,
    mouse_mode,
    pad_mode,
    key_mode
} menu_input_mode_t;

extern menu_input_mode_t help_input, old_help_input; // pad_mode or key_mode.
extern menu_input_mode_t menu_input, old_menu_input;

extern int maxscreenblocks;

extern short whichSkull; // which skull to draw (he blinks)

extern int warning_about_changes, print_warning_about_changes;

extern const char *gamma_strings[];

enum
{
    MN_FNT_H = 7,
    MN_PAD = 2,
    MN_SPC = 9,
    MN_SPC_ITEM =   (MN_SPC + 4),
    MN_SPC_SLIDER = (MN_SPC + 4),
    MN_SPC_CHOICE = (MN_SPC + 4),

    MN_CHOICE_SIZE = 9,
    MN_SLIDER_SIZE = 8,
};

#define MN_LAYOUT                   \
    {.max_width = MN_PX(500),       \
     .items_min_width = MN_PX(246), \
     .value_column = MN_PX(147),    \
     .item_spacing = 1}

#define MN_GAP(gap)   {.type = ItemType_Skip, .height = (gap)}
#define MN_TITLE(txt) {.label = (txt), .type = ItemType_Title}

#define MN_TAB(tab_, items_) \
    {.tab = (tab_), .items = (items_), .num_items = arrlen(items_)}

static const char *offon_strings[] = {"Off", "On"};

#define MN_TOGGLE(lbl, key, desc)          \
    {.label = (lbl),                       \
     .cfg = (key),                         \
     .type = ItemType_Choice,              \
     .choices = offon_strings,             \
     .num_choices = arrlen(offon_strings), \
     .choice_size = MN_CHOICE_SIZE,        \
     .description = (desc)}

#define MN_TEXT(lbl, key, size, desc) \
    {.label = (lbl),                  \
     .cfg = (key),                    \
     .type = ItemType_Text,           \
     .choice_size = (size),           \
     .description = (desc)}

#define MN_CHOICE(lbl, key, strs, desc) \
    {.label = (lbl),                    \
     .cfg = (key),                      \
     .type = ItemType_Choice,           \
     .choices = (strs),                 \
     .num_choices = arrlen(strs),       \
     .choice_size = MN_CHOICE_SIZE,     \
     .description = (desc)}

#define MN_CHOICE_ACT(lbl, key, strs, act, desc) \
    {.label = (lbl),                             \
     .cfg = (key),                               \
     .type = ItemType_Choice,                    \
     .choices = (strs),                          \
     .num_choices = arrlen(strs),                \
     .choice_size = MN_CHOICE_SIZE,              \
     .action = (act),                            \
     .description = (desc)}

void M_ChooseSkill(int choice);

#endif

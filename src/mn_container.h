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

#ifndef MN_CONTAINER_H
#define MN_CONTAINER_H

#include "doomtype.h"
#include "mn_internal.h"

#define MN_SPC        9
#define MN_SPC_ITEM   (MN_SPC + 3)
#define MN_SPC_SLIDER (MN_SPC + 5)
#define MN_SPC_CHOICE (MN_SPC + 3)
#define MN_MAX_TABS   16

enum
{
    MN_MENU_CENTER = (1u << 0),
    MN_MENU_HINTS = (1u << 1),

    // Centers the whole layout box.
    MN_MENU_CENTER_LAYOUT = (1u << 2),
};

typedef enum
{
    MN_ITEM_TITLE,
    MN_ITEM_TOGGLE,
    MN_ITEM_NUMBER,
    MN_ITEM_CHOICE,
    MN_ITEM_SLIDER,
    MN_ITEM_INPUT,
    MN_ITEM_WEAPON,
    MN_ITEM_ACTION,
    MN_ITEM_RESET,
    MN_ITEM_SUBMENU,
    MN_ITEM_SKIP,
    MN_ITEM_SAVESLOT,
    MN_ITEM_END
} mn_item_type_t;

enum
{
    MN_ITEM_DISABLED = (1u << 0),
    MN_ITEM_WARN_LEVEL = (1u << 1),
    MN_ITEM_WARN_PROGRAM = (1u << 2),
    MN_ITEM_STRICT = (1u << 3),
    MN_ITEM_BOOM = (1u << 4),
    MN_ITEM_MBF = (1u << 5),
    MN_ITEM_VANILLA = (1u << 6),
    MN_ITEM_CENTER = (1u << 8),
    MN_ITEM_KEEP= (1u << 9),
    MN_ITEM_DEFER_ACTION = (1u << 10),
    MN_ITEM_TWO_LINES = (1u << 11),
    MN_ITEM_DIRECT_COORDS = (1u << 12),
    MN_ITEM_DRAW_CROSSHAIR = (1u << 13),
    MN_ITEM_HIDE_VALUE = (1u << 14),
};

typedef struct mn_item_ops_s mn_item_ops_t;
typedef struct mn_item_s mn_item_t;
typedef struct mn_menu_s mn_menu_t;
typedef struct mn_page_s mn_page_t;

typedef struct
{
    int x;
    int y;
    int w;
    int h;
    int value_x;
    int value_w;
    int mouse_x;
    int color;
    boolean selected;
    boolean active;
    boolean disabled;
    boolean mouse_inside;
} mn_draw_ctx_t;

struct mn_item_s
{
    // Declarative part.
    const char *label;
    const char *cfg;
    mn_item_type_t type;
    const char **choices;
    int num_choices;
    int slider_size;
    int choice_width;
    uint32_t flags;
    const char *desc;
    const char *suffix;
    void (*action)(void);
    mn_menu_t *submenu;
    int input_id;
    int group;

    // Runtime part.
    void (*getchoices)(mn_item_t *item);
    struct default_s *def;
    const mn_item_ops_t *ops;
    mrect_t rect;
    int height;

    boolean gathering;
    int gather_count;
    char gather[8];

    boolean disabled;
    boolean action_pending;

    int draw_value_x;
    int draw_value_w;
};

struct mn_item_ops_s
{
    void (*draw)(const mn_item_t *item, const mn_draw_ctx_t *ctx);
    boolean (*respond)(mn_item_t *item, menu_action_t action, int ch);
    void (*drag)(mn_item_t *item, int mouse_x);
    boolean (*click)(mn_item_t *item, int mouse_x, int mouse_y);
    void (*hover)(mn_item_t *item, int mouse_x, int mouse_y);
};

struct mn_page_s
{
    const char *tab;
    mn_item_t *items;
    int num_items;
    int selected;
    int scroll;
    int scroll_target;
    int content_h;
    int label_width;

    // Empty space kept above the first row and below the last one, for
    // item graphics that overhang their rect and would otherwise be clipped
    // when the list is scrolled to either end.
    int pad_top;
    int pad_bottom;

    // Draws page content that is not bound to a single item, e.g. a preview
    // panel next to the list.
    void (*draw)(const mn_menu_t *menu, const mn_page_t *page);
};

typedef enum
{
    MN_UNIT_AUTO,
    MN_UNIT_PX,
    MN_UNIT_PERCENT
} mn_unit_t;

typedef struct
{
    int value;
    mn_unit_t unit;
} mn_length_t;

#define MN_PX(v)  {.value = (v), .unit = MN_UNIT_PX}
#define MN_PCT(v) {.value = (v), .unit = MN_UNIT_PERCENT}

// Margins and the layout width resolve percentages against the full logical
// screen width, hint sizes resolve them against the resulting content box.
// MN_UNIT_AUTO fields keep the original hard-coded geometry.
typedef struct
{
    mn_length_t margin_left;
    mn_length_t margin_right;
    mn_length_t min_width;
    mn_length_t max_width;
    mn_length_t hint_width;
    mn_length_t hint_min_width;
    mn_length_t hint_max_width;
    int hint_gap;
} mn_layout_t;

struct mn_menu_s
{
    const char *title_patch;
    const char *title_text;
    const char *background;
    uint32_t flags;
    int item_spacing;
    mn_layout_t layout;
    boolean init;

    mn_page_t *pages;
    int num_pages;
    int page;

    int hover;
    int help_hover;
    mn_item_t *active_item;
    mn_item_t *drag_item;

    mrect_t tabbar;
    mrect_t viewport;
    mrect_t hint_rect;
    mrect_t tab_rects[MN_MAX_TABS];

    int highlight_tab;

    mrect_t scroll_track;
    mrect_t scroll_thumb;
    boolean dragging_scroll;
    int drag_offset;

    boolean confirm;
    const char *confirm_text;
    void (*confirm_action)(void);
};

#define MN_END        {.type = MN_ITEM_END}
#define MN_GAP(gap)   {.type = MN_ITEM_SKIP, .height = (gap)}
#define MN_TITLE(txt) {.label = (txt), .type = MN_ITEM_TITLE}

#define MN_TOGGLE(lbl, key) \
    {.label = (lbl), .cfg = (key), .type = MN_ITEM_TOGGLE}

#define MN_CHOICE(lbl, key, strs, width) \
    {.label = (lbl),                     \
     .cfg = (key),                       \
     .type = MN_ITEM_CHOICE,             \
     .choices = (strs),                  \
     .num_choices = arrlen(strs),        \
     .choice_width = (width)}
#define MN_CHOICE_ACT(lbl, key, strs, width, act) \
    {.label = (lbl),                              \
     .cfg = (key),                                \
     .type = MN_ITEM_CHOICE,                      \
     .choices = (strs),                           \
     .num_choices = arrlen(strs),                 \
     .choice_width = (width),                     \
     .action = (act)}

typedef enum
{
    MN_CONTAINER_HANDLED,
    MN_CONTAINER_CLOSE,
    MN_CONTAINER_ESCAPE
} mn_container_result_t;

void MN_ContainerOpen(mn_menu_t *menu);
void MN_ContainerClose(void);
boolean MN_ContainerActive(void);
mn_page_t *MN_ContainerCurrentPage(void);

mn_container_result_t MN_ContainerResponder(menu_action_t action, int ch);
void MN_ContainerMouseMove(int x, int y);
mn_container_result_t MN_ContainerMouseButton(int button, boolean down);
mn_container_result_t MN_ContainerWheel(int delta);

void MN_ContainerTicker(void);
void MN_ContainerDrawer(void);
void MN_ContainerRelayout(void);

void MN_ContainerConfirm(const char *text, void (*action)(void));
void MN_ContainerEnsureVisible(mn_item_t *item);
void MN_ContainerRefresh(void);
void MN_ContainerEndEdit(void);

// The index of the item the help text and the preview refer to: the
// mouse-hovered one in mouse mode, otherwise the keyboard-selected one.
int MN_ContainerHighlightedIndex(const mn_menu_t *menu);

// Helpers shared by item implementations.
boolean MN_ContainerPointInside(const mrect_t *rect, int x, int y);
void MN_ContainerItemChanged(mn_item_t *item);

// Binds the input that triggered the current event to the item's action.
boolean MN_ContainerBindInput(mn_item_t *item);

// Text layout helpers shared by the hints panel and multi-line items.
// Wraps a mutable string to the given width, inserting newlines, and
// returns the number of lines. Truncates a single line to the width.
int MN_WrapText(char *buffer, int width);
void MN_TruncateText(char *buffer, int width);

void MN_DisableItemInternal(boolean condition, mn_menu_t *menu,
                            const char *names[], int size);
#define MN_DisableItem(condition, menu, ...)                                   \
    MN_DisableItemInternal((condition), (menu), (const char *[]){__VA_ARGS__}, \
                         sizeof((const char *[]){__VA_ARGS__})               \
                             / sizeof(const char *))

void MN_BindMenuVariables(void);

#endif

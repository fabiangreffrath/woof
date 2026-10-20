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
//  Small tabbed/page container with smooth scrolling, used by the new
//  options menu.
//

#ifndef MN_CONTAINER_H
#define MN_CONTAINER_H

#include "doomtype.h"
#include "mn_internal.h"

#define MN_SPC      9
#define MN_MAX_TABS 16

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
    MN_ITEM_COSMETIC = (1u << 7),
    MN_ITEM_CENTER = (1u << 8),
    MN_ITEM_LEFTJUST = (1u << 9),
    MN_ITEM_KEEP = (1u << 10),
    MN_ITEM_WRAP = (1u << 11),
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
    const char *const *choices;
    int num_choices;
    int slider_size;
    int choice_width;
    unsigned flags;
    const char *desc;
    const char *suffix;
    void (*action)(void);
    int input_id;
    int group;

    // Runtime part.
    struct default_s *def;
    const mn_item_ops_t *ops;
    mrect_t rect;
    int height;

    boolean gathering;
    int gather_count;
    char gather[8];

    boolean disabled;

    int draw_value_x;
    int draw_value_w;

    int hover_segment;
    int hover_arrow;
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
    void (*refresh)(void);
};

struct mn_menu_s
{
    const char *title_patch;
    const char *title_text;
    const char *background;

    mn_page_t *pages;
    int num_pages;
    int page;

    int hover;
    mn_item_t *active_item;
    mn_item_t *drag_item;

    mrect_t tabbar;
    mrect_t viewport;
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

typedef enum
{
    MN_CONTAINER_HANDLED,
    MN_CONTAINER_CLOSE,
    MN_CONTAINER_ESCAPE
} mn_container_result_t;

void MN_ContainerOpen(mn_menu_t *menu);
void MN_ContainerClose(void);
boolean MN_ContainerActive(void);
mn_menu_t *MN_ContainerCurrent(void);
mn_page_t *MN_ContainerCurrentPage(void);
mn_item_t *MN_ContainerSelected(void);

mn_container_result_t MN_ContainerResponder(menu_action_t action, int ch);
boolean MN_ContainerMouseMove(int x, int y);
boolean MN_ContainerMouseButton(int button, boolean down);
boolean MN_ContainerWheel(int delta);

void MN_ContainerTicker(void);
void MN_ContainerDrawer(void);
void MN_ContainerRelayout(void);

void MN_ContainerConfirm(const char *text, void (*action)(void));
void MN_ContainerEnsureVisible(mn_item_t *item);
void MN_ContainerRefresh(void);
void MN_ContainerEndEdit(void);

// Helpers shared by item implementations.
boolean MN_ContainerPointInside(const mrect_t *rect, int x, int y);
void MN_ContainerItemChanged(mn_item_t *item);

#endif

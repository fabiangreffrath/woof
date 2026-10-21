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
//  Small tabbed/page container with smooth scrolling.
//

#include <stdlib.h>
#include <string.h>

#include "doomdef.h"
#include "doomkeys.h"
#include "m_misc.h"
#include "mn_container.h"
#include "mn_internal.h"
#include "mn_menu.h"
#include "sounds.h"
#include "v_palette.h"
#include "v_patch.h"
#include "v_trans.h"
#include "v_video.h"

static mn_menu_t *active_menu;
static int last_mouse_x;
static int last_mouse_y;

#define MN_MAX_DEPTH 8

static mn_menu_t *menu_stack[MN_MAX_DEPTH];
static int menu_depth;

static byte MenuColor(int cr, boolean shaded)
{
    const byte index =
        shaded ? cr_shaded[playpal_global->white] : playpal_global->white;

    return xlat[cr].table[index];
}

boolean MN_ContainerPointInside(const mrect_t *rect, int x, int y)
{
    return x > rect->x + video.deltaw && x < rect->x + rect->w + video.deltaw
           && y > rect->y && y < rect->y + rect->h;
}

static boolean IsNavigable(const mn_item_t *item)
{
    return item->type != MN_ITEM_END && item->type != MN_ITEM_TITLE;
}

static boolean IsSelectable(const mn_item_t *item)
{
    return IsNavigable(item) && !item->disabled;
}

static int FirstNavigable(const mn_page_t *page)
{
    for (int i = 0; i < page->num_items; ++i)
    {
        if (IsNavigable(&page->items[i]))
        {
            return i;
        }
    }

    return 0;
}

static void EnsureValidSelection(mn_menu_t *menu, int page_index)
{
    mn_page_t *page = &menu->pages[page_index];

    if (page->selected < 0 || page->selected >= page->num_items
        || !IsNavigable(&page->items[page->selected]))
    {
        page->selected = FirstNavigable(page);
    }
}

static int MaxScroll(const mn_menu_t *menu, const mn_page_t *page)
{
    return MAX(0, page->content_h - menu->viewport.h);
}

static boolean HasTabs(const mn_menu_t *menu)
{
    return menu->num_pages > 1 || (menu->num_pages == 1 && menu->pages[0].tab);
}

static void ClampScroll(const mn_menu_t *menu, mn_page_t *page)
{
    page->scroll_target = CLAMP(page->scroll_target, 0, MaxScroll(menu, page));
    page->scroll = CLAMP(page->scroll, 0, MaxScroll(menu, page));
}

static void LayoutPage(mn_menu_t *menu, mn_page_t *page)
{
    int y = 0;
    int count = 0;
    int label_width = 0;

    for (mn_item_t *item = page->items; item->type != MN_ITEM_END; ++item)
    {
        int height;
        if (item->height)
        {
            height = item->height;
        }
        else if (item->type == MN_ITEM_SLIDER || item->type == MN_ITEM_CHOICE)
        {
            height = MN_SPC + 5;
        }
        else
        {
            height = MN_SPC;
        }

        item->rect.x = 0;
        item->rect.y = y;
        item->rect.w = menu->viewport.w;
        item->rect.h = height;

        if (item->type != MN_ITEM_TITLE)
        {
            label_width = MAX(label_width, MN_GetPixelWidth(item->label));
        }

        y += height;

        if ((item + 1)->type != MN_ITEM_END)
        {
            y += menu->item_spacing;
        }

        count++;
    }

    page->num_items = count;
    page->content_h = y;
    page->label_width = label_width;

    if ((menu->flags & MN_MENU_CENTER) && y < menu->viewport.h)
    {
        const int offset = (menu->viewport.h - y) / 2;

        for (int i = 0; i < count; ++i)
        {
            page->items[i].rect.y += offset;
        }
    }

    ClampScroll(menu, page);
}

static void LayoutTabs(mn_menu_t *menu)
{
    int total = 0;

    if (!HasTabs(menu))
    {
        menu->tabbar = (mrect_t){0};

        for (int i = 0; i < MN_MAX_TABS; ++i)
        {
            menu->tab_rects[i] = (mrect_t){0};
        }

        return;
    }

    for (int i = 0; i < menu->num_pages && i < MN_MAX_TABS; ++i)
    {
        const int width = MN_GetPixelWidth(menu->pages[i].tab);

        if (i)
        {
            total += 8;
        }

        total += width;
    }

    int x = menu->viewport.x + (menu->viewport.w - total) / 2;

    for (int i = 0; i < menu->num_pages && i < MN_MAX_TABS; ++i)
    {
        const int width = MN_GetPixelWidth(menu->pages[i].tab);

        if (i)
        {
            x += 8;
        }

        menu->tab_rects[i] = (mrect_t){x, 22, width, MN_SPC};
        x += width;
    }

    menu->tabbar = (mrect_t){menu->viewport.x, 22, menu->viewport.w, MN_SPC};
}

static void LayoutMenu(mn_menu_t *menu)
{
    const int top = HasTabs(menu) ? 40 : 30;
    const int height = SCREENHEIGHT - top - 20;

    // Coordinates are logical: the whole widescreen width is available,
    // with the non-widescreen part centered by video.deltaw.
    const int left = 24 - video.deltaw;
    int width = (video.unscaledw > 0 ? video.unscaledw : SCREENWIDTH) - 48;

    menu->hint_rect = (mrect_t){0};

    if ((menu->flags & MN_MENU_HINTS) && video.deltaw > 0)
    {
        const int gap = 16;
        const int hint_w = MAX(96, width / 3);

        menu->hint_rect =
            (mrect_t){left + width - hint_w, top, hint_w, height};
        width -= hint_w + gap;
    }

    menu->viewport = (mrect_t){left, top, width, height};

    LayoutTabs(menu);

    for (int i = 0; i < menu->num_pages; ++i)
    {
        if (menu->pages[i].refresh)
        {
            menu->pages[i].refresh();
        }

        LayoutPage(menu, &menu->pages[i]);
    }
}

void MN_ContainerRelayout(void)
{
    if (!active_menu)
    {
        return;
    }

    LayoutMenu(active_menu);

    mn_page_t *page = &active_menu->pages[active_menu->page];
    EnsureValidSelection(active_menu, active_menu->page);
    MN_ContainerEnsureVisible(&page->items[page->selected]);
}

void MN_ContainerEnsureVisible(mn_item_t *item)
{
    if (!active_menu || !item)
    {
        return;
    }

    mn_page_t *page = &active_menu->pages[active_menu->page];

    int item_index = -1;
    for (int i = 0; i < page->num_items; ++i)
    {
        if (&page->items[i] == item)
        {
            item_index = i;
            break;
        }
    }

    int target_y = item->rect.y;
    int target_h = item->rect.h;

    if (item_index > 0 && !IsNavigable(&page->items[item_index - 1]))
    {
        int first_visible = item_index - 1;
        while (first_visible > 0 && !IsNavigable(&page->items[first_visible - 1]))
        {
            first_visible--;
        }
        target_y = page->items[first_visible].rect.y;
        target_h = item->rect.y + item->rect.h - target_y;
    }

    if (target_y < page->scroll_target)
    {
        page->scroll_target = target_y;
    }
    else if (target_y + target_h
             > page->scroll_target + active_menu->viewport.h)
    {
        page->scroll_target =
            target_y + target_h - active_menu->viewport.h;
    }

    ClampScroll(active_menu, page);
}

static void SelectFirst(mn_menu_t *menu)
{
    mn_page_t *page = &menu->pages[menu->page];

    page->selected = FirstNavigable(page);
    MN_ContainerEnsureVisible(&page->items[page->selected]);
}

static void MoveSelection(mn_menu_t *menu, int direction)
{
    mn_page_t *page = &menu->pages[menu->page];

    if (page->num_items <= 0)
    {
        return;
    }

    int index = page->selected;

    for (int i = 0; i < page->num_items; ++i)
    {
        index = (index + direction + page->num_items) % page->num_items;

        if (IsNavigable(&page->items[index]))
        {
            page->selected = index;
            M_StartSound(sfx_mnumov);
            MN_ContainerEnsureVisible(&page->items[index]);
            return;
        }
    }
}

static void CancelActiveItem(mn_menu_t *menu)
{
    if (menu->active_item)
    {
        if (menu->active_item->ops && menu->active_item->ops->respond)
        {
            menu->active_item->ops->respond(menu->active_item, MENU_ESCAPE, 0);
        }
        menu->active_item = NULL;
    }
}

static void ResetMenuState(mn_menu_t *menu)
{
    menu->page = 0;
    menu->hover = -1;
    menu->active_item = NULL;
    menu->drag_item = NULL;
    menu->dragging_scroll = false;
    menu->confirm = false;
    menu->highlight_tab = -1;

    for (int i = 0; i < menu->num_pages; ++i)
    {
        mn_page_t *page = &menu->pages[i];

        page->selected = 0;
        page->scroll = 0;
        page->scroll_target = 0;
    }
}

static void EnterMenu(mn_menu_t *menu)
{
    active_menu = menu;
    ResetMenuState(menu);
    LayoutMenu(menu);
    SelectFirst(menu);
}

static void PushMenu(mn_menu_t *menu)
{
    if (!active_menu || !menu || menu_depth >= MN_MAX_DEPTH)
    {
        return;
    }

    CancelActiveItem(active_menu);
    menu_stack[menu_depth++] = active_menu;
    EnterMenu(menu);
}

static boolean PopMenu(void)
{
    if (menu_depth <= 0)
    {
        return false;
    }

    active_menu = menu_stack[--menu_depth];
    active_menu->hover = -1;
    active_menu->active_item = NULL;
    active_menu->drag_item = NULL;
    active_menu->dragging_scroll = false;
    active_menu->confirm = false;
    active_menu->highlight_tab = -1;

    LayoutMenu(active_menu);
    EnsureValidSelection(active_menu, active_menu->page);

    mn_page_t *page = &active_menu->pages[active_menu->page];
    MN_ContainerEnsureVisible(&page->items[page->selected]);

    return true;
}

static boolean NextPage(mn_menu_t *menu, int direction)
{
    const int page = menu->page + direction;

    if (page < 0 || page >= menu->num_pages)
    {
        return false;
    }

    CancelActiveItem(menu);

    menu->page = page;
    menu->hover = -1;
    EnsureValidSelection(menu, page);
    MN_ContainerEnsureVisible(
        &menu->pages[page].items[menu->pages[page].selected]);
    M_StartSound(sfx_mnumov);
    return true;
}

static void ActivateItem(mn_menu_t *menu, int index, boolean mouse)
{
    mn_page_t *page = &menu->pages[menu->page];

    if (index < 0 || index >= page->num_items)
    {
        return;
    }

    mn_item_t *item = &page->items[index];

    if (!IsSelectable(item))
    {
        M_StartSound(sfx_mnuerr);
        return;
    }

    if (menu->active_item && menu->active_item != item)
    {
        CancelActiveItem(menu);
    }

    page->selected = index;
    MN_ContainerEnsureVisible(item);

    if (item->type == MN_ITEM_SUBMENU && item->submenu)
    {
        PushMenu(item->submenu);
        M_StartSound(sfx_mnuact);
        return;
    }

    if (item->type == MN_ITEM_NUMBER || item->type == MN_ITEM_INPUT
        || item->type == MN_ITEM_WEAPON)
    {
        menu->active_item = item;

        if (item->ops && item->ops->respond)
        {
            item->ops->respond(item, MENU_ENTER, 0);
        }
        return;
    }

    if (item->type == MN_ITEM_CHOICE || item->type == MN_ITEM_SLIDER)
    {
        if (mouse)
        {
            menu->active_item = NULL;

            if (item->ops && item->ops->respond)
            {
                item->ops->respond(item, MENU_RIGHT, 0);
            }
        }
        else
        {
            menu->active_item = item;
        }
        return;
    }

    menu->active_item = NULL;

    if (item->ops && item->ops->respond)
    {
        item->ops->respond(item, MENU_ENTER, 0);
    }

    if (item->type == MN_ITEM_ACTION || item->type == MN_ITEM_RESET
        || item->type == MN_ITEM_TOGGLE)
    {
        M_StartSound(sfx_mnuact);
    }
}

static void MouseSelect(mn_menu_t *menu, int x, int y)
{
    mn_page_t *page = &menu->pages[menu->page];

    menu->hover = -1;

    for (int i = 0; i < page->num_items; ++i)
    {
        mn_item_t *item = &page->items[i];
        item->hover_segment = -1;
        item->hover_arrow = -1;
        mrect_t rect = item->rect;

        rect.x += menu->viewport.x;
        rect.y += menu->viewport.y - page->scroll;

        if (!MN_ContainerPointInside(&rect, x, y) || item->disabled)
        {
            continue;
        }

        menu->hover = IsNavigable(item) ? i : -1;

        if (menu->hover >= 0 && item->ops && item->ops->hover)
        {
            item->ops->hover(item, x, y);
        }
    }
}

static void UpdateScrollbar(mn_menu_t *menu)
{
    mn_page_t *page = &menu->pages[menu->page];
    const int max = MaxScroll(menu, page);

    menu->scroll_track = (mrect_t){0};
    menu->scroll_thumb = (mrect_t){0};

    if (max <= 0 || menu->viewport.h <= 0)
    {
        return;
    }

    const int thumb_h =
        MAX(8, menu->viewport.h * menu->viewport.h / page->content_h);

    menu->scroll_track = (mrect_t){menu->viewport.x + menu->viewport.w + 6,
                                   menu->viewport.y,
                                   4,
                                   menu->viewport.h};

    const int thumb_y = menu->scroll_track.y
                        + page->scroll * (menu->scroll_track.h - thumb_h) / max;

    menu->scroll_thumb =
        (mrect_t){menu->scroll_track.x, thumb_y, menu->scroll_track.w, thumb_h};
}

static void ScrollToThumb(mn_menu_t *menu, int mouse_y)
{
    mn_page_t *page = &menu->pages[menu->page];
    const int max = MaxScroll(menu, page);
    const int range = menu->scroll_track.h - menu->scroll_thumb.h;

    if (max <= 0 || range <= 0)
    {
        return;
    }

    int value =
        (mouse_y - menu->scroll_track.y - menu->drag_offset) * max / range;

    page->scroll = CLAMP(value, 0, max);
    page->scroll_target = page->scroll;
}

void MN_ContainerOpen(mn_menu_t *menu)
{
    if (!menu)
    {
        return;
    }

    menu_depth = 0;
    EnterMenu(menu);
}

void MN_ContainerClose(void)
{
    active_menu = NULL;
    menu_depth = 0;
}

boolean MN_ContainerActive(void)
{
    return active_menu != NULL;
}

mn_menu_t *MN_ContainerCurrent(void)
{
    return active_menu;
}

mn_page_t *MN_ContainerCurrentPage(void)
{
    return active_menu ? &active_menu->pages[active_menu->page] : NULL;
}

mn_item_t *MN_ContainerSelected(void)
{
    mn_page_t *page = MN_ContainerCurrentPage();

    if (!page || page->selected < 0 || page->selected >= page->num_items)
    {
        return NULL;
    }

    return &page->items[page->selected];
}

void MN_ContainerEndEdit(void)
{
    if (active_menu)
    {
        active_menu->active_item = NULL;
    }

    if (print_warning_about_changes > 0)
    {
        print_warning_about_changes--;
    }
}

void MN_ContainerRefresh(void)
{
    if (!active_menu)
    {
        return;
    }

    for (int i = 0; i < active_menu->num_pages; ++i)
    {
        if (active_menu->pages[i].refresh)
        {
            active_menu->pages[i].refresh();
        }
    }

    MN_ContainerRelayout();
    SelectFirst(active_menu);
}

void MN_ContainerConfirm(const char *text, void (*action)(void))
{
    if (!active_menu)
    {
        return;
    }

    active_menu->confirm = true;
    active_menu->confirm_text = text;
    active_menu->confirm_action = action;
}

void MN_ContainerItemChanged(mn_item_t *item)
{
    default_t *def = item->def;

    if (!def)
    {
        return;
    }

    if (item->flags & (MN_ITEM_WARN_LEVEL | MN_ITEM_WARN_PROGRAM))
    {
        warning_about_changes =
            item->flags & (MN_ITEM_WARN_LEVEL | MN_ITEM_WARN_PROGRAM);
        print_warning_about_changes = 2;
    }
    else if (def->current.i)
    {
        *def->current.i = *def->location.i;
    }

    if (item->action)
    {
        item->action();
    }
}

mn_container_result_t MN_ContainerResponder(menu_action_t action, int ch)
{
    if (!active_menu)
    {
        return MN_CONTAINER_HANDLED;
    }

    mn_menu_t *menu = active_menu;
    mn_page_t *page = &menu->pages[menu->page];

    if (menu->confirm)
    {
        if (M_ToUpper(ch) == 'Y' || action == MENU_ENTER)
        {
            menu->confirm = false;
            if (menu->confirm_action)
            {
                menu->confirm_action();
            }
            M_StartSound(sfx_mnusel);
        }
        else if (M_ToUpper(ch) == 'N' || action == MENU_BACKSPACE
                 || action == MENU_ESCAPE)
        {
            menu->confirm = false;
            M_StartSound(sfx_mnusel);
        }
        return MN_CONTAINER_HANDLED;
    }

    if (menu->active_item)
    {
        if (menu->active_item->ops && menu->active_item->ops->respond)
        {
            menu->active_item->ops->respond(menu->active_item, action, ch);
        }
        return MN_CONTAINER_HANDLED;
    }

    if (action == MENU_ESCAPE)
    {
        return MN_CONTAINER_ESCAPE;
    }

    if (action == MENU_BACKSPACE)
    {
        if (PopMenu())
        {
            M_StartSound(sfx_mnubak);
            return MN_CONTAINER_HANDLED;
        }

        return MN_CONTAINER_CLOSE;
    }

    if (page->num_items <= 0)
    {
        return MN_CONTAINER_HANDLED;
    }

    if (action == MENU_UP)
    {
        MoveSelection(menu, -1);
        return MN_CONTAINER_HANDLED;
    }

    if (action == MENU_DOWN)
    {
        MoveSelection(menu, 1);
        return MN_CONTAINER_HANDLED;
    }

    if (action == MENU_LEFT)
    {
        NextPage(menu, -1);
        return MN_CONTAINER_HANDLED;
    }

    if (action == MENU_RIGHT)
    {
        NextPage(menu, 1);
        return MN_CONTAINER_HANDLED;
    }

    if (action == MENU_ENTER)
    {
        ActivateItem(menu, page->selected, false);
        return MN_CONTAINER_HANDLED;
    }

    mn_item_t *item = &page->items[page->selected];

    if (ch && item->ops && item->ops->respond)
    {
        item->ops->respond(item, action, ch);
    }

    return MN_CONTAINER_HANDLED;
}

boolean MN_ContainerMouseMove(int x, int y)
{
    if (!active_menu)
    {
        return false;
    }

    last_mouse_x = x;
    last_mouse_y = y;

    if (active_menu->drag_item)
    {
        if (active_menu->drag_item->ops && active_menu->drag_item->ops->drag)
        {
            active_menu->drag_item->ops->drag(active_menu->drag_item, x);
        }
        return true;
    }

    if (active_menu->dragging_scroll)
    {
        ScrollToThumb(active_menu, y);
    }

    int highlight_tab = -1;
    for (int i = 0; i < active_menu->num_pages && i < MN_MAX_TABS; ++i)
    {
        mrect_t *rect = &active_menu->tab_rects[i];
        if (MN_ContainerPointInside(rect, x, y))
        {
            highlight_tab = i;
            break;
        }
    }

    if (highlight_tab != active_menu->page &&
        highlight_tab != active_menu->highlight_tab)
    {
        active_menu->highlight_tab = highlight_tab;
        if (highlight_tab >= 0)
        {
            M_StartSound(sfx_mnusel);
        }
    }

    MouseSelect(active_menu, x, y);
    return true;
}

boolean MN_ContainerMouseButton(int button, boolean down)
{
    if (!active_menu || button != MOUSE_BUTTON_LEFT)
    {
        return active_menu != NULL;
    }

    mn_menu_t *menu = active_menu;
    mn_page_t *page = &menu->pages[menu->page];

    if (menu->confirm)
    {
        if (down)
        {
            MN_ContainerResponder(MENU_ENTER, 0);
        }
        return true;
    }

    UpdateScrollbar(menu);

    if (!down)
    {
        menu->dragging_scroll = false;
        menu->drag_item = NULL;
        return true;
    }

    if (MN_ContainerPointInside(&menu->scroll_thumb, last_mouse_x,
                                last_mouse_y))
    {
        menu->dragging_scroll = true;
        menu->drag_offset = last_mouse_y - menu->scroll_thumb.y;
        return true;
    }

    if (MN_ContainerPointInside(&menu->scroll_track, last_mouse_x,
                                last_mouse_y))
    {
        menu->dragging_scroll = true;
        menu->drag_offset = menu->scroll_thumb.h / 2;
        ScrollToThumb(menu, last_mouse_y);
        return true;
    }

    for (int i = 0; i < menu->num_pages && i < MN_MAX_TABS; ++i)
    {
        if (MN_ContainerPointInside(&menu->tab_rects[i], last_mouse_x,
                                    last_mouse_y))
        {
            CancelActiveItem(menu);
            menu->page = i;
            menu->hover = -1;
            menu->highlight_tab = -1;
            EnsureValidSelection(menu, i);
            MN_ContainerEnsureVisible(
                &menu->pages[i].items[menu->pages[i].selected]);
            M_StartSound(sfx_mnumov);
            return true;
        }
    }

    MouseSelect(menu, last_mouse_x, last_mouse_y);

    if (menu->hover >= 0)
    {
        mn_item_t *item = &page->items[menu->hover];

        if (item->type == MN_ITEM_SLIDER && item->ops && item->ops->drag)
        {
            menu->drag_item = item;
            item->ops->drag(item, last_mouse_x);
            return true;
        }

        if (item->ops && item->ops->click)
        {
            if (item->ops->click(item, last_mouse_x, last_mouse_y))
            {
                return true;
            }
        }

        ActivateItem(menu, menu->hover, true);
    }

    return true;
}

boolean MN_ContainerWheel(int delta)
{
    if (!active_menu)
    {
        return false;
    }

    mn_page_t *page = &active_menu->pages[active_menu->page];
    page->scroll_target += delta * 3 * MN_SPC;
    ClampScroll(active_menu, page);
    return true;
}

void MN_ContainerTicker(void)
{
    if (!active_menu)
    {
        return;
    }

    mn_page_t *page = &active_menu->pages[active_menu->page];

    if (page->scroll != page->scroll_target)
    {
        const int diff = page->scroll_target - page->scroll;
        int step = diff / 3;

        if (step == 0 && diff != 0)
        {
            step = diff > 0 ? 1 : -1;
        }
        else if (abs(step) < 2 && abs(diff) > 3)
        {
            step = diff > 0 ? 2 : -2;
        }

        page->scroll += step;

        if (abs(page->scroll_target - page->scroll) <= 2)
        {
            page->scroll = page->scroll_target;
        }
    }
}

static void DrawTabs(const mn_menu_t *menu)
{
    if (!HasTabs(menu))
    {
        return;
    }

    for (int i = 0; i < menu->num_pages && i < MN_MAX_TABS; ++i)
    {
        const mrect_t *rect = &menu->tab_rects[i];

        if (i == menu->page)
        {
            MN_DrawString(rect->x, rect->y, CR_GOLD, menu->pages[i].tab);
            V_FillRect(rect->x + video.deltaw, rect->y + MN_SPC, rect->w, 1,
                       MenuColor(CR_GOLD, false));
        }
        else if (i == menu->highlight_tab)
        {
            MN_DrawStringCR(rect->x, rect->y, xlat[CR_GRAY].table,
                            xlat[CR_BRIGHT].table,  menu->pages[i].tab);
        }
        else
        {
            MN_DrawString(rect->x, rect->y, CR_GRAY, menu->pages[i].tab);
        }
    }
}

static void DrawItemLabel(const mn_item_t *item, const mn_draw_ctx_t *ctx,
                          int label_x)
{
    if (item->type == MN_ITEM_TITLE)
    {
        return;
    }

    byte *cr = ctx->disabled   ? cr_dark
               : ctx->selected ? xlat[CR_BRIGHT].table
                               : xlat[CR_NONE].table;

    int y = ctx->y;
    if (item->height > MN_SPC)
    {
        y += (item->height - MN_SPC) / 2;
    }

    if (ctx->selected && whichSkull)
    {
        MN_DrawStringCR(label_x - 8, y, cr, NULL, ">");
    }

    MN_DrawStringCR(label_x, y, cr, NULL, item->label);
}

static void DrawItems(const mn_menu_t *menu)
{
    const mn_page_t *page = &menu->pages[menu->page];
    const int top = menu->viewport.y;
    const int bottom = menu->viewport.y + menu->viewport.h;
    const boolean hide_values = (menu->flags & MN_MENU_HIDE_VALUES) != 0;

    for (int i = 0; i < page->num_items; ++i)
    {
        mn_item_t *item = &page->items[i];
        const int y = menu->viewport.y + item->rect.y - page->scroll;

        if (y + item->rect.h <= top || y >= bottom)
        {
            continue;
        }

        const int x = menu->viewport.x;
        const int label_w = (menu->flags & MN_MENU_CENTER)
                                ? page->label_width
                                : MN_GetPixelWidth(item->label);
        const int label_x = (menu->flags & MN_MENU_CENTER)
                                ? x + (item->rect.w - page->label_width) / 2
                                : x;
        const int value_x =
            MIN(label_x + MAX(label_w + 12, 120), x + item->rect.w - 48);
        const int value_w = x + item->rect.w - value_x;

        item->draw_value_x = value_x;
        item->draw_value_w = MAX(0, value_w);

        mn_draw_ctx_t ctx = {
            .x = x,
            .y = y,
            .w = item->rect.w,
            .h = item->rect.h,
            .value_x = value_x,
            .value_w = MAX(0, value_w),
            .color = (i == page->selected) ? CR_BRIGHT : CR_NONE,
            .selected = (i == page->selected || i == menu->hover),
            .active = (item == menu->active_item),
            .disabled = item->disabled,
            .mouse_inside = (i == menu->hover),
        };

        DrawItemLabel(item, &ctx, label_x);

        if (!hide_values && item->ops && item->ops->draw)
        {
            item->ops->draw(item, &ctx);
        }
    }
}

static void DrawWrappedText(int x, int y, int width, const char *text)
{
    const int line_h = MN_StringHeight("X") + 1;
    char line[256];
    int len = 0;

    while (*text)
    {
        if (*text == '\n')
        {
            line[len] = '\0';
            MN_DrawString(x, y, CR_GRAY, line);
            y += line_h;
            len = 0;
            text++;
            continue;
        }

        if (*text == ' ')
        {
            text++;
            continue;
        }

        const char *word = text;

        while (*text && *text != ' ' && *text != '\n')
        {
            text++;
        }

        int word_len = text - word;

        char probe[256];
        int probe_len = 0;

        if (len)
        {
            memcpy(probe, line, len);
            probe_len = len;
            probe[probe_len++] = ' ';
        }

        if (probe_len + word_len >= (int)sizeof(probe))
        {
            word_len = (int)sizeof(probe) - probe_len - 1;
        }

        memcpy(probe + probe_len, word, word_len);
        probe_len += word_len;
        probe[probe_len] = '\0';

        if (len && MN_GetPixelWidth(probe) > width)
        {
            line[len] = '\0';
            MN_DrawString(x, y, CR_GRAY, line);
            y += line_h;
            memcpy(line, word, word_len);
            len = word_len;
        }
        else
        {
            memcpy(line, probe, probe_len);
            len = probe_len;
        }
    }

    if (len)
    {
        line[len] = '\0';
        MN_DrawString(x, y, CR_GRAY, line);
    }
}

static void DrawHints(const mn_menu_t *menu)
{
    if (menu->hint_rect.w <= 0)
    {
        return;
    }

    mn_item_t *item = MN_ContainerSelected();
    const char *text = NULL;

    if (item)
    {
        text = item->desc;

        if (!text && item->def)
        {
            text = item->def->help;
        }
    }

    if (text)
    {
        DrawWrappedText(menu->hint_rect.x, menu->hint_rect.y,
                        menu->hint_rect.w, text);
    }
}

static void DrawScrollbar(const mn_menu_t *menu)
{
    if (menu->scroll_track.w <= 0)
    {
        return;
    }

    V_FillRect(menu->scroll_track.x + video.deltaw + 1,
               menu->scroll_track.y,
               menu->scroll_track.w - 2,
               menu->scroll_track.h,
               MenuColor(CR_WHITE, false));

    V_FillRect(menu->scroll_thumb.x + video.deltaw + 1,
               menu->scroll_thumb.y,
               menu->scroll_thumb.w - 2,
               menu->scroll_thumb.h,
               MenuColor(CR_GRAY, false));
}

static void DrawInstructions(const mn_menu_t *menu)
{
    const mn_item_t *item = MN_ContainerSelected();
    const char *text;

    if (menu->confirm)
    {
        return;
    }

    if (print_warning_about_changes > 0)
    {
        text = (warning_about_changes & MN_ITEM_WARN_PROGRAM)
                   ? "Warning: Restart to see changes"
                   : "Warning: Changes apply to next game";

        MN_DrawStringCR((SCREENWIDTH - MN_GetPixelWidth(text)) / 2,
                        SCREENHEIGHT - 15, xlat[CR_RED].table, NULL, text);
        return;
    }

    if (menu->active_item)
    {
        text = "Enter: accept, Esc: cancel";
    }
    else if (item && item->desc && menu->hint_rect.w <= 0)
    {
        text = item->desc;
    }
    else if (HasTabs(menu))
    {
        text = "Up/Down: select, Left/Right: page, Enter: change";
    }
    else
    {
        text = "Up/Down: select, Enter: change";
    }

    MN_DrawStringCR((SCREENWIDTH - MN_GetPixelWidth(text)) / 2,
                    SCREENHEIGHT - 15, cr_shaded, NULL, text);
}

static void DrawConfirm(const mn_menu_t *menu)
{
    patch_t *patch = V_CachePatchName("M_VBOX", PU_CACHE);
    const int x = (SCREENWIDTH - patch->width) / 2;
    const int y = (SCREENHEIGHT - patch->height) / 2;

    V_DrawPatch(x, y, patch);

    if (whichSkull)
    {
        MN_DrawString((SCREENWIDTH - MN_GetPixelWidth(menu->confirm_text)) / 2,
                      y + patch->height / 2, CR_RED, menu->confirm_text);
    }
}

void MN_ContainerDrawer(void)
{
    if (!active_menu)
    {
        return;
    }

    mn_menu_t *menu = active_menu;

    if (menu_backdrop == MENU_BG_TEXTURE && menu->background)
    {
        V_DrawBackground(menu->background);
    }

    MN_DrawTitle(M_X_CENTER, 2, menu->title_patch, menu->title_text);
    DrawTabs(menu);

    V_SetClipY(menu->viewport.y, menu->viewport.y + menu->viewport.h);
    DrawItems(menu);
    DrawHints(menu);
    V_ResetClipY();

    UpdateScrollbar(menu);
    DrawScrollbar(menu);
    DrawInstructions(menu);

    if (menu->confirm)
    {
        DrawConfirm(menu);
    }
}

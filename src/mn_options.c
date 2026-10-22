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

#include <stdlib.h>
#include <string.h>

#include "doomstat.h"
#include "doomtype.h"
#include "g_game.h"
#include "i_oalequalizer.h"
#include "i_oalsound.h"
#include "i_sound.h"
#include "i_system.h"
#include "i_video.h"
#include "m_array.h"
#include "m_config.h"
#include "m_input.h"
#include "m_misc.h"
#include "mn_container.h"
#include "mn_internal.h"
#include "mn_menu.h"
#include "mn_options.h"
#include "p_mobj.h"
#include "r_main.h"
#include "r_voxel.h"
#include "s_sound.h"
#include "sounds.h"
#include "v_palette.h"
#include "v_patch.h"
#include "v_trans.h"
#include "v_video.h"

static int ItemMin(const mn_item_t *item)
{
    if (item->def && item->def->limit.min != UL)
    {
        return item->def->limit.min;
    }

    return 0;
}

static int ItemMax(const mn_item_t *item)
{
    if (item->type == MN_ITEM_CHOICE && item->num_choices > 0)
    {
        return item->num_choices - 1;
    }

    if (item->def && item->def->limit.max != UL)
    {
        return item->def->limit.max;
    }

    if (item->num_choices > 0)
    {
        return item->num_choices - 1;
    }

    return 100;
}

static int OptionValue(const mn_item_t *item)
{
    return item->def ? *item->def->location.i : 0;
}

static boolean OptionEnabled(const mn_item_t *item)
{
    if (!item || (item->flags & MN_ITEM_DISABLED))
    {
        return false;
    }

    if (item->def && item->def->disabled)
    {
        return false;
    }

    if (item->cfg && M_CheckIfDisabled(item->cfg))
    {
        return false;
    }

    const complevel_t complevel =
        force_complevel != CL_NONE ? force_complevel : default_complevel;

    if ((item->flags & MN_ITEM_STRICT) && strictmode)
    {
        return false;
    }

    if ((item->flags & MN_ITEM_BOOM) && complevel < CL_BOOM)
    {
        return false;
    }

    if ((item->flags & MN_ITEM_MBF) && complevel < CL_MBF)
    {
        return false;
    }

    if ((item->flags & MN_ITEM_VANILLA) && complevel != CL_VANILLA)
    {
        return false;
    }

    return true;
}

static void OptionSetValue(mn_item_t *item, int value)
{
    if (!item->def)
    {
        return;
    }

    value = CLAMP(value, ItemMin(item), ItemMax(item));

    if (*item->def->location.i != value)
    {
        *item->def->location.i = value;
        M_StartSound(sfx_mnusli);
    }

    MN_ContainerItemChanged(item);
}

static void DisableItem(boolean condition, mn_menu_t *menu, const char *name)
{
    for (int i = 0; i < menu->num_pages; ++i)
    {
        mn_page_t *page = &menu->pages[i];

        for (mn_item_t *item = page->items; item->type != MN_ITEM_END; ++item)
        {
            if (!strcmp(item->cfg, name))
            {
                item->disabled = condition;
                return;
            }
        }
    }

    I_Error("%s not found.", name);
}

static int SliderPixelWidth(int size)
{
    return 8 + (size + 1) * 8 + 8;
}

static void OptionSliderSetFromMouse(mn_item_t *item, int mouse_x)
{
    const int size = item->slider_size ? item->slider_size : 8;
    const int thermo_w = SliderPixelWidth(size);
    const int bar_x = item->draw_value_x + item->draw_value_w - thermo_w + 8;
    const int bar_w = size * 8;
    const int min = ItemMin(item);
    const int max = ItemMax(item);

    if (bar_w <= 0 || max <= min)
    {
        return;
    }

    const int rel = mouse_x - video.deltaw - bar_x;
    const int value = min + rel * (max - min) / bar_w;

    OptionSetValue(item, value);
}

static byte *ValueColor(const mn_draw_ctx_t *ctx)
{
    if (ctx->disabled)
    {
        return cr_dark;
    }

    if (ctx->selected || ctx->active)
    {
        return xlat[CR_BRIGHT].table;
    }

    return NULL;
}

static void DrawValueString(const mn_draw_ctx_t *ctx, const char *text)
{
    const int width = MN_GetPixelWidth(text);

    MN_DrawStringCR(ctx->value_x + ctx->value_w - width, ctx->y,
                    xlat[CR_GREEN].table, ValueColor(ctx), text);
}

static void TitleDraw(const mn_item_t *item, const mn_draw_ctx_t *ctx)
{
    MN_DrawString(ctx->x, ctx->y, CR_GOLD, item->label);
}

static boolean TitleRespond(mn_item_t *item, menu_action_t action, int ch)
{
    return false;
}

static void ToggleDraw(const mn_item_t *item, const mn_draw_ctx_t *ctx)
{
    DrawValueString(ctx, OptionValue(item) ? "ON" : "OFF");
}

static boolean ToggleRespond(mn_item_t *item, menu_action_t action, int ch)
{
    if (action != MENU_ENTER)
    {
        return false;
    }

    *item->def->location.i = !OptionValue(item);
    MN_ContainerItemChanged(item);
    MN_ContainerEndEdit();
    return true;
}

static void NumberDraw(const mn_item_t *item, const mn_draw_ctx_t *ctx)
{
    char buffer[32];

    if (item->gathering)
    {
        M_snprintf(buffer, sizeof(buffer), "%s", item->gather);
    }
    else if (item->suffix && !strcmp(item->suffix, "%"))
    {
        M_snprintf(buffer, sizeof(buffer), "%d%%", OptionValue(item));
    }
    else if (item->suffix)
    {
        M_snprintf(buffer, sizeof(buffer), "%d %s", OptionValue(item),
                   item->suffix);
    }
    else
    {
        M_snprintf(buffer, sizeof(buffer), "%d", OptionValue(item));
    }

    DrawValueString(ctx, buffer);
}

static void NumberCommit(mn_item_t *item)
{
    if (!item->gathering)
    {
        return;
    }

    item->gather[item->gather_count] = '\0';

    if (item->gather_count)
    {
        OptionSetValue(item, atoi(item->gather));
    }

    item->gathering = false;
    item->gather_count = 0;
    MN_ContainerEndEdit();
}

static boolean NumberRespond(mn_item_t *item, menu_action_t action, int ch)
{
    if (action == MENU_ENTER)
    {
        if (!item->gathering)
        {
            item->gathering = true;
            item->gather_count = 0;
            item->gather[0] = '\0';
        }
        else
        {
            NumberCommit(item);
        }
        return true;
    }

    if (!item->gathering)
    {
        return false;
    }

    if (action == MENU_ESCAPE)
    {
        item->gathering = false;
        item->gather_count = 0;
        MN_ContainerEndEdit();
        return true;
    }

    if (action == MENU_BACKSPACE)
    {
        if (item->gather_count)
        {
            item->gather[--item->gather_count] = '\0';
        }
        else
        {
            item->gathering = false;
            MN_ContainerEndEdit();
        }
        return true;
    }

    if ((ch >= '0' && ch <= '9') || (ch == '-' && item->gather_count == 0))
    {
        if (item->gather_count < (int)sizeof(item->gather) - 1)
        {
            item->gather[item->gather_count++] = ch;
            item->gather[item->gather_count] = '\0';
        }
    }

    return true;
}

enum
{
    CHOICE_ARROW_W = 8,
    CHOICE_PADDING = 4,
    CHOICE_SEG_W = 8,
    CHOICE_SEG_GAP = 2,
    CHOICE_MIN_VISIBLE = 2,
    CHOICE_MAX_VISIBLE = 12,
};

typedef struct
{
    int x;
    int bar_x;
    int bar_w;
    int content_w;
    int visible;
    int min;
    int max;
    int num_choices;
} choice_layout_t;

static choice_layout_t ChoiceLayout(const mn_item_t *item)
{
    choice_layout_t l;

    l.min = ItemMin(item);
    l.max = ItemMax(item);
    l.num_choices = l.max - l.min + 1;
    l.visible = CLAMP(l.num_choices, CHOICE_MIN_VISIBLE, CHOICE_MAX_VISIBLE);

    const int text_area_w =
        MAX(item->choice_width > 0 ? item->choice_width * 8 : l.num_choices * 8,
            CHOICE_MIN_VISIBLE * 8);

    l.bar_w = l.visible * CHOICE_SEG_W + (l.visible - 1) * CHOICE_SEG_GAP;
    l.content_w = MAX(text_area_w, l.bar_w);
    l.x = item->draw_value_x + item->draw_value_w
          - (CHOICE_ARROW_W * 2 + CHOICE_PADDING * 2 + l.content_w);
    l.bar_x =
        l.x + CHOICE_ARROW_W + CHOICE_PADDING + (l.content_w - l.bar_w) / 2;

    return l;
}

static const char *ChoiceText(const mn_item_t *item, int value)
{
    static char buffer[32];

    if (item->choices && value >= 0 && value < item->num_choices)
    {
        return item->choices[value];
    }

    M_snprintf(buffer, sizeof(buffer), "%d", value);
    return buffer;
}

static int ChoiceStep(const mn_item_t *item, boolean right)
{
    const int value = OptionValue(item);
    const int min = ItemMin(item);
    const int max = ItemMax(item);

    return right ? (value >= max ? min : value + 1)
                 : (value <= min ? max : value - 1);
}

static void ChoiceApplyValue(mn_item_t *item, int value)
{
    if (*item->def->location.i != value)
    {
        *item->def->location.i = value;
        M_StartSound(sfx_mnusli);
        MN_ContainerItemChanged(item);
    }
}

// Locates the mouse inside the widget. Returns false when outside of it.
static boolean ChoiceHitTest(const mn_item_t *item, int mouse_x, int *arrow,
                             int *segment)
{
    const choice_layout_t l = ChoiceLayout(item);
    int rel_x = mouse_x - video.deltaw - l.x;

    *arrow = -1;
    *segment = -1;

    if (rel_x >= 0 && rel_x < CHOICE_ARROW_W + CHOICE_PADDING)
    {
        *arrow = 0;
        return true;
    }

    rel_x -= CHOICE_ARROW_W + CHOICE_PADDING;

    if (rel_x >= 0 && rel_x < l.content_w)
    {
        const int bar_rel_x = rel_x - (l.content_w - l.bar_w) / 2;

        if (bar_rel_x >= 0 && bar_rel_x < l.bar_w)
        {
            *segment = bar_rel_x / (CHOICE_SEG_W + CHOICE_SEG_GAP);
        }

        return true;
    }

    rel_x -= l.content_w + CHOICE_PADDING;

    if (rel_x >= 0 && rel_x < CHOICE_ARROW_W + CHOICE_PADDING)
    {
        *arrow = 1;
        return true;
    }

    return false;
}

static void ChoiceHover(mn_item_t *item, int mouse_x, int mouse_y)
{
    ChoiceHitTest(item, mouse_x, &item->hover_arrow, &item->hover_segment);
}

static void ChoiceDraw(const mn_item_t *item, const mn_draw_ctx_t *ctx)
{
    const int value = OptionValue(item);
    const choice_layout_t l = ChoiceLayout(item);
    const int content_x = l.x + CHOICE_ARROW_W + CHOICE_PADDING;
    const int y = ctx->y;
    const boolean hovered = ctx->mouse_inside && menu_input == mouse_mode;
    const boolean left_hover = hovered && (item->hover_arrow == 0);
    const boolean right_hover = hovered && (item->hover_arrow == 1);
    byte *cr = ValueColor(ctx);

    const char *current_text = NULL;
    if (hovered && item->hover_segment >= 0 && item->hover_segment < l.visible
        && item->choices)
    {
        const int hover_idx = l.min + item->hover_segment;

        if (hover_idx >= 0 && hover_idx < item->num_choices)
        {
            current_text = item->choices[hover_idx];
        }
    }

    if (!current_text)
    {
        current_text = ChoiceText(item, value);
    }

    MN_DrawStringCR(l.x, y,
                    left_hover ? xlat[CR_GRAY].table : xlat[CR_GREEN].table,
                    left_hover ? NULL : cr, "<");

    MN_DrawStringCR(content_x
                        + (l.content_w - MN_GetPixelWidth(current_text)) / 2,
                    y, xlat[CR_GREEN].table, cr, current_text);

    const byte dark = cr_dark[playpal_global->white];
    const byte shade = cr_shaded[playpal_global->white];

    for (int i = 0; i < l.visible; ++i)
    {
        byte seg_color;

        if (l.min + i == value)
        {
            seg_color = xlat[CR_WHITE].table[shade];
        }
        else if (hovered && item->hover_segment == i)
        {
            seg_color = xlat[CR_WHITE].table[playpal_global->white];
        }
        else if (ctx->disabled)
        {
            seg_color = xlat[CR_GREEN].table[dark];
        }
        else
        {
            seg_color = xlat[CR_GREEN].table[shade];
        }

        V_FillRect(l.bar_x + i * (CHOICE_SEG_W + CHOICE_SEG_GAP) + video.deltaw,
                   y + MN_SPC, CHOICE_SEG_W, 1, seg_color);
    }

    MN_DrawStringCR(content_x + l.content_w + CHOICE_PADDING, y,
                    right_hover ? xlat[CR_GRAY].table : xlat[CR_GREEN].table,
                    right_hover ? NULL : cr, ">");
}

static boolean ChoiceRespond(mn_item_t *item, menu_action_t action, int ch)
{
    if (action == MENU_ENTER || action == MENU_ESCAPE
        || action == MENU_BACKSPACE)
    {
        MN_ContainerEndEdit();
        return true;
    }

    if (action != MENU_LEFT && action != MENU_RIGHT)
    {
        return false;
    }

    const int value = ChoiceStep(item, action == MENU_RIGHT);

    if (*item->def->location.i != value)
    {
        *item->def->location.i = value;
        M_StartSound(sfx_mnusli);
    }

    MN_ContainerItemChanged(item);
    return true;
}

static boolean ChoiceClick(mn_item_t *item, int mouse_x, int mouse_y)
{
    int arrow;
    int segment;

    if (!ChoiceHitTest(item, mouse_x, &arrow, &segment))
    {
        return false;
    }

    if (arrow >= 0)
    {
        ChoiceApplyValue(item, ChoiceStep(item, arrow == 1));
    }
    else if (segment >= 0)
    {
        ChoiceApplyValue(item, MIN(ItemMin(item) + segment, ItemMax(item)));
    }

    return true;
}

static void DrawThermo(const mn_item_t *item, const mn_draw_ctx_t *ctx)
{
    const int size = item->slider_size ? item->slider_size : 8;
    const int value = OptionValue(item);
    const int min = ItemMin(item);
    const int max = ItemMax(item);
    byte *cr = ctx->disabled                    ? cr_dark
               : (ctx->selected || ctx->active) ? xlat[CR_BRIGHT].table
                                                : NULL;

    const int thermo_w = SliderPixelWidth(size);
    int x = ctx->value_x + ctx->value_w - thermo_w;

    V_DrawPatchTranslated(x, ctx->y, V_CachePatchName("M_THERML", PU_CACHE), cr);
    x += 8;

    patch_t *middle = V_CachePatchName("M_THERMM", PU_CACHE);

    for (int i = 0; i < size + 1; ++i)
    {
        V_DrawPatchTranslated(x, ctx->y, middle, cr);
        x += 8;
    }

    V_DrawPatchTranslated(x, ctx->y, V_CachePatchName("M_THERMR", PU_CACHE), cr);

    if (max > min)
    {
        const int dot = (value - min) * size * 8 / (max - min);

        V_DrawPatchTranslated(ctx->value_x + ctx->value_w - thermo_w + 8 + dot,
                              ctx->y, V_CachePatchName("M_THERMO", PU_CACHE), cr);
    }
}

static void SliderDraw(const mn_item_t *item, const mn_draw_ctx_t *ctx)
{
    DrawThermo(item, ctx);

    char buffer[32];
    const int value = OptionValue(item);

    if (item->choices && value >= 0 && value < item->num_choices)
    {
        M_snprintf(buffer, sizeof(buffer), "%s", item->choices[value]);
    }
    else if (item->suffix && !strcmp(item->suffix, "%"))
    {
        M_snprintf(buffer, sizeof(buffer), "%d%%", value);
    }
    else if (item->suffix)
    {
        M_snprintf(buffer, sizeof(buffer), "%d %s", value, item->suffix);
    }
    else
    {
        M_snprintf(buffer, sizeof(buffer), "%d", value);
    }

    const int size = item->slider_size ? item->slider_size : 8;
    const int thermo_w = SliderPixelWidth(size);
    const int text_w = MN_GetPixelWidth(buffer);

    int text_x = ctx->value_x + ctx->value_w - thermo_w - text_w;
    int text_y = ctx->y + (MN_SPC_SLIDER - MN_SPC) / 2;

    MN_DrawStringCR(text_x, text_y, xlat[CR_GREEN].table, ValueColor(ctx), buffer);
}

static boolean SliderRespond(mn_item_t *item, menu_action_t action, int ch)
{
    if (action == MENU_ENTER || action == MENU_ESCAPE
        || action == MENU_BACKSPACE)
    {
        MN_ContainerEndEdit();
        return true;
    }

    if (action != MENU_LEFT && action != MENU_RIGHT)
    {
        return false;
    }

    int value = OptionValue(item);

    value += (action == MENU_RIGHT) ? 1 : -1;
    OptionSetValue(item, value);
    return true;
}

static void SliderDrag(mn_item_t *item, int mouse_x)
{
    OptionSliderSetFromMouse(item, mouse_x);
}

static void ActionDraw(const mn_item_t *item, const mn_draw_ctx_t *ctx)
{
    DrawValueString(ctx, ". . .");
}

static boolean ActionRespond(mn_item_t *item, menu_action_t action, int ch)
{
    if (action == MENU_ENTER)
    {
        if (item->action)
        {
            item->action();
        }
        MN_ContainerEndEdit();
        return true;
    }

    return false;
}

static void ResetDraw(const mn_item_t *item, const mn_draw_ctx_t *ctx)
{
    static const char *const names[2] = {"M_BUTT1", "M_BUTT2"};
    const int index = (ctx->selected || ctx->active) ? whichSkull : 0;
    patch_t *patch = V_CachePatchName(names[index], PU_CACHE);

    V_DrawPatch(ctx->value_x + ctx->value_w - patch->width, ctx->y, patch);
}

static void OptionsResetPage(void);

static boolean ResetRespond(mn_item_t *item, menu_action_t action, int ch)
{
    if (action == MENU_ENTER)
    {
        MN_ContainerConfirm("Restore defaults? (Y/N)", OptionsResetPage);
        MN_ContainerEndEdit();
        return true;
    }

    return false;
}

static void InputDraw(const mn_item_t *item, const mn_draw_ctx_t *ctx)
{
    DrawValueString(ctx, "N/A");
}

static boolean InputRespond(mn_item_t *item, menu_action_t action, int ch)
{
    MN_ContainerEndEdit();
    return true;
}

static void WeaponDraw(const mn_item_t *item, const mn_draw_ctx_t *ctx)
{
    char buffer[8];

    M_snprintf(buffer, sizeof(buffer), "%d", OptionValue(item));
    DrawValueString(ctx, buffer);
}

static boolean WeaponRespond(mn_item_t *item, menu_action_t action, int ch)
{
    if (action == MENU_ENTER)
    {
        MN_ContainerEndEdit();
        return true;
    }

    if (ch >= '1' && ch <= '9')
    {
        OptionSetValue(item, ch - '0');
    }

    return true;
}

static const mn_item_ops_t title_ops =  {TitleDraw, TitleRespond};
static const mn_item_ops_t toggle_ops = {ToggleDraw, ToggleRespond};
static const mn_item_ops_t number_ops = {NumberDraw, NumberRespond};
static const mn_item_ops_t choice_ops = {ChoiceDraw, ChoiceRespond,
                                         .click = ChoiceClick,
                                         .hover = ChoiceHover};
static const mn_item_ops_t slider_ops = {SliderDraw, SliderRespond,
                                         .drag = SliderDrag};
static const mn_item_ops_t action_ops = {ActionDraw, ActionRespond};
static const mn_item_ops_t reset_ops =  {ResetDraw, ResetRespond};
static const mn_item_ops_t input_ops =  {InputDraw, InputRespond};
static const mn_item_ops_t weapon_ops = {WeaponDraw, WeaponRespond};

static void BindItem(mn_item_t *item)
{
    item->gathering = false;
    item->gather_count = 0;
    item->gather[0] = '\0';
    item->action_pending = false;
    item->hover_arrow = -1;
    item->hover_segment = -1;

    switch (item->type)
    {
        case MN_ITEM_TITLE:
            item->ops = &title_ops;
            break;
        case MN_ITEM_TOGGLE:
            item->ops = &toggle_ops;
            break;
        case MN_ITEM_NUMBER:
            item->ops = &number_ops;
            break;
        case MN_ITEM_CHOICE:
            item->ops = &choice_ops;
            break;
        case MN_ITEM_SLIDER:
            item->ops = &slider_ops;
            break;
        case MN_ITEM_ACTION:
            item->ops = &action_ops;
            break;
        case MN_ITEM_SUBMENU:
            item->ops = &action_ops;
            break;
        case MN_ITEM_RESET:
            item->ops = &reset_ops;
            break;
        case MN_ITEM_INPUT:
            item->ops = &input_ops;
            break;
        case MN_ITEM_WEAPON:
            item->ops = &weapon_ops;
            break;
        default:
            item->ops = NULL;
            break;
    }

    if (item->cfg)
    {
        item->def = M_LookupDefault(item->cfg);

        if (!item->def)
        {
            I_Error("Could not find config variable \"%s\"", item->cfg);
        }
    }

    if (item->getchoices)
    {
        item->getchoices(item);
    }

    item->disabled = !OptionEnabled(item);
}

static void BindMenu(mn_menu_t *menu)
{
    if (menu->init)
    {
        return;
    }

    for (int i = 0; i < menu->num_pages; ++i)
    {
        mn_page_t *page = &menu->pages[i];

        for (mn_item_t *item = page->items; item->type != MN_ITEM_END; ++item)
        {
            BindItem(item);
        }
    }
    menu->init = true;
}

static void RefreshMenu(mn_menu_t *menu)
{
    for (int i = 0; i < menu->num_pages; ++i)
    {
        mn_page_t *page = &menu->pages[i];

        for (mn_item_t *item = page->items; item->type != MN_ITEM_END; ++item)
        {
            item->disabled = !OptionEnabled(item);
        }
    }
}

void MN_OptionsBind(void)
{
    BindMenu(&options_root_menu);
    BindMenu(&display_menu);
    BindMenu(&video_menu);
    BindMenu(&audio_menu);
    BindMenu(&equalizer_menu);
}

void MN_OptionsRefresh(void)
{
    RefreshMenu(&options_root_menu);
    RefreshMenu(&display_menu);
    RefreshMenu(&video_menu);
    RefreshMenu(&audio_menu);
    RefreshMenu(&equalizer_menu);
}

static void OptionsResetPage(void)
{
    mn_page_t *page = MN_ContainerCurrentPage();

    if (!page)
    {
        return;
    }

    for (mn_item_t *item = page->items; item->type != MN_ITEM_END; ++item)
    {
        default_t *def = item->def;

        if (def)
        {
            if (def->type == number || def->type == menu)
            {
                *def->location.i = def->defaultvalue.number;

                if (def->current.i)
                {
                    *def->current.i = *def->location.i;
                }
            }
            else if (def->type == string)
            {
                free(*def->location.s);
                *def->location.s = strdup(def->defaultvalue.string);

                if (def->current.s)
                {
                    *def->current.s = *def->location.s;
                }
            }
            else if (def->type == input)
            {
                M_InputSetDefault(def->input_id);
            }
        }
        else if (item->input_id)
        {
            M_InputSetDefault(item->input_id);
        }

        if (def && item->action)
        {
            item->action();
        }
    }

    MN_OptionsRefresh();
    MN_ContainerRefresh();
}

void MN_OptionsOpen(void)
{
    MN_OptionsBind();
    MN_OptionsRefresh();
    MN_ContainerOpen(&options_root_menu);
}

void MN_OptionsUpdateSfxVolume(void)
{
    S_SetSfxVolume(snd_SfxVolume);
}

void MN_OptionsUpdateMusicVolume(void)
{
    S_SetMusicVolume(snd_MusicVolume);
}

static int resolution_scale, max_resolution_scale;
#define DRS_MENU_STEP 50

void MN_GetResolutionScaleStrings(mn_item_t *item)
{
    array_push(item->choices, "100%");

    resolution_scale = 0;
    int val = SCREENHEIGHT * 2;
    char buf[8];
    int i;

    for (i = 1; val < max_height_adjusted; ++i)
    {
        if (val == current_video_height)
        {
            resolution_scale = i;
        }

        int pct = val * 100 / SCREENHEIGHT;
        M_snprintf(buf, sizeof(buf), "%d%%", pct);
        array_push(item->choices, M_StringDuplicate(buf));

        val += DRS_MENU_STEP;
    }

    max_resolution_scale = i;

    if (current_video_height == max_height_adjusted)
    {
        resolution_scale = max_resolution_scale;
    }

    array_push(item->choices, "max");

    item->num_choices = array_size(item->choices);
}

void MN_OptionsUpdateResolutionScale(void)
{
    if (resolution_scale == max_resolution_scale)
    {
        current_video_height = max_height_adjusted;
    }
    else if (resolution_scale == 0)
    {
        current_video_height = SCREENHEIGHT;
    }
    else
    {
        current_video_height =
            SCREENHEIGHT * 2 + (resolution_scale - 1) * DRS_MENU_STEP;
    }

    if (!dynamic_resolution)
    {
        VX_ResetMaxDist();
    }

    DisableItem(current_video_height <= DRS_MIN_HEIGHT, &video_menu,
                "dynamic_resolution");

    resetneeded = true;
}

void MN_OptionsUpdateFullscreen(void)
{
    toggle_fullscreen = true;
}

void MN_OptionsUpdateVSync(void)
{
    I_ToggleVsync();
}

void MN_OptionsUpdateFPSLimit(void)
{
    setrefreshneeded = true;
}

void MN_OptionsUpdateFOV(void)
{
    setsizeneeded = true;
}

void MN_OptionsResetVideo(void)
{
    resetneeded = true;
}

void MN_OptionsResetPalette(void)
{
    V_ResetPalette();
}

void MN_OptionsSetSoundModule(void)
{
    if (!I_AllowReinitSound())
    {
        warning_about_changes = MN_ITEM_WARN_PROGRAM;
        print_warning_about_changes = 2;
        return;
    }

    S_EvictChannels();
    I_SetSoundModule();
}

void MN_OptionsRestartMusic(void)
{
    S_StopMusic();
    S_SetMusicVolume(snd_MusicVolume);
    S_RestartMusic();
}

void MN_GetMidiPlayerStrings(mn_item_t *item)
{
    item->choices = I_DeviceList();
    item->num_choices = array_size(item->choices);
}

void MN_GetResamplerStrings(mn_item_t *item)
{
    item->choices = I_OAL_GetResamplerStrings();
    item->num_choices = array_size(item->choices);
}

void MN_OptionsSetMidiPlayer(void)
{
    S_StopMusic();
    I_SetMidiPlayer();
    S_SetMusicVolume(snd_MusicVolume);
    S_RestartMusic();
}

void MN_OptionsEqualizerPreset(void)
{
    I_OAL_EqualizerPreset();
}

typedef enum
{
    FREELOOK_OFF,        // Free look disabled.
    FREELOOK_AUTO_AIM,   // Free look enabled with traditional auto-aiming.
    FREELOOK_DIRECT_AIM, // Free look enabled with direct vertical aiming.
} freelook_mode_t;

static freelook_mode_t freelook_mode;

void MN_UpdateFreeLook(void)
{
    P_UpdateDirectVerticalAiming();

    if (!freelook)
    {
        for (int i = 0; i < MAXPLAYERS; ++i)
        {
            if (playeringame[i])
            {
                players[i].centering = true;
            }
        }
    }
}

static void UpdateFreeLookMode(void)
{
    switch (freelook_mode)
    {
        case FREELOOK_OFF:
            freelook = false;
            default_direct_vertical_aiming = false;
            break;
        case FREELOOK_AUTO_AIM:
            freelook = true;
            default_direct_vertical_aiming = false;
            break;
        case FREELOOK_DIRECT_AIM:
            freelook = true;
            default_direct_vertical_aiming = true;
            break;
    }

    MN_UpdateFreeLook();
}

void MN_BindOptionsVariables(void)
{
    BIND_NUM_MENU(resolution_scale, 0, UL);
    BIND_NUM_MENU(freelook_mode, FREELOOK_OFF, FREELOOK_DIRECT_AIM);
}

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
#include "hu_crosshair.h"
#include "i_gyro.h"
#include "i_oalequalizer.h"
#include "i_oalsound.h"
#include "i_input.h"
#include "i_rumble.h"
#include "i_sound.h"
#include "i_system.h"
#include "i_video.h"
#include "m_array.h"
#include "m_config.h"
#include "m_input.h"
#include "m_misc.h"
#include "mn_container.h"
#include "mn_font.h"
#include "mn_internal.h"
#include "mn_menu.h"
#include "mn_options.h"
#include "p_mobj.h"
#include "r_main.h"
#include "r_voxel.h"
#include "s_sound.h"
#include "sounds.h"
#include "st_stuff.h"
#include "st_widgets.h"
#include "v_palette.h"
#include "v_patch.h"
#include "v_trans.h"
#include "v_video.h"
#include "m_swap.h"
#include "ws_stuff.h"

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

    if (ctx->active)
    {
        return xlat[CR_GRAY].table;
    }

    if (ctx->selected)
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
    CHOICE_PADDING = 2,
    CHOICE_SEG_W = 8,
    CHOICE_SEG_GAP = 2,
    CHOICE_MIN_VISIBLE = 2,
    CHOICE_MAX_VISIBLE = 12,
};

typedef struct
{
    int x;
    int text_x;
    int text_w;
    int text_h;
    int bar_x;
    int bar_w;
    int seg_w;
    int seg_extra;
    int gap;
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

    if (item->choice_width > 0)
    {
        l.text_w = item->choice_width * 8;
    }
    else
    {
        l.text_w = MAX(l.num_choices * 8,
                       CHOICE_MIN_VISIBLE * CHOICE_SEG_W
                           + (CHOICE_MIN_VISIBLE - 1) * CHOICE_SEG_GAP);
    }

    l.visible = CLAMP(l.num_choices, CHOICE_MIN_VISIBLE, CHOICE_MAX_VISIBLE);
    l.gap = CHOICE_SEG_GAP;

    // Fill the whole text area with segments, keeping each at least
    // CHOICE_SEG_W wide. Shrink the number of segments.
    while (l.visible > 1
           && l.visible * CHOICE_SEG_W + (l.visible - 1) * l.gap > l.text_w)
    {
        l.visible--;
    }

    const int usable = MAX(l.visible * CHOICE_SEG_W,
                           l.text_w - (l.visible - 1) * l.gap);

    l.seg_w = usable / l.visible;
    l.seg_extra = usable % l.visible;
    l.bar_w = usable + (l.visible - 1) * l.gap;
    l.text_h = (item->flags & MN_ITEM_TWO_LINES) ? 2 * MN_SPC : MN_SPC;

    l.x = item->draw_value_x + item->draw_value_w
          - (CHOICE_ARROW_W * 2 + CHOICE_PADDING * 2 + l.text_w);
    l.text_x = l.x + CHOICE_ARROW_W + CHOICE_PADDING;
    l.bar_x = l.text_x;

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

    if (rel_x >= 0 && rel_x < l.text_w)
    {
        int seg_x = 0;

        for (int i = 0; i < l.visible; ++i)
        {
            const int seg_w = l.seg_w + (i < l.seg_extra ? 1 : 0);

            // Gaps belong to the preceding segment, so the mouse has no
            // dead zones between segments.
            if (rel_x < seg_x + seg_w + l.gap || i == l.visible - 1)
            {
                *segment = i;
                break;
            }

            seg_x += seg_w + l.gap;
        }

        return true;
    }

    rel_x -= l.text_w + CHOICE_PADDING;

    if (rel_x >= 0 && rel_x < CHOICE_ARROW_W + CHOICE_PADDING)
    {
        *arrow = 1;
        return true;
    }

    return false;
}

static void ChoiceHover(mn_item_t *item, int mouse_x, int mouse_y)
{
    static int hover;

    ChoiceHitTest(item, mouse_x, &item->hover_arrow, &item->hover_segment);

    if (hover != item->hover_segment)
    {
        hover = item->hover_segment;
        M_StartSound(sfx_mnusel);
    }
}

static void ChoiceDrawText(const mn_item_t *item, const choice_layout_t *l,
                           int y, const char *text, byte *cr)
{
    char buffer[256];

    M_snprintf(buffer, sizeof(buffer), "%s", text);

    if (!(item->flags & MN_ITEM_TWO_LINES))
    {
        MN_TruncateText(buffer, l->text_w);
        MN_DrawStringCR(
            l->text_x + (l->text_w - MN_GetPixelWidth(buffer)) / 2, y,
            xlat[CR_GREEN].table, cr, buffer);
        return;
    }

    const int num_lines = MN_WrapText(buffer, l->text_w);
    const int draw_lines = MIN(num_lines, 2);
    int start = 0;

    y += MAX(0, (l->text_h - draw_lines * MN_SPC) / 2);

    for (int i = 0; i < draw_lines && buffer[start]; ++i)
    {
        int end = start;

        while (buffer[end] && buffer[end] != '\n')
        {
            end++;
        }

        const char saved = buffer[end];
        buffer[end] = '\0';
        MN_TruncateText(buffer + start, l->text_w);
        MN_DrawStringCR(
            l->text_x + (l->text_w - MN_GetPixelWidth(buffer + start)) / 2, y,
            xlat[CR_GREEN].table, cr, buffer + start);
        buffer[end] = saved;

        if (!saved)
        {
            break;
        }

        y += MN_SPC;
        start = end + 1;
    }
}

static void ChoiceDraw(const mn_item_t *item, const mn_draw_ctx_t *ctx)
{
    const int value = OptionValue(item);
    const choice_layout_t l = ChoiceLayout(item);
    const int y = ctx->y;
    const int arrow_y = y + (l.text_h - MN_SPC) / 2;
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

    if (left_hover)
    {
        MN_DrawStringCR(l.x, arrow_y, xlat[CR_GRAY].table, NULL, "<");
    }
    else
    {
        MN_DrawStringCR(l.x, arrow_y, xlat[CR_GREEN].table, cr, "<");
    }

    ChoiceDrawText(item, &l, y, current_text, cr);

    const byte dark = cr_dark[playpal_global->white];
    const byte shade = cr_shaded[playpal_global->white];
    const int bar_y = y + l.text_h;
    int seg_x = l.bar_x;

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

        const int seg_w = l.seg_w + (i < l.seg_extra ? 1 : 0);

        V_FillRect(seg_x + video.deltaw, bar_y, seg_w, 1, seg_color);
        seg_x += seg_w + l.gap;
    }

    if (right_hover)
    {
        MN_DrawStringCR(l.text_x + l.text_w + CHOICE_PADDING, arrow_y,
                        xlat[CR_GRAY].table, NULL, ">");
    }
    else
    {
        MN_DrawStringCR(l.text_x + l.text_w + CHOICE_PADDING, arrow_y,
                        xlat[CR_GREEN].table, cr, ">");
    }
}

static void ChoiceDrawCrosshair(const mn_item_t *item, const mn_draw_ctx_t *ctx)
{
    ChoiceDraw(item, ctx);

    if (!(item->flags & MN_ITEM_DRAW_CROSSHAIR))
    {
        return;
    }

    const choice_layout_t l = ChoiceLayout(item);
    const boolean hovered = ctx->mouse_inside && menu_input == mouse_mode;

    int current_idx = hud_crosshair;
    if (hovered && item->hover_segment >= 0 && item->hover_segment < l.visible)
    {
        const int hover_idx = l.min + item->hover_segment;

        if (hover_idx >= 0 && hover_idx < item->num_choices)
        {
            current_idx = hover_idx;
        }
    }

    if (current_idx)
    {
        patch_t *patch =
            V_CachePatchName(crosshair_lumps[current_idx], PU_CACHE);

        int x = ctx->x + 105 - SHORT(patch->width) / 2;
        int y = ctx->y + MN_SPC_CHOICE / 2 - SHORT(patch->height) / 2 - 1;

        V_DrawPatchTranslated(x, y, patch, xlat[hud_crosshair_color].table);
    }
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
    int x = ctx->x + MN_GetPixelWidth(item->label);
    int y = ctx->y;
    if (item->height > MN_SPC)
    {
        y += (item->height - MN_SPC) / 2;
    }
    MN_DrawStringCR(x, y, xlat[CR_NONE].table, NULL, " . . .");
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
    V_DrawPatchDirect(item->rect.x, item->rect.y, patch);
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

static void AppendText(char *buffer, int size, int *offset, const char *text)
{
    if (!text || *offset >= size - 1)
    {
        return;
    }

    int len = strlen(text);

    if (*offset + len > size - 1)
    {
        len = size - 1 - *offset;
    }

    memcpy(buffer + *offset, text, len);
    *offset += len;
    buffer[*offset] = '\0';
}

static void InputDraw(const mn_item_t *item, const mn_draw_ctx_t *ctx)
{
    const input_t *inputs = M_Input(item->input_id);
    const int count = array_size(inputs);
    char buffer[128];
    int offset = 0;

    buffer[0] = '\0';

    for (int i = 0; i < count; ++i)
    {
        const char *name = NULL;
        char key_name[2] = {0, 0};
        int key = inputs[i].value;

        switch (inputs[i].type)
        {
            case INPUT_KEY:
                if (key >= 33 && key <= 126)
                {
                    // Match the legacy display of shifted key names.
                    switch (key)
                    {
                        case '=':
                            key = '+';
                            break;
                        case ',':
                            key = '<';
                            break;
                        case '.':
                            key = '>';
                            break;
                        case '`':
                            key = '\'';
                            break;
                        default:
                            break;
                    }

                    key_name[0] = key;
                    name = key_name;
                }
                else
                {
                    name = M_GetNameForKey(key);
                }
                break;

            case INPUT_MOUSEB:
                name = M_GetNameForMouseB(key);
                break;

            case INPUT_JOYB:
                name = M_GetPlatformName(key);
                break;

            default:
                break;
        }

        if (!name)
        {
            continue;
        }

        if (offset > 0)
        {
            AppendText(buffer, sizeof(buffer), &offset, " + ");
        }

        AppendText(buffer, sizeof(buffer), &offset, name);
    }

    if (offset == 0)
    {
        M_snprintf(buffer, sizeof(buffer), "NONE");
    }

    DrawValueString(ctx, buffer);
}

static boolean InputRespond(mn_item_t *item, menu_action_t action, int ch)
{
    if (!item->gathering)
    {
        if (action == MENU_CLEAR)
        {
            M_InputReset(item->input_id);
            return true;
        }

        if (action == MENU_ENTER)
        {
            item->gathering = true;
            return true;
        }

        MN_ContainerEndEdit();
        return true;
    }

    if (action == MENU_ESCAPE)
    {
        item->gathering = false;
        MN_ContainerEndEdit();
        return true;
    }

    if (MN_ContainerBindInput(item))
    {
        item->gathering = false;
        MN_ContainerEndEdit();
    }

    return true;
}

static boolean InputClick(mn_item_t *item, int x, int y)
{
    if (!item->gathering)
    {
        return false;
    }

    if (MN_ContainerBindInput(item))
    {
        item->gathering = false;
        MN_ContainerEndEdit();
    }

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

static const mn_item_ops_t title_ops = {TitleDraw, TitleRespond};
static const mn_item_ops_t toggle_ops = {ToggleDraw, ToggleRespond};
static const mn_item_ops_t number_ops = {NumberDraw, NumberRespond};
static const mn_item_ops_t choice_ops = {
    ChoiceDrawCrosshair, ChoiceRespond, .click = ChoiceClick, .hover = ChoiceHover
};
static const mn_item_ops_t slider_ops = {
    SliderDraw, SliderRespond, .drag = SliderDrag
};
static const mn_item_ops_t action_ops = {ActionDraw, ActionRespond};
static const mn_item_ops_t reset_ops = {ResetDraw, ResetRespond};
static const mn_item_ops_t input_ops = {InputDraw, InputRespond,
                                        .click = InputClick};
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

    // Initialize rect dimensions from patch for direct-coords reset button
    if (item->type == MN_ITEM_RESET)
    {
        patch_t *patch = V_CachePatchName("M_BUTT1", PU_CACHE);
        item->rect.x = 301;
        item->rect.y = (SCREENHEIGHT - 15 - 3);
        item->rect.w = SHORT(patch->width);
        item->rect.h = SHORT(patch->height);
    }

    item->disabled = !OptionEnabled(item);
}

void MN_BindMenu(mn_menu_t *menu)
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

static mn_menu_t *all_menus[] = {
    &display_menu,
    &video_menu,
    &audio_menu,
    &equalizer_menu,
    &controls_menu,
    &advanced_gamepad_menu,
    &advanced_gyro_menu,
    &input_bindings_menu,
    &interface_menu,
    &gameplay_menu,
    &options_menu,
};

void MN_OptionsBind(void)
{
    for (int i = 0; i < arrlen(all_menus); ++i)
    {
        MN_BindMenu(all_menus[i]);
    }
}

void MN_OptionsRefresh(void)
{
    for (int i = 0; i < arrlen(all_menus); ++i)
    {
        RefreshMenu(all_menus[i]);
    }
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
    MN_ContainerOpen(&options_menu);
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
#define RS_MENU_STEP 50

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

        val += RS_MENU_STEP;
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
            SCREENHEIGHT * 2 + (resolution_scale - 1) * RS_MENU_STEP;
    }

    if (!dynamic_resolution)
    {
        VX_ResetMaxDist();
    }

    MN_DisableItem(current_video_height <= DRS_MIN_HEIGHT, &video_menu,
                "dynamic_resolution");

    resetneeded = true;
}

void MN_OptionsUpdateFullscreen(void)
{
    toggle_fullscreen = true;
}

void MN_OptionsUpdateFPSLimit(void)
{
    setrefreshneeded = true;
}

void MN_OptionsUpdateFPSLimitItem(void)
{
    MN_DisableItem(!uncapped, &video_menu, "fpslimit");
    G_ClearInput();
    G_UpdateAngleFunctions();
}

void MN_OptionsUpdateFOV(void)
{
    setsizeneeded = true;
}

void MN_OptionsResetVideo(void)
{
    resetneeded = true;
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

void MN_OptionsUpdateAdvancedSoundItems(boolean toggle)
{
    MN_DisableItem(toggle, &audio_menu, "snd_hrtf", "snd_doppler");
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
    MN_DisableItem(!I_OAL_EqualizerInitialized(), &equalizer_menu, "snd_equalizer");

    const boolean condition = !I_OAL_CustomEqualizer();

    MN_DisableItem(condition, &equalizer_menu, "snd_eq_preamp", "snd_eq_low_gain",
                 "snd_eq_low_cutoff", "snd_eq_mid1_gain", "snd_eq_mid1_center",
                 "snd_eq_mid2_gain", "snd_eq_mid2_center", "snd_eq_high_gain",
                 "snd_eq_high_cutoff");
}

static void UpdateGainItems(void)
{
#if defined (HAVE_FLUIDSYNTH)
    MN_DisableItem(auto_gain, &audio_menu, "fl_gain");
#endif
    MN_DisableItem(auto_gain, &audio_menu, "opl_gain");
}

void MN_OptionsResetAutoGain(void)
{
    MN_OptionsRestartMusic();
    UpdateGainItems();
}

typedef enum
{
    FREELOOK_OFF,        // Free look disabled.
    FREELOOK_AUTO_AIM,   // Free look enabled with traditional auto-aiming.
    FREELOOK_DIRECT_AIM, // Free look enabled with direct vertical aiming.
} freelook_mode_t;

static freelook_mode_t freelook_mode;

static void SetItemLimit(const char *name, int min, int max)
{
    default_t *def = M_LookupDefault(name);
    def->limit.min = min;
    def->limit.max = max;
}

void MN_OptionsInitFreeLook(void)
{
    if (strictmode)
    {
        MN_DisableItem(true, &controls_menu, "freelook_mode");
        MN_DisableItem(true, &controls_menu, "freelook_mode");
    }
    if (demorecording || (netgame && !solonet))
    {
        SetItemLimit("freelook_mode", FREELOOK_OFF, FREELOOK_AUTO_AIM);
    }
}

void MN_OptionsUpdateFreeLook(void)
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

void MN_OptionsUpdateFreeLookMode(void)
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

    MN_OptionsUpdateFreeLook();
}

void MN_GetGamepadDeviceStrings(mn_item_t *item)
{
    item->choices = I_GamepadDeviceList();
    item->num_choices = array_size(item->choices);
}

void MN_OptionsUpdateGamepadDevice(void)
{
    I_UpdateGamepadDevice(menu_input == pad_mode);
}

static void UpdateGamepadItems(void)
{
    const boolean devices = (I_GamepadEnabled() && I_GamepadDevices());
    const boolean gamepad = (I_UseGamepad() && devices);
    const boolean gyro = (I_GyroEnabled() && I_GyroSupported());
    const boolean sticks = I_UseStickLayout();
    const boolean flick = (gamepad && sticks && !I_StandardLayout());
    const boolean ramp = (gamepad && sticks && I_RampTimeEnabled());
    const boolean condition = (!gamepad || !sticks);

    MN_DisableItem(!devices, &controls_menu, "joy_device");
    MN_DisableItem(!gamepad, &controls_menu, "Advanced Options");
    MN_DisableItem(!gamepad || !I_RumbleSupported(), &controls_menu, "joy_rumble");
    MN_DisableItem(!gamepad || (!sticks && !gyro) || strictmode, &controls_menu,
        "freelook_mode");
    MN_DisableItem(condition, &controls_menu, "joy_invert_look",
        "joy_movement_inner_deadzone", "joy_camera_inner_deadzone",
        "joy_turn_speed", "joy_look_speed");

    MN_DisableItem(!gamepad, &controls_menu, "joy_stick_layout");
    MN_DisableItem(!flick, &controls_menu, "joy_flick_time");
    MN_DisableItem(condition, &advanced_gamepad_menu, "joy_movement_type",
        "joy_forward_sensitivity", "joy_strafe_sensitivity",
        "joy_outer_turn_speed");
    MN_DisableItem(!ramp, &advanced_gamepad_menu, "joy_outer_ramp_time");
    MN_DisableItem(condition, &advanced_gamepad_menu, "joy_camera_curve");
}

void MN_OptionsUpdateGamepad(void)
{
    UpdateGamepadItems();
    I_ResetGamepad();
}

#define MS_TIME_STRINGS_SIZE (100 + 1)

void MN_GetMsTimeStrings(mn_item_t *item)
{
    static const char *strings[MS_TIME_STRINGS_SIZE];
    char buf[8];

    for (int i = 0; i < MS_TIME_STRINGS_SIZE; ++i)
    {
        M_snprintf(buf, sizeof(buf), "%d ms", i * 10);
        strings[i] = M_StringDuplicate(buf);
    }

    item->choices = strings;
    item->num_choices = MS_TIME_STRINGS_SIZE;
}

void MN_OptionsUpdateRumble(void)
{
    I_UpdateRumbleEnabled();
    I_RumbleMenuFeedback();
}

#define MOVEMENT_SENSITIVITY_STRINGS_SIZE (40 + 1)

void MN_GetMovementSensitivityStrings(mn_item_t *item)
{
    static const char *strings[MOVEMENT_SENSITIVITY_STRINGS_SIZE];
    char buf[8];

    for (int i = 0; i < MOVEMENT_SENSITIVITY_STRINGS_SIZE; i++)
    {
        M_snprintf(buf, sizeof(buf), "%1d.%1d", i / 10, i % 10);
        strings[i] = M_StringDuplicate(buf);
    }

    item->choices = strings;
    item->num_choices = MOVEMENT_SENSITIVITY_STRINGS_SIZE;
}

static void UpdateGyroItems(void)
{
    const boolean gamepad = (I_UseGamepad() && I_GamepadEnabled());
    const boolean gyro = (I_GyroEnabled() && I_GyroSupported());
    const boolean acceleration = (gamepad && gyro && I_GyroAcceleration());
    const boolean condition = (!gamepad || !gyro);

    MN_DisableItem(!gamepad || !I_GyroSupported(), &controls_menu, "gyro_enable");
    MN_DisableItem(condition, &controls_menu, "gyro_button_action",
                   "gyro_stick_action", "gyro_turn_sensitivity",
                   "gyro_look_sensitivity", "Calibrate Gyro");
    MN_DisableItem(condition, &advanced_gyro_menu, "gyro_space",
                   "gyro_acceleration", "gyro_smooth_threshold");
    MN_DisableItem(!acceleration, &advanced_gyro_menu,
                   "gyro_accel_min_threshold", "gyro_accel_max_threshold");
}

void MN_OptionsUpdateGyroAiming(void)
{
    UpdateGamepadItems(); // Update "Gyro Options" and freelook.
    UpdateGyroItems();
    I_SetSensorsEnabled(I_GyroEnabled());
    I_ResetGamepad();
}

#define GYRO_SENS_STRINGS_SIZE (500 + 1)

void MN_GetGyroSensitivityStrings(mn_item_t *item)
{
    static const char *strings[GYRO_SENS_STRINGS_SIZE];
    char buf[8];

    for (int i = 0; i < GYRO_SENS_STRINGS_SIZE; i++)
    {
        M_snprintf(buf, sizeof(buf), "%1d.%1d", i / 10, i % 10);
        strings[i] = M_StringDuplicate(buf);
    }
    
    item->choices = strings;
    item->num_choices = GYRO_SENS_STRINGS_SIZE;
}

#define GYRO_ACCEL_STRINGS_SIZE (200 + 1)

void MN_GetGyroAccelStrings(mn_item_t *item)
{
    static const char *strings[GYRO_ACCEL_STRINGS_SIZE] = {
        "", "", "", "", "", "", "", "", "", "", "Off"
    };
    char buf[8];

    for (int i = 11; i < GYRO_ACCEL_STRINGS_SIZE; i++)
    {
        M_snprintf(buf, sizeof(buf), "%1d.%1d", i / 10, i % 10);
        strings[i] = M_StringDuplicate(buf);
    }
    
    item->choices = strings;
    item->num_choices = GYRO_ACCEL_STRINGS_SIZE;
}

void MN_OptionsUpdateGyroSteadying(void)
{
    I_UpdateGyroSteadying();
    I_ResetGamepad();
}

void MN_OptionsUpdateGyroAcceleration(void)
{
    UpdateGyroItems();
    I_ResetGamepad();
}

// !!! TODO
#if 0
static void DrawGyroCalibration(void)
{
    switch (I_GetGyroCalibrationState())
    {
        case GYRO_CALIBRATION_INACTIVE:
            break;

        case GYRO_CALIBRATION_STARTING:
            //block_input = true;
            DrawNotification("Starting calibration...", CR_GRAY, false);
            I_UpdateGyroCalibrationState();
            if (I_GetGyroCalibrationState() == GYRO_CALIBRATION_ACTIVE)
            {
                M_StartSound(sfx_mnumov);
            }
            break;

        case GYRO_CALIBRATION_ACTIVE:
            DrawNotification("Calibrating, please wait...", CR_GRAY, false);
            I_UpdateGyroCalibrationState();
            if (I_GetGyroCalibrationState() == GYRO_CALIBRATION_COMPLETE)
            {
                M_StartSound(sfx_mnumov);
            }
            break;

        case GYRO_CALIBRATION_COMPLETE:
            DrawNotification("Calibration complete!", CR_GREEN, false);
            I_UpdateGyroCalibrationState();
            if (I_GetGyroCalibrationState() == GYRO_CALIBRATION_INACTIVE)
            {
                M_StartSound(sfx_mnucls);
                //block_input = false;
            }
            break;
    }
}
#endif

char slot_labels[NUM_WS_SLOTS * NUM_WS_WEAPS][WS_BUF_SIZE];

void MN_OptionsUpdateWeaponSlotLabels(void)
{
    const char *keys[NUM_WS_SLOTS];

    switch (WS_Selection())
    {
        case WS_SELECT_DPAD:
            keys[0] = M_GetPlatformName(GAMEPAD_DPAD_UP);
            keys[1] = M_GetPlatformName(GAMEPAD_DPAD_DOWN);
            keys[2] = M_GetPlatformName(GAMEPAD_DPAD_LEFT);
            keys[3] = M_GetPlatformName(GAMEPAD_DPAD_RIGHT);
            break;

        case WS_SELECT_FACE_BUTTONS:
            keys[0] = M_GetPlatformName(GAMEPAD_NORTH);
            keys[1] = M_GetPlatformName(GAMEPAD_SOUTH);
            keys[2] = M_GetPlatformName(GAMEPAD_WEST);
            keys[3] = M_GetPlatformName(GAMEPAD_EAST);
            break;

        default: // WS_SELECT_1234
            keys[0] = "1-Key";
            keys[1] = "2-Key";
            keys[2] = "3-Key";
            keys[3] = "4-Key";
            break;
    }

    const char *pos[NUM_WS_WEAPS] = {"1st", "2nd", "3rd"};
    int num = 0;

    for (int i = 0; i < NUM_WS_SLOTS; i++)
    {
        M_snprintf(slot_labels[num++], WS_BUF_SIZE, "%s %s", keys[i], pos[0]);

        for (int j = 1; j < NUM_WS_WEAPS; j++)
        {
            M_snprintf(slot_labels[num++], WS_BUF_SIZE, "%s", pos[j]);
        }
    }
}

static void UpdateWeaponSlotItems(void)
{
    const boolean condition = !WS_Enabled();

    MN_DisableItem(condition, &controls_menu, "weapon_slots_selection",
                 "weapon_slots_1_1", "weapon_slots_1_2", "weapon_slots_1_3",
                 "weapon_slots_2_1", "weapon_slots_2_2", "weapon_slots_2_3",
                 "weapon_slots_3_1", "weapon_slots_3_2", "weapon_slots_3_3",
                 "weapon_slots_4_1", "weapon_slots_4_2", "weapon_slots_4_3");
}

void MN_OptionsUpdateWeaponSlotActivation(void)
{
    WS_Reset();
    UpdateWeaponSlotItems();
}

void MN_OptionsUpdateWeaponSlotSelection(void)
{
    WS_UpdateSelection();
    WS_Reset();
    MN_OptionsUpdateWeaponSlotLabels();
}

void MN_OptionsUpdateWeaponSlots(void)
{
    WS_UpdateSlots();
    WS_Reset();
}

void MN_GetWeaponSlotStrings(mn_item_t *item)
{
    static const char *vanilla_doom_strings[] = {
        "--", "Chainsaw/Fist", "Pistol", "Shotgun", "Chaingun",
        "Rocket", "Plasma", "BFG", "Chainsaw/Fist", "Shotgun"
    };
    static const char *vanilla_doom2_strings[] = {
        "--", "Chainsaw/Fist", "Pistol", "SSG/Shotgun", "Chaingun",
        "Rocket", "Plasma", "BFG", "Chainsaw/Fist", "SSG/Shotgun"
    };
    static const char *full_doom2_strings[] = {
        "--", "Fist", "Pistol", "Shotgun", "Chaingun",
        "Rocket", "Plasma", "BFG", "Chainsaw", "SSG"
    };

    if (force_complevel == CL_VANILLA || default_complevel == CL_VANILLA)
    {
        item->choices = ALLOW_SSG ? vanilla_doom2_strings : vanilla_doom_strings;
    }
    else
    {
        item->choices = full_doom2_strings;
    }
    item->num_choices = arrlen(full_doom2_strings);
}

void MN_GetScreenSizeStrings(mn_item_t *item)
{
    const char **strings = NULL;

    for (int i = 0; i < 3; ++i)
    {
        array_push(strings, "");
    }
    for (int i = 3; i < 10; ++i)
    {
        char buf[8];
        buf[0] = '\0';
        M_snprintf(buf, sizeof(buf), "%d", i);
        array_push(strings, M_StringDuplicate(buf));
    }

    const char **st_strings = ST_StatusbarList();
    for (int i = 0; i < array_size(st_strings); ++i)
    {
        array_push(strings, st_strings[i]);
    }

    maxscreenblocks = array_size(strings) - 1;
    screenblocks = MIN(screenblocks, maxscreenblocks);

    item->choices = strings;
    item->num_choices = array_size(strings);
}

void MN_OptionsUpdateAllGamepadItems(void)
{
    // !!! TODO selectstrings[str_gamepad_device] = GetGamepadDeviceStrings();
    MN_OptionsUpdateWeaponSlotSelection();
    UpdateGamepadItems();
    UpdateGyroItems();
}

void MN_OptionsViewSize(void)
{
    R_SetViewSize(screenblocks);
}

void MN_OptionsUpdateStatsFormatItem(void)
{
    MN_DisableItem(!hud_level_stats, &interface_menu, "hud_stats_format");
}

void MN_OptionsUpdateCrosshairItems(void)
{
    MN_DisableItem(!hud_crosshair, &interface_menu, "hud_crosshair_health",
                 "hud_crosshair_target", "hud_crosshair_lockon",
                 "hud_crosshair_color");
    MN_DisableItem(
        !(hud_crosshair && hud_crosshair_target == crosstarget_highlight),
        &interface_menu, "hud_crosshair_target_color");

    HU_StartCrosshair();
}

void MN_OptionsUpdateCenteredWeaponItem(void)
{
    MN_DisableItem(!weapon_bobbing_pct, &gameplay_menu, "center_weapon");
}

void MN_BindOptionsVariables(void)
{
    BIND_NUM_MENU(resolution_scale, 0, UL);
    BIND_NUM_MENU(freelook_mode, FREELOOK_OFF, FREELOOK_DIRECT_AIM);
    BIND_BOOL(menu_pause_demos, false,
        "Pause demo loop while menu is open");
    BIND_NUM(menu_backdrop, MENU_BG_DARK, MENU_BG_OFF, MENU_BG_TEXTURE,
        "Menu backdrop (0 = Off; 1 = Dark; 2 = Texture)");
}

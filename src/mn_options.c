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

#include "doomdef.h"
#include "doomstat.h"
#include "g_game.h"
#include "i_oalequalizer.h"
#include "i_sound.h"
#include "i_system.h"
#include "i_video.h"
#include "m_config.h"
#include "m_input.h"
#include "m_misc.h"
#include "mn_container.h"
#include "mn_internal.h"
#include "mn_menu.h"
#include "mn_options.h"
#include "r_main.h"
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

int MN_OptionValue(const mn_item_t *item)
{
    return item->def ? *item->def->location.i : 0;
}

boolean MN_OptionEnabled(const mn_item_t *item)
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

void MN_OptionSetValue(mn_item_t *item, int value)
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

void MN_OptionSliderSetFromMouse(mn_item_t *item, int mouse_x)
{
    const int size = item->slider_size ? item->slider_size : 8;
    const int bar_x = item->draw_value_x + 8;
    const int bar_w = size * 8;
    const int min = ItemMin(item);
    const int max = ItemMax(item);

    if (bar_w <= 0 || max <= min)
    {
        return;
    }

    const int rel = mouse_x - video.deltaw - bar_x;
    const int value = min + rel * (max - min) / bar_w;

    MN_OptionSetValue(item, value);
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

    return xlat[CR_GREEN].table;
}

static void DrawValueString(const mn_draw_ctx_t *ctx, const char *text)
{
    const int width = MN_GetPixelWidth(text);

    MN_DrawStringCR(ctx->value_x + ctx->value_w - width, ctx->y,
                    ValueColor(ctx), NULL, text);
}

static void TitleDraw(const mn_item_t *item, const mn_draw_ctx_t *ctx)
{
    const int width = MN_GetPixelWidth(item->label);

    MN_DrawString((SCREENWIDTH - width) / 2, ctx->y, CR_GOLD, item->label);
}

static boolean TitleRespond(mn_item_t *item, menu_action_t action, int ch)
{
    return false;
}

static void ToggleDraw(const mn_item_t *item, const mn_draw_ctx_t *ctx)
{
    DrawValueString(ctx, MN_OptionValue(item) ? "ON" : "OFF");
}

static boolean ToggleRespond(mn_item_t *item, menu_action_t action, int ch)
{
    if (action != MENU_ENTER)
    {
        return false;
    }

    *item->def->location.i = !MN_OptionValue(item);
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
        M_snprintf(buffer, sizeof(buffer), "%d%%", MN_OptionValue(item));
    }
    else if (item->suffix)
    {
        M_snprintf(buffer, sizeof(buffer), "%d %s", MN_OptionValue(item),
                   item->suffix);
    }
    else
    {
        M_snprintf(buffer, sizeof(buffer), "%d", MN_OptionValue(item));
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
        MN_OptionSetValue(item, atoi(item->gather));
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

static void ChoiceDraw(const mn_item_t *item, const mn_draw_ctx_t *ctx)
{
    const int value = MN_OptionValue(item);

    if (item->choices && value >= 0 && value < item->num_choices)
    {
        DrawValueString(ctx, item->choices[value]);
    }
    else
    {
        char buffer[32];
        M_snprintf(buffer, sizeof(buffer), "%d", value);
        DrawValueString(ctx, buffer);
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

    int value = MN_OptionValue(item);
    const int min = ItemMin(item);
    const int max = ItemMax(item);

    if (action == MENU_RIGHT)
    {
        value = (value >= max) ? min : value + 1;
    }
    else
    {
        value = (value <= min) ? max : value - 1;
    }

    if (*item->def->location.i != value)
    {
        *item->def->location.i = value;
        M_StartSound(sfx_mnusli);
    }

    MN_ContainerItemChanged(item);
    return true;
}

static void DrawThermo(const mn_item_t *item, const mn_draw_ctx_t *ctx)
{
    const int size = item->slider_size ? item->slider_size : 8;
    const int value = MN_OptionValue(item);
    const int min = ItemMin(item);
    const int max = ItemMax(item);
    byte *cr = ctx->disabled                    ? cr_dark
               : (ctx->selected || ctx->active) ? xlat[CR_BRIGHT].table
                                                : NULL;

    int x = ctx->value_x;

    V_DrawPatchTranslated(x, ctx->y, V_CachePatchName("M_THERML", PU_CACHE), cr);
    x += 8;

    patch_t *middle = V_CachePatchName("M_THERMM", PU_CACHE);

    for (int i = 0; i < size; ++i)
    {
        V_DrawPatchTranslated(x, ctx->y, middle, cr);
        x += 8;
    }

    V_DrawPatchTranslated(x, ctx->y, V_CachePatchName("M_THERMR", PU_CACHE), cr);

    if (max > min)
    {
        const int dot = (value - min) * size * 8 / (max - min);

        V_DrawPatchTranslated(ctx->value_x + 8 + dot, ctx->y,
                              V_CachePatchName("M_THERMO", PU_CACHE), cr);
    }
}

static void SliderDraw(const mn_item_t *item, const mn_draw_ctx_t *ctx)
{
    DrawThermo(item, ctx);

    char buffer[32];
    const int value = MN_OptionValue(item);

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
    const int bar_end = ctx->value_x + 8 + size * 8 + 8;
    const int text_w = MN_GetPixelWidth(buffer);

    if (bar_end + 4 + text_w <= ctx->value_x + ctx->value_w)
    {
        DrawValueString(ctx, buffer);
    }
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

    int value = MN_OptionValue(item);

    value += (action == MENU_RIGHT) ? 1 : -1;
    MN_OptionSetValue(item, value);
    return true;
}

static void SliderDrag(mn_item_t *item, int mouse_x)
{
    MN_OptionSliderSetFromMouse(item, mouse_x);
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

static boolean ResetRespond(mn_item_t *item, menu_action_t action, int ch)
{
    if (action == MENU_ENTER)
    {
        MN_ContainerConfirm("Restore defaults? (Y/N)", MN_OptionsResetPage);
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

    M_snprintf(buffer, sizeof(buffer), "%d", MN_OptionValue(item));
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
        MN_OptionSetValue(item, ch - '0');
    }

    return true;
}

static const mn_item_ops_t title_ops =  {TitleDraw,  TitleRespond,  NULL};
static const mn_item_ops_t toggle_ops = {ToggleDraw, ToggleRespond, NULL};
static const mn_item_ops_t number_ops = {NumberDraw, NumberRespond, NULL};
static const mn_item_ops_t choice_ops = {ChoiceDraw, ChoiceRespond, NULL};
static const mn_item_ops_t slider_ops = {SliderDraw, SliderRespond, NULL, SliderDrag};
static const mn_item_ops_t action_ops = {ActionDraw, ActionRespond, NULL};
static const mn_item_ops_t reset_ops =  {ResetDraw,  ResetRespond,  NULL};
static const mn_item_ops_t input_ops =  {InputDraw,  InputRespond,  NULL};
static const mn_item_ops_t weapon_ops = {WeaponDraw, WeaponRespond, NULL};

static void BindItem(mn_item_t *item)
{
    item->gathering = false;
    item->gather_count = 0;
    item->gather[0] = '\0';

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

    if (item->choices && item->num_choices == 0)
    {
        if (item->type == MN_ITEM_CHOICE && item->def && item->def->limit.max != UL)
        {
            item->num_choices = item->def->limit.max + 1;
        }
        else
        {
            while (item->choices[item->num_choices] && item->num_choices < 64)
            {
                item->num_choices++;
            }
        }
    }

    if (item->type == MN_ITEM_SLIDER && item->height == 0)
    {
        item->height = MN_SPC + 5;
    }

    item->disabled = !MN_OptionEnabled(item);
}

void MN_OptionsBind(void)
{
    for (int i = 0; i < options_menu.num_pages; ++i)
    {
        mn_page_t *page = &options_menu.pages[i];

        for (mn_item_t *item = page->items; item->type != MN_ITEM_END; ++item)
        {
            BindItem(item);
        }
    }
}

void MN_OptionsRefresh(void)
{
    for (int i = 0; i < options_menu.num_pages; ++i)
    {
        mn_page_t *page = &options_menu.pages[i];

        for (mn_item_t *item = page->items; item->type != MN_ITEM_END; ++item)
        {
            item->disabled = !MN_OptionEnabled(item);
        }
    }
}

void MN_OptionsResetPage(void)
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

void MN_OptionsOpenClassic(void)
{
    MN_ContainerClose();
    MN_General(0);
}

void MN_OptionsUpdateSfxVolume(void)
{
    S_SetSfxVolume(snd_SfxVolume);
}

void MN_OptionsUpdateMusicVolume(void)
{
    S_SetMusicVolume(snd_MusicVolume);
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

void MN_OptionsEqualizerPreset(void)
{
    I_OAL_EqualizerPreset();
}

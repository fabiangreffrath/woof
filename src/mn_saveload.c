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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "d_event.h"
#include "deh_strings.h"
#include "doomdef.h"
#include "doomkeys.h"
#include "doomstat.h"
#include "doomtype.h"
#include "g_game.h"
#include "g_umapinfo.h"
#include "i_input.h"
#include "m_io.h"
#include "m_json.h"
#include "m_misc.h"
#include "mn_container.h"
#include "mn_font.h"
#include "mn_internal.h"
#include "mn_menu.h"
#include "mn_saveload.h"
#include "mn_snapshot.h"
#include "p_saveg.h"
#include "r_draw.h"
#include "sounds.h"
#include "v_palette.h"
#include "v_patch.h"
#include "v_trans.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

#include "miniz.h"

#define SAVE_ROW_HEIGHT   16
#define SAVE_BORDER_CHARS 24
#define SAVE_BORDER_WIDTH (SAVE_BORDER_CHARS * 8)
#define SAVE_LIST_WIDTH   (SAVE_BORDER_WIDTH + 16)
#define SAVE_BORDER_Y     7

// The slot border patches are drawn at SAVE_BORDER_Y and have a top offset of
// 11, so their graphics sticks out of the slot rect on both sides. Keep that
// much room at the ends of the list, otherwise the first and the last slot
// are clipped when the list is scrolled all the way up or down.
#define SAVE_BORDER_TOP_OFFSET 11
#define SAVE_EDGE_PADDING      (SAVE_BORDER_TOP_OFFSET - SAVE_BORDER_Y)

// Space reserved to the left of the list for the snapshot, the gap between
// the snapshot and the list, and the smallest useful snapshot width.
#define SAVE_PREVIEW_EDGE      8
#define SAVE_PREVIEW_GAP       16
#define SAVE_PREVIEW_MIN_WIDTH (SCREENWIDTH / 4)
#define SAVE_PREVIEW_LIST_X \
    (SAVE_PREVIEW_EDGE + SAVE_PREVIEW_MIN_WIDTH + SAVE_PREVIEW_GAP)
#define SAVE_TIME_HEIGHT 10

static mn_item_t load_items[SAVE_ENTRY_COUNT + 1];
static mn_item_t save_items[SAVE_ENTRY_COUNT + 1];

static char descriptions[SAVE_ENTRY_COUNT][SAVE_STRING_SIZE];
static char times[SAVE_ENTRY_COUNT][32];
static boolean loaded[SAVE_ENTRY_COUNT];
static boolean exists[SAVE_ENTRY_COUNT];

static int preview_entry = -1;
static int delete_entry = -1;

static int cursor_load = SAVE_ENTRY_FIRST;
static int cursor_save = SAVE_ENTRY_FIRST;
static boolean quicksave_used;

static boolean editing;
static boolean edit_modified;
static int edit_entry;
static int edit_char_index;
static char edit_old_string[SAVE_STRING_SIZE];

static void EntryFile(int entry, int *slot, int *page)
{
    if (entry == SAVE_ENTRY_AUTO)
    {
        *slot = AUTOSAVESLOT;
        *page = QUICKSAVEPAGE;
    }
    else if (entry == SAVE_ENTRY_QUICK)
    {
        *slot = 1;
        *page = QUICKSAVEPAGE;
    }
    else
    {
        const int index = entry - SAVE_ENTRY_FIRST;
        *slot = index % SAVE_SLOTS_PER_PAGE;
        *page = index / SAVE_SLOTS_PER_PAGE;
    }
}

// [FG] override savegame name if it already starts with a map identifier
static boolean MN_StartsWithMapIdentifier(char *str)
{
    if (strnlen(str, 8) >= 4 && M_ToUpper(str[0]) == 'E' && isdigit(str[1])
        && M_ToUpper(str[2]) == 'M' && isdigit(str[3]))
    {
        return true;
    }

    if (strnlen(str, 8) >= 5 && M_ToUpper(str[0]) == 'M'
        && M_ToUpper(str[1]) == 'A' && M_ToUpper(str[2]) == 'P'
        && isdigit(str[3]) && isdigit(str[4]))
    {
        return true;
    }

    return false;
}

// [FG] generate a default save slot name when the user saves to an empty slot
void MN_SetDefaultSaveName(char *name, const char *append)
{
    char *maplump = MapName(gameepisode, gamemap);
    int maplumpnum = W_CheckNumForName(maplump);

    if (gamemapinfo && gamemapinfo->label
        && !(gamemapinfo->flags & MI_LabelClear))
    {
        maplump = gamemapinfo->label;
    }

    if (!organize_savefiles)
    {
        char *wadname = M_StringDuplicate(W_WadNameForLump(maplumpnum));
        char *ext = strrchr(wadname, '.');

        if (ext != NULL)
        {
            *ext = '\0';
        }

        if (append)
        {
            M_snprintf(name, SAVE_STRING_SIZE, "%s (%s) (%s)", maplump, wadname,
                       append);
        }
        else
        {
            M_snprintf(name, SAVE_STRING_SIZE, "%s (%s)", maplump, wadname);
        }

        free(wadname);
    }
    else
    {
        if (append)
        {
            M_snprintf(name, SAVE_STRING_SIZE, "%s (%s)", maplump, append);
        }
        else
        {
            M_snprintf(name, SAVE_STRING_SIZE, "%s", maplump);
        }
    }

    M_StringToUpper(name);
}

static void EmptySaveString(char *name, int entry)
{
    if (entry == SAVE_ENTRY_QUICK)
    {
        M_snprintf(name, SAVE_STRING_SIZE, "%s %s", DEH_String(EMPTYSTRING),
                   "(Quick)");
    }
    else if (entry == SAVE_ENTRY_AUTO)
    {
        M_snprintf(name, SAVE_STRING_SIZE, "%s %s", DEH_String(EMPTYSTRING),
                   "(Auto)");
    }
    else
    {
        M_snprintf(name, SAVE_STRING_SIZE, "%s", DEH_String(EMPTYSTRING));
    }
}

static void ReadSaveGameTime(int entry, const char *name)
{
    const time_t mtime = (time_t)M_FileMTime(name);

    if (mtime == 0)
    {
        times[entry][0] = '\0';
        return;
    }

// [FG] suppress the most useless compiler warning ever
#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wformat-y2k"
#endif
    strftime(times[entry], sizeof(times[entry]), "%x %X", localtime(&mtime));
#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
}

// Parses the already-resolved save file `name` and stores its description
// and (optionally) screenshot.
static void ReadSaveGameContents(int entry, boolean read_screenshot)
{
    int slot, page;
    EntryFile(entry, &slot, &page);

    char *name = G_SaveGameName(slot, page);

    if (!M_FileExistsNotDir(name))
    {
        free(name);
        name = G_MBFSaveGameName(slot, page);
    }

    ReadSaveGameTime(entry, name);

    if (read_screenshot)
    {
        MN_ResetSnapshot();
    }

    if (!M_FileExistsNotDir(name))
    {
        EmptySaveString(descriptions[entry], entry);
        exists[entry] = false;
        loaded[entry] = true;
        free(name);
        return;
    }

    // Open file and read content

    int savegamesize = M_ReadFile(name, &save_p);
    savebuffer = save_p;
    free(name);

    if (savegamesize < SAVE_STRING_SIZE)
    {
        EmptySaveString(descriptions[entry], entry);
        exists[entry] = false;
        loaded[entry] = true;
        Z_Free(save_p);
        return;
    }

    // Check for zlib-compressed JSON stream

    unsigned char *decomp_str = NULL;
    mz_ulong decomp_len = (mz_ulong)saveg_read32();

    if (M_CheckStreamLength((int32_t)decomp_len) && M_CheckZlibHeader(save_p))
    {
        decomp_str = malloc((size_t)decomp_len);

        if (decomp_str)
        {
            mz_ulong actual_len = decomp_len;
            int mz_ret = mz_uncompress(
                decomp_str, &actual_len, (const unsigned char *)save_p,
                (mz_ulong)savegamesize - sizeof(int32_t));

            if (mz_ret != MZ_OK || actual_len != decomp_len)
            {
                free(decomp_str);
                decomp_str = NULL;
            }
        }
    }

    // Uncompressed stream

    unsigned char *json_str = decomp_str;
    size_t json_len = (size_t)decomp_len;

    if (json_str == NULL)
    {
        json_str = savebuffer;
        json_len = savegamesize - 1;
    }

    // Check for JSON stream

    json_t *root = NULL;
    if (M_CheckJSONStream(json_str, json_len))
    {
        root = JS_OpenString((char *)json_str, json_len);
    }

    // Parse JSON stream or legacy binary savegame

    if (root)
    {
        const char *savegamestring = JS_GetStringValue(root, "savedescription");

        M_snprintf(descriptions[entry], SAVE_STRING_SIZE, "%s",
                   savegamestring ? savegamestring : DEH_String(EMPTYSTRING));

        if (read_screenshot)
        {
            json_t *snapshot_obj = JS_GetObject(root, "snapshot");
            const char *snapshot = JS_GetString(snapshot_obj);
            const int snapshot_len = JS_GetStringLen(snapshot_obj);
            if (!MN_ReadSnapshot((byte *)snapshot, snapshot_len, true))
            {
                MN_ResetSnapshot();
            }
        }

        JS_CloseOptions(NO_INDEX);
    }
    else
    {
        M_snprintf(descriptions[entry], SAVE_STRING_SIZE, "%s",
                   (char *)savebuffer);

        if (read_screenshot
            && !MN_ReadSnapshot(savebuffer, savegamesize, false))
        {
            MN_ResetSnapshot();
        }
    }

    if (decomp_str)
    {
        free(decomp_str);
    }

    if (savebuffer)
    {
        Z_Free(savebuffer);
        savebuffer = save_p = NULL;
    }

    exists[entry] = true;
    loaded[entry] = true;
}

static void EnsureLoaded(int entry)
{
    if (!loaded[entry])
    {
        ReadSaveGameContents(entry, false);
    }
}

static void LoadGameSlot(int slot, int page, int entry)
{
    char *name = G_SaveGameName(slot, page);
    saveg_compat = saveg_woof510;

    if (!M_FileExistsNotDir(name))
    {
        free(name);
        name = G_MBFSaveGameName(slot, page);
        saveg_compat = saveg_mbf;
    }

    G_LoadGame(name, slot, page, false);
    free(name);

    cursor_save = entry;
    MN_CloseMenu();
}

void MN_LoadQuickSave(void)
{
    LoadGameSlot(1, QUICKSAVEPAGE, SAVE_ENTRY_QUICK);
    M_StartSound(sfx_mnucls);
}

void MN_QuickSave(void)
{
    MN_SetDefaultSaveName(descriptions[SAVE_ENTRY_QUICK], "Quick");
    G_SaveGame(1, QUICKSAVEPAGE, descriptions[SAVE_ENTRY_QUICK]);

    quicksave_used = true;
    cursor_load = SAVE_ENTRY_QUICK;
    loaded[SAVE_ENTRY_QUICK] = false;
    preview_entry = -1;

    MN_CloseMenu();
    M_StartSound(sfx_mnucls);
}

boolean MN_QuickSaveUsed(void)
{
    return quicksave_used;
}

static void SaveGameAtEntry(int entry)
{
    int slot, page;
    EntryFile(entry, &slot, &page);

    G_SaveGame(slot, page, descriptions[entry]);

    cursor_load = entry;
    loaded[entry] = false;
    preview_entry = -1;

    MN_CloseMenu();
}

static void DeleteEntry(void)
{
    if (delete_entry < 0)
    {
        return;
    }

    int slot, page;
    EntryFile(delete_entry, &slot, &page);

    char *name = G_SaveGameName(slot, page);
    M_remove(name);
    free(name);

    G_ClearPendingSaveSlot(slot);

    if (delete_entry == SAVE_ENTRY_QUICK)
    {
        quicksave_used = false;
    }

    loaded[delete_entry] = false;
    preview_entry = -1;
    delete_entry = -1;
}

static void DeletePrompt(int entry)
{
    delete_entry = entry;
    MN_ContainerConfirm("Delete savegame? (Y/N)", DeleteEntry);
}

static boolean DigitJump(int base, int ch)
{
    mn_page_t *page = MN_ContainerCurrentPage();

    if (!page || ch < '1' || ch > '8')
    {
        return false;
    }

    const int regular = MAX(0, page->selected + base - SAVE_ENTRY_FIRST);
    const int block = regular / SAVE_SLOTS_PER_PAGE;
    const int index = SAVE_ENTRY_FIRST + block * SAVE_SLOTS_PER_PAGE
                      + (ch - '1') - base;

    if (index < 0 || index >= page->num_items)
    {
        return false;
    }

    page->selected = index;
    MN_ContainerEnsureVisible(&page->items[index]);
    M_StartSound(sfx_mnumov);
    return true;
}

static void StartEdit(int entry)
{
    if (menu_input == pad_mode)
    {
        MN_SetDefaultSaveName(descriptions[entry], NULL);
        SaveGameAtEntry(entry);
        M_StartSound(sfx_mnuact);
        return;
    }

    editing = true;
    edit_modified = false;
    edit_entry = entry;
    M_StringCopy(edit_old_string, descriptions[entry], SAVE_STRING_SIZE);

    if (!strcmp(descriptions[entry], DEH_String(EMPTYSTRING))
        || MN_StartsWithMapIdentifier(descriptions[entry]))
    {
        descriptions[entry][0] = 0;
        MN_SetDefaultSaveName(descriptions[entry], NULL);
    }

    edit_char_index = strlen(descriptions[entry]);

    I_StartTextInput();

    cursor_load = entry;

    M_StartSound(sfx_mnuact);
}

static void SlotDraw(const mn_item_t *item, const mn_draw_ctx_t *ctx,
                     int entry)
{
    EnsureLoaded(entry);

    const int x = ctx->x;
    const int y = ctx->y;
    byte *cr = ctx->selected ? xlat[CR_BRIGHT].table : NULL;

    V_DrawPatchTranslated(x, y + SAVE_BORDER_Y,
                          V_CachePatchName("M_LSLEFT", PU_CACHE), cr);

    for (int i = 0; i < SAVE_BORDER_CHARS; ++i)
    {
        V_DrawPatchTranslated(x + 8 + i * 8, y + SAVE_BORDER_Y,
                              V_CachePatchName("M_LSCNTR", PU_CACHE), cr);
    }

    V_DrawPatchTranslated(x + 8 + SAVE_BORDER_WIDTH, y + SAVE_BORDER_Y,
                          V_CachePatchName("M_LSRGHT", PU_CACHE), cr);

    byte *text_cr = (entry == SAVE_ENTRY_QUICK && quicksave_used)
                        ? xlat[CR_GOLD].table
                        : xlat[CR_NONE].table;

    MN_DrawStringCR(ctx->x + 8, y, text_cr, NULL, descriptions[entry]);

    if (editing && edit_entry == entry)
    {
        const int width = MN_GetPixelWidth(descriptions[entry]);

        MN_DrawString(ctx->x + 8 + width, y, CR_NONE, "_");
    }
}

static void SlotPreview(const mn_menu_t *menu, const mn_page_t *page, int base)
{
    const int index = MN_ContainerHighlightedIndex(menu);

    if (index < 0 || index >= page->num_items)
    {
        return;
    }

    const mn_item_t *item = &page->items[index];
    const int entry = index + base;

    if (entry < 0 || entry >= SAVE_ENTRY_COUNT)
    {
        return;
    }

    EnsureLoaded(entry);

    if (preview_entry != entry)
    {
        ReadSaveGameContents(entry, true);
        preview_entry = entry;
    }

    const int list_x = menu->viewport.x + video.deltaw;
    int width = (list_x - SAVE_PREVIEW_EDGE - SAVE_PREVIEW_GAP) & ~7;

    width = MIN(width, SCREENWIDTH / 2);

    // [FG] a snapshot window smaller than 80*48 px is considered too small
    if (width < SAVE_PREVIEW_MIN_WIDTH)
    {
        return;
    }

    int height = (width * SCREENHEIGHT / SCREENWIDTH) & ~7;

    height = MIN(height, SCREENHEIGHT / 2);

    const int x = MAX(SAVE_PREVIEW_EDGE, list_x - SAVE_PREVIEW_GAP - width);
    const int row_y = menu->viewport.y + item->rect.y - page->scroll;

    int y = row_y + (item->height - height) / 2;

    y = CLAMP(y, menu->viewport.y,
              menu->viewport.y + menu->viewport.h - height - SAVE_TIME_HEIGHT);

    const boolean draw_shot = MN_DrawSnapshot(x, y, width, height);

    const char *txt = !draw_shot ? "N/A"
                      : (entry == SAVE_ENTRY_AUTO)
                          ? "Auto Save"
                          : (entry == SAVE_ENTRY_QUICK) ? "Quick Save" : NULL;

    if (txt)
    {
        MN_DrawString(x + (width - MN_StringWidth(txt)) / 2 - video.deltaw,
                      y + height / 2 - MN_StringHeight(txt) / 2, CR_NONE, txt);
    }

    txt = times[entry];

    MN_DrawString(x + width / 2 - MN_GetPixelWidth(txt) / 2 - video.deltaw,
                  y + height + MN_StringHeight(txt), CR_GOLD, txt);

    // [FG] draw the view border around the snapshot
    R_DrawBorder(x, y, width, height);
}

static void LoadSlotDraw(const mn_item_t *item, const mn_draw_ctx_t *ctx)
{
    SlotDraw(item, ctx, item - load_items);
}

static void SaveSlotDraw(const mn_item_t *item, const mn_draw_ctx_t *ctx)
{
    SlotDraw(item, ctx, item - save_items);
}

static void LoadPreviewDraw(const mn_menu_t *menu, const mn_page_t *page)
{
    SlotPreview(menu, page, 0);
}

static void SavePreviewDraw(const mn_menu_t *menu, const mn_page_t *page)
{
    SlotPreview(menu, page, SAVE_ENTRY_FIRST);
}

static boolean LoadSlotRespond(mn_item_t *item, menu_action_t action, int ch)
{
    const int entry = item - load_items;

    EnsureLoaded(entry);

    if (action == MENU_ENTER)
    {
        if (!exists[entry])
        {
            M_StartSound(sfx_mnuerr);
            return true;
        }

        int slot, page;
        EntryFile(entry, &slot, &page);

        LoadGameSlot(slot, page, entry);
        M_StartSound(sfx_mnuact);
        return true;
    }

    if (action == MENU_CLEAR)
    {
        if (exists[entry])
        {
            DeletePrompt(entry);
        }
        else
        {
            M_StartSound(sfx_mnuerr);
        }
        return true;
    }

    return DigitJump(0, ch);
}

static boolean SaveSlotRespond(mn_item_t *item, menu_action_t action, int ch)
{
    const int entry = item - save_items;

    EnsureLoaded(entry);

    if (action == MENU_ENTER)
    {
        StartEdit(entry);
        return true;
    }

    if (action == MENU_CLEAR)
    {
        if (exists[entry])
        {
            DeletePrompt(entry);
        }
        else
        {
            M_StartSound(sfx_mnuerr);
        }
        return true;
    }

    return DigitJump(SAVE_ENTRY_FIRST, ch);
}

static const mn_item_ops_t load_slot_ops = {LoadSlotDraw, LoadSlotRespond};
static const mn_item_ops_t save_slot_ops = {SaveSlotDraw, SaveSlotRespond};

static void InitItems(void)
{
    static boolean init;

    if (init)
    {
        return;
    }

    for (int i = 0; i < SAVE_ENTRY_COUNT; ++i)
    {
        load_items[i] = (mn_item_t){.label = "",
                                    .type = MN_ITEM_SAVESLOT,
                                    .height = SAVE_ROW_HEIGHT,
                                    .ops = &load_slot_ops};

        save_items[i] = (mn_item_t){
            .label = (i < SAVE_ENTRY_FIRST) ? NULL : "",
            .type = (i < SAVE_ENTRY_FIRST) ? MN_ITEM_SKIP : MN_ITEM_SAVESLOT,
            .height = SAVE_ROW_HEIGHT,
            .ops = (i < SAVE_ENTRY_FIRST) ? NULL : &save_slot_ops,
        };
    }

    load_items[SAVE_ENTRY_COUNT] = (mn_item_t){.type = MN_ITEM_END};
    save_items[SAVE_ENTRY_COUNT] = (mn_item_t){.type = MN_ITEM_END};

    init = true;
}

static mn_page_t load_page = {
    .items = load_items,
    .draw = LoadPreviewDraw,
    .pad_top = SAVE_EDGE_PADDING,
    .pad_bottom = SAVE_EDGE_PADDING,
};

static mn_page_t save_page = {
    .items = &save_items[SAVE_ENTRY_FIRST],
    .draw = SavePreviewDraw,
    .pad_top = SAVE_EDGE_PADDING,
    .pad_bottom = SAVE_EDGE_PADDING,
};

static mn_menu_t load_menu = {
    .title_patch = "M_LOADG",
    .title_text = "Load Game",
    .layout = {.max_width = MN_PX(SAVE_LIST_WIDTH)},
    .pages = &load_page,
    .num_pages = 1,
};

static mn_menu_t save_menu = {
    .title_patch = "M_SAVEG",
    .title_text = "Save Game",
    .layout = {.max_width = MN_PX(SAVE_LIST_WIDTH)},
    .pages = &save_page,
    .num_pages = 1,
};

void MN_SaveLoadOpen(boolean save)
{
    InitItems();

    for (int i = 0; i < SAVE_ENTRY_COUNT; ++i)
    {
        loaded[i] = false;
    }

    preview_entry = -1;
    editing = false;
    delete_entry = -1;

    mn_menu_t *menu = save ? &save_menu : &load_menu;
    const int avail = video.unscaledw > 0 ? video.unscaledw : SCREENWIDTH;
    int left = (avail - SAVE_LIST_WIDTH) / 2;

    // Shift the list right on widescreen displays to make room for the
    // snapshot panel.
    if (video.deltaw > 0)
    {
        left = MAX(left, SAVE_PREVIEW_LIST_X);
    }

    menu->layout.margin_left = (mn_length_t)MN_PX(left);
    menu->layout.margin_right = (mn_length_t)MN_PX(0);

    MN_ContainerOpen(menu);

    mn_page_t *page = MN_ContainerCurrentPage();
    const int index = (save ? cursor_save : cursor_load) - (save ? SAVE_ENTRY_FIRST : 0);

    if (page && index >= 0 && index < page->num_items)
    {
        page->selected = index;
        MN_ContainerEnsureVisible(&page->items[index]);
    }
}

boolean MN_SaveLoadTextInput(const event_t *ev, menu_action_t action, int ch)
{
    if (!editing)
    {
        return false;
    }

    char *description = descriptions[edit_entry];

    if (ch == KEY_BACKSPACE)
    {
        if (edit_char_index > 0)
        {
            if (MN_StartsWithMapIdentifier(description) && !edit_modified)
            {
                edit_char_index = 0;
            }
            else
            {
                edit_char_index--;
            }

            description[edit_char_index] = 0;
        }
    }
    else if (action == MENU_ESCAPE)
    {
        I_StopTextInput();
        editing = false;
        M_StringCopy(description, edit_old_string, SAVE_STRING_SIZE);
    }
    else if (action == MENU_ENTER)
    {
        I_StopTextInput();
        editing = false;

        if (description[0])
        {
            SaveGameAtEntry(edit_entry);
        }
    }
    else if (ev->type == ev_text)
    {
        int txt = ev->data1.i;

        txt = M_ToUpper(txt);

        if (txt >= ' ' && txt <= '_' && edit_char_index < SAVE_STRING_SIZE - 1
            && MN_StringWidth(description) < (SAVE_STRING_SIZE - 2) * 8)
        {
            description[edit_char_index++] = txt;
            description[edit_char_index] = 0;
        }

        edit_modified = true;
    }

    return true;
}

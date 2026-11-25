//
// Copyright(C) 2024 Roman Fomin
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

#include "st_widgets.h"

#include <math.h>
#include <string.h>

#include "d_event.h"
#include "d_player.h"
#include "deh_strings.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "g_game.h"
#include "g_skillinfo.h"
#include "g_umapinfo.h"
#include "hu_chat.h"
#include "hu_command.h"
#include "hu_coordinates.h"
#include "hu_message.h"
#include "hu_obituary.h"
#include "i_timer.h"
#include "i_video.h"
#include "m_array.h"
#include "m_config.h"
#include "m_misc.h"
#include "mn_menu.h"
#include "p_mobj.h"
#include "p_spec.h"
#include "r_main.h"
#include "r_voxel.h"
#include "st_sbardef.h"
#include "st_stuff.h"
#include "v_video.h"

boolean       show_messages;
boolean       show_toggle_messages;
boolean       show_pickup_messages;

secretmessage_t hud_secret_message; // "A secret is revealed!" message
widgetstate_t hud_level_stats;
widgetstate_t hud_level_time;
boolean       hud_time_use;
widgetstate_t hud_player_coords;
widgetstate_t hud_widget_font;

static boolean hud_map_announce;
boolean message_colorized;

//jff 2/16/98 hud supported automap colors added
int hudcolor_titl;  // color range of automap level title
int hudcolor_xyco;  // color range of new coords on automap

void ST_ClearLines(sbe_widget_t *widget)
{
    array_clear(widget->lines);
}

void ST_AddLine(sbe_widget_t *widget, const char *string)
{
    stringline_t line = { .string = string };
    array_push(widget->lines, line);
}

static void SetLine(sbe_widget_t *widget, const char *string)
{
    array_clear(widget->lines);
    stringline_t line = { .string = string };
    array_push(widget->lines, line);
}

static int message_duration_left;

static boolean message_review;

static void UpdateMessage(sbe_widget_t *widget)
{
    ST_ClearLines(widget);

    static char string[HU_MAXLINELENGTH];

    // A game message replaces the one on screen and restarts its time. Only
    // feedback about the local player (chat echo, the message settings)
    // holds the following game messages back until it expires.

    static boolean overwrite = true;
    static boolean messages_enabled = true;
    char text[HU_MAXLINELENGTH];

    if (HU_PopPostedMessage(text, sizeof(text)))
    {
        M_StringCopy(string, text, sizeof(string));
        message_duration_left = widget->duration;
        overwrite = false;
    }
    else if (messages_enabled && overwrite && HU_PopMessage(text, sizeof(text)))
    {
        M_StringCopy(string, text, sizeof(string));
        message_duration_left = widget->duration;
    }
    else if (messages_enabled && message_review)
    {
        message_review = false;
        message_duration_left = widget->duration;
    }

    if (messages_enabled != show_messages)
    {
        messages_enabled = show_messages;
    }

    if (message_duration_left == 0)
    {
        overwrite = true;
        return;
    }

    ST_AddLine(widget, string);
    --message_duration_left;
}

void ST_ReviewMessages(void)
{
    message_review = true;
}

static char announce_string[HU_MAXLINELENGTH], author_string[HU_MAXLINELENGTH];
static int announce_duration_left;
static boolean announce_secret;

static void UpdateAnnounceMessage(sbe_widget_t *widget)
{
    ST_ClearLines(widget);

    static char string[HU_MAXLINELENGTH];
    char text[HU_MAXLINELENGTH];

    if (announce_string[0])
    {
        if (!hud_map_announce)
        {
            announce_string[0] = '\0';
            author_string[0] = '\0';
            return;
        }

        M_StringCopy(string, announce_string, sizeof(string));
        announce_string[0] = '\0';
        announce_secret = false;
        announce_duration_left = widget->duration;
    }
    else if (HU_PopAnnouncement(text, sizeof(text)))
    {
        M_snprintf(string, sizeof(string), "%s%s%s", xlat[CR_GOLD].str, text,
                   ORIG_S);
        announce_secret = true;
        announce_duration_left = widget->duration;
    }
    else if (announce_duration_left == 0)
    {
        return;
    }

    // Turning the setting off stops the announcement of a revealed secret,
    // but not the level title.

    if (announce_secret && !hud_secret_message)
    {
        announce_duration_left = 0;
        return;
    }

    if (announce_duration_left > 0)
    {
        ST_AddLine(widget, string);
        if (!announce_secret && author_string[0])
        {
            ST_AddLine(widget, author_string);
        }
        --announce_duration_left;
    }
}

void ST_ResetMessages(void)
{
    message_duration_left = 0;
    message_review = false;
    announce_duration_left = 0;
    announce_secret = false;
    announce_string[0] = '\0';
    author_string[0] = '\0';

    HU_ResetMessages();
    HU_ResetChat();
}

static char title_string[HU_MAXLINELENGTH];

void ST_ResetTitle(void)
{
    char string[120];
    string[0] = '\0';

    const char *s = MI_GetLevelTitle();

    char *n;

    // [FG] cap at line break
    if ((n = strchr(s, '\n')))
    {
        *n = '\0';
    }

    M_StringConcat(string, s, sizeof(string));

    title_string[0] = '\0';
    M_snprintf(title_string, sizeof(title_string), "\x1b%c%s" ORIG_S,
               '0' + hudcolor_titl, string);

    announce_string[0] = '\0';
    author_string[0] = '\0';
    if (hud_map_announce && leveltime == 0)
    {
        MI_MapAnnouncement(announce_string, author_string, string, sizeof(string));
    }
}

static void UpdateTitle(sbe_widget_t *widget)
{
    SetLine(widget, title_string);
}

static boolean WidgetEnabled(widgetstate_t state)
{
    if (automapactive && !(state & HUD_WIDGET_AUTOMAP))
    {
        return false;
    }
    else if (!automapactive && !(state & HUD_WIDGET_HUD))
    {
        return false;
    }
    return true;
}

static void ForceDoomFont(sbe_widget_t *widget)
{
    if (WidgetEnabled(hud_widget_font))
    {
        widget->font = stcfnt;
    }
    else
    {
        widget->font = widget->default_font;
    }
}

static void UpdateCoord(sbe_widget_t *widget, player_t *player)
{
    ST_ClearLines(widget);

    if (strictmode)
    {
        return;
    }

    if (hud_player_coords == HUD_WIDGET_ADVANCED)
    {
        HU_BuildCoordinatesEx(widget, player->mo);
        return;
    }

    if (!WidgetEnabled(hud_player_coords))
    {
        return;
    }

    ForceDoomFont(widget);

    fixed_t x, y, z; // killough 10/98:
    void AM_Coordinates(const mobj_t *, fixed_t *, fixed_t *, fixed_t *);

    // killough 10/98: allow coordinates to display non-following pointer
    AM_Coordinates(player->mo, &x, &y, &z);

    if (!widget->vertical)
    {
        static char string[80];
        // jff 2/16/98 output new coord display
        M_snprintf(string, sizeof(string),
                   "\x1b%cX %s%d \x1b%cY %s%d \x1b%cZ %s%d",
                   '0' + hudcolor_xyco, xlat[CR_GRAY].str, x >> FRACBITS,
                   '0' + hudcolor_xyco, xlat[CR_GRAY].str, y >> FRACBITS,
                   '0' + hudcolor_xyco, xlat[CR_GRAY].str, z >> FRACBITS);
        ST_AddLine(widget, string);
    }
    else
    {
        static char string1[16];
        M_snprintf(string1, sizeof(string1), "\x1b%cX %s%d",
                   '0' + hudcolor_xyco, xlat[CR_GRAY].str, x >> FRACBITS);
        ST_AddLine(widget, string1);
        static char string2[16];
        M_snprintf(string2, sizeof(string2), "\x1b%cY %s%d",
                   '0' + hudcolor_xyco, xlat[CR_GRAY].str, y >> FRACBITS);
        ST_AddLine(widget, string2);
        static char string3[16];
        M_snprintf(string3, sizeof(string3), "\x1b%cZ %s%d",
                   '0' + hudcolor_xyco, xlat[CR_GRAY].str, z >> FRACBITS);
        ST_AddLine(widget, string3);
    }
}

typedef enum
{
    STATSFORMAT_RATIO,
    STATSFORMAT_BOOLEAN,
    STATSFORMAT_PERCENT,
    STATSFORMAT_REMAINING,
    STATSFORMAT_COUNT
} statsformat_t;

static statsformat_t hud_stats_format;

static void StatsFormatFunc_Ratio(char *buffer, size_t size, const int count,
                                  const int total)
{
    M_snprintf(buffer, size, "%d/%d", count, total);
}

static void StatsFormatFunc_Boolean(char *buffer, size_t size, const int count,
                                    const int total)
{
    M_snprintf(buffer, size, "%s", (count >= total) ? "YES" : "NO");
}

static void StatsFormatFunc_Percent(char *buffer, size_t size, const int count,
                                    const int total)
{
    M_snprintf(buffer, size, "%d%%", !total ? 100 : count * 100 / total);
}

static void StatsFormatFunc_Remaining(char *buffer, size_t size,
                                      const int count, const int total)
{
    M_snprintf(buffer, size, "%d", total - count);
}

static void StatsFormatFunc_Count(char *buffer, size_t size, const int count,
                                  const int total)
{
    M_snprintf(buffer, size, "%d", count);
}

typedef void (*statsformatfunc_t)(char *buffer, size_t size, const int count,
                                  const int total);

static const statsformatfunc_t StatsFormatFuncs[] = {
    StatsFormatFunc_Ratio,     StatsFormatFunc_Boolean, StatsFormatFunc_Percent,
    StatsFormatFunc_Remaining, StatsFormatFunc_Count,
};

static char killchar = 'K';
static char statscolor = '0' + CR_RED;

static void UpdateMonSec(sbe_widget_t *widget)
{
    ST_ClearLines(widget);

    if (!WidgetEnabled(hud_level_stats))
    {
        return;
    }

    ForceDoomFont(widget);

    const int cr_blue = (widget->font == stcfnt) ? CR_BLUE2 : CR_BLUE1;

    int fullkillcount = 0;
    int fullitemcount = 0;
    int fullsecretcount = 0;
    int kill_percent_count = 0;

    for (int i = 0; i < MAXPLAYERS; ++i)
    {
        if (playeringame[i])
        {
            fullkillcount += players[i].killcount - players[i].maxkilldiscount;
            fullitemcount += players[i].itemcount;
            fullsecretcount += players[i].secretcount;
            kill_percent_count += players[i].killcount;
        }
    }

    if (skill_info.respawn_time)
    {
        fullkillcount = kill_percent_count;
        max_kill_requirement = totalkills;
    }

    int killcolor = (fullkillcount >= max_kill_requirement) ? '0' + cr_blue
                                                            : '0' + CR_GRAY;
    int secretcolor =
        (fullsecretcount >= totalsecret) ? '0' + cr_blue : '0' + CR_GRAY;
    int itemcolor =
        (fullitemcount >= totalitems) ? '0' + cr_blue : '0' + CR_GRAY;

    char kill_str[80], item_str[80], secret_str[80];

    statsformatfunc_t StatsFormatFunc = StatsFormatFuncs[hud_stats_format];

    StatsFormatFunc(kill_str, sizeof(kill_str), fullkillcount, max_kill_requirement);
    StatsFormatFunc(item_str, sizeof(item_str), fullitemcount, totalitems);
    StatsFormatFunc(secret_str, sizeof(secret_str), fullsecretcount, totalsecret);

    if (!widget->vertical)
    {
        static char string[120];
        M_snprintf(string, sizeof(string),
            "\x1b%c%c \x1b%c%s \x1b%cI \x1b%c%s \x1b%cS \x1b%c%s",
            statscolor, killchar, killcolor, kill_str,
            statscolor, itemcolor, item_str,
            statscolor, secretcolor, secret_str);
        ST_AddLine(widget, string);
    }
    else
    {
        static char string1[80];
        M_snprintf(string1, sizeof(string1), "\x1b%c%c \x1b%c%s", statscolor, killchar, killcolor, kill_str);
        ST_AddLine(widget, string1);
        static char string2[80];
        M_snprintf(string2, sizeof(string2), "\x1b%cI \x1b%c%s", statscolor, itemcolor, item_str);
        ST_AddLine(widget, string2);
        static char string3[80];
        M_snprintf(string3, sizeof(string3), "\x1b%cS \x1b%c%s", statscolor, secretcolor, secret_str);
        ST_AddLine(widget, string3);
    }
}

static void UpdateDM(sbe_widget_t *widget)
{
    ST_ClearLines(widget);

    if (!WidgetEnabled(hud_level_stats))
    {
        return;
    }

    ForceDoomFont(widget);

    static char string[120];

    const int cr_blue = (widget->font == stcfnt) ? CR_BLUE2 : CR_BLUE1;

    int offset = 0;

    for (int i = 0; i < MAXPLAYERS; ++i)
    {
        int result = 0, others = 0;

        if (!playeringame[i])
        {
            continue;
        }

        for (int p = 0; p < MAXPLAYERS; ++p)
        {
            if (!playeringame[p])
            {
                continue;
            }

            if (i != p)
            {
                result += players[i].frags[p];
                others -= players[p].frags[i];
            }
            else
            {
                result -= players[i].frags[p];
            }
        }

        offset += M_snprintf(string + offset, sizeof(string) - offset,
                             "\x1b%c%02d/%02d ", (i == displayplayer) ?
                             '0' + cr_blue : '0' + CR_GRAY, result, others);
    }

    ST_AddLine(widget, string);
}

static void UpdateStTime(sbe_widget_t *widget, player_t *player)
{
    ST_ClearLines(widget);

    if (!WidgetEnabled(hud_level_time) && !player->btuse_tics)
    {
        return;
    }

    ForceDoomFont(widget);

    static char string[80];

    int offset = 0;

    if (WidgetEnabled(hud_level_time))
    {
        if (time_scale != 100)
        {
            offset += M_snprintf(string, sizeof(string), "%s%d%% ",
                                 (widget->font == stcfnt) ? xlat[CR_BLUE2].str
                                                          : xlat[CR_BLUE1].str,
                                 time_scale);
        }

        if (levelTimer == true)
        {
            const int time = levelTimeCount / TICRATE;

            offset += M_snprintf(string + offset, sizeof(string) - offset,
                                 "%s%d:%02d ", xlat[CR_BROWN].str, time / 60,
                                 time % 60);
        }
        else if (totalleveltimes)
        {
            const int time = (totalleveltimes + leveltime) / TICRATE;

            offset += M_snprintf(string + offset, sizeof(string) - offset,
                                 "%s%d:%02d ", xlat[CR_GREEN].str, time / 60,
                                 time % 60);
        }
    }

    if (player->btuse_tics)
    {
        M_snprintf(string + offset, sizeof(string) - offset, "%sU %d:%05.2f\t",
                   xlat[CR_GOLD].str, player->btuse / TICRATE / 60,
                   (float)(player->btuse % (60 * TICRATE)) / TICRATE);
    }
    else
    {
        M_snprintf(string + offset, sizeof(string) - offset, "%s%d:%05.2f\t",
                   xlat[CR_GRAY].str, leveltime / TICRATE / 60,
                   (float)(leveltime % (60 * TICRATE)) / TICRATE);
    }

    ST_AddLine(widget, string);
}

static void UpdateFPS(sbe_widget_t *widget, player_t *player)
{
    ST_ClearLines(widget);

    if (!(player->cheats & CF_SHOWFPS))
    {
        return;
    }

    ForceDoomFont(widget);

    static char string[20];
    M_snprintf(string, sizeof(string), "%s%d %sFPS", xlat[CR_GRAY].str, fps,
               xlat[CR_GREEN].str);
    ST_AddLine(widget, string);
}

static void UpdateRate(sbe_widget_t *widget, player_t *player)
{
    ST_ClearLines(widget);

    if (!(player->cheats & CF_RENDERSTATS))
    {
        return;
    }

    static char line1[80];
    M_snprintf(line1, sizeof(line1),
               "%sSprites %4d Segs %4d Visplanes %4d   "
               "%sFPS %3d %dx%d",
               xlat[CR_GRAY].str, rendered_vissprites, rendered_segs,
               rendered_visplanes, xlat[CR_GREEN].str, fps, video.width,
               video.height);
    ST_AddLine(widget, line1);

    if (voxels_rendering)
    {
        static char line2[60];
        M_snprintf(line2, sizeof(line2), "%s Voxels %4d", xlat[CR_GRAY].str,
                   rendered_voxels);
        ST_AddLine(widget, line2);
    }
}

int speedometer;

static void UpdateSpeed(sbe_widget_t *widget, player_t *player)
{
    if (speedometer <= 0)
    {
        ST_ClearLines(widget);
        return;
    }

    ForceDoomFont(widget);

    static const double factor[] = {TICRATE, 2.4003, 525.0 / 352.0};
    static const char *units[] = {"ups", "km/h", "mph"};
    const int type = speedometer - 1;
    const mobj_t *mo = player->mo;
    const double dx = FixedToDouble(mo->x - mo->oldx);
    const double dy = FixedToDouble(mo->y - mo->oldy);
    const double dz = FixedToDouble(mo->z - mo->oldz);
    const double speed = sqrt(dx * dx + dy * dy + dz * dz) * factor[type];

    static char string[60];
    M_snprintf(string, sizeof(string), "%s%.*f %s%s", xlat[CR_GRAY].str,
               type && speed ? 1 : 0, speed, xlat[CR_GREEN].str, units[type]);
    SetLine(widget, string);
}

static void UpdateCmd(sbe_widget_t *widget)
{
    ST_ClearLines(widget);

    if (!STRICTMODE(hud_command_history))
    {
        return;
    }

    HU_BuildCommandHistory(widget);
}

// [crispy] print a bar indicating demo progress at the bottom of the screen
boolean ST_DemoProgressBar(boolean force)
{
    const int progress = video.unscaledw * playback_tic / playback_totaltics;
    static int old_progress = 0;

    if (old_progress < progress)
    {
        old_progress = progress;
    }
    else if (!force)
    {
        return false;
    }

    V_FillRect(0, SCREENHEIGHT - 2, progress, 1, playpal_global->black);
    V_FillRect(0, SCREENHEIGHT - 1, progress, 1, playpal_global->white);

    return true;
}

static void ColorizeString(const char *haystack, const char *needle, xlat_index_t cr)
{
    char replacement[18];
    M_snprintf(replacement, sizeof(replacement), "%s%s%s", xlat[cr].str, needle, ORIG_S);
    char * colorized = M_StringReplaceWord(DEH_String(haystack), needle, replacement);
    DEH_AddStringColorizedReplacement(haystack, colorized);
    free(colorized);
}

void ST_InitWidgets(void)
{
    // [Woof!] colorize keycard and skull key messages
    ColorizeString(GOTBLUECARD, "blue",   CR_BLUE2);
    ColorizeString(GOTBLUESKUL, "blue",   CR_BLUE2);
    ColorizeString(GOTREDCARD,  "red",    CR_RED);
    ColorizeString(GOTREDSKULL, "red",    CR_RED);
    ColorizeString(GOTYELWCARD, "yellow", CR_GOLD);
    ColorizeString(GOTYELWSKUL, "yellow", CR_GOLD);
    ColorizeString(PD_BLUEC,    "blue",   CR_BLUE2);
    ColorizeString(PD_BLUEK,    "blue",   CR_BLUE2);
    ColorizeString(PD_BLUEO,    "blue",   CR_BLUE2);
    ColorizeString(PD_BLUES,    "blue",   CR_BLUE2);
    ColorizeString(PD_REDC,     "red",    CR_RED);
    ColorizeString(PD_REDK,     "red",    CR_RED);
    ColorizeString(PD_REDO,     "red",    CR_RED);
    ColorizeString(PD_REDS,     "red",    CR_RED);
    ColorizeString(PD_YELLOWC,  "yellow", CR_GOLD);
    ColorizeString(PD_YELLOWK,  "yellow", CR_GOLD);
    ColorizeString(PD_YELLOWO,  "yellow", CR_GOLD);
    ColorizeString(PD_YELLOWS,  "yellow", CR_GOLD);

    ColorizeString(HUSTR_PLRGREEN,  "Green:",  CR_GREEN);
    ColorizeString(HUSTR_PLRINDIGO, "Indigo:", CR_GRAY);
    ColorizeString(HUSTR_PLRBROWN,  "Brown:",  CR_BROWN);
    ColorizeString(HUSTR_PLRRED,    "Red:",    CR_RED);

    if (gamemission == pack_chex || gamemission == pack_chex3v)
    {
        killchar = 'F';
        statscolor = '0' + CR_GREEN;
    }
    else if (gamemission == pack_hacx)
    {
        statscolor = '0' + CR_BLUE2;
    }
    else if (gamemission == pack_rekkr)
    {
        statscolor = '0' + CR_GOLD;
    }
}

sbarelem_t *st_time_elem = NULL, *st_cmd_elem = NULL;

boolean message_centered;

static void ForceCenterMessage(sbarelem_t *elem)
{
    if (message_centered)
    {
        elem->x_pos = SCREENWIDTH / 2;
        elem->alignment = sbe_h_middle;
    }
    else
    {
        elem->x_pos = elem->orig_x_pos;
        elem->alignment = elem->orig_alignment;
    }
}

void ST_UpdateWidget(sbarelem_t *elem, player_t *player)
{
    sbe_widget_t *widget = elem->subtype.widget;

    switch (widget->type)
    {
        case sbw_message:
            ForceCenterMessage(elem);
            UpdateMessage(widget);
            break;
        case sbw_chat:
            HU_UpdateChatWidget(widget);
            break;
        case sbw_announce:
            UpdateAnnounceMessage(widget);
            break;
        case sbw_title:
            UpdateTitle(widget);
            break;

        case sbw_monsec:
            if (deathmatch)
                UpdateDM(widget);
            else
                UpdateMonSec(widget);
            break;
        case sbw_time:
            st_time_elem = elem;
            UpdateStTime(widget, player);
            break;
        case sbw_coord:
            UpdateCoord(widget, player);
            break;
        case sbw_fps:
            UpdateFPS(widget, player);
            break;
        case sbw_rate:
            UpdateRate(widget, player);
            break;
        case sbw_cmd:
            st_cmd_elem = elem;
            UpdateCmd(widget);
            break;
        case sbw_speed:
            UpdateSpeed(widget, player);
            break;
        default:
            break;
    }
}

void ST_BindHUDVariables(void)
{
    BIND_NUM(hud_level_stats, HUD_WIDGET_OFF, HUD_WIDGET_OFF, HUD_WIDGET_ALWAYS,
        "Show level stats (kills, items, and secrets) widget (1 = On automap; "
        "2 = On HUD; 3 = Always)");
    BIND_NUM(hud_stats_format, STATSFORMAT_RATIO, STATSFORMAT_RATIO, STATSFORMAT_COUNT,
        "Format of level stats (0 = Ratio; 1 = Boolean; 2 = Percent; 3 = Remaining; 4 = Count)");
    BIND_NUM(hud_level_time, HUD_WIDGET_OFF, HUD_WIDGET_OFF, HUD_WIDGET_ALWAYS,
        "Show level time widget (1 = On automap; 2 = On HUD; 3 = Always)");
    BIND_NUM(hud_player_coords, HUD_WIDGET_AUTOMAP, HUD_WIDGET_OFF, HUD_WIDGET_ADVANCED,
        "Show player coordinates widget (1 = On automap; 2 = On HUD; 3 = "
        "Always; 4 = Advanced)");
    BIND_BOOL(hud_command_history, false, "Show command history widget");
    BIND_NUM(hud_command_history_size, 10, 1, HU_MAXMESSAGES,
        "Number of commands to display for command history widget");
    BIND_BOOL(hud_hide_empty_commands, true,
        "Hide empty commands from command history widget");
    BIND_BOOL(hud_time_use, false,
        "Show split time when pressing the use-button");
    BIND_NUM(hud_widget_font, HUD_WIDGET_AUTOMAP, HUD_WIDGET_OFF, HUD_WIDGET_ALWAYS,
        "Use standard Doom font for widgets (1 = On automap; 2 = On HUD; 3 "
        "= Always)");

    M_BindNum("hudcolor_titl", &hudcolor_titl, NULL, CR_GOLD, CR_BRICK, CR_NONE,
        wad_yes, "Color range used for automap level title");
    M_BindNum("hudcolor_xyco", &hudcolor_xyco, NULL, CR_GREEN, CR_BRICK, CR_NONE,
        wad_yes, "Color range used for automap coordinates");

    BIND_BOOL(show_messages, true, "Show messages");
    BIND_NUM(hud_secret_message, SECRETMESSAGE_ON, SECRETMESSAGE_OFF, SECRETMESSAGE_COUNT,
        "Announce revealed secrets (0 = Off; 1 = On; 2 = Count)");
    BIND_BOOL(hud_map_announce, false, "Announce map titles");
    BIND_BOOL(show_toggle_messages, true, "Show toggle messages");
    BIND_BOOL(show_pickup_messages, true, "Show pickup messages");
    BIND_BOOL(show_obituary_messages, true, "Show obituaries");
    BIND_NUM(hudcolor_obituary, CR_GRAY, CR_BRICK, CR_NONE,
        "Color range used for obituaries");
    BIND_BOOL(message_centered, false, "Center messages horizontally");
    BIND_BOOL(message_colorized, false, "Colorize player messages");
}

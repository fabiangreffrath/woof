//
//  Copyright (C) 2025 Roman Fomin
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

#include "config.h"
#include "doomtype.h"
#include "i_printf.h"
#include "i_system.h"

#ifdef HAVE_DISCORD_RPC

#include <time.h>

#include "discord_rpc.h"

// https://discord.com/oauth2/authorize?client_id=1425007665855336460
#define DEFAULT_DISCORD_APP_ID "1425007665855336460"

static void DiscordReady(const DiscordUser *connectedUser)
{
    Printf("Discord: connected to user %s#%s - %s", connectedUser->username,
           connectedUser->discriminator, connectedUser->userId);
}

static void DiscordDisconnected(int errcode, const char *message)
{
    Printf("Discord: disconnected (%d: %s)", errcode, message);
}

static void DiscordError(int errcode, const char *message)
{
    Printf("Discord: error (%d: %s)", errcode, message);
}

static void DiscordSpectate(const char *secret)
{
    Printf("Discord: spectate (%s)", secret);
}

static void DiscordJoin(const char *secret)
{
    Printf("Discord: join (%s)", secret);
}

static void DiscordJoinRequest(const DiscordUser *request)
{
    // we can't join in-game
    int response = DISCORD_REPLY_NO;
    Discord_Respond(request->userId, response);
    Printf("Discord: join request from %s#%s - %s", request->username,
           request->discriminator, request->userId);
}

void I_UpdateDiscordPresence(const char *curstate, const char *curstatus)
{
    static boolean initialized;
    static int64_t starttime;

    const char *curappid = DEFAULT_DISCORD_APP_ID;

    if (!initialized)
    {
        initialized = true;
        DiscordEventHandlers handlers = {
            .ready = DiscordReady,
            .disconnected = DiscordDisconnected,
            .errored = DiscordError,
            .joinGame = DiscordJoin,
            .spectateGame = DiscordSpectate,
            .joinRequest = DiscordJoinRequest
        };
        Discord_Initialize(curappid, &handlers, 1, NULL);
        
        I_AtExit(Discord_ClearPresence, true);
    }

    DiscordRichPresence presence = {0};
    presence.state = curstate;
    if (!starttime)
    {
        starttime = time(0);
    }
    presence.startTimestamp = starttime;
    presence.details = curstatus;
    presence.largeImageKey = "game-image";
    presence.instance = 0;
    Discord_UpdatePresence(&presence);
}

#else

void I_UpdateDiscordPresence(const char *curstate, const char *curstatus)
{
    Printf("Build without discord-rpc");
}

#endif

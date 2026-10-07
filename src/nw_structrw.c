//
// Copyright(C) 2005-2014 Simon Howard
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
//
// Reading and writing various structures into packets
//

#include <string.h>

#include "doomtype.h"
#include "i_system.h"
#include "m_misc.h"
#include "nw_packet.h"
#include "nw_structrw.h"

// String names for the enum values in nw_protocol_t, which are what is
// sent over the wire. Every enum value must have an entry in this list.
static struct
{
    nw_protocol_t protocol;
    const char *name;
} protocol_names[] = {
    {NW_PROTOCOL_CHOCOLATE_DOOM_0, "CHOCOLATE_DOOM_0"},
};

void NW_WriteConnectData(nw_packet_t *packet, nw_connect_data_t *data)
{
    NW_WriteInt8(packet, data->gamemode);
    NW_WriteInt8(packet, data->gamemission);
    NW_WriteInt8(packet, data->lowres_turn);
    NW_WriteInt8(packet, data->drone);
    NW_WriteInt8(packet, data->max_players);
    NW_WriteInt8(packet, data->is_freedoom);
    NW_WriteSHA1Sum(packet, data->wad_sha1sum);
    NW_WriteSHA1Sum(packet, data->deh_sha1sum);
    NW_WriteInt8(packet, data->player_class);
}

boolean NW_ReadConnectData(nw_packet_t *packet, nw_connect_data_t *data)
{
    return NW_ReadInt8(packet, (unsigned int *)&data->gamemode)
           && NW_ReadInt8(packet, (unsigned int *)&data->gamemission)
           && NW_ReadInt8(packet, (unsigned int *)&data->lowres_turn)
           && NW_ReadInt8(packet, (unsigned int *)&data->drone)
           && NW_ReadInt8(packet, (unsigned int *)&data->max_players)
           && NW_ReadInt8(packet, (unsigned int *)&data->is_freedoom)
           && NW_ReadSHA1Sum(packet, data->wad_sha1sum)
           && NW_ReadSHA1Sum(packet, data->deh_sha1sum)
           && NW_ReadInt8(packet, (unsigned int *)&data->player_class);
}

void NW_WriteSettings(nw_packet_t *packet, nw_gamesettings_t *settings)
{
    int i;

    NW_WriteInt8(packet, settings->ticdup);
    NW_WriteInt8(packet, settings->extratics);
    NW_WriteInt8(packet, settings->deathmatch);
    NW_WriteInt8(packet, settings->nomonsters);
    NW_WriteInt8(packet, settings->fast_monsters);
    NW_WriteInt8(packet, settings->respawn_monsters);
    NW_WriteInt8(packet, settings->episode);
    NW_WriteInt8(packet, settings->map);
    NW_WriteInt8(packet, settings->skill);
    NW_WriteInt8(packet, settings->gameversion);
    NW_WriteInt8(packet, settings->lowres_turn);
    NW_WriteInt8(packet, settings->new_sync);
    NW_WriteInt32(packet, settings->timelimit);
    NW_WriteInt8(packet, settings->loadgame);
    NW_WriteInt8(packet, settings->random);
    NW_WriteInt8(packet, settings->num_players);
    NW_WriteInt8(packet, settings->consoleplayer);

    for (i = 0; i < settings->num_players; ++i)
    {
        NW_WriteInt8(packet, settings->player_classes[i]);
    }

    NW_WriteInt8(packet, settings->demo_version);
    for (i = 0; i < NW_GAME_OPTION_SIZE; ++i)
    {
        NW_WriteInt8(packet, settings->options[i]);
    }
}

boolean NW_ReadSettings(nw_packet_t *packet, nw_gamesettings_t *settings)
{
    boolean success;
    int i;

    success = NW_ReadInt8(packet, (unsigned int *)&settings->ticdup)
           && NW_ReadInt8(packet, (unsigned int *)&settings->extratics)
           && NW_ReadInt8(packet, (unsigned int *)&settings->deathmatch)
           && NW_ReadInt8(packet, (unsigned int *)&settings->nomonsters)
           && NW_ReadInt8(packet, (unsigned int *)&settings->fast_monsters)
           && NW_ReadInt8(packet, (unsigned int *)&settings->respawn_monsters)
           && NW_ReadInt8(packet, (unsigned int *)&settings->episode)
           && NW_ReadInt8(packet, (unsigned int *)&settings->map)
           && NW_ReadSInt8(packet, &settings->skill)
           && NW_ReadInt8(packet, (unsigned int *)&settings->gameversion)
           && NW_ReadInt8(packet, (unsigned int *)&settings->lowres_turn)
           && NW_ReadInt8(packet, (unsigned int *)&settings->new_sync)
           && NW_ReadInt32(packet, (unsigned int *)&settings->timelimit)
           && NW_ReadSInt8(packet, (signed int *)&settings->loadgame)
           && NW_ReadInt8(packet, (unsigned int *)&settings->random)
           && NW_ReadInt8(packet, (unsigned int *)&settings->num_players)
           && NW_ReadSInt8(packet, (signed int *)&settings->consoleplayer);

    if (!success)
    {
        return false;
    }

    for (i = 0; i < settings->num_players && i < NW_MAXPLAYERS; ++i)
    {
        if (!NW_ReadInt8(packet, (unsigned int *)&settings->player_classes[i]))
        {
            return false;
        }
    }

    NW_ReadInt8(packet, (unsigned int *)&settings->demo_version);

    for (i = 0; i < NW_GAME_OPTION_SIZE; ++i)
    {
        unsigned int value;
        if (NW_ReadInt8(packet, &value))
        {
            settings->options[i] = (byte)value;
        }
    }

    return true;
}

boolean NW_ReadQueryData(nw_packet_t *packet, nw_querydata_t *query)
{
    boolean success;

    query->version = NW_ReadSafeString(packet);

    success = query->version != NULL
           && NW_ReadInt8(packet, (unsigned int *)&query->server_state)
           && NW_ReadInt8(packet, (unsigned int *)&query->num_players)
           && NW_ReadInt8(packet, (unsigned int *)&query->max_players)
           && NW_ReadInt8(packet, (unsigned int *)&query->gamemode)
           && NW_ReadInt8(packet, (unsigned int *)&query->gamemission);

    if (!success)
    {
        return false;
    }

    query->description = NW_ReadSafeString(packet);

    // We read the list of protocols supported by the server. However,
    // old versions of Chocolate Doom do not support this field; it is
    // okay if it cannot be successfully read.
    query->protocol = NW_ReadProtocolList(packet);

    return query->description != NULL;
}

void NW_WriteQueryData(nw_packet_t *packet, nw_querydata_t *query)
{
    NW_WriteString(packet, query->version);
    NW_WriteInt8(packet, query->server_state);
    NW_WriteInt8(packet, query->num_players);
    NW_WriteInt8(packet, query->max_players);
    NW_WriteInt8(packet, query->gamemode);
    NW_WriteInt8(packet, query->gamemission);
    NW_WriteString(packet, query->description);

    // Write a list of all supported protocols. Note that the query->protocol
    // field is ignored here; it is only used when receiving.
    NW_WriteProtocolList(packet);
}

void NW_WriteTiccmdDiff(nw_packet_t *packet, nw_ticdiff_t *diff,
                         boolean lowres_turn)
{
    // Header

    NW_WriteInt8(packet, diff->diff);

    // Write the fields which are enabled:

    if (diff->diff & NW_TICDIFF_FORWARD)
    {
        NW_WriteInt8(packet, diff->cmd.forwardmove);
    }
    if (diff->diff & NW_TICDIFF_SIDE)
    {
        NW_WriteInt8(packet, diff->cmd.sidemove);
    }
    if (diff->diff & NW_TICDIFF_TURN)
    {
        if (lowres_turn)
        {
            NW_WriteInt8(packet, diff->cmd.angleturn / 256);
        }
        else
        {
            NW_WriteInt16(packet, diff->cmd.angleturn);
        }
    }
    if (diff->diff & NW_TICDIFF_BUTTONS)
    {
        NW_WriteInt8(packet, diff->cmd.buttons);
    }
    if (diff->diff & NW_TICDIFF_CONSISTANCY)
    {
        NW_WriteInt8(packet, diff->cmd.consistancy);
    }
    if (diff->diff & NW_TICDIFF_CHATCHAR)
    {
        NW_WriteInt8(packet, diff->cmd.chatchar);
    }
}

boolean NW_ReadTiccmdDiff(nw_packet_t *packet, nw_ticdiff_t *diff,
                           boolean lowres_turn)
{
    unsigned int val;
    signed int sval;

    // Read header

    if (!NW_ReadInt8(packet, &diff->diff))
    {
        return false;
    }

    // Read fields

    if (diff->diff & NW_TICDIFF_FORWARD)
    {
        if (!NW_ReadSInt8(packet, &sval))
        {
            return false;
        }
        diff->cmd.forwardmove = sval;
    }

    if (diff->diff & NW_TICDIFF_SIDE)
    {
        if (!NW_ReadSInt8(packet, &sval))
        {
            return false;
        }
        diff->cmd.sidemove = sval;
    }

    if (diff->diff & NW_TICDIFF_TURN)
    {
        if (lowres_turn)
        {
            if (!NW_ReadSInt8(packet, &sval))
            {
                return false;
            }
            diff->cmd.angleturn = sval * 256;
        }
        else
        {
            if (!NW_ReadSInt16(packet, &sval))
            {
                return false;
            }
            diff->cmd.angleturn = sval;
        }
    }

    if (diff->diff & NW_TICDIFF_BUTTONS)
    {
        if (!NW_ReadInt8(packet, &val))
        {
            return false;
        }
        diff->cmd.buttons = val;
    }

    if (diff->diff & NW_TICDIFF_CONSISTANCY)
    {
        if (!NW_ReadInt8(packet, &val))
        {
            return false;
        }
        diff->cmd.consistancy = val;
    }

    if (diff->diff & NW_TICDIFF_CHATCHAR)
    {
        if (!NW_ReadInt8(packet, &val))
        {
            return false;
        }
        diff->cmd.chatchar = val;
    }

    return true;
}

void NW_TiccmdDiff(ticcmd_t *tic1, ticcmd_t *tic2, nw_ticdiff_t *diff)
{
    diff->diff = 0;
    diff->cmd = *tic2;

    if (tic1->forwardmove != tic2->forwardmove)
    {
        diff->diff |= NW_TICDIFF_FORWARD;
    }
    if (tic1->sidemove != tic2->sidemove)
    {
        diff->diff |= NW_TICDIFF_SIDE;
    }
    if (tic1->angleturn != tic2->angleturn)
    {
        diff->diff |= NW_TICDIFF_TURN;
    }
    if (tic1->buttons != tic2->buttons)
    {
        diff->diff |= NW_TICDIFF_BUTTONS;
    }
    if (tic1->consistancy != tic2->consistancy)
    {
        diff->diff |= NW_TICDIFF_CONSISTANCY;
    }
    if (tic2->chatchar != 0)
    {
        diff->diff |= NW_TICDIFF_CHATCHAR;
    }
}

void NW_TiccmdPatch(ticcmd_t *src, nw_ticdiff_t *diff, ticcmd_t *dest)
{
    memmove(dest, src, sizeof(ticcmd_t));

    // Apply the diff

    if (diff->diff & NW_TICDIFF_FORWARD)
    {
        dest->forwardmove = diff->cmd.forwardmove;
    }
    if (diff->diff & NW_TICDIFF_SIDE)
    {
        dest->sidemove = diff->cmd.sidemove;
    }
    if (diff->diff & NW_TICDIFF_TURN)
    {
        dest->angleturn = diff->cmd.angleturn;
    }
    if (diff->diff & NW_TICDIFF_BUTTONS)
    {
        dest->buttons = diff->cmd.buttons;
    }
    if (diff->diff & NW_TICDIFF_CONSISTANCY)
    {
        dest->consistancy = diff->cmd.consistancy;
    }

    if (diff->diff & NW_TICDIFF_CHATCHAR)
    {
        dest->chatchar = diff->cmd.chatchar;
    }
    else
    {
        dest->chatchar = 0;
    }
}

//
// nw_full_ticcmd_t
//

boolean NW_ReadFullTiccmd(nw_packet_t *packet, nw_full_ticcmd_t *cmd,
                           boolean lowres_turn)
{
    unsigned int bitfield;
    int i;

    // Latency

    if (!NW_ReadSInt16(packet, &cmd->latency))
    {
        return false;
    }

    // Regenerate playeringame from the "header" bitfield

    if (!NW_ReadInt8(packet, &bitfield))
    {
        return false;
    }

    for (i = 0; i < NW_MAXPLAYERS; ++i)
    {
        cmd->playeringame[i] = (bitfield & (1 << i)) != 0;
    }

    // Read cmds

    for (i = 0; i < NW_MAXPLAYERS; ++i)
    {
        if (cmd->playeringame[i])
        {
            if (!NW_ReadTiccmdDiff(packet, &cmd->cmds[i], lowres_turn))
            {
                return false;
            }
        }
    }

    return true;
}

void NW_WriteFullTiccmd(nw_packet_t *packet, nw_full_ticcmd_t *cmd,
                         boolean lowres_turn)
{
    unsigned int bitfield;
    int i;

    // Write the latency

    NW_WriteInt16(packet, cmd->latency);

    // Write "header" byte indicating which players are active
    // in this ticcmd

    bitfield = 0;

    for (i = 0; i < NW_MAXPLAYERS; ++i)
    {
        if (cmd->playeringame[i])
        {
            bitfield |= 1 << i;
        }
    }

    NW_WriteInt8(packet, bitfield);

    // Write player ticcmds

    for (i = 0; i < NW_MAXPLAYERS; ++i)
    {
        if (cmd->playeringame[i])
        {
            NW_WriteTiccmdDiff(packet, &cmd->cmds[i], lowres_turn);
        }
    }
}

void NW_WriteWaitData(nw_packet_t *packet, nw_waitdata_t *data)
{
    int i;

    NW_WriteInt8(packet, data->num_players);
    NW_WriteInt8(packet, data->num_drones);
    NW_WriteInt8(packet, data->ready_players);
    NW_WriteInt8(packet, data->max_players);
    NW_WriteInt8(packet, data->is_controller);
    NW_WriteInt8(packet, data->consoleplayer);

    for (i = 0; i < data->num_players && i < NW_MAXPLAYERS; ++i)
    {
        NW_WriteString(packet, data->player_names[i]);
        NW_WriteString(packet, data->player_addrs[i]);
    }

    NW_WriteSHA1Sum(packet, data->wad_sha1sum);
    NW_WriteSHA1Sum(packet, data->deh_sha1sum);
    NW_WriteInt8(packet, data->is_freedoom);
}

boolean NW_ReadWaitData(nw_packet_t *packet, nw_waitdata_t *data)
{
    int i;
    char *s;

    if (!NW_ReadInt8(packet, (unsigned int *)&data->num_players)
        || !NW_ReadInt8(packet, (unsigned int *)&data->num_drones)
        || !NW_ReadInt8(packet, (unsigned int *)&data->ready_players)
        || !NW_ReadInt8(packet, (unsigned int *)&data->max_players)
        || !NW_ReadInt8(packet, (unsigned int *)&data->is_controller)
        || !NW_ReadSInt8(packet, &data->consoleplayer))
    {
        return false;
    }

    for (i = 0; i < data->num_players && i < NW_MAXPLAYERS; ++i)
    {
        s = NW_ReadString(packet);

        if (s == NULL || strlen(s) >= MAXPLAYERNAME)
        {
            return false;
        }

        M_StringCopy(data->player_names[i], s, MAXPLAYERNAME);

        s = NW_ReadString(packet);

        if (s == NULL || strlen(s) >= MAXPLAYERNAME)
        {
            return false;
        }

        M_StringCopy(data->player_addrs[i], s, MAXPLAYERNAME);
    }

    return NW_ReadSHA1Sum(packet, data->wad_sha1sum)
           && NW_ReadSHA1Sum(packet, data->deh_sha1sum)
           && NW_ReadInt8(packet, (unsigned int *)&data->is_freedoom);
}

static boolean NW_ReadBlob(nw_packet_t *packet, uint8_t *buf, size_t len)
{
    unsigned int b;
    int i;

    for (i = 0; i < len; ++i)
    {
        if (!NW_ReadInt8(packet, &b))
        {
            return false;
        }

        buf[i] = b;
    }

    return true;
}

static void NW_WriteBlob(nw_packet_t *packet, uint8_t *buf, size_t len)
{
    int i;

    for (i = 0; i < len; ++i)
    {
        NW_WriteInt8(packet, buf[i]);
    }
}

boolean NW_ReadSHA1Sum(nw_packet_t *packet, sha1_digest_t digest)
{
    return NW_ReadBlob(packet, digest, sizeof(sha1_digest_t));
}

void NW_WriteSHA1Sum(nw_packet_t *packet, sha1_digest_t digest)
{
    NW_WriteBlob(packet, digest, sizeof(sha1_digest_t));
}

static nw_protocol_t ParseProtocolName(const char *name)
{
    int i;

    for (i = 0; i < arrlen(protocol_names); ++i)
    {
        if (!strcmp(protocol_names[i].name, name))
        {
            return protocol_names[i].protocol;
        }
    }

    return NW_PROTOCOL_UNKNOWN;
}

// NW_ReadProtocol reads a single string-format protocol name from the given
// packet, returning NW_PROTOCOL_UNKNOWN if the string describes an unknown
// protocol.
nw_protocol_t NW_ReadProtocol(nw_packet_t *packet)
{
    const char *name;

    name = NW_ReadString(packet);
    if (name == NULL)
    {
        return NW_PROTOCOL_UNKNOWN;
    }

    return ParseProtocolName(name);
}

// NW_WriteProtocol writes a single string-format protocol name to a packet.
void NW_WriteProtocol(nw_packet_t *packet, nw_protocol_t protocol)
{
    int i;

    for (i = 0; i < arrlen(protocol_names); ++i)
    {
        if (protocol_names[i].protocol == protocol)
        {
            NW_WriteString(packet, protocol_names[i].name);
            return;
        }
    }

    // If you add an entry to the nw_protocol_t enum, a corresponding entry
    // must be added to the protocol_names list.
    I_Error("protocol %d missing from protocol_names list; please add it.",
            protocol);
}

// NW_ReadProtocolList reads a list of string-format protocol names from
// the given packet, returning a single protocol number. The protocol that is
// returned is the last protocol in the list that is a supported protocol. If
// no recognized protocols are read, NW_PROTOCOL_UNKNOWN is returned.
nw_protocol_t NW_ReadProtocolList(nw_packet_t *packet)
{
    nw_protocol_t result;
    unsigned int num_protocols;
    int i;

    if (!NW_ReadInt8(packet, &num_protocols))
    {
        return NW_PROTOCOL_UNKNOWN;
    }

    result = NW_PROTOCOL_UNKNOWN;

    for (i = 0; i < num_protocols; ++i)
    {
        nw_protocol_t p;
        const char *name;

        name = NW_ReadString(packet);
        if (name == NULL)
        {
            return NW_PROTOCOL_UNKNOWN;
        }

        p = ParseProtocolName(name);
        if (p != NW_PROTOCOL_UNKNOWN)
        {
            result = p;
        }
    }

    return result;
}

// NW_WriteProtocolList writes a list of string-format protocol names into
// the given packet, all the supported protocols in the nw_protocol_t enum.
// This is slightly different to other functions in this file, in that there
// is nothing the caller can "choose" to write; the built-in list of all
// protocols is always sent.
void NW_WriteProtocolList(nw_packet_t *packet)
{
    int i;

    NW_WriteInt8(packet, NW_NUM_PROTOCOLS);

    for (i = 0; i < NW_NUM_PROTOCOLS; ++i)
    {
        NW_WriteProtocol(packet, i);
    }
}

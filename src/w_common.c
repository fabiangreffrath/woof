//
// Copyright(C) 2026 Roman Fomin
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

#include "doomdef.h"
#include "doomstat.h"
#include "i_printf.h"
#include "m_array.h"
#include "m_misc.h"
#include "m_swap.h"
#include "w_common.h"

// [ZDoom PK3] sub-directories of a PK3 archive or folder that assign
// files to the WAD namespaces.
// https://zdoom.org/wiki/Using_ZIPs_as_WAD_replacement
static const w_dir_spec_t subdirs[] =
{
    {"actors",    "AC_START",  "AC_END",   ns_actors,    false},
    {"colormaps", "C_START",   "C_END",    ns_colormaps, false},
    {"flats",     "F_START",   "F_END",    ns_flats,     false},
    {"graphics",  NULL,        NULL,       ns_global,    false},
    {"hires",     "HI_START",  "HI_END",   ns_hires,     false},
    {"maps",      NULL,        NULL,       ns_global,    true },
    {"music",     NULL,        NULL,       ns_global,    false},
    {"patches",   NULL,        NULL,       ns_global,    false},
    {"sounds",    NULL,        NULL,       ns_global,    false},
    {"sprites",   "S_START",   "S_END",    ns_sprites,   false},
    {"textures",  "TX_START",  "TX_END",   ns_textures,  false},
    {"voxels",    "VX_START",  "VX_END",   ns_voxels,    false},
};

static const struct
{
    const char *dir;
    GameMode_t mode;
    GameMission_t mission;
} filters[] = {
    {"doom.id.doom1",            shareware,    doom     },
    {"doom.id.doom1.registered", registered,   doom     },
    {"doom.id.doom1.ultimate",   retail,       doom     },
    {"doom.id.doom2.commercial", commercial,   doom2    },
    {"doom.id.doom2.plutonia",   commercial,   pack_plut},
    {"doom.id.doom2.tnt",        commercial,   pack_tnt },
    {"doom.freedoom.phase1",     retail,       pack_freedoom},
    {"doom.freedoom.phase2",     commercial,   pack_freedoom},
    {"chex.chex1",               retail,       pack_chex},
    {"hacx.hacx1",               commercial,   pack_hacx},
    {"rekkr",                    retail,       pack_rekkr}
};

void W_ConvertSlashes(char *path)
{
    for (char *p = path; *p; ++p)
    {
        if (*p == '\\')
        {
            *p = '/';
        }
    }
}

void W_ConvertSpriteName(char *name)
{
    for (int i = 0; i < 8; ++i)
    {
        if (name[i] == '^')
        {
            name[i] = '\\';
        }
    }
}

// [ZDoom PK3] The file list of an archive or folder is sorted, so all
// files of one directory are consecutive. Loading a base directory is a
// single pass that assigns files to the reserved directories on the fly.

const w_dir_spec_t *W_LookupDirSpec(const char *name)
{
    const char *sep = strchr(name, '/');
    size_t length = sep ? (size_t)(sep - name) : strlen(name);

    for (int i = 0; i < arrlen(subdirs); ++i)
    {
        if (strlen(subdirs[i].dir) == length
            && !strncasecmp(name, subdirs[i].dir, length))
        {
            return &subdirs[i];
        }
    }
    return NULL;
}

void W_CoalesceAllResources(void)
{
    for (int i = 0; i < arrlen(subdirs); ++i)
    {
        if (subdirs[i].namespace != ns_global)
        {
            W_CoalesceMarkedResource(subdirs[i].start_marker,
                                     subdirs[i].end_marker,
                                     subdirs[i].namespace);
        }
    }
}

static const w_dir_spec_t *cur_spec; // reserved directory of the current run
static boolean run_lumps;            // lumps added to the current run

static void CloseDirRun(void)
{
    if (cur_spec && run_lumps && cur_spec->end_marker)
    {
        W_AddMarker(cur_spec->end_marker);
    }
    cur_spec = NULL;
    run_lumps = false;
}

const w_dir_spec_t *W_DirSpecOfFile(const char *relpath)
{
    const w_dir_spec_t *spec =
        strchr(relpath, '/') ? W_LookupDirSpec(relpath) : NULL;

    if (spec != cur_spec)
    {
        CloseDirRun();
        cur_spec = spec;
    }

    return spec;
}

void W_BeginDirLump(const w_dir_spec_t *spec)
{
    if (spec && spec->start_marker && !run_lumps)
    {
        W_AddMarker(spec->start_marker);
    }
    run_lumps = true;
}

void W_FlushDirRun(void)
{
    CloseDirRun();
}

void W_Filter(w_module_t *module, w_handle_t handle)
{
    char *dir = NULL;

    for (int i = 0; i < arrlen(filters); ++i)
    {
        if (filters[i].mode == gamemode && filters[i].mission == gamemission)
        {
            dir = M_StringJoin("filter", DIR_SEPARATOR_S, filters[i].dir);
            break;
        }
    }

    if (!dir)
    {
        return;
    }

    for (char *p = dir; *p; ++p)
    {
        if (*p == '.')
        {
            *p = '\0';
            module->AddDir(handle, dir);
            *p = '.';
        }
    }
    module->AddDir(handle, dir);

    free(dir);
}

void W_FilterAutoload(w_module_t *module, w_handle_t handle)
{
    if (gamemission < pack_chex)
    {
        module->AddDir(handle, "filter" DIR_SEPARATOR_S "doom-all");
        module->AddDir(handle, "filter" DIR_SEPARATOR_S "game-doom");
    }
    if (gamemission == pack_chex || gamemission == pack_chex3v)
    {
        module->AddDir(handle, "filter" DIR_SEPARATOR_S "chex-all");
        module->AddDir(handle, "filter" DIR_SEPARATOR_S "game-chex");
    }
    if (gamemission == doom)
    {
        module->AddDir(handle, "filter" DIR_SEPARATOR_S "doom1-all");
    }
    else if (gamemission >= doom2 && gamemission <= pack_plut)
    {
        module->AddDir(handle, "filter" DIR_SEPARATOR_S "doom2-all");
    }
    else if (gamemission == pack_freedoom)
    {
        module->AddDir(handle, "filter" DIR_SEPARATOR_S "freedoom-all");
        if (gamemode == commercial)
        {
            module->AddDir(handle, "filter" DIR_SEPARATOR_S "freedoom2-all");
        }
        else
        {
            module->AddDir(handle, "filter" DIR_SEPARATOR_S "freedoom1-all");
        }
    }
    else if (gamemission == pack_rekkr)
    {
        module->AddDir(handle, "filter" DIR_SEPARATOR_S "rekkr-all");
    }

    for (int i = 0; i < array_size(wadfiles); ++i)
    {
        char *dir = M_StringJoin("filter", DIR_SEPARATOR_S, M_BaseName(wadfiles[i]));
        module->AddDir(handle, dir);
        free(dir);
    }
}

// Map data lumps that may follow a level marker lump.
static boolean IsMapLumpName(const char *name)
{
    static const char *const names[] = {
        "TEXTMAP",  "THINGS",   "LINEDEFS", "SIDEDEFS", "VERTEXES",
        "SEGS",     "SSECTORS", "NODES",    "SECTORS",  "REJECT",
        "BLOCKMAP", "BEHAVIOR", "ZNODES",   "ENDMAP"
    };

    for (int i = 0; i < arrlen(names); ++i)
    {
        if (!strcasecmp(name, names[i]))
        {
            return true;
        }
    }
    return false;
}

void W_AddWadFromMemory(const char *name, const void *data, size_t data_size,
                        const char *map_name)
{
    wadinfo_t header;

    if (sizeof(header) > data_size)
    {
        I_Error("Error reading header from %s", name);
    }

    memcpy(&header, data, sizeof(header));

    if (strncmp(header.identification, "IWAD", 4)
        && strncmp(header.identification, "PWAD", 4))
    {
        I_Error("Wad file %s doesn't have IWAD or PWAD id", name);
    }

    header.numlumps = LONG(header.numlumps);
    header.infotableofs = LONG(header.infotableofs);

    if (header.numlumps == 0)
    {
        I_Printf(VB_WARNING, "Wad file %s is empty", name);
        return;
    }

    if (header.infotableofs + header.numlumps * sizeof(filelump_t) > data_size)
    {
        I_Printf(VB_WARNING, "Error seeking offset from %s", name);
        return;
    }

    filelump_t *fileinfo = (filelump_t *)((const byte *)data
                                         + header.infotableofs);

    const char *wadname = M_StringDuplicate(name);

    I_Printf(VB_INFO, " - adding %s", name);

    if (map_name)
    {
        // [ZDoom PK3] WADs under maps/ hold the data of one single
        // level, and the file name determines the map name. Add the level
        // marker lump renamed after the file, the lumps of the first level,
        // and skip any other data.
        boolean in_map = false;

        for (int i = 0; i < header.numlumps; ++i)
        {
            char lumpname[9] = {0};
            memcpy(lumpname, fileinfo[i].name, 8);

            boolean is_map_lump = IsMapLumpName(lumpname);

            if (!is_map_lump && in_map)
            {
                break; // end of the first level, ignore the rest
            }

            if (!in_map)
            {
                // Add the level marker lump, named after the WAD file.
                in_map = true;

                lumpinfo_t marker = {0};
                M_CopyLumpName(marker.name, map_name);
                marker.size = 0;
                marker.wad_file = wadname;
                array_push(lumpinfo, marker);
                numlumps++;

                if (!is_map_lump)
                {
                    continue; // skip the marker lump's own data
                }
            }

            lumpinfo_t item = {0};
            M_CopyLumpName(item.name, fileinfo[i].name);
            int size = LONG(fileinfo[i].size);
            int position = LONG(fileinfo[i].filepos);
            if (position < 0 || size < 0
                || position + size > (int64_t)data_size)
            {
                I_Error("Error reading lump %d from %s", i, wadname);
            }
            item.size = size;
            item.data = (const byte *)data + position;
            item.wad_file = wadname;
            array_push(lumpinfo, item);
            numlumps++;
        }
    }
    else
    {
        array_push(wadfiles, wadname);

        numlumps += header.numlumps;

        for (int i = 0; i < header.numlumps; ++i)
        {
            lumpinfo_t item = {0};
            M_CopyLumpName(item.name, fileinfo[i].name);
            int size = LONG(fileinfo[i].size);
            int position = LONG(fileinfo[i].filepos);
            if (position < 0 || size < 0
                || position + size > (int64_t)data_size)
            {
                I_Error("Error reading lump %d from %s", i, wadname);
            }
            item.size = size;
            item.data = (const byte *)data + position;
            item.wad_file = wadname;
            array_push(lumpinfo, item);
        }
    }
}

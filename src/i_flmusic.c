//
// Copyright(C) 2022 Roman Fomin
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
// DESCRIPTION:
//      FluidSynth backend

#include "SDL.h"
#include "fluidsynth.h"

#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "i_oalstream.h"
#include "m_config.h"

#if (FLUIDSYNTH_VERSION_MAJOR < 2 \
     || (FLUIDSYNTH_VERSION_MAJOR == 2 && FLUIDSYNTH_VERSION_MINOR < 2))
typedef int fluid_int_t;
typedef long fluid_long_long_t;
#else
typedef fluid_long_long_t fluid_int_t;
#endif

#include "d_iwad.h"
#include "d_main.h"
#include "doomtype.h"
#include "i_glob.h"
#include "i_printf.h"
#include "i_sound.h"
#include "m_array.h"
#include "m_io.h"
#include "m_misc.h"
#include "memio.h"
#include "mus2mid.h"
#include "w_wad.h"
#include "z_zone.h"

static const char *soundfont_dir = "";
static boolean mus_chorus;
static boolean mus_reverb;

static fluid_synth_t *synth = NULL;
static fluid_settings_t *settings = NULL;
static fluid_player_t *player = NULL;

static const char **soundfonts = NULL;

// Load SNDFONT lump

static byte *lump;
static int lumplen;

static void *FL_sfopen(const char *path)
{
    MEMFILE *instream;

    instream = mem_fopen_read(lump, lumplen);

    return instream;
}

static int FL_sfread(void *buf, fluid_int_t count, void *handle)
{
    if (mem_fread(buf, sizeof(byte), count, (MEMFILE *)handle) == count)
    {
        return FLUID_OK;
    }
    return FLUID_FAILED;
}

static int FL_sfseek(void *handle, fluid_long_long_t offset, int origin)
{
    if (mem_fseek((MEMFILE *)handle, offset, origin) >= 0)
    {
        return FLUID_OK;
    }
    return FLUID_FAILED;
}

static int FL_sfclose(void *handle)
{
    mem_fclose((MEMFILE *)handle);
    Z_ChangeTag(lump, PU_CACHE);
    return FLUID_OK;
}

static fluid_long_long_t FL_sftell(void *handle)
{
    return mem_ftell((MEMFILE *)handle);
}

static void ScanDir(const char *dir, boolean recursion)
{
    glob_t *glob;

    if (recursion == false)
    {
        // [FG] replace global "/usr/share" with user's "~/.local/share"
        const char usr_share[] = "/usr/share";
        if (strncmp(dir, usr_share, strlen(usr_share)) == 0)
        {
            char *home_dir = M_getenv("XDG_DATA_HOME");

            if (home_dir == NULL)
            {
                home_dir = M_getenv("HOME");
            }

            if (home_dir)
            {
                char *local_share = M_StringJoin(home_dir, "/.local/share");
                char *local_dir = M_StringReplace(dir, usr_share, local_share);
                free(local_share);
                ScanDir(local_dir, true);
                free(local_dir);
            }
        }
        else if (dir[0] == '.')
        {
            // [FG] relative to the executable directory
            char *rel = M_StringJoin(D_DoomExeDir(), DIR_SEPARATOR_S, dir);
            ScanDir(rel, true);
            free(rel);

            // [FG] relative to the config directory (if different)
            if (dir[1] != '.' && strcmp(D_DoomExeDir(), D_DoomPrefDir()) != 0)
            {
                rel = M_StringJoin(D_DoomPrefDir(), DIR_SEPARATOR_S, dir);
                ScanDir(rel, true);
                free(rel);
            }

            // [FG] never absolute path
            return;
        }
    }

    I_Printf(VB_DEBUG, "Scanning for soundfonts in %s", dir);

    glob = I_StartMultiGlob(dir, GLOB_FLAG_NOCASE | GLOB_FLAG_SORTED, "*.sf2",
                            "*.sf3");

    while (1)
    {
        const char *filename = I_NextGlob(glob);

        if (filename == NULL)
        {
            break;
        }

        array_push(soundfonts, M_StringDuplicate(filename));
    }

    I_EndGlob(glob);
}

static void GetSoundFonts(void)
{
    char *left, *p, *dup_path;

    if (array_size(soundfonts))
    {
        return;
    }

    // Split into individual dirs within the list.
    dup_path = M_StringDuplicate(soundfont_dir);

    left = dup_path;

    while (1)
    {
        p = strchr(left, PATH_SEPARATOR);
        if (p != NULL)
        {
            // Break at the separator and use the left hand side
            // as another soundfont dir
            *p = '\0';

            ScanDir(left, false);

            left = p + 1;
        }
        else
        {
            break;
        }
    }

    ScanDir(left, false);

    free(dup_path);
}

static void FreeSynthAndSettings(void)
{
    // deleting the synth also deletes sfloader
    if (synth)
    {
        delete_fluid_synth(synth);
        synth = NULL;
    }

    if (settings)
    {
        delete_fluid_settings(settings);
        settings = NULL;
    }
}

static void I_FL_Log_Error(int level, const char *message, void *data)
{
    I_Printf(VB_ERROR, "%s", message);
}

static void I_FL_Log_Debug(int level, const char *message, void *data)
{
    I_Printf(VB_DEBUG, "%s", message);
}

static boolean I_FL_InitStream(int device)
{
    int sf_id;
    int lumpnum;

    fluid_set_log_function(FLUID_PANIC, I_FL_Log_Error, NULL);
    fluid_set_log_function(FLUID_ERR,   I_FL_Log_Error, NULL);
    fluid_set_log_function(FLUID_WARN,  I_FL_Log_Debug, NULL);
    fluid_set_log_function(FLUID_INFO,  NULL,           NULL);
    fluid_set_log_function(FLUID_DBG,   NULL,           NULL);

    settings = new_fluid_settings();

    fluid_settings_setnum(settings, "synth.sample-rate", SND_SAMPLERATE);

    fluid_settings_setint(settings, "synth.chorus.active", mus_chorus);
    fluid_settings_setint(settings, "synth.reverb.active", mus_reverb);

    if (mus_reverb)
    {
        fluid_settings_setnum(settings, "synth.reverb.room-size", 0.6);
        fluid_settings_setnum(settings, "synth.reverb.damp", 0.4);
        fluid_settings_setnum(settings, "synth.reverb.width", 4);
        fluid_settings_setnum(settings, "synth.reverb.level", 0.15);
    }

    if (mus_chorus)
    {
        fluid_settings_setnum(settings, "synth.chorus.level", 0.35);
        fluid_settings_setnum(settings, "synth.chorus.depth", 5);
    }

    synth = new_fluid_synth(settings);

    if (synth == NULL)
    {
        I_Printf(VB_ERROR,
                 "I_FL_InitMusic: FluidSynth failed to initialize synth.");
        return false;
    }

    lumpnum = W_CheckNumForName("SNDFONT");
    if (lumpnum >= 0)
    {
        fluid_sfloader_t *sfloader;

        lump = W_CacheLumpNum(lumpnum, PU_STATIC);
        lumplen = W_LumpLength(lumpnum);

        sfloader = new_fluid_defsfloader(settings);
        fluid_sfloader_set_callbacks(sfloader, FL_sfopen, FL_sfread, FL_sfseek,
                                     FL_sftell, FL_sfclose);
        fluid_synth_add_sfloader(synth, sfloader);
        sf_id = fluid_synth_sfload(synth, "", true);
    }
    else
    {
        GetSoundFonts();

        if (device >= array_size(soundfonts))
        {
            FreeSynthAndSettings();
            return false;
        }

        sf_id = fluid_synth_sfload(synth, soundfonts[device], true);
    }

    if (sf_id == FLUID_FAILED)
    {
        char *errmsg;
        errmsg = M_StringJoin(
            "Error loading FluidSynth soundfont: ",
            lumpnum >= 0 ? "SNDFONT lump" : soundfonts[device]);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, PROJECT_STRING, errmsg,
                                 NULL);
        free(errmsg);
        FreeSynthAndSettings();
        return false;
    }

    I_Printf(VB_INFO, "FluidSynth Init: Using '%s'.",
             lumpnum >= 0 ? "SNDFONT lump" : soundfonts[device]);

    return true;
}

static boolean I_FL_OpenStream(void *data, ALsizei size, ALenum *format,
                               ALsizei *freq, ALsizei *frame_size)
{
    if (!IsMid(data, size) && !IsMus(data, size))
    {
        return false;
    }

    if (!synth)
    {
        return false;
    }

    int result = FLUID_FAILED;

    player = new_fluid_player(synth);

    if (player == NULL)
    {
        I_Printf(VB_ERROR,
                 "I_FL_InitMusic: FluidSynth failed to initialize player.");
        return false;
    }

    if (IsMid(data, size))
    {
        result = fluid_player_add_mem(player, data, size);
    }
    else
    {
        // Assume a MUS file and try to convert
        MEMFILE *instream;
        MEMFILE *outstream;
        void *outbuf;
        size_t outbuf_len;

        instream = mem_fopen_read(data, size);
        outstream = mem_fopen_write();

        if (mus2mid(instream, outstream) == 0)
        {
            mem_get_buf(outstream, &outbuf, &outbuf_len);
            result = fluid_player_add_mem(player, outbuf, outbuf_len);
        }

        mem_fclose(instream);
        mem_fclose(outstream);
    }

    if (result != FLUID_OK)
    {
        delete_fluid_player(player);
        player = NULL;
        I_Printf(VB_ERROR, "I_FL_RegisterSong: Failed to load in-memory song.");
        return false;
    }

    *format = AL_FORMAT_STEREO16;
    *freq = SND_SAMPLERATE;
    *frame_size = 2 * sizeof(short);

    return true;
}

static int I_FL_FillStream(byte *buffer, int buffer_samples)
{
    int result;

    result = fluid_synth_write_s16(synth, buffer_samples, buffer, 0, 2, buffer,
                                   1, 2);

    if (result != FLUID_OK)
    {
        I_Printf(VB_ERROR, "FL_Callback: Error generating FluidSynth audio");
    }

    return buffer_samples;
}

static void I_FL_PlayStream(boolean looping)
{
    if (!player)
    {
        return;
    }

    fluid_player_set_loop(player, looping ? -1 : 1);
    fluid_player_play(player);
}

static void I_FL_CloseStream(void)
{
    if (!player || !synth)
    {
        return;
    }

    fluid_player_stop(player);

    fluid_synth_program_reset(synth);
    fluid_synth_system_reset(synth);

    delete_fluid_player(player);
    player = NULL;
}

static void I_FL_ShutdownStream(void)
{
    FreeSynthAndSettings();
}

#define NAME_MAX_LENGTH 25

static const char **I_FL_DeviceList(void)
{
    static const char **devices = NULL;

    if (array_size(devices))
    {
        return devices;
    }

    if (W_CheckNumForName("SNDFONT") >= 0)
    {
        array_push(devices, "FluidSynth: SNDFONT");
        return devices;
    }

    GetSoundFonts();

    for (int i = 0; i < array_size(soundfonts); ++i)
    {
        char *name = M_StringDuplicate(M_BaseName(soundfonts[i]));
        if (strlen(name) > NAME_MAX_LENGTH)
        {
            name[NAME_MAX_LENGTH] = '\0';
        }
        array_push(devices, M_StringJoin("FluidSynth: ", name));
        free(name);
    }

    return devices;
}

static void I_FL_BindVariables(void)
{
    M_BindStr("soundfont_dir", &soundfont_dir,
#if defined(_WIN32)
    "soundfonts",
#else
    "./soundfonts:"
    // RedHat/Fedora/Arch
    "/usr/share/soundfonts:"
    // Debian/Ubuntu/OpenSUSE
    "/usr/share/sounds/sf2:"
    "/usr/share/sounds/sf3:"
    // AppImage
    "../share/" PROJECT_SHORTNAME "/soundfonts",
#endif
    wad_no, "FluidSynth soundfont directories");
    BIND_BOOL_MIDI(mus_chorus, false, "FluidSynth chorus");
    BIND_BOOL_MIDI(mus_reverb, false, "FluidSynth reverb");
}

stream_module_t stream_fl_module =
{
    I_FL_InitStream,
    I_FL_OpenStream,
    I_FL_FillStream,
    I_FL_PlayStream,
    I_FL_CloseStream,
    I_FL_ShutdownStream,
    I_FL_DeviceList,
    I_FL_BindVariables,
};

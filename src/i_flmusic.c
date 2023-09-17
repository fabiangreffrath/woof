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

#include "fluidsynth.h"

#if (FLUIDSYNTH_VERSION_MAJOR < 2 || (FLUIDSYNTH_VERSION_MAJOR == 2 && FLUIDSYNTH_VERSION_MINOR < 2))
  typedef int fluid_int_t;
  typedef long fluid_long_long_t;
#else
  typedef fluid_long_long_t fluid_int_t;
#endif

#include "SDL.h"
#include "i_oalmusic.h"

#include "doomtype.h"
#include "i_printf.h"
#include "i_system.h"
#include "i_sound.h"
#include "m_misc2.h"
#include "memio.h"
#include "mus2mid.h"
#include "w_wad.h"
#include "z_zone.h"
#include "i_glob.h"
#include "d_iwad.h" // [FG] D_DoomExeDir()

char *soundfont_path = "";
char *soundfont_dir = "";
boolean mus_chorus;
boolean mus_reverb;
int     mus_gain = 100;

static fluid_synth_t *synth = NULL;
static fluid_settings_t *settings = NULL;
static fluid_player_t *player = NULL;

#define SOUNDFONTS_INITIAL_SIZE 16
static char **soundfonts;
static int soundfonts_num;

static uint32_t FL_Callback(uint8_t *buffer, uint32_t buffer_samples)
{
    int result;

    result = fluid_synth_write_s16(synth, buffer_samples, buffer, 0, 2, buffer, 1, 2);

    if (result != FLUID_OK)
    {
        I_Printf(VB_ERROR, "FL_Callback: Error generating FluidSynth audio");
    }

    return buffer_samples;
}

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

static void AddSoundFont(const char *path)
{
    static int size;

    if (soundfonts_num >= size)
    {
        size = (size ? size * 2 : SOUNDFONTS_INITIAL_SIZE);
        soundfonts = I_Realloc(soundfonts, size * sizeof(*soundfonts));
    }

    soundfonts[soundfonts_num++] = M_StringDuplicate(path);
}

static void ScanDir(const char *dir)
{
    char *rel = NULL;
    glob_t *glob;

    // [FG] relative to the executable directory
    if (dir[0] == '.')
    {
        rel = M_StringJoin(D_DoomExeDir(), DIR_SEPARATOR_S, dir, NULL);
        dir = rel;
    }

    glob = I_StartMultiGlob(dir, GLOB_FLAG_NOCASE|GLOB_FLAG_SORTED,
                            "*.sf2", "*.sf3", NULL);

    while(1)
    {
        const char *filename = I_NextGlob(glob);

        if (filename == NULL)
        {
            break;
        }

        AddSoundFont(filename);
    }

    I_EndGlob(glob);

    if (rel)
    {
        free(rel);
    }
}

static void GetSoundFonts(void)
{
    char *left, *p, *dup_path;

    if (soundfonts_num)
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

            ScanDir(left);

            left = p + 1;
        }
        else
        {
            break;
        }
    }

    ScanDir(left);

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

static boolean I_FL_InitMusic(int device)
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
        if (device == DEFAULT_MIDI_DEVICE)
        {
            int i;

            GetSoundFonts();

            for (i = 0; i < soundfonts_num; ++i)
            {
                if (!strcasecmp(soundfonts[i], soundfont_path))
                {
                    device = i;
                    break;
                }
            }
            if (i == soundfonts_num)
            {
                device = 0;
            }
        }

        if (soundfonts_num)
        {
            if (device >= soundfonts_num)
            {
                device = 0;
            }
            soundfont_path = soundfonts[device];
        }

        sf_id = fluid_synth_sfload(synth, soundfont_path, true);
    }

    if (sf_id == FLUID_FAILED)
    {
        char *errmsg;
        errmsg = M_StringJoin("Error loading FluidSynth soundfont: ",
            lumpnum >= 0 ? "SNDFONT lump" : soundfont_path, NULL);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, PROJECT_STRING,
            errmsg, NULL);
        free(errmsg);
        FreeSynthAndSettings();
        return false;
    }

    I_Printf(VB_INFO, "FluidSynth Init: Using '%s'.",
        lumpnum >= 0 ? "SNDFONT lump" : soundfont_path);

    return true;
}

static void I_FL_SetMusicVolume(int volume)
{
    if (synth)
    {
        // FluidSynth's default is 0.2. Make 1.0 the maximum.
        // 0 -- 0.2 -- 10.0
        fluid_synth_set_gain(synth, ((float)volume / 15) * ((float)mus_gain / 100));
    }
}

static void I_FL_PauseSong(void *handle)
{
    if (player)
    {
        I_OAL_HookMusic(NULL);
    }
}

static void I_FL_ResumeSong(void *handle)
{
    if (player)
    {
        I_OAL_HookMusic(FL_Callback);
    }
}

static void I_FL_PlaySong(void *handle, boolean looping)
{
    if (player)
    {
        fluid_player_set_loop(player, looping ? -1 : 1);
        fluid_player_play(player);
    }
}

static void I_FL_StopSong(void *handle)
{
    if (player)
    {
       fluid_player_stop(player);
    }
}

static void *I_FL_RegisterSong(void *data, int len)
{
    int result = FLUID_FAILED;

    player = new_fluid_player(synth);

    if (player == NULL)
    {
        I_Printf(VB_ERROR,
                "I_FL_InitMusic: FluidSynth failed to initialize player.");
        return NULL;
    }

    if (IsMid(data, len))
    {
        result = fluid_player_add_mem(player, data, len);
    }
    else
    {
        // Assume a MUS file and try to convert
        MEMFILE *instream;
        MEMFILE *outstream;
        void *outbuf;
        size_t outbuf_len;

        instream = mem_fopen_read(data, len);
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
        return NULL;
    }

    if (!I_OAL_HookMusic(FL_Callback))
    {
        delete_fluid_player(player);
        player = NULL;
        return NULL;
    }

    return (void *)1;
}

static void I_FL_UnRegisterSong(void *handle)
{
    if (player)
    {
        fluid_synth_program_reset(synth);
        fluid_synth_system_reset(synth);

        I_OAL_HookMusic(NULL);

        delete_fluid_player(player);
        player = NULL;
    }
}

static void I_FL_ShutdownMusic(void)
{
    I_FL_StopSong(NULL);
    I_FL_UnRegisterSong(NULL);

    FreeSynthAndSettings();
}

#define NAME_MAX_LENGTH 25

static int I_FL_DeviceList(const char *devices[], int size, int *current_device)
{
    int i;

    *current_device = 0;

    if (W_CheckNumForName("SNDFONT") >= 0)
    {
        if (size > 0)
        {
            devices[0] = "FluidSynth (SNDFONT)";
            return 1;
        }
        return 0;
    }

    GetSoundFonts();

    for (i = 0; i < size && i < soundfonts_num; ++i)
    {
        char *name = M_StringDuplicate(M_BaseName(soundfonts[i]));
        if (strlen(name) >= NAME_MAX_LENGTH)
        {
            name[NAME_MAX_LENGTH] = '\0';
        }

        devices[i] = M_StringJoin("FluidSynth (", name, ")", NULL);

        if (!strcasecmp(soundfonts[i], soundfont_path))
        {
            *current_device = i;
        }
        free(name);
    }

    return i;
}

music_module_t music_fl_module =
{
    I_FL_InitMusic,
    I_FL_ShutdownMusic,
    I_FL_SetMusicVolume,
    I_FL_PauseSong,
    I_FL_ResumeSong,
    I_FL_RegisterSong,
    I_FL_PlaySong,
    I_FL_StopSong,
    I_FL_UnRegisterSong,
    I_FL_DeviceList,
};

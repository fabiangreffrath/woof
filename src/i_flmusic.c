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

#if defined(HAVE_FLUIDSYNTH)

#include "fluidsynth.h"

#if (FLUIDSYNTH_VERSION_MAJOR < 2 || (FLUIDSYNTH_VERSION_MAJOR == 2 && FLUIDSYNTH_VERSION_MINOR < 2))
  typedef int fluid_int_t;
  typedef long fluid_long_long_t;
#else
  typedef fluid_long_long_t fluid_int_t;
#endif

#include "SDL.h"
#include "SDL_mixer.h"

#include "doomtype.h"
#include "i_system.h"
#include "i_sound.h"
#include "m_misc2.h"
#include "memio.h"
#include "mus2mid.h"
#include "w_wad.h"
#include "z_zone.h"

char *soundfont_path = "";
boolean mus_chorus;
boolean mus_reverb;
int     mus_gain = 100;

static fluid_synth_t *synth = NULL;
static fluid_settings_t *settings = NULL;
static fluid_player_t *player = NULL;

static void FL_Mix_Callback(void *udata, Uint8 *stream, int len)
{
    int result;

    result = fluid_synth_write_s16(synth, len / 4, stream, 0, 2, stream, 1, 2);

    if (result != FLUID_OK)
    {
        fprintf(stderr, "Error generating FluidSynth audio");
    }
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
    if (mem_fseek((MEMFILE *)handle, offset, origin) < 0)
    {
        return FLUID_FAILED;
    }
    return FLUID_OK;
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

static boolean I_FL_InitMusic(void)
{
    int sf_id;
    int lumpnum;

    settings = new_fluid_settings();

    fluid_settings_setnum(settings, "synth.sample-rate", snd_samplerate);

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
        sf_id = fluid_synth_sfload(synth, soundfont_path, true);
    }

    if (sf_id == FLUID_FAILED)
    {
        char *errmsg;
        // deleting the synth also deletes sfloader
        delete_fluid_synth(synth);
        delete_fluid_settings(settings);
        errmsg = M_StringJoin("Error loading FluidSynth soundfont: ",
                              lumpnum >= 0 ? "SNDFONT lump" : soundfont_path, NULL);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING,
                                 PROJECT_STRING, errmsg, NULL);
        free(errmsg);
        return false;
    }

    return true;
}

static void I_FL_SetMusicVolume(int volume)
{
    // FluidSynth's default is 0.2. Make 1.0 the maximum.
    // 0 -- 0.2 -- 10.0
    fluid_synth_set_gain(synth, ((float)volume / 15) * ((float)mus_gain / 100));
}

static void I_FL_PauseSong(void *handle)
{
    fluid_player_stop(player);
}

static void I_FL_ResumeSong(void *handle)
{
    fluid_player_play(player);
}

static void I_FL_PlaySong(void *handle, boolean looping)
{
    fluid_player_set_loop(player, looping ? -1 : 1);
    fluid_player_play(player);
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
    int result = 0;

    player = new_fluid_player(synth);

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
        fprintf(stderr, "FluidSynth failed to load in-memory song");
        return NULL;
    }

    Mix_HookMusic(FL_Mix_Callback, NULL);

    return (void *)1;
}

static void I_FL_UnRegisterSong(void *handle)
{
    if (player)
    {
        fluid_synth_program_reset(synth);
        fluid_synth_system_reset(synth);

        Mix_HookMusic(NULL, NULL);

        delete_fluid_player(player);
        player = NULL;
    }
}

static void I_FL_ShutdownMusic(void)
{
    I_FL_StopSong(NULL);
    I_FL_UnRegisterSong(NULL);

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
};

#endif

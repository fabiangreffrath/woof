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

#include <stdlib.h>
#include <string.h>

#if (FLUIDSYNTH_VERSION_MAJOR < 2 \
     || (FLUIDSYNTH_VERSION_MAJOR == 2 && FLUIDSYNTH_VERSION_MINOR < 2))
typedef int fluid_int_t;
typedef long fluid_long_long_t;
#else
typedef fluid_long_long_t fluid_int_t;
#endif

#include "d_iwad.h"
#include "doomtype.h"
#include "i_glob.h"
#include "i_oalstream.h"
#include "i_printf.h"
#include "i_sound.h"
#include "i_system.h"
#include "m_array.h"
#include "m_config.h"
#include "m_io.h"
#include "m_misc.h"
#include "memio.h"
#include "mididevice.h"
#include "midiseq.h"
#include "w_wad.h"
#include "z_zone.h"

static const char *soundfont_dirs = "";
static int fl_polyphony;
static boolean fl_interpolation;
static boolean fl_reverb;
static boolean fl_chorus;
static int fl_reverb_damp;
static int fl_reverb_level;
static int fl_reverb_roomsize;
static int fl_reverb_width;
static int fl_chorus_depth;
static int fl_chorus_level;
static int fl_chorus_nr;
static int fl_chorus_speed;
static int fl_note_cut;

static fluid_synth_t *synth = NULL;
static fluid_settings_t *settings = NULL;

// The sequencer is driven by the audio stream: the song position is the number
// of frames rendered since the start of playback.

static boolean song_loaded;
static midi_position_t position;
static uint64_t song_frames;
static boolean event_pending;
static boolean song_finished;

static const char **soundfonts = NULL;
static int interp_method;

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

static void ScanDir(const char *dir, boolean makedir)
{
    I_Printf(VB_DEBUG, "Scanning for soundfonts in %s", dir);

    if (makedir)
    {
        M_MakeDirectory(dir);
    }

    glob_t *glob = I_StartMultiGlob(dir, GLOB_FLAG_NOCASE | GLOB_FLAG_SORTED,
                                    "*.sf2", "*.sf3", "*.dls");

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
    if (array_size(soundfonts))
    {
        return;
    }

#if defined(_WIN32)
    const char *sys = M_getenv("SystemRoot");
    char *path = M_StringJoin(sys, "\\System32\\drivers\\gm.dls");
    if (M_FileExistsNotDir(path))
    {
        array_push(soundfonts, path);
    }
    else
    {
        free(path);
    }
#endif

    if (strlen(soundfont_dirs) > 0)
    {
        // Split into individual dirs within the list.
        char *dup_path = M_StringDuplicate(soundfont_dirs);
        char *left = dup_path;

        while (1)
        {
            char *p = strchr(left, PATH_SEPARATOR);
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
    else
    {
        const constructed_dir_t constructed_dirs[] = {
            {D_DoomPrefDir, "soundfonts", NULL, true},
            {D_DoomExeDir, "soundfonts", D_DoomPrefDir},
#if !defined(_WIN32)
            // RedHat/Fedora/Arch
            {NULL, "/usr/share/soundfonts"},
            {M_DataDir, "soundfonts"},
            // Debian/Ubuntu/OpenSUSE
            {NULL, "/usr/share/sounds/sf2"},
            {M_DataDir, "sounds/sf2"},
            {NULL, "/usr/share/sounds/sf3"},
            {M_DataDir, "sounds/sf3"},
            // AppImage
            {D_DoomExeDir, "../share/" PROJECT_SHORTNAME "/soundfonts"},
#endif
        };

        for (int i = 0; i < arrlen(constructed_dirs); ++i)
        {
            const constructed_dir_t d = constructed_dirs[i];

            if (d.check_func && d.func && d.check_func() == d.func())
            {
                continue;
            }

            if (d.func && d.dir)
            {
                char *dir = M_StringJoin(d.func(), DIR_SEPARATOR_S, d.dir);
                ScanDir(dir, d.makedir);
                free(dir);
            }
            else if (d.dir)
            {
                ScanDir(d.dir, d.makedir);
            }
            else if (d.func)
            {
                ScanDir(d.func(), d.makedir);
            }
        }
    }
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
    I_Printf(VB_ERROR, "FluidSynth: \"%s\"", message);
}

static void I_FL_Log_Debug(int level, const char *message, void *data)
{
    I_Printf(VB_DEBUG, "FluidSynth: \"%s\"", message);
}

static void I_FL_EventHandler(const midi_event_t *event);

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

    interp_method =
        fl_interpolation ? FLUID_INTERP_HIGHEST : FLUID_INTERP_DEFAULT;

    fluid_settings_setint(settings, "synth.polyphony", fl_polyphony);
    fluid_settings_setnum(settings, "synth.gain", 0.2);
    fluid_settings_setnum(settings, "synth.sample-rate", SND_SAMPLERATE);
    fluid_settings_setint(settings, "synth.device-id", 0); // Disable SysEx.
    fluid_settings_setstr(settings, "synth.midi-bank-select", "gs");
    fluid_settings_setint(settings, "synth.reverb.active", fl_reverb);
    fluid_settings_setint(settings, "synth.chorus.active", fl_chorus);
    fluid_settings_setint(settings, "synth.note-cut", fl_note_cut);

    if (fl_reverb)
    {
        fluid_settings_setnum(settings, "synth.reverb.damp",
                              fl_reverb_damp / 100.0);
        fluid_settings_setnum(settings, "synth.reverb.level",
                              fl_reverb_level / 100.0);
        fluid_settings_setnum(settings, "synth.reverb.room-size",
                              fl_reverb_roomsize / 100.0);
        fluid_settings_setnum(settings, "synth.reverb.width",
                              fl_reverb_width / 100.0);
    }

    if (fl_chorus)
    {
        fluid_settings_setnum(settings, "synth.chorus.depth",
                              fl_chorus_depth / 100.0);
        fluid_settings_setnum(settings, "synth.chorus.level",
                              fl_chorus_level / 100.0);
        fluid_settings_setint(settings, "synth.chorus.nr", fl_chorus_nr);
        fluid_settings_setnum(settings, "synth.chorus.speed",
                              fl_chorus_speed / 100.0);
    }

    synth = new_fluid_synth(settings);

    if (synth == NULL)
    {
        I_Printf(VB_ERROR, "FluidSynth: Failed to initialize synth.");
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
        I_MessageBox("Error loading FluidSynth soundfont: %s",
                     lumpnum >= 0 ? "SNDFONT lump" : soundfonts[device]);
        FreeSynthAndSettings();
        return false;
    }

    I_Printf(VB_INFO, "FluidSynth Init: Using '%s'.",
             lumpnum >= 0 ? "SNDFONT lump" : soundfonts[device]);

    MIDISeq_SetEventHandler(I_FL_EventHandler);

    return true;
}

static const char *music_format = "Unknown";

// Maps a song event onto the synthesizer. Unlike native MIDI output, the events
// are sent as is: FluidSynth is a General MIDI synth which handles bank select
// and instrument fallback itself.

static void I_FL_EventHandler(const midi_event_t *event)
{
    if (event->event_type == MIDI_EVENT_SYSEX)
    {
        // SysEx is disabled for the synth, see "synth.device-id".
        return;
    }

    const int channel = event->data.channel.channel;

    switch (event->event_type)
    {
        case MIDI_EVENT_NOTE_OFF:
            fluid_synth_noteoff(synth, channel, event->data.channel.param1);
            break;
        case MIDI_EVENT_NOTE_ON:
            fluid_synth_noteon(synth, channel, event->data.channel.param1,
                               event->data.channel.param2);
            break;
        case MIDI_EVENT_CONTROLLER:
            fluid_synth_cc(synth, channel, event->data.channel.param1,
                           event->data.channel.param2);
            break;
        case MIDI_EVENT_PROGRAM_CHANGE:
            fluid_synth_program_change(synth, channel,
                                       event->data.channel.param1);
            break;
        case MIDI_EVENT_PITCH_BEND:
            fluid_synth_pitch_bend(synth, channel,
                                   event->data.channel.param1
                                       | (event->data.channel.param2 << 7));
            break;
        case MIDI_EVENT_CHAN_AFTERTOUCH:
            fluid_synth_channel_pressure(synth, channel,
                                         event->data.channel.param1);
            break;
        case MIDI_EVENT_AFTERTOUCH:
            fluid_synth_key_pressure(synth, channel,
                                     event->data.channel.param1,
                                     event->data.channel.param2);
            break;
        default:
            break;
    }
}

static boolean I_FL_OpenStream(void *data, ALsizei size, ALenum *format,
                               ALsizei *freq, ALsizei *frame_size)
{
    if (!synth)
    {
        return false;
    }

    if (!IsMid(data, size) && !IsMus(data, size))
    {
        return false;
    }

    if (!MIDISeq_RegisterSong(data, size))
    {
        return false;
    }

    music_format = IsMid(data, size) ? "MIDI (FluidSynth)" : "MUS (FluidSynth)";

    song_loaded = true;

    *format = AL_FORMAT_STEREO_FLOAT32;
    *freq = SND_SAMPLERATE;
    *frame_size = 2 * sizeof(float);

    return true;
}

static uint64_t USToFrames(uint64_t us)
{
    return (us * SND_SAMPLERATE + 500000) / 1000000;
}

static void RenderFrames(float *buffer, int frames)
{
    if (fluid_synth_write_float(synth, frames, buffer, 0, 2, buffer, 1, 2)
        != FLUID_OK)
    {
        I_Printf(VB_ERROR, "FluidSynth: Error generating audio");
    }
}

// Renders the stream, sending the song events to the synth at the exact frame
// at which they are due.

static int I_FL_FillStream(void *buffer, int buffer_samples)
{
    if (!synth)
    {
        memset(buffer, 0, buffer_samples * 2 * sizeof(float));
        return buffer_samples;
    }

    float *out = buffer;
    int frames_left = buffer_samples;

    while (frames_left > 0)
    {
        if (!event_pending && !song_finished)
        {
            if (MIDISeq_NextEvent(&position))
            {
                event_pending = true;
            }
            else
            {
                song_finished = true;
            }
        }

        int frames = frames_left;

        if (event_pending)
        {
            const uint64_t event_frame = USToFrames(position.time_us);

            if (event_frame > song_frames)
            {
                const uint64_t wait = event_frame - song_frames;

                if (wait < (uint64_t)frames)
                {
                    frames = (int)wait;
                }
            }
            else
            {
                MIDISeq_SendEvent(&position);
                event_pending = false;
                continue;
            }
        }

        RenderFrames(out, frames);

        out += frames * 2;
        song_frames += frames;
        frames_left -= frames;
    }

    return buffer_samples;
}

static void I_FL_PlayStream(boolean looping)
{
    if (!synth || !song_loaded)
    {
        return;
    }

    fluid_synth_set_interp_method(synth, -1, interp_method);

    // The output volume is applied by OpenAL, not by the sequencer.
    MIDIDevice_SetVolumeFactor(1.0f);

    fluid_synth_system_reset(synth);

    MIDISeq_Play(looping);

    song_frames = 0;
    event_pending = false;
    song_finished = false;
}

static void I_FL_CloseStream(void)
{
    if (!synth)
    {
        return;
    }

    MIDISeq_UnregisterSong();
    song_loaded = false;
    event_pending = false;
    song_finished = true;

    fluid_synth_program_reset(synth);
    fluid_synth_system_reset(synth);
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
    M_BindStr("soundfont_dirs", &soundfont_dirs, "", wad_no,
        "[FluidSynth] Soundfont directories");
    BIND_NUM(fl_polyphony, 256, 1, 65535,
        "[FluidSynth] Number of voices that can be played in parallel");
    BIND_BOOL(fl_interpolation, false,
        "[FluidSynth] Interpolation method (0 = Default; 1 = Highest Quality)");
    BIND_BOOL_MUSIC(fl_reverb, false,
        "[FluidSynth] Enable reverb effects");
    BIND_BOOL_MUSIC(fl_chorus, false,
        "[FluidSynth] Enable chorus effects");
    BIND_NUM(fl_reverb_damp, 30, 0, 100,
        "[FluidSynth] Reverb damping");
    BIND_NUM(fl_reverb_level, 70, 0, 100,
        "[FluidSynth] Reverb output level");
    BIND_NUM(fl_reverb_roomsize, 50, 0, 100,
        "[FluidSynth] Reverb room size");
    BIND_NUM(fl_reverb_width, 80, 0, 10000,
        "[FluidSynth] Reverb width (stereo spread)");
    BIND_NUM(fl_chorus_depth, 360, 0, 25600,
        "[FluidSynth] Chorus modulation depth");
    BIND_NUM(fl_chorus_level, 55, 0, 1000,
        "[FluidSynth] Chorus output level");
    BIND_NUM(fl_chorus_nr, 4, 0, 99,
        "[FluidSynth] Chorus voice count");
    BIND_NUM(fl_chorus_speed, 36, 10, 500,
        "[FluidSynth] Chorus modulation speed");
    BIND_NUM(fl_note_cut, 1, 0, 2,
        "[FluidSynth] Note cut mode for compatibility with "
        "MS GS Wavetable Synth (0 = Off; 1 = Drums; 2 = All)");
}

static const char *I_FL_MusicFormat(void)
{
    return music_format;
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
    I_FL_MusicFormat,
};

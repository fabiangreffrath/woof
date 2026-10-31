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
//
// DESCRIPTION:
//      CLAP plugin host for MIDI music playback
//

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "clap/clap.h"

#include "d_iwad.h"
#include "doomtype.h"
#include "i_glob.h"
#include "i_oalstream.h"
#include "i_printf.h"
#include "i_sound.h"
#include "i_system.h"
#include "m_array.h"
#include "m_misc.h"
#include "mididevice.h"
#include "midiseq.h"

#define CLAP_MAX_FRAMES     8192
#define CLAP_BUFFER_SAMPLES 4096

static const char *music_format;

typedef struct
{
    void *handle;
    const clap_plugin_entry_t *entry;
    const clap_plugin_factory_t *factory;
} clap_library_t;

typedef struct
{
    const clap_plugin_t *plugin;
    clap_library_t *library;

    clap_audio_buffer_t audio_in;
    clap_audio_buffer_t audio_out;
    float audio_in_data[2][CLAP_MAX_FRAMES];
    float audio_out_data[2][CLAP_MAX_FRAMES];
    float *audio_in_channels[2];
    float *audio_out_channels[2];

    clap_process_t process;
    boolean active;
    boolean processing;
} clap_plugin_instance_t;

typedef struct
{
    uint8_t *data;
    int len;
    union
    {
        clap_event_midi_t midi;
        clap_event_midi_sysex_t sysex;
    } event;
} clap_midi_event_t;

typedef struct
{
    clap_midi_event_t *events;
    int count;
    int capacity;
} clap_event_list_t;

typedef struct
{
    clap_plugin_instance_t *plugin;
    clap_library_t *library;
    clap_event_list_t event_list;

    midi_position_t position;
    boolean event_pending;
    boolean song_finished;
    boolean looping;
    uint64_t song_frames;

    ALenum format;
    ALsizei freq;
    ALsizei frame_size;
    int channels;
} clap_stream_t;

static clap_stream_t clap_stream = {0};
static boolean clap_initialized = false;
static const char **clap_plugins = NULL;

static clap_host_t clap_host;

static const void *ClapHostGetExtension(const clap_host_t *host, const char *extension_id)
{
    (void)host;
    (void)extension_id;
    return NULL;
}

static void ClapHostRequestRestart(const clap_host_t *host)
{
    (void)host;
}

static void ClapHostRequestProcess(const clap_host_t *host)
{
    (void)host;
}

static void ClapHostRequestCallback(const clap_host_t *host)
{
    (void)host;
}

static void InitClapHost(void)
{
    clap_host.clap_version = CLAP_VERSION;
    clap_host.host_data = NULL;
    clap_host.name = PROJECT_NAME;
    clap_host.vendor = "The Woof Project Team";
    clap_host.url = "https://github.com/fabiangreffrath/woof";
    clap_host.version = "1.0";
    clap_host.get_extension = ClapHostGetExtension;
    clap_host.request_restart = ClapHostRequestRestart;
    clap_host.request_process = ClapHostRequestProcess;
    clap_host.request_callback = ClapHostRequestCallback;
}

static void I_CLAP_CloseStream(void);

static boolean LoadClapLibrary(clap_library_t *lib, const char *path);

static void ScanPluginsDir(const char *dir)
{
    I_Printf(VB_DEBUG, "Scanning for CLAP plugins in %s", dir);

    glob_t *glob = I_StartMultiGlob(dir, GLOB_FLAG_NOCASE | GLOB_FLAG_SORTED,
                                    "*.clap");

    while (1)
    {
        const char *filename = I_NextGlob(glob);

        if (filename == NULL)
        {
            break;
        }

        array_push(clap_plugins, M_StringDuplicate(filename));
    }

    I_EndGlob(glob);
}

static void GetClapPlugins(void)
{
    if (array_size(clap_plugins))
    {
        return;
    }

    const constructed_dir_t constructed_dirs[] = {
        {D_DoomPrefDir, "plugins", NULL, true},
        {D_DoomExeDir, "plugins", D_DoomPrefDir},
#if !defined(_WIN32)
        {M_DataDir, "plugins"},
        {NULL, "/usr/lib/clap"},
        {NULL, "/usr/local/lib/clap"},
        {D_DoomExeDir, "../share/" PROJECT_SHORTNAME "/plugins"},
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
            ScanPluginsDir(dir);
            free(dir);
        }
        else if (d.dir)
        {
            ScanPluginsDir(d.dir);
        }
        else if (d.func)
        {
            ScanPluginsDir(d.func());
        }
    }
}
    
static boolean LoadClapLibrary(clap_library_t *lib, const char *path)
{
    lib->handle = I_LoadObject(path);

    if (!lib->handle)
    {
        I_Printf(VB_ERROR, "CLAP: Failed to load plugin library '%s'", path);
        return false;
    }

    lib->entry = I_LoadFunction(lib->handle, "clap_entry");
    if (!lib->entry)
    {
        I_Printf(VB_ERROR,
                 "CLAP: Invalid plugin library '%s', cannot find 'clap_entry' "
                 "symbol",
                 path);
        I_UnloadObject(lib->handle);
        lib->handle = NULL;
        return false;
    }

    if (!clap_version_is_compatible(lib->entry->clap_version))
    {
        I_Printf(VB_ERROR, "CLAP: Incompatible API version %u.%u.%u in '%s'",
                 lib->entry->clap_version.major, lib->entry->clap_version.minor,
                 lib->entry->clap_version.revision, path);
        I_UnloadObject(lib->handle);
        lib->handle = NULL;
        return false;
    }

    if (!lib->entry->init(path))
    {
        I_Printf(VB_ERROR, "CLAP: Failed to initialize plugin library '%s'",
                 path);
        I_UnloadObject(lib->handle);
        lib->handle = NULL;
        return false;
    }

    lib->factory = lib->entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
    if (!lib->factory)
    {
        I_Printf(VB_ERROR,
            "CLAP: Plugin library '%s' does not provide a plugin factory", path);
        lib->entry->deinit();
        I_UnloadObject(lib->handle);
        lib->handle = NULL;
        return false;
    }

    return true;
}

static void UnloadClapLibrary(clap_library_t *lib)
{
    if (lib->entry)
    {
        lib->entry->deinit();
        lib->entry = NULL;
    }
    if (lib->handle)
    {
        I_UnloadObject(lib->handle);
        lib->handle = NULL;
    }
    lib->factory = NULL;
}

static boolean ValidateNotePorts(const clap_plugin_t *plugin)
{
    const clap_plugin_note_ports_t *note_ports = plugin->get_extension(
        plugin, CLAP_EXT_NOTE_PORTS);

    if (!note_ports)
    {
        I_Printf(VB_DEBUG, "CLAP: Only plugins that implement the note ports "
                           "extension are supported");
        return false;
    }

    if (note_ports->count(plugin, true) == 0)
    {
        I_Printf(VB_DEBUG,
                 "CLAP: Plugins must have at least one MIDI input port");
        return false;
    }

    clap_note_port_info_t info = {0};
    if (!note_ports->get(plugin, 0, true, &info))
    {
        I_Printf(VB_DEBUG, "CLAP: Cannot get the first note input port");
        return false;
    }

    if ((info.supported_dialects & CLAP_NOTE_DIALECT_MIDI) == 0)
    {
        I_Printf(VB_DEBUG,
            "CLAP: The first note input port must support the MIDI dialect");
        return false;
    }

    return true;
}

static boolean ValidateAudioPorts(const clap_plugin_t *plugin,
                                  uint32_t *input_channel_counts,
                                  uint32_t *input_port_count,
                                  uint32_t *output_channel_counts,
                                  uint32_t *output_port_count)
{
    const clap_plugin_audio_ports_t *audio_ports =
        plugin->get_extension(plugin, CLAP_EXT_AUDIO_PORTS);

    if (!audio_ports)
    {
        I_Printf(VB_DEBUG, "CLAP: Only plugins that implement the audio ports "
                           "extension are supported");
        return false;
    }

    uint32_t num_in_ports = audio_ports->count(plugin, true);
    *input_port_count = num_in_ports;

    for (uint32_t i = 0; i < num_in_ports; ++i)
    {
        clap_audio_port_info_t port = {0};
        if (!audio_ports->get(plugin, i, true, &port)
            || port.channel_count == 0)
        {
            I_Printf(VB_DEBUG, "CLAP: Invalid audio input port %u", i);
            return false;
        }
        input_channel_counts[i] = port.channel_count;
    }

    uint32_t num_out_ports = audio_ports->count(plugin, false);
    if (num_out_ports == 0)
    {
        I_Printf(VB_DEBUG,
                 "CLAP: Plugins must have at least one audio output port");
        return false;
    }
    *output_port_count = num_out_ports;

    for (uint32_t i = 0; i < num_out_ports; ++i)
    {
        clap_audio_port_info_t port = {0};
        if (!audio_ports->get(plugin, i, false, &port)
            || port.channel_count == 0)
        {
            I_Printf(VB_DEBUG, "CLAP: Invalid audio output port %u", i);
            return false;
        }

        if (i == 0
            && !(port.channel_count == 2 && port.port_type
                 && strcmp(port.port_type, CLAP_PORT_STEREO) == 0))
        {
            I_Printf(VB_DEBUG, "CLAP: The first audio output port must be "
                               "stereo with two channels");
            return false;
        }

        output_channel_counts[i] = port.channel_count;
    }

    return true;
}

static clap_plugin_instance_t *CreatePluginInstance(clap_library_t *lib,
                                                    const char *plugin_id)
{
    if (!lib->factory)
    {
        return NULL;
    }

    const clap_plugin_t *plugin =
        lib->factory->create_plugin(lib->factory, &clap_host, plugin_id);
    if (!plugin)
    {
        I_Printf(VB_ERROR, "CLAP: Error creating plugin with ID '%s'",
                 plugin_id);
        return NULL;
    }

    if (!plugin->init(plugin))
    {
        I_Printf(VB_DEBUG, "CLAP: Error initializing plugin with ID '%s'",
                 plugin_id);
        plugin->destroy(plugin);
        return NULL;
    }

    if (!ValidateNotePorts(plugin))
    {
        plugin->destroy(plugin);
        return NULL;
    }

    uint32_t input_channel_counts[16];
    uint32_t output_channel_counts[16];
    uint32_t input_port_count, output_port_count;

    if (!ValidateAudioPorts(plugin, input_channel_counts, &input_port_count,
                              output_channel_counts, &output_port_count))
    {
        plugin->destroy(plugin);
        return NULL;
    }

    clap_plugin_instance_t *instance = calloc(1, sizeof(clap_plugin_instance_t));
    if (!instance)
    {
        plugin->destroy(plugin);
        return NULL;
    }

    instance->plugin = plugin;
    instance->library = lib;

    for (uint32_t i = 0; i < input_port_count; ++i)
    {
        instance->audio_in_data[i][0] = 0.0f;
        instance->audio_in_channels[i] = instance->audio_in_data[i];
    }
    instance->audio_in.data32 = instance->audio_in_channels;
    instance->audio_in.channel_count = 0;
    for (uint32_t i = 0; i < input_port_count; ++i)
    {
        instance->audio_in.channel_count += input_channel_counts[i];
    }

    for (uint32_t i = 0; i < output_port_count; ++i)
    {
        instance->audio_out_data[i][0] = 0.0f;
        instance->audio_out_channels[i] = instance->audio_out_data[i];
    }
    instance->audio_out.data32 = instance->audio_out_channels;
    instance->audio_out.channel_count = 0;
    for (uint32_t i = 0; i < output_port_count; ++i)
    {
        instance->audio_out.channel_count += output_channel_counts[i];
    }

    instance->process.audio_inputs =
        instance->audio_in.channel_count > 0 ? &instance->audio_in : NULL;
    instance->process.audio_inputs_count = input_port_count;
    instance->process.audio_outputs = &instance->audio_out;
    instance->process.audio_outputs_count = output_port_count;

    I_Printf(VB_INFO, "CLAP: Plugin '%s' loaded", plugin_id);

    return instance;
}

static void DestroyPluginInstance(clap_plugin_instance_t *instance)
{
    if (!instance)
    {
        return;
    }

    if (instance->active)
    {
        instance->plugin->deactivate(instance->plugin);
        instance->active = false;
    }

    if (instance->processing)
    {
        instance->plugin->stop_processing(instance->plugin);
        instance->processing = false;
    }

    instance->plugin->destroy(instance->plugin);
    free(instance);
}

static void ClapEventListInit(clap_event_list_t *list)
{
    list->events = NULL;
    list->count = 0;
    list->capacity = 0;
}

static void ClapEventListClear(clap_event_list_t *list)
{
    for (int i = 0; i < list->count; ++i)
    {
        free(list->events[i].data);
    }
    list->count = 0;
}

static void ClapEventListAddMsg(clap_event_list_t *list,
                                const uint8_t *msg, int len,
                                uint32_t sample_offset)
{
    if (len <= 0)
    {
        return;
    }

    if (list->count >= list->capacity)
    {
        int new_capacity = list->capacity ? list->capacity * 2 : 256;
        clap_midi_event_t *new_events =
            I_Realloc(list->events, new_capacity * sizeof(clap_midi_event_t));
        list->events = new_events;
        list->capacity = new_capacity;
    }

    clap_midi_event_t *ev = &list->events[list->count];
    ev->data = malloc(len);
    if (!ev->data)
    {
        return;
    }
    memcpy(ev->data, msg, len);
    ev->len = len;

    if (msg[0] == 0xF0)
    {
        clap_event_midi_sysex_t *sev = &ev->event.sysex;
        memset(sev, 0, sizeof(*sev));
        sev->header.size = sizeof(*sev);
        sev->header.time = sample_offset;
        sev->header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        sev->header.type = CLAP_EVENT_MIDI_SYSEX;
        sev->buffer = ev->data;
        sev->size = len;
    }
    else
    {
        clap_event_midi_t *mev = &ev->event.midi;
        memset(mev, 0, sizeof(*mev));
        mev->header.size = sizeof(*mev);
        mev->header.time = sample_offset;
        mev->header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        mev->header.type = CLAP_EVENT_MIDI;
        for (int i = 0; i < len && i < 3; ++i)
        {
            mev->data[i] = ev->data[i];
        }
    }

    list->count++;
}

static void ClapEventListFree(clap_event_list_t *list)
{
    for (int i = 0; i < list->count; ++i)
    {
        free(list->events[i].data);
    }
    free(list->events);
    list->events = NULL;
    list->count = 0;
    list->capacity = 0;
}

static uint32_t ClapInputEventsSize(const struct clap_input_events *list)
{
    clap_event_list_t *evlist = (clap_event_list_t *)list->ctx;
    return evlist->count;
}

static const clap_event_header_t *
ClapInputEventsGet(const struct clap_input_events *list, uint32_t index)
{
    clap_event_list_t *evlist = (clap_event_list_t *)list->ctx;
    if (index >= (uint32_t)evlist->count)
    {
        return NULL;
    }

    clap_midi_event_t *ev = &evlist->events[index];

    if (ev->data[0] == 0xF0)
    {
        return (const clap_event_header_t *)&ev->event.sysex;
    }

    return (const clap_event_header_t *)&ev->event.midi;
}

static bool ClapOutputEventsTryPush(const struct clap_output_events *list,
                                    const clap_event_header_t *event)
{
    (void)list;
    (void)event;
    return false;
}

static clap_input_events_t clap_input_events = {
    .ctx = NULL,
    .size = ClapInputEventsSize,
    .get = ClapInputEventsGet
};

static clap_output_events_t clap_output_events = {
    .ctx = NULL,
    .try_push = ClapOutputEventsTryPush
};

static boolean I_CLAP_InitStream(int device)
{
    if (clap_initialized)
    {
        return true;
    }

    InitClapHost();

    GetClapPlugins();

    if (device >= array_size(clap_plugins))
    {
        I_Printf(VB_ERROR, "CLAP: Invalid plugin device index %d", device);
        return false;
    }

    const char *plugin_path = clap_plugins[device];

    clap_library_t *lib = calloc(1, sizeof(clap_library_t));
    if (!lib)
    {
        return false;
    }

    if (!LoadClapLibrary(lib, plugin_path))
    {
        free(lib);
        return false;
    }

    uint32_t plugin_count = lib->factory->get_plugin_count(lib->factory);
    boolean found = false;

    for (uint32_t i = 0; i < plugin_count; ++i)
    {
        const clap_plugin_descriptor_t *desc =
            lib->factory->get_plugin_descriptor(lib->factory, i);
        if (!desc || !desc->id || !*desc->id || !desc->name || !*desc->name)
        {
            continue;
        }

        boolean has_instrument = false;
        boolean has_synthesizer = false;
        boolean has_stereo = false;

        if (desc->features)
        {
            for (const char *const *f = desc->features; *f; ++f)
            {
                if (strcmp(*f, CLAP_PLUGIN_FEATURE_INSTRUMENT) == 0)
                {
                    has_instrument = true;
                }
                else if (strcmp(*f, CLAP_PLUGIN_FEATURE_SYNTHESIZER) == 0)
                {
                    has_synthesizer = true;
                }
                else if (strcmp(*f, CLAP_PLUGIN_FEATURE_STEREO) == 0)
                {
                    has_stereo = true;
                }
            }
        }

        if (has_instrument && has_synthesizer && has_stereo)
        {
            clap_stream.plugin = CreatePluginInstance(lib, desc->id);
            if (clap_stream.plugin)
            {
                found = true;
                I_Printf(VB_INFO, "CLAP: Using plugin '%s' (version %s)",
                         desc->name, desc->version ? desc->version : "unknown");
                break;
            }
        }
    }

    if (!found)
    {
        I_Printf(VB_ERROR, "CLAP: No suitable instrument plugin found in '%s'", plugin_path);
        UnloadClapLibrary(lib);
        free(lib);
        return false;
    }

    clap_stream.library = lib;
    ClapEventListInit(&clap_stream.event_list);
    clap_initialized = true;

    return true;
}

static void I_CLAP_CloseStream(void)
{
    if (!clap_initialized)
    {
        return;
    }

    MIDISeq_UnregisterSong();

    if (clap_stream.plugin)
    {
        // Reset the plugin state (clears buffers, kills voices) but keep it
        // active
        clap_stream.plugin->plugin->reset(clap_stream.plugin->plugin);
    }

    ClapEventListClear(&clap_stream.event_list);
    clap_stream.song_frames = 0;
    clap_stream.event_pending = false;
    clap_stream.song_finished = true;
}

static void I_CLAP_ShutdownStream(void)
{
    if (!clap_initialized)
    {
        return;
    }

    I_CLAP_CloseStream();

    if (clap_stream.plugin)
    {
        DestroyPluginInstance(clap_stream.plugin);
        clap_stream.plugin = NULL;
    }

    if (clap_stream.library)
    {
        UnloadClapLibrary(clap_stream.library);
        free(clap_stream.library);
        clap_stream.library = NULL;
    }

    ClapEventListFree(&clap_stream.event_list);

    // Free the plugin list
    for (int i = 0; i < array_size(clap_plugins); ++i)
    {
        free((char *)clap_plugins[i]);
    }
    array_free(clap_plugins);
    clap_plugins = NULL;

    MIDIDevice_SetSink(NULL);

    clap_initialized = false;
}

static void I_CLAP_SendShortMsg(const byte *message, unsigned int length)
{
    ClapEventListAddMsg(&clap_stream.event_list, message, length, 0);
}

static void I_CLAP_SendLongMsg(const byte *message, unsigned int length)
{
    ClapEventListAddMsg(&clap_stream.event_list, message, length, 0);
}

// The plugin processes all events of a block at once, there is no need to
// wait for it to execute a reset.

static const midi_output_sink_t clap_sink =
{
    I_CLAP_SendShortMsg,
    I_CLAP_SendLongMsg,
    NULL,
    NULL,
};

static boolean I_CLAP_OpenStream(void *data, ALsizei size, ALenum *format,
                                 ALsizei *freq, ALsizei *frame_size)
{
    if (!clap_initialized || !clap_stream.plugin)
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

    music_format = IsMid(data, size) ? "MIDI (CLAP)" : "MUS (CLAP)";

    MIDIDevice_SetSink(&clap_sink);
    MIDISeq_SetEventHandler(MIDIDevice_HandleEvent);

    clap_stream.format = AL_FORMAT_STEREO_FLOAT32;
    clap_stream.freq = 32000;
    clap_stream.frame_size = 2 * sizeof(float);
    clap_stream.channels = 2;

    *format = clap_stream.format;
    *freq = clap_stream.freq;
    *frame_size = clap_stream.frame_size;

    return true;
}

static int I_CLAP_FillStream(void *buffer, int frames)
{
    if (!clap_initialized || !clap_stream.plugin || !clap_stream.plugin->active)
    {
        memset(buffer, 0, frames * clap_stream.frame_size);
        return frames;
    }

    float *out = (float *)buffer;
    int frames_left = frames;

    while (frames_left > 0)
    {
        // Get next event if none pending
        if (!clap_stream.event_pending && !clap_stream.song_finished)
        {
            if (MIDISeq_NextEvent(&clap_stream.position))
            {
                clap_stream.event_pending = true;
            }
            else
            {
                clap_stream.song_finished = true;
            }
        }

        int frames_to_process = frames_left;

        if (clap_stream.event_pending && !clap_stream.song_finished)
        {
            uint64_t event_frame =
                (clap_stream.position.time_us * clap_stream.freq + 500000)
                / 1000000;
            if (event_frame > clap_stream.song_frames)
            {
                uint64_t wait = event_frame - clap_stream.song_frames;
                if (wait < (uint64_t)frames_to_process)
                {
                    frames_to_process = (int)wait;
                }
            }
            else
            {
                // Send the event to the custom event handler (which adds to
                // CLAP event list)
                MIDISeq_SendEvent(&clap_stream.position);
                clap_stream.event_pending = false;
                continue;
            }
        }

        if (frames_to_process > 0)
        {
            // Create temporary deinterleaved output buffers for the plugin
            // The plugin expects separate channel buffers (non-interleaved)
            static float temp_left[CLAP_BUFFER_SAMPLES];
            static float temp_right[CLAP_BUFFER_SAMPLES];
            float *temp_channels[2] = {temp_left, temp_right};

            clap_stream.plugin->process.frames_count = frames_to_process;
            clap_stream.plugin->process.steady_time = -1;
            clap_stream.plugin->process.transport = NULL;
            clap_stream.plugin->process.in_events = &clap_input_events;
            clap_stream.plugin->process.out_events = &clap_output_events;
            clap_input_events.ctx = &clap_stream.event_list;
            clap_output_events.ctx = &clap_stream.event_list;

            // Set up output buffers to point to our temporary deinterleaved
            // buffers
            clap_stream.plugin->audio_out.data32 = temp_channels;
            clap_stream.plugin->audio_out.channel_count = 2;
            clap_stream.plugin->audio_out.constant_mask = 0;
            clap_stream.plugin->process.audio_outputs =
                &clap_stream.plugin->audio_out;
            clap_stream.plugin->process.audio_outputs_count = 1;

            clap_stream.plugin->plugin->process(clap_stream.plugin->plugin,
                                                &clap_stream.plugin->process);

            // Interleave the stereo output from temporary buffers to the OpenAL
            // buffer
            for (int i = 0; i < frames_to_process; ++i)
            {
                *out++ = temp_left[i];
                *out++ = temp_right[i];
            }

            ClapEventListClear(&clap_stream.event_list);
            clap_stream.song_frames += frames_to_process;
            frames_left -= frames_to_process;
        }
    }

    return frames;
}

static void I_CLAP_PlayStream(boolean looping)
{
    if (!clap_initialized || !clap_stream.plugin)
    {
        return;
    }

    clap_stream.looping = looping;
    clap_stream.song_frames = 0;
    clap_stream.event_pending = false;
    clap_stream.song_finished = false;
    ClapEventListClear(&clap_stream.event_list);

    // Reset the plugin state before starting new playback (clears buffers,
    // kills voices)
    clap_stream.plugin->plugin->reset(clap_stream.plugin->plugin);

    // The output volume is applied by OpenAL, not by the sequencer.
    MIDIDevice_SetVolumeFactor(1.0f);

    // The device reset messages are queued and sent with the first block.
    MIDIDevice_Reset(true);

    MIDISeq_Play(looping);

    if (!clap_stream.plugin->active)
    {
        if (!clap_stream.plugin->plugin->activate(clap_stream.plugin->plugin,
                                                  32000.0, 1, CLAP_MAX_FRAMES))
        {
            I_Printf(VB_ERROR, "CLAP: Plugin activation failed");
            return;
        }
        clap_stream.plugin->active = true;
    }

    if (!clap_stream.plugin->processing)
    {
        if (!clap_stream.plugin->plugin->start_processing(
                clap_stream.plugin->plugin))
        {
            I_Printf(VB_ERROR, "CLAP: Plugin failed to start processing");
            return;
        }
        clap_stream.plugin->processing = true;
    }
}

static const char **I_CLAP_DeviceList(void)
{
    static const char **devices = NULL;

    if (array_size(devices))
    {
        return devices;
    }

    GetClapPlugins();

    for (int i = 0; i < array_size(clap_plugins); ++i)
    {
        char *name = M_StringDuplicate(M_BaseName(clap_plugins[i]));
        array_push(devices, M_StringJoin("CLAP: ", name));
        free(name);
    }

    return devices;
}

static void I_CLAP_BindVariables(void)
{
}

static const char *I_CLAP_MusicFormat(void)
{
    return music_format;
}

stream_module_t stream_clap_module = {
    I_CLAP_InitStream,
    I_CLAP_OpenStream,
    I_CLAP_FillStream,
    I_CLAP_PlayStream,
    I_CLAP_CloseStream,
    I_CLAP_ShutdownStream,
    I_CLAP_DeviceList,
    I_CLAP_BindVariables,
    I_CLAP_MusicFormat,
};

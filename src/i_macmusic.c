//
// Copyright(C) 2009 Ryan C. Gordon
// Copyright(C) 2023 Roman Fomin
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#include "doomtype.h"
#include "i_sound.h"
#include "memio.h"

#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>
#include <AvailabilityMacros.h>

// Native Midi song
typedef struct
{
    MusicPlayer player;
    MusicSequence sequence;
    AudioUnit audiounit;
} native_midi_song_t;

static native_midi_song_t *song;

static OSStatus GetSequenceAudioUnitMatching(MusicSequence sequence,
                                  AudioUnit *aunit, OSType type, OSType subtype)
{
    AUGraph graph;
    UInt32 nodecount, i;
    OSStatus err;

    err = MusicSequenceGetAUGraph(sequence, &graph);
    if (err != noErr)
        return err;

    err = AUGraphGetNodeCount(graph, &nodecount);
    if (err != noErr)
        return err;

    for (i = 0; i < nodecount; i++)
    {
        AUNode node;
        AudioComponentDescription desc;

        if (AUGraphGetIndNode(graph, i, &node) != noErr)
            continue;  // better luck next time.

        if (AUGraphNodeInfo(graph, node, &desc, aunit) != noErr)
            continue;
        else if (desc.componentType != type)
            continue;
        else if (desc.componentSubType != subtype)
            continue;

        return noErr;  // found it!
    }

    *aunit = NULL;
    return kAUGraphErr_NodeNotFound;
}

typedef struct {
    AudioUnit aunit;
    CFURLRef default_url;
} macosx_load_soundfont_ctx_t;

static void SetSequenceSoundFont(MusicSequence sequence)
{
    OSStatus err;
    macosx_load_soundfont_ctx_t ctx;
    ctx.default_url = NULL;

    CFBundleRef bundle = CFBundleGetBundleWithIdentifier(
                                   CFSTR("com.apple.audio.units.Components"));
    if (bundle)
        ctx.default_url = CFBundleCopyResourceURL(bundle,
                                                  CFSTR("gs_instruments"),
                                                  CFSTR("dls"), NULL);

    err = GetSequenceAudioUnitMatching(sequence, &ctx.aunit,
                                 kAudioUnitType_MusicDevice,
                                 kAudioUnitSubType_DLSSynth);
    if (err != noErr)
        return;

    if (ctx.default_url)
        CFRelease(ctx.default_url);
}

static boolean I_MAC_InitMusic(int device)
{
    return true;
}

static void I_MAC_SetMusicVolume(int volume)
{
    static int latched_volume = -1;

    if (latched_volume == volume)
        return;

    latched_volume = volume;

    if (song && song->audiounit)
    {
        AudioUnitSetParameter(song->audiounit, kHALOutputParam_Volume,
                              kAudioUnitScope_Global, 0, (float) volume / 15, 0);
    }
}

static void I_MAC_PauseSong(void *handle)
{
    if (song)
        MusicPlayerStop(song->player);
}

static void I_MAC_ResumeSong(void *handle)
{
    if (song)
        MusicPlayerStart(song->player);
}

static void I_MAC_PlaySong(void *handle, boolean looping)
{
    uint32_t i, ntracks;

    if (song == NULL)
        return;

    MusicPlayerPreroll(song->player);
    GetSequenceAudioUnitMatching(song->sequence, &song->audiounit,
                                 kAudioUnitType_Output,
                                 kAudioUnitSubType_DefaultOutput);
    SetSequenceSoundFont(song->sequence);

    MusicSequenceGetTrackCount(song->sequence, &ntracks);

    for (i = 0; i < ntracks; i++)
    {
        MusicTrack track;
        MusicTimeStamp tracklen;
        MusicTrackLoopInfo info;
        uint32_t tracklenlen = sizeof(tracklen);

        MusicSequenceGetIndTrack(song->sequence, i, &track);

        MusicTrackGetProperty(track, kSequenceTrackProperty_TrackLength,
                              &tracklen, &tracklenlen);

        info.loopDuration = tracklen;
        info.numberOfLoops = (looping ? 0 : 1);

        MusicTrackSetProperty(track, kSequenceTrackProperty_LoopInfo,
                              &info, sizeof(info));
    }

    MusicPlayerSetTime(song->player, 0);
    MusicPlayerStart(song->player);
}

static void I_MAC_StopSong(void *handle)
{
    if (song == NULL)
        return;

    MusicPlayerStop(song->player);

    // needed to prevent error and memory leak when disposing sequence
    MusicPlayerSetSequence(song->player, NULL);
}

static void FreeSong(void)
{
    if (song)
    {
        if (song->sequence)
            DisposeMusicSequence(song->sequence);
        if (song->player)
            DisposeMusicPlayer(song->player);
        free(song);
        song = NULL;
    }
}

static void *I_MAC_RegisterSong(void *data, int len)
{
    CFDataRef data_ref = NULL;

    song = calloc(1, sizeof(native_midi_song_t));

    if (NewMusicPlayer(&song->player) != noErr)
    {
        FreeSong();
        return NULL;
    }
    if (NewMusicSequence(&song->sequence) != noErr)
    {
        FreeSong();
        return NULL;
    }

    if (IsMid(data, len))
    {
        data_ref = CFDataCreate(NULL, (const uint8_t *)data, len);
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
            data_ref = CFDataCreate(NULL, (const uint8_t *)outbuf, outbuf_len);
        }

        mem_fclose(instream);
        mem_fclose(outstream);
    }

    if (data_ref == NULL)
    {
        FreeSong();
        return NULL;
    }

    if (MusicSequenceFileLoadData(song->sequence, data_ref, 0, 0) != noErr)
    {
        FreeSong();
        CFRelease(data_ref);
        return NULL;
    }

    CFRelease(data_ref);

    if (MusicPlayerSetSequence(song->player, song->sequence) != noErr)
    {
        FreeSong();
        return NULL;
    }

    return (void *)1;
}

static void I_MAC_UnRegisterSong(void *handle)
{
    FreeSong();
}

static void I_MAC_ShutdownMusic(void)
{
    I_MAC_StopSong(NULL);
    I_MAC_UnRegisterSong(NULL);
}

static int I_MAC_DeviceList(const char* devices[], int size, int *current_device)
{
    *current_device = 0;
    if (size > 0)
    {
        devices[0] = "Native MIDI";
        return 1;
    }
    return 0;
}

music_module_t music_mac_module =
{
    I_MAC_InitMusic,
    I_MAC_ShutdownMusic,
    I_MAC_SetMusicVolume,
    I_MAC_PauseSong,
    I_MAC_ResumeSong,
    I_MAC_RegisterSong,
    I_MAC_PlaySong,
    I_MAC_StopSong,
    I_MAC_UnRegisterSong,
    I_MAC_DeviceList,
};

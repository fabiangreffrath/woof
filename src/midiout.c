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

#include "midiout.h"

#include <stdlib.h>

#include "config.h"
#include "doomtype.h"
#include "i_printf.h"

//---------------------------------------------------------
//    WINMM
//---------------------------------------------------------
#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>

#include "m_io.h"

static HMIDIOUT hMidiOut;
static HANDLE hCallbackEvent;

static void MidiErrorInternal(const char *prefix, MMRESULT result)
{
    wchar_t werror[MAXERRORLENGTH];

    if (midiOutGetErrorTextW(result, (LPWSTR)werror, MAXERRORLENGTH)
        == MMSYSERR_NOERROR)
    {
        char *error = M_ConvertWideToUtf8(werror);
        I_Printf(VB_ERROR, "%s: %s", prefix, error);
        free(error);
    }
    else
    {
        I_Printf(VB_ERROR, "%s: Unknown error", prefix);
    }
}

#define MidiError(result) MidiErrorInternal(__func__, result)

static void CALLBACK MidiOutProc(HMIDIOUT hmo, UINT wMsg, DWORD_PTR dwInstance,
                                 DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    if (wMsg == MOM_DONE)
    {
        SetEvent(hCallbackEvent);
    }
}

void MIDI_SendShortMsg(const byte *message, unsigned int length)
{
    MMRESULT result;

    // Pack MIDI bytes into double word.
    DWORD packet = 0;
    byte *ptr = (byte *)&packet;
    for (int i = 0; i < length && i < sizeof(DWORD); ++i)
    {
        *ptr++ = message[i];
    }

    result = midiOutShortMsg(hMidiOut, packet);
    if (result != MMSYSERR_NOERROR)
    {
        MidiError(result);
    }
}

void MIDI_SendLongMsg(const byte *message, unsigned int length)
{
    MMRESULT result;
    MIDIHDR hdr = {0};

    hdr.lpData = (LPSTR)message;
    hdr.dwBufferLength = length;
    hdr.dwFlags = 0;

    result = midiOutPrepareHeader(hMidiOut, &hdr, sizeof(MIDIHDR));
    if (result != MMSYSERR_NOERROR)
    {
        MidiError(result);
        return;
    }

    result = midiOutLongMsg(hMidiOut, &hdr, sizeof(MIDIHDR));
    if (result != MMSYSERR_NOERROR)
    {
        MidiError(result);
        return;
    }

    if (WaitForSingleObject(hCallbackEvent, INFINITE) == WAIT_OBJECT_0)
    {
        result = midiOutUnprepareHeader(hMidiOut, &hdr, sizeof(MIDIHDR));
        if (result != MMSYSERR_NOERROR)
        {
            MidiError(result);
        }
    }
}

int MIDI_CountDevices(void)
{
    return midiOutGetNumDevs();
}

const char *MIDI_GetDeviceName(int device)
{
    MMRESULT result;
    MIDIOUTCAPSW caps;

    result = midiOutGetDevCapsW(device, &caps, sizeof(caps));
    if (result == MMSYSERR_NOERROR)
    {
        return M_ConvertWideToUtf8(caps.szPname);
    }
    else
    {
        MidiError(result);
    }

    return NULL;
}

boolean MIDI_OpenDevice(int device)
{
    MMRESULT result = midiOutOpen(&hMidiOut, device, (DWORD_PTR)&MidiOutProc,
                                  (DWORD_PTR)NULL, CALLBACK_FUNCTION);
    if (result != MMSYSERR_NOERROR)
    {
        MidiError(result);
        return false;
    }

    hCallbackEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    return true;
}

void MIDI_CloseDevice(void)
{
    MMRESULT result = midiOutClose(hMidiOut);
    if (result != MMSYSERR_NOERROR)
    {
        MidiError(result);
    }
    CloseHandle(hCallbackEvent);
}

//---------------------------------------------------------
//    ALSA
//---------------------------------------------------------
#elif defined(HAVE_ALSA)

#include <alsa/asoundlib.h>

#include "m_array.h"
#include "m_misc.h"

#define MIDI_DEFAULT_BUFFER_SIZE 32

static snd_seq_t *seq;
static snd_seq_port_subscribe_t *subscription;
static snd_midi_event_t *coder;
static size_t buffer_size;
static int vport = -1;

typedef struct
{
    const char *name;
    int port_id;
    int client_id;
} port_info_t;

static port_info_t *ports;

static boolean Init(void)
{
    if (seq)
    {
        return true;
    }

    int err;
    snd_seq_client_info_t *cinfo;
    snd_seq_port_info_t *pinfo;

    err = snd_seq_open(&seq, "default", SND_SEQ_OPEN_OUTPUT, 0);
    if (err < 0)
    {
        I_Printf(VB_ERROR, "Init: %s", snd_strerror(err));
        seq = NULL;
        return false;
    }

    snd_seq_client_info_alloca(&cinfo);
    snd_seq_port_info_alloca(&pinfo);

    snd_seq_client_info_set_client(cinfo, -1);
    while (snd_seq_query_next_client(seq, cinfo) == 0)
    {
        snd_seq_port_info_set_client(pinfo,
                                     snd_seq_client_info_get_client(cinfo));
        snd_seq_port_info_set_port(pinfo, -1);
        while (snd_seq_query_next_port(seq, pinfo) == 0)
        {
            if (snd_seq_port_info_get_client(pinfo) == SND_SEQ_CLIENT_SYSTEM)
            {
                continue; // ignore Timer and Announce ports on client 0
            }
            unsigned int caps = snd_seq_port_info_get_capability(pinfo);
            if (caps & SND_SEQ_PORT_CAP_SUBS_WRITE)
            {
                port_info_t port;
                port.name =
                    M_StringDuplicate(snd_seq_port_info_get_name(pinfo));
                port.port_id = snd_seq_port_info_get_port(pinfo);
                port.client_id = snd_seq_port_info_get_client(pinfo);
                array_push(ports, port);
            }
        }
    }

    buffer_size = MIDI_DEFAULT_BUFFER_SIZE;
    snd_midi_event_new(buffer_size, &coder);
    snd_midi_event_init(coder);
    return true;
}

static void Cleanup(void)
{
    if (!seq)
    {
        return;
    }
    if (subscription)
    {
        snd_seq_unsubscribe_port(seq, subscription);
        snd_seq_port_subscribe_free(subscription);
        subscription = NULL;
    }
    if (vport >= 0)
    {
        snd_seq_delete_port(seq, vport);
        vport = -1;
    }
    if (coder)
    {
        snd_midi_event_free(coder);
        coder = NULL;
    }
    snd_seq_close(seq);
    seq = NULL;
}

static void SendMessage(const byte *message, unsigned int length)
{
    if (length > buffer_size)
    {
        buffer_size = length;
        snd_midi_event_resize_buffer(coder, buffer_size);
    }

    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_source(&ev, vport);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);

    for (int i = 0; i < length; ++i)
    {
        if (snd_midi_event_encode_byte(coder, message[i], &ev) == 1)
        {
            // Send the event
            int err = snd_seq_event_output(seq, &ev);
            if (err < 0)
            {
                I_Printf(VB_ERROR, "SendMessage: %s", snd_strerror(err));
                return;
            }
            break;
        }
    }

    snd_seq_drain_output(seq);
}

void MIDI_SendShortMsg(const byte *message, unsigned int length)
{
    SendMessage(message, length);
}

void MIDI_SendLongMsg(const byte *message, unsigned int length)
{
    SendMessage(message, length);
}

int MIDI_CountDevices(void)
{
    Init();
    return array_size(ports);
}

const char *MIDI_GetDeviceName(int device)
{
    if (device >= array_size(ports))
    {
        return NULL;
    }

    return ports[device].name;
}

boolean MIDI_OpenDevice(int device)
{
    if (!Init() || device >= array_size(ports))
    {
        return false;
    }

    int err;
    snd_seq_addr_t sender, receiver;
    receiver.client = ports[device].client_id;
    receiver.port = ports[device].port_id;
    sender.client = snd_seq_client_id(seq);

    vport = snd_seq_create_simple_port(
        seq, NULL,
        SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,
        SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    if (vport < 0)
    {
        I_Printf(VB_ERROR, "MIDI_OpenDevice: %s", snd_strerror(vport));
        Cleanup();
        return false;
    }

    sender.port = vport;

    // Make subscription
    snd_seq_port_subscribe_malloc(&subscription);
    snd_seq_port_subscribe_set_sender(subscription, &sender);
    snd_seq_port_subscribe_set_dest(subscription, &receiver);
    snd_seq_port_subscribe_set_time_update(subscription, 1);
    snd_seq_port_subscribe_set_time_real(subscription, 1);
    err = snd_seq_subscribe_port(seq, subscription);
    if (err < 0)
    {
        I_Printf(VB_ERROR, "MIDI_OpenDevice: %s", snd_strerror(err));
        Cleanup();
        return false;
    }

    return true;
}

void MIDI_CloseDevice(void)
{
    Cleanup();
}

//---------------------------------------------------------
//    CoreMIDI and DLS Synth
//---------------------------------------------------------
#elif defined(__APPLE__)

#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/HostTime.h>
#include <CoreMIDI/MIDIServices.h>
#include <CoreServices/CoreServices.h>
#include <AvailabilityMacros.h>

#include "m_array.h"
#include "m_misc.h"

static AUGraph graph;
static AudioUnit unit;

static MIDIPortRef port;
static MIDIClientRef client;
static MIDIEndpointRef endpoint;

// Maximum buffer size that CoreMIDI can handle for MIDIPacketList
#define PACKET_BUFFER_SIZE 65536
static Byte *packet_buffer = NULL;

typedef struct
{
    char *name;
    int id;
} device_t;

static device_t *devices = NULL;
static int current_device;

static void Init(void)
{
    if (array_size(devices))
    {
        return;
    }

    device_t device;
    device.name = "DLS Synth";
    device.id = -1;
    array_push(devices, device);

    int num_dest = MIDIGetNumberOfDestinations();
    for (int i = 0; i < num_dest; ++i)
    {
        MIDIEndpointRef dest = MIDIGetDestination(i);
        if (!dest)
        {
            continue;
        }

        CFStringRef name;
        if (MIDIObjectGetStringProperty(dest, kMIDIPropertyName, &name)
            == noErr)
        {
            CFIndex length;
            CFRange range = {0, CFStringGetLength(name)};

            CFStringGetBytes(name, range, kCFStringEncodingASCII, '?', false,
                             NULL, INT_MAX, &length);

            char *buffer = malloc(length + 1);

            CFStringGetBytes(name, range, kCFStringEncodingASCII, '?', false,
                             (UInt8 *)buffer, length, NULL);

            buffer[length] = '\0';

            device.name = buffer;
            device.id = i;
            array_push(devices, device);
        }
    }
}

static void Cleanup(void)
{
    if (graph)
    {
        AUGraphStop(graph);
        DisposeAUGraph(graph);
        graph = NULL;
    }
    if (port)
    {
        MIDIPortDispose(port);
        port = 0;
    }
    if (client)
    {
        MIDIClientDispose(client);
        client = 0;
    }
    if (packet_buffer)
    {
        free(packet_buffer);
        packet_buffer = NULL;
    }
}

static void SendMessage(const byte *message, unsigned int length)
{
    MIDITimeStamp time_stamp = AudioGetCurrentHostTime();
    MIDIPacketList *packet_list = (MIDIPacketList *)packet_buffer;
    MIDIPacket *packet = MIDIPacketListInit(packet_list);

    // MIDIPacketList and MIDIPacket consume extra buffer areas for meta
    // information, and available size is smaller than buffer size. Here, we
    // simply assume that at least half size is available for data payload.
    ByteCount send_size = MIN(length, PACKET_BUFFER_SIZE / 2);

    // Add message to the MIDIPacketList
    MIDIPacketListAdd(packet_list, PACKET_BUFFER_SIZE, packet, time_stamp,
                      send_size, message);

    if (MIDISend(port, endpoint, packet_list) != noErr)
    {
        I_Printf(VB_ERROR, "SendMessage: MIDISend failed");
    }
}

void MIDI_SendShortMsg(const byte *message, unsigned int length)
{
    if (current_device > 0)
    {
        SendMessage(message, length);
        return;
    }

    UInt32 data[3] = {0};
    for (int i = 0; i < length && i < 3; ++i)
    {
        data[i] = message[i];
    }

    if (MusicDeviceMIDIEvent(unit, data[0], data[1], data[2], 0) != noErr)
    {
        I_Printf(VB_ERROR, "MIDI_SendShortMsg: MusicDeviceMIDIEvent failed");
    }
}

void MIDI_SendLongMsg(const byte *message, unsigned int length)
{
    if (current_device > 0)
    {
        SendMessage(message, length);
        return;
    }

    if (MusicDeviceSysEx(unit, message, length) != noErr)
    {
        I_Printf(VB_ERROR, "MIDI_SendLongMsg: MusicDeviceSysEx failed");
    }
}

int MIDI_CountDevices(void)
{
    Init();
    return array_size(devices);
}

const char *MIDI_GetDeviceName(int device)
{
    if (device >= array_size(devices))
    {
        return NULL;
    }
    return devices[device].name;
}

#define CHECK_ERR(stmt)                                                   \
    do                                                                    \
    {                                                                     \
        if ((stmt) != noErr)                                              \
        {                                                                 \
            I_Printf(VB_ERROR, "%s: Failed at %d", __func__, __LINE__);   \
            goto error;                                                   \
        }                                                                 \
    } while (0)

static boolean OpenDLSSynth(void)
{
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1050
    ComponentDescription d;
#else
    AudioComponentDescription d;
#endif
    AUNode synth, output;

    CHECK_ERR(NewAUGraph(&graph));

    // The default output device
    d.componentType = kAudioUnitType_Output;
    d.componentSubType = kAudioUnitSubType_DefaultOutput;
    d.componentManufacturer = kAudioUnitManufacturer_Apple;
    d.componentFlags = 0;
    d.componentFlagsMask = 0;
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1050
    CHECK_ERR(AUGraphNewNode(graph, &d, 0, NULL, &output));
#else
    CHECK_ERR(AUGraphAddNode(graph, &d, &output));
#endif

    // The built-in default (softsynth) music device
    d.componentType = kAudioUnitType_MusicDevice;
    d.componentSubType = kAudioUnitSubType_DLSSynth;
    d.componentManufacturer = kAudioUnitManufacturer_Apple;
    d.componentFlags = 0;
    d.componentFlagsMask = 0;
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1050
    CHECK_ERR(AUGraphNewNode(graph, &d, 0, NULL, &synth));
#else
    CHECK_ERR(AUGraphAddNode(graph, &d, &synth));
#endif

    CHECK_ERR(AUGraphConnectNodeInput(graph, synth, 0, output, 0));
    CHECK_ERR(AUGraphOpen(graph));
    CHECK_ERR(AUGraphInitialize(graph));
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1050
    CHECK_ERR(AUGraphGetNodeInfo(graph, synth, NULL, NULL, NULL, &unit));
#else
    CHECK_ERR(AUGraphNodeInfo(graph, synth, NULL, &unit));
#endif
    CHECK_ERR(AUGraphStart(graph));

    return true;

error:
    Cleanup();
    return false;
}

boolean MIDI_OpenDevice(int device)
{
    Init();
    if (device >= array_size(devices))
    {
        return false;
    }

    current_device = device;

    if (current_device == 0)
    {
        return OpenDLSSynth();
    }

    endpoint = MIDIGetDestination(devices[current_device].id);

    // Create a MIDI client and port
    CHECK_ERR(MIDIClientCreate(CFSTR(PROJECT_NAME), NULL, NULL, &client));
    CHECK_ERR(MIDIOutputPortCreate(client, CFSTR(PROJECT_NAME"Port"), &port));

    packet_buffer = malloc(PACKET_BUFFER_SIZE);

    return true;

error:
    Cleanup();
    return false;
}

void MIDI_CloseDevice(void)
{
    Cleanup();
}

//---------------------------------------------------------
//    DUMMY
//---------------------------------------------------------
#else

void MIDI_SendShortMsg(const byte *message, unsigned int length)
{
    ;
}

void MIDI_SendLongMsg(const byte *message, unsigned int length)
{
    ;
}

int MIDI_CountDevices(void)
{
    return 0;
}

const char *MIDI_GetDeviceName(int device)
{
    return NULL;
}

boolean MIDI_OpenDevice(int device)
{
    return false;
}

void MIDI_CloseDevice(void)
{
    ;
}

#endif

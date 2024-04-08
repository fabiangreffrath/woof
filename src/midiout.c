
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

#define PACK_MSG(status, data1, data2)                        \
    ((((data2) << 16) & 0xFF0000) | (((data1) << 8) & 0xFF00) \
    | ((status) & 0xFF))

static void MidiError(const char *prefix, MMRESULT result)
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

void MIDI_SendShortMsg(byte status, byte channel, byte param1, byte param2)
{
    MMRESULT result =
        midiOutShortMsg(hMidiOut, PACK_MSG(status | channel, param1, param2));
    if (result != MMSYSERR_NOERROR)
    {
        MidiError("MIDI_SendShortMsg", result);
    }
}

#define DEFAULT_SYSEX_BUFFER_SIZE 1024

static byte sysex_buffer[DEFAULT_SYSEX_BUFFER_SIZE];

void MIDI_SendLongMsg(const byte *message, unsigned int length)
{
    MMRESULT result;
    MIDIHDR sysex = {0};

    if (length > DEFAULT_SYSEX_BUFFER_SIZE)
    {
        I_Printf(VB_ERROR, "MIDI_SendLongMsg: SysEx message is too long");
        return;
    }
    memcpy(sysex_buffer, message, length);

    sysex.lpData = (LPSTR)sysex_buffer;
    sysex.dwBufferLength = length;
    sysex.dwFlags = 0;

    result = midiOutPrepareHeader(hMidiOut, &sysex, sizeof(MIDIHDR));
    if (result != MMSYSERR_NOERROR )
    {
        MidiError("MIDI_SendLongMsg", result);
        return;
    }

    result = midiOutLongMsg(hMidiOut, &sysex, sizeof(MIDIHDR));
    if (result != MMSYSERR_NOERROR )
    {
        MidiError("MIDI_SendLongMsg", result);
        return;
    }

    while (MIDIERR_STILLPLAYING
           == midiOutUnprepareHeader(hMidiOut, &sysex, sizeof(MIDIHDR)))
    {
        Sleep(1);
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
        MidiError("MIDI_GetDeviceName", result);
    }

    return NULL;
}

boolean MIDI_OpenDevice(int device)
{
    MMRESULT result = midiOutOpen(&hMidiOut, device, (DWORD_PTR)NULL,
                                  (DWORD_PTR)NULL, CALLBACK_NULL);

    if (result != MMSYSERR_NOERROR)
    {
        MidiError("MIDI_OpenDevice", result);
        return false;
    }

    return true;
}

void MIDI_CloseDevice(void)
{
    MMRESULT result = midiOutClose(hMidiOut);
    if (result != MMSYSERR_NOERROR)
    {
        MidiError("MIDI_CloseDevice", result);
    }
}

//---------------------------------------------------------
//    ALSA
//---------------------------------------------------------
#elif defined(HAVE_ALSA)

#include <alsa/asoundlib.h>

#include "m_array.h"
#include "m_misc.h"

static snd_seq_t *seq;
static snd_seq_port_subscribe_t *subscription;
static snd_midi_event_t *coder;
static unsigned int buffer_size = 32;
static byte *buffer;
static int vport;

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

    snd_midi_event_new(buffer_size, &coder);
    buffer = malloc(buffer_size);
    snd_midi_event_init(coder);
    return true;
}

static void SendMessage(const byte *message, unsigned int length)
{
    int err;

    if (length > buffer_size )
    {
        buffer_size = length;
        snd_midi_event_resize_buffer(coder, length);
        free(buffer);
        buffer = malloc(buffer_size);
    }

    memcpy(buffer, message, length);

    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);

    for (int i = 0; i < length; ++i)
    {
        if (snd_midi_event_encode_byte(coder, buffer[i], &ev) == 1)
        {
            // Send the event
            snd_seq_ev_set_source(&ev, vport);
            snd_seq_ev_set_subs(&ev);
            snd_seq_ev_set_direct(&ev);
            err = snd_seq_event_output(seq, &ev);
            if (err < 0 )
            {
                I_Printf(VB_ERROR, "SendMessage: %s", snd_strerror(err));
                return;
            }
            break;
        }
    }

    snd_seq_drain_output(seq);
}

void MIDI_SendShortMsg(byte status, byte channel, byte param1, byte param2)
{
    byte message[] = { status | channel, param1, param2 };
    SendMessage(message, sizeof(message));
}

void MIDI_SendLongMsg(const byte *message, unsigned int length)
{
    SendMessage(message, length);
}

int MIDI_CountDevices(void)
{
    if (!Init())
    {
        return 0;
    }

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
    if (!Init())
    {
        return false;
    }

    if (device > array_size(ports))
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
        I_Printf(VB_ERROR, "MIDI_OpenDevice: %s", snd_strerror(vport));
        return false;
    }

    return true;
}

void MIDI_CloseDevice(void)
{
    if (!seq)
    {
        return;
    }

    int err;

    if (subscription)
    {
        err = snd_seq_unsubscribe_port(seq, subscription);
        if (err < 0)
        {
            I_Printf(VB_ERROR, "MIDI_CloseDevice: %s", snd_strerror(err));
        }
        snd_seq_port_subscribe_free(subscription);
        subscription = NULL;
    }
    if (vport >= 0)
    {
        snd_seq_delete_port(seq, vport);
    }
    if (coder)
    {
        snd_midi_event_free(coder);
        coder = NULL;
    }

    snd_seq_close(seq);
    seq = NULL;
}

#else

void MIDI_SendShortMsg(byte status, byte channel, byte param1, byte param2)
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

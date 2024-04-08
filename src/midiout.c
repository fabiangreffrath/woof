
#include "midiout.h"

#include <stdlib.h>

#include "doomtype.h"
#include "i_printf.h"

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

#endif

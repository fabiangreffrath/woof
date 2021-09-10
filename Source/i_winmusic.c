//
//  Copyright(C) 2021 Roman Fomin
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 
//  02111-1307, USA.
//
// DESCRIPTION:
//      Windows native MIDI

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>

#include "doomstat.h"
#include "m_misc.h"
#include "m_misc2.h"
#include "midifile.h"

#include "../win32/win_fopen.h"

static HMIDISTRM hMidiStream;

static HANDLE hBufferReturnEvent;
static HANDLE hExitEvent;
static HANDLE hPlayerThread;

typedef struct
{
  DWORD dwDeltaTime;
  DWORD dwStreamID;
  DWORD dwEvent;
} native_event_t;

typedef struct
{
  native_event_t *native_events;
  int num_events;
  int position;
  boolean looping;
} win_midi_song_t;

static win_midi_song_t song;

static float volume_factor = 1.0;

static int channel_volume[MIDI_CHANNELS_PER_TRACK];

typedef struct
{
  midi_track_iter_t *iter;
  int absolute_time;
} win_midi_track_t;

boolean win_midi_registered = false;

static void MidiErrorMessageBox(DWORD dwError)
{
  char szErrorBuf[MAXERRORLENGTH];

  if (midiOutGetErrorText(dwError, (LPSTR) szErrorBuf, MAXERRORLENGTH) == MMSYSERR_NOERROR)
  {
    MessageBox(NULL, szErrorBuf, "midiOut Error", MB_ICONEXCLAMATION);
  }
  else
  {
    fprintf(stderr, "Unknown midiOut Error\n");
  }
}

#define MIDIEVENT_CHANNEL(x)    (x & 0x0000000F)
#define MIDIEVENT_TYPE(x)       (x & 0x000000F0)
#define MIDIEVENT_DATA1(x)     ((x & 0x0000FF00) >> 8)
#define MIDIEVENT_VOLUME(x)    ((x & 0x007F0000) >> 16)

#define STREAM_MAX_EVENTS   4
#define STREAM_NUM_BUFFERS  2
#define STREAM_CALLBACK_TIMEOUT 2000 // wait 2 seconds for callback

typedef struct
{
  native_event_t events[STREAM_MAX_EVENTS];
  int num_events;
  MIDIHDR MidiStreamHdr;
  boolean prepared;
} buffer_t;

static buffer_t buffers[STREAM_NUM_BUFFERS];
static int current_buffer = 0;

static void FillBuffer(void)
{
  int i;

  for (i = 0; i < STREAM_MAX_EVENTS; ++i)
  {
    native_event_t *event = &buffers[current_buffer].events[i];

    if (song.position >= song.num_events)
    {
      if (song.looping)
        song.position = 0;
      else
        break;
    }

    *event = song.native_events[song.position];

    if (MIDIEVENT_TYPE(event->dwEvent) == MIDI_EVENT_CONTROLLER &&
        MIDIEVENT_DATA1(event->dwEvent) == MIDI_CONTROLLER_MAIN_VOLUME)
    {
      int value = MIDIEVENT_VOLUME(event->dwEvent) * volume_factor;

      channel_volume[MIDIEVENT_CHANNEL(event->dwEvent)] = MIDIEVENT_VOLUME(event->dwEvent);

      event->dwEvent = (event->dwEvent & 0xFF00FFFF) | ((value & 0x7F) << 16);
    }

    song.position++;
  }

  buffers[current_buffer].num_events = i;
}

static void StreamOut(void)
{
  MMRESULT mmr;

  MIDIHDR *hdr = &buffers[current_buffer].MidiStreamHdr;

  int num_events = buffers[current_buffer].num_events;

  if (num_events == 0)
    return;

  hdr->lpData = (LPSTR)buffers[current_buffer].events;
  hdr->dwBytesRecorded = num_events * sizeof(native_event_t);

  if (!buffers[current_buffer].prepared)
  {
    hdr->dwBufferLength = STREAM_MAX_EVENTS * sizeof(native_event_t);
    hdr->dwFlags = 0;
    hdr->dwOffset = 0;

    mmr = midiOutPrepareHeader((HMIDIOUT)hMidiStream, hdr, sizeof(MIDIHDR));
    if (mmr != MMSYSERR_NOERROR)
    {
      MidiErrorMessageBox(mmr);
      return;
    }

    buffers[current_buffer].prepared = true;
  }

  mmr = midiStreamOut(hMidiStream, hdr, sizeof(MIDIHDR));
  if (mmr != MMSYSERR_NOERROR)
  {
    MidiErrorMessageBox(mmr);
  }
}

static void CALLBACK MidiProc(HMIDIIN hMidi, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
  if (uMsg == MOM_DONE)
  {
    SetEvent(hBufferReturnEvent);
  }
}

static DWORD WINAPI PlayerProc(void)
{
  HANDLE events[2] = { hBufferReturnEvent, hExitEvent };

  while (1)
  {
    switch (WaitForMultipleObjects(2, events, FALSE, INFINITE))
    {
      case WAIT_OBJECT_0:
        StreamOut();
        current_buffer = !current_buffer;
        FillBuffer();
        break;

      case WAIT_OBJECT_0 + 1:
        return 0;
    }
  }
  return 0;
}

static void MIDItoStream(midi_file_t *file)
{
  int i;
  native_event_t *native_event;

  int num_tracks =  MIDI_NumTracks(file);
  win_midi_track_t *tracks = (malloc)(num_tracks * sizeof(win_midi_track_t));

  int current_time = 0;

  for (i = 0; i < num_tracks; ++i)
  {
    tracks[i].iter = MIDI_IterateTrack(file, i);
    tracks[i].absolute_time = 0;
  }

  song.native_events = (calloc)(MIDI_NumEvents(file), sizeof(native_event_t));

  native_event = song.native_events;

  while (1)
  {
    midi_event_t *event;
    int min_time = INT_MAX;
    int idx = -1;

    for (i = 0; i < num_tracks; ++i)
    {
      int time = 0;

      if (tracks[i].iter == NULL)
        continue;

      time = tracks[i].absolute_time + MIDI_GetDeltaTime(tracks[i].iter);

      if (time < min_time)
      {
        min_time = time;
        idx = i;
      }
    }

    if (idx == -1)
      break;

    if (!MIDI_GetNextEvent(tracks[idx].iter, &event))
    {
      tracks[idx].iter = NULL;
      continue;
    }

    switch ((int)event->event_type)
    {
      case MIDI_EVENT_META:
        if (event->data.meta.type == MIDI_META_SET_TEMPO)
        {
          native_event->dwDeltaTime = min_time - current_time;
          native_event->dwStreamID = 0;
          native_event->dwEvent = event->data.meta.data[2] |
                                  (event->data.meta.data[1] << 8) |
                                  (event->data.meta.data[0] << 16) |
                                  (MEVT_TEMPO << 24);
          native_event++;
          song.num_events++;
          current_time = tracks[idx].absolute_time = min_time;
        }
        break;

      case MIDI_EVENT_NOTE_OFF:
      case MIDI_EVENT_NOTE_ON:
      case MIDI_EVENT_AFTERTOUCH:
      case MIDI_EVENT_CONTROLLER:
      case MIDI_EVENT_PITCH_BEND:
        native_event->dwDeltaTime = min_time - current_time;
        native_event->dwStreamID = 0;
        native_event->dwEvent = event->event_type |
                                event->data.channel.channel |
                                (event->data.channel.param1 << 8) |
                                (event->data.channel.param2 << 16) |
                                (MEVT_SHORTMSG << 24);
        native_event++;
        song.num_events++;
        current_time = tracks[idx].absolute_time = min_time;
        break;

      case MIDI_EVENT_PROGRAM_CHANGE:
      case MIDI_EVENT_CHAN_AFTERTOUCH:
        native_event->dwDeltaTime = min_time - current_time;
        native_event->dwStreamID = 0;
        native_event->dwEvent = event->event_type |
                                event->data.channel.channel |
                                (event->data.channel.param1 << 8) |
                                (0 << 16) |
                                (MEVT_SHORTMSG << 24);
        native_event++;
        song.num_events++;
        current_time = tracks[idx].absolute_time = min_time;
        break;
    }
  }

  if (tracks)
    (free)(tracks);
}

boolean I_WIN_InitMusic(void)
{
  int i;

  hMidiStream = NULL;
  hPlayerThread = NULL;

  song.native_events = NULL;
  song.num_events = 0;
  song.position = 0;
  song.looping = false;

  for (i = 0; i < STREAM_NUM_BUFFERS; ++i)
  {
     buffers[i].num_events = 0;
     buffers[i].prepared = false;
  }

  return true;
}

void I_WIN_SetMusicVolume(int volume)
{
  int i;

  volume_factor = (float)volume / 15;

  for (i = 0; i < MIDI_CHANNELS_PER_TRACK; ++i)
  {
    int value = channel_volume[i] * volume_factor;

    DWORD msg = MIDI_EVENT_CONTROLLER | i |
                (MIDI_CONTROLLER_MAIN_VOLUME << 8) |
                (value << 16);

    midiOutShortMsg((HMIDIOUT)hMidiStream, msg);
  }
}

void I_WIN_StopSong(void)
{
  MMRESULT mmr;

  if (hPlayerThread)
  {
    SetEvent(hExitEvent);
    WaitForSingleObject(hPlayerThread, INFINITE);

    CloseHandle(hPlayerThread);
    CloseHandle(hExitEvent);
    hPlayerThread = NULL;
  }

  if (hMidiStream)
  {
    int i;
    DWORD ret;

    midiStreamStop(hMidiStream);
    midiOutReset((HMIDIOUT)hMidiStream);

    ret = WaitForSingleObject(hBufferReturnEvent, STREAM_CALLBACK_TIMEOUT);

    if (ret == WAIT_TIMEOUT)
      fprintf(stderr, "Timed out waiting for MIDI callback\n");

    if (ret == WAIT_OBJECT_0)
    {
      for (i = 0; i < STREAM_NUM_BUFFERS; ++i)
      {
        if (buffers[i].prepared)
        {
          mmr = midiOutUnprepareHeader((HMIDIOUT)hMidiStream, &buffers[i].MidiStreamHdr, sizeof(MIDIHDR));
          if (mmr != MMSYSERR_NOERROR)
          {
            MidiErrorMessageBox(mmr);
          }
          buffers[i].prepared = false;
        }
      }
    }

    mmr = midiStreamClose(hMidiStream);
    if (mmr != MMSYSERR_NOERROR)
    {
      MidiErrorMessageBox(mmr);
    }

    CloseHandle(hBufferReturnEvent);
    hMidiStream = NULL;
  }

  if (song.native_events)
    (free)(song.native_events);
  song.native_events = NULL;
  song.num_events = 0;
  song.position = 0;

  win_midi_registered = false;
}

void I_WIN_PlaySong(boolean looping)
{
  MMRESULT mmr;

  song.looping = looping;

  mmr = midiStreamRestart(hMidiStream);
  if (mmr != MMSYSERR_NOERROR)
  {
    MidiErrorMessageBox(mmr);
  }

  hPlayerThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)PlayerProc, 0, 0, 0);
  SetThreadPriority(hPlayerThread, THREAD_PRIORITY_TIME_CRITICAL);
}

void I_WIN_RegisterSong(void *data, int len)
{
  int i;
  midi_file_t *file;
  char *filename;

  UINT MidiDevice = MIDI_MAPPER;
  MIDIPROPTIMEDIV prop;
  MMRESULT mmr;

  filename = M_TempFile("doom.mid");

  M_WriteFile(filename, data, len);

  file = MIDI_LoadFile(filename);

  if (file == NULL)
  {
    fprintf(stderr, "I_WIN_RegisterSong: Failed to load MID.\n");
  }

  I_WIN_StopSong();

  MIDItoStream(file);

  mmr = midiStreamOpen(&hMidiStream, &MidiDevice, (DWORD)1, (DWORD_PTR)MidiProc, (DWORD_PTR)NULL, CALLBACK_FUNCTION);
  if (mmr != MMSYSERR_NOERROR)
  {
    MidiErrorMessageBox(mmr);
    return;
  }

  prop.cbStruct = sizeof(MIDIPROPTIMEDIV);
  prop.dwTimeDiv = MIDI_GetFileTimeDivision(file);
  mmr = midiStreamProperty(hMidiStream, (LPBYTE)&prop, MIDIPROP_SET | MIDIPROP_TIMEDIV);
  if (mmr != MMSYSERR_NOERROR)
  {
    MidiErrorMessageBox(mmr);
  }

  for (i = 0; i < MIDI_CHANNELS_PER_TRACK; ++i)
  {
     channel_volume[i] = 100;
  }

  I_WIN_SetMusicVolume(snd_MusicVolume);

  hBufferReturnEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  hExitEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

  FillBuffer();

  StreamOut();

  current_buffer = !current_buffer;

  FillBuffer();

  win_midi_registered = true;

  MIDI_FreeFile(file);
  remove(filename);
  (free)(filename);
}

void I_WIN_UnRegisterSong(void)
{
  I_WIN_StopSong();
}

#endif

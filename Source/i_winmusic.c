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

static HMIDISTRM hMidiStream;
static MIDIHDR MidiStreamHdr;

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
} midi_song_t;

static midi_song_t song;

typedef struct
{
  int index;
  int volume;
} volume_events_t;

static volume_events_t *volume_events = NULL;
static int num_volume_events = 0;

static int channel_volume[MIDI_CHANNELS_PER_TRACK];

typedef struct
{
  midi_track_iter_t *iter;
  int absolute_time;
} win_track_data_t;

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

#define MAX_BLOCK_EVENTS 128
static native_event_t buffer[MAX_BLOCK_EVENTS];

static void BlockOut(void)
{
  MIDIHDR *hdr = &MidiStreamHdr;
  MMRESULT mmr;
  int block_events = 0;
  int block_size = 0;

  block_events = song.num_events - song.position;
  if (block_events <= 0)
  {
    if (song.looping)
    {
      song.position = 0;
      block_events = song.num_events;
    }
    else
      return;
  }
  if (block_events > MAX_BLOCK_EVENTS)
    block_events = MAX_BLOCK_EVENTS;

  block_size = block_events * sizeof(native_event_t);

  mmr = midiOutUnprepareHeader((HMIDIOUT)hMidiStream, hdr, sizeof(MIDIHDR));
  if (mmr != MMSYSERR_NOERROR)
  {
    MidiErrorMessageBox(mmr);
  }

  memcpy(buffer, song.native_events + song.position, block_size);

  song.position += block_events;

  hdr->lpData = (LPSTR)buffer;
  hdr->dwBufferLength = block_size;
  hdr->dwBytesRecorded = block_size;
  hdr->dwFlags = 0;
  hdr->dwOffset = 0;

  mmr = midiOutPrepareHeader((HMIDIOUT)hMidiStream, hdr, sizeof(MIDIHDR));
  if (mmr != MMSYSERR_NOERROR)
  {
    MidiErrorMessageBox(mmr);
    return;
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
        BlockOut();
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
  native_event_t *native_event = NULL;
  int num_events = 0;

  win_track_data_t *tracks = NULL;
  int num_tracks = 0;

  int current_time = 0;

  num_events = MIDI_NumEvents(file);
  song.native_events = (calloc)(num_events, sizeof(native_event_t));
  volume_events = (malloc)(num_events * sizeof(volume_events_t));

  num_tracks = MIDI_NumTracks(file);
  tracks = (malloc)(num_tracks * sizeof(win_track_data_t));

  for (i = 0; i < num_tracks; ++i)
  {
    tracks[i].iter = MIDI_IterateTrack(file, i);
    tracks[i].absolute_time = 0;
  }

  native_event = song.native_events;
  song.num_events = 0;

  while (1)
  {
    midi_event_t *event;
    int delta_time = INT_MAX;
    int idx = -1;

    for (i = 0; i < num_tracks; ++i)
    {
      int time = 0;

      if (tracks[i].iter == NULL)
        continue;

      time = tracks[i].absolute_time + MIDI_GetDeltaTime(tracks[i].iter);

      if (time < delta_time)
      {
        delta_time = time;
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
          native_event->dwDeltaTime = delta_time - current_time;
          native_event->dwStreamID = 0;
          native_event->dwEvent = event->data.meta.data[2] |
                                  (event->data.meta.data[1] << 8) |
                                  (event->data.meta.data[0] << 16) |
                                  (MEVT_TEMPO << 24);
          native_event++;
          song.num_events++;
          current_time = tracks[idx].absolute_time = delta_time;
        }
        break;

      case MIDI_EVENT_NOTE_OFF:
      case MIDI_EVENT_NOTE_ON:
      case MIDI_EVENT_AFTERTOUCH:
      case MIDI_EVENT_CONTROLLER:
      case MIDI_EVENT_PITCH_BEND:
        native_event->dwDeltaTime = delta_time - current_time;
        native_event->dwStreamID = 0;
        native_event->dwEvent = event->event_type |
                                event->data.channel.channel |
                                (event->data.channel.param1 << 8) |
                                (event->data.channel.param2 << 16) |
                                (MEVT_SHORTMSG << 24);

        if (event->event_type == MIDI_EVENT_CONTROLLER &&
            event->data.channel.param1 == MIDI_CONTROLLER_MAIN_VOLUME)
        {
          volume_events_t *volume_event = volume_events + num_volume_events;

          volume_event->index = song.num_events;
          volume_event->volume = event->data.channel.param2;
          num_volume_events++;
        }

        native_event++;
        song.num_events++;
        current_time = tracks[idx].absolute_time = delta_time;
        break;

      case MIDI_EVENT_PROGRAM_CHANGE:
      case MIDI_EVENT_CHAN_AFTERTOUCH:
        native_event->dwDeltaTime = delta_time - current_time;
        native_event->dwStreamID = 0;
        native_event->dwEvent = event->event_type |
                                event->data.channel.channel |
                                (event->data.channel.param1 << 8) |
                                (0 << 16) |
                                (MEVT_SHORTMSG << 24);
        native_event++;
        song.num_events++;
        current_time = tracks[idx].absolute_time = delta_time;
        break;
    }
  }

  if (tracks)
    (free)(tracks);
}

boolean I_WIN_InitMusic(void)
{
  hMidiStream = NULL;
  hPlayerThread = NULL;

  song.native_events = NULL;
  song.num_events = 0;
  song.position = 0;
  song.looping = false;

  return true;
}

void I_WIN_SetMusicVolume(int volume)
{
  int i;

  float volume_factor = (float)volume / 15;

  for (i = 0; i < num_volume_events; ++i)
  {
    int value;
    native_event_t *native_event;

    native_event = song.native_events + volume_events[i].index;
    value = volume_events[i].volume * volume_factor;
    native_event->dwEvent = (native_event->dwEvent & 0xFF00FFFF) | ((value & 0x7F) << 16);

    if (volume_events[i].index <= song.position)
    {
        channel_volume[(native_event->dwEvent & 0xF)] = volume_events[i].volume;
    }
  }

  for (i = 0; i < MIDI_CHANNELS_PER_TRACK; ++i)
  {
    DWORD msg = MIDI_EVENT_CONTROLLER | i |
               (MIDI_CONTROLLER_MAIN_VOLUME << 8) |
               ((DWORD)(channel_volume[i] * volume_factor) << 16);

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
    CloseHandle(hBufferReturnEvent);
    CloseHandle(hExitEvent);
    hPlayerThread = NULL;
  }

  if (hMidiStream)
  {
    midiStreamStop(hMidiStream);
    midiOutReset((HMIDIOUT)hMidiStream);

    mmr = midiOutUnprepareHeader((HMIDIOUT)hMidiStream, &MidiStreamHdr, sizeof(MIDIHDR));
    if (mmr != MMSYSERR_NOERROR)
    {
      MidiErrorMessageBox(mmr);
    }

    mmr = midiStreamClose(hMidiStream);
    if (mmr != MMSYSERR_NOERROR)
    {
      MidiErrorMessageBox(mmr);
    }

    hMidiStream = NULL;
  }

  if (song.native_events)
    (free)(song.native_events);
  song.native_events = NULL;
  song.num_events = 0;
  song.position = 0;

  if (volume_events)
    (free)(volume_events);
  volume_events = NULL;
  num_volume_events = 0;

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

  BlockOut();

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

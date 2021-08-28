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
static UINT MidiDevice = MIDI_MAPPER;

typedef struct
{
  DWORD *native_events;
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

static int channel_volume[MIDI_CHANNELS_PER_TRACK] = {0};

typedef struct
{
    midi_track_iter_t *iter;
    unsigned int absolute_time;
} win_track_data_t;

boolean win_midi_registered = false;

#define MAX_BLOCK_EVENTS 128

static void MidiErrorMessageBox(DWORD dwError)
{
  char szErrorBuf[MAXERRORLENGTH];

  if (midiOutGetErrorText(dwError, (LPSTR) szErrorBuf, MAXERRORLENGTH) == MMSYSERR_NOERROR)
  {
    MessageBox(NULL, szErrorBuf, "midiOut Error", MB_ICONEXCLAMATION);
  }
  else
  {
    printf("Unknown midiOut Error\n");
  }
}

static DWORD buffer[MAX_BLOCK_EVENTS * 3];

static void PrepareHeader(void)
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

  block_size = block_events * 3 * sizeof(DWORD);

  if (win_midi_registered)
  {
    mmr = midiOutUnprepareHeader((HMIDIOUT)hMidiStream, hdr, sizeof(MIDIHDR));
    if (mmr != MMSYSERR_NOERROR)
    {
      MidiErrorMessageBox(mmr);
    }
  }

  memcpy(buffer, song.native_events + song.position * 3, block_size);

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
    PrepareHeader();
  }
}

static void MIDItoStream(midi_file_t *file)
{
  int i;
  DWORD *events = NULL;
  int num_events = 0;

  win_track_data_t *tracks = NULL;
  int num_tracks = 0;

  int current_time = 0;

  for (i = 0; i < MIDI_CHANNELS_PER_TRACK; ++i)
  {
     channel_volume[i] = 100;
  }

  num_events = MIDI_NumEvents(file);
  song.native_events = (calloc)(num_events * 3,  sizeof(DWORD));
  volume_events = (malloc)(num_events * sizeof(volume_events_t));

  num_tracks = MIDI_NumTracks(file);
  tracks = (malloc)(num_tracks * sizeof(win_track_data_t));

  for (i = 0; i < num_tracks; ++i)
  {
    tracks[i].iter = MIDI_IterateTrack(file, i);
    tracks[i].absolute_time = 0;
  }

  events = song.native_events;
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
          events[0] = delta_time - current_time; // dwDeltaTime
          events[2] = event->data.meta.data[2] | // dwEvent
                      (event->data.meta.data[1] << 8) |
                      (event->data.meta.data[0] << 16) |
                      (MEVT_TEMPO << 24);
          events += 3;
          song.num_events++;
          current_time = tracks[idx].absolute_time = delta_time;
        }
        break;

      case MIDI_EVENT_NOTE_OFF:
      case MIDI_EVENT_NOTE_ON:
      case MIDI_EVENT_AFTERTOUCH:
      case MIDI_EVENT_CONTROLLER:
      case MIDI_EVENT_PITCH_BEND:
        events[0] = delta_time - current_time; // dwDeltaTime
        events[2] = event->event_type |        // dwEvent
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

        events += 3;
        song.num_events++;
        current_time = tracks[idx].absolute_time = delta_time;
        break;

      case MIDI_EVENT_PROGRAM_CHANGE:
      case MIDI_EVENT_CHAN_AFTERTOUCH:
        events[0] = delta_time - current_time; // dwDeltaTime
        events[2] = event->event_type |        // dwEvent
                    event->data.channel.channel |
                    (event->data.channel.param1 << 8) |
                    (0 << 16) |
                    (MEVT_SHORTMSG << 24);
        events += 3;
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
  song.native_events = NULL;
  song.num_events = 0;
  song.position = 0;
  song.looping = false;

  return true;
}

void I_WIN_SetMusicVolume(int volume)
{
  int i;

  float vf = (float)volume / 15;

  for (i = 0; i < num_volume_events; ++i)
  {
    int value;
    DWORD *events;

    events = song.native_events + volume_events[i].index * 3;
    value = volume_events[i].volume * vf;
    events[2] = (events[2] & 0xFF00FFFF) | ((value & 0x7F) << 16);

    if (volume_events[i].index <= song.position)
    {
        channel_volume[(events[2] & 0xF)] = volume_events[i].volume;
    }
  }

  for (i = 0; i < MIDI_CHANNELS_PER_TRACK; ++i)
  {
    DWORD msg = MIDI_EVENT_CONTROLLER | i |
               (MIDI_CONTROLLER_MAIN_VOLUME << 8) |
               ((DWORD)(channel_volume[i] * vf) << 16);

    midiOutShortMsg((HMIDIOUT)hMidiStream, msg);
  }
}

void I_WIN_StopSong(void)
{
  MIDIHDR *hdr = &MidiStreamHdr;
  MMRESULT mmr;

  if (hMidiStream)
  {
    mmr = midiStreamStop(hMidiStream);
    if (mmr != MMSYSERR_NOERROR)
    {
      MidiErrorMessageBox(mmr);
    }

    mmr = midiOutReset((HMIDIOUT)hMidiStream);
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
}

void I_WIN_RegisterSong(void *data, int len)
{
  midi_file_t *file;
  char *filename;
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

  I_WIN_SetMusicVolume(snd_MusicVolume);

  PrepareHeader();

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

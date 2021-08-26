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
#include "i_sound.h"
#include "m_misc.h"
#include "m_misc2.h"
#include "midifile.h"
#include "mmus2mid.h"

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

static int channel_volume[16] = {0};

typedef struct
{
    midi_track_iter_t *iter;
    unsigned int absolute_time;
} win_track_data_t;

boolean win_midi_registered = false;

#define MAX_BLOCK_SIZE 128

#define MIDIEVENT_CHANNEL(x) (x & 0x0000000F)
#define MIDIEVENT_TYPE(x)    (x & 0x000000F0)
#define MIDIEVENT_DATA1(x)  ((x & 0x0000FF00) >> 8)
#define MIDIEVENT_VOLUME(x) ((x & 0x007F0000) >> 16)

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

static void PrepareHeader(void)
{
  MIDIHDR *hdr = &MidiStreamHdr;
  MMRESULT mmr;
  unsigned int block_size = 0;

  block_size = song.num_events - song.position;
  if (block_size <= 0)
  {
    if (song.looping)
    {
      song.position = 0;
      block_size = song.num_events;
    }
    else
      return;
  }
  if (block_size > MAX_BLOCK_SIZE)
    block_size = MAX_BLOCK_SIZE;

  hdr->lpData = (LPSTR)(song.native_events + song.position * 3);
  song.position += block_size;
  hdr->dwBufferLength = hdr->dwBytesRecorded = block_size * 3 * sizeof(DWORD);
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
  else if (uMsg == MOM_POSITIONCB)
  {
    MIDIHDR *mh = (MIDIHDR *)dwParam1;
    MIDIEVENT *me = (MIDIEVENT *)(mh->lpData + mh->dwOffset);

    if (MIDIEVENT_TYPE(me->dwEvent) == MIDI_EVENT_CONTROLLER &&
        MIDIEVENT_DATA1(me->dwEvent) == MIDI_CONTROLLER_MAIN_VOLUME)
    {
      // Mask off the channel number and cache the volume data byte.
      channel_volume[MIDIEVENT_CHANNEL(me->dwEvent)] = MIDIEVENT_VOLUME(me->dwEvent);
    }
  }
}

static void MIDItoStream(midi_file_t *file)
{
  int i;
  DWORD *events = NULL;
  int native_events_size = 0;

  win_track_data_t *tracks = NULL;
  int num_tracks = 0;

  int current_time = 0;

  native_events_size = MIDI_NumEvents(file) * 3 * sizeof(DWORD);
  song.native_events = (malloc)(native_events_size);
  memset(song.native_events, 0, native_events_size);

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
    unsigned int min_delta = INT_MAX;
    int idx = -1;

    for (i = 0; i < num_tracks; ++i)
    {
      int time = 0;

      if (tracks[i].iter == NULL)
        continue;

      time = tracks[i].absolute_time + MIDI_GetDeltaTime(tracks[i].iter);

      if (time < min_delta)
      {
        min_delta = time;
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

    tracks[idx].absolute_time = min_delta;

    switch ((int)event->event_type)
    {
      case MIDI_EVENT_META:
        if (event->data.meta.type == MIDI_META_SET_TEMPO)
        {
          events[0] = tracks[idx].absolute_time - current_time; // dwDeltaTime
          events[2] = event->data.meta.data[2] |                // dwEvent
                      (event->data.meta.data[1] << 8) |
                      (event->data.meta.data[0] << 16) |
                      (MEVT_TEMPO << 24);
          events += 3;
          song.num_events++;
        }
        break;

      case MIDI_EVENT_NOTE_OFF:
      case MIDI_EVENT_NOTE_ON:
      case MIDI_EVENT_AFTERTOUCH:
      case MIDI_EVENT_CONTROLLER:
      case MIDI_EVENT_PITCH_BEND:
        events[0] = tracks[idx].absolute_time - current_time; // dwDeltaTime
        events[2] = event->event_type |                       // dwEvent
                    event->data.channel.channel |
                    (event->data.channel.param1 << 8) |
                    (event->data.channel.param2 << 16) |
                    (MEVT_SHORTMSG << 24);

        if (MIDIEVENT_TYPE(events[2]) == MIDI_EVENT_CONTROLLER &&
            MIDIEVENT_DATA1(events[2]) == MIDI_CONTROLLER_MAIN_VOLUME)
        {
          events[2] |= MEVT_F_CALLBACK;
        }

        events += 3;
        song.num_events++;
        break;

      case MIDI_EVENT_PROGRAM_CHANGE:
      case MIDI_EVENT_CHAN_AFTERTOUCH:
        events[0] = tracks[idx].absolute_time - current_time; // dwDeltaTime
        events[2] = event->event_type |                       // dwEvent
                    event->data.channel.channel |
                    (event->data.channel.param1 << 8) |
                    (0 << 16) |
                    (MEVT_SHORTMSG << 24);
        events += 3;
        song.num_events++;
        break;
    }

    current_time = tracks[idx].absolute_time;
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
  DWORD *events = song.native_events;

  float vf = (float)volume / 15;

  for (i = 0; i < 16; ++i)
  {
    DWORD msg = MIDI_EVENT_CONTROLLER | i |
               (MIDI_CONTROLLER_MAIN_VOLUME << 8) |
               ((DWORD)(channel_volume[i] * vf) << 16);

    midiOutShortMsg((HMIDIOUT)hMidiStream, msg);
  }

  for (i = 0; i < song.num_events; ++i)
  {
    if (MIDIEVENT_TYPE(events[2]) == MIDI_EVENT_CONTROLLER &&
        MIDIEVENT_DATA1(events[2]) == MIDI_CONTROLLER_MAIN_VOLUME)
    {
      events[2] &= ((DWORD)(MIDIEVENT_VOLUME(events[2]) * vf) << 16);
    }
    events += 3;
  }
}

void I_WIN_StopSong(void)
{
  MIDIHDR *hdr = &MidiStreamHdr;

  if (!hMidiStream)
    return;

  midiOutReset((HMIDIOUT)hMidiStream);
  midiOutUnprepareHeader((HMIDIOUT)hMidiStream, hdr, sizeof(MIDIHDR));

  if (song.native_events)
    (free)(song.native_events);
  song.position = 0;

  win_midi_registered = false;

  midiStreamClose(hMidiStream);
  hMidiStream = NULL;
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

  I_WIN_SetMusicVolume(snd_MusicVolume);
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

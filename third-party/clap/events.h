/*
 * CLAP - CLever Audio Plugin
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * Copyright (c) 2014...2022 Alexandre BIQUE <bique.alexandre@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include "private/std.h"
#include "fixedpoint.h"
#include "id.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct clap_event_header {
    uint32_t size;
    uint32_t time;
    uint16_t space_id;
    uint16_t type;
    uint32_t flags;
} clap_event_header_t;

static const CLAP_CONSTEXPR uint16_t CLAP_CORE_EVENT_SPACE_ID = 0;

enum clap_event_flags {
    CLAP_EVENT_IS_LIVE = 1 << 0,
    CLAP_EVENT_DONT_RECORD = 1 << 1,
};

enum {
    CLAP_EVENT_NOTE_ON = 0,
    CLAP_EVENT_NOTE_OFF = 1,
    CLAP_EVENT_NOTE_CHOKE = 2,
    CLAP_EVENT_NOTE_END = 3,
    CLAP_EVENT_NOTE_EXPRESSION = 4,
    CLAP_EVENT_PARAM_VALUE = 5,
    CLAP_EVENT_PARAM_MOD = 6,
    CLAP_EVENT_PARAM_GESTURE_BEGIN = 7,
    CLAP_EVENT_PARAM_GESTURE_END = 8,
    CLAP_EVENT_TRANSPORT = 9,
    CLAP_EVENT_MIDI = 10,
    CLAP_EVENT_MIDI_SYSEX = 11,
    CLAP_EVENT_MIDI2 = 12,
};

typedef struct clap_event_note {
    clap_event_header_t header;
    int32_t note_id;
    int16_t port_index;
    int16_t channel;
    int16_t key;
    double  velocity;
} clap_event_note_t;

enum {
    CLAP_NOTE_EXPRESSION_VOLUME = 0,
    CLAP_NOTE_EXPRESSION_PAN = 1,
    CLAP_NOTE_EXPRESSION_TUNING = 2,
    CLAP_NOTE_EXPRESSION_VIBRATO = 3,
    CLAP_NOTE_EXPRESSION_EXPRESSION = 4,
    CLAP_NOTE_EXPRESSION_BRIGHTNESS = 5,
    CLAP_NOTE_EXPRESSION_PRESSURE = 6,
};
typedef int32_t clap_note_expression;

typedef struct clap_event_note_expression {
    clap_event_header_t header;
    clap_note_expression expression_id;
    int32_t note_id;
    int16_t port_index;
    int16_t channel;
    int16_t key;
    double value;
} clap_event_note_expression_t;

typedef struct clap_event_param_value {
    clap_event_header_t header;
    clap_id param_id;
    void   *cookie;
    int32_t note_id;
    int16_t port_index;
    int16_t channel;
    int16_t key;
    double value;
} clap_event_param_value_t;

typedef struct clap_event_param_mod {
    clap_event_header_t header;
    clap_id param_id;
    void   *cookie;
    int32_t note_id;
    int16_t port_index;
    int16_t channel;
    int16_t key;
    double amount;
} clap_event_param_mod_t;

typedef struct clap_event_param_gesture {
    clap_event_header_t header;
    clap_id param_id;
} clap_event_param_gesture_t;

enum clap_transport_flags {
    CLAP_TRANSPORT_HAS_TEMPO = 1 << 0,
    CLAP_TRANSPORT_HAS_BEATS_TIMELINE = 1 << 1,
    CLAP_TRANSPORT_HAS_SECONDS_TIMELINE = 1 << 2,
    CLAP_TRANSPORT_HAS_TIME_SIGNATURE = 1 << 3,
    CLAP_TRANSPORT_IS_PLAYING = 1 << 4,
    CLAP_TRANSPORT_IS_RECORDING = 1 << 5,
    CLAP_TRANSPORT_IS_LOOP_ACTIVE = 1 << 6,
    CLAP_TRANSPORT_IS_WITHIN_PRE_ROLL = 1 << 7,
};

typedef struct clap_event_transport {
    clap_event_header_t header;
    uint32_t flags;
    clap_beattime song_pos_beats;
    clap_sectime  song_pos_seconds;
    double tempo;
    double tempo_inc;
    clap_beattime loop_start_beats;
    clap_beattime loop_end_beats;
    clap_sectime  loop_start_seconds;
    clap_sectime  loop_end_seconds;
    clap_beattime bar_start;
    int32_t       bar_number;
    uint16_t tsig_num;
    uint16_t tsig_denom;
} clap_event_transport_t;

typedef struct clap_event_midi {
    clap_event_header_t header;
    uint16_t port_index;
    uint8_t  data[3];
} clap_event_midi_t;

typedef struct clap_event_midi_sysex {
    clap_event_header_t header;
    uint16_t       port_index;
    const uint8_t *buffer;
    uint32_t       size;
} clap_event_midi_sysex_t;

typedef struct clap_event_midi2 {
    clap_event_header_t header;
    uint16_t port_index;
    uint32_t data[4];
} clap_event_midi2_t;

typedef struct clap_input_events {
    void *ctx;
    uint32_t(CLAP_ABI *size)(const struct clap_input_events *list);
    const clap_event_header_t *(CLAP_ABI *get)(const struct clap_input_events *list, uint32_t index);
} clap_input_events_t;

typedef struct clap_output_events {
    void *ctx;
    bool(CLAP_ABI *try_push)(const struct clap_output_events *list,
                             const clap_event_header_t       *event);
} clap_output_events_t;

#ifdef __cplusplus
}
#endif
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

#include "events.h"
#include "audio-buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    CLAP_PROCESS_ERROR = 0,
    CLAP_PROCESS_CONTINUE = 1,
    CLAP_PROCESS_CONTINUE_IF_NOT_QUIET = 2,
    CLAP_PROCESS_TAIL = 3,
    CLAP_PROCESS_SLEEP = 4,
};
typedef int32_t clap_process_status;

typedef struct clap_process {
    int64_t steady_time;
    uint32_t frames_count;
    const clap_event_transport_t *transport;
    const clap_audio_buffer_t *audio_inputs;
    clap_audio_buffer_t       *audio_outputs;
    uint32_t                   audio_inputs_count;
    uint32_t                   audio_outputs_count;
    const clap_input_events_t  *in_events;
    const clap_output_events_t *out_events;
} clap_process_t;

#ifdef __cplusplus
}
#endif
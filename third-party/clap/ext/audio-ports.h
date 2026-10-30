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

#include "plugin.h"
#include "string-sizes.h"

static CLAP_CONSTEXPR const char CLAP_EXT_AUDIO_PORTS[] = "clap.audio-ports";
static CLAP_CONSTEXPR const char CLAP_PORT_MONO[] = "mono";
static CLAP_CONSTEXPR const char CLAP_PORT_STEREO[] = "stereo";

#ifdef __cplusplus
extern "C" {
#endif

enum {
    CLAP_AUDIO_PORT_IS_MAIN = 1 << 0,
    CLAP_AUDIO_PORT_SUPPORTS_64BITS = 1 << 1,
    CLAP_AUDIO_PORT_PREFERS_64BITS = 1 << 2,
    CLAP_AUDIO_PORT_REQUIRES_COMMON_SAMPLE_SIZE = 1 << 3,
};

typedef struct clap_audio_port_info {
    clap_id id;
    char    name[CLAP_NAME_SIZE];
    uint32_t flags;
    uint32_t channel_count;
    const char *port_type;
    clap_id in_place_pair;
} clap_audio_port_info_t;

typedef struct clap_plugin_audio_ports {
    uint32_t(CLAP_ABI *count)(const clap_plugin_t *plugin, bool is_input);
    bool(CLAP_ABI *get)(const clap_plugin_t    *plugin,
                        uint32_t                index,
                        bool                    is_input,
                        clap_audio_port_info_t *info);
} clap_plugin_audio_ports_t;

enum {
    CLAP_AUDIO_PORTS_RESCAN_NAMES = 1 << 0,
    CLAP_AUDIO_PORTS_RESCAN_FLAGS = 1 << 1,
    CLAP_AUDIO_PORTS_RESCAN_CHANNEL_COUNT = 1 << 2,
    CLAP_AUDIO_PORTS_RESCAN_PORT_TYPE = 1 << 3,
    CLAP_AUDIO_PORTS_RESCAN_IN_PLACE_PAIR = 1 << 4,
    CLAP_AUDIO_PORTS_RESCAN_LIST = 1 << 5,
};

typedef struct clap_host_audio_ports {
    bool(CLAP_ABI *is_rescan_flag_supported)(const clap_host_t *host, uint32_t flag);
    void(CLAP_ABI *rescan)(const clap_host_t *host, uint32_t flags);
} clap_host_audio_ports_t;

#ifdef __cplusplus
}
#endif
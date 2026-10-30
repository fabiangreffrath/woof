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

#include "../plugin.h"

// Use it to retrieve const clap_plugin_factory_t* from
// clap_plugin_entry.get_factory()
static const CLAP_CONSTEXPR char CLAP_PLUGIN_FACTORY_ID[] = "clap.plugin-factory";

#ifdef __cplusplus
extern "C" {
#endif

typedef struct clap_plugin_factory {
    uint32_t(CLAP_ABI *get_plugin_count)(const struct clap_plugin_factory *factory);
    const clap_plugin_descriptor_t *(CLAP_ABI *get_plugin_descriptor)(
        const struct clap_plugin_factory *factory, uint32_t index);
    const clap_plugin_t *(CLAP_ABI *create_plugin)(const struct clap_plugin_factory *factory,
                                                   const clap_host_t                *host,
                                                   const char                       *plugin_id);
} clap_plugin_factory_t;

#ifdef __cplusplus
}
#endif
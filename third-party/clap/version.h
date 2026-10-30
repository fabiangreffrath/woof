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

#include "private/macros.h"
#include "private/std.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct clap_version {
    uint32_t major;
    uint32_t minor;
    uint32_t revision;
} clap_version_t;

#ifdef __cplusplus
}
#endif

#define CLAP_VERSION_MAJOR 1
#define CLAP_VERSION_MINOR 2
#define CLAP_VERSION_REVISION 10

#define CLAP_VERSION_INIT \
    { (uint32_t)CLAP_VERSION_MAJOR, (uint32_t)CLAP_VERSION_MINOR, (uint32_t)CLAP_VERSION_REVISION }

#define CLAP_VERSION_LT(maj,min,rev) ((CLAP_VERSION_MAJOR < (maj)) || \
                    ((maj) == CLAP_VERSION_MAJOR && CLAP_VERSION_MINOR < (min)) || \
                    ((maj) == CLAP_VERSION_MAJOR && (min) == CLAP_VERSION_MINOR && CLAP_VERSION_REVISION < (rev)))
#define CLAP_VERSION_EQ(maj,min,rev) (((maj) == CLAP_VERSION_MAJOR) && ((min) == CLAP_VERSION_MINOR) && ((rev) == CLAP_VERSION_REVISION))
#define CLAP_VERSION_GE(maj,min,rev) (!CLAP_VERSION_LT(maj,min,rev))

static const CLAP_CONSTEXPR clap_version_t CLAP_VERSION = CLAP_VERSION_INIT;

CLAP_NODISCARD static inline CLAP_CONSTEXPR bool
clap_version_is_compatible(const clap_version_t v) {
    return v.major >= 1;
}
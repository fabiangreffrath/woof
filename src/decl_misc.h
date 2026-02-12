//
//  Copyright (c) 2015, Braden "Blzut3" Obrzut
//  Copyright (c) 2026, Roman Fomin
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the
//       distribution.
//     * The names of its contributors may be used to endorse or promote
//       products derived from this software without specific prior written
//       permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.

#ifndef DECL_MISC_H
#define DECL_MISC_H

#include "doomtype.h"
#include "m_scanner.h"

inline static int DECL_CheckKeywordInternal(scanner_t *sc,
                                            const char *keywords[], int count)
{
    const char *string = SC_GetString(sc);
    for (int i = 0; i < count; ++i)
    {
        if (strcasecmp(keywords[i], string) == 0)
        {
            return i;
        }
    }
    return -1;
}

#define DECL_CheckKeyword(sc, ...)                                  \
    DECL_CheckKeywordInternal(sc, (const char *[]){__VA_ARGS__},    \
                              sizeof((const char *[]){__VA_ARGS__}) \
                                  / sizeof(const char *))

inline static int DECL_RequireKeywordInternal(scanner_t *sc,
                                              const char *keywords[], int count)
{
    int result = DECL_CheckKeywordInternal(sc, keywords, count);
    if (result >= 0)
    {
        return result;
    }
    SC_Error(sc, "Invalid keyword at this point.");
    return -1;
}

#define DECL_RequireKeyword(sc, ...)                                  \
    DECL_RequireKeywordInternal(sc, (const char *[]){__VA_ARGS__},    \
                                sizeof((const char *[]){__VA_ARGS__}) \
                                    / sizeof(const char *))

// Helper function for when we need to parse a signed integer.
inline static int DECL_GetNegativeInteger(scanner_t *sc)
{
    boolean neg = SC_CheckToken(sc, '-');
    SC_MustGetToken(sc, TK_IntConst);
    return neg ? -SC_GetNumber(sc) : SC_GetNumber(sc);
}

inline static double DECL_GetNegativeDecimal(scanner_t *sc)
{
    boolean neg = SC_CheckToken(sc, '-');
    SC_MustGetToken(sc, TK_FloatConst);
    return neg ? -SC_GetDecimal(sc) : SC_GetDecimal(sc);
}

#endif

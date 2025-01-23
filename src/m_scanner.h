//
//  Copyright (c) 2015, Braden "Blzut3" Obrzut
//  Copyright (c) 2024, Roman Fomin
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

#ifndef M_SCANNER_H
#define M_SCANNER_H

#include "doomtype.h"

typedef struct scanner_s scanner_t;

enum
{
    TK_Identifier,      // Ex: SomeIdentifier
    TK_StringConst,     // Ex: "Some String"
    TK_IntConst,        // Ex: 27
    TK_BoolConst,       // Ex: true
    TK_FloatConst,      // Ex: 1.5

    TK_LumpName,

    TK_AnnotateStart,   // Block comment start
    TK_AnnotateEnd,     // Block comment end

    TK_ScopeResolution, // ::

    TK_NumSpecialTokens,

    TK_NoToken = -1
};

scanner_t *SC_Open(const char *scriptname, const char *data, int length);
void SC_Close(scanner_t *s);

const char *SC_GetString(scanner_t *s);
int SC_GetNumber(scanner_t *s);
boolean SC_GetBoolean(scanner_t *s);
double SC_GetDecimal(scanner_t *s);

boolean SC_TokensLeft(scanner_t *s);
boolean SC_CheckToken(scanner_t *s, char token);
boolean SC_GetNextToken(scanner_t *s, boolean expandstate);
void SC_GetNextLineToken(scanner_t *s);
void SC_MustGetToken(scanner_t *s, char token);
void SC_Rewind(scanner_t *s); // Only can rewind one step.

void SC_GetNextTokenLumpName(scanner_t *s);

void SC_Error(scanner_t *s, const char *msg, ...) PRINTF_ATTR(2, 3);

#endif

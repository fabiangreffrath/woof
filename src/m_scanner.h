//
//  Copyright (c) 2015, Braden "Blzut3" Obrzut
//  Copyright (c) 2024, Roman Fomin
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.

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

scanner_t *SC_Open(const char *type, const char* data, int length);
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

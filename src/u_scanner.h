// Copyright (c) 2010, Braden "Blzut3" Obrzut <admin@maniacsvault.net>
// Copyright (c) 2019, Fernando Carmona Varo  <ferkiwi@gmail.com>
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//    * Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//    * Neither the name of the <organization> nor the
//      names of its contributors may be used to endorse or promote products
//      derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
// THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef __U_SCANNER_H__
#define __U_SCANNER_H__

#include "doomtype.h"

enum
{
    TK_Identifier,  // Ex: SomeIdentifier
    TK_StringConst, // Ex: "Some String"
    TK_IntConst,    // Ex: 27
    TK_FloatConst,  // Ex: 1.5
    TK_BoolConst,   // Ex: true
    TK_AndAnd,      // &&
    TK_OrOr,        // ||
    TK_EqEq,        // ==
    TK_NotEq,       // !=
    TK_GtrEq,       // >=
    TK_LessEq,      // <=
    TK_ShiftLeft,   // <<
    TK_ShiftRight,  // >>

    TK_NumSpecialTokens,

    TK_NoToken = -1
};

typedef struct
{
    char *string;
    int number;
    double decimal;
    boolean sc_boolean;
    char token;
    unsigned int tokenLine;
    unsigned int tokenLinePosition;
} u_parserstate_t;

typedef struct
{
    const char *name;

    u_parserstate_t nextState;

    char *data;
    unsigned int length;

    unsigned int line;
    unsigned int lineStart;
    unsigned int logicalPosition;
    unsigned int tokenLine;
    unsigned int tokenLinePosition;
    unsigned int scanPos;

    boolean needNext; // If checkToken returns false this will be false.

    char *string;
    int number;
    double decimal;
    boolean sc_boolean;
    char token;

} u_scanner_t;

u_scanner_t *U_ScanOpen(const char *data, int length, const char *name);
void U_ScanClose(u_scanner_t *s);
boolean U_GetNextToken(u_scanner_t *s, boolean expandState);
boolean U_GetNextLineToken(u_scanner_t *s);
boolean U_HasTokensLeft(u_scanner_t *s);
boolean U_MustGetToken(u_scanner_t *s, char token);
boolean U_MustGetIdentifier(u_scanner_t *s, const char *ident);
boolean U_MustGetInteger(u_scanner_t *s);
boolean U_MustGetFloat(u_scanner_t *s);
boolean U_CheckToken(u_scanner_t *s, char token);
boolean U_CheckInteger(u_scanner_t *s);
boolean U_CheckFloat(u_scanner_t *s);
void U_Unget(u_scanner_t *s);
boolean U_GetString(u_scanner_t *s);

void PRINTF_ATTR(2, 0) U_Error(u_scanner_t *s, const char *msg, ...);
void U_ErrorToken(u_scanner_t *s, int token);
void U_ErrorString(u_scanner_t *s, const char *mustget);

#endif /* __U_SCANNER_H__ */

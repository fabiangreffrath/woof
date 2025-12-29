//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2025 Guilherme Miranda
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//
// Dehacked string replacements
//

#ifndef DEH_STRINGS_H
#define DEH_STRINGS_H

#include "doomtype.h"
#include "d_englsh.h" // In order to not 'double include' in other files

// Used to do dehacked text substitutions throughout the program

const char *DEH_String(const char *s) PRINTF_ARG_ATTR(1);
const char *DEH_StringForMnemonic(const char *s);
void DEH_AddStringReplacement(const char *from_text, const char *to_text);
boolean DEH_HasStringReplacement(const char *s);

// BEX mnemonics stuff
typedef struct bex_string_s bex_string_t;

extern const struct bex_string_s
{
    const char *mnemonic;
    const char *original_string;
} bex_mnemonic_table[];

extern const char * const mapnames[];
extern const char * const mapnames2[];
extern const char * const mapnamesp[];
extern const char * const mapnamest[];
extern const char * const strings_players[];
extern const char * const strings_quit_messages[];

// killough 1/18/98:
// replace hardcoded limit with extern var (silly hack, I know)
extern const int num_quit_mnemonics;

// Colorized messages
extern boolean message_colorized;

extern char *c_GOTBLUECARD,    *c_GOTBLUESKUL,     *c_GOTREDCARD;
extern char *c_GOTREDSKULL,    *c_GOTYELWCARD,     *c_GOTYELWSKUL;
extern char *c_PD_BLUEC,       *c_PD_BLUEK,        *c_PD_BLUEO,       *c_PD_BLUES;
extern char *c_PD_REDC,        *c_PD_REDK,         *c_PD_REDO,        *c_PD_REDS;
extern char *c_PD_YELLOWC,     *c_PD_YELLOWK,      *c_PD_YELLOWO,     *c_PD_YELLOWS;
extern char *c_HUSTR_PLRGREEN, *c_HUSTR_PLRINDIGO, *c_HUSTR_PLRBROWN, *c_HUSTR_PLRRED;

#define color_GOTBLUECARD     (message_colorized ? c_GOTBLUECARD     : DEH_String(GOTBLUECARD))
#define color_GOTBLUESKUL     (message_colorized ? c_GOTBLUESKUL     : DEH_String(GOTBLUESKUL))
#define color_GOTREDCARD      (message_colorized ? c_GOTREDCARD      : DEH_String(GOTREDCARD))
#define color_GOTREDSKULL     (message_colorized ? c_GOTREDSKULL     : DEH_String(GOTREDSKULL))
#define color_GOTYELWCARD     (message_colorized ? c_GOTYELWCARD     : DEH_String(GOTYELWCARD))
#define color_GOTYELWSKUL     (message_colorized ? c_GOTYELWSKUL     : DEH_String(GOTYELWSKUL))
#define color_PD_BLUEC        (message_colorized ? c_PD_BLUEC        : DEH_String(PD_BLUEC))
#define color_PD_BLUEK        (message_colorized ? c_PD_BLUEK        : DEH_String(PD_BLUEK))
#define color_PD_BLUEO        (message_colorized ? c_PD_BLUEO        : DEH_String(PD_BLUEO))
#define color_PD_BLUES        (message_colorized ? c_PD_BLUES        : DEH_String(PD_BLUES))
#define color_PD_REDC         (message_colorized ? c_PD_REDC         : DEH_String(PD_REDC))
#define color_PD_REDK         (message_colorized ? c_PD_REDK         : DEH_String(PD_REDK))
#define color_PD_REDO         (message_colorized ? c_PD_REDO         : DEH_String(PD_REDO))
#define color_PD_REDS         (message_colorized ? c_PD_REDS         : DEH_String(PD_REDS))
#define color_PD_YELLOWC      (message_colorized ? c_PD_YELLOWC      : DEH_String(PD_YELLOWC))
#define color_PD_YELLOWK      (message_colorized ? c_PD_YELLOWK      : DEH_String(PD_YELLOWK))
#define color_PD_YELLOWO      (message_colorized ? c_PD_YELLOWO      : DEH_String(PD_YELLOWO))
#define color_PD_YELLOWS      (message_colorized ? c_PD_YELLOWS      : DEH_String(PD_YELLOWS))
#define color_HUSTR_PLRGREEN  (message_colorized ? c_HUSTR_PLRGREEN  : DEH_String(HUSTR_PLRGREEN))
#define color_HUSTR_PLRINDIGO (message_colorized ? c_HUSTR_PLRINDIGO : DEH_String(HUSTR_PLRINDIGO))
#define color_HUSTR_PLRBROWN  (message_colorized ? c_HUSTR_PLRBROWN  : DEH_String(HUSTR_PLRBROWN))
#define color_HUSTR_PLRRED    (message_colorized ? c_HUSTR_PLRRED    : DEH_String(HUSTR_PLRRED))

#endif /* #ifndef DEH_STR_H */

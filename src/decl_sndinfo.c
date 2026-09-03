//
// Copyright(C) 2026, Fabian Greffrath
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
// SNDINFO is a legacy competitor to DECLARE for sound/ambient
// definitions. Only what DECLARE's "sound" and "ambient" blocks
// express is supported: name-to-lump assignments and $ambient
// (continuous/random/periodic).
//
// Sound assignments use old ("name lump") or new ("name = lump")
// syntax; whichever appears first in a lump is enforced for the rest
// of it. Quoted values (file paths) and non-existent lumps are
// rejected. Names may be "/"-namespaced but never start or end with
// "/" (see ReadLogicalName()).
//
// Unknown $-directives are skipped, including brace-delimited ones
// spanning multiple lines (see SkipDirective()). Nothing here ever
// aborts the program: malformed input just prints a VB_WARNING and is
// skipped.

#include <stdlib.h>

#include "decl_sndinfo.h"
#include "decl_sounds.h"
#include "doomtype.h"
#include "i_printf.h"
#include "m_misc.h"
#include "m_scanner.h"
#include "w_wad.h"
#include "z_zone.h"

// DoomEd numbers 14001 to 14064 are supported (see decl_sounds.c).
#define MAX_AMBIENT_DATA 64

typedef enum
{
    SNDINFO_SYNTAX_UNKNOWN,
    SNDINFO_SYNTAX_OLD,  // logicalname lumpname
    SNDINFO_SYNTAX_NEW,  // logicalname = lumpname
} sndinfo_syntax_t;

// Non-fatal counterpart to SC_MustGetToken(): on a mismatch it warns,
// skips to the next line, and returns false instead of aborting.
static boolean ExpectToken(scanner_t *sc, char token, const char *what)
{
    if (SC_CheckToken(sc, token))
    {
        return true;
    }
    I_Printf(VB_WARNING, "SNDINFO: Expected %s, skipping entry.", what);
    SC_GetNextLineToken(sc);
    return false;
}

// Reassembles a "/"-namespaced logical name (e.g. "world/splash1")
// from the separate tokens m_scanner produces for it, since "/" isn't
// a valid identifier character. Returns NULL and warns on a trailing
// "/" (name invalid, entry must be skipped); otherwise the caller
// owns the returned string.
static char *ReadLogicalName(scanner_t *sc, const char *first)
{
    char *name = M_StringDuplicate(first);

    while (SC_CheckToken(sc, '/'))
    {
        if (!SC_CheckToken(sc, TK_Identifier))
        {
            I_Printf(VB_WARNING,
                     "SNDINFO: Name '%s' has a trailing '/' with nothing "
                     "after it, skipping.", name);
            free(name);
            return NULL;
        }
        char *joined = M_StringJoin(name, "/", SC_GetString(sc));
        free(name);
        name = joined;
    }

    return name;
}

// Skips to the matching closing '}' of a block whose opening '{' has
// already been consumed, handling nesting and line breaks. Mirrors
// UDMF_SkipScan() in p_udmf.c, which does the same for unsupported
// UDMF fields.
static void SkipBracedBlock(scanner_t *sc)
{
    int depth = 1;
    while (depth > 0 && SC_TokensLeft(sc))
    {
        if (SC_CheckToken(sc, '{'))
        {
            depth++;
        }
        else if (SC_CheckToken(sc, '}'))
        {
            depth--;
        }
        else
        {
            SC_GetNextToken(sc, true);
        }
    }
}

// Skips an unsupported directive: to the end of the line if it's
// simple, or (checking one extra line for a leading '{', since e.g.
// $random's brace may start on the next line) up through the matching
// '}' if it's brace-delimited.
static void SkipDirective(scanner_t *sc)
{
    while (SC_TokensLeft(sc) && SC_SameLine(sc))
    {
        if (SC_CheckToken(sc, '{'))
        {
            SkipBracedBlock(sc);
            return;
        }
        SC_GetNextToken(sc, true);
    }

    if (SC_CheckToken(sc, '{'))
    {
        SkipBracedBlock(sc);
    }
}

static void ParseSoundAssignment(scanner_t *sc, const char *name,
                                  sndinfo_syntax_t *syntax)
{
    boolean has_equals = SC_CheckToken(sc, '=');

    if (*syntax == SNDINFO_SYNTAX_UNKNOWN)
    {
        *syntax = has_equals ? SNDINFO_SYNTAX_NEW : SNDINFO_SYNTAX_OLD;
    }
    else if (has_equals != (*syntax == SNDINFO_SYNTAX_NEW))
    {
        I_Printf(VB_WARNING,
                 "SNDINFO: Sound '%s' uses a different assignment syntax "
                 "than the rest of the lump, skipping.", name);
        SC_GetNextLineToken(sc);
        return;
    }

    if (SC_CheckToken(sc, TK_StringConst))
    {
        // A quoted value is a file-system path; Woof has no virtual
        // /sounds/ namespace to resolve it against.
        I_Printf(VB_WARNING,
                 "SNDINFO: Sound '%s' uses a file path instead of a "
                 "lump name, which is not supported, skipping.", name);
        return;
    }

    if (!ExpectToken(sc, TK_Identifier, "a lump name"))
    {
        return;
    }

    const char *lump = SC_GetString(sc);
    if (W_CheckNumForName(lump) < 0)
    {
        I_Printf(VB_WARNING,
                 "SNDINFO: Sound '%s' references lump '%s', which does "
                 "not exist, skipping.", name, lump);
        return;
    }

    DECL_AddSndInfoSound(name, lump);
}

static void ParseAmbientDirective(scanner_t *sc)
{
    if (!ExpectToken(sc, TK_IntConst, "an ambient index"))
    {
        return;
    }
    int index = SC_GetNumber(sc);
    if (index < 1 || index > MAX_AMBIENT_DATA)
    {
        I_Printf(VB_WARNING,
                 "SNDINFO: Ambient index %d not in range 1 to %d, "
                 "skipping.", index, MAX_AMBIENT_DATA);
        SC_GetNextLineToken(sc);
        return;
    }

    if (!ExpectToken(sc, TK_Identifier, "a sound name"))
    {
        return;
    }
    char *sound_name = ReadLogicalName(sc, SC_GetString(sc));
    if (!sound_name)
    {
        SC_GetNextLineToken(sc);
        return;
    }

    // "point" or "world"; Woof's ambient system always treats sounds
    // as point sources currently, just like DECLARE (see decl_sounds.c).
    if (!ExpectToken(sc, TK_Identifier, "'point' or 'world'"))
    {
        free(sound_name);
        return;
    }
    if (SC_CheckKeyword(sc, "point", "world") < 0)
    {
        I_Printf(VB_WARNING,
                 "SNDINFO: Ambient sound '%s' has an unknown type, "
                 "expected 'point' or 'world', skipping.", sound_name);
        SC_GetNextLineToken(sc);
        free(sound_name);
        return;
    }

    if (!ExpectToken(sc, TK_FloatConst, "an attenuation value"))
    {
        free(sound_name);
        return;
    }
    double attenuation = SC_GetDecimal(sc);

    if (!ExpectToken(sc, TK_Identifier, "an ambient mode"))
    {
        free(sound_name);
        return;
    }
    static const char *keywords[] = {
        [AMB_MODE_CONTINUOUS] = "continuous",
        [AMB_MODE_RANDOM] = "random",
        [AMB_MODE_PERIODIC] = "periodic"
    };
    int mode_index = SC_CheckKeywordInternal(sc, keywords, arrlen(keywords));
    if (mode_index < 0)
    {
        I_Printf(VB_WARNING,
                 "SNDINFO: Ambient sound '%s' has an unknown mode, "
                 "skipping.", sound_name);
        SC_GetNextLineToken(sc);
        free(sound_name);
        return;
    }
    ambient_mode_t mode = (ambient_mode_t)mode_index;

    double param1 = 0.0, param2 = 0.0;
    if (mode == AMB_MODE_RANDOM)
    {
        if (!ExpectToken(sc, TK_FloatConst, "a minperiod value"))
        {
            free(sound_name);
            return;
        }
        param1 = SC_GetDecimal(sc);

        if (!ExpectToken(sc, TK_FloatConst, "a maxperiod value"))
        {
            free(sound_name);
            return;
        }
        param2 = SC_GetDecimal(sc);
    }
    else if (mode == AMB_MODE_PERIODIC)
    {
        if (!ExpectToken(sc, TK_FloatConst, "a period value"))
        {
            free(sound_name);
            return;
        }
        param1 = SC_GetDecimal(sc);
    }

    if (!ExpectToken(sc, TK_FloatConst, "a volume value"))
    {
        free(sound_name);
        return;
    }
    double volume = SC_GetDecimal(sc);

    DECL_AddSndInfoAmbient(index, mode, sound_name, attenuation, param1,
                           param2, volume);

    free(sound_name);
}

static void ParseSndInfo(scanner_t *sc)
{
    sndinfo_syntax_t syntax = SNDINFO_SYNTAX_UNKNOWN;

    while (SC_TokensLeft(sc))
    {
        if (SC_CheckToken(sc, '$'))
        {
            if (!ExpectToken(sc, TK_Identifier, "a directive name"))
            {
                continue;
            }
            if (SC_CheckKeyword(sc, "ambient") == 0)
            {
                ParseAmbientDirective(sc);
            }
            else
            {
                I_Printf(VB_WARNING,
                         "SNDINFO: Unknown directive '$%s', skipping.",
                         SC_GetString(sc));
                SkipDirective(sc);
            }
        }
        else if (SC_CheckToken(sc, TK_Identifier))
        {
            char *name = ReadLogicalName(sc, SC_GetString(sc));
            if (name)
            {
                ParseSoundAssignment(sc, name, &syntax);
                free(name);
            }
            else
            {
                SC_GetNextLineToken(sc);
            }
        }
        else
        {
            I_Printf(VB_WARNING, "SNDINFO: Unexpected token, skipping.");
            SC_GetNextLineToken(sc);
        }
    }
}

void SNDINFO_Parse(int lumpnum)
{
    I_Printf(VB_WARNING,
             "SNDINFO is deprecated and only partially supported. "
             "Please consider switching to DECLARATE instead.");

    char lumpname[9] = {0};
    M_CopyLumpName(lumpname, lumpinfo[lumpnum].name);
    scanner_t *sc = SC_Open(lumpname, W_CacheLumpNum(lumpnum, PU_CACHE),
                            W_LumpLength(lumpnum));
    ParseSndInfo(sc);
    SC_Close(sc);
}

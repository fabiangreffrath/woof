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
// Dehacked I/O code (does all reads from dehacked files)
//

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "deh_defs.h"
#include "deh_io.h"
#include "i_printf.h"
#include "m_io.h"
#include "m_misc.h"
#include "w_wad.h"
#include "z_zone.h"

typedef enum
{
    DEH_INPUT_FILE,
    DEH_INPUT_LUMP
} deh_input_type_t;

struct deh_context_s
{
    deh_input_type_t type;
    char *filename;

    // If the input comes from a memory buffer, pointer to the memory buffer.
    unsigned char *input_buffer;
    size_t input_buffer_len;
    unsigned int input_buffer_pos;
    int lumpnum;

    // If the input comes from a file, the file stream for reading data.
    FILE *stream;

    // Current line number that we have reached:
    int linenum;

    // Used by DEH_ReadLine:
    boolean last_was_newline;
    char *readbuffer;
    int readbuffer_size;

    // Error handling.
    boolean had_error;

    // [crispy] pointer to start of current line
    long linestart;
};

static deh_context_t *DEH_NewContext(void)
{
    deh_context_t *context = malloc(sizeof(*context));

    // Initial read buffer size of 128 bytes
    context->readbuffer_size = 128;
    context->readbuffer = malloc(context->readbuffer_size);
    context->linenum = 0;
    context->last_was_newline = true;

    context->had_error = false;
    context->linestart = -1; // [crispy] initialize

    return context;
}

// Open a dehacked file for reading
// Returns NULL if open failed
deh_context_t *DEH_OpenFile(const char *filename)
{
    FILE *fstream = M_fopen(filename, "r");

    if (fstream == NULL)
    {
        return NULL;
    }

    deh_context_t *context = DEH_NewContext();

    context->type = DEH_INPUT_FILE;
    context->stream = fstream;
    context->filename = M_StringDuplicate(filename);

    return context;
}

// Open a WAD lump for reading.
deh_context_t *DEH_OpenLump(int lumpnum)
{
    void *lump = W_CacheLumpNum(lumpnum, PU_STATIC);
    deh_context_t *context = DEH_NewContext();

    context->type = DEH_INPUT_LUMP;
    context->lumpnum = lumpnum;
    context->input_buffer = lump;
    context->input_buffer_len = W_LumpLength(lumpnum);
    context->input_buffer_pos = 0;

    context->filename = malloc(9);
    M_StringCopy(context->filename, lumpinfo[lumpnum].name, 9);

    return context;
}

// Close dehacked file
void DEH_CloseFile(deh_context_t *context)
{
    if (context->type == DEH_INPUT_FILE)
    {
        fclose(context->stream);
    }
    else if (context->type == DEH_INPUT_LUMP)
    {
        Z_Free(lumpcache[context->lumpnum]);
    }

    free(context->filename);
    free(context->readbuffer);
    free(context);
}

int DEH_GetCharFile(deh_context_t *context)
{
    if (feof(context->stream))
    {
        // end of file
        return -1;
    }

    return fgetc(context->stream);
}

int DEH_GetCharLump(deh_context_t *context)
{
    if (context->input_buffer_pos >= context->input_buffer_len)
    {
        return -1;
    }

    int result = context->input_buffer[context->input_buffer_pos];
    ++context->input_buffer_pos;
    return result;
}

// Reads a single character from a dehacked file
int DEH_GetChar(deh_context_t *context)
{
    int result = 0;
    boolean last_was_cr = false;

    // Track the current line number
    if (context->last_was_newline)
    {
        ++context->linenum;
    }

    // Read characters, converting CRLF to LF
    do
    {
        switch (context->type)
        {
            case DEH_INPUT_FILE:
                result = DEH_GetCharFile(context);
                break;

            case DEH_INPUT_LUMP:
                result = DEH_GetCharLump(context);
                break;
        }

        // Handle \r characters not paired with \n
        if (last_was_cr && result != '\n')
        {
            switch (context->type)
            {
                case DEH_INPUT_FILE:
                    ungetc(result, context->stream);
                    break;

                case DEH_INPUT_LUMP:
                    --context->input_buffer_pos;
                    break;
            }

            return '\r';
        }

        last_was_cr = result == '\r';

    } while (last_was_cr);

    context->last_was_newline = result == '\n';
    return result;
}

// Increase the read buffer size
static void IncreaseReadBuffer(deh_context_t *context)
{
    int newbuffer_size = context->readbuffer_size * 2;
    char *newbuffer = malloc(newbuffer_size);

    memcpy(newbuffer, context->readbuffer, context->readbuffer_size);

    free(context->readbuffer);

    context->readbuffer = newbuffer;
    context->readbuffer_size = newbuffer_size;
}

// [crispy] Save pointer to start of current line ...
void DEH_SaveLineStart(deh_context_t *context)
{
    if (context->type == DEH_INPUT_FILE)
    {
        context->linestart = ftell(context->stream);
    }
    else if (context->type == DEH_INPUT_LUMP)
    {
        context->linestart = context->input_buffer_pos;
    }
}

// [crispy] ... and reset context to start of current line
// to retry with previous line parser in case of a parsing error
void DEH_RestoreLineStart(deh_context_t *context)
{
    // [crispy] never point past the start
    if (context->linestart < 0)
    {
        return;
    }

    if (context->type == DEH_INPUT_FILE)
    {
        fseek(context->stream, context->linestart, SEEK_SET);
    }
    else if (context->type == DEH_INPUT_LUMP)
    {
        context->input_buffer_pos = context->linestart;
    }

    // [crispy] don't count this line twice
    --context->linenum;
}

// Read a whole line
char *DEH_ReadLine(deh_context_t *context, boolean extended)
{
    int c;
    int pos;
    boolean escaped = false;

    for (pos = 0;;)
    {
        c = DEH_GetChar(context);

        if (c < 0 && pos == 0)
        {
            // end of file

            return NULL;
        }

        // cope with lines of any length: increase the buffer size
        if (pos >= context->readbuffer_size)
        {
            IncreaseReadBuffer(context);
        }

        // extended string support
        if (extended && c == '\\')
        {
            c = DEH_GetChar(context);

            // "\n" in the middle of a string indicates an internal linefeed
            if (c == 'n')
            {
                context->readbuffer[pos] = '\n';
                ++pos;
                continue;
            }

            // values to be assigned may be split onto multiple lines by ending
            // each line that is to be continued with a backslash
            if (c == '\n')
            {
                escaped = true;
                continue;
            }
        }

        // blanks before the backslash are included in the string
        // but indentation after the linefeed is not
        if (escaped && c >= 0 && isspace(c) && c != '\n')
        {
            continue;
        }
        else
        {
            escaped = false;
        }

        if (c == '\n' || c < 0)
        {
            // end of line: a full line has been read
            context->readbuffer[pos] = '\0';
            break;
        }
        else if (c != '\0')
        {
            // normal character; don't allow NUL characters to be added.
            context->readbuffer[pos] = (char)c;
            ++pos;
        }
    }

    return context->readbuffer;
}

void DEH_PrintMessage(deh_context_t *context, verbosity_t verbosity, const char *msg, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, msg);
    M_vsnprintf(buffer, sizeof(buffer), msg, args);
    va_end(args);

    const char *location = (context->type == DEH_INPUT_FILE)
                          ? context->filename
                          : lumpinfo[context->lumpnum].wad_file;
    I_Printf(verbosity, "%s:%i: %s", location, context->linenum, buffer);
}

boolean DEH_HadError(deh_context_t *context)
{
    return context->had_error;
}

// [crispy] return the filename of the DEHACKED file
// or NULL if it is a DEHACKED lump loaded from a PWAD
char *DEH_FileName(deh_context_t *context)
{
    if (context->type == DEH_INPUT_FILE)
    {
        return context->filename;
    }

    return NULL;
}

//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
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
// Emulates the IO functions in C stdio.h reading and writing to
// memory.
//

#include <string.h>

#include "doomtype.h"
#include "memio.h"
#include "z_zone.h"

typedef enum
{
    MODE_READ,
    MODE_WRITE,
} memfile_mode_t;

struct _MEMFILE
{
    unsigned char *buf;
    size_t buflen;
    size_t alloced;
    unsigned int position;
    boolean read_eof;
    boolean eof;
    memfile_mode_t mode;
};

// Open a memory area for reading

MEMFILE *mem_fopen_read(void *buf, size_t buflen)
{
    MEMFILE *file;

    file = Z_Malloc(sizeof(MEMFILE), PU_STATIC, 0);

    file->buf = (unsigned char *)buf;
    file->buflen = buflen;
    file->position = 0;
    file->read_eof = false;
    file->eof = false;
    file->mode = MODE_READ;

    return file;
}

// Read bytes

size_t mem_fread(void *buf, size_t size, size_t nmemb, MEMFILE *stream)
{
    size_t items;

    if (stream->mode != MODE_READ)
    {
        return -1;
    }

    if (size == 0 || nmemb == 0)
    {
        return 0;
    }

    // Trying to read more bytes than we have left?

    items = nmemb;

    if (items * size > stream->buflen - stream->position)
    {
        if (stream->read_eof)
        {
            stream->eof = true;
        }
        else
        {
            stream->read_eof = true;
        }

        items = (stream->buflen - stream->position) / size;
    }

    // Copy bytes to buffer

    memcpy(buf, stream->buf + stream->position, items * size);

    // Update position

    stream->position += items * size;

    return items;
}

// Open a memory area for writing

MEMFILE *mem_fopen_write(void)
{
    MEMFILE *file;

    file = Z_Malloc(sizeof(MEMFILE), PU_STATIC, 0);

    file->alloced = 1024;
    file->buf = Z_Malloc(file->alloced, PU_STATIC, 0);
    file->buflen = 0;
    file->position = 0;
    file->read_eof = false;
    file->eof = false;
    file->mode = MODE_WRITE;

    return file;
}

// Write bytes to stream

size_t mem_fwrite(const void *ptr, size_t size, size_t nmemb, MEMFILE *stream)
{
    size_t bytes;

    if (stream->mode != MODE_WRITE)
    {
        return -1;
    }

    // More bytes than can fit in the buffer?
    // If so, reallocate bigger.

    bytes = size * nmemb;

    while (bytes > stream->alloced - stream->position)
    {
        unsigned char *newbuf;

        newbuf = Z_Malloc(stream->alloced * 2, PU_STATIC, 0);
        memcpy(newbuf, stream->buf, stream->alloced);
        Z_Free(stream->buf);
        stream->buf = newbuf;
        stream->alloced *= 2;
    }

    // Copy into buffer

    memcpy(stream->buf + stream->position, ptr, bytes);
    stream->position += bytes;

    if (stream->position > stream->buflen)
    {
        stream->buflen = stream->position;
    }

    return nmemb;
}

int mem_fputs(const char *str, MEMFILE *stream)
{
    if (str == NULL)
    {
        return -1;
    }

    return mem_fwrite(str, sizeof(char), strlen(str), stream);
}

char *mem_fgets(char *str, int count, MEMFILE *stream)
{
    int i;

    if (str == NULL || count < 0)
    {
        return NULL;
    }

    for (i = 0; i < count - 1; ++i)
    {
        byte ch;

        if (mem_fread(&ch, 1, 1, stream) != 1)
        {
            if (mem_feof(stream))
            {
                return NULL;
            }
            break;
        }

        str[i] = ch;

        if (ch == '\0')
        {
            return str;
        }

        if (ch == '\n')
        {
            ++i;
            break;
        }
    }

    str[i] = '\0';
    return str;
}

int mem_fgetc(MEMFILE *stream)
{
    byte ch;

    if (mem_fread(&ch, 1, 1, stream) == 1)
    {
        return (int)ch;
    }

    return -1; // EOF
}

void mem_get_buf(MEMFILE *stream, void **buf, size_t *buflen)
{
    *buf = stream->buf;
    *buflen = stream->buflen;
}

void mem_fclose(MEMFILE *stream)
{
    if (stream->mode == MODE_WRITE)
    {
        Z_Free(stream->buf);
    }

    Z_Free(stream);
}

long mem_ftell(MEMFILE *stream)
{
    return stream->position;
}

int mem_fseek(MEMFILE *stream, signed long position, mem_rel_t whence)
{
    unsigned int newpos;

    switch (whence)
    {
        case MEM_SEEK_SET:
            newpos = (int)position;
            break;

        case MEM_SEEK_CUR:
            newpos = (int)(stream->position + position);
            break;

        case MEM_SEEK_END:
            newpos = (int)(stream->buflen + position);
            break;
        default:
            return -1;
    }

    // We should allow seeking to the end of a MEMFILE with MEM_SEEK_END to
    // match stdio.h behavior.

    if (newpos <= stream->buflen)
    {
        stream->position = newpos;
        stream->read_eof = false;
        stream->eof = false;
        return 0;
    }
    else
    {
        return -1;
    }
}

int mem_feof(MEMFILE *stream)
{
    if (stream->eof)
    {
        return 1;
    }

    return 0;
}

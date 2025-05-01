//
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2019 Simon Judd
// Copyright(C) 2024 Roman Fomin
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

#include "v_fmt.h"

#include <stdlib.h>
#include <string.h>

#include "doomtype.h"
#include "i_printf.h"
#include "i_video.h"
#include "m_swap.h"
#include "r_defs.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

#include "spng.h"

#define M_ARRAY_INIT_CAPACITY 256
#include "m_array.h"

#define NO_COLOR_KEY (-1)

typedef struct
{
    byte row_off;
    byte *pixels;
} vpost_t;

typedef struct
{
    vpost_t *posts;
} vcolumn_t;

#define PUTBYTE(r, v)  \
    *r = (uint8_t)(v); \
    ++r

#define PUTSHORT(r, v)                              \
    *(r + 0) = (byte)(((uint16_t)(v) >> 0) & 0xff); \
    *(r + 1) = (byte)(((uint16_t)(v) >> 8) & 0xff); \
    r += 2

#define PUTLONG(r, v)                                \
    *(r + 0) = (byte)(((uint32_t)(v) >> 0) & 0xff);  \
    *(r + 1) = (byte)(((uint32_t)(v) >> 8) & 0xff);  \
    *(r + 2) = (byte)(((uint32_t)(v) >> 16) & 0xff); \
    *(r + 3) = (byte)(((uint32_t)(v) >> 24) & 0xff); \
    r += 4

//
// Converts a linear graphic to a patch with transparency. Mostly straight
// from SLADE.
//
patch_t *V_LinearToTransPatch(const byte *data, int width, int height,
                              int *output_size, int color_key, pu_tag tag,
                              void **user)
{
    vcolumn_t *columns = NULL;

    // Go through columns
    uint32_t offset = 0;
    for (int c = 0; c < width; c++)
    {
        vcolumn_t col = {0};
        vpost_t post = {0};
        post.row_off = 0;
        boolean ispost = false;
        boolean first_254 = true; // first 254 pixels use absolute offsets

        offset = c;
        byte row_off = 0;
        for (int r = 0; r < height; r++)
        {
            // if we're at offset 254, create a dummy post for tall doom gfx
            // support
            if (row_off == 254)
            {
                // Finish current post if any
                if (ispost)
                {
                    array_push(col.posts, post);
                    post.pixels = NULL;
                    ispost = false;
                }

                // Begin relative offsets
                first_254 = false;

                // Create dummy post
                post.row_off = 254;
                array_push(col.posts, post);

                // Clear post
                row_off = 0;
                ispost = false;
            }

            // If the current pixel is not transparent, add it to the current
            // post
            if (color_key == NO_COLOR_KEY || data[offset] != color_key)
            {
                // If we're not currently building a post, begin one and set its
                // offset
                if (!ispost)
                {
                    // Set offset
                    post.row_off = row_off;

                    // Reset offset if we're in relative offsets mode
                    if (!first_254)
                    {
                        row_off = 0;
                    }

                    // Start post
                    ispost = true;
                }

                // Add the pixel to the post
                array_push(post.pixels, data[offset]);
            }
            else if (ispost)
            {
                // If the current pixel is transparent and we are currently
                // building a post, add the current post to the list and clear
                // it
                array_push(col.posts, post);
                post.pixels = NULL;
                ispost = false;
            }

            // Go to next row
            offset += width;
            ++row_off;
        }

        // If the column ended with a post, add it
        if (ispost)
        {
            array_push(col.posts, post);
        }

        // Add the column data
        array_push(columns, col);

        // Go to next column
        ++offset;
    }

    size_t size = 0;

    // Calculate needed memory size to allocate patch buffer
    size += 4 * sizeof(int16_t);                   // 4 header shorts
    size += array_size(columns) * sizeof(int32_t); // offsets table

    for (int c = 0; c < array_size(columns); c++)
    {
        for (int p = 0; p < array_size(columns[c].posts); p++)
        {
            size_t post_len = 0;

            post_len += 2; // two bytes for post header
            post_len += 1; // dummy byte
            post_len += array_size(columns[c].posts[p].pixels); // pixels
            post_len += 1; // dummy byte

            size += post_len;
        }

        size += 1; // room for 0xff cap byte
    }

    byte *output = Z_Malloc(size, tag, user);
    byte *rover = output;

    // write header fields
    PUTSHORT(rover, width);
    PUTSHORT(rover, height);
    // This is written to afterwards
    PUTSHORT(rover, 0);
    PUTSHORT(rover, 0);

    // set starting position of column offsets table, and skip over it
    byte *col_offsets = rover;
    rover += array_size(columns) * 4;

    for (int c = 0; c < array_size(columns); c++)
    {
        // write column offset to offset table
        uint32_t offs = (uint32_t)(rover - output);
        PUTLONG(col_offsets, offs);

        // write column posts
        for (int p = 0; p < array_size(columns[c].posts); p++)
        {
            // Write row offset
            PUTBYTE(rover, columns[c].posts[p].row_off);

            // Write number of pixels
            int numpixels = array_size(columns[c].posts[p].pixels);
            PUTBYTE(rover, numpixels);

            // Write pad byte
            byte lastval = numpixels ? columns[c].posts[p].pixels[0] : 0;
            PUTBYTE(rover, lastval);

            // Write pixels
            for (int a = 0; a < numpixels; a++)
            {
                lastval = columns[c].posts[p].pixels[a];
                PUTBYTE(rover, lastval);
            }

            // Write pad byte
            PUTBYTE(rover, lastval);

            array_free(columns[c].posts[p].pixels);
        }

        // Write 255 cap byte
        PUTBYTE(rover, 0xff);

        array_free(columns[c].posts);
    }

    array_free(columns);

    if (output_size)
    {
        *output_size = size;
    }

    // Done!
    return (patch_t *)output;
}

static patch_t *DummyPatch(int lump, pu_tag tag)
{
    int num = (W_CheckNumForName)("TNT1A0", ns_sprites);
    int len = W_LumpLength(num);
    patch_t *dummy = V_CachePatchNum(num, PU_CACHE);

    Z_Malloc(len, tag, &lumpcache[lump]);
    memcpy(lumpcache[lump], dummy, len);
    return lumpcache[lump];
}

static void *DummyFlat(int lump, pu_tag tag)
{
    const int size = 64 * 64;

    Z_Malloc(size, tag, &lumpcache[lump]);
    memset(lumpcache[lump], v_darkest_color, size);
    return lumpcache[lump];
}

// Uniform Color Quantization
//
// Each color component axis (red, green and blue) is divided into a few fixed
// segments (8-8-4 in 256 colors). Each found color is placed into a
// corresponding segment slot. After all the colors are added, an average
// color is calculated for each slot. Those are the colors of the palette.

typedef struct
{
    int value;
    int pixel_count;
} color_slot_t;

static void AddValue(color_slot_t *s, int component)
{
    s->value += component;
    s->pixel_count++;
}

static int GetAverage(color_slot_t *s)
{
    int result = 0;

    if (s->pixel_count > 0)
    {
        result = s->value / s->pixel_count;
    }

    return result;
}

typedef struct
{
    color_slot_t red_slots[8];
    color_slot_t green_slots[8];
    color_slot_t blue_slots[4];

    byte palette[768];
} uniform_quantizer_t;

static void AddColor(uniform_quantizer_t *q, int r, int g, int b)
{
    int red_index = r >> 5;
    int green_index = g >> 5;
    int blue_index = b >> 6;
    AddValue(&q->red_slots[red_index], r);
    AddValue(&q->green_slots[green_index], g);
    AddValue(&q->blue_slots[blue_index], b);
}

static void GetPalette(uniform_quantizer_t *q)
{
    byte *roller = q->palette;

    for (int rs = 0; rs < arrlen(q->red_slots); ++rs)
    {
        for (int gs = 0; gs < arrlen(q->green_slots); ++gs)
        {
            for (int bs = 0; bs < arrlen(q->blue_slots); ++bs)
            {
                *roller++ = GetAverage(&q->red_slots[rs]);
                *roller++ = GetAverage(&q->green_slots[gs]);
                *roller++ = GetAverage(&q->blue_slots[bs]);
            }
        }
    }
}

static int GetPaletteIndex(int r, int g, int b)
{
    int red_index = r >> 5;
    int green_index = g >> 5;
    int blue_index = b >> 6;
    return (red_index << 5) + (green_index << 2) + blue_index;
}

typedef struct
{
    spng_ctx *ctx;
    byte *image;
    byte *translate;
    size_t image_size;
    int width;
    int height;
    int color_key;
} png_t;

// Set memory usage limits for storing standard and unknown chunks,
// this is important when reading untrusted files!
#define PNG_MEM_LIMIT (1024 * 1024 * 64)

static boolean InitPNG(png_t *png, void *buffer, int buffer_length)
{
    spng_ctx *ctx = spng_ctx_new(0);

    // Ignore and don't calculate chunk CRC's
    int ret = spng_set_crc_action(ctx, SPNG_CRC_USE, SPNG_CRC_USE);

    if (ret)
    {
        I_Printf(VB_ERROR, "InitPNG: spng_set_crc_action %s\n",
                 spng_strerror(ret));
        return false;
    }

    ret = spng_set_chunk_limits(ctx, PNG_MEM_LIMIT, PNG_MEM_LIMIT);

    if (ret)
    {
        I_Printf(VB_ERROR, "InitPNG: spng_set_chunk_limits %s\n",
                 spng_strerror(ret));
        return false;
    }

    ret = spng_set_png_buffer(ctx, buffer, buffer_length);

    if (ret)
    {
        I_Printf(VB_ERROR, "InitPNG: spng_set_png_buffer %s\n",
                 spng_strerror(ret));
        return false;
    }

    png->ctx = ctx;

    return true;
}

static void FreePNG(png_t *png)
{
    spng_ctx_free(png->ctx);
    if (png->image)
    {
        free(png->image);
    }
    if (png->translate)
    {
        free(png->translate);
    }
}

static boolean DecodePNG(png_t *png)
{
    struct spng_ihdr ihdr = {0};
    int ret = spng_get_ihdr(png->ctx, &ihdr);

    if (ret)
    {
        I_Printf(VB_ERROR, "DecodePNG: spng_get_ihdr %s\n", spng_strerror(ret));
        return false;
    }

    png->width = ihdr.width;
    png->height = ihdr.height;

    int fmt;
    switch (ihdr.color_type)
    {
        case SPNG_COLOR_TYPE_INDEXED:
            fmt = SPNG_FMT_PNG;
            break;
        case SPNG_COLOR_TYPE_GRAYSCALE:
        case SPNG_COLOR_TYPE_TRUECOLOR:
            fmt = SPNG_FMT_RGB8;
            break;
        default:
            fmt = SPNG_FMT_RGBA8;
            break;
    }

    size_t image_size = 0;
    ret = spng_decoded_image_size(png->ctx, fmt, &image_size);

    if (ret)
    {
        I_Printf(VB_ERROR, "DecodePNG: spng_decoded_image_size %s",
                 spng_strerror(ret));
        return false;
    }

    byte *image = malloc(image_size);
    ret = spng_decode_image(png->ctx, image, image_size, fmt, 0);

    if (ret)
    {
        I_Printf(VB_ERROR, "DecodePNG: spng_decode_image %s",
                 spng_strerror(ret));
        free(image);
        return false;
    }

    byte *playpal = W_CacheLumpName("PLAYPAL", PU_CACHE);

    if (fmt == SPNG_FMT_RGB8)
    {
        int indexed_size = image_size / 3;
        byte *indexed_image = malloc(indexed_size);

        uniform_quantizer_t q = {0};

        byte *roller = image;

        for (int i = 0; i < indexed_size; ++i)
        {
            int r = *roller++;
            int g = *roller++;
            int b = *roller++;

            AddColor(&q, r, g, b);
        }

        GetPalette(&q);

        byte translate[256];
        byte *palette = q.palette;
        for (int i = 0; i < 256; ++i)
        {
            int r = *palette++;
            int g = *palette++;
            int b = *palette++;

            translate[i] = I_GetNearestColor(playpal, r, g, b);
        }

        roller = image;

        for (int i = 0; i < indexed_size; ++i)
        {
            int r = *roller++;
            int g = *roller++;
            int b = *roller++;

            indexed_image[i] = translate[GetPaletteIndex(r, g, b)];
        }

        free(image);

        png->image = indexed_image;
        png->image_size = indexed_size;
    }
    else if (fmt == SPNG_FMT_RGBA8)
    {
        int indexed_size = image_size / 4;
        byte *indexed_image = malloc(indexed_size);

        uniform_quantizer_t q = {0};

        byte *roller = image;

        byte used_colors[256] = {0};
        boolean has_alpha = false;

        for (int i = 0; i < indexed_size; ++i)
        {
            int r = *roller++;
            int g = *roller++;
            int b = *roller++;
            int a = *roller++;
            if (a < 255)
            {
                has_alpha = true;
                continue;
            }

            AddColor(&q, r, g, b);
        }

        GetPalette(&q);

        byte translate[256];
        byte *palette = q.palette;
        for (int i = 0; i < 256; ++i)
        {
            int r = *palette++;
            int g = *palette++;
            int b = *palette++;

            byte c = I_GetNearestColor(playpal, r, g, b);
            used_colors[c] = 1;
            translate[i] = c;
        }

        int color_key = NO_COLOR_KEY;

        if (has_alpha)
        {
            for (int i = 0; i < 256; ++i)
            {
                if (used_colors[i] == 0)
                {
                    color_key = i;
                    break;
                }
            }
            png->color_key = color_key;
        }

        roller = image;

        for (int i = 0; i < indexed_size; ++i)
        {
            int r = *roller++;
            int g = *roller++;
            int b = *roller++;
            int a = *roller++;
            if (a < 255)
            {
                indexed_image[i] = color_key;
                continue;
            }

            indexed_image[i] = translate[GetPaletteIndex(r, g, b)];
        }

        free(image);

        png->image = indexed_image;
        png->image_size = indexed_size;
    }
    else
    {
        struct spng_plte plte = {0};
        ret = spng_get_plte(png->ctx, &plte);

        if (ret)
        {
            I_Printf(VB_ERROR, "DecodePNG: spng_get_plte %s\n",
                     spng_strerror(ret));
            return false;
        }

        byte *translate = malloc(plte.n_entries);
        boolean need_translation = false;
        byte *palette = playpal;

        for (int i = 0; i < plte.n_entries; ++i)
        {
            struct spng_plte_entry *e = &plte.entries[i];

            byte r = *palette++;
            byte g = *palette++;
            byte b = *palette++;

            if (e->red == r && e->green == g && e->blue == b)
            {
                translate[i] = i;
                continue;
            }

            need_translation = true;
            translate[i] =
                I_GetNearestColor(playpal, e->red, e->green, e->blue);
        }

        if (need_translation)
        {
            png->translate = translate;
        }
        else
        {
            free(translate);
        }

        png->image = image;
        png->image_size = image_size;
    }

    return true;
}

static void TranslatePatch(patch_t *patch, const byte *translate)
{
    int width = SHORT(patch->width);

    for (int i = 0; i < width; i++)
    {
        int offset = LONG(patch->columnofs[i]);
        byte *rover = (byte *)patch + offset;

        while (*rover != 0xff)
        {
            int count = *(rover + 1);
            byte *src = rover + 3;

            while (count--)
            {
                *src = translate[*src];
                ++src;
            }

            rover = src + 1;
        }
    }
}

patch_t *V_CachePatchNum(int lump, pu_tag tag)
{
    if (lump >= numlumps)
    {
        I_Error("V_CachePatchNum: %d >= numlumps", lump);
    }

    if (lumpcache[lump])
    {
        Z_ChangeTag(lumpcache[lump], tag);
        return lumpcache[lump];
    }

    void *buffer = W_CacheLumpNum(lump, tag);
    int buffer_length = W_LumpLength(lump);

    if (buffer_length < 8 || memcmp(buffer, "\211PNG\r\n\032\n", 8))
    {
        return buffer;
    }

    png_t png = {0};

    if (!InitPNG(&png, buffer, buffer_length))
    {
        goto error;
    }

    spng_set_option(png.ctx, SPNG_KEEP_UNKNOWN_CHUNKS, 1);

    png.color_key = NO_COLOR_KEY;

    struct spng_trns trns = {0};
    int ret = spng_get_trns(png.ctx, &trns);

    if (ret && ret != SPNG_ECHUNKAVAIL)
    {
        I_Printf(VB_ERROR, "V_CachePatchNum: spng_get_trns %s",
                 spng_strerror(ret));
        goto error;
    }

    for (int i = 0; i < trns.n_type3_entries; ++i)
    {
        if (trns.type3_alpha[i] < 255)
        {
            png.color_key = i;
            break;
        }
    }

    if (!DecodePNG(&png))
    {
        goto error;
    }

    int leftoffset = 0, topoffset = 0;
    uint32_t n_chunks = 0;
    ret = spng_get_unknown_chunks(png.ctx, NULL, &n_chunks);

    if (ret && ret != SPNG_ECHUNKAVAIL)
    {
        I_Printf(VB_ERROR, "V_CachePatchNum: spng_get_unknown_chunks %s",
                 spng_strerror(ret));
        goto error;
    }

    if (n_chunks > 0)
    {
        struct spng_unknown_chunk *chunks = malloc(n_chunks * sizeof(*chunks));
        spng_get_unknown_chunks(png.ctx, chunks, &n_chunks);
        for (int i = 0; i < n_chunks; ++i)
        {
            if (!memcmp(chunks[i].type, "grAb", 4) && chunks[i].length == 8)
            {
                int *p = chunks[i].data;
                leftoffset = SWAP_BE32(p[0]);
                topoffset = SWAP_BE32(p[1]);
                break;
            }
        }
        free(chunks);
    }

    Z_Free(buffer);

    patch_t *patch = V_LinearToTransPatch(png.image, png.width, png.height,
                                          &lumpinfo[lump].fmt_size,
                                          png.color_key, tag, &lumpcache[lump]);
    patch->leftoffset = leftoffset;
    patch->topoffset = topoffset;

    if (png.translate)
    {
        TranslatePatch(patch, png.translate);
    }

    FreePNG(&png);

    return lumpcache[lump];

error:
    FreePNG(&png);
    Z_Free(buffer);
    return DummyPatch(lump, tag);
}

void *V_CacheFlatNum(int lump, pu_tag tag)
{
    if (lump >= numlumps)
    {
        I_Error("V_CacheFlatNum: %d >= numlumps", lump);
    }

    if (lumpcache[lump])
    {
        Z_ChangeTag(lumpcache[lump], tag);
        return lumpcache[lump];
    }

    void *buffer = W_CacheLumpNum(lump, tag);
    int buffer_length = W_LumpLength(lump);

    if (buffer_length < 8 || memcmp(buffer, "\211PNG\r\n\032\n", 8))
    {
        return buffer;
    }

    png_t png = {0};

    if (!InitPNG(&png, buffer, buffer_length))
    {
        goto error;
    }

    if (!DecodePNG(&png))
    {
        goto error;
    }

    if (png.translate)
    {
        for (int i = 0; i < png.image_size; ++i)
        {
            png.image[i] = png.translate[png.image[i]];
        }
    }

    Z_Free(buffer);

    lumpinfo[lump].fmt_size = png.image_size;
    Z_Malloc(png.image_size, tag, &lumpcache[lump]);
    memcpy(lumpcache[lump], png.image, png.image_size);

    FreePNG(&png);

    return lumpcache[lump];

error:
    FreePNG(&png);
    Z_Free(buffer);
    return DummyFlat(lump, tag);
}

int V_LumpSize(int lump)
{
    if (lump >= numlumps)
    {
        I_Error("V_LumpFmtSize: %i >= numlumps", lump);
    }

    return lumpinfo[lump].fmt_size ? lumpinfo[lump].fmt_size
                                   : lumpinfo[lump].size;
}

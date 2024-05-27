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
#include "m_swap.h"
#include "r_defs.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

#include "spng.h"

#define M_ARRAY_INIT_CAPACITY 256
#include "m_array.h"

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
                              int color_key, pu_tag tag, void **user)
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
            if (color_key == -1 || data[offset] != color_key)
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

    // Done!
    return (patch_t *)output;
}

static patch_t *DummyPatch(int lump, pu_tag tag)
{
    int num = (W_CheckNumForName)("TNT1A0", ns_sprites);
    int len = W_LumpLength(num);
    patch_t *dummy = W_CacheLumpNum(num, PU_CACHE);

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

// Set memory usage limits for storing standard and unknown chunks,
// this is important when reading untrusted files!
#define PNG_MEM_LIMIT (1024 * 1024 * 64)

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

    spng_ctx *ctx = spng_ctx_new(0);

    // Ignore and don't calculate chunk CRC's
    spng_set_crc_action(ctx, SPNG_CRC_USE, SPNG_CRC_USE);

    spng_set_chunk_limits(ctx, PNG_MEM_LIMIT, PNG_MEM_LIMIT);

    spng_set_option(ctx, SPNG_KEEP_UNKNOWN_CHUNKS, 1);

    spng_set_png_buffer(ctx, buffer, buffer_length);

    struct spng_ihdr ihdr = {0};
    int ret = spng_get_ihdr(ctx, &ihdr);

    if (ret)
    {
        I_Printf(VB_ERROR, "V_CachePatchNum: spng_get_ihdr %s\n",
                 spng_strerror(ret));
        goto error;
    }

    if (ihdr.color_type != SPNG_COLOR_TYPE_INDEXED || ihdr.bit_depth != 8)
    {
        I_Printf(VB_ERROR, "V_CachePatchNum: Only 8-bit paletted PNG supported");
        goto error;
    }

    int color_key = NO_COLOR_KEY;
    struct spng_trns trns = {0};
    ret = spng_get_trns(ctx, &trns);

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
            color_key = i;
            break;
        }
    }

    int leftoffset = 0, topoffset = 0;
    uint32_t n_chunks = 0;
    ret = spng_get_unknown_chunks(ctx, NULL, &n_chunks);

    if (ret && ret != SPNG_ECHUNKAVAIL)
    {
        I_Printf(VB_ERROR, "V_CachePatchNum: spng_get_unknown_chunks %s",
                 spng_strerror(ret));
        goto error;
    }

    if (n_chunks > 0)
    {
        struct spng_unknown_chunk *chunks = malloc(n_chunks * sizeof(*chunks));
        spng_get_unknown_chunks(ctx, chunks, &n_chunks);
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

    size_t image_size = 0;
    ret = spng_decoded_image_size(ctx, SPNG_FMT_PNG, &image_size);

    if (ret)
    {
        I_Printf(VB_ERROR, "V_CachePatchNum: spng_decoded_image_size %s",
                 spng_strerror(ret));
        goto error;
    }

    byte *image = malloc(image_size);

    ret = spng_decode_image(ctx, image, image_size, SPNG_FMT_PNG, 0);

    if (ret)
    {
        I_Printf(VB_ERROR, "V_CachePatchNum: spng_decode_image %s",
                 spng_strerror(ret));
        free(image);
        goto error;
    }

    Z_Free(buffer);

    patch_t *patch = V_LinearToTransPatch(image, ihdr.width, ihdr.height,
                                          color_key, tag, &lumpcache[lump]);
    patch->leftoffset = leftoffset;
    patch->topoffset = topoffset;

    spng_ctx_free(ctx);
    free(image);

    return lumpcache[lump];

error:
    spng_ctx_free(ctx);
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

    spng_ctx *ctx = spng_ctx_new(0);

    // Ignore and don't calculate chunk CRC's
    spng_set_crc_action(ctx, SPNG_CRC_USE, SPNG_CRC_USE);

    spng_set_chunk_limits(ctx, PNG_MEM_LIMIT, PNG_MEM_LIMIT);

    spng_set_png_buffer(ctx, buffer, buffer_length);

    struct spng_ihdr ihdr = {0};
    int ret = spng_get_ihdr(ctx, &ihdr);

    if (ret)
    {
        I_Printf(VB_ERROR, "V_CacheFlatNum: spng_get_ihdr %s",
                 spng_strerror(ret));
        goto error;
    }

    if (ihdr.color_type != SPNG_COLOR_TYPE_INDEXED || ihdr.bit_depth != 8)
    {
        I_Printf(VB_ERROR, "V_CacheFlatNum: Only 8-bit paletted PNG supported");
        goto error;
    }

    size_t image_size = 0;
    ret = spng_decoded_image_size(ctx, SPNG_FMT_PNG, &image_size);

    if (ret)
    {
        I_Printf(VB_ERROR, "V_CacheFlatNum: spng_decoded_image_size %s",
                 spng_strerror(ret));
        goto error;
    }

    void *image = malloc(image_size);
    ret = spng_decode_image(ctx, image, image_size, SPNG_FMT_PNG, 0);

    if (ret)
    {
        I_Printf(VB_ERROR, "V_CacheFlatNum: spng_decode_image %s",
                 spng_strerror(ret));
        free(image);
        goto error;
    }

    spng_ctx_free(ctx);
    Z_Free(buffer);

    Z_Malloc(image_size, tag, &lumpcache[lump]);
    memcpy(lumpcache[lump], image, image_size);
    free(image);

    return lumpcache[lump];

error:
    spng_ctx_free(ctx);
    Z_Free(buffer);
    return DummyFlat(lump, tag);
}


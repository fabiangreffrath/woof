//
// Copyright(C) 2024 ceski
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
// DESCRIPTION:
//      Gamepad rumble.
//

#include "alext.h"
#include "pffft.h"

#include <math.h>
#include <string.h>

#include "doomstat.h"
#include "i_gamepad.h"
#include "i_rumble.h"
#include "i_sound.h"
#include "m_config.h"
#include "p_mobj.h"
#include "sounds.h"

#define MAX_RUMBLE_COUNT 4
#define MAX_RUMBLE_SDL   0xFFFF
#define RUMBLE_DURATION  1000
#define R_CLOSE          64.0f
#define R_CLIP           320.0f
#define RUMBLE_TICS      7
#define MAX_TICLENGTH    70 // TICRATE * 2

static int joy_rumble;
static int last_rumble;

typedef struct rumble_channel_s
{
    struct rumble_channel_s *prev, *next;
    int handle;         // Sound channel using this rumble channel.
    rumble_type_t type; // Rumble type.
    float scale;        // Rumble scale for this channel.
    int pos;            // Position in rumble sequence.
    const float *low;   // Pointer to cached low frequency rumble values.
    const float *high;  // Pointer to cached high frequency rumble values.
    int ticlength;      // Array size equal to sound duration in tics.
} rumble_channel_t;

typedef struct
{
    SDL_GameController *gamepad;
    boolean enabled;            // Rumble enabled?
    boolean supported;          // Rumble supported?
    float scale;                // Overall rumble scale, based on joy_rumble.
    rumble_channel_t *channels; // Rumble channel for each sound channel.
    rumble_channel_t base;      // Linked list.
    int count;                  // Number of active rumble channels.
} rumble_t;

static rumble_t rumble;

typedef struct
{
    PFFFT_Setup *setup;
    float *in;
    float *out;
    float *window;
    int size;
    float amp_scale;
    float freq_scale;
    int start;
    int end;
} fft_t;

static fft_t fft;

// Rumble pattern presets.

static const float rumble_itemup[] = {0.12f, 0.02f, 0.0f};
static const float rumble_wpnup[] = {0.12f, 0.07f, 0.04f, 0.16f, 0.0f};
static const float rumble_getpow[] = {0.07f, 0.14f, 0.16f, 0.14f, 0.0f};
static const float rumble_oof[] = {0.14f, 0.12f, 0.0f};
static const float rumble_pain[] = {0.12f, 0.31f, 0.26f, 0.22f,
                                    0.32f, 0.34f, 0.0f};
static const float rumble_hitfloor[] = {0.48f, 0.45f, 0.40f, 0.35f, 0.30f,
                                        0.25f, 0.20f, 0.15f, 0.0f};

typedef struct
{
    const float *data;
    int ticlength;
} rumble_preset_t;

static const rumble_preset_t presets[] = {
    [RUMBLE_NONE]     = {NULL,            0                      },
    [RUMBLE_ITEMUP]   = {rumble_itemup,   arrlen(rumble_itemup)  },
    [RUMBLE_WPNUP]    = {rumble_wpnup,    arrlen(rumble_wpnup)   },
    [RUMBLE_GETPOW]   = {rumble_getpow,   arrlen(rumble_getpow)  },
    [RUMBLE_OOF]      = {rumble_oof,      arrlen(rumble_oof)     },
    [RUMBLE_PAIN]     = {rumble_pain,     arrlen(rumble_pain)    },
    [RUMBLE_HITFLOOR] = {rumble_hitfloor, arrlen(rumble_hitfloor)},
};

static const float default_scale[] = {
    [RUMBLE_NONE]     = 0.0f,
    [RUMBLE_ITEMUP]   = 1.0f,
    [RUMBLE_WPNUP]    = 1.0f,
    [RUMBLE_GETPOW]   = 1.0f,
    [RUMBLE_OOF]      = 1.0f,
    [RUMBLE_PAIN]     = 1.0f,
    [RUMBLE_HITFLOOR] = 0.0f,
    [RUMBLE_PLAYER]   = 1.0f,
    [RUMBLE_ORIGIN]   = 0.8f,
    [RUMBLE_PISTOL]   = 0.55f,
    [RUMBLE_SHOTGUN]  = 1.0f,
    [RUMBLE_SSG]      = 1.25f,
    [RUMBLE_CGUN]     = 0.55f,
    [RUMBLE_ROCKET]   = 1.15f,
    [RUMBLE_PLASMA]   = 0.75f,
    [RUMBLE_BFG]      = 1.0f,
};

static void ResetChannel(int handle)
{
    rumble_channel_t *node = &rumble.channels[handle];
    memset(node, 0, sizeof(*node));
    node->prev = node->next = node;
    node->handle = handle;
}

static void ResetAllChannels(void)
{
    for (int i = 0; i < MAX_CHANNELS; i++)
    {
        ResetChannel(i);
    }

    rumble.base.prev = rumble.base.next = &rumble.base;
    rumble.count = 0;
}

static void UnlinkNode(rumble_channel_t *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
    rumble.count--;
}

static boolean RemoveOldNodes(void)
{
    do
    {
        rumble_channel_t *base = &rumble.base;
        rumble_channel_t *node = base->next;

        if (node == base)
        {
            break;
        }

        while (node->next != base)
        {
            node = node->next;
        }

        UnlinkNode(node);
        ResetChannel(node->handle);
    } while (rumble.count >= MAX_RUMBLE_COUNT);

    return (rumble.count < MAX_RUMBLE_COUNT);
}

static void RemoveNode(int handle)
{
    rumble_channel_t *base = &rumble.base;
    rumble_channel_t *node;

    for (node = base->next; node != base; node = node->next)
    {
        if (node->handle == handle)
        {
            UnlinkNode(node);
            break;
        }
    }

    ResetChannel(handle);
}

static void AddNode(int handle)
{
    rumble_channel_t *base = &rumble.base;
    rumble_channel_t *node = &rumble.channels[handle];
    node->prev = base;
    node->next = base->next;
    base->next->prev = node;
    base->next = node;
    rumble.count++;
}

static void FreeFFT(void)
{
    if (fft.window)
    {
        pffft_aligned_free(fft.window);
        fft.window = NULL;
    }

    if (fft.out)
    {
        pffft_aligned_free(fft.out);
        fft.out = NULL;
    }

    if (fft.in)
    {
        pffft_aligned_free(fft.in);
        fft.in = NULL;
    }

    if (fft.setup)
    {
        pffft_destroy_setup(fft.setup);
        fft.setup = NULL;
    }
}

void I_ShutdownRumble(void)
{
    if (!I_GamepadEnabled())
    {
        return;
    }

    if (rumble.enabled)
    {
        SDL_GameControllerRumble(rumble.gamepad, 0, 0, 0);
    }

    for (int i = 1; i < num_sfx; i++)
    {
        sfxinfo_t *sfx = &S_sfx[i];

        if (sfx->name && sfx->cached)
        {
            free(sfx->rumble.low);
            free(sfx->rumble.high);
        }
    }

    free(rumble.channels);
    FreeFFT();
}

void I_InitRumble(void)
{
    if (!I_GamepadEnabled())
    {
        return;
    }

    last_rumble = joy_rumble;
    rumble.channels = malloc(sizeof(*rumble.channels) * MAX_CHANNELS);
    ResetAllChannels();
}

static uint32_t RoundUpPowerOfTwo(int n)
{
    // https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
    uint32_t v = n;
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

static void InitFFT(int rate, int step)
{
    static int last_rate = -1;

    if (last_rate == rate)
    {
        return;
    }

    last_rate = rate;
    FreeFFT();

    // FFT size must be a power of two (see pffft.h).
    const uint32_t step2 = RoundUpPowerOfTwo(step);
    fft.size = (int)CLAMP(step2, 128, 8192);

    fft.setup = pffft_new_setup(fft.size, PFFFT_REAL);

    const size_t size = sizeof(float) * fft.size;
    fft.in = pffft_aligned_malloc(size);
    fft.out = pffft_aligned_malloc(size);
    fft.window = pffft_aligned_malloc(size);

    const float mult = 2.0f * PI_F / (fft.size - 1.0f);
    float sum = 0.0f;

    for (int i = 0; i < fft.size; i++)
    {
        // Hann window.
        fft.window[i] = 0.5f * (1.0f - cosf(i * mult));
        sum += fft.window[i];
    }

    fft.amp_scale = 2.0f / sum;
    fft.freq_scale = 0.5f * rate / fft.size;
    fft.start = lroundf(ceilf(100.0f / fft.freq_scale * 0.5f)) * 2;
    fft.end = lroundf(ceilf(0.9f * 0.5f * rate / fft.freq_scale * 0.5f)) * 2;
}

static float Normalize_Mono8(const void *data, int pos)
{
    const float val = (((byte *)data)[pos] - 128) / 127.0f;
    return MAX(-1.0f, val);
}

static float Normalize_Mono16(const void *data, int pos)
{
    const float val = ((int16_t *)data)[pos] / 32767.0f;
    return MAX(-1.0f, val);
}

static float Normalize_Mono32(const void *data, int pos)
{
    const float val = ((float *)data)[pos];
    return CLAMP(val, -1.0f, 1.0f);
}

static float (*Normalize)(const void *data, int pos);

static void CalcPeakFFT(const byte *data, int rate, int length, int pos,
                        float *amp, float *freq)
{
    const int remaining = length - pos;
    const int size = MIN(fft.size, remaining);

    for (int i = 0; i < size; i++)
    {
        // Fill input buffer with data.
        fft.in[i] = Normalize(data, pos + i);

        // Apply window.
        fft.in[i] *= fft.window[i];
    }

    for (int i = size; i < fft.size; i++)
    {
        // Fill remaining space with zeros.
        fft.in[i] = 0.0f;
    }

    // Forward transform.
    pffft_transform_ordered(fft.setup, fft.in, fft.out, NULL, PFFFT_FORWARD);

    // Find index of max value.
    int max_index = fft.start;
    float max_value = fft.out[max_index] * fft.out[max_index]
                      + fft.out[max_index + 1] * fft.out[max_index + 1];

    for (int i = fft.start + 2; i < fft.end; i += 2)
    {
        // PFFFT returns interleaved values.
        const float current_value =
            fft.out[i] * fft.out[i] + fft.out[i + 1] * fft.out[i + 1];

        if (max_value < current_value)
        {
            max_value = current_value;
            max_index = i;
        }
    }

    *amp = sqrtf(max_value) * fft.amp_scale;
    *freq = max_index * fft.freq_scale;
}

static void SfxToRumble(const byte *data, int rate, int length,
                        float **low, float **high, int *ticlength)
{
    const int ticlen = *ticlength - 1;
    *low = malloc(sizeof(float) * (ticlen + 1));
    *high = malloc(sizeof(float) * (ticlen + 1));
    (*low)[ticlen] = 0.0f;
    (*high)[ticlen] = 0.0f;

    const int step = rate / TICRATE;
    InitFFT(rate, step);

    for (int i = 0; i < ticlen; i++)
    {
        const int pos = i * step;
        float amp_peak, freq_peak;
        CalcPeakFFT(data, rate, length, pos, &amp_peak, &freq_peak);

        if (amp_peak > 0.0001f)
        {
            float weight = (freq_peak - 640.0f) * 0.00625f; // 1/160
            weight = CLAMP(weight, -1.0f, 1.0f);

            const float dBFS = 20.0f * log10f(amp_peak) + 6.0f;
            const float dBFS_low = dBFS - 6.0f * weight;
            const float dBFS_high = dBFS + 6.0f * weight;

            (*low)[i] = powf(10.0f, MIN(dBFS_low, -12.0f) * 0.05f);
            (*high)[i] = powf(10.0f, MIN(dBFS_high, -12.0f) * 0.05f);
        }
        else
        {
            (*low)[i] = 0.0f;
            (*high)[i] = 0.0f;
        }
    }

    for (int i = 0; i < ticlen; i++)
    {
        if ((*low)[i] > 0.0f || (*high)[i] > 0.0f)
        {
            return;
        }
    }

    free(*low);
    free(*high);
    *low = NULL;
    *high = NULL;
    *ticlength = 0;
}

static int GetMonoLength(const sfxinfo_t *sfx, int size)
{
    ALint bits = 0;
    alGetError();
    alGetBufferi(sfx->buffer, AL_BITS, &bits);
    return ((alGetError() == AL_NO_ERROR && bits > 0) ? (size * 8 / bits) : 0);
}

void I_CacheRumble(sfxinfo_t *sfx, int format, const byte *data, int size,
                   int rate)
{
    if (!I_GamepadEnabled() || !sfx || !alIsBuffer(sfx->buffer) || !data
        || size < 1 || rate < TICRATE)
    {
        return;
    }

    switch (format)
    {
        case AL_FORMAT_MONO8:
            Normalize = Normalize_Mono8;
            break;
        case AL_FORMAT_MONO16:
            Normalize = Normalize_Mono16;
            break;
        case AL_FORMAT_MONO_FLOAT32:
            Normalize = Normalize_Mono32;
            break;

        default: // Unsupported format.
            return;
    }

    const int length = GetMonoLength(sfx, size);
    int ticlength = length * TICRATE / rate;

    if (!length || !ticlength)
    {
        return;
    }

    ticlength++;
    ticlength = MIN(ticlength, MAX_TICLENGTH);
    float *low, *high;
    SfxToRumble(data, rate, length, &low, &high, &ticlength);
    sfx->rumble.low = low;
    sfx->rumble.high = high;
    sfx->rumble.ticlength = ticlength;
}

boolean I_RumbleEnabled(void)
{
    return rumble.enabled;
}

boolean I_RumbleSupported(void)
{
    return rumble.supported;
}

void I_RumbleMenuFeedback(void)
{
    if (!rumble.supported || last_rumble == joy_rumble)
    {
        return;
    }

    last_rumble = joy_rumble;
    const uint16_t test = (uint16_t)(rumble.scale * 0.25f);
    SDL_GameControllerRumble(rumble.gamepad, test, test, 125);
}

void I_UpdateRumbleEnabled(void)
{
    rumble.scale = 0.2f * joy_rumble * MAX_RUMBLE_SDL;
    rumble.enabled = (joy_rumble && rumble.supported);
}

void I_SetRumbleSupported(SDL_GameController *gamepad)
{
    rumble.gamepad = gamepad;
    rumble.supported =
        gamepad && (SDL_GameControllerHasRumble(gamepad) == SDL_TRUE);
    I_UpdateRumbleEnabled();
}

void I_ResetRumbleChannel(int handle)
{
    if (!rumble.supported)
    {
        return;
    }

#ifdef RANGECHECK
    if (handle < 0 || handle >= MAX_CHANNELS)
    {
        return;
    }
#endif

    RemoveNode(handle);
}

void I_ResetAllRumbleChannels(void)
{
    if (!rumble.supported)
    {
        return;
    }

    ResetAllChannels();
    SDL_GameControllerRumble(rumble.gamepad, 0, 0, 0);
}

static void GetNodeScale(const rumble_channel_t *node, float *scale_down,
                         float *scale)
{
    *scale = node->scale;

    if (node->type == RUMBLE_BFG)
    {
        if (node->pos > 40)
        {
            *scale *= 1.0f - (float)(node->pos - 40) / (node->ticlength - 40);
        }
    }
    else if (node->pos > RUMBLE_TICS)
    {
        *scale *= 1.0f - (node->pos - RUMBLE_TICS) / 3.0f;
    }

    *scale = MAX(*scale, 0.0f);

    if (node->type != RUMBLE_ORIGIN)
    {
        *scale *= *scale_down;
        *scale_down *= 0.1f;
    }
}

void I_UpdateRumble(void)
{
    if (!rumble.enabled || menuactive || demoplayback)
    {
        return;
    }

    float scale_low = 0.0f;
    float scale_high = 0.0f;
    float scale_down = 1.0f;
    float scale;
    rumble_channel_t *base = &rumble.base;
    rumble_channel_t *node, *next;

    for (node = base->next; node != base; node = next)
    {
        next = node->next;
        GetNodeScale(node, &scale_down, &scale);
        scale_low += node->low[node->pos] * scale;
        scale_high += node->high[node->pos] * scale;
        node->pos++;

        if (node->pos >= node->ticlength)
        {
            UnlinkNode(node);
            ResetChannel(node->handle);
        }
    }

    scale_low *= rumble.scale;
    scale_high *= rumble.scale;
    const uint16_t low = lroundf(MIN(scale_low, MAX_RUMBLE_SDL));
    const uint16_t high = lroundf(MIN(scale_high, MAX_RUMBLE_SDL));
    SDL_GameControllerRumble(rumble.gamepad, low, high, RUMBLE_DURATION);
}

static boolean CalcChannelScale(const mobj_t *listener, const mobj_t *origin,
                                rumble_channel_t *node)
{
    if (node->type != RUMBLE_ORIGIN || !origin || !listener)
    {
        return true;
    }

    const float dx = (listener->x >> FRACBITS) - (origin->x >> FRACBITS);
    const float dy = (listener->y >> FRACBITS) - (origin->y >> FRACBITS);
    const float dist = sqrtf(dx * dx + dy * dy);

    if (dist <= R_CLOSE)
    {
        node->scale = default_scale[node->type];
    }
    else if (dist >= R_CLIP)
    {
        node->scale = 0.0f;
    }
    else
    {
        node->scale = default_scale[node->type] * (R_CLIP - dist) * R_CLOSE
                      / ((R_CLIP - R_CLOSE) * dist);
    }

    return (node->scale > 0.0f);
}

void I_UpdateRumbleParams(const mobj_t *listener, const mobj_t *origin,
                          int handle)
{
    if (!rumble.enabled || menuactive || demoplayback)
    {
        return;
    }

#ifdef RANGECHECK
    if (handle < 0 || handle >= MAX_CHANNELS)
    {
        return;
    }
#endif

    rumble_channel_t *node = &rumble.channels[handle];

    if (node->type == RUMBLE_NONE)
    {
        return;
    }

    if (!CalcChannelScale(listener, origin, node))
    {
        RemoveNode(handle);
    }
}

static float ScaleHitFloor(const mobj_t *listener)
{
    if (!listener || listener->momz >= -8 * GRAVITY)
    {
        return 0.25f;
    }
    else if (listener->momz <= -40 * GRAVITY)
    {
        return 1.0f;
    }
    else
    {
        float scale = (float)FixedToDouble(listener->momz) + 8.0f;
        //scale = (1 - 0.25) / pow(40 - 8, 2) * pow(scale, 2) + 0.25;
        scale = 0.75f / 1024.0f * scale * scale + 0.25f;
        return CLAMP(scale, 0.25f, 1.0f);
    }
}

static boolean GetSFXParams(const mobj_t *listener, const sfxinfo_t *sfx,
                            rumble_channel_t *node)
{
    switch (node->type)
    {
        case RUMBLE_ITEMUP:
        case RUMBLE_WPNUP:
        case RUMBLE_GETPOW:
        case RUMBLE_OOF:
        case RUMBLE_PAIN:
            node->low = node->high = presets[node->type].data;
            node->ticlength = presets[node->type].ticlength;
            break;

        case RUMBLE_HITFLOOR:
            node->low = node->high = presets[node->type].data;
            node->scale = ScaleHitFloor(listener);
            node->ticlength =
                lroundf(node->scale * presets[node->type].ticlength);
            break;

        case RUMBLE_PLAYER:
        case RUMBLE_ORIGIN:
        case RUMBLE_PISTOL:
        case RUMBLE_SHOTGUN:
        case RUMBLE_SSG:
        case RUMBLE_CGUN:
        case RUMBLE_ROCKET:
        case RUMBLE_PLASMA:
        case RUMBLE_BFG:
            node->low = sfx->rumble.low;
            node->high = sfx->rumble.high;
            node->ticlength = sfx->rumble.ticlength;
            break;

        default:
            node->low = node->high = NULL;
            node->ticlength = 0;
            break;
    }

    return (node->low && node->high && node->ticlength > 0);
}

void I_StartRumble(const mobj_t *listener, const mobj_t *origin,
                   const sfxinfo_t *sfx, int handle, rumble_type_t rumble_type)
{
    if (!rumble.enabled || menuactive || demoplayback || !sfx
        || !sfx->rumble.low || !sfx->rumble.high)
    {
        return;
    }

#ifdef RANGECHECK
    if (handle < 0 || handle >= MAX_CHANNELS)
    {
        return;
    }
#endif

    rumble_channel_t *node = &rumble.channels[handle];

    if (node->type != RUMBLE_NONE)
    {
        RemoveNode(handle);
    }

    if (rumble.count >= MAX_RUMBLE_COUNT && !RemoveOldNodes())
    {
        return;
    }

    node->type = rumble_type;
    node->scale = default_scale[node->type];

    if (CalcChannelScale(listener, origin, node)
        && GetSFXParams(listener, sfx, node))
    {
        AddNode(handle);
    }
    else
    {
        ResetChannel(handle);
    }
}

// For gyro calibration only.
void I_DisableRumble(void)
{
    if (!rumble.enabled)
    {
        return;
    }

    rumble.enabled = false;
    ResetAllChannels();
    SDL_GameControllerRumble(rumble.gamepad, 0, 0, 0);
}

void I_BindRumbleVariables(void)
{
    BIND_NUM_GENERAL(joy_rumble, 5, 0, 10,
        "Rumble intensity (0 = Off; 10 = 100%)");
}

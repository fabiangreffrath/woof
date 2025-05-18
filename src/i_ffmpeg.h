
#ifndef I_FFMPEG
#define I_FFMPEG

#include "doomtype.h"
#include <SDL_pixels.h>

void I_MPG_Init(void);
void I_MPG_Flush(void);
void I_MPG_Shutdown(void);
void I_MPG_EncodeVideo(const pixel_t *frame);
void I_MPG_SetPalette(SDL_Color *palette);
void I_MPG_EncodeAudio(void);

#endif

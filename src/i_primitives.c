
#include <SDL3/SDL.h>

#include "i_video.h"
#include "m_array.h"

static SDL_Renderer *renderer;
static SDL_Palette *palette;
static SDL_Vertex *line_batch_vertices;
static int *line_batch_indices;

void I_InitPrimitives(void)
{
    renderer = I_GetSDLRenderer();
    palette = I_GetSDLPalette();
}

void I_BeginLineBatch(void)
{
    array_clear(line_batch_vertices);
    array_clear(line_batch_indices);
}

void I_EndLineBatch(void)
{
    if (array_size(line_batch_vertices) > 0)
    {
        SDL_RenderGeometry(renderer, NULL, line_batch_vertices,
                           array_size(line_batch_vertices), line_batch_indices,
                           array_size(line_batch_indices));
    }
}

inline static void ColorToFColor(SDL_Color *color, SDL_FColor *fcolor)
{
    fcolor->r = color->r / 255.0f;
    fcolor->g = color->g / 255.0f;
    fcolor->b = color->b / 255.0f;
    fcolor->a = color->a / 255.0f;
}

void I_DrawLine(int x1, int y1, int x2, int y2, int pixelvalue, int thickness)
{
    SDL_FColor color;
    ColorToFColor(&palette->colors[pixelvalue], &color);
    SDL_FColor color_trans = {color.r, color.g, color.b, 0.0f};

    float dx = (float)x2 - x1;
    float dy = (float)y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);

    if (len == 0.0f)
    {
        return;
    }

    const float inner_half_width = thickness / 2.0f;
    const float outer_half_width = inner_half_width + 1.0f;

    float nx = -dy / len;
    float ny = dx / len;

    int base_vertex = array_size(line_batch_vertices);

    // 4 vertices for each end of the line segment
    // outer-left, inner-left, inner-right, outer-right
    SDL_Vertex verts[8];

    // Vertices for point 1
    verts[0].position = (SDL_FPoint){x1 + nx * outer_half_width,
                                     y1 + ny * outer_half_width};
    verts[0].color = color_trans;
    verts[1].position = (SDL_FPoint){x1 + nx * inner_half_width,
                                     y1 + ny * inner_half_width};
    verts[1].color = color;
    verts[2].position = (SDL_FPoint){x1 - nx * inner_half_width,
                                     y1 - ny * inner_half_width};
    verts[2].color = color;
    verts[3].position = (SDL_FPoint){x1 - nx * outer_half_width,
                                     y1 - ny * outer_half_width};
    verts[3].color = color_trans;

    // Vertices for point 2
    verts[4].position = (SDL_FPoint){x2 + nx * outer_half_width,
                                     y2 + ny * outer_half_width};
    verts[4].color = color_trans;
    verts[5].position = (SDL_FPoint){x2 + nx * inner_half_width,
                                     y2 + ny * inner_half_width};
    verts[5].color = color;
    verts[6].position = (SDL_FPoint){x2 - nx * inner_half_width,
                                     y2 - ny * inner_half_width};
    verts[6].color = color;
    verts[7].position = (SDL_FPoint){x2 - nx * outer_half_width,
                                     y2 - ny * outer_half_width};
    verts[7].color = color_trans;

    for (int i = 0; i < 8; i++)
    {
        array_push(line_batch_vertices, verts[i]);
    }

    // 3 quads, 2 triangles each, 6 indices per quad
    int indices[] = {// left fade
                     base_vertex + 0, base_vertex + 1, base_vertex + 5,
                     base_vertex + 0, base_vertex + 5, base_vertex + 4,
                     // solid
                     base_vertex + 1, base_vertex + 2, base_vertex + 6,
                     base_vertex + 1, base_vertex + 6, base_vertex + 5,
                     // right fade
                     base_vertex + 2, base_vertex + 3, base_vertex + 7,
                     base_vertex + 2, base_vertex + 7, base_vertex + 6};
    
    for (int i = 0; i < 18; i++)
    {
        array_push(line_batch_indices, indices[i]);
    }
}

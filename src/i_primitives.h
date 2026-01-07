
#ifndef I_PRIMITIVES_H
#define I_PRIMITIVES_H

void I_InitPrimitives(void);

void I_BeginLineBatch(void);
void I_EndLineBatch(void);
void I_DrawLine(int x1, int y1, int x2, int y2, int pixelvalue, int thickness);

#endif

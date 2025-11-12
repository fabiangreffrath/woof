//
// sRGB transform (C)
//
// Copyright (c) 2017 Project Nayuki. (MIT License)
// https://www.nayuki.io/page/srgb-transform-library
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
// - The above copyright notice and this permission notice shall be included in
//   all copies or substantial portions of the Software.
// - The Software is provided "as is", without warranty of any kind, express or
//   implied, including but not limited to the warranties of merchantability,
//   fitness for a particular purpose and noninfringement. In no event shall the
//   authors or copyright holders be liable for any claim, damages or other
//   liability, whether in an action of contract, tort or otherwise, arising from,
//   out of or in connection with the Software or the use or other dealings in the
//   Software.
//
// References for gamma adjustment:
//   https://en.wikipedia.org/wiki/SRGB#Definition
//   https://www.youtube.com/watch?v=LKnqECcg6Gw
//   https://www.nayuki.io/page/srgb-transform-library
//

#ifndef __R_SRGB_TRANSFORM__
#define __R_SRGB_TRANSFORM__

#include <math.h>
#include "doomtype.h"

inline static const double byte_to_linear(const byte c)
{
    double cs = c / 255.0;
    if (cs <= 0.04045)
    {
        return cs / 12.92;
    }
    else
    {
        return pow((cs + 0.055) / 1.055, 2.4);
    }
}

inline static const byte linear_to_byte(double x)
{
    if (x <= 0.0031308)
    {
        x *= 12.92;
    }
    else
    {
        x = 1.055 * pow(x, 1.0 / 2.4) - 0.055;
    }

    x = CLAMP(x, 0.0, 1.0);
    return (x * 255.0 + 0.5);
}

#endif

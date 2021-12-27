//
// Copyright(C) 2021 Roman Fomin
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
//      Packed struct macro

#ifdef PACKED_SECTION
    #error "PACKED_SECTION is already defined!"
#else
    #define PACKED_SECTION
#endif

/* Declare compiler-specific directives for structure packing. */
#if defined(__GNUC__)
    #define PACKEDPREFIX
    #if defined(_WIN32) && !defined(__clang__)
        #define PACKEDATTR __attribute__((packed,gcc_struct))
    #else
        #define PACKEDATTR __attribute__((packed))
    #endif
#elif defined(__WATCOMC__)
    #define PACKEDPREFIX _Packed
    #define PACKEDATTR
#elif defined(_MSC_VER)
    #pragma pack(push, 1)
    #define PACKEDPREFIX
    #define PACKEDATTR
#endif

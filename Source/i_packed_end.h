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

// Error check - PACKED_SECTION is defined in i_packed_start.h and undefined in
// i_packed_end.h. If it is NOT defined at this point, then there is a
// missing include of i_packed_start.h.

#ifdef PACKED_SECTION
    #undef PACKED_SECTION
#else
    #error "PACKED_SECTION is NOT defined!"
#endif

#if defined(_MSC_VER)
    #pragma pack(pop)
#endif

// Compiler-specific directives for structure packing are declared in
// i_packed_start.h. This marks the end of the structure packing section,
// so, undef them here.

#undef PACKEDPREFIX
#undef PACKEDATTR

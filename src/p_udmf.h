//
//  Copyright (C) 2025 Guilherme Miranda
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  DESCRIPTION:
//    Universal Doom Map Format support.
//

#ifndef __P_UDMF__
#define __P_UDMF__

#include "p_extnodes.h"

extern void UDMF_LoadMap(int lumpnum, nodeformat_t *nodeformat,
                         int *gen_blockmap, int *pad_reject);
extern void UDMF_LoadThings(void);

#endif

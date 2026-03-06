//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2025 Guilherme Miranda
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
//
// Dehacked I/O code (does all reads from dehacked files)
//

#ifndef DEH_IO_H
#define DEH_IO_H

#include "deh_defs.h"
#include "doomtype.h"
#include "i_printf.h"

deh_context_t *DEH_OpenFile(const char *filename);
deh_context_t *DEH_OpenLump(int lumpnum);
void DEH_CloseFile(deh_context_t *context);
int DEH_GetChar(deh_context_t *context);
char *DEH_ReadLine(deh_context_t *context, boolean extended);
void DEH_PrintMessage(deh_context_t *context, verbosity_t verbosity, const char *msg, ...) PRINTF_ATTR(3, 4);
boolean DEH_HadError(deh_context_t *context);
char *DEH_FileName(deh_context_t *context); // [crispy] returns filename
#define DEH_Debug(context, ...) DEH_PrintMessage(context, VB_DEBUG, __VA_ARGS__)
#define DEH_Warning(context, ...) DEH_PrintMessage(context, VB_WARNING, __VA_ARGS__)
#define DEH_Error(context, ...) DEH_PrintMessage(context, VB_ERROR, __VA_ARGS__)

#endif /* #ifndef DEH_IO_H */

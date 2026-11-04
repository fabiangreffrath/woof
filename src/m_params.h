//
//  Copyright (C) 2026 Roman Fomin
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

#ifndef PARAMS_TABLE_H
#define PARAMS_TABLE_H

typedef enum
{
    TypeFlag,
    TypeString,
    TypeInt,
    TypeFloat,
    TypeFileList,
    TypeTwoArgs,
    TypeOneOrTwoArgs,
    TypeOptArg,
} param_type_t;

typedef enum
{
    SectionGeneral,
    SectionGame,
    SectionNet,
    SectionDemo,
    SectionCompat,
    SectionVideo,
    SectionMod,
    SectionDebug,
} param_section_t;

typedef struct
{
    const char *name;
    param_type_t type;
    param_section_t section;
    const char *help;
} param_def_t;

const param_def_t *M_FindParam(const char *arg);

const char *M_GetParamHelpString(void);
void M_GenerateParamMarkdown(const char *filename);
void M_PrintParamHelp(void);

#endif

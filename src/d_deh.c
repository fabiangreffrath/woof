//
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
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
// Dehacked file support
// New for the TeamTNT "Boom" engine
//
// Author: Ty Halderman, TeamTNT
//
//--------------------------------------------------------------------

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "d_items.h"
#include "d_main.h"
#include "d_think.h"
#include "doomdef.h"
#include "doomtype.h"
#include "dsdhacked.h"
#include "dstrings.h"
#include "g_game.h"
#include "i_printf.h"
#include "i_system.h"
#include "info.h"
#include "m_argv.h"
#include "m_array.h"
#include "m_cheat.h"
#include "m_fixed.h"
#include "m_io.h"
#include "m_misc.h"
#include "memio.h"
#include "p_action.h"
#include "p_inter.h"
#include "p_mobj.h"
#include "sounds.h"
#include "w_wad.h"
#include "z_zone.h"

static boolean bfgcells_modified = false;

// killough 10/98: new functions, to allow processing DEH files in-memory
// (e.g. from wads)

typedef struct
{
    MEMFILE *memfile;
    FILE *file;
} DEHFILE;

// killough 10/98: emulate IO whether input really comes from a file or not

// haleyjd: got rid of macros for MSCV

static char *deh_fgets(char *str, size_t count, DEHFILE *fp)
{
    if (fp->file)
    {
        return fgets(str, count, fp->file);
    }
    else if (fp->memfile)
    {
        return mem_fgets(str, count, fp->memfile);
    }

    return NULL;
}

static int deh_feof(DEHFILE *fp)
{
    if (fp->file)
    {
        return feof(fp->file);
    }
    else if (fp->memfile)
    {
        return mem_feof(fp->memfile);
    }

    return 0;
}

static int deh_fgetc(DEHFILE *fp)
{
    if (fp->file)
    {
        return fgetc(fp->file);
    }
    else if (fp->memfile)
    {
        return mem_fgetc(fp->memfile);
    }

    return -1;
}

static long deh_ftell(DEHFILE *fp)
{
    if (fp->file)
    {
        return ftell(fp->file);
    }
    else if (fp->memfile)
    {
        return mem_ftell(fp->memfile);
    }

    return 0;
}

static int deh_fseek(DEHFILE *fp, long offset)
{
    if (fp->file)
    {
        return fseek(fp->file, offset, SEEK_SET);
    }
    else if (fp->memfile)
    {
        return mem_fseek(fp->memfile, offset, MEM_SEEK_SET);
    }

    return 0;
}

static FILE *deh_log_file;

static void PRINTF_ATTR(1, 2) deh_log(const char *fmt, ...)
{
    va_list v;

    if (!deh_log_file)
    {
        return;
    }

    va_start(v, fmt);
    vfprintf(deh_log_file, fmt, v);
    va_end(v);
}

// variables used in other routines
boolean deh_pars = false; // in wi_stuff to allow pars in modified games

boolean deh_set_blood_color = false;

int deh_maxhealth;
boolean deh_set_maxhealth = false;

char **dehfiles = NULL; // filenames of .deh files for demo footer

// #include "d_deh.h" -- we don't do that here but we declare the
// variables.  This externalizes everything that there is a string
// set for in the language files.  See d_deh.h for detailed comments,
// original English values etc.  These are set to the macro values,
// which are set by D_ENGLSH.H or D_FRENCH.H(etc).  BEX files are a
// better way of changing these strings globally by language.

static char *s_D_DEVSTR = D_DEVSTR;
static char *s_D_CDROM = D_CDROM;
static char *s_PRESSKEY = PRESSKEY;
static char *s_PRESSYN = PRESSYN;
char *s_QUITMSG = QUITMSG;
char *s_QUITMSG1 = QUITMSG1;
char *s_QUITMSG2 = QUITMSG2;
char *s_QUITMSG3 = QUITMSG3;
char *s_QUITMSG4 = QUITMSG4;
char *s_QUITMSG5 = QUITMSG5;
char *s_QUITMSG6 = QUITMSG6;
char *s_QUITMSG7 = QUITMSG7;
char *s_QUITMSG8 = QUITMSG8;
char *s_QUITMSG9 = QUITMSG9;
char *s_QUITMSG10 = QUITMSG10;
char *s_QUITMSG11 = QUITMSG11;
char *s_QUITMSG12 = QUITMSG12;
char *s_QUITMSG13 = QUITMSG13;
char *s_QUITMSG14 = QUITMSG14;
char *s_LOADNET = LOADNET;     // PRESSKEY; // killough 4/4/98:
char *s_QLOADNET = QLOADNET;   // PRESSKEY;
char *s_QSAVESPOT = QSAVESPOT; // PRESSKEY;
char *s_SAVEDEAD = SAVEDEAD;   // PRESSKEY; // remove duplicate y/n
char *s_QSPROMPT = QSPROMPT;   // PRESSYN;
char *s_QLPROMPT = QLPROMPT;   // PRESSYN;
char *s_NEWGAME = NEWGAME;     // PRESSKEY;
char *s_NIGHTMARE = NIGHTMARE; // PRESSYN;
char *s_SWSTRING = SWSTRING;   // PRESSKEY;
char *s_MSGOFF = MSGOFF;
char *s_MSGON = MSGON;
char *s_NETEND = NETEND;   // PRESSKEY;
char *s_ENDGAME = ENDGAME; // PRESSYN; // killough 4/4/98: end
char *s_DOSY = DOSY;
char *s_DETAILHI = DETAILHI;
char *s_DETAILLO = DETAILLO;
char *s_GAMMALVL0 = GAMMALVL0;
char *s_GAMMALVL1 = GAMMALVL1;
char *s_GAMMALVL2 = GAMMALVL2;
char *s_GAMMALVL3 = GAMMALVL3;
char *s_GAMMALVL4 = GAMMALVL4;
char *s_EMPTYSTRING = EMPTYSTRING;
char *s_GOTARMOR = GOTARMOR;
char *s_GOTMEGA = GOTMEGA;
char *s_GOTHTHBONUS = GOTHTHBONUS;
char *s_GOTARMBONUS = GOTARMBONUS;
char *s_GOTSTIM = GOTSTIM;
char *s_GOTMEDINEED = GOTMEDINEED;
char *s_GOTMEDIKIT = GOTMEDIKIT;
char *s_GOTSUPER = GOTSUPER;
char *s_GOTBLUECARD = GOTBLUECARD;
char *s_GOTYELWCARD = GOTYELWCARD;
char *s_GOTREDCARD = GOTREDCARD;
char *s_GOTBLUESKUL = GOTBLUESKUL;
char *s_GOTYELWSKUL = GOTYELWSKUL;
char *s_GOTREDSKULL = GOTREDSKULL;
char *s_GOTINVUL = GOTINVUL;
char *s_GOTBERSERK = GOTBERSERK;
char *s_GOTINVIS = GOTINVIS;
char *s_GOTSUIT = GOTSUIT;
char *s_GOTMAP = GOTMAP;
char *s_GOTVISOR = GOTVISOR;
char *s_GOTMSPHERE = GOTMSPHERE;
char *s_GOTCLIP = GOTCLIP;
char *s_GOTCLIPBOX = GOTCLIPBOX;
char *s_GOTROCKET = GOTROCKET;
char *s_GOTROCKBOX = GOTROCKBOX;
char *s_GOTCELL = GOTCELL;
char *s_GOTCELLBOX = GOTCELLBOX;
char *s_GOTSHELLS = GOTSHELLS;
char *s_GOTSHELLBOX = GOTSHELLBOX;
char *s_GOTBACKPACK = GOTBACKPACK;
char *s_GOTBFG9000 = GOTBFG9000;
char *s_GOTCHAINGUN = GOTCHAINGUN;
char *s_GOTCHAINSAW = GOTCHAINSAW;
char *s_GOTLAUNCHER = GOTLAUNCHER;
char *s_GOTPLASMA = GOTPLASMA;
char *s_GOTSHOTGUN = GOTSHOTGUN;
char *s_GOTSHOTGUN2 = GOTSHOTGUN2;
char *s_BETA_BONUS3 = BETA_BONUS3;
char *s_BETA_BONUS4 = BETA_BONUS4;
char *s_PD_BLUEO = PD_BLUEO;
char *s_PD_REDO = PD_REDO;
char *s_PD_YELLOWO = PD_YELLOWO;
char *s_PD_BLUEK = PD_BLUEK;
char *s_PD_REDK = PD_REDK;
char *s_PD_YELLOWK = PD_YELLOWK;
char *s_PD_BLUEC = PD_BLUEC;
char *s_PD_REDC = PD_REDC;
char *s_PD_YELLOWC = PD_YELLOWC;
char *s_PD_BLUES = PD_BLUES;
char *s_PD_REDS = PD_REDS;
char *s_PD_YELLOWS = PD_YELLOWS;
char *s_PD_ANY = PD_ANY;
char *s_PD_ALL3 = PD_ALL3;
char *s_PD_ALL6 = PD_ALL6;
char *s_GGSAVED = GGSAVED;
static char *s_HUSTR_MSGU = HUSTR_MSGU;
static char *s_HUSTR_E1M1 = HUSTR_E1M1;
static char *s_HUSTR_E1M2 = HUSTR_E1M2;
static char *s_HUSTR_E1M3 = HUSTR_E1M3;
static char *s_HUSTR_E1M4 = HUSTR_E1M4;
static char *s_HUSTR_E1M5 = HUSTR_E1M5;
static char *s_HUSTR_E1M6 = HUSTR_E1M6;
static char *s_HUSTR_E1M7 = HUSTR_E1M7;
static char *s_HUSTR_E1M8 = HUSTR_E1M8;
static char *s_HUSTR_E1M9 = HUSTR_E1M9;
static char *s_HUSTR_E2M1 = HUSTR_E2M1;
static char *s_HUSTR_E2M2 = HUSTR_E2M2;
static char *s_HUSTR_E2M3 = HUSTR_E2M3;
static char *s_HUSTR_E2M4 = HUSTR_E2M4;
static char *s_HUSTR_E2M5 = HUSTR_E2M5;
static char *s_HUSTR_E2M6 = HUSTR_E2M6;
static char *s_HUSTR_E2M7 = HUSTR_E2M7;
static char *s_HUSTR_E2M8 = HUSTR_E2M8;
static char *s_HUSTR_E2M9 = HUSTR_E2M9;
static char *s_HUSTR_E3M1 = HUSTR_E3M1;
static char *s_HUSTR_E3M2 = HUSTR_E3M2;
static char *s_HUSTR_E3M3 = HUSTR_E3M3;
static char *s_HUSTR_E3M4 = HUSTR_E3M4;
static char *s_HUSTR_E3M5 = HUSTR_E3M5;
static char *s_HUSTR_E3M6 = HUSTR_E3M6;
static char *s_HUSTR_E3M7 = HUSTR_E3M7;
static char *s_HUSTR_E3M8 = HUSTR_E3M8;
static char *s_HUSTR_E3M9 = HUSTR_E3M9;
static char *s_HUSTR_E4M1 = HUSTR_E4M1;
static char *s_HUSTR_E4M2 = HUSTR_E4M2;
static char *s_HUSTR_E4M3 = HUSTR_E4M3;
static char *s_HUSTR_E4M4 = HUSTR_E4M4;
static char *s_HUSTR_E4M5 = HUSTR_E4M5;
static char *s_HUSTR_E4M6 = HUSTR_E4M6;
static char *s_HUSTR_E4M7 = HUSTR_E4M7;
static char *s_HUSTR_E4M8 = HUSTR_E4M8;
static char *s_HUSTR_E4M9 = HUSTR_E4M9;
static char *s_HUSTR_1 = HUSTR_1;
static char *s_HUSTR_2 = HUSTR_2;
static char *s_HUSTR_3 = HUSTR_3;
static char *s_HUSTR_4 = HUSTR_4;
static char *s_HUSTR_5 = HUSTR_5;
static char *s_HUSTR_6 = HUSTR_6;
static char *s_HUSTR_7 = HUSTR_7;
static char *s_HUSTR_8 = HUSTR_8;
static char *s_HUSTR_9 = HUSTR_9;
static char *s_HUSTR_10 = HUSTR_10;
static char *s_HUSTR_11 = HUSTR_11;
static char *s_HUSTR_12 = HUSTR_12;
static char *s_HUSTR_13 = HUSTR_13;
static char *s_HUSTR_14 = HUSTR_14;
static char *s_HUSTR_15 = HUSTR_15;
static char *s_HUSTR_16 = HUSTR_16;
static char *s_HUSTR_17 = HUSTR_17;
static char *s_HUSTR_18 = HUSTR_18;
static char *s_HUSTR_19 = HUSTR_19;
static char *s_HUSTR_20 = HUSTR_20;
static char *s_HUSTR_21 = HUSTR_21;
static char *s_HUSTR_22 = HUSTR_22;
static char *s_HUSTR_23 = HUSTR_23;
static char *s_HUSTR_24 = HUSTR_24;
static char *s_HUSTR_25 = HUSTR_25;
static char *s_HUSTR_26 = HUSTR_26;
static char *s_HUSTR_27 = HUSTR_27;
static char *s_HUSTR_28 = HUSTR_28;
static char *s_HUSTR_29 = HUSTR_29;
static char *s_HUSTR_30 = HUSTR_30;
static char *s_HUSTR_31 = HUSTR_31;
static char *s_HUSTR_32 = HUSTR_32;
static char *s_PHUSTR_1 = PHUSTR_1;
static char *s_PHUSTR_2 = PHUSTR_2;
static char *s_PHUSTR_3 = PHUSTR_3;
static char *s_PHUSTR_4 = PHUSTR_4;
static char *s_PHUSTR_5 = PHUSTR_5;
static char *s_PHUSTR_6 = PHUSTR_6;
static char *s_PHUSTR_7 = PHUSTR_7;
static char *s_PHUSTR_8 = PHUSTR_8;
static char *s_PHUSTR_9 = PHUSTR_9;
static char *s_PHUSTR_10 = PHUSTR_10;
static char *s_PHUSTR_11 = PHUSTR_11;
static char *s_PHUSTR_12 = PHUSTR_12;
static char *s_PHUSTR_13 = PHUSTR_13;
static char *s_PHUSTR_14 = PHUSTR_14;
static char *s_PHUSTR_15 = PHUSTR_15;
static char *s_PHUSTR_16 = PHUSTR_16;
static char *s_PHUSTR_17 = PHUSTR_17;
static char *s_PHUSTR_18 = PHUSTR_18;
static char *s_PHUSTR_19 = PHUSTR_19;
static char *s_PHUSTR_20 = PHUSTR_20;
static char *s_PHUSTR_21 = PHUSTR_21;
static char *s_PHUSTR_22 = PHUSTR_22;
static char *s_PHUSTR_23 = PHUSTR_23;
static char *s_PHUSTR_24 = PHUSTR_24;
static char *s_PHUSTR_25 = PHUSTR_25;
static char *s_PHUSTR_26 = PHUSTR_26;
static char *s_PHUSTR_27 = PHUSTR_27;
static char *s_PHUSTR_28 = PHUSTR_28;
static char *s_PHUSTR_29 = PHUSTR_29;
static char *s_PHUSTR_30 = PHUSTR_30;
static char *s_PHUSTR_31 = PHUSTR_31;
static char *s_PHUSTR_32 = PHUSTR_32;
static char *s_THUSTR_1 = THUSTR_1;
static char *s_THUSTR_2 = THUSTR_2;
static char *s_THUSTR_3 = THUSTR_3;
static char *s_THUSTR_4 = THUSTR_4;
static char *s_THUSTR_5 = THUSTR_5;
static char *s_THUSTR_6 = THUSTR_6;
static char *s_THUSTR_7 = THUSTR_7;
static char *s_THUSTR_8 = THUSTR_8;
static char *s_THUSTR_9 = THUSTR_9;
static char *s_THUSTR_10 = THUSTR_10;
static char *s_THUSTR_11 = THUSTR_11;
static char *s_THUSTR_12 = THUSTR_12;
static char *s_THUSTR_13 = THUSTR_13;
static char *s_THUSTR_14 = THUSTR_14;
static char *s_THUSTR_15 = THUSTR_15;
static char *s_THUSTR_16 = THUSTR_16;
static char *s_THUSTR_17 = THUSTR_17;
static char *s_THUSTR_18 = THUSTR_18;
static char *s_THUSTR_19 = THUSTR_19;
static char *s_THUSTR_20 = THUSTR_20;
static char *s_THUSTR_21 = THUSTR_21;
static char *s_THUSTR_22 = THUSTR_22;
static char *s_THUSTR_23 = THUSTR_23;
static char *s_THUSTR_24 = THUSTR_24;
static char *s_THUSTR_25 = THUSTR_25;
static char *s_THUSTR_26 = THUSTR_26;
static char *s_THUSTR_27 = THUSTR_27;
static char *s_THUSTR_28 = THUSTR_28;
static char *s_THUSTR_29 = THUSTR_29;
static char *s_THUSTR_30 = THUSTR_30;
static char *s_THUSTR_31 = THUSTR_31;
static char *s_THUSTR_32 = THUSTR_32;
static char *s_HUSTR_CHATMACRO1 = HUSTR_CHATMACRO1;
static char *s_HUSTR_CHATMACRO2 = HUSTR_CHATMACRO2;
static char *s_HUSTR_CHATMACRO3 = HUSTR_CHATMACRO3;
static char *s_HUSTR_CHATMACRO4 = HUSTR_CHATMACRO4;
static char *s_HUSTR_CHATMACRO5 = HUSTR_CHATMACRO5;
static char *s_HUSTR_CHATMACRO6 = HUSTR_CHATMACRO6;
static char *s_HUSTR_CHATMACRO7 = HUSTR_CHATMACRO7;
static char *s_HUSTR_CHATMACRO8 = HUSTR_CHATMACRO8;
static char *s_HUSTR_CHATMACRO9 = HUSTR_CHATMACRO9;
static char *s_HUSTR_CHATMACRO0 = HUSTR_CHATMACRO0;
static char *s_HUSTR_TALKTOSELF1 = HUSTR_TALKTOSELF1;
static char *s_HUSTR_TALKTOSELF2 = HUSTR_TALKTOSELF2;
static char *s_HUSTR_TALKTOSELF3 = HUSTR_TALKTOSELF3;
static char *s_HUSTR_TALKTOSELF4 = HUSTR_TALKTOSELF4;
static char *s_HUSTR_TALKTOSELF5 = HUSTR_TALKTOSELF5;
static char *s_HUSTR_MESSAGESENT = HUSTR_MESSAGESENT;
char *s_HUSTR_SECRETFOUND = HUSTR_SECRETFOUND;
char *s_HUSTR_PLRGREEN = HUSTR_PLRGREEN;
char *s_HUSTR_PLRINDIGO = HUSTR_PLRINDIGO;
char *s_HUSTR_PLRBROWN = HUSTR_PLRBROWN;
char *s_HUSTR_PLRRED = HUSTR_PLRRED;
// char sc_HUSTR_KEYGREEN   = HUSTR_KEYGREEN;
// char sc_HUSTR_KEYINDIGO  = HUSTR_KEYINDIGO;
// char sc_HUSTR_KEYBROWN   = HUSTR_KEYBROWN;
// char sc_HUSTR_KEYRED     = HUSTR_KEYRED;
char *s_AMSTR_FOLLOWON = AMSTR_FOLLOWON;
char *s_AMSTR_FOLLOWOFF = AMSTR_FOLLOWOFF;
char *s_AMSTR_GRIDON = AMSTR_GRIDON;
char *s_AMSTR_GRIDOFF = AMSTR_GRIDOFF;
char *s_AMSTR_MARKEDSPOT = AMSTR_MARKEDSPOT;
char *s_AMSTR_MARKSCLEARED = AMSTR_MARKSCLEARED;
char *s_AMSTR_OVERLAYON = AMSTR_OVERLAYON;
char *s_AMSTR_OVERLAYOFF = AMSTR_OVERLAYOFF;
char *s_AMSTR_ROTATEON = AMSTR_ROTATEON;
char *s_AMSTR_ROTATEOFF = AMSTR_ROTATEOFF;
char *s_STSTR_MUS = STSTR_MUS;
char *s_STSTR_NOMUS = STSTR_NOMUS;
char *s_STSTR_DQDON = STSTR_DQDON;
char *s_STSTR_DQDOFF = STSTR_DQDOFF;
char *s_STSTR_KFAADDED = STSTR_KFAADDED;
char *s_STSTR_FAADDED = STSTR_FAADDED;
char *s_STSTR_NCON = STSTR_NCON;
char *s_STSTR_NCOFF = STSTR_NCOFF;
char *s_STSTR_BEHOLD = STSTR_BEHOLD;
char *s_STSTR_BEHOLDX = STSTR_BEHOLDX;
char *s_STSTR_CHOPPERS = STSTR_CHOPPERS;
char *s_STSTR_CLEV = STSTR_CLEV;
char *s_STSTR_COMPON = STSTR_COMPON;
char *s_STSTR_COMPOFF = STSTR_COMPOFF;
char *s_E1TEXT = E1TEXT;
char *s_E2TEXT = E2TEXT;
char *s_E3TEXT = E3TEXT;
char *s_E4TEXT = E4TEXT;
char *s_C1TEXT = C1TEXT;
char *s_C2TEXT = C2TEXT;
char *s_C3TEXT = C3TEXT;
char *s_C4TEXT = C4TEXT;
char *s_C5TEXT = C5TEXT;
char *s_C6TEXT = C6TEXT;
char *s_P1TEXT = P1TEXT;
char *s_P2TEXT = P2TEXT;
char *s_P3TEXT = P3TEXT;
char *s_P4TEXT = P4TEXT;
char *s_P5TEXT = P5TEXT;
char *s_P6TEXT = P6TEXT;
char *s_T1TEXT = T1TEXT;
char *s_T2TEXT = T2TEXT;
char *s_T3TEXT = T3TEXT;
char *s_T4TEXT = T4TEXT;
char *s_T5TEXT = T5TEXT;
char *s_T6TEXT = T6TEXT;
char *s_CC_ZOMBIE = CC_ZOMBIE;
char *s_CC_SHOTGUN = CC_SHOTGUN;
char *s_CC_HEAVY = CC_HEAVY;
char *s_CC_IMP = CC_IMP;
char *s_CC_DEMON = CC_DEMON;
char *s_CC_LOST = CC_LOST;
char *s_CC_CACO = CC_CACO;
char *s_CC_HELL = CC_HELL;
char *s_CC_BARON = CC_BARON;
char *s_CC_ARACH = CC_ARACH;
char *s_CC_PAIN = CC_PAIN;
char *s_CC_REVEN = CC_REVEN;
char *s_CC_MANCU = CC_MANCU;
char *s_CC_ARCH = CC_ARCH;
char *s_CC_SPIDER = CC_SPIDER;
char *s_CC_CYBER = CC_CYBER;
char *s_CC_HERO = CC_HERO;
// Ty 03/30/98 - new substitutions for background textures
//               during int screens
char *bgflatE1 = "FLOOR4_8";   // end of DOOM Episode 1
char *bgflatE2 = "SFLR6_1";    // end of DOOM Episode 2
char *bgflatE3 = "MFLR8_4";    // end of DOOM Episode 3
char *bgflatE4 = "MFLR8_3";    // end of DOOM Episode 4
char *bgflat06 = "SLIME16";    // DOOM2 after MAP06
char *bgflat11 = "RROCK14";    // DOOM2 after MAP11
char *bgflat20 = "RROCK07";    // DOOM2 after MAP20
char *bgflat30 = "RROCK17";    // DOOM2 after MAP30
char *bgflat15 = "RROCK13";    // DOOM2 going MAP15 to MAP31
char *bgflat31 = "RROCK19";    // DOOM2 going MAP31 to MAP32
char *bgcastcall = "BOSSBACK"; // Panel behind cast call

char *startup1 = ""; // blank lines are default and are not printed
char *startup2 = "";
char *startup3 = "";
char *startup4 = "";
char *startup5 = "";

// [FG] obituaries

char *s_OB_CRUSH = OB_CRUSH;
char *s_OB_SLIME = OB_SLIME;
char *s_OB_LAVA = OB_LAVA;
char *s_OB_KILLEDSELF = OB_KILLEDSELF;
char *s_OB_VOODOO = OB_VOODOO;
char *s_OB_MONTELEFRAG = OB_MONTELEFRAG;
char *s_OB_DEFAULT = OB_DEFAULT;
char *s_OB_MPDEFAULT = OB_MPDEFAULT;

char *s_OB_UNDEADHIT = OB_UNDEADHIT;
char *s_OB_IMPHIT = OB_IMPHIT;
char *s_OB_CACOHIT = OB_CACOHIT;
char *s_OB_DEMONHIT = OB_DEMONHIT;
char *s_OB_SPECTREHIT = OB_SPECTREHIT;
char *s_OB_BARONHIT = OB_BARONHIT;
char *s_OB_KNIGHTHIT = OB_KNIGHTHIT;

char *s_OB_ZOMBIE = OB_ZOMBIE;
char *s_OB_SHOTGUY = OB_SHOTGUY;
char *s_OB_VILE = OB_VILE;
char *s_OB_UNDEAD = OB_UNDEAD;
char *s_OB_FATSO = OB_FATSO;
char *s_OB_CHAINGUY = OB_CHAINGUY;
char *s_OB_SKULL = OB_SKULL;
char *s_OB_IMP = OB_IMP;
char *s_OB_CACO = OB_CACO;
char *s_OB_BARON = OB_BARON;
char *s_OB_KNIGHT = OB_KNIGHT;
char *s_OB_SPIDER = OB_SPIDER;
char *s_OB_BABY = OB_BABY;
char *s_OB_CYBORG = OB_CYBORG;
char *s_OB_WOLFSS = OB_WOLFSS;

char *s_OB_MPFIST = OB_MPFIST;
char *s_OB_MPCHAINSAW = OB_MPCHAINSAW;
char *s_OB_MPPISTOL = OB_MPPISTOL;
char *s_OB_MPSHOTGUN = OB_MPSHOTGUN;
char *s_OB_MPSSHOTGUN = OB_MPSSHOTGUN;
char *s_OB_MPCHAINGUN = OB_MPCHAINGUN;
char *s_OB_MPROCKET = OB_MPROCKET;
char *s_OB_MPPLASMARIFLE = OB_MPPLASMARIFLE;
char *s_OB_MPBFG_BOOM = OB_MPBFG_BOOM;
char *s_OB_MPTELEFRAG = OB_MPTELEFRAG;

// Ty 05/03/98 - externalized
char *savegamename;

// end d_deh.h variable declarations
// ====================================================================

// Do this for a lookup--the pointer (loaded above) is cross-referenced
// to a string key that is the same as the define above.  We will use
// strdups to set these new values that we read from the file, orphaning
// the original value set above.

typedef struct
{
    char **ppstr; // doubly indirect pointer to string
    char *lookup; // pointer to lookup string name
    const char *orig;
} deh_strs;

static deh_strs deh_strlookup[] = {
    {&s_D_DEVSTR,           "D_DEVSTR"          },
    {&s_D_CDROM,            "D_CDROM"           },
    {&s_PRESSKEY,           "PRESSKEY"          },
    {&s_PRESSYN,            "PRESSYN"           },
    {&s_QUITMSG,            "QUITMSG"           },
    {&s_QUITMSG1,           "QUITMSG1"          },
    {&s_QUITMSG2,           "QUITMSG2"          },
    {&s_QUITMSG3,           "QUITMSG3"          },
    {&s_QUITMSG4,           "QUITMSG4"          },
    {&s_QUITMSG5,           "QUITMSG5"          },
    {&s_QUITMSG6,           "QUITMSG6"          },
    {&s_QUITMSG7,           "QUITMSG7"          },
    {&s_QUITMSG8,           "QUITMSG8"          },
    {&s_QUITMSG9,           "QUITMSG9"          },
    {&s_QUITMSG10,          "QUITMSG10"         },
    {&s_QUITMSG11,          "QUITMSG11"         },
    {&s_QUITMSG12,          "QUITMSG12"         },
    {&s_QUITMSG13,          "QUITMSG13"         },
    {&s_QUITMSG14,          "QUITMSG14"         },
    {&s_LOADNET,            "LOADNET"           },
    {&s_QLOADNET,           "QLOADNET"          },
    {&s_QSAVESPOT,          "QSAVESPOT"         },
    {&s_SAVEDEAD,           "SAVEDEAD"          },
    {&s_QSPROMPT,           "QSPROMPT"          },
    {&s_QLPROMPT,           "QLPROMPT"          },
    {&s_NEWGAME,            "NEWGAME"           },
    {&s_NIGHTMARE,          "NIGHTMARE"         },
    {&s_SWSTRING,           "SWSTRING"          },
    {&s_MSGOFF,             "MSGOFF"            },
    {&s_MSGON,              "MSGON"             },
    {&s_NETEND,             "NETEND"            },
    {&s_ENDGAME,            "ENDGAME"           },
    {&s_DOSY,               "DOSY"              },
    {&s_DETAILHI,           "DETAILHI"          },
    {&s_DETAILLO,           "DETAILLO"          },
    {&s_GAMMALVL0,          "GAMMALVL0"         },
    {&s_GAMMALVL1,          "GAMMALVL1"         },
    {&s_GAMMALVL2,          "GAMMALVL2"         },
    {&s_GAMMALVL3,          "GAMMALVL3"         },
    {&s_GAMMALVL4,          "GAMMALVL4"         },
    {&s_EMPTYSTRING,        "EMPTYSTRING"       },
    {&s_GOTARMOR,           "GOTARMOR"          },
    {&s_GOTMEGA,            "GOTMEGA"           },
    {&s_GOTHTHBONUS,        "GOTHTHBONUS"       },
    {&s_GOTARMBONUS,        "GOTARMBONUS"       },
    {&s_GOTSTIM,            "GOTSTIM"           },
    {&s_GOTMEDINEED,        "GOTMEDINEED"       },
    {&s_GOTMEDIKIT,         "GOTMEDIKIT"        },
    {&s_GOTSUPER,           "GOTSUPER"          },
    {&s_GOTBLUECARD,        "GOTBLUECARD"       },
    {&s_GOTYELWCARD,        "GOTYELWCARD"       },
    {&s_GOTREDCARD,         "GOTREDCARD"        },
    {&s_GOTBLUESKUL,        "GOTBLUESKUL"       },
    {&s_GOTYELWSKUL,        "GOTYELWSKUL"       },
    {&s_GOTREDSKULL,        "GOTREDSKULL"       },
    {&s_GOTINVUL,           "GOTINVUL"          },
    {&s_GOTBERSERK,         "GOTBERSERK"        },
    {&s_GOTINVIS,           "GOTINVIS"          },
    {&s_GOTSUIT,            "GOTSUIT"           },
    {&s_GOTMAP,             "GOTMAP"            },
    {&s_GOTVISOR,           "GOTVISOR"          },
    {&s_GOTMSPHERE,         "GOTMSPHERE"        },
    {&s_GOTCLIP,            "GOTCLIP"           },
    {&s_GOTCLIPBOX,         "GOTCLIPBOX"        },
    {&s_GOTROCKET,          "GOTROCKET"         },
    {&s_GOTROCKBOX,         "GOTROCKBOX"        },
    {&s_GOTCELL,            "GOTCELL"           },
    {&s_GOTCELLBOX,         "GOTCELLBOX"        },
    {&s_GOTSHELLS,          "GOTSHELLS"         },
    {&s_GOTSHELLBOX,        "GOTSHELLBOX"       },
    {&s_GOTBACKPACK,        "GOTBACKPACK"       },
    {&s_GOTBFG9000,         "GOTBFG9000"        },
    {&s_GOTCHAINGUN,        "GOTCHAINGUN"       },
    {&s_GOTCHAINSAW,        "GOTCHAINSAW"       },
    {&s_GOTLAUNCHER,        "GOTLAUNCHER"       },
    {&s_GOTPLASMA,          "GOTPLASMA"         },
    {&s_GOTSHOTGUN,         "GOTSHOTGUN"        },
    {&s_GOTSHOTGUN2,        "GOTSHOTGUN2"       },
    {&s_BETA_BONUS3,        "BETA_BONUS3"       },
    {&s_BETA_BONUS4,        "BETA_BONUS4"       },
    {&s_PD_BLUEO,           "PD_BLUEO"          },
    {&s_PD_REDO,            "PD_REDO"           },
    {&s_PD_YELLOWO,         "PD_YELLOWO"        },
    {&s_PD_BLUEK,           "PD_BLUEK"          },
    {&s_PD_REDK,            "PD_REDK"           },
    {&s_PD_YELLOWK,         "PD_YELLOWK"        },
    {&s_PD_BLUEC,           "PD_BLUEC"          },
    {&s_PD_REDC,            "PD_REDC"           },
    {&s_PD_YELLOWC,         "PD_YELLOWC"        },
    {&s_PD_BLUES,           "PD_BLUES"          },
    {&s_PD_REDS,            "PD_REDS"           },
    {&s_PD_YELLOWS,         "PD_YELLOWS"        },
    {&s_PD_ANY,             "PD_ANY"            },
    {&s_PD_ALL3,            "PD_ALL3"           },
    {&s_PD_ALL6,            "PD_ALL6"           },
    {&s_GGSAVED,            "GGSAVED"           },
    {&s_HUSTR_MSGU,         "HUSTR_MSGU"        },
    {&s_HUSTR_E1M1,         "HUSTR_E1M1"        },
    {&s_HUSTR_E1M2,         "HUSTR_E1M2"        },
    {&s_HUSTR_E1M3,         "HUSTR_E1M3"        },
    {&s_HUSTR_E1M4,         "HUSTR_E1M4"        },
    {&s_HUSTR_E1M5,         "HUSTR_E1M5"        },
    {&s_HUSTR_E1M6,         "HUSTR_E1M6"        },
    {&s_HUSTR_E1M7,         "HUSTR_E1M7"        },
    {&s_HUSTR_E1M8,         "HUSTR_E1M8"        },
    {&s_HUSTR_E1M9,         "HUSTR_E1M9"        },
    {&s_HUSTR_E2M1,         "HUSTR_E2M1"        },
    {&s_HUSTR_E2M2,         "HUSTR_E2M2"        },
    {&s_HUSTR_E2M3,         "HUSTR_E2M3"        },
    {&s_HUSTR_E2M4,         "HUSTR_E2M4"        },
    {&s_HUSTR_E2M5,         "HUSTR_E2M5"        },
    {&s_HUSTR_E2M6,         "HUSTR_E2M6"        },
    {&s_HUSTR_E2M7,         "HUSTR_E2M7"        },
    {&s_HUSTR_E2M8,         "HUSTR_E2M8"        },
    {&s_HUSTR_E2M9,         "HUSTR_E2M9"        },
    {&s_HUSTR_E3M1,         "HUSTR_E3M1"        },
    {&s_HUSTR_E3M2,         "HUSTR_E3M2"        },
    {&s_HUSTR_E3M3,         "HUSTR_E3M3"        },
    {&s_HUSTR_E3M4,         "HUSTR_E3M4"        },
    {&s_HUSTR_E3M5,         "HUSTR_E3M5"        },
    {&s_HUSTR_E3M6,         "HUSTR_E3M6"        },
    {&s_HUSTR_E3M7,         "HUSTR_E3M7"        },
    {&s_HUSTR_E3M8,         "HUSTR_E3M8"        },
    {&s_HUSTR_E3M9,         "HUSTR_E3M9"        },
    {&s_HUSTR_E4M1,         "HUSTR_E4M1"        },
    {&s_HUSTR_E4M2,         "HUSTR_E4M2"        },
    {&s_HUSTR_E4M3,         "HUSTR_E4M3"        },
    {&s_HUSTR_E4M4,         "HUSTR_E4M4"        },
    {&s_HUSTR_E4M5,         "HUSTR_E4M5"        },
    {&s_HUSTR_E4M6,         "HUSTR_E4M6"        },
    {&s_HUSTR_E4M7,         "HUSTR_E4M7"        },
    {&s_HUSTR_E4M8,         "HUSTR_E4M8"        },
    {&s_HUSTR_E4M9,         "HUSTR_E4M9"        },
    {&s_HUSTR_1,            "HUSTR_1"           },
    {&s_HUSTR_2,            "HUSTR_2"           },
    {&s_HUSTR_3,            "HUSTR_3"           },
    {&s_HUSTR_4,            "HUSTR_4"           },
    {&s_HUSTR_5,            "HUSTR_5"           },
    {&s_HUSTR_6,            "HUSTR_6"           },
    {&s_HUSTR_7,            "HUSTR_7"           },
    {&s_HUSTR_8,            "HUSTR_8"           },
    {&s_HUSTR_9,            "HUSTR_9"           },
    {&s_HUSTR_10,           "HUSTR_10"          },
    {&s_HUSTR_11,           "HUSTR_11"          },
    {&s_HUSTR_12,           "HUSTR_12"          },
    {&s_HUSTR_13,           "HUSTR_13"          },
    {&s_HUSTR_14,           "HUSTR_14"          },
    {&s_HUSTR_15,           "HUSTR_15"          },
    {&s_HUSTR_16,           "HUSTR_16"          },
    {&s_HUSTR_17,           "HUSTR_17"          },
    {&s_HUSTR_18,           "HUSTR_18"          },
    {&s_HUSTR_19,           "HUSTR_19"          },
    {&s_HUSTR_20,           "HUSTR_20"          },
    {&s_HUSTR_21,           "HUSTR_21"          },
    {&s_HUSTR_22,           "HUSTR_22"          },
    {&s_HUSTR_23,           "HUSTR_23"          },
    {&s_HUSTR_24,           "HUSTR_24"          },
    {&s_HUSTR_25,           "HUSTR_25"          },
    {&s_HUSTR_26,           "HUSTR_26"          },
    {&s_HUSTR_27,           "HUSTR_27"          },
    {&s_HUSTR_28,           "HUSTR_28"          },
    {&s_HUSTR_29,           "HUSTR_29"          },
    {&s_HUSTR_30,           "HUSTR_30"          },
    {&s_HUSTR_31,           "HUSTR_31"          },
    {&s_HUSTR_32,           "HUSTR_32"          },
    {&s_PHUSTR_1,           "PHUSTR_1"          },
    {&s_PHUSTR_2,           "PHUSTR_2"          },
    {&s_PHUSTR_3,           "PHUSTR_3"          },
    {&s_PHUSTR_4,           "PHUSTR_4"          },
    {&s_PHUSTR_5,           "PHUSTR_5"          },
    {&s_PHUSTR_6,           "PHUSTR_6"          },
    {&s_PHUSTR_7,           "PHUSTR_7"          },
    {&s_PHUSTR_8,           "PHUSTR_8"          },
    {&s_PHUSTR_9,           "PHUSTR_9"          },
    {&s_PHUSTR_10,          "PHUSTR_10"         },
    {&s_PHUSTR_11,          "PHUSTR_11"         },
    {&s_PHUSTR_12,          "PHUSTR_12"         },
    {&s_PHUSTR_13,          "PHUSTR_13"         },
    {&s_PHUSTR_14,          "PHUSTR_14"         },
    {&s_PHUSTR_15,          "PHUSTR_15"         },
    {&s_PHUSTR_16,          "PHUSTR_16"         },
    {&s_PHUSTR_17,          "PHUSTR_17"         },
    {&s_PHUSTR_18,          "PHUSTR_18"         },
    {&s_PHUSTR_19,          "PHUSTR_19"         },
    {&s_PHUSTR_20,          "PHUSTR_20"         },
    {&s_PHUSTR_21,          "PHUSTR_21"         },
    {&s_PHUSTR_22,          "PHUSTR_22"         },
    {&s_PHUSTR_23,          "PHUSTR_23"         },
    {&s_PHUSTR_24,          "PHUSTR_24"         },
    {&s_PHUSTR_25,          "PHUSTR_25"         },
    {&s_PHUSTR_26,          "PHUSTR_26"         },
    {&s_PHUSTR_27,          "PHUSTR_27"         },
    {&s_PHUSTR_28,          "PHUSTR_28"         },
    {&s_PHUSTR_29,          "PHUSTR_29"         },
    {&s_PHUSTR_30,          "PHUSTR_30"         },
    {&s_PHUSTR_31,          "PHUSTR_31"         },
    {&s_PHUSTR_32,          "PHUSTR_32"         },
    {&s_THUSTR_1,           "THUSTR_1"          },
    {&s_THUSTR_2,           "THUSTR_2"          },
    {&s_THUSTR_3,           "THUSTR_3"          },
    {&s_THUSTR_4,           "THUSTR_4"          },
    {&s_THUSTR_5,           "THUSTR_5"          },
    {&s_THUSTR_6,           "THUSTR_6"          },
    {&s_THUSTR_7,           "THUSTR_7"          },
    {&s_THUSTR_8,           "THUSTR_8"          },
    {&s_THUSTR_9,           "THUSTR_9"          },
    {&s_THUSTR_10,          "THUSTR_10"         },
    {&s_THUSTR_11,          "THUSTR_11"         },
    {&s_THUSTR_12,          "THUSTR_12"         },
    {&s_THUSTR_13,          "THUSTR_13"         },
    {&s_THUSTR_14,          "THUSTR_14"         },
    {&s_THUSTR_15,          "THUSTR_15"         },
    {&s_THUSTR_16,          "THUSTR_16"         },
    {&s_THUSTR_17,          "THUSTR_17"         },
    {&s_THUSTR_18,          "THUSTR_18"         },
    {&s_THUSTR_19,          "THUSTR_19"         },
    {&s_THUSTR_20,          "THUSTR_20"         },
    {&s_THUSTR_21,          "THUSTR_21"         },
    {&s_THUSTR_22,          "THUSTR_22"         },
    {&s_THUSTR_23,          "THUSTR_23"         },
    {&s_THUSTR_24,          "THUSTR_24"         },
    {&s_THUSTR_25,          "THUSTR_25"         },
    {&s_THUSTR_26,          "THUSTR_26"         },
    {&s_THUSTR_27,          "THUSTR_27"         },
    {&s_THUSTR_28,          "THUSTR_28"         },
    {&s_THUSTR_29,          "THUSTR_29"         },
    {&s_THUSTR_30,          "THUSTR_30"         },
    {&s_THUSTR_31,          "THUSTR_31"         },
    {&s_THUSTR_32,          "THUSTR_32"         },
    {&s_HUSTR_CHATMACRO1,   "HUSTR_CHATMACRO1"  },
    {&s_HUSTR_CHATMACRO2,   "HUSTR_CHATMACRO2"  },
    {&s_HUSTR_CHATMACRO3,   "HUSTR_CHATMACRO3"  },
    {&s_HUSTR_CHATMACRO4,   "HUSTR_CHATMACRO4"  },
    {&s_HUSTR_CHATMACRO5,   "HUSTR_CHATMACRO5"  },
    {&s_HUSTR_CHATMACRO6,   "HUSTR_CHATMACRO6"  },
    {&s_HUSTR_CHATMACRO7,   "HUSTR_CHATMACRO7"  },
    {&s_HUSTR_CHATMACRO8,   "HUSTR_CHATMACRO8"  },
    {&s_HUSTR_CHATMACRO9,   "HUSTR_CHATMACRO9"  },
    {&s_HUSTR_CHATMACRO0,   "HUSTR_CHATMACRO0"  },
    {&s_HUSTR_TALKTOSELF1,  "HUSTR_TALKTOSELF1" },
    {&s_HUSTR_TALKTOSELF2,  "HUSTR_TALKTOSELF2" },
    {&s_HUSTR_TALKTOSELF3,  "HUSTR_TALKTOSELF3" },
    {&s_HUSTR_TALKTOSELF4,  "HUSTR_TALKTOSELF4" },
    {&s_HUSTR_TALKTOSELF5,  "HUSTR_TALKTOSELF5" },
    {&s_HUSTR_MESSAGESENT,  "HUSTR_MESSAGESENT" },
    {&s_HUSTR_SECRETFOUND,  "HUSTR_SECRETFOUND" },
    {&s_HUSTR_PLRGREEN,     "HUSTR_PLRGREEN"    },
    {&s_HUSTR_PLRINDIGO,    "HUSTR_PLRINDIGO"   },
    {&s_HUSTR_PLRBROWN,     "HUSTR_PLRBROWN"    },
    {&s_HUSTR_PLRRED,       "HUSTR_PLRRED"      },
    //{c_HUSTR_KEYGREEN,"HUSTR_KEYGREEN"},
    //{c_HUSTR_KEYINDIGO,"HUSTR_KEYINDIGO"},
    //{c_HUSTR_KEYBROWN,"HUSTR_KEYBROWN"},
    //{c_HUSTR_KEYRED,"HUSTR_KEYRED"},
    {&s_AMSTR_FOLLOWON,     "AMSTR_FOLLOWON"    },
    {&s_AMSTR_FOLLOWOFF,    "AMSTR_FOLLOWOFF"   },
    {&s_AMSTR_GRIDON,       "AMSTR_GRIDON"      },
    {&s_AMSTR_GRIDOFF,      "AMSTR_GRIDOFF"     },
    {&s_AMSTR_MARKEDSPOT,   "AMSTR_MARKEDSPOT"  },
    {&s_AMSTR_MARKSCLEARED, "AMSTR_MARKSCLEARED"},
    {&s_AMSTR_OVERLAYON,    "AMSTR_OVERLAYON"   },
    {&s_AMSTR_OVERLAYOFF,   "AMSTR_FOLLOWOFF"   },
    {&s_AMSTR_ROTATEON,     "AMSTR_ROTATEON"    },
    {&s_AMSTR_ROTATEOFF,    "AMSTR_ROTATEOFF"   },
    {&s_STSTR_MUS,          "STSTR_MUS"         },
    {&s_STSTR_NOMUS,        "STSTR_NOMUS"       },
    {&s_STSTR_DQDON,        "STSTR_DQDON"       },
    {&s_STSTR_DQDOFF,       "STSTR_DQDOFF"      },
    {&s_STSTR_KFAADDED,     "STSTR_KFAADDED"    },
    {&s_STSTR_FAADDED,      "STSTR_FAADDED"     },
    {&s_STSTR_NCON,         "STSTR_NCON"        },
    {&s_STSTR_NCOFF,        "STSTR_NCOFF"       },
    {&s_STSTR_BEHOLD,       "STSTR_BEHOLD"      },
    {&s_STSTR_BEHOLDX,      "STSTR_BEHOLDX"     },
    {&s_STSTR_CHOPPERS,     "STSTR_CHOPPERS"    },
    {&s_STSTR_CLEV,         "STSTR_CLEV"        },
    {&s_STSTR_COMPON,       "STSTR_COMPON"      },
    {&s_STSTR_COMPOFF,      "STSTR_COMPOFF"     },
    {&s_E1TEXT,             "E1TEXT"            },
    {&s_E2TEXT,             "E2TEXT"            },
    {&s_E3TEXT,             "E3TEXT"            },
    {&s_E4TEXT,             "E4TEXT"            },
    {&s_C1TEXT,             "C1TEXT"            },
    {&s_C2TEXT,             "C2TEXT"            },
    {&s_C3TEXT,             "C3TEXT"            },
    {&s_C4TEXT,             "C4TEXT"            },
    {&s_C5TEXT,             "C5TEXT"            },
    {&s_C6TEXT,             "C6TEXT"            },
    {&s_P1TEXT,             "P1TEXT"            },
    {&s_P2TEXT,             "P2TEXT"            },
    {&s_P3TEXT,             "P3TEXT"            },
    {&s_P4TEXT,             "P4TEXT"            },
    {&s_P5TEXT,             "P5TEXT"            },
    {&s_P6TEXT,             "P6TEXT"            },
    {&s_T1TEXT,             "T1TEXT"            },
    {&s_T2TEXT,             "T2TEXT"            },
    {&s_T3TEXT,             "T3TEXT"            },
    {&s_T4TEXT,             "T4TEXT"            },
    {&s_T5TEXT,             "T5TEXT"            },
    {&s_T6TEXT,             "T6TEXT"            },
    {&s_CC_ZOMBIE,          "CC_ZOMBIE"         },
    {&s_CC_SHOTGUN,         "CC_SHOTGUN"        },
    {&s_CC_HEAVY,           "CC_HEAVY"          },
    {&s_CC_IMP,             "CC_IMP"            },
    {&s_CC_DEMON,           "CC_DEMON"          },
    {&s_CC_LOST,            "CC_LOST"           },
    {&s_CC_CACO,            "CC_CACO"           },
    {&s_CC_HELL,            "CC_HELL"           },
    {&s_CC_BARON,           "CC_BARON"          },
    {&s_CC_ARACH,           "CC_ARACH"          },
    {&s_CC_PAIN,            "CC_PAIN"           },
    {&s_CC_REVEN,           "CC_REVEN"          },
    {&s_CC_MANCU,           "CC_MANCU"          },
    {&s_CC_ARCH,            "CC_ARCH"           },
    {&s_CC_SPIDER,          "CC_SPIDER"         },
    {&s_CC_CYBER,           "CC_CYBER"          },
    {&s_CC_HERO,            "CC_HERO"           },
    {&bgflatE1,             "BGFLATE1"          },
    {&bgflatE2,             "BGFLATE2"          },
    {&bgflatE3,             "BGFLATE3"          },
    {&bgflatE4,             "BGFLATE4"          },
    {&bgflat06,             "BGFLAT06"          },
    {&bgflat11,             "BGFLAT11"          },
    {&bgflat20,             "BGFLAT20"          },
    {&bgflat30,             "BGFLAT30"          },
    {&bgflat15,             "BGFLAT15"          },
    {&bgflat31,             "BGFLAT31"          },
    {&bgcastcall,           "BGCASTCALL"        },
    // Ty 04/08/98 - added 5 general purpose startup announcement
    // strings for hacker use.  See m_menu.c
    {&startup1,             "STARTUP1"          },
    {&startup2,             "STARTUP2"          },
    {&startup3,             "STARTUP3"          },
    {&startup4,             "STARTUP4"          },
    {&startup5,             "STARTUP5"          },
    {&savegamename,         "SAVEGAMENAME"      }, // Ty 05/03/98

    // [FG] obituaries

    {&s_OB_CRUSH,           "OB_CRUSH"          },
    {&s_OB_SLIME,           "OB_SLIME"          },
    {&s_OB_LAVA,            "OB_LAVA"           },
    {&s_OB_KILLEDSELF,      "OB_KILLEDSELF"     },
    {&s_OB_VOODOO,          "OB_VOODOO"         },
    {&s_OB_MONTELEFRAG,     "OB_MONTELEFRAG"    },
    {&s_OB_DEFAULT,         "OB_DEFAULT"        },
    {&s_OB_MPDEFAULT,       "OB_MPDEFAULT"      },
    {&s_OB_UNDEADHIT,       "OB_UNDEADHIT"      },
    {&s_OB_IMPHIT,          "OB_IMPHIT"         },
    {&s_OB_CACOHIT,         "OB_CACOHIT"        },
    {&s_OB_DEMONHIT,        "OB_DEMONHIT"       },
    {&s_OB_SPECTREHIT,      "OB_SPECTREHIT"     },
    {&s_OB_BARONHIT,        "OB_BARONHIT"       },
    {&s_OB_KNIGHTHIT,       "OB_KNIGHTHIT"      },
    {&s_OB_ZOMBIE,          "OB_ZOMBIE"         },
    {&s_OB_SHOTGUY,         "OB_SHOTGUY"        },
    {&s_OB_VILE,            "OB_VILE"           },
    {&s_OB_UNDEAD,          "OB_UNDEAD"         },
    {&s_OB_FATSO,           "OB_FATSO"          },
    {&s_OB_CHAINGUY,        "OB_CHAINGUY"       },
    {&s_OB_SKULL,           "OB_SKULL"          },
    {&s_OB_IMP,             "OB_IMP"            },
    {&s_OB_CACO,            "OB_CACO"           },
    {&s_OB_BARON,           "OB_BARON"          },
    {&s_OB_KNIGHT,          "OB_KNIGHT"         },
    {&s_OB_SPIDER,          "OB_SPIDER"         },
    {&s_OB_BABY,            "OB_BABY"           },
    {&s_OB_CYBORG,          "OB_CYBORG"         },
    {&s_OB_WOLFSS,          "OB_WOLFSS"         },
    {&s_OB_MPFIST,          "OB_MPFIST"         },
    {&s_OB_MPCHAINSAW,      "OB_MPCHAINSAW"     },
    {&s_OB_MPPISTOL,        "OB_MPPISTOL"       },
    {&s_OB_MPSHOTGUN,       "OB_MPSHOTGUN"      },
    {&s_OB_MPSSHOTGUN,      "OB_MPSSHOTGUN"     },
    {&s_OB_MPCHAINGUN,      "OB_MPCHAINGUN"     },
    {&s_OB_MPROCKET,        "OB_MPROCKET"       },
    {&s_OB_MPPLASMARIFLE,   "OB_MPPLASMARIFLE"  },
    {&s_OB_MPBFG_BOOM,      "OB_MPBFG_BOOM"     },
    {&s_OB_MPTELEFRAG,      "OB_MPTELEFRAG"     },
};

static char *deh_newlevel = "NEWLEVEL";

char **mapnames[] = // DOOM shareware/registered/retail (Ultimate) names.
{
    &s_HUSTR_E1M1, &s_HUSTR_E1M2, &s_HUSTR_E1M3, &s_HUSTR_E1M4, &s_HUSTR_E1M5,
    &s_HUSTR_E1M6, &s_HUSTR_E1M7, &s_HUSTR_E1M8, &s_HUSTR_E1M9,

    &s_HUSTR_E2M1, &s_HUSTR_E2M2, &s_HUSTR_E2M3, &s_HUSTR_E2M4, &s_HUSTR_E2M5,
    &s_HUSTR_E2M6, &s_HUSTR_E2M7, &s_HUSTR_E2M8, &s_HUSTR_E2M9,

    &s_HUSTR_E3M1, &s_HUSTR_E3M2, &s_HUSTR_E3M3, &s_HUSTR_E3M4, &s_HUSTR_E3M5,
    &s_HUSTR_E3M6, &s_HUSTR_E3M7, &s_HUSTR_E3M8, &s_HUSTR_E3M9,

    &s_HUSTR_E4M1, &s_HUSTR_E4M2, &s_HUSTR_E4M3, &s_HUSTR_E4M4, &s_HUSTR_E4M5,
    &s_HUSTR_E4M6, &s_HUSTR_E4M7, &s_HUSTR_E4M8, &s_HUSTR_E4M9,

    &deh_newlevel, // spares?  Unused.
    &deh_newlevel, &deh_newlevel, &deh_newlevel, &deh_newlevel, &deh_newlevel,
    &deh_newlevel, &deh_newlevel, &deh_newlevel
};

char **mapnames2[] = // DOOM 2 map names.
{
    &s_HUSTR_1,  &s_HUSTR_2,  &s_HUSTR_3,  &s_HUSTR_4,  &s_HUSTR_5,
    &s_HUSTR_6,  &s_HUSTR_7,  &s_HUSTR_8,  &s_HUSTR_9,  &s_HUSTR_10,
    &s_HUSTR_11, &s_HUSTR_12, &s_HUSTR_13, &s_HUSTR_14, &s_HUSTR_15,
    &s_HUSTR_16, &s_HUSTR_17, &s_HUSTR_18, &s_HUSTR_19, &s_HUSTR_20,
    &s_HUSTR_21, &s_HUSTR_22, &s_HUSTR_23, &s_HUSTR_24, &s_HUSTR_25,
    &s_HUSTR_26, &s_HUSTR_27, &s_HUSTR_28, &s_HUSTR_29, &s_HUSTR_30,
    &s_HUSTR_31, &s_HUSTR_32,
};

char **mapnamesp[] = // Plutonia WAD map names.
{
    &s_PHUSTR_1,  &s_PHUSTR_2,  &s_PHUSTR_3,  &s_PHUSTR_4,  &s_PHUSTR_5,
    &s_PHUSTR_6,  &s_PHUSTR_7,  &s_PHUSTR_8,  &s_PHUSTR_9,  &s_PHUSTR_10,
    &s_PHUSTR_11, &s_PHUSTR_12, &s_PHUSTR_13, &s_PHUSTR_14, &s_PHUSTR_15,
    &s_PHUSTR_16, &s_PHUSTR_17, &s_PHUSTR_18, &s_PHUSTR_19, &s_PHUSTR_20,
    &s_PHUSTR_21, &s_PHUSTR_22, &s_PHUSTR_23, &s_PHUSTR_24, &s_PHUSTR_25,
    &s_PHUSTR_26, &s_PHUSTR_27, &s_PHUSTR_28, &s_PHUSTR_29, &s_PHUSTR_30,
    &s_PHUSTR_31, &s_PHUSTR_32,
};

char **mapnamest[] = // TNT WAD map names.
{
    &s_THUSTR_1,  &s_THUSTR_2,  &s_THUSTR_3,  &s_THUSTR_4,  &s_THUSTR_5,
    &s_THUSTR_6,  &s_THUSTR_7,  &s_THUSTR_8,  &s_THUSTR_9,  &s_THUSTR_10,
    &s_THUSTR_11, &s_THUSTR_12, &s_THUSTR_13, &s_THUSTR_14, &s_THUSTR_15,
    &s_THUSTR_16, &s_THUSTR_17, &s_THUSTR_18, &s_THUSTR_19, &s_THUSTR_20,
    &s_THUSTR_21, &s_THUSTR_22, &s_THUSTR_23, &s_THUSTR_24, &s_THUSTR_25,
    &s_THUSTR_26, &s_THUSTR_27, &s_THUSTR_28, &s_THUSTR_29, &s_THUSTR_30,
    &s_THUSTR_31, &s_THUSTR_32,
};

// Strings for dehacked replacements of the startup banner
//
// These are from the original source: some of them are perhaps
// not used in any dehacked patches

static const char *const banners[] = {
    // doom2.wad
    "                         "
    "DOOM 2: Hell on Earth v%i.%i"
    "                           ",
    // doom2.wad v1.666
    "                         "
    "DOOM 2: Hell on Earth v%i.%i66"
    "                          ",
    // doom1.wad
    "                            "
    "DOOM Shareware Startup v%i.%i"
    "                           ",
    // doom.wad
    "                            "
    "DOOM Registered Startup v%i.%i"
    "                           ",
    // Registered DOOM uses this
    "                          "
    "DOOM System Startup v%i.%i"
    "                          ",
    // Doom v1.666
    "                          "
    "DOOM System Startup v%i.%i66"
    "                          "
    // doom.wad (Ultimate DOOM)
    "                         "
    "The Ultimate DOOM Startup v%i.%i"
    "                        ",
    // tnt.wad
    "                     "
    "DOOM 2: TNT - Evilution v%i.%i"
    "                           ",
    // plutonia.wad
    "                   "
    "DOOM 2: Plutonia Experiment v%i.%i"
    "                           ",
};

// Function prototypes
static void lfstrip(char *);     // strip the \r and/or \n off of a line
static void rstrip(char *);      // strip trailing whitespace
static char *ptr_lstrip(char *); // point past leading whitespace
static boolean deh_GetData(char *, char *, long *, char **);
static boolean deh_procStringSub(char *, char *, char *);
static char *dehReformatStr(char *);

// Prototypes for block processing functions
// Pointers to these functions are used as the blocks are encountered.

static void deh_procThing(DEHFILE *fpin, char *line);
static void deh_procFrame(DEHFILE *fpin, char *line);
static void deh_procPointer(DEHFILE *fpin, char *line);
static void deh_procSounds(DEHFILE *fpin, char *line);
static void deh_procAmmo(DEHFILE *fpin, char *line);
static void deh_procWeapon(DEHFILE *fpin, char *line);
static void deh_procSprite(DEHFILE *fpin, char *line);
static void deh_procCheat(DEHFILE *fpin, char *line);
static void deh_procMisc(DEHFILE *fpin, char *line);
static void deh_procText(DEHFILE *fpin, char *line);
static void deh_procPars(DEHFILE *fpin, char *line);
static void deh_procStrings(DEHFILE *fpin, char *line);
static void deh_procError(DEHFILE *fpin, char *line);
static void deh_procBexCodePointers(DEHFILE *fpin, char *line);
// haleyjd: handlers to fully deprecate the DeHackEd text section
static void deh_procBexSprites(DEHFILE *fpin, char *line);
static void deh_procBexSounds(DEHFILE *fpin, char *line);

// Structure deh_block is used to hold the block names that can
// be encountered, and the routines to use to decipher them

typedef struct
{
    char *key;                                     // a mnemonic block code name
    void (*const fptr)(DEHFILE *fpin, char *line); // handler
} deh_block_t;

#define DEH_BUFFERMAX 1024 // input buffer area size, hardcoded for now
// killough 8/9/98: make DEH_BLOCKMAX self-adjusting
#define DEH_BLOCKMAX arrlen(deh_blocks)
#define DEH_MAXKEYLEN 32 // as much of any key as we'll look at

// Put all the block header values, and the function to be called when that
// one is encountered, in this array:
static const deh_block_t deh_blocks[] = {
    {"Thing",     deh_procThing          },
    {"Frame",     deh_procFrame          },
    {"Pointer",   deh_procPointer        },
    {"Sound",     deh_procSounds         }, // Ty 03/16/98 corrected from "Sounds"
    {"Ammo",      deh_procAmmo           },
    {"Weapon",    deh_procWeapon         },
    {"Sprite",    deh_procSprite         },
    {"Cheat",     deh_procCheat          },
    {"Misc",      deh_procMisc           },
    {"Text",      deh_procText           }, // --  end of standard "deh" entries,

    // begin BOOM Extensions (BEX)

    {"[STRINGS]", deh_procStrings        }, // new string changes
    {"[PARS]",    deh_procPars           }, // alternative block marker
    {"[CODEPTR]", deh_procBexCodePointers}, // bex codepointers by mnemonic
    {"[SPRITES]", deh_procBexSprites     }, // bex style sprites
    {"[SOUNDS]",  deh_procBexSounds      }, // bex style sounds
    {"",          deh_procError          }  // dummy to handle anything else
};

// flag to skip included deh-style text, used with INCLUDE NOTEXT directive
static boolean includenotext = false;

// MOBJINFO - Dehacked block name = "Thing"
// Usage: Thing nn (name)
// These are for mobjinfo_t types.  Each is an integer
// within the structure, so we can use index of the string in this
// array to offset by sizeof(int) into the mobjinfo_t array at [nn]
// * things are base zero but dehacked considers them to start at #1. ***

enum
{
    DEH_MOBJINFO_DOOMEDNUM,
    DEH_MOBJINFO_SPAWNSTATE,
    DEH_MOBJINFO_SPAWNHEALTH,
    DEH_MOBJINFO_SEESTATE,
    DEH_MOBJINFO_SEESOUND,
    DEH_MOBJINFO_REACTIONTIME,
    DEH_MOBJINFO_ATTACKSOUND,
    DEH_MOBJINFO_PAINSTATE,
    DEH_MOBJINFO_PAINCHANCE,
    DEH_MOBJINFO_PAINSOUND,
    DEH_MOBJINFO_MELEESTATE,
    DEH_MOBJINFO_MISSILESTATE,
    DEH_MOBJINFO_DEATHSTATE,
    DEH_MOBJINFO_XDEATHSTATE,
    DEH_MOBJINFO_DEATHSOUND,
    DEH_MOBJINFO_SPEED,
    DEH_MOBJINFO_RADIUS,
    DEH_MOBJINFO_HEIGHT,
    DEH_MOBJINFO_MASS,
    DEH_MOBJINFO_DAMAGE,
    DEH_MOBJINFO_ACTIVESOUND,
    DEH_MOBJINFO_FLAGS,
    DEH_MOBJINFO_RAISESTATE,

    // mbf21
    DEH_MOBJINFO_INFIGHTING_GROUP,
    DEH_MOBJINFO_PROJECTILE_GROUP,
    DEH_MOBJINFO_SPLASH_GROUP,
    DEH_MOBJINFO_FLAGS2,
    DEH_MOBJINFO_RIPSOUND,
    DEH_MOBJINFO_ALTSPEED,
    DEH_MOBJINFO_MELEERANGE,

    // [Woof!]
    DEH_MOBJINFO_BLOODCOLOR,
    DEH_MOBJINFO_FLAGS_EXTRA,

    // DEHEXTRA
    DEH_MOBJINFO_DROPPEDITEM,

    DEH_MOBJINFOMAX
};

static const char *deh_mobjinfo[] = {
    "ID #",               // .doomednum
    "Initial frame",      // .spawnstate
    "Hit points",         // .spawnhealth
    "First moving frame", // .seestate
    "Alert sound",        // .seesound
    "Reaction time",      // .reactiontime
    "Attack sound",       // .attacksound
    "Injury frame",       // .painstate
    "Pain chance",        // .painchance
    "Pain sound",         // .painsound
    "Close attack frame", // .meleestate
    "Far attack frame",   // .missilestate
    "Death frame",        // .deathstate
    "Exploding frame",    // .xdeathstate
    "Death sound",        // .deathsound
    "Speed",              // .speed
    "Width",              // .radius
    "Height",             // .height
    "Mass",               // .mass
    "Missile damage",     // .damage
    "Action sound",       // .activesound
    "Bits",               // .flags
    "Respawn frame",      // .raisestate

    // mbf21
    "Infighting group", // .infighting_group
    "Projectile group", // .projectile_group
    "Splash group",     // .splash_group
    "MBF21 Bits",       // .flags2
    "Rip sound",        // .ripsound
    "Fast speed",       // .altspeed
    "Melee range",      // .meleerange

    // [Woof!]
    "Blood color", // .bloodcolor
    "Woof Bits",   // .flags_extra

    // DEHEXTRA
    "Dropped item", // .droppeditem
};

// Strings that are used to indicate flags ("Bits" in mobjinfo)
// This is an array of bit masks that are related to p_mobj.h
// values, using the smae names without the MF_ in front.
// Ty 08/27/98 new code
//
// killough 10/98:
//
// Convert array to struct to allow multiple values, make array size variable

typedef struct
{
    const char *name;
    long value;
} deh_flag_t;

static const deh_flag_t deh_mobjflags[] = {
    {"SPECIAL",      0x00000001}, // call  P_Specialthing when touched
    {"SOLID",        0x00000002}, // block movement
    {"SHOOTABLE",    0x00000004}, // can be hit
    {"NOSECTOR",     0x00000008}, // invisible but touchable
    {"NOBLOCKMAP",   0x00000010}, // inert but displayable
    {"AMBUSH",       0x00000020}, // deaf monster
    {"JUSTHIT",      0x00000040}, // will try to attack right back
    {"JUSTATTACKED", 0x00000080}, // take at least 1 step before attacking
    {"SPAWNCEILING", 0x00000100}, // initially hang from ceiling
    {"NOGRAVITY",    0x00000200}, // don't apply gravity during play
    {"DROPOFF",      0x00000400}, // can jump from high places
    {"PICKUP",       0x00000800}, // will pick up items
    {"NOCLIP",       0x00001000}, // goes through walls
    {"SLIDE",        0x00002000}, // keep info about sliding along walls
    {"FLOAT",        0x00004000}, // allow movement to any height
    {"TELEPORT",     0x00008000}, // don't cross lines or look at heights
    {"MISSILE",      0x00010000}, // don't hit same species, explode on block
    {"DROPPED",      0x00020000}, // dropped, not spawned (like ammo clip)
    {"SHADOW",       0x00040000}, // use fuzzy draw like spectres
    {"NOBLOOD",      0x00080000}, // puffs instead of blood when shot
    {"CORPSE",       0x00100000}, // so it will slide down steps when dead
    {"INFLOAT",      0x00200000}, // float but not to target height
    {"COUNTKILL",    0x00400000}, // count toward the kills total
    {"COUNTITEM",    0x00800000}, // count toward the items total
    {"SKULLFLY",     0x01000000}, // special handling for flying skulls
    {"NOTDMATCH",    0x02000000}, // do not spawn in deathmatch

    // killough 10/98: TRANSLATION consists of 2 bits, not 1:

    {"TRANSLATION",  0x04000000}, // for Boom bug-compatibility
    {"TRANSLATION1", 0x04000000}, // use translation table for color (players)
    {"TRANSLATION2", 0x08000000}, // use translation table for color (players)
    {"UNUSED1",      0x08000000}, // unused bit # 1 -- For Boom bug-compatibility
    {"UNUSED2",      0x10000000}, // unused bit # 2 -- For Boom compatibility
    {"UNUSED3",      0x20000000}, // unused bit # 3 -- For Boom compatibility
    {"UNUSED4",      0x40000000}, // unused bit # 4 -- For Boom compatibility
    {"TOUCHY",       0x10000000}, // dies on contact with solid objects (MBF)
    {"BOUNCES",      0x20000000}, // bounces off floors, ceilings and maybe walls
    {"FRIEND",       0x40000000}, // a friend of the player(s) (MBF)
    {"TRANSLUCENT",  0x80000000}, // apply translucency to sprite (BOOM)
};

static const deh_flag_t deh_mobjflags_mbf21[] = {
    {"LOGRAV",         MF2_LOGRAV       }, // low gravity
    {"SHORTMRANGE",    MF2_SHORTMRANGE  }, // short missile range
    {"DMGIGNORED",     MF2_DMGIGNORED   }, // other things ignore its attacks
    {"NORADIUSDMG",    MF2_NORADIUSDMG  }, // doesn't take splash damage
    {"FORCERADIUSDMG", MF2_FORCERADIUSDMG }, // causes splash damage even if target immune
    {"HIGHERMPROB",    MF2_HIGHERMPROB  }, // higher missile attack probability
    {"RANGEHALF",      MF2_RANGEHALF    }, // use half distance for missile attack probability
    {"NOTHRESHOLD",    MF2_NOTHRESHOLD  }, // no targeting threshold
    {"LONGMELEE",      MF2_LONGMELEE    }, // long melee range
    {"BOSS",           MF2_BOSS         }, // full volume see / death sound + splash immunity
    {"MAP07BOSS1",     MF2_MAP07BOSS1   }, // Tag 666 "boss" on doom 2 map 7
    {"MAP07BOSS2",     MF2_MAP07BOSS2   }, // Tag 667 "boss" on doom 2 map 7
    {"E1M8BOSS",       MF2_E1M8BOSS     }, // E1M8 boss
    {"E2M8BOSS",       MF2_E2M8BOSS     }, // E2M8 boss
    {"E3M8BOSS",       MF2_E3M8BOSS     }, // E3M8 boss
    {"E4M6BOSS",       MF2_E4M6BOSS     }, // E4M6 boss
    {"E4M8BOSS",       MF2_E4M8BOSS     }, // E4M8 boss
    {"RIP",            MF2_RIP          }, // projectile rips through targets
    {"FULLVOLSOUNDS",  MF2_FULLVOLSOUNDS}, // full volume see / death sound
};

static const deh_flag_t deh_mobjflags_extra[] = {
    {"MIRROREDCORPSE", MFX_MIRROREDCORPSE} // [crispy] randomly flip corpse, blood and death animation sprites
};

static const deh_flag_t deh_weaponflags_mbf21[] = {
    {"NOTHRUST", WPF_NOTHRUST}, // doesn't thrust Mobj's
    {"SILENT", WPF_SILENT}, // weapon is silent
    {"NOAUTOFIRE", WPF_NOAUTOFIRE}, // weapon won't autofire in A_WeaponReady
    {"FLEEMELEE", WPF_FLEEMELEE}, // monsters consider it a melee weapon
    {"AUTOSWITCHFROM", WPF_AUTOSWITCHFROM}, // can be switched away from when ammo is picked up
    {"NOAUTOSWITCHTO", WPF_NOAUTOSWITCHTO}, // cannot be switched to when ammo is picked up
    {NULL}
};

// STATE - Dehacked block name = "Frame" and "Pointer"
// Usage: Frame nn
// Usage: Pointer nn (Frame nn)
// These are indexed separately, for lookup to the actual
// function pointers.  Here we'll take whatever Dehacked gives
// us and go from there.  The (Frame nn) after the pointer is the
// real place to put this value.  The "Pointer" value is an xref
// that Dehacked uses and is useless to us.
// * states are base zero and have a dummy #0 (TROO)

static const char *deh_state[] =
{
  "Sprite number",    // .sprite (spritenum_t) // an enum
  "Sprite subnumber", // .frame (long)
  "Duration",         // .tics (long)
  "Next frame",       // .nextstate (statenum_t)
  // This is set in a separate "Pointer" block from Dehacked
  "Codep Frame",      // pointer to first use of action (actionf_t)
  "Unknown 1",        // .misc1 (long)
  "Unknown 2",        // .misc2 (long)
  "Args1",            // .args[0] (long)
  "Args2",            // .args[1] (long)
  "Args3",            // .args[2] (long)
  "Args4",            // .args[3] (long)
  "Args5",            // .args[4] (long)
  "Args6",            // .args[5] (long)
  "Args7",            // .args[6] (long)
  "Args8",            // .args[7] (long)
  "MBF21 Bits",       // .flags
};

static const deh_flag_t deh_stateflags_mbf21[] = {
    {"SKILL5FAST", STATEF_SKILL5FAST}, // tics halve on nightmare skill
    {NULL}
};

// SFXINFO_STRUCT - Dehacked block name = "Sounds"
// Sound effects, typically not changed (redirected, and new sfx put
// into the pwad, but not changed here.  Can you tell that Gregdidn't
// know what they were for, mostly?  Can you tell that I don't either?
// Mostly I just put these into the same slots as they are in the struct.
// This may not be supported in our -deh option if it doesn't make sense by
// then.

// * sounds are base zero but have a dummy #0

static const char *deh_sfxinfo[] = {
    "Offset",     // pointer to a name string, changed in text
    "Zero/One",   // .singularity (int, one at a time flag)
    "Value",      // .priority
    "Zero 1",     // .link (sfxinfo_t*) referenced sound if linked
    "Zero 2",     // .pitch
    "Zero 3",     // .volume
    "Zero 4",     // .data (SAMPLE*) sound data
    "Neg. One 1", // .usefulness
    "Neg. One 2"  // .lumpnum
};

// MUSICINFO is not supported in Dehacked.  Ignored here.
// * music entries are base zero but have a dummy #0

// SPRITE - Dehacked block name = "Sprite"
// Usage = Sprite nn
// Sprite redirection by offset into the text area - unsupported by BOOM
// * sprites are base zero and dehacked uses it that way.

#if 0
static char *deh_sprite[] = {
    "Offset" // supposed to be the offset into the text section
};
#endif

// AMMO - Dehacked block name = "Ammo"
// usage = Ammo n (name)
// Ammo information for the few types of ammo

static const char *deh_ammo[] = {
    "Max ammo", // maxammo[]
    "Per ammo"  // clipammo[]
};

// WEAPONS - Dehacked block name = "Weapon"
// Usage: Weapon nn (name)
// Basically a list of frames and what kind of ammo (see above)it uses.

static const char *deh_weapon[] = {
    "Ammo type",      // .ammo
    "Deselect frame", // .upstate
    "Select frame",   // .downstate
    "Bobbing frame",  // .readystate
    "Shooting frame", // .atkstate
    "Firing frame",   // .flashstate
    // mbf21
    "Ammo per shot", // .ammopershot
    "MBF21 Bits",    // .flags
    // id24
    "Carousel icon", // .carouselicon
};

// CHEATS - Dehacked block name = "Cheat"
// Usage: Cheat 0
// Always uses a zero in the dehacked file, for consistency.  No meaning.
// These are just plain funky terms compared with id's
//
// killough 4/18/98: integrated into main cheat table now (see st_stuff.c)

// MISC - Dehacked block name = "Misc"
// Usage: Misc 0
// Always uses a zero in the dehacked file, for consistency.  No meaning.

static const char *deh_misc[] = {
    "Initial Health",    // initial_health
    "Initial Bullets",   // initial_bullets
    "Max Health",        // maxhealth
    "Max Armor",         // max_armor
    "Green Armor Class", // green_armor_class
    "Blue Armor Class",  // blue_armor_class
    "Max Soulsphere",    // max_soul
    "Soulsphere Health", // soul_health
    "Megasphere Health", // mega_health
    "God Mode Health",   // god_health
    "IDFA Armor",        // idfa_armor
    "IDFA Armor Class",  // idfa_armor_class
    "IDKFA Armor",       // idkfa_armor
    "IDKFA Armor Class", // idkfa_armor_class
    "BFG Cells/Shot",    // BFGCELLS
    "Monsters Infight"   // Unknown--not a specific number it seems, but
                         // the logic has to be here somewhere or
                         // it'd happen always
};

// TEXT - Dehacked block name = "Text"
// Usage: Text fromlen tolen
// Dehacked allows a bit of adjustment to the length (why?)

// BEX extension [CODEPTR]
// Usage: Start block, then each line is:
// FRAME nnn = PointerMnemonic

// External references to action functions scattered about the code

typedef struct
{
    actionf_t cptr; // actual pointer to the subroutine
    char *lookup;   // mnemonic lookup string to be specified in BEX
    // mbf21
    int argcount; // [XA] number of mbf21 args this action uses, if any
    long default_args[MAXSTATEARGS]; // default values for mbf21 args
} deh_bexptr_t;

static const deh_bexptr_t deh_bexptrs[] = {
    {{.p2 = A_Light0}, "A_Light0"},
    {{.p2 = A_WeaponReady}, "A_WeaponReady"},
    {{.p2 = A_Lower}, "A_Lower"},
    {{.p2 = A_Raise}, "A_Raise"},
    {{.p2 = A_Punch}, "A_Punch"},
    {{.p2 = A_ReFire}, "A_ReFire"},
    {{.p2 = A_FirePistol}, "A_FirePistol"},
    {{.p2 = A_Light1}, "A_Light1"},
    {{.p2 = A_FireShotgun}, "A_FireShotgun"},
    {{.p2 = A_Light2}, "A_Light2"},
    {{.p2 = A_FireShotgun2}, "A_FireShotgun2"},
    {{.p2 = A_CheckReload}, "A_CheckReload"},
    {{.p2 = A_OpenShotgun2}, "A_OpenShotgun2"},
    {{.p2 = A_LoadShotgun2}, "A_LoadShotgun2"},
    {{.p2 = A_CloseShotgun2}, "A_CloseShotgun2"},
    {{.p2 = A_FireCGun}, "A_FireCGun"},
    {{.p2 = A_GunFlash}, "A_GunFlash"},
    {{.p2 = A_FireMissile}, "A_FireMissile"},
    {{.p2 = A_Saw}, "A_Saw"},
    {{.p2 = A_FirePlasma}, "A_FirePlasma"},
    {{.p2 = A_BFGsound}, "A_BFGsound"},
    {{.p2 = A_FireBFG}, "A_FireBFG"},
    {{.pm = A_BFGSpray}, "A_BFGSpray"},
    {{.pm = A_Explode}, "A_Explode"},
    {{.pm = A_Pain}, "A_Pain"},
    {{.pm = A_PlayerScream}, "A_PlayerScream"},
    {{.pm = A_Fall}, "A_Fall"},
    {{.pm = A_XScream}, "A_XScream"},
    {{.pm = A_Look}, "A_Look"},
    {{.pm = A_Chase}, "A_Chase"},
    {{.pm = A_FaceTarget}, "A_FaceTarget"},
    {{.pm = A_PosAttack}, "A_PosAttack"},
    {{.pm = A_Scream}, "A_Scream"},
    {{.pm = A_SPosAttack}, "A_SPosAttack"},
    {{.pm = A_VileChase}, "A_VileChase"},
    {{.pm = A_VileStart}, "A_VileStart"},
    {{.pm = A_VileTarget}, "A_VileTarget"},
    {{.pm = A_VileAttack}, "A_VileAttack"},
    {{.pm = A_StartFire}, "A_StartFire"},
    {{.pm = A_Fire}, "A_Fire"},
    {{.pm = A_FireCrackle}, "A_FireCrackle"},
    {{.pm = A_Tracer}, "A_Tracer"},
    {{.pm = A_SkelWhoosh}, "A_SkelWhoosh"},
    {{.pm = A_SkelFist}, "A_SkelFist"},
    {{.pm = A_SkelMissile}, "A_SkelMissile"},
    {{.pm = A_FatRaise}, "A_FatRaise"},
    {{.pm = A_FatAttack1}, "A_FatAttack1"},
    {{.pm = A_FatAttack2}, "A_FatAttack2"},
    {{.pm = A_FatAttack3}, "A_FatAttack3"},
    {{.pm = A_BossDeath}, "A_BossDeath"},
    {{.pm = A_CPosAttack}, "A_CPosAttack"},
    {{.pm = A_CPosRefire}, "A_CPosRefire"},
    {{.pm = A_TroopAttack}, "A_TroopAttack"},
    {{.pm = A_SargAttack}, "A_SargAttack"},
    {{.pm = A_HeadAttack}, "A_HeadAttack"},
    {{.pm = A_BruisAttack}, "A_BruisAttack"},
    {{.pm = A_SkullAttack}, "A_SkullAttack"},
    {{.pm = A_Metal}, "A_Metal"},
    {{.pm = A_SpidRefire}, "A_SpidRefire"},
    {{.pm = A_BabyMetal}, "A_BabyMetal"},
    {{.pm = A_BspiAttack}, "A_BspiAttack"},
    {{.pm = A_Hoof}, "A_Hoof"},
    {{.pm = A_CyberAttack}, "A_CyberAttack"},
    {{.pm = A_PainAttack}, "A_PainAttack"},
    {{.pm = A_PainDie}, "A_PainDie"},
    {{.pm = A_KeenDie}, "A_KeenDie"},
    {{.pm = A_BrainPain}, "A_BrainPain"},
    {{.pm = A_BrainScream}, "A_BrainScream"},
    {{.pm = A_BrainDie}, "A_BrainDie"},
    {{.pm = A_BrainAwake}, "A_BrainAwake"},
    {{.pm = A_BrainSpit}, "A_BrainSpit"},
    {{.pm = A_SpawnSound}, "A_SpawnSound"},
    {{.pm = A_SpawnFly}, "A_SpawnFly"},
    {{.pm = A_BrainExplode}, "A_BrainExplode"},
    {{.pm = A_Detonate}, "A_Detonate"}, // killough 8/9/98
    {{.pm = A_Mushroom}, "A_Mushroom"}, // killough 10/98
    {{.pm = A_Die}, "A_Die"}, // killough 11/98
    {{.pm = A_Spawn}, "A_Spawn"}, // killough 11/98
    {{.pm = A_Turn}, "A_Turn"}, // killough 11/98
    {{.pm = A_Face}, "A_Face"}, // killough 11/98
    {{.pm = A_Scratch}, "A_Scratch"}, // killough 11/98
    {{.pm = A_PlaySound}, "A_PlaySound"}, // killough 11/98
    {{.pm = A_RandomJump}, "A_RandomJump"}, // killough 11/98
    {{.pm = A_LineEffect}, "A_LineEffect"}, // killough 11/98

    {{.p2 = A_FireOldBFG},
     "A_FireOldBFG"}, // killough 7/19/98: classic BFG firing function
    {{.pm = A_BetaSkullAttack},
     "A_BetaSkullAttack"}, // killough 10/98: beta lost souls attacked different
    {{.pm = A_Stop}, "A_Stop"},

    // [XA] New mbf21 codepointers
    {{.pm = A_SpawnObject}, "A_SpawnObject", 8},
    {{.pm = A_MonsterProjectile}, "A_MonsterProjectile", 5},
    {{.pm = A_MonsterBulletAttack},"A_MonsterBulletAttack", 5, {0, 0, 1, 3, 5}},
    {{.pm = A_MonsterMeleeAttack}, "A_MonsterMeleeAttack", 4, {3, 8, 0, 0}},
    {{.pm = A_RadiusDamage}, "A_RadiusDamage", 2},
    {{.pm = A_NoiseAlert}, "A_NoiseAlert", 0},
    {{.pm = A_HealChase}, "A_HealChase", 2},
    {{.pm = A_SeekTracer}, "A_SeekTracer", 2},
    {{.pm = A_FindTracer}, "A_FindTracer", 2, {0, 10}},
    {{.pm = A_ClearTracer}, "A_ClearTracer", 0},
    {{.pm = A_JumpIfHealthBelow}, "A_JumpIfHealthBelow", 2},
    {{.pm = A_JumpIfTargetInSight}, "A_JumpIfTargetInSight", 2},
    {{.pm = A_JumpIfTargetCloser}, "A_JumpIfTargetCloser", 2},
    {{.pm = A_JumpIfTracerInSight}, "A_JumpIfTracerInSight", 2},
    {{.pm = A_JumpIfTracerCloser}, "A_JumpIfTracerCloser", 2},
    {{.pm = A_JumpIfFlagsSet}, "A_JumpIfFlagsSet", 3},
    {{.pm = A_AddFlags}, "A_AddFlags", 2},
    {{.pm = A_RemoveFlags}, "A_RemoveFlags", 2},
    {{.p2 = A_WeaponProjectile}, "A_WeaponProjectile", 5},
    {{.p2 = A_WeaponBulletAttack}, "A_WeaponBulletAttack", 5, {0, 0, 1, 5, 3}},
    {{.p2 = A_WeaponMeleeAttack}, "A_WeaponMeleeAttack", 5, {2, 10, 1 * FRACUNIT, 0, 0}},
    {{.p2 = A_WeaponSound}, "A_WeaponSound", 2},
    {{.p2 = A_WeaponAlert}, "A_WeaponAlert", 0},
    {{.p2 = A_WeaponJump}, "A_WeaponJump", 2},
    {{.p2 = A_ConsumeAmmo}, "A_ConsumeAmmo", 1},
    {{.p2 = A_CheckAmmo}, "A_CheckAmmo", 2},
    {{.p2 = A_RefireTo}, "A_RefireTo", 2},
    {{.p2 = A_GunFlashTo}, "A_GunFlashTo", 2},

    // This NULL entry must be the last in the list
    {{NULL}, "A_NULL"}, // Ty 05/16/98
};

// ====================================================================
// ProcessDehFile
// Purpose: Read and process a DEH or BEX file
// Args:    filename    -- name of the DEH/BEX file
//          outfilename -- output file (DEHOUT.TXT), appended to here
// Returns: void
//
// killough 10/98:
// substantially modified to allow input from wad lumps instead of .deh files.

static boolean processed_dehacked = false;

void ProcessDehFile(const char *filename, char *outfilename, int lumpnum)
{
    DEHFILE infile, *filein = &infile; // killough 10/98
    char inbuffer[DEH_BUFFERMAX];      // Place to put the primary infostring
    static int last_block;
    static long filepos;

    processed_dehacked = true;

    // Open output file if we're writing output
    if (outfilename && *outfilename && !deh_log_file)
    {
        static boolean firstfile = true; // to allow append to output log
        if (!strcmp(outfilename, "-"))
        {
            deh_log_file = stdout;
        }
        else
        {
            deh_log_file = M_fopen(outfilename, firstfile ? "wt" : "at");
        
            if (!deh_log_file)
            {
                I_Printf(VB_WARNING,
                         "Could not open -dehout file %s\n... using stdout.",
                         outfilename);
                deh_log_file = stdout;
            }
        }
        firstfile = false;
    }

    // killough 10/98: allow DEH files to come from wad lumps

    if (filename)
    {
        if (!(infile.file = M_fopen(filename, "rt")))
        {
            I_Printf(VB_WARNING, "-deh file %s not found", filename);
            return; // should be checked up front anyway
        }

        array_push(dehfiles, M_StringDuplicate(filename));

        infile.memfile = NULL;
    }
    else // DEH file comes from lump indicated by third argument
    {
        void *buf = W_CacheLumpNum(lumpnum, PU_STATIC);

        filename = W_WadNameForLump(lumpnum);

        // [FG] skip empty DEHACKED lumps
        if (!buf)
        {
            I_Printf(VB_WARNING, "skipping empty DEHACKED lump from file %s",
                     filename);
            return;
        }

        infile.memfile = mem_fopen_read(buf, W_LumpLength(lumpnum));
        infile.file = NULL;
    }

    I_Printf(VB_INFO, "Loading DEH %sfile %s", infile.memfile ? "lump from " : "",
             filename);
    deh_log("\nLoading DEH file %s\n\n", filename);

    // loop until end of file

    last_block = DEH_BLOCKMAX - 1;
    filepos = 0;
    while (deh_fgets(inbuffer, sizeof(inbuffer), filein))
    {
        boolean match;
        int i;

        lfstrip(inbuffer);
        deh_log("Line='%s'\n", inbuffer);
        if (!*inbuffer || *inbuffer == '#' || *inbuffer == ' ')
        {
            continue; /* Blank line or comment line */
        }

        // -- If DEH_BLOCKMAX is set right, the processing is independently
        // -- handled based on data in the deh_blocks[] structure array

        // killough 10/98: INCLUDE code rewritten to allow arbitrary nesting,
        // and to greatly simplify code, fix memory leaks, other bugs

        if (!strncasecmp(inbuffer, "INCLUDE", 7)) // include a file
        {
            // preserve state while including a file
            // killough 10/98: moved to here

            char *nextfile;
            boolean oldnotext = includenotext; // killough 10/98

            // killough 10/98: exclude if inside wads (only to discourage
            // the practice, since the code could otherwise handle it)

            if (infile.memfile)
            {
                deh_log("No files may be included from wads: %s\n", inbuffer);
                continue;
            }

            // check for no-text directive, used when including a DEH
            // file but using the BEX format to handle strings

            if (!strncasecmp(nextfile = ptr_lstrip(inbuffer + 7), "NOTEXT", 6))
            {
                includenotext = true, nextfile = ptr_lstrip(nextfile + 6);
            }

            deh_log("Branching to include file %s...\n", nextfile);

            // killough 10/98:
            // Second argument must be NULL to prevent closing fileout too soon

            ProcessDehFile(nextfile, NULL, 0); // do the included file

            includenotext = oldnotext;
            deh_log("...continuing with %s\n", filename);
            continue;
        }

        for (match = 0, i = 0; i < DEH_BLOCKMAX - 1; i++)
        {
            if (!strncasecmp(inbuffer, deh_blocks[i].key,
                             strlen(deh_blocks[i].key)))
            { // matches one
                match = 1;
                break; // we got one, that's enough for this block
            }
        }

        if (match) // inbuffer matches a valid block code name
        {
            last_block = i;
        }
        else if (last_block >= 10
                 && last_block < DEH_BLOCKMAX - 1) // restrict to BEX style lumps
        { // process that same line again with the last valid block code handler
            i = last_block;
            deh_fseek(filein, filepos);
        }

        deh_log("Processing function [%d] for %s\n", i, deh_blocks[i].key);
        deh_blocks[i].fptr(filein, inbuffer); // call function

        filepos = deh_ftell(filein); // back up line start
    }

    if (infile.memfile)
    {
        mem_fclose(infile.memfile);
    }
    else if (infile.file)
    {
        fclose(infile.file); // Close real file
    }

    if (outfilename) // killough 10/98: only at top recursion level
    {
        if (deh_log_file
            && deh_log_file != stdout) // haleyjd: don't fclose(NULL)
        {
            fclose(deh_log_file);
        }
        deh_log_file = NULL;
    }
}

// ====================================================================
// deh_procBexCodePointers
// Purpose: Handle [CODEPTR] block, BOOM Extension
// Args:    fpin  -- input file stream
//          fpout -- output file stream (DEHOUT.TXT)
//          line  -- current line in file to process
// Returns: void
//
static void deh_procBexCodePointers(DEHFILE *fpin, char *line)
{
    char key[DEH_MAXKEYLEN];
    char inbuffer[DEH_BUFFERMAX + 1];
    int indexnum;
    char mnemonic[DEH_MAXKEYLEN]; // to hold the codepointer mnemonic
    int i;                        // looper
    boolean found; // know if we found this one during lookup or not

    // Ty 05/16/98 - initialize it to something, dummy!
    strncpy(inbuffer, line, DEH_BUFFERMAX);

    // for this one, we just read 'em until we hit a blank line
    while (!deh_feof(fpin) && *inbuffer && (*inbuffer != ' '))
    {
        if (!deh_fgets(inbuffer, sizeof(inbuffer), fpin))
        {
            break;
        }
        lfstrip(inbuffer);
        if (!*inbuffer)
        {
            break; // killough 11/98: really exit on blank line
        }

        // killough 8/98: allow hex numbers in input:
        if ((3 != sscanf(inbuffer, "%s %i = %s", key, &indexnum, mnemonic))
            || (strcasecmp(key, "FRAME"))) // NOTE: different format from normal
        {
            deh_log("Invalid BEX codepointer line - must start with 'FRAME': "
                    "'%s'\n",
                    inbuffer);
            return; // early return
        }

        deh_log("Processing pointer at index %d: %s\n", indexnum, mnemonic);
        if (indexnum < 0)
        {
            deh_log("Pointer number must be positive (%d)\n", indexnum);
            return; // killough 10/98: fix SegViol
        }

        dsdh_EnsureStatesCapacity(indexnum);

        strcpy(key, "A_"); // reusing the key area to prefix the mnemonic
        strcat(key, ptr_lstrip(mnemonic));

        found = false;
        i = -1; // incremented to start at zero at the top of the loop
        do      // Ty 05/16/98 - fix loop logic to look for null ending entry
        {
            ++i;
            if (!strcasecmp(key, deh_bexptrs[i].lookup))
            { // Ty 06/01/98  - add  to states[].action for new djgcc version
                states[indexnum].action = deh_bexptrs[i].cptr; // assign
                deh_log(" - applied %p from codeptr[%d] to states[%d]\n",
                        (void *)(intptr_t)deh_bexptrs[i].cptr.v, i, indexnum);
                found = true;
            }
        } while (!found && (deh_bexptrs[i].cptr.v != NULL)); // [FG] lookup is never NULL!

        if (!found)
        {
            deh_log("Invalid frame pointer mnemonic '%s' at %d\n", mnemonic,
                    indexnum);
        }
    }
    return;
}

// ====================================================================
// deh_procThing
// Purpose: Handle DEH Thing block
// Args:    fpin  -- input file stream
//          fpout -- output file stream (DEHOUT.TXT)
//          line  -- current line in file to process
// Returns: void
//
// Ty 8/27/98 - revised to also allow mnemonics for
// bit masks for monster attributes
//

static void deh_procThing(DEHFILE *fpin, char *line)
{
    char key[DEH_MAXKEYLEN];
    char inbuffer[DEH_BUFFERMAX + 1];
    long value; // All deh values are ints or longs
    int indexnum;
    int ix;
    int *pix; // Ptr to int, since all Thing structure entries are ints
    char *strval;

    strncpy(inbuffer, line, DEH_BUFFERMAX);
    deh_log("Thing line: '%s'\n", inbuffer);

    // killough 8/98: allow hex numbers in input:
    ix = sscanf(inbuffer, "%s %i", key, &indexnum);
    deh_log("count=%d, Thing %d\n", ix, indexnum);

    // Note that the mobjinfo[] array is base zero, but object numbers
    // in the dehacked file start with one.  Grumble.
    --indexnum;

    dsdh_EnsureMobjInfoCapacity(indexnum);

    // now process the stuff
    // Note that for Things we can look up the key and use its offset
    // in the array of key strings as an int offset in the structure

    // get a line until a blank or end of file--it's not
    // blank now because it has our incoming key in it
    while (!deh_feof(fpin) && *inbuffer && (*inbuffer != ' '))
    {
        if (!deh_fgets(inbuffer, sizeof(inbuffer), fpin))
        {
            break;
        }
        lfstrip(inbuffer); // toss the end of line

        // killough 11/98: really bail out on blank lines (break != continue)
        if (!*inbuffer)
        {
            break; // bail out with blank line between sections
        }
        if (!deh_GetData(inbuffer, key, &value, &strval)) // returns true if ok
        {
            deh_log("Bad data pair in '%s'\n", inbuffer);
            continue;
        }
        for (ix = 0; ix < DEH_MOBJINFOMAX; ix++)
        {
            if (strcasecmp(key, deh_mobjinfo[ix])) // killough 8/98
            {
                continue;
            }

            switch (ix)
            {
                // Woof!
                case DEH_MOBJINFO_FLAGS_EXTRA:
                    if (!value)
                    {
                        for (value = 0; (strval = strtok(strval, ",+| \t\f\r"));
                            strval = NULL)
                        {
                            size_t iy;

                            for (iy = 0; iy < arrlen(deh_mobjflags_extra); iy++)
                            {
                                if (strcasecmp(strval,
                                              deh_mobjflags_extra[iy].name))
                                {
                                    continue;
                                }

                                value |= deh_mobjflags_extra[iy].value;
                                break;
                            }

                            if (iy >= arrlen(deh_mobjflags_extra))
                            {
                                deh_log(
                                    "Could not find Woof bit mnemonic %s\n",
                                    strval);
                            }
                        }
                    }

                    mobjinfo[indexnum].flags_extra = value;
                    break;

                // mbf21: process thing flags
                case DEH_MOBJINFO_FLAGS2:
                    if (!value)
                    {
                        for (value = 0; (strval = strtok(strval, ",+| \t\f\r"));
                             strval = NULL)
                        {
                            size_t iy;

                            for (iy = 0; iy < arrlen(deh_mobjflags_mbf21); iy++)
                            {
                                if (strcasecmp(strval,
                                               deh_mobjflags_mbf21[iy].name))
                                {
                                    continue;
                                }

                                value |= deh_mobjflags_mbf21[iy].value;
                                break;
                            }

                            if (iy >= arrlen(deh_mobjflags_mbf21))
                            {
                                deh_log(
                                    "Could not find MBF21 bit mnemonic %s\n",
                                    strval);
                            }
                        }
                    }

                    mobjinfo[indexnum].flags2 = value;
                    break;

                case DEH_MOBJINFO_FLAGS:
                    if (!value) // killough 10/98
                    {
                        // figure out what the bits are
                        value = 0;

                        // killough 10/98: replace '+' kludge with strtok() loop
                        // Fix error-handling case ('found' var wasn't being
                        // reset)
                        //
                        // Use OR logic instead of addition, to allow repetition

                        for (; (strval = strtok(strval, ",+| \t\f\r"));
                             strval = NULL)
                        {
                            int iy;
                            for (iy = 0; iy < arrlen(deh_mobjflags); iy++)
                            {
                                if (!strcasecmp(strval, deh_mobjflags[iy].name))
                                {
                                    deh_log("ORed value 0x%08lx %s\n",
                                            deh_mobjflags[iy].value, strval);
                                    value |= deh_mobjflags[iy].value;
                                    break;
                                }
                            }
                            if (iy >= arrlen(deh_mobjflags))
                            {
                                deh_log("Could not find bit mnemonic %s\n",
                                        strval);
                            }
                        }

                        // Don't worry about conversion -- simply print values
                        deh_log("Bits = 0x%08lX = %ld \n", value, value);
                    }

                    mobjinfo[indexnum].flags = value;
                    break;

                case DEH_MOBJINFO_INFIGHTING_GROUP:
                    {
                        mobjinfo_t *mi = &mobjinfo[indexnum];
                        mi->infighting_group = (int)(value);
                        if (mi->infighting_group < 0)
                        {
                            I_Error("Infighting groups must be >= 0 (check "
                                    "your dehacked)");
                            return;
                        }
                        mi->infighting_group = mi->infighting_group + IG_END;
                    }
                    break;

                case DEH_MOBJINFO_PROJECTILE_GROUP:
                    {
                        mobjinfo_t *mi = &mobjinfo[indexnum];
                        mi->projectile_group = (int)(value);
                        if (mi->projectile_group < 0)
                        {
                            mi->projectile_group = PG_GROUPLESS;
                        }
                        else
                        {
                            mi->projectile_group =
                                mi->projectile_group + PG_END;
                        }
                    }
                    break;

                case DEH_MOBJINFO_SPLASH_GROUP:
                    {
                        mobjinfo_t *mi = &mobjinfo[indexnum];
                        mi->splash_group = (int)(value);
                        if (mi->splash_group < 0)
                        {
                            I_Error("Splash groups must be >= 0 (check your "
                                    "dehacked)");
                            return;
                        }
                        mi->splash_group = mi->splash_group + SG_END;
                    }
                    break;

                case DEH_MOBJINFO_BLOODCOLOR:
                    {
                        mobjinfo_t *mi = &mobjinfo[indexnum];
                        if (value < 0 || value > 8)
                        {
                            I_Error("Blood color must be >= 0 and <= 8 (check "
                                    "your dehacked)");
                            return;
                        }
                        mi->bloodcolor = (int)(value);
                        deh_set_blood_color = true;
                    }
                    break;

                case DEH_MOBJINFO_DROPPEDITEM:
                    if (value < 0) // MT_NULL = -1
                    {
                        I_Error(
                            "Dropped item must be >= 0 (check your dehacked)");
                        return;
                    }
                    // make it base zero (deh is 1-based)
                    mobjinfo[indexnum].droppeditem = (int)(value - 1);
                    break;

                default:
                    pix = (int *)&mobjinfo[indexnum];
                    pix[ix] = (int)value;
                    break;
            }

            deh_log("Assigned %d to %s(%d) at index %d\n", (int)value, key,
                    indexnum, ix);
        }
    }
    return;
}

// ====================================================================
// deh_procFrame
// Purpose: Handle DEH Frame block
// Args:    fpin  -- input file stream
//          fpout -- output file stream (DEHOUT.TXT)
//          line  -- current line in file to process
// Returns: void
//
static void deh_procFrame(DEHFILE *fpin, char *line)
{
    char key[DEH_MAXKEYLEN];
    char inbuffer[DEH_BUFFERMAX + 1];
    long value; // All deh values are ints or longs
    int indexnum;
    char *strval;

    strncpy(inbuffer, line, DEH_BUFFERMAX);

    // killough 8/98: allow hex numbers in input:
    sscanf(inbuffer, "%s %i", key, &indexnum);
    deh_log("Processing Frame at index %d: %s\n", indexnum, key);
    if (indexnum < 0)
    {
        deh_log("Frame number must be positive (%d)\n", indexnum);
        return;
    }

    dsdh_EnsureStatesCapacity(indexnum);

    while (!deh_feof(fpin) && *inbuffer && (*inbuffer != ' '))
    {
        if (!deh_fgets(inbuffer, sizeof(inbuffer), fpin))
        {
            break;
        }
        lfstrip(inbuffer);
        if (!*inbuffer)
        {
            break; // killough 11/98
        }
        if (!deh_GetData(inbuffer, key, &value, &strval)) // returns true if ok
        {
            deh_log("Bad data pair in '%s'\n", inbuffer);
            continue;
        }
        if (!strcasecmp(key, deh_state[0])) // Sprite number
        {
            deh_log(" - sprite = %ld\n", value);
            states[indexnum].sprite = (spritenum_t)value;
        }
        else if (!strcasecmp(key, deh_state[1])) // Sprite subnumber
        {
            deh_log(" - frame = %ld\n", value);
            states[indexnum].frame = value; // long
        }
        else if (!strcasecmp(key, deh_state[2])) // Duration
        {
            deh_log(" - tics = %ld\n", value);
            states[indexnum].tics = value; // long
        }
        else if (!strcasecmp(key, deh_state[3])) // Next frame
        {
            deh_log(" - nextstate = %ld\n", value);
            states[indexnum].nextstate = (statenum_t)value;
        }
        else if (!strcasecmp(key, deh_state[4])) // Codep frame (not set in Frame deh block)
        {
            deh_log(" - codep, should not be set in Frame section!\n");
            /* nop */;
        }
        else if (!strcasecmp(key, deh_state[5])) // Unknown 1
        {
            deh_log(" - misc1 = %ld\n", value);
            states[indexnum].misc1 = value; // long
        }
        else if (!strcasecmp(key, deh_state[6])) // Unknown 2
        {
            deh_log(" - misc2 = %ld\n", value);
            states[indexnum].misc2 = value; // long
        }
        else if (!strcasecmp(key, deh_state[7])) // Args1
        {
            deh_log(" - args[0] = %ld\n", (long)value);
            states[indexnum].args[0] = (long)value; // long
            defined_codeptr_args[indexnum] |= (1 << 0);
        }
        else if (!strcasecmp(key, deh_state[8])) // Args2
        {
            deh_log(" - args[1] = %ld\n", (long)value);
            states[indexnum].args[1] = (long)value; // long
            defined_codeptr_args[indexnum] |= (1 << 1);
        }
        else if (!strcasecmp(key, deh_state[9])) // Args3
        {
            deh_log(" - args[2] = %ld\n", (long)value);
            states[indexnum].args[2] = (long)value; // long
            defined_codeptr_args[indexnum] |= (1 << 2);
        }
        else if (!strcasecmp(key, deh_state[10])) // Args4
        {
            deh_log(" - args[3] = %ld\n", (long)value);
            states[indexnum].args[3] = (long)value; // long
            defined_codeptr_args[indexnum] |= (1 << 3);
        }
        else if (!strcasecmp(key, deh_state[11])) // Args5
        {
            deh_log(" - args[4] = %ld\n", (long)value);
            states[indexnum].args[4] = (long)value; // long
            defined_codeptr_args[indexnum] |= (1 << 4);
        }
        else if (!strcasecmp(key, deh_state[12])) // Args6
        {
            deh_log(" - args[5] = %ld\n", (long)value);
            states[indexnum].args[5] = (long)value; // long
            defined_codeptr_args[indexnum] |= (1 << 5);
        }
        else if (!strcasecmp(key, deh_state[13])) // Args7
        {
            deh_log(" - args[6] = %ld\n", (long)value);
            states[indexnum].args[6] = (long)value; // long
            defined_codeptr_args[indexnum] |= (1 << 6);
        }
        else if (!strcasecmp(key, deh_state[14])) // Args8
        {
            deh_log(" - args[7] = %ld\n", (long)value);
            states[indexnum].args[7] = (long)value; // long
            defined_codeptr_args[indexnum] |= (1 << 7);
        }
        else if (!strcasecmp(key, deh_state[15])) // MBF21 Bits
        {
            if (!value)
            {
                for (value = 0; (strval = strtok(strval, ",+| \t\f\r"));
                     strval = NULL)
                {
                    const deh_flag_t *flag;

                    for (flag = deh_stateflags_mbf21; flag->name; flag++)
                    {
                        if (strcasecmp(strval, flag->name))
                        {
                            continue;
                        }

                        value |= flag->value;
                        break;
                    }

                    if (!flag->name)
                    {
                        deh_log("Could not find MBF21 frame bit mnemonic %s\n",
                            strval);
                    }
                }
            }

            states[indexnum].flags = value;
        }
        else
        {
            deh_log("Invalid frame string index for '%s'\n", key);
        }
    }
    return;
}

// ====================================================================
// deh_procPointer
// Purpose: Handle DEH Code pointer block, can use BEX [CODEPTR] instead
// Args:    fpin  -- input file stream
//          fpout -- output file stream (DEHOUT.TXT)
//          line  -- current line in file to process
// Returns: void
//
static void deh_procPointer(DEHFILE *fpin, char *line) // done
{
    char key[DEH_MAXKEYLEN];
    char inbuffer[DEH_BUFFERMAX + 1];
    long value; // All deh values are ints or longs
    int indexnum;
    int i; // looper

    strncpy(inbuffer, line, DEH_BUFFERMAX);
    // NOTE: different format from normal

    // killough 8/98: allow hex numbers in input, fix error case:
    if (sscanf(inbuffer, "%*s %*i (%s %i)", key, &indexnum) != 2)
    {
        deh_log("Bad data pair in '%s'\n", inbuffer);
        return;
    }

    deh_log("Processing Pointer at index %d: %s\n", indexnum, key);
    if (indexnum < 0)
    {
        deh_log("Pointer number must be positive (%d)\n", indexnum);
        return;
    }

    dsdh_EnsureStatesCapacity(indexnum);

    while (!deh_feof(fpin) && *inbuffer && (*inbuffer != ' '))
    {
        if (!deh_fgets(inbuffer, sizeof(inbuffer), fpin))
        {
            break;
        }
        lfstrip(inbuffer);
        if (!*inbuffer)
        {
            break; // killough 11/98
        }
        if (!deh_GetData(inbuffer, key, &value, NULL)) // returns true if ok
        {
            deh_log("Bad data pair in '%s'\n", inbuffer);
            continue;
        }

        if (value < 0)
        {
            deh_log("Pointer number must be positive (%ld)\n", value);
            return;
        }

        dsdh_EnsureStatesCapacity(value);

        if (!strcasecmp(key, deh_state[4])) // Codep frame (not set in Frame deh block)
        {
            states[indexnum].action = deh_codeptr[value];
            deh_log(" - applied %p from codeptr[%ld] to states[%d]\n",
                    (void *)(intptr_t)deh_codeptr[value].v, value, indexnum);
            // Write BEX-oriented line to match:
            for (i = 0; i < arrlen(deh_bexptrs); i++) // [FG] array size!
            {
                if (deh_bexptrs[i].cptr.v == deh_codeptr[value].v)
                {
                    deh_log("BEX [CODEPTR] -> FRAME %d = %s\n", indexnum,
                            &deh_bexptrs[i].lookup[2]);
                    break;
                }
            }
        }
        else
        {
            deh_log("Invalid frame pointer index for '%s' at %ld, xref %p\n",
                    key, value, (void *)(intptr_t)deh_codeptr[value].v);
        }
    }
    return;
}

// ====================================================================
// deh_procSounds
// Purpose: Handle DEH Sounds block
// Args:    fpin  -- input file stream
//          fpout -- output file stream (DEHOUT.TXT)
//          line  -- current line in file to process
// Returns: void
//
static void deh_procSounds(DEHFILE *fpin, char *line)
{
    char key[DEH_MAXKEYLEN];
    char inbuffer[DEH_BUFFERMAX + 1];
    long value; // All deh values are ints or longs
    int indexnum;

    strncpy(inbuffer, line, DEH_BUFFERMAX);

    // killough 8/98: allow hex numbers in input:
    sscanf(inbuffer, "%s %i", key, &indexnum);
    deh_log("Processing Sounds at index %d: %s\n", indexnum, key);
    if (indexnum < 0)
    {
        deh_log("Sound number must be positive (%d)\n", indexnum);
    }

    dsdh_EnsureSFXCapacity(indexnum);

    while (!deh_feof(fpin) && *inbuffer && (*inbuffer != ' '))
    {
        if (!deh_fgets(inbuffer, sizeof(inbuffer), fpin))
        {
            break;
        }
        lfstrip(inbuffer);
        if (!*inbuffer)
        {
            break; // killough 11/98
        }
        if (!deh_GetData(inbuffer, key, &value, NULL)) // returns true if ok
        {
            deh_log("Bad data pair in '%s'\n", inbuffer);
            continue;
        }
        if (!strcasecmp(key, deh_sfxinfo[0])) // Offset
            /* nop */; // we don't know what this is, I don't think
        else if (!strcasecmp(key, deh_sfxinfo[1])) // Zero/One
        {
            S_sfx[indexnum].singularity = value;
        }
        else if (!strcasecmp(key, deh_sfxinfo[2])) // Value
        {
            S_sfx[indexnum].priority = value;
        }
        else if (!strcasecmp(key, deh_sfxinfo[3])) // Zero 1
            ;                                      // ignored
        else if (!strcasecmp(key, deh_sfxinfo[4])) // Zero 2
            ;                                      // ignored
        else if (!strcasecmp(key, deh_sfxinfo[5])) // Zero 3
            ;                                      // ignored
        else if (!strcasecmp(key, deh_sfxinfo[6])) // Zero 4
            ;                                      // ignored
        else if (!strcasecmp(key, deh_sfxinfo[7])) // Neg. One 1
            ;                                      // ignored
        else if (!strcasecmp(key, deh_sfxinfo[8])) // Neg. One 2
            ;                                      // ignored
        else
        {
            deh_log("Invalid sound string index for '%s'\n", key);
        }
    }
    return;
}

// ====================================================================
// deh_procAmmo
// Purpose: Handle DEH Ammo block
// Args:    fpin  -- input file stream
//          fpout -- output file stream (DEHOUT.TXT)
//          line  -- current line in file to process
// Returns: void
//
static void deh_procAmmo(DEHFILE *fpin, char *line)
{
    char key[DEH_MAXKEYLEN];
    char inbuffer[DEH_BUFFERMAX + 1];
    long value; // All deh values are ints or longs
    int indexnum;

    strncpy(inbuffer, line, DEH_BUFFERMAX);

    // killough 8/98: allow hex numbers in input:
    sscanf(inbuffer, "%s %i", key, &indexnum);
    deh_log("Processing Ammo at index %d: %s\n", indexnum, key);
    if (indexnum < 0 || indexnum >= NUMAMMO)
    {
        deh_log("Bad ammo number %d of %d\n", indexnum, NUMAMMO);
    }

    while (!deh_feof(fpin) && *inbuffer && (*inbuffer != ' '))
    {
        if (!deh_fgets(inbuffer, sizeof(inbuffer), fpin))
        {
            break;
        }
        lfstrip(inbuffer);
        if (!*inbuffer)
        {
            break; // killough 11/98
        }
        if (!deh_GetData(inbuffer, key, &value, NULL)) // returns true if ok
        {
            deh_log("Bad data pair in '%s'\n", inbuffer);
            continue;
        }
        if (!strcasecmp(key, deh_ammo[0])) // Max ammo
        {
            maxammo[indexnum] = value;
        }
        else if (!strcasecmp(key, deh_ammo[1])) // Per ammo
        {
            clipammo[indexnum] = value;
        }
        else
        {
            deh_log("Invalid ammo string index for '%s'\n", key);
        }
    }
    return;
}

// ====================================================================
// deh_procWeapon
// Purpose: Handle DEH Weapon block
// Args:    fpin  -- input file stream
//          fpout -- output file stream (DEHOUT.TXT)
//          line  -- current line in file to process
// Returns: void
//
static void deh_procWeapon(DEHFILE *fpin, char *line)
{
    char key[DEH_MAXKEYLEN];
    char inbuffer[DEH_BUFFERMAX + 1];
    long value; // All deh values are ints or longs
    int indexnum;
    char *strval;

    strncpy(inbuffer, line, DEH_BUFFERMAX);

    // killough 8/98: allow hex numbers in input:
    sscanf(inbuffer, "%s %i", key, &indexnum);
    deh_log("Processing Weapon at index %d: %s\n", indexnum, key);
    if (indexnum < 0 || indexnum >= NUMWEAPONS)
    {
        deh_log("Bad weapon number %d of %d\n", indexnum, NUMAMMO);
    }

    while (!deh_feof(fpin) && *inbuffer && (*inbuffer != ' '))
    {
        if (!deh_fgets(inbuffer, sizeof(inbuffer), fpin))
        {
            break;
        }
        lfstrip(inbuffer);
        if (!*inbuffer)
        {
            break; // killough 11/98
        }
        if (!deh_GetData(inbuffer, key, &value, &strval)) // returns true if ok
        {
            deh_log("Bad data pair in '%s'\n", inbuffer);
            continue;
        }
        if (!strcasecmp(key, deh_weapon[0])) // Ammo type
        {
            weaponinfo[indexnum].ammo = value;
        }
        else if (!strcasecmp(key, deh_weapon[1])) // Deselect frame
        {
            weaponinfo[indexnum].upstate = value;
        }
        else if (!strcasecmp(key, deh_weapon[2])) // Select frame
        {
            weaponinfo[indexnum].downstate = value;
        }
        else if (!strcasecmp(key, deh_weapon[3])) // Bobbing frame
        {
            weaponinfo[indexnum].readystate = value;
        }
        else if (!strcasecmp(key, deh_weapon[4])) // Shooting frame
        {
            weaponinfo[indexnum].atkstate = value;
        }
        else if (!strcasecmp(key, deh_weapon[5])) // Firing frame
        {
            weaponinfo[indexnum].flashstate = value;
        }
        else if (!strcasecmp(key, deh_weapon[6])) // Ammo per shot
        {
            weaponinfo[indexnum].ammopershot = value;
            weaponinfo[indexnum].intflags |= WIF_ENABLEAPS;
        }
        else if (!strcasecmp(key, deh_weapon[7])) // MBF21 Bits
        {
            if (!value)
            {
                for (value = 0; (strval = strtok(strval, ",+| \t\f\r"));
                     strval = NULL)
                {
                    const deh_flag_t *flag;

                    for (flag = deh_weaponflags_mbf21; flag->name; flag++)
                    {
                        if (strcasecmp(strval, flag->name))
                        {
                            continue;
                        }

                        value |= flag->value;
                        break;
                    }

                    if (!flag->name)
                    {
                        deh_log("Could not find MBF21 weapon bit mnemonic %s\n",
                            strval);
                    }
                }
            }

            weaponinfo[indexnum].flags = value;
        }
        else if (!strcasecmp(key, deh_weapon[8])) // Carousel icon
        {
            char candidate[9] = {0};
            M_CopyLumpName(candidate, ptr_lstrip(strval));
            int len = strlen(candidate);
            if (len < 1 || len > 7)
            {
                deh_log("Bad length for carousel icon name '%s'\n", candidate);
                continue;
            }
            weaponinfo[indexnum].carouselicon = M_StringDuplicate(candidate);
        }
        else
        {
            deh_log("Invalid weapon string index for '%s'\n", key);
        }
    }
    return;
}

// ====================================================================
// deh_procSprite
// Purpose: Dummy - we do not support the DEH Sprite block
// Args:    fpin  -- input file stream
//          fpout -- output file stream (DEHOUT.TXT)
//          line  -- current line in file to process
// Returns: void
//
static void deh_procSprite(DEHFILE *fpin, char *line) // Not supported
{
    char key[DEH_MAXKEYLEN];
    char inbuffer[DEH_BUFFERMAX + 1];
    int indexnum;

    // Too little is known about what this is supposed to do, and
    // there are better ways of handling sprite renaming.  Not supported.

    strncpy(inbuffer, line, DEH_BUFFERMAX);

    // killough 8/98: allow hex numbers in input:
    sscanf(inbuffer, "%s %i", key, &indexnum);
    deh_log("Ignoring Sprite offset change at index %d: %s\n", indexnum, key);
    while (!deh_feof(fpin) && *inbuffer && (*inbuffer != ' '))
    {
        if (!deh_fgets(inbuffer, sizeof(inbuffer), fpin))
        {
            break;
        }
        lfstrip(inbuffer);
        if (!*inbuffer)
        {
            break; // killough 11/98
        }
        // ignore line
        deh_log("- %s\n", inbuffer);
    }
    return;
}

// ====================================================================
// deh_procPars
// Purpose: Handle BEX extension for PAR times
// Args:    fpin  -- input file stream
//          fpout -- output file stream (DEHOUT.TXT)
//          line  -- current line in file to process
// Returns: void
//
static void deh_procPars(DEHFILE *fpin, char *line) // extension
{
    char key[DEH_MAXKEYLEN];
    char inbuffer[DEH_BUFFERMAX + 1];
    int indexnum;
    int episode, level, partime, oldpar;

    // new item, par times
    // usage: After [PARS] Par 0 section identifier, use one or more of these
    // lines:
    //  par 3 5 120
    //  par 14 230
    // The first would make the par for E3M5 be 120 seconds, and the
    // second one makes the par for MAP14 be 230 seconds.  The number
    // of parameters on the line determines which group of par values
    // is being changed.  Error checking is done based on current fixed
    // array sizes of[4][10] and [32]

    strncpy(inbuffer, line, DEH_BUFFERMAX);

    // killough 8/98: allow hex numbers in input:
    sscanf(inbuffer, "%s %i", key, &indexnum);
    deh_log("Processing Par value at index %d: %s\n", indexnum, key);
    // indexnum is a dummy entry
    while (!deh_feof(fpin) && *inbuffer && (*inbuffer != ' '))
    {
        if (!deh_fgets(inbuffer, sizeof(inbuffer), fpin))
        {
            break;
        }
        M_StringToLower(inbuffer); // lowercase it
        lfstrip(inbuffer);
        if (!*inbuffer)
        {
            break; // killough 11/98
        }
        if (3 != sscanf(inbuffer, "par %i %i %i", &episode, &level, &partime))
        { // not 3
            if (2 != sscanf(inbuffer, "par %i %i", &level, &partime))
            { // not 2
                deh_log("Invalid par time setting string: %s\n", inbuffer);
            }
            else
            { // is 2
                // Ty 07/11/98 - wrong range check, not zero-based
                if (level < 1 || level > 32) // base 0 array (but 1-based parm)
                {
                    deh_log("Invalid MAPnn value MAP%d\n", level);
                }
                else
                {
                    oldpar = cpars[level - 1];
                    deh_log("Changed par time for MAP%02d from %d to %d\n",
                            level, oldpar, partime);
                    cpars[level - 1] = partime;
                    deh_pars = true;
                }
            }
        }
        else
        { // is 3
            // note that though it's a [4][10] array, the "left" and "top"
            // aren't used, effectively making it a base 1 array.
            // Ty 07/11/98 - level was being checked against max 3 - dumb error
            // Note that episode 4 does not have par times per original design
            // in Ultimate DOOM so that is not supported here.
            if (episode < 1 || episode > 3 || level < 1 || level > 9)
            {
                deh_log("Invalid ExMx values E%dM%d\n", episode, level);
            }
            else
            {
                oldpar = pars[episode][level];
                pars[episode][level] = partime;
                deh_log("Changed par time for E%dM%d from %d to %d\n", episode,
                        level, oldpar, partime);
                deh_pars = true;
            }
        }
    }
    return;
}

// ====================================================================
// deh_procCheat
// Purpose: Handle DEH Cheat block
// Args:    fpin  -- input file stream
//          fpout -- output file stream (DEHOUT.TXT)
//          line  -- current line in file to process
// Returns: void
//
static void deh_procCheat(DEHFILE *fpin, char *line) // done
{
    char key[DEH_MAXKEYLEN];
    char inbuffer[DEH_BUFFERMAX + 1];
    long value;        // All deh values are ints or longs
    char *strval = ""; // pointer to the value area
    int ix, iy;        // array indices
    char *p;           // utility pointer

    boolean deh_apply_cheats = true;

    //!
    // @category mod
    //
    // Ignore cheats in dehacked files.
    //

    if (M_CheckParm("-nocheats"))
    {
        deh_apply_cheats = false;
    }

    deh_log("Processing Cheat: %s\n", line);

    strncpy(inbuffer, line, DEH_BUFFERMAX);
    while (!deh_feof(fpin) && *inbuffer && (*inbuffer != ' '))
    {
        if (!deh_fgets(inbuffer, sizeof(inbuffer), fpin))
        {
            break;
        }
        lfstrip(inbuffer);
        if (!*inbuffer)
        {
            break; // killough 11/98
        }
        if (!deh_GetData(inbuffer, key, &value, &strval)) // returns true if ok
        {
            deh_log("Bad data pair in '%s'\n", inbuffer);
            continue;
        }
        // [FG] ignore cheats in dehacked files
        if (!deh_apply_cheats)
        {
            continue;
        }

        // Otherwise we got a (perhaps valid) cheat name,
        // so look up the key in the array

        // killough 4/18/98: use main cheat code table in st_stuff.c now
        for (ix = 0; cheat[ix].cheat; ix++)
        {
            if (cheat[ix].deh_cheat) // killough 4/18/98: skip non-deh
            {
                if (!strcasecmp(key, cheat[ix].deh_cheat)) // found the cheat, ignored case
                {
                    // replace it but don't overflow it.  Use current length as
                    // limit.
                    // Ty 03/13/98 - add 0xff code
                    // Deal with the fact that the cheats in deh files are
                    // extended with character 0xFF to the original cheat
                    // length, which we don't do.
                    for (iy = 0; strval[iy]; iy++)
                    {
                        strval[iy] = (strval[iy] == (char)0xff) ? '\0' : strval[iy];
                    }

                    iy = ix; // killough 4/18/98

                    // Ty 03/14/98 - skip leading spaces
                    p = strval;
                    while (*p == ' ')
                    {
                        ++p;
                    }
                    // Ty 03/16/98 - change to use a strdup and orphan the
                    // original.
                    // Also has the advantage of allowing length changes.
                    // strncpy(cheat[iy].cheat,p,strlen(cheat[iy].cheat));
                    //
                    // killough 9/12/98: disable cheats which are prefixes of
                    // this one
                    for (int i = 0; cheat[i].cheat; i++)
                    {
                        if (cheat[i].when & not_deh
                            && !strncasecmp(cheat[i].cheat, cheat[iy].cheat,
                                            strlen(cheat[i].cheat))
                            && i != iy)
                        {
                            cheat[i].deh_modified = true;
                        }
                    }
                    cheat[iy].cheat = strdup(p);
                    deh_log("Assigned new cheat '%s' to cheat '%s'at index %d\n",
                        p, cheat[ix].deh_cheat, iy); // killough 4/18/98
                }
            }
        }
        deh_log("- %s\n", inbuffer);
    }
    return;
}

// ====================================================================
// deh_procMisc
// Purpose: Handle DEH Misc block
// Args:    fpin  -- input file stream
//          fpout -- output file stream (DEHOUT.TXT)
//          line  -- current line in file to process
// Returns: void
//
static void deh_procMisc(DEHFILE *fpin, char *line) // done
{
    char key[DEH_MAXKEYLEN];
    char inbuffer[DEH_BUFFERMAX + 1];
    long value; // All deh values are ints or longs

    strncpy(inbuffer, line, DEH_BUFFERMAX);
    while (!deh_feof(fpin) && *inbuffer && (*inbuffer != ' '))
    {
        if (!deh_fgets(inbuffer, sizeof(inbuffer), fpin))
        {
            break;
        }
        lfstrip(inbuffer);
        if (!*inbuffer)
        {
            break; // killough 11/98
        }
        if (!deh_GetData(inbuffer, key, &value, NULL)) // returns true if ok
        {
            deh_log("Bad data pair in '%s'\n", inbuffer);
            continue;
        }
        // Otherwise it's ok
        deh_log("Processing Misc item '%s'\n", key);

        if (!strcasecmp(key, deh_misc[0])) // Initial Health
        {
            initial_health = value;
        }
        else if (!strcasecmp(key, deh_misc[1])) // Initial Bullets
        {
            initial_bullets = value;
        }
        else if (!strcasecmp(key, deh_misc[2])) // Max Health
        {
            deh_maxhealth = value;
            deh_set_maxhealth = true;
        }
        else if (!strcasecmp(key, deh_misc[3])) // Max Armor
        {
            max_armor = value;
        }
        else if (!strcasecmp(key, deh_misc[4])) // Green Armor Class
        {
            green_armor_class = value;
        }
        else if (!strcasecmp(key, deh_misc[5])) // Blue Armor Class
        {
            blue_armor_class = value;
        }
        else if (!strcasecmp(key, deh_misc[6])) // Max Soulsphere
        {
            max_soul = value;
        }
        else if (!strcasecmp(key, deh_misc[7])) // Soulsphere Health
        {
            soul_health = value;
        }
        else if (!strcasecmp(key, deh_misc[8])) // Megasphere Health
        {
            mega_health = value;
        }
        else if (!strcasecmp(key, deh_misc[9])) // God Mode Health
        {
            god_health = value;
        }
        else if (!strcasecmp(key, deh_misc[10])) // IDFA Armor
        {
            idfa_armor = value;
        }
        else if (!strcasecmp(key, deh_misc[11])) // IDFA Armor Class
        {
            idfa_armor_class = value;
        }
        else if (!strcasecmp(key, deh_misc[12])) // IDKFA Armor
        {
            idkfa_armor = value;
        }
        else if (!strcasecmp(key, deh_misc[13])) // IDKFA Armor Class
        {
            idkfa_armor_class = value;
        }
        else if (!strcasecmp(key, deh_misc[14])) // BFG Cells/Shot
        {
            weaponinfo[wp_bfg].ammopershot = bfgcells = value;
            bfgcells_modified = true;
        }
        else if (!strcasecmp(key, deh_misc[15])) // Monsters Infight
        {
            // Andrey Budko: Dehacked support - monsters infight
            if (value == 202)
            {
                deh_species_infighting = 0;
            }
            else if (value == 221)
            {
                deh_species_infighting = 1;
            }
            else
            {
                deh_log("Invalid value for 'Monsters Infight': %i", (int)value);
            }
            /* No such switch in DOOM - nop */;
        }
        else
        {
            deh_log("Invalid misc item string index for '%s'\n", key);
        }
    }
    return;
}

// ====================================================================
// deh_procText
// Purpose: Handle DEH Text block
// Notes:   We look things up in the current information and if found
//          we replace it.  At the same time we write the new and
//          improved BEX syntax to the log file for future use.
// Args:    fpin  -- input file stream
//          fpout -- output file stream (DEHOUT.TXT)
//          line  -- current line in file to process
// Returns: void
//
static void deh_procText(DEHFILE *fpin, char *line)
{
    char key[DEH_MAXKEYLEN];
    char inbuffer[DEH_BUFFERMAX * 2]; // can't use line -- double size buffer too.
    int i;                 // loop variable
    int fromlen, tolen;    // as specified on the text block line
    int usedlen;           // shorter of fromlen and tolen if not matched
    boolean found = false; // to allow early exit once found
    char *line2 = NULL;    // duplicate line for rerouting

    // Ty 04/11/98 - Included file may have NOTEXT skip flag set
    if (includenotext) // flag to skip included deh-style text
    {
        deh_log("Skipped text block because of notext directive\n");
        strcpy(inbuffer, line);
        while (!deh_feof(fpin) && *inbuffer && (*inbuffer != ' '))
        {
            deh_fgets(inbuffer, sizeof(inbuffer), fpin); // skip block
        }
        // Ty 05/17/98 - don't care if this fails
        return; // ************** Early return
    }

    // killough 8/98: allow hex numbers in input:
    sscanf(line, "%s %i %i", key, &fromlen, &tolen);
    deh_log("Processing Text (key=%s, from=%d, to=%d)\n", key, fromlen, tolen);

    // killough 10/98: fix incorrect usage of feof
    {
        int c, totlen = 0;
        while (totlen < fromlen + tolen && (c = deh_fgetc(fpin)) != EOF)
        {
            if (c != '\r') // [FG] fix CRLF mismatch
            {
                inbuffer[totlen++] = c;
            }
        }
        inbuffer[totlen] = '\0';
    }

    // if the from and to are 4, this may be a sprite rename.  Check it
    // against the array and process it as such if it matches.  Remember
    // that the original names are (and should remain) uppercase.
    // Future: this will be from a separate [SPRITES] block.
    if (fromlen == 4 && tolen == 4)
    {
        i = dsdh_GetDehSpriteIndex(inbuffer);

        if (i >= 0)
        {
            deh_log("Changing name of sprite at index %d from %s to %*s\n", i,
                    sprnames[i], tolen, &inbuffer[fromlen]);
            // Ty 03/18/98 - not using strdup because length is fixed

            // killough 10/98: but it's an array of pointers, so we must
            // use strdup unless we redeclare sprnames and change all else
            sprnames[i] = strdup(sprnames[i]);

            strncpy(sprnames[i], &inbuffer[fromlen], tolen);
            found = true;
        }
    }

    if (!found && fromlen < 7
        && tolen < 7) // lengths of music and sfx are 6 or shorter
    {
        usedlen = (fromlen < tolen) ? fromlen : tolen;
        if (fromlen != tolen)
        {
            deh_log("Warning: Mismatched lengths from=%d, to=%d, used %d\n",
                    fromlen, tolen, usedlen);
        }
        i = dsdh_GetDehSFXIndex(inbuffer, (size_t)fromlen);
        if (i >= 0)
        {
            deh_log("Changing name of sfx from %s to %*s\n", S_sfx[i].name,
                    usedlen, &inbuffer[fromlen]);

            S_sfx[i].name = strdup(&inbuffer[fromlen]);
            found = true;
        }
        if (!found) // not yet
        {
            i = dsdh_GetDehMusicIndex(inbuffer, fromlen);
            if (i >= 0)
            {
                deh_log("Changing name of music from %s to %*s\n",
                        S_music[i].name, usedlen, &inbuffer[fromlen]);

                S_music[i].name = strdup(&inbuffer[fromlen]);
                found = true;
            }
        } // end !found test
    }

    if (!found) // Nothing we want to handle here--see if strings can deal with
                // it.
    {
        deh_log(
            "Checking text area through strings for '%.12s%s' from=%d to=%d\n",
            inbuffer, (strlen(inbuffer) > 12) ? "..." : "", fromlen, tolen);
        if (fromlen <= strlen(inbuffer))
        {
            line2 = strdup(&inbuffer[fromlen]);
            inbuffer[fromlen] = '\0';
        }

        deh_procStringSub(NULL, inbuffer, line2);
    }
    free(line2); // may be NULL, ignored by free()
    return;
}

static void deh_procError(DEHFILE *fpin, char *line)
{
    char inbuffer[DEH_BUFFERMAX + 1];

    strncpy(inbuffer, line, DEH_BUFFERMAX);
    deh_log("Unmatched Block: '%s'\n", inbuffer);
    return;
}

// [FG] Obituaries
static boolean deh_procObituarySub(char *key, char *newstring)
{
    boolean found = false;
    int actor = -1;

    if (sscanf(key, "Obituary_Deh_Actor_%d", &actor) == 1)
    {
        if (actor >= 0 && actor < num_mobj_types)
        {
            if (M_StringEndsWith(key, "_Melee"))
            {
                mobjinfo[actor].obituary_melee = strdup(newstring);
            }
            else
            {
                mobjinfo[actor].obituary = strdup(newstring);
            }

            found = true;
        }
    }

    return found;
}

// ====================================================================
// deh_procStrings
// Purpose: Handle BEX [STRINGS] extension
// Args:    fpin  -- input file stream
//          fpout -- output file stream (DEHOUT.TXT)
//          line  -- current line in file to process
// Returns: void
//
static void deh_procStrings(DEHFILE *fpin, char *line)
{
    char key[DEH_MAXKEYLEN];
    char inbuffer[DEH_BUFFERMAX + 1];
    long value;                 // All deh values are ints or longs
    char *strval;               // holds the string value of the line
    static int maxstrlen = 128; // maximum string length, bumped 128 at
    // a time as needed
    // holds the final result of the string after concatenation
    static char *holdstring = NULL;
    boolean found = false; // looking for string continuation

    deh_log("Processing extended string substitution\n");

    if (!holdstring)
    {
        holdstring = malloc(maxstrlen * sizeof(*holdstring));
    }

    *holdstring = '\0'; // empty string to start with
    strncpy(inbuffer, line, DEH_BUFFERMAX);
    // Ty 04/24/98 - have to allow inbuffer to start with a blank for
    // the continuations of C1TEXT etc.
    while (!deh_feof(fpin) && *inbuffer) /* && (*inbuffer != ' ') */
    {
        if (!deh_fgets(inbuffer, sizeof(inbuffer), fpin))
        {
            break;
        }
        if (*inbuffer == '#')
        {
            continue; // skip comment lines
        }
        lfstrip(inbuffer);
        if (!*inbuffer && !*holdstring)
        {
            break; // killough 11/98
        }
        if (!*holdstring) // first one--get the key
        {
            if (!deh_GetData(inbuffer, key, &value,
                             &strval)) // returns true if ok
            {
                deh_log("Bad data pair in '%s'\n", inbuffer);
                continue;
            }
        }
        while (strlen(holdstring) + strlen(inbuffer) > maxstrlen) // Ty03/29/98 - fix stupid error
        {
            // killough 11/98: allocate enough the first time
            maxstrlen = strlen(holdstring)
                        + strlen(inbuffer); // [FG] fix buffer size calculation
            deh_log("* increased buffer from to %d for buffer size %d\n",
                    maxstrlen, (int)strlen(inbuffer));
            holdstring = I_Realloc(holdstring, maxstrlen * sizeof(*holdstring));
        }
        // concatenate the whole buffer if continuation or the value iffirst
        strcat(holdstring, ptr_lstrip(((*holdstring) ? inbuffer : strval)));
        rstrip(holdstring);
        // delete any trailing blanks past the backslash
        // note that blanks before the backslash will be concatenated
        // but ones at the beginning of the next line will not, allowing
        // indentation in the file to read well without affecting the
        // string itself.
        if (holdstring[strlen(holdstring) - 1] == '\\')
        {
            holdstring[strlen(holdstring) - 1] = '\0';
            continue; // ready to concatenate
        }
        if (*holdstring) // didn't have a backslash, trap above would catch that
        {
            // go process the current string
            found = deh_procStringSub(key, NULL, holdstring); // supply keyand not search string

            // [FG] Obituaries
            if (!found)
            {
                found = deh_procObituarySub(key, holdstring);
            }

            if (!found)
            {
                deh_log("Invalid string key '%s', substitution skipped.\n", key);
            }

            *holdstring = '\0'; // empty string for the next one
        }
    }
    return;
}

// ====================================================================
// deh_procStringSub
// Purpose: Common string parsing and handling routine for DEH and BEX
// Args:    key       -- place to put the mnemonic for the string if found
//          lookfor   -- original value string to look for
//          newstring -- string to put in its place if found
//          fpout     -- file stream pointer for log file (DEHOUT.TXT)
// Returns: boolean: True if string found, false if not
//
static boolean deh_procStringSub(char *key, char *lookfor, char *newstring)
{
    boolean found; // loop exit flag
    int i;         // looper

    found = false;
    for (i = 0; i < arrlen(deh_strlookup); i++)
    {
        if (deh_strlookup[i].orig == NULL)
        {
            deh_strlookup[i].orig = *deh_strlookup[i].ppstr;
        }
        found = lookfor ? !strcasecmp(deh_strlookup[i].orig, lookfor)
                        : !strcasecmp(deh_strlookup[i].lookup, key);

        if (found)
        {
            *deh_strlookup[i].ppstr = strdup(newstring); // orphan originalstring
            found = true;
            // Handle embedded \n's in the incoming string, convert to 0x0a's
            {
                char *s, *t;
                for (s = t = *deh_strlookup[i].ppstr; *s; ++s, ++t)
                {
                    if (*s == '\\' && (s[1] == 'n' || s[1] == 'N')) // found one
                    {
                        ++s, *t = '\n'; // skip one extra for second character
                    }
                    else
                    {
                        *t = *s;
                    }
                }
                *t = '\0'; // cap off the target string
            }

            if (key)
            {
                deh_log("Assigned key %s => '%s'\n", key, newstring);
            }

            if (!key)
            {
                deh_log("Assigned '%.12s%s' to'%.12s%s' at key %s\n",
                        lookfor ? lookfor : "",
                        (lookfor && strlen(lookfor) > 12)
                            ? "..."
                            : "", // [FG] NULL dereference
                        newstring, (strlen(newstring) > 12) ? "..." : "",
                        deh_strlookup[i].lookup);
            }

            if (!key) // must have passed an old style string so showBEX
            {
                deh_log("*BEX FORMAT:\n%s=%s\n*END BEX\n",
                        deh_strlookup[i].lookup, dehReformatStr(newstring));
            }

            break;
        }
    }

    if (!found && lookfor)
    {
        for (i = 0; i < arrlen(banners); i++)
        {
            found = !strcasecmp(banners[i], lookfor);

            if (found)
            {
                const int version = DV_VANILLA; // We only support version 1.9 of Vanilla Doom
                char *deh_gamename = M_StringDuplicate(newstring);
                char *fmt = deh_gamename;

                // Expand "%i" in deh_gamename to include the Doom version
                // number We cannot use sprintf() here, because deh_gamename
                // isn't a string literal

                fmt = strstr(fmt, "%i");
                if (fmt)
                {
                    *fmt++ = '0' + version / 100;
                    memmove(fmt, fmt + 1, strlen(fmt));

                    fmt = strstr(fmt, "%i");
                    if (fmt)
                    {
                        *fmt++ = '0' + version % 100;
                        memmove(fmt, fmt + 1, strlen(fmt));
                    }
                }

                // Cut off trailing and leading spaces to get the basic name

                rstrip(deh_gamename);
                gamedescription = ptr_lstrip(deh_gamename);

                break;
            }
        }
    }

    if (!found)
    {
        deh_log("Could not find '%.12s'\n", key ? key : lookfor);
    }

    return found;
}

//
// deh_procBexSprites
//
// Supports sprite name substitutions without requiring use
// of the DeHackEd Text block
//
static void deh_procBexSprites(DEHFILE *fpin, char *line)
{
    char key[DEH_MAXKEYLEN];
    char inbuffer[DEH_BUFFERMAX];
    long value;   // All deh values are ints or longs
    char *strval; // holds the string value of the line
    char candidate[5];
    int match;

    deh_log("Processing sprite name substitution\n");

    strncpy(inbuffer, line, DEH_BUFFERMAX - 1);

    while (!deh_feof(fpin) && *inbuffer && (*inbuffer != ' '))
    {
        if (!deh_fgets(inbuffer, sizeof(inbuffer), fpin))
        {
            break;
        }
        if (*inbuffer == '#')
        {
            continue; // skip comment lines
        }
        lfstrip(inbuffer);
        if (!*inbuffer)
        {
            break; // killough 11/98
        }
        if (!deh_GetData(inbuffer, key, &value, &strval)) // returns true if ok
        {
            deh_log("Bad data pair in '%s'\n", inbuffer);
            continue;
        }
        // do it
        memset(candidate, 0, sizeof(candidate));
        strncpy(candidate, ptr_lstrip(strval), 4);
        if (strlen(candidate) != 4)
        {
            deh_log("Bad length for sprite name '%s'\n", candidate);
            continue;
        }

        match = dsdh_GetOriginalSpriteIndex(key);
        if (match >= 0)
        {
            deh_log("Substituting '%s' for sprite '%s'\n", candidate, key);
            sprnames[match] = strdup(candidate);
        }
    }
}

// ditto for sound names
static void deh_procBexSounds(DEHFILE *fpin, char *line)
{
    char key[DEH_MAXKEYLEN];
    char inbuffer[DEH_BUFFERMAX];
    long value;   // All deh values are ints or longs
    char *strval; // holds the string value of the line
    char candidate[7];
    int match;
    size_t len;

    deh_log("Processing sound name substitution\n");

    strncpy(inbuffer, line, DEH_BUFFERMAX - 1);

    while (!deh_feof(fpin) && *inbuffer && (*inbuffer != ' '))
    {
        if (!deh_fgets(inbuffer, sizeof(inbuffer), fpin))
        {
            break;
        }
        if (*inbuffer == '#')
        {
            continue; // skip comment lines
        }
        lfstrip(inbuffer);
        if (!*inbuffer)
        {
            break; // killough 11/98
        }
        if (!deh_GetData(inbuffer, key, &value, &strval)) // returns true if ok
        {
            deh_log("Bad data pair in '%s'\n", inbuffer);
            continue;
        }
        // do it
        memset(candidate, 0, 7);
        strncpy(candidate, ptr_lstrip(strval), 6);
        len = strlen(candidate);
        if (len < 1 || len > 6)
        {
            deh_log("Bad length for sound name '%s'\n", candidate);
            continue;
        }

        match = dsdh_GetOriginalSFXIndex(key);
        if (match >= 0)
        {
            deh_log("Substituting '%s' for sound '%s'\n", candidate, key);
            S_sfx[match].name = strdup(candidate);
        }
    }
}

// ====================================================================
// General utility function(s)
// ====================================================================

// ====================================================================
// dehReformatStr
// Purpose: Convert a string into a continuous string with embedded
//          linefeeds for "\n" sequences in the source string
// Args:    string -- the string to convert
// Returns: the converted string (converted in a static buffer)
//
static char *dehReformatStr(char *string)
{
    static char buff[DEH_BUFFERMAX]; // only processing the changed string,
    //  don't need double buffer
    char *s, *t;

    s = string; // source
    t = buff;   // target
    // let's play...

    while (*s)
    {
        if (*s == '\n')
        {
            ++s, *t++ = '\\', *t++ = 'n', *t++ = '\\', *t++ = '\n';
        }
        else
        {
            *t++ = *s++;
        }
    }
    *t = '\0';
    return buff;
}

// ====================================================================
// lfstrip
// Purpose: Strips CR/LF off the end of a string
// Args:    s -- the string to work on
// Returns: void -- the string is modified in place
//
// killough 10/98: only strip at end of line, not entire string

static void lfstrip(char *s) // strip the \r and/or \n off of a line
{
    char *p = s + strlen(s);
    while (p > s && (*--p == '\r' || *p == '\n'))
    {
        *p = '\0';
    }
}

// ====================================================================
// rstrip
// Purpose: Strips trailing blanks off a string
// Args:    s -- the string to work on
// Returns: void -- the string is modified in place
//
static void rstrip(char *s) // strip trailing whitespace
{
    char *p = s + strlen(s);       // killough 4/4/98: same here
    while (p > s && isspace(*--p)) // break on first non-whitespace
    {
        *p = '\0';
    }
}

// ====================================================================
// ptr_lstrip
// Purpose: Points past leading whitespace in a string
// Args:    s -- the string to work on
// Returns: char * pointing to the first nonblank character in the
//          string.  The original string is not changed.
//
static char *ptr_lstrip(char *p) // point past leading whitespace
{
    while (*p && isspace(*p))
    {
        p++;
    }
    return p;
}

// ====================================================================
// deh_GetData
// Purpose: Get a key and data pair from a passed string
// Args:    s -- the string to be examined
//          k -- a place to put the key
//          l -- pointer to a long integer to store the number
//          strval -- a pointer to the place in s where the number
//                    value comes from.  Pass NULL to not use this.
//          fpout  -- stream pointer to output log (DEHOUT.TXT)
// Notes:   Expects a key phrase, optional space, equal sign,
//          optional space and a value, mostly an int but treated
//          as a long just in case.  The passed pointer to hold
//          the key must be DEH_MAXKEYLEN in size.

static boolean deh_GetData(char *s, char *k, long *l, char **strval)
{
    char *t;                    // current char
    int val;                    // to hold value of pair
    char buffer[DEH_MAXKEYLEN]; // to hold key in progress
    boolean okrc = true;        // assume good unless we have problems
    int i;                      // iterator

    *buffer = '\0';
    val = 0; // defaults in case not otherwise set
    for (i = 0, t = s; *t && i < DEH_MAXKEYLEN; t++, i++)
    {
        if (*t == '=')
        {
            break;
        }
        buffer[i] = *t; // copy it
    }
    if (i > 0)
    {
        buffer[--i] = '\0'; // terminate the key before the '='
    }
    if (!*t) // end of string with no equal sign
    {
        okrc = false;
    }
    else
    {
        if (!*++t)
        {
            val = 0; // in case "thiskey =" with no value
            okrc = false;
        }
        // we've incremented t
        // val = strtol(t,NULL,0);  // killough 8/9/98: allow hex or octal input
        M_StrToInt(t, &val);
    }

    // go put the results in the passed pointers
    *l = val; // may be a faked zero

    // if spaces between key and equal sign, strip them
    strcpy(k, ptr_lstrip(buffer)); // could be a zero-length string

    if (strval != NULL) // pass NULL if you don't want this back
    {
        *strval = t; // pointer, has to be somewhere in s,
    }
    // even if pointing at the zero byte.

    return (okrc);
}

static const deh_bexptr_t null_bexptr = {{NULL}, "(NULL)"};

static boolean CheckSafeState(statenum_t state)
{
    int count = 0;

    for (statenum_t s = state; s != S_NULL; s = states[s].nextstate)
    {
        // [FG] recursive/nested states
        if (count++ >= 100)
        {
            return false;
        }

        // [crispy] a state with -1 tics never changes
        if (states[s].tics == -1)
        {
            break;
        }

        if (states[s].action.p2)
        {
            // [FG] A_Light*() considered harmless
            if (states[s].action.p2 == (actionf_p2)A_Light0
                || states[s].action.p2 == (actionf_p2)A_Light1
                || states[s].action.p2 == (actionf_p2)A_Light2)
            {
                continue;
            }
            else
            {
                return false;
            }
        }
    }

    return true;
}

void PostProcessDeh(void)
{
    int i, j;
    const deh_bexptr_t *bexptr_match;

    // sanity-check bfgcells and bfg ammopershot
    if (bfgcells_modified && weaponinfo[wp_bfg].intflags & WIF_ENABLEAPS
        && bfgcells != weaponinfo[wp_bfg].ammopershot)
    {
        I_Error("Mismatch between bfgcells and bfg ammo per shot "
                "modifications! Check your dehacked.");
    }

    if (processed_dehacked)
    {
        for (i = 0; i < num_states; i++)
        {
            bexptr_match = &null_bexptr;

            for (j = 0; deh_bexptrs[j].cptr.v != NULL; ++j)
            {
                if (states[i].action.v == deh_bexptrs[j].cptr.v)
                {
                    bexptr_match = &deh_bexptrs[j];
                    break;
                }
            }

            // ensure states don't use more mbf21 args than their
            // action pointer expects, for future-proofing's sake
            for (j = MAXSTATEARGS - 1; j >= bexptr_match->argcount; j--)
            {
                if (states[i].args[j] != 0)
                {
                    I_Error("Action %s on state %d expects no more than %d "
                            "nonzero args (%d found). Check your dehacked.",
                            bexptr_match->lookup, i, bexptr_match->argcount,
                            j + 1);
                }
            }

            // replace unset fields with default values
            for (; j >= 0; j--)
            {
                if (!(defined_codeptr_args[i] & (1 << j)))
                {
                    states[i].args[j] = bexptr_match->default_args[j];
                }
            }
        }
    }

    // [FG] fix desyncs by SSG-flash correction
    if (CheckSafeState(S_DSGUNFLASH1) && states[S_DSGUNFLASH1].tics == 5)
    {
        states[S_DSGUNFLASH1].tics = 4;
    }

    dsdh_FreeTables();
}

//---------------------------------------------------------------------
//
// $Log: d_deh.c,v $
// Revision 1.20  1998/06/01  22:30:38  thldrmn
// fix .acv pointer for new GCC version
//
// Revision 1.19  1998/05/17  09:39:48  thldrmn
// Bug fix to avoid processing last line twice
//
// Revision 1.17  1998/05/04  21:36:21  thldrmn
// commenting, reformatting and savegamename change
//
// Revision 1.16  1998/05/03  22:09:59  killough
// use p_inter.h for extern declarations and fix a pointer cast
//
// Revision 1.15  1998/04/26  14:46:24  thldrmn
// BEX code pointer additions
//
// Revision 1.14  1998/04/24  23:49:35  thldrmn
// Strings continuation fix
//
// Revision 1.13  1998/04/19  01:18:58  killough
// Change deh cheat code handling to use new cheat table
//
// Revision 1.12  1998/04/11  14:47:31  thldrmn
// Added include, fixed pars
//
// Revision 1.11  1998/04/10  06:49:15  killough
// Fix CVS stuff
//
// Revision 1.10  1998/04/09  09:17:00  thldrmn
// Update to text handling
//
// Revision 1.00  1998/04/07  04:43:59  ty
// First time with cvs revision info
//
//---------------------------------------------------------------------

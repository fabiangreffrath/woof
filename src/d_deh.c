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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "d_main.h"
#include "deh_bex_sounds.h"
#include "deh_bex_sprites.h"
#include "doomstat.h"
#include "doomtype.h"
#include "dstrings.h"
#include "info.h"
#include "m_cheat.h"
#include "m_misc.h"
#include "memio.h"
#include "sounds.h"

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
static boolean deh_procStringSub(char *, char *, char *);

static void deh_procCheat(DEHFILE *fpin, char *line);
static void deh_procText(DEHFILE *fpin, char *line);
static void deh_procStrings(DEHFILE *fpin, char *line);

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
    {"Cheat",     deh_procCheat          },
    {"Text",      deh_procText           }, // --  end of standard "deh" entries,

    // begin BOOM Extensions (BEX)

    {"[STRINGS]", deh_procStrings        }, // new string changes
};

static boolean processed_dehacked = false;


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
    int ix, iy;        // array indices
    char *p;           // utility pointer

    while (!deh_feof(fpin) && *inbuffer && (*inbuffer != ' '))
    {
        for (ix = 0; cheat[ix].sequence; ix++)
        {
            if (cheat[ix].deh_cheat) // killough 4/18/98: skip non-deh
            {
                if (!strcasecmp(key, cheat[ix].deh_cheat)) // found the cheat, ignored case
                {
                    for (int i = 0; cheat[i].sequence; i++)
                    {
                        if (cheat[i].when & not_deh
                            && !strncasecmp(cheat[i].sequence, cheat[iy].sequence, strlen(cheat[i].sequence))
                            && i != iy)
                        {
                            cheat[i].deh_modified = true;
                        }
                    }
                    cheat[iy].sequence = strdup(p);
                }
            }
        }
        deh_log("- %s\n", inbuffer);
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
    char inbuffer[DEH_BUFFERMAX * 2]; // can't use line -- double size buffer too.
    int i;                 // loop variable
    int fromlen, tolen;    // as specified on the text block line
    boolean found = false; // to allow early exit once found
    char *line2 = NULL;    // duplicate line for rerouting

    // if the from and to are 4, this may be a sprite rename.  Check it
    // against the array and process it as such if it matches.  Remember
    // that the original names are (and should remain) uppercase.
    // Future: this will be from a separate [SPRITES] block.
    if (fromlen == 4 && tolen == 4)
    {
        i = DEH_SpritesGetIndex(inbuffer);
        if (i >= 0)
        {
            sprnames[i] = strdup(sprnames[i]);
            found = true;
        }
    }

    if (!found && fromlen < 7 && tolen < 7)
        // lengths of music and sfx are 6 or shorter
    {
        i = DEH_SoundsGetIndex(inbuffer, (size_t)fromlen);
        if (i >= 0)
        {
            S_sfx[i].name = strdup(&inbuffer[fromlen]);
            found = true;
        }
        if (!found) // not yet
        {
            i = DEH_MusicGetIndex(inbuffer, fromlen);
            if (i >= 0)
            {
                S_music[i].name = strdup(&inbuffer[fromlen]);
                found = true;
            }
        } // end !found test
    }

    if (!found) // Nothing we want to handle here--see if strings can deal with it.
    {
        deh_procStringSub(NULL, inbuffer, line2);
    }
    free(line2); // may be NULL, ignored by free()
    return;
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
    // a time as needed
    // holds the final result of the string after concatenation
    static char *holdstring = NULL;
    boolean found = false; // looking for string continuation

    while (!deh_feof(fpin) && *inbuffer) /* && (*inbuffer != ' ') */
    {
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

    return found;
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

// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: d_deh.c,v 1.20 1998/06/01 22:30:38 thldrmn Exp $
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
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 
//  02111-1307, USA.
//
// Dehacked file support
// New for the TeamTNT "Boom" engine
//
// Author: Ty Halderman, TeamTNT
//
//--------------------------------------------------------------------

static const char
rcsid[] = "$Id: d_deh.c,v 1.20 1998/06/01 22:30:38 thldrmn Exp $";

// killough 5/2/98: fixed headers, removed rendunant external declarations:
#include "doomdef.h"
#include "doomstat.h"
#include "sounds.h"
#include "info.h"
#include "m_cheat.h"
#include "p_inter.h"
#include "g_game.h"
#include "d_think.h"
#include "w_wad.h"
#include "d_io.h"


// killough 10/98: new functions, to allow processing DEH files in-memory
// (e.g. from wads)

typedef struct {
  byte *inp, *lump; // Pointer to string or FILE
  long size;
} DEHFILE;

// killough 10/98: emulate IO whether input really comes from a file or not

// haleyjd: got rid of macros for MSCV

char *dehfgets(char *buf, size_t n, DEHFILE *fp)
{
  if (!fp->lump)                                     // If this is a real file,
    return fgets(buf, n, (FILE *) fp->inp);        // return regular fgets
  if (!n || !*fp->inp || fp->size<=0)                // If no more characters
    return NULL;
  if (n==1)
    fp->size--, *buf = *fp->inp++;
  else
    {                                                // copy buffer
      char *p = buf;
      while (n>1 && *fp->inp && fp->size &&
             (n--, fp->size--, *p++ = *fp->inp++) != '\n')
        ;
      *p = 0;
    }
  return buf;                                        // Return buffer pointer
}

int dehfeof(DEHFILE *fp)
{
  return !fp->lump ? feof((FILE *) fp->inp) : !*fp->inp || fp->size<=0;
}

int dehfgetc(DEHFILE *fp)
{
  return !fp->lump ? fgetc((FILE *) fp->inp) : fp->size > 0 ?
    fp->size--, *fp->inp++ : EOF;
}


// variables used in other routines
boolean deh_pars = FALSE; // in wi_stuff to allow pars in modified games

// #include "d_deh.h" -- we don't do that here but we declare the
// variables.  This externalizes everything that there is a string
// set for in the language files.  See d_deh.h for detailed comments,
// original English values etc.  These are set to the macro values,
// which are set by D_ENGLSH.H or D_FRENCH.H(etc).  BEX files are a
// better way of changing these strings globally by language.

// ====================================================================
// Any of these can be changed using the bex extensions
#include "dstrings.h"  // to get the initial values
char *s_D_DEVSTR    = D_DEVSTR;
char *s_D_CDROM     = D_CDROM;
char *s_PRESSKEY    = PRESSKEY;
char *s_PRESSYN     = PRESSYN;
char *s_QUITMSG     = QUITMSG;
char *s_LOADNET     = LOADNET;   // PRESSKEY; // killough 4/4/98:
char *s_QLOADNET    = QLOADNET;  // PRESSKEY;
char *s_QSAVESPOT   = QSAVESPOT; // PRESSKEY;
char *s_SAVEDEAD    = SAVEDEAD;  // PRESSKEY; // remove duplicate y/n
char *s_QSPROMPT    = QSPROMPT;  // PRESSYN;
char *s_QLPROMPT    = QLPROMPT;  // PRESSYN;
char *s_NEWGAME     = NEWGAME;   // PRESSKEY;
char *s_NIGHTMARE   = NIGHTMARE; // PRESSYN;
char *s_SWSTRING    = SWSTRING;  // PRESSKEY;
char *s_MSGOFF      = MSGOFF;
char *s_MSGON       = MSGON;
char *s_NETEND      = NETEND;    // PRESSKEY;
char *s_ENDGAME     = ENDGAME;   // PRESSYN; // killough 4/4/98: end
char *s_DOSY        = DOSY;
char *s_DETAILHI    = DETAILHI;
char *s_DETAILLO    = DETAILLO;
char *s_GAMMALVL0   = GAMMALVL0;
char *s_GAMMALVL1   = GAMMALVL1;
char *s_GAMMALVL2   = GAMMALVL2;
char *s_GAMMALVL3   = GAMMALVL3;
char *s_GAMMALVL4   = GAMMALVL4;
char *s_EMPTYSTRING = EMPTYSTRING;
char *s_GOTARMOR    = GOTARMOR;
char *s_GOTMEGA     = GOTMEGA;
char *s_GOTHTHBONUS = GOTHTHBONUS;
char *s_GOTARMBONUS = GOTARMBONUS;
char *s_GOTSTIM     = GOTSTIM;
char *s_GOTMEDINEED = GOTMEDINEED;
char *s_GOTMEDIKIT  = GOTMEDIKIT;
char *s_GOTSUPER    = GOTSUPER;
char *s_GOTBLUECARD = GOTBLUECARD;
char *s_GOTYELWCARD = GOTYELWCARD;
char *s_GOTREDCARD  = GOTREDCARD;
char *s_GOTBLUESKUL = GOTBLUESKUL;
char *s_GOTYELWSKUL = GOTYELWSKUL;
char *s_GOTREDSKULL = GOTREDSKULL;
char *s_GOTINVUL    = GOTINVUL;
char *s_GOTBERSERK  = GOTBERSERK;
char *s_GOTINVIS    = GOTINVIS;
char *s_GOTSUIT     = GOTSUIT;
char *s_GOTMAP      = GOTMAP;
char *s_GOTVISOR    = GOTVISOR;
char *s_GOTMSPHERE  = GOTMSPHERE;
char *s_GOTCLIP     = GOTCLIP;
char *s_GOTCLIPBOX  = GOTCLIPBOX;
char *s_GOTROCKET   = GOTROCKET;
char *s_GOTROCKBOX  = GOTROCKBOX;
char *s_GOTCELL     = GOTCELL;
char *s_GOTCELLBOX  = GOTCELLBOX;
char *s_GOTSHELLS   = GOTSHELLS;
char *s_GOTSHELLBOX = GOTSHELLBOX;
char *s_GOTBACKPACK = GOTBACKPACK;
char *s_GOTBFG9000  = GOTBFG9000;
char *s_GOTCHAINGUN = GOTCHAINGUN;
char *s_GOTCHAINSAW = GOTCHAINSAW;
char *s_GOTLAUNCHER = GOTLAUNCHER;
char *s_GOTPLASMA   = GOTPLASMA;
char *s_GOTSHOTGUN  = GOTSHOTGUN;
char *s_GOTSHOTGUN2 = GOTSHOTGUN2;
char *s_PD_BLUEO    = PD_BLUEO;
char *s_PD_REDO     = PD_REDO;
char *s_PD_YELLOWO  = PD_YELLOWO;
char *s_PD_BLUEK    = PD_BLUEK;
char *s_PD_REDK     = PD_REDK;
char *s_PD_YELLOWK  = PD_YELLOWK;
char *s_PD_BLUEC    = PD_BLUEC;
char *s_PD_REDC     = PD_REDC;
char *s_PD_YELLOWC  = PD_YELLOWC;
char *s_PD_BLUES    = PD_BLUES;
char *s_PD_REDS     = PD_REDS;
char *s_PD_YELLOWS  = PD_YELLOWS;
char *s_PD_ANY      = PD_ANY;
char *s_PD_ALL3     = PD_ALL3;
char *s_PD_ALL6     = PD_ALL6;
char *s_GGSAVED     = GGSAVED;
char *s_HUSTR_MSGU  = HUSTR_MSGU;
char *s_HUSTR_E1M1  = HUSTR_E1M1;
char *s_HUSTR_E1M2  = HUSTR_E1M2;
char *s_HUSTR_E1M3  = HUSTR_E1M3;
char *s_HUSTR_E1M4  = HUSTR_E1M4;
char *s_HUSTR_E1M5  = HUSTR_E1M5;
char *s_HUSTR_E1M6  = HUSTR_E1M6;
char *s_HUSTR_E1M7  = HUSTR_E1M7;
char *s_HUSTR_E1M8  = HUSTR_E1M8;
char *s_HUSTR_E1M9  = HUSTR_E1M9;
char *s_HUSTR_E2M1  = HUSTR_E2M1;
char *s_HUSTR_E2M2  = HUSTR_E2M2;
char *s_HUSTR_E2M3  = HUSTR_E2M3;
char *s_HUSTR_E2M4  = HUSTR_E2M4;
char *s_HUSTR_E2M5  = HUSTR_E2M5;
char *s_HUSTR_E2M6  = HUSTR_E2M6;
char *s_HUSTR_E2M7  = HUSTR_E2M7;
char *s_HUSTR_E2M8  = HUSTR_E2M8;
char *s_HUSTR_E2M9  = HUSTR_E2M9;
char *s_HUSTR_E3M1  = HUSTR_E3M1;
char *s_HUSTR_E3M2  = HUSTR_E3M2;
char *s_HUSTR_E3M3  = HUSTR_E3M3;
char *s_HUSTR_E3M4  = HUSTR_E3M4;
char *s_HUSTR_E3M5  = HUSTR_E3M5;
char *s_HUSTR_E3M6  = HUSTR_E3M6;
char *s_HUSTR_E3M7  = HUSTR_E3M7;
char *s_HUSTR_E3M8  = HUSTR_E3M8;
char *s_HUSTR_E3M9  = HUSTR_E3M9;
char *s_HUSTR_E4M1  = HUSTR_E4M1;
char *s_HUSTR_E4M2  = HUSTR_E4M2;
char *s_HUSTR_E4M3  = HUSTR_E4M3;
char *s_HUSTR_E4M4  = HUSTR_E4M4;
char *s_HUSTR_E4M5  = HUSTR_E4M5;
char *s_HUSTR_E4M6  = HUSTR_E4M6;
char *s_HUSTR_E4M7  = HUSTR_E4M7;
char *s_HUSTR_E4M8  = HUSTR_E4M8;
char *s_HUSTR_E4M9  = HUSTR_E4M9;
char *s_HUSTR_1     = HUSTR_1;
char *s_HUSTR_2     = HUSTR_2;
char *s_HUSTR_3     = HUSTR_3;
char *s_HUSTR_4     = HUSTR_4;
char *s_HUSTR_5     = HUSTR_5;
char *s_HUSTR_6     = HUSTR_6;
char *s_HUSTR_7     = HUSTR_7;
char *s_HUSTR_8     = HUSTR_8;
char *s_HUSTR_9     = HUSTR_9;
char *s_HUSTR_10    = HUSTR_10;
char *s_HUSTR_11    = HUSTR_11;
char *s_HUSTR_12    = HUSTR_12;
char *s_HUSTR_13    = HUSTR_13;
char *s_HUSTR_14    = HUSTR_14;
char *s_HUSTR_15    = HUSTR_15;
char *s_HUSTR_16    = HUSTR_16;
char *s_HUSTR_17    = HUSTR_17;
char *s_HUSTR_18    = HUSTR_18;
char *s_HUSTR_19    = HUSTR_19;
char *s_HUSTR_20    = HUSTR_20;
char *s_HUSTR_21    = HUSTR_21;
char *s_HUSTR_22    = HUSTR_22;
char *s_HUSTR_23    = HUSTR_23;
char *s_HUSTR_24    = HUSTR_24;
char *s_HUSTR_25    = HUSTR_25;
char *s_HUSTR_26    = HUSTR_26;
char *s_HUSTR_27    = HUSTR_27;
char *s_HUSTR_28    = HUSTR_28;
char *s_HUSTR_29    = HUSTR_29;
char *s_HUSTR_30    = HUSTR_30;
char *s_HUSTR_31    = HUSTR_31;
char *s_HUSTR_32    = HUSTR_32;
char *s_PHUSTR_1    = PHUSTR_1;
char *s_PHUSTR_2    = PHUSTR_2;
char *s_PHUSTR_3    = PHUSTR_3;
char *s_PHUSTR_4    = PHUSTR_4;
char *s_PHUSTR_5    = PHUSTR_5;
char *s_PHUSTR_6    = PHUSTR_6;
char *s_PHUSTR_7    = PHUSTR_7;
char *s_PHUSTR_8    = PHUSTR_8;
char *s_PHUSTR_9    = PHUSTR_9;
char *s_PHUSTR_10   = PHUSTR_10;
char *s_PHUSTR_11   = PHUSTR_11;
char *s_PHUSTR_12   = PHUSTR_12;
char *s_PHUSTR_13   = PHUSTR_13;
char *s_PHUSTR_14   = PHUSTR_14;
char *s_PHUSTR_15   = PHUSTR_15;
char *s_PHUSTR_16   = PHUSTR_16;
char *s_PHUSTR_17   = PHUSTR_17;
char *s_PHUSTR_18   = PHUSTR_18;
char *s_PHUSTR_19   = PHUSTR_19;
char *s_PHUSTR_20   = PHUSTR_20;
char *s_PHUSTR_21   = PHUSTR_21;
char *s_PHUSTR_22   = PHUSTR_22;
char *s_PHUSTR_23   = PHUSTR_23;
char *s_PHUSTR_24   = PHUSTR_24;
char *s_PHUSTR_25   = PHUSTR_25;
char *s_PHUSTR_26   = PHUSTR_26;
char *s_PHUSTR_27   = PHUSTR_27;
char *s_PHUSTR_28   = PHUSTR_28;
char *s_PHUSTR_29   = PHUSTR_29;
char *s_PHUSTR_30   = PHUSTR_30;
char *s_PHUSTR_31   = PHUSTR_31;
char *s_PHUSTR_32   = PHUSTR_32;
char *s_THUSTR_1    = THUSTR_1;
char *s_THUSTR_2    = THUSTR_2;
char *s_THUSTR_3    = THUSTR_3;
char *s_THUSTR_4    = THUSTR_4;
char *s_THUSTR_5    = THUSTR_5;
char *s_THUSTR_6    = THUSTR_6;
char *s_THUSTR_7    = THUSTR_7;
char *s_THUSTR_8    = THUSTR_8;
char *s_THUSTR_9    = THUSTR_9;
char *s_THUSTR_10   = THUSTR_10;
char *s_THUSTR_11   = THUSTR_11;
char *s_THUSTR_12   = THUSTR_12;
char *s_THUSTR_13   = THUSTR_13;
char *s_THUSTR_14   = THUSTR_14;
char *s_THUSTR_15   = THUSTR_15;
char *s_THUSTR_16   = THUSTR_16;
char *s_THUSTR_17   = THUSTR_17;
char *s_THUSTR_18   = THUSTR_18;
char *s_THUSTR_19   = THUSTR_19;
char *s_THUSTR_20   = THUSTR_20;
char *s_THUSTR_21   = THUSTR_21;
char *s_THUSTR_22   = THUSTR_22;
char *s_THUSTR_23   = THUSTR_23;
char *s_THUSTR_24   = THUSTR_24;
char *s_THUSTR_25   = THUSTR_25;
char *s_THUSTR_26   = THUSTR_26;
char *s_THUSTR_27   = THUSTR_27;
char *s_THUSTR_28   = THUSTR_28;
char *s_THUSTR_29   = THUSTR_29;
char *s_THUSTR_30   = THUSTR_30;
char *s_THUSTR_31   = THUSTR_31;
char *s_THUSTR_32   = THUSTR_32;
char *s_HUSTR_CHATMACRO1   = HUSTR_CHATMACRO1;
char *s_HUSTR_CHATMACRO2   = HUSTR_CHATMACRO2;
char *s_HUSTR_CHATMACRO3   = HUSTR_CHATMACRO3;
char *s_HUSTR_CHATMACRO4   = HUSTR_CHATMACRO4;
char *s_HUSTR_CHATMACRO5   = HUSTR_CHATMACRO5;
char *s_HUSTR_CHATMACRO6   = HUSTR_CHATMACRO6;
char *s_HUSTR_CHATMACRO7   = HUSTR_CHATMACRO7;
char *s_HUSTR_CHATMACRO8   = HUSTR_CHATMACRO8;
char *s_HUSTR_CHATMACRO9   = HUSTR_CHATMACRO9;
char *s_HUSTR_CHATMACRO0   = HUSTR_CHATMACRO0;
char *s_HUSTR_TALKTOSELF1  = HUSTR_TALKTOSELF1;
char *s_HUSTR_TALKTOSELF2  = HUSTR_TALKTOSELF2;
char *s_HUSTR_TALKTOSELF3  = HUSTR_TALKTOSELF3;
char *s_HUSTR_TALKTOSELF4  = HUSTR_TALKTOSELF4;
char *s_HUSTR_TALKTOSELF5  = HUSTR_TALKTOSELF5;
char *s_HUSTR_MESSAGESENT  = HUSTR_MESSAGESENT;
char *s_HUSTR_PLRGREEN     = HUSTR_PLRGREEN;
char *s_HUSTR_PLRINDIGO    = HUSTR_PLRINDIGO;
char *s_HUSTR_PLRBROWN     = HUSTR_PLRBROWN;
char *s_HUSTR_PLRRED       = HUSTR_PLRRED;
//char sc_HUSTR_KEYGREEN   = HUSTR_KEYGREEN;
//char sc_HUSTR_KEYINDIGO  = HUSTR_KEYINDIGO;
//char sc_HUSTR_KEYBROWN   = HUSTR_KEYBROWN;
//char sc_HUSTR_KEYRED     = HUSTR_KEYRED;
char *s_AMSTR_FOLLOWON     = AMSTR_FOLLOWON;
char *s_AMSTR_FOLLOWOFF    = AMSTR_FOLLOWOFF;
char *s_AMSTR_GRIDON       = AMSTR_GRIDON;
char *s_AMSTR_GRIDOFF      = AMSTR_GRIDOFF;
char *s_AMSTR_MARKEDSPOT   = AMSTR_MARKEDSPOT;
char *s_AMSTR_MARKSCLEARED = AMSTR_MARKSCLEARED;
char *s_STSTR_MUS          = STSTR_MUS;
char *s_STSTR_NOMUS        = STSTR_NOMUS;
char *s_STSTR_DQDON        = STSTR_DQDON;
char *s_STSTR_DQDOFF       = STSTR_DQDOFF;
char *s_STSTR_KFAADDED     = STSTR_KFAADDED;
char *s_STSTR_FAADDED      = STSTR_FAADDED;
char *s_STSTR_NCON         = STSTR_NCON;
char *s_STSTR_NCOFF        = STSTR_NCOFF;
char *s_STSTR_BEHOLD       = STSTR_BEHOLD;
char *s_STSTR_BEHOLDX      = STSTR_BEHOLDX;
char *s_STSTR_CHOPPERS     = STSTR_CHOPPERS;
char *s_STSTR_CLEV         = STSTR_CLEV;
char *s_STSTR_COMPON       = STSTR_COMPON;
char *s_STSTR_COMPOFF      = STSTR_COMPOFF;
char *s_E1TEXT     = E1TEXT;
char *s_E2TEXT     = E2TEXT;
char *s_E3TEXT     = E3TEXT;
char *s_E4TEXT     = E4TEXT;
char *s_C1TEXT     = C1TEXT;
char *s_C2TEXT     = C2TEXT;
char *s_C3TEXT     = C3TEXT;
char *s_C4TEXT     = C4TEXT;
char *s_C5TEXT     = C5TEXT;
char *s_C6TEXT     = C6TEXT;
char *s_P1TEXT     = P1TEXT;
char *s_P2TEXT     = P2TEXT;
char *s_P3TEXT     = P3TEXT;
char *s_P4TEXT     = P4TEXT;
char *s_P5TEXT     = P5TEXT;
char *s_P6TEXT     = P6TEXT;
char *s_T1TEXT     = T1TEXT;
char *s_T2TEXT     = T2TEXT;
char *s_T3TEXT     = T3TEXT;
char *s_T4TEXT     = T4TEXT;
char *s_T5TEXT     = T5TEXT;
char *s_T6TEXT     = T6TEXT;
char *s_CC_ZOMBIE  = CC_ZOMBIE;
char *s_CC_SHOTGUN = CC_SHOTGUN;
char *s_CC_HEAVY   = CC_HEAVY;
char *s_CC_IMP     = CC_IMP;
char *s_CC_DEMON   = CC_DEMON;
char *s_CC_LOST    = CC_LOST;
char *s_CC_CACO    = CC_CACO;
char *s_CC_HELL    = CC_HELL;
char *s_CC_BARON   = CC_BARON;
char *s_CC_ARACH   = CC_ARACH;
char *s_CC_PAIN    = CC_PAIN;
char *s_CC_REVEN   = CC_REVEN;
char *s_CC_MANCU   = CC_MANCU;
char *s_CC_ARCH    = CC_ARCH;
char *s_CC_SPIDER  = CC_SPIDER;
char *s_CC_CYBER   = CC_CYBER;
char *s_CC_HERO    = CC_HERO;
// Ty 03/30/98 - new substitutions for background textures
//               during int screens
char* bgflatE1     = "FLOOR4_8"; // end of DOOM Episode 1
char* bgflatE2     = "SFLR6_1";  // end of DOOM Episode 2
char* bgflatE3     = "MFLR8_4";  // end of DOOM Episode 3
char* bgflatE4     = "MFLR8_3";  // end of DOOM Episode 4
char* bgflat06     = "SLIME16";  // DOOM2 after MAP06
char* bgflat11     = "RROCK14";  // DOOM2 after MAP11
char* bgflat20     = "RROCK07";  // DOOM2 after MAP20
char* bgflat30     = "RROCK17";  // DOOM2 after MAP30
char* bgflat15     = "RROCK13";  // DOOM2 going MAP15 to MAP31
char* bgflat31     = "RROCK19";  // DOOM2 going MAP31 to MAP32
char* bgcastcall   = "BOSSBACK"; // Panel behind cast call

char* startup1     = "";  // blank lines are default and are not printed
char* startup2     = "";
char* startup3     = "";
char* startup4     = "";
char* startup5     = "";

// Ty 05/03/98 - externalized
char* savegamename;

// end d_deh.h variable declarations
// ====================================================================

// Do this for a lookup--the pointer (loaded above) is cross-referenced
// to a string key that is the same as the define above.  We will use
// strdups to set these new values that we read from the file, orphaning
// the original value set above.

typedef struct {
  char **ppstr;  // doubly indirect pointer to string
  char *lookup;  // pointer to lookup string name
} deh_strs;

deh_strs deh_strlookup[] = {
  {&s_D_DEVSTR,"D_DEVSTR"},
  {&s_D_CDROM,"D_CDROM"},
  {&s_PRESSKEY,"PRESSKEY"},
  {&s_PRESSYN,"PRESSYN"},
  {&s_QUITMSG,"QUITMSG"},
  {&s_LOADNET,"LOADNET"},
  {&s_QLOADNET,"QLOADNET"},
  {&s_QSAVESPOT,"QSAVESPOT"},
  {&s_SAVEDEAD,"SAVEDEAD"},
  {&s_QSPROMPT,"QSPROMPT"},
  {&s_QLPROMPT,"QLPROMPT"},
  {&s_NEWGAME,"NEWGAME"},
  {&s_NIGHTMARE,"NIGHTMARE"},
  {&s_SWSTRING,"SWSTRING"},
  {&s_MSGOFF,"MSGOFF"},
  {&s_MSGON,"MSGON"},
  {&s_NETEND,"NETEND"},
  {&s_ENDGAME,"ENDGAME"},
  {&s_DOSY,"DOSY"},
  {&s_DETAILHI,"DETAILHI"},
  {&s_DETAILLO,"DETAILLO"},
  {&s_GAMMALVL0,"GAMMALVL0"},
  {&s_GAMMALVL1,"GAMMALVL1"},
  {&s_GAMMALVL2,"GAMMALVL2"},
  {&s_GAMMALVL3,"GAMMALVL3"},
  {&s_GAMMALVL4,"GAMMALVL4"},
  {&s_EMPTYSTRING,"EMPTYSTRING"},
  {&s_GOTARMOR,"GOTARMOR"},
  {&s_GOTMEGA,"GOTMEGA"},
  {&s_GOTHTHBONUS,"GOTHTHBONUS"},
  {&s_GOTARMBONUS,"GOTARMBONUS"},
  {&s_GOTSTIM,"GOTSTIM"},
  {&s_GOTMEDINEED,"GOTMEDINEED"},
  {&s_GOTMEDIKIT,"GOTMEDIKIT"},
  {&s_GOTSUPER,"GOTSUPER"},
  {&s_GOTBLUECARD,"GOTBLUECARD"},
  {&s_GOTYELWCARD,"GOTYELWCARD"},
  {&s_GOTREDCARD,"GOTREDCARD"},
  {&s_GOTBLUESKUL,"GOTBLUESKUL"},
  {&s_GOTYELWSKUL,"GOTYELWSKUL"},
  {&s_GOTREDSKULL,"GOTREDSKULL"},
  {&s_GOTINVUL,"GOTINVUL"},
  {&s_GOTBERSERK,"GOTBERSERK"},
  {&s_GOTINVIS,"GOTINVIS"},
  {&s_GOTSUIT,"GOTSUIT"},
  {&s_GOTMAP,"GOTMAP"},
  {&s_GOTVISOR,"GOTVISOR"},
  {&s_GOTMSPHERE,"GOTMSPHERE"},
  {&s_GOTCLIP,"GOTCLIP"},
  {&s_GOTCLIPBOX,"GOTCLIPBOX"},
  {&s_GOTROCKET,"GOTROCKET"},
  {&s_GOTROCKBOX,"GOTROCKBOX"},
  {&s_GOTCELL,"GOTCELL"},
  {&s_GOTCELLBOX,"GOTCELLBOX"},
  {&s_GOTSHELLS,"GOTSHELLS"},
  {&s_GOTSHELLBOX,"GOTSHELLBOX"},
  {&s_GOTBACKPACK,"GOTBACKPACK"},
  {&s_GOTBFG9000,"GOTBFG9000"},
  {&s_GOTCHAINGUN,"GOTCHAINGUN"},
  {&s_GOTCHAINSAW,"GOTCHAINSAW"},
  {&s_GOTLAUNCHER,"GOTLAUNCHER"},
  {&s_GOTPLASMA,"GOTPLASMA"},
  {&s_GOTSHOTGUN,"GOTSHOTGUN"},
  {&s_GOTSHOTGUN2,"GOTSHOTGUN2"},
  {&s_PD_BLUEO,"PD_BLUEO"},
  {&s_PD_REDO,"PD_REDO"},
  {&s_PD_YELLOWO,"PD_YELLOWO"},
  {&s_PD_BLUEK,"PD_BLUEK"},
  {&s_PD_REDK,"PD_REDK"},
  {&s_PD_YELLOWK,"PD_YELLOWK"},
  {&s_PD_BLUEC,"PD_BLUEC"},
  {&s_PD_REDC,"PD_REDC"},
  {&s_PD_YELLOWC,"PD_YELLOWC"},
  {&s_PD_BLUES,"PD_BLUES"},
  {&s_PD_REDS,"PD_REDS"},
  {&s_PD_YELLOWS,"PD_YELLOWS"},
  {&s_PD_ANY,"PD_ANY"},
  {&s_PD_ALL3,"PD_ALL3"},
  {&s_PD_ALL6,"PD_ALL6"},
  {&s_GGSAVED,"GGSAVED"},
  {&s_HUSTR_MSGU,"HUSTR_MSGU"},
  {&s_HUSTR_E1M1,"HUSTR_E1M1"},
  {&s_HUSTR_E1M2,"HUSTR_E1M2"},
  {&s_HUSTR_E1M3,"HUSTR_E1M3"},
  {&s_HUSTR_E1M4,"HUSTR_E1M4"},
  {&s_HUSTR_E1M5,"HUSTR_E1M5"},
  {&s_HUSTR_E1M6,"HUSTR_E1M6"},
  {&s_HUSTR_E1M7,"HUSTR_E1M7"},
  {&s_HUSTR_E1M8,"HUSTR_E1M8"},
  {&s_HUSTR_E1M9,"HUSTR_E1M9"},
  {&s_HUSTR_E2M1,"HUSTR_E2M1"},
  {&s_HUSTR_E2M2,"HUSTR_E2M2"},
  {&s_HUSTR_E2M3,"HUSTR_E2M3"},
  {&s_HUSTR_E2M4,"HUSTR_E2M4"},
  {&s_HUSTR_E2M5,"HUSTR_E2M5"},
  {&s_HUSTR_E2M6,"HUSTR_E2M6"},
  {&s_HUSTR_E2M7,"HUSTR_E2M7"},
  {&s_HUSTR_E2M8,"HUSTR_E2M8"},
  {&s_HUSTR_E2M9,"HUSTR_E2M9"},
  {&s_HUSTR_E3M1,"HUSTR_E3M1"},
  {&s_HUSTR_E3M2,"HUSTR_E3M2"},
  {&s_HUSTR_E3M3,"HUSTR_E3M3"},
  {&s_HUSTR_E3M4,"HUSTR_E3M4"},
  {&s_HUSTR_E3M5,"HUSTR_E3M5"},
  {&s_HUSTR_E3M6,"HUSTR_E3M6"},
  {&s_HUSTR_E3M7,"HUSTR_E3M7"},
  {&s_HUSTR_E3M8,"HUSTR_E3M8"},
  {&s_HUSTR_E3M9,"HUSTR_E3M9"},
  {&s_HUSTR_E4M1,"HUSTR_E4M1"},
  {&s_HUSTR_E4M2,"HUSTR_E4M2"},
  {&s_HUSTR_E4M3,"HUSTR_E4M3"},
  {&s_HUSTR_E4M4,"HUSTR_E4M4"},
  {&s_HUSTR_E4M5,"HUSTR_E4M5"},
  {&s_HUSTR_E4M6,"HUSTR_E4M6"},
  {&s_HUSTR_E4M7,"HUSTR_E4M7"},
  {&s_HUSTR_E4M8,"HUSTR_E4M8"},
  {&s_HUSTR_E4M9,"HUSTR_E4M9"},
  {&s_HUSTR_1,"HUSTR_1"},
  {&s_HUSTR_2,"HUSTR_2"},
  {&s_HUSTR_3,"HUSTR_3"},
  {&s_HUSTR_4,"HUSTR_4"},
  {&s_HUSTR_5,"HUSTR_5"},
  {&s_HUSTR_6,"HUSTR_6"},
  {&s_HUSTR_7,"HUSTR_7"},
  {&s_HUSTR_8,"HUSTR_8"},
  {&s_HUSTR_9,"HUSTR_9"},
  {&s_HUSTR_10,"HUSTR_10"},
  {&s_HUSTR_11,"HUSTR_11"},
  {&s_HUSTR_12,"HUSTR_12"},
  {&s_HUSTR_13,"HUSTR_13"},
  {&s_HUSTR_14,"HUSTR_14"},
  {&s_HUSTR_15,"HUSTR_15"},
  {&s_HUSTR_16,"HUSTR_16"},
  {&s_HUSTR_17,"HUSTR_17"},
  {&s_HUSTR_18,"HUSTR_18"},
  {&s_HUSTR_19,"HUSTR_19"},
  {&s_HUSTR_20,"HUSTR_20"},
  {&s_HUSTR_21,"HUSTR_21"},
  {&s_HUSTR_22,"HUSTR_22"},
  {&s_HUSTR_23,"HUSTR_23"},
  {&s_HUSTR_24,"HUSTR_24"},
  {&s_HUSTR_25,"HUSTR_25"},
  {&s_HUSTR_26,"HUSTR_26"},
  {&s_HUSTR_27,"HUSTR_27"},
  {&s_HUSTR_28,"HUSTR_28"},
  {&s_HUSTR_29,"HUSTR_29"},
  {&s_HUSTR_30,"HUSTR_30"},
  {&s_HUSTR_31,"HUSTR_31"},
  {&s_HUSTR_32,"HUSTR_32"},
  {&s_PHUSTR_1,"PHUSTR_1"},
  {&s_PHUSTR_2,"PHUSTR_2"},
  {&s_PHUSTR_3,"PHUSTR_3"},
  {&s_PHUSTR_4,"PHUSTR_4"},
  {&s_PHUSTR_5,"PHUSTR_5"},
  {&s_PHUSTR_6,"PHUSTR_6"},
  {&s_PHUSTR_7,"PHUSTR_7"},
  {&s_PHUSTR_8,"PHUSTR_8"},
  {&s_PHUSTR_9,"PHUSTR_9"},
  {&s_PHUSTR_10,"PHUSTR_10"},
  {&s_PHUSTR_11,"PHUSTR_11"},
  {&s_PHUSTR_12,"PHUSTR_12"},
  {&s_PHUSTR_13,"PHUSTR_13"},
  {&s_PHUSTR_14,"PHUSTR_14"},
  {&s_PHUSTR_15,"PHUSTR_15"},
  {&s_PHUSTR_16,"PHUSTR_16"},
  {&s_PHUSTR_17,"PHUSTR_17"},
  {&s_PHUSTR_18,"PHUSTR_18"},
  {&s_PHUSTR_19,"PHUSTR_19"},
  {&s_PHUSTR_20,"PHUSTR_20"},
  {&s_PHUSTR_21,"PHUSTR_21"},
  {&s_PHUSTR_22,"PHUSTR_22"},
  {&s_PHUSTR_23,"PHUSTR_23"},
  {&s_PHUSTR_24,"PHUSTR_24"},
  {&s_PHUSTR_25,"PHUSTR_25"},
  {&s_PHUSTR_26,"PHUSTR_26"},
  {&s_PHUSTR_27,"PHUSTR_27"},
  {&s_PHUSTR_28,"PHUSTR_28"},
  {&s_PHUSTR_29,"PHUSTR_29"},
  {&s_PHUSTR_30,"PHUSTR_30"},
  {&s_PHUSTR_31,"PHUSTR_31"},
  {&s_PHUSTR_32,"PHUSTR_32"},
  {&s_THUSTR_1,"THUSTR_1"},
  {&s_THUSTR_2,"THUSTR_2"},
  {&s_THUSTR_3,"THUSTR_3"},
  {&s_THUSTR_4,"THUSTR_4"},
  {&s_THUSTR_5,"THUSTR_5"},
  {&s_THUSTR_6,"THUSTR_6"},
  {&s_THUSTR_7,"THUSTR_7"},
  {&s_THUSTR_8,"THUSTR_8"},
  {&s_THUSTR_9,"THUSTR_9"},
  {&s_THUSTR_10,"THUSTR_10"},
  {&s_THUSTR_11,"THUSTR_11"},
  {&s_THUSTR_12,"THUSTR_12"},
  {&s_THUSTR_13,"THUSTR_13"},
  {&s_THUSTR_14,"THUSTR_14"},
  {&s_THUSTR_15,"THUSTR_15"},
  {&s_THUSTR_16,"THUSTR_16"},
  {&s_THUSTR_17,"THUSTR_17"},
  {&s_THUSTR_18,"THUSTR_18"},
  {&s_THUSTR_19,"THUSTR_19"},
  {&s_THUSTR_20,"THUSTR_20"},
  {&s_THUSTR_21,"THUSTR_21"},
  {&s_THUSTR_22,"THUSTR_22"},
  {&s_THUSTR_23,"THUSTR_23"},
  {&s_THUSTR_24,"THUSTR_24"},
  {&s_THUSTR_25,"THUSTR_25"},
  {&s_THUSTR_26,"THUSTR_26"},
  {&s_THUSTR_27,"THUSTR_27"},
  {&s_THUSTR_28,"THUSTR_28"},
  {&s_THUSTR_29,"THUSTR_29"},
  {&s_THUSTR_30,"THUSTR_30"},
  {&s_THUSTR_31,"THUSTR_31"},
  {&s_THUSTR_32,"THUSTR_32"},
  {&s_HUSTR_CHATMACRO1,"HUSTR_CHATMACRO1"},
  {&s_HUSTR_CHATMACRO2,"HUSTR_CHATMACRO2"},
  {&s_HUSTR_CHATMACRO3,"HUSTR_CHATMACRO3"},
  {&s_HUSTR_CHATMACRO4,"HUSTR_CHATMACRO4"},
  {&s_HUSTR_CHATMACRO5,"HUSTR_CHATMACRO5"},
  {&s_HUSTR_CHATMACRO6,"HUSTR_CHATMACRO6"},
  {&s_HUSTR_CHATMACRO7,"HUSTR_CHATMACRO7"},
  {&s_HUSTR_CHATMACRO8,"HUSTR_CHATMACRO8"},
  {&s_HUSTR_CHATMACRO9,"HUSTR_CHATMACRO9"},
  {&s_HUSTR_CHATMACRO0,"HUSTR_CHATMACRO0"},
  {&s_HUSTR_TALKTOSELF1,"HUSTR_TALKTOSELF1"},
  {&s_HUSTR_TALKTOSELF2,"HUSTR_TALKTOSELF2"},
  {&s_HUSTR_TALKTOSELF3,"HUSTR_TALKTOSELF3"},
  {&s_HUSTR_TALKTOSELF4,"HUSTR_TALKTOSELF4"},
  {&s_HUSTR_TALKTOSELF5,"HUSTR_TALKTOSELF5"},
  {&s_HUSTR_MESSAGESENT,"HUSTR_MESSAGESENT"},
  {&s_HUSTR_PLRGREEN,"HUSTR_PLRGREEN"},
  {&s_HUSTR_PLRINDIGO,"HUSTR_PLRINDIGO"},
  {&s_HUSTR_PLRBROWN,"HUSTR_PLRBROWN"},
  {&s_HUSTR_PLRRED,"HUSTR_PLRRED"},
  //{c_HUSTR_KEYGREEN,"HUSTR_KEYGREEN"},
  //{c_HUSTR_KEYINDIGO,"HUSTR_KEYINDIGO"},
  //{c_HUSTR_KEYBROWN,"HUSTR_KEYBROWN"},
  //{c_HUSTR_KEYRED,"HUSTR_KEYRED"},
  {&s_AMSTR_FOLLOWON,"AMSTR_FOLLOWON"},
  {&s_AMSTR_FOLLOWOFF,"AMSTR_FOLLOWOFF"},
  {&s_AMSTR_GRIDON,"AMSTR_GRIDON"},
  {&s_AMSTR_GRIDOFF,"AMSTR_GRIDOFF"},
  {&s_AMSTR_MARKEDSPOT,"AMSTR_MARKEDSPOT"},
  {&s_AMSTR_MARKSCLEARED,"AMSTR_MARKSCLEARED"},
  {&s_STSTR_MUS,"STSTR_MUS"},
  {&s_STSTR_NOMUS,"STSTR_NOMUS"},
  {&s_STSTR_DQDON,"STSTR_DQDON"},
  {&s_STSTR_DQDOFF,"STSTR_DQDOFF"},
  {&s_STSTR_KFAADDED,"STSTR_KFAADDED"},
  {&s_STSTR_FAADDED,"STSTR_FAADDED"},
  {&s_STSTR_NCON,"STSTR_NCON"},
  {&s_STSTR_NCOFF,"STSTR_NCOFF"},
  {&s_STSTR_BEHOLD,"STSTR_BEHOLD"},
  {&s_STSTR_BEHOLDX,"STSTR_BEHOLDX"},
  {&s_STSTR_CHOPPERS,"STSTR_CHOPPERS"},
  {&s_STSTR_CLEV,"STSTR_CLEV"},
  {&s_STSTR_COMPON,"STSTR_COMPON"},
  {&s_STSTR_COMPOFF,"STSTR_COMPOFF"},
  {&s_E1TEXT,"E1TEXT"},
  {&s_E2TEXT,"E2TEXT"},
  {&s_E3TEXT,"E3TEXT"},
  {&s_E4TEXT,"E4TEXT"},
  {&s_C1TEXT,"C1TEXT"},
  {&s_C2TEXT,"C2TEXT"},
  {&s_C3TEXT,"C3TEXT"},
  {&s_C4TEXT,"C4TEXT"},
  {&s_C5TEXT,"C5TEXT"},
  {&s_C6TEXT,"C6TEXT"},
  {&s_P1TEXT,"P1TEXT"},
  {&s_P2TEXT,"P2TEXT"},
  {&s_P3TEXT,"P3TEXT"},
  {&s_P4TEXT,"P4TEXT"},
  {&s_P5TEXT,"P5TEXT"},
  {&s_P6TEXT,"P6TEXT"},
  {&s_T1TEXT,"T1TEXT"},
  {&s_T2TEXT,"T2TEXT"},
  {&s_T3TEXT,"T3TEXT"},
  {&s_T4TEXT,"T4TEXT"},
  {&s_T5TEXT,"T5TEXT"},
  {&s_T6TEXT,"T6TEXT"},
  {&s_CC_ZOMBIE,"CC_ZOMBIE"},
  {&s_CC_SHOTGUN,"CC_SHOTGUN"},
  {&s_CC_HEAVY,"CC_HEAVY"},
  {&s_CC_IMP,"CC_IMP"},
  {&s_CC_DEMON,"CC_DEMON"},
  {&s_CC_LOST,"CC_LOST"},
  {&s_CC_CACO,"CC_CACO"},
  {&s_CC_HELL,"CC_HELL"},
  {&s_CC_BARON,"CC_BARON"},
  {&s_CC_ARACH,"CC_ARACH"},
  {&s_CC_PAIN,"CC_PAIN"},
  {&s_CC_REVEN,"CC_REVEN"},
  {&s_CC_MANCU,"CC_MANCU"},
  {&s_CC_ARCH,"CC_ARCH"},
  {&s_CC_SPIDER,"CC_SPIDER"},
  {&s_CC_CYBER,"CC_CYBER"},
  {&s_CC_HERO,"CC_HERO"},
  {&bgflatE1,"BGFLATE1"},
  {&bgflatE2,"BGFLATE2"},
  {&bgflatE3,"BGFLATE3"},
  {&bgflatE4,"BGFLATE4"},
  {&bgflat06,"BGFLAT06"},
  {&bgflat11,"BGFLAT11"},
  {&bgflat20,"BGFLAT20"},
  {&bgflat30,"BGFLAT30"},
  {&bgflat15,"BGFLAT15"},
  {&bgflat31,"BGFLAT31"},
  {&bgcastcall,"BGCASTCALL"},
  // Ty 04/08/98 - added 5 general purpose startup announcement
  // strings for hacker use.  See m_menu.c
  {&startup1,"STARTUP1"},
  {&startup2,"STARTUP2"},
  {&startup3,"STARTUP3"},
  {&startup4,"STARTUP4"},
  {&startup5,"STARTUP5"},
  {&savegamename,"SAVEGAMENAME"},  // Ty 05/03/98
};

static int deh_numstrlookup =
sizeof(deh_strlookup)/sizeof(deh_strlookup[0]);

char *deh_newlevel = "NEWLEVEL";

char **mapnames[] =  // DOOM shareware/registered/retail (Ultimate) names.
{
  &s_HUSTR_E1M1,
  &s_HUSTR_E1M2,
  &s_HUSTR_E1M3,
  &s_HUSTR_E1M4,
  &s_HUSTR_E1M5,
  &s_HUSTR_E1M6,
  &s_HUSTR_E1M7,
  &s_HUSTR_E1M8,
  &s_HUSTR_E1M9,

  &s_HUSTR_E2M1,
  &s_HUSTR_E2M2,
  &s_HUSTR_E2M3,
  &s_HUSTR_E2M4,
  &s_HUSTR_E2M5,
  &s_HUSTR_E2M6,
  &s_HUSTR_E2M7,
  &s_HUSTR_E2M8,
  &s_HUSTR_E2M9,

  &s_HUSTR_E3M1,
  &s_HUSTR_E3M2,
  &s_HUSTR_E3M3,
  &s_HUSTR_E3M4,
  &s_HUSTR_E3M5,
  &s_HUSTR_E3M6,
  &s_HUSTR_E3M7,
  &s_HUSTR_E3M8,
  &s_HUSTR_E3M9,

  &s_HUSTR_E4M1,
  &s_HUSTR_E4M2,
  &s_HUSTR_E4M3,
  &s_HUSTR_E4M4,
  &s_HUSTR_E4M5,
  &s_HUSTR_E4M6,
  &s_HUSTR_E4M7,
  &s_HUSTR_E4M8,
  &s_HUSTR_E4M9,

  &deh_newlevel,  // spares?  Unused.
  &deh_newlevel,
  &deh_newlevel,
  &deh_newlevel,
  &deh_newlevel,
  &deh_newlevel,
  &deh_newlevel,
  &deh_newlevel,
  &deh_newlevel
};

char **mapnames2[] = // DOOM 2 map names.
{
  &s_HUSTR_1,
  &s_HUSTR_2,
  &s_HUSTR_3,
  &s_HUSTR_4,
  &s_HUSTR_5,
  &s_HUSTR_6,
  &s_HUSTR_7,
  &s_HUSTR_8,
  &s_HUSTR_9,
  &s_HUSTR_10,
  &s_HUSTR_11,

  &s_HUSTR_12,
  &s_HUSTR_13,
  &s_HUSTR_14,
  &s_HUSTR_15,
  &s_HUSTR_16,
  &s_HUSTR_17,
  &s_HUSTR_18,
  &s_HUSTR_19,
  &s_HUSTR_20,

  &s_HUSTR_21,
  &s_HUSTR_22,
  &s_HUSTR_23,
  &s_HUSTR_24,
  &s_HUSTR_25,
  &s_HUSTR_26,
  &s_HUSTR_27,
  &s_HUSTR_28,
  &s_HUSTR_29,
  &s_HUSTR_30,
  &s_HUSTR_31,
  &s_HUSTR_32,
};


char **mapnamesp[] = // Plutonia WAD map names.
{
  &s_PHUSTR_1,
  &s_PHUSTR_2,
  &s_PHUSTR_3,
  &s_PHUSTR_4,
  &s_PHUSTR_5,
  &s_PHUSTR_6,
  &s_PHUSTR_7,
  &s_PHUSTR_8,
  &s_PHUSTR_9,
  &s_PHUSTR_10,
  &s_PHUSTR_11,

  &s_PHUSTR_12,
  &s_PHUSTR_13,
  &s_PHUSTR_14,
  &s_PHUSTR_15,
  &s_PHUSTR_16,
  &s_PHUSTR_17,
  &s_PHUSTR_18,
  &s_PHUSTR_19,
  &s_PHUSTR_20,

  &s_PHUSTR_21,
  &s_PHUSTR_22,
  &s_PHUSTR_23,
  &s_PHUSTR_24,
  &s_PHUSTR_25,
  &s_PHUSTR_26,
  &s_PHUSTR_27,
  &s_PHUSTR_28,
  &s_PHUSTR_29,
  &s_PHUSTR_30,
  &s_PHUSTR_31,
  &s_PHUSTR_32,
};


char **mapnamest[] = // TNT WAD map names.
{
  &s_THUSTR_1,
  &s_THUSTR_2,
  &s_THUSTR_3,
  &s_THUSTR_4,
  &s_THUSTR_5,
  &s_THUSTR_6,
  &s_THUSTR_7,
  &s_THUSTR_8,
  &s_THUSTR_9,
  &s_THUSTR_10,
  &s_THUSTR_11,

  &s_THUSTR_12,
  &s_THUSTR_13,
  &s_THUSTR_14,
  &s_THUSTR_15,
  &s_THUSTR_16,
  &s_THUSTR_17,
  &s_THUSTR_18,
  &s_THUSTR_19,
  &s_THUSTR_20,

  &s_THUSTR_21,
  &s_THUSTR_22,
  &s_THUSTR_23,
  &s_THUSTR_24,
  &s_THUSTR_25,
  &s_THUSTR_26,
  &s_THUSTR_27,
  &s_THUSTR_28,
  &s_THUSTR_29,
  &s_THUSTR_30,
  &s_THUSTR_31,
  &s_THUSTR_32,
};

// Function prototypes
void    lfstrip(char *);     // strip the \r and/or \n off of a line
void    rstrip(char *);      // strip trailing whitespace
char *  ptr_lstrip(char *);  // point past leading whitespace
boolean deh_GetData(char *, char *, long *, char **, FILE *);
boolean deh_procStringSub(char *, char *, char *, FILE *);
char *  dehReformatStr(char *);

// Prototypes for block processing functions
// Pointers to these functions are used as the blocks are encountered.

void deh_procThing(DEHFILE *, FILE*, char *);
void deh_procFrame(DEHFILE *, FILE*, char *);
void deh_procPointer(DEHFILE *, FILE*, char *);
void deh_procSounds(DEHFILE *, FILE*, char *);
void deh_procAmmo(DEHFILE *, FILE*, char *);
void deh_procWeapon(DEHFILE *, FILE*, char *);
void deh_procSprite(DEHFILE *, FILE*, char *);
void deh_procCheat(DEHFILE *, FILE*, char *);
void deh_procMisc(DEHFILE *, FILE*, char *);
void deh_procText(DEHFILE *, FILE*, char *);
void deh_procPars(DEHFILE *, FILE*, char *);
void deh_procStrings(DEHFILE *, FILE*, char *);
void deh_procError(DEHFILE *, FILE*, char *);
void deh_procBexCodePointers(DEHFILE *, FILE*, char *);

// Structure deh_block is used to hold the block names that can
// be encountered, and the routines to use to decipher them

typedef struct
{
  char *key;       // a mnemonic block code name
  void (*const fptr)(DEHFILE *, FILE*, char *); // handler
} deh_block;

#define DEH_BUFFERMAX 1024 // input buffer area size, hardcodedfor now
// killough 8/9/98: make DEH_BLOCKMAX self-adjusting
#define DEH_BLOCKMAX (sizeof deh_blocks/sizeof*deh_blocks)  // size of array
#define DEH_MAXKEYLEN 32 // as much of any key as we'll look at
#define DEH_MOBJINFOMAX 23 // number of ints in the mobjinfo_t structure (!)

// Put all the block header values, and the function to be called when that
// one is encountered, in this array:
deh_block deh_blocks[] = {
  /* 0 */  {"Thing",deh_procThing},
  /* 1 */  {"Frame",deh_procFrame},
  /* 2 */  {"Pointer",deh_procPointer},
  /* 3 */  {"Sound",deh_procSounds},  // Ty 03/16/98 corrected from "Sounds"
  /* 4 */  {"Ammo",deh_procAmmo},
  /* 5 */  {"Weapon",deh_procWeapon},
  /* 6 */  {"Sprite",deh_procSprite},
  /* 7 */  {"Cheat",deh_procCheat},
  /* 8 */  {"Misc",deh_procMisc},
  /* 9 */  {"Text",deh_procText},  // --  end of standard "deh" entries,

  //     begin BOOM Extensions (BEX)

  /* 10 */ {"[STRINGS]",deh_procStrings}, // new string changes
  /* 11 */ {"[PARS]",deh_procPars}, // alternative block marker
  /* 12 */ {"[CODEPTR]",deh_procBexCodePointers}, // bex codepointers by mnemonic
  /* 13 */ {"",deh_procError} // dummy to handle anything else
};

// flag to skip included deh-style text, used with INCLUDE NOTEXT directive
static boolean includenotext = false;

// MOBJINFO - Dehacked block name = "Thing"
// Usage: Thing nn (name)
// These are for mobjinfo_t types.  Each is an integer
// within the structure, so we can use index of the string in this
// array to offset by sizeof(int) into the mobjinfo_t array at [nn]
// * things are base zero but dehacked considers them to start at #1. ***

char *deh_mobjinfo[DEH_MOBJINFOMAX] =
{
  "ID #",                // .doomednum
  "Initial frame",       // .spawnstate
  "Hit points",          // .spawnhealth
  "First moving frame",  // .seestate
  "Alert sound",         // .seesound
  "Reaction time",       // .reactiontime
  "Attack sound",        // .attacksound
  "Injury frame",        // .painstate
  "Pain chance",         // .painchance
  "Pain sound",          // .painsound
  "Close attack frame",  // .meleestate
  "Far attack frame",    // .missilestate
  "Death frame",         // .deathstate
  "Exploding frame",     // .xdeathstate
  "Death sound",         // .deathsound
  "Speed",               // .speed
  "Width",               // .radius
  "Height",              // .height
  "Mass",                // .mass
  "Missile damage",      // .damage
  "Action sound",        // .activesound
  "Bits",                // .flags
  "Respawn frame"        // .raisestate
};

// Strings that are used to indicate flags ("Bits" in mobjinfo)
// This is an array of bit masks that are related to p_mobj.h
// values, using the smae names without the MF_ in front.
// Ty 08/27/98 new code
//
// killough 10/98:
//
// Convert array to struct to allow multiple values, make array size variable

#define DEH_MOBJFLAGMAX (sizeof deh_mobjflags/sizeof*deh_mobjflags)

struct { 
  char *name;
  long value;
} deh_mobjflags[] = {
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

// STATE - Dehacked block name = "Frame" and "Pointer"
// Usage: Frame nn
// Usage: Pointer nn (Frame nn)
// These are indexed separately, for lookup to the actual
// function pointers.  Here we'll take whatever Dehacked gives
// us and go from there.  The (Frame nn) after the pointer is the
// real place to put this value.  The "Pointer" value is an xref
// that Dehacked uses and is useless to us.
// * states are base zero and have a dummy #0 (TROO)

char *deh_state[] =
{
  "Sprite number",    // .sprite (spritenum_t) // an enum
  "Sprite subnumber", // .frame (long)
  "Duration",         // .tics (long)
  "Next frame",       // .nextstate (statenum_t)
  // This is set in a separate "Pointer" block from Dehacked
  "Codep Frame",      // pointer to first use of action (actionf_t)
  "Unknown 1",        // .misc1 (long)
  "Unknown 2"         // .misc2 (long)
};

// SFXINFO_STRUCT - Dehacked block name = "Sounds"
// Sound effects, typically not changed (redirected, and new sfx put
// into the pwad, but not changed here.  Can you tell that Gregdidn't
// know what they were for, mostly?  Can you tell that I don't either?
// Mostly I just put these into the same slots as they are in the struct.
// This may not be supported in our -deh option if it doesn't make sense by then.

// * sounds are base zero but have a dummy #0

char *deh_sfxinfo[] =
{
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

char *deh_sprite[] =
{
  "Offset"      // supposed to be the offset into the text section
};

// AMMO - Dehacked block name = "Ammo"
// usage = Ammo n (name)
// Ammo information for the few types of ammo

char *deh_ammo[] =
{
  "Max ammo",   // maxammo[]
  "Per ammo"    // clipammo[]
};

// WEAPONS - Dehacked block name = "Weapon"
// Usage: Weapon nn (name)
// Basically a list of frames and what kind of ammo (see above)it uses.

char *deh_weapon[] =
{
  "Ammo type",      // .ammo
  "Deselect frame", // .upstate
  "Select frame",   // .downstate
  "Bobbing frame",  // .readystate
  "Shooting frame", // .atkstate
  "Firing frame"    // .flashstate
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

char *deh_misc[] =
{
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

extern void A_Light0();
extern void A_WeaponReady();
extern void A_Lower();
extern void A_Raise();
extern void A_Punch();
extern void A_ReFire();
extern void A_FirePistol();
extern void A_Light1();
extern void A_FireShotgun();
extern void A_Light2();
extern void A_FireShotgun2();
extern void A_CheckReload();
extern void A_OpenShotgun2();
extern void A_LoadShotgun2();
extern void A_CloseShotgun2();
extern void A_FireCGun();
extern void A_GunFlash();
extern void A_FireMissile();
extern void A_Saw();
extern void A_FirePlasma();
extern void A_BFGsound();
extern void A_FireBFG();
extern void A_BFGSpray();
extern void A_Explode();
extern void A_Pain();
extern void A_PlayerScream();
extern void A_Fall();
extern void A_XScream();
extern void A_Look();
extern void A_Chase();
extern void A_FaceTarget();
extern void A_PosAttack();
extern void A_Scream();
extern void A_SPosAttack();
extern void A_VileChase();
extern void A_VileStart();
extern void A_VileTarget();
extern void A_VileAttack();
extern void A_StartFire();
extern void A_Fire();
extern void A_FireCrackle();
extern void A_Tracer();
extern void A_SkelWhoosh();
extern void A_SkelFist();
extern void A_SkelMissile();
extern void A_FatRaise();
extern void A_FatAttack1();
extern void A_FatAttack2();
extern void A_FatAttack3();
extern void A_BossDeath();
extern void A_CPosAttack();
extern void A_CPosRefire();
extern void A_TroopAttack();
extern void A_SargAttack();
extern void A_HeadAttack();
extern void A_BruisAttack();
extern void A_SkullAttack();
extern void A_Metal();
extern void A_SpidRefire();
extern void A_BabyMetal();
extern void A_BspiAttack();
extern void A_Hoof();
extern void A_CyberAttack();
extern void A_PainAttack();
extern void A_PainDie();
extern void A_KeenDie();
extern void A_BrainPain();
extern void A_BrainScream();
extern void A_BrainDie();
extern void A_BrainAwake();
extern void A_BrainSpit();
extern void A_SpawnSound();
extern void A_SpawnFly();
extern void A_BrainExplode();
extern void A_Detonate();        // killough 8/9/98
extern void A_Mushroom();        // killough 10/98
extern void A_Die();             // killough 11/98
extern void A_Spawn();           // killough 11/98
extern void A_Turn();            // killough 11/98
extern void A_Face();            // killough 11/98
extern void A_Scratch();         // killough 11/98
extern void A_PlaySound();       // killough 11/98
extern void A_RandomJump();      // killough 11/98
extern void A_LineEffect();      // killough 11/98

typedef struct {
  actionf_t cptr;  // actual pointer to the subroutine
  char *lookup;  // mnemonic lookup string to be specified in BEX
} deh_bexptr;

deh_bexptr deh_bexptrs[] =
{
  {A_Light0,         "A_Light0"},
  {A_WeaponReady,    "A_WeaponReady"},
  {A_Lower,          "A_Lower"},
  {A_Raise,          "A_Raise"},
  {A_Punch,          "A_Punch"},
  {A_ReFire,         "A_ReFire"},
  {A_FirePistol,     "A_FirePistol"},
  {A_Light1,         "A_Light1"},
  {A_FireShotgun,    "A_FireShotgun"},
  {A_Light2,         "A_Light2"},
  {A_FireShotgun2,   "A_FireShotgun2"},
  {A_CheckReload,    "A_CheckReload"},
  {A_OpenShotgun2,   "A_OpenShotgun2"},
  {A_LoadShotgun2,   "A_LoadShotgun2"},
  {A_CloseShotgun2,  "A_CloseShotgun2"},
  {A_FireCGun,       "A_FireCGun"},
  {A_GunFlash,       "A_GunFlash"},
  {A_FireMissile,    "A_FireMissile"},
  {A_Saw,            "A_Saw"},
  {A_FirePlasma,     "A_FirePlasma"},
  {A_BFGsound,       "A_BFGsound"},
  {A_FireBFG,        "A_FireBFG"},
  {A_BFGSpray,       "A_BFGSpray"},
  {A_Explode,        "A_Explode"},
  {A_Pain,           "A_Pain"},
  {A_PlayerScream,   "A_PlayerScream"},
  {A_Fall,           "A_Fall"},
  {A_XScream,        "A_XScream"},
  {A_Look,           "A_Look"},
  {A_Chase,          "A_Chase"},
  {A_FaceTarget,     "A_FaceTarget"},
  {A_PosAttack,      "A_PosAttack"},
  {A_Scream,         "A_Scream"},
  {A_SPosAttack,     "A_SPosAttack"},
  {A_VileChase,      "A_VileChase"},
  {A_VileStart,      "A_VileStart"},
  {A_VileTarget,     "A_VileTarget"},
  {A_VileAttack,     "A_VileAttack"},
  {A_StartFire,      "A_StartFire"},
  {A_Fire,           "A_Fire"},
  {A_FireCrackle,    "A_FireCrackle"},
  {A_Tracer,         "A_Tracer"},
  {A_SkelWhoosh,     "A_SkelWhoosh"},
  {A_SkelFist,       "A_SkelFist"},
  {A_SkelMissile,    "A_SkelMissile"},
  {A_FatRaise,       "A_FatRaise"},
  {A_FatAttack1,     "A_FatAttack1"},
  {A_FatAttack2,     "A_FatAttack2"},
  {A_FatAttack3,     "A_FatAttack3"},
  {A_BossDeath,      "A_BossDeath"},
  {A_CPosAttack,     "A_CPosAttack"},
  {A_CPosRefire,     "A_CPosRefire"},
  {A_TroopAttack,    "A_TroopAttack"},
  {A_SargAttack,     "A_SargAttack"},
  {A_HeadAttack,     "A_HeadAttack"},
  {A_BruisAttack,    "A_BruisAttack"},
  {A_SkullAttack,    "A_SkullAttack"},
  {A_Metal,          "A_Metal"},
  {A_SpidRefire,     "A_SpidRefire"},
  {A_BabyMetal,      "A_BabyMetal"},
  {A_BspiAttack,     "A_BspiAttack"},
  {A_Hoof,           "A_Hoof"},
  {A_CyberAttack,    "A_CyberAttack"},
  {A_PainAttack,     "A_PainAttack"},
  {A_PainDie,        "A_PainDie"},
  {A_KeenDie,        "A_KeenDie"},
  {A_BrainPain,      "A_BrainPain"},
  {A_BrainScream,    "A_BrainScream"},
  {A_BrainDie,       "A_BrainDie"},
  {A_BrainAwake,     "A_BrainAwake"},
  {A_BrainSpit,      "A_BrainSpit"},
  {A_SpawnSound,     "A_SpawnSound"},
  {A_SpawnFly,       "A_SpawnFly"},
  {A_BrainExplode,   "A_BrainExplode"},
  {A_Detonate,       "A_Detonate"},       // killough 8/9/98
  {A_Mushroom,       "A_Mushroom"},       // killough 10/98
  {A_Die,            "A_Die"},            // killough 11/98
  {A_Spawn,          "A_Spawn"},          // killough 11/98
  {A_Turn,           "A_Turn"},           // killough 11/98
  {A_Face,           "A_Face"},           // killough 11/98
  {A_Scratch,        "A_Scratch"},        // killough 11/98
  {A_PlaySound,      "A_PlaySound"},      // killough 11/98
  {A_RandomJump,     "A_RandomJump"},     // killough 11/98
  {A_LineEffect,     "A_LineEffect"},     // killough 11/98

  // This NULL entry must be the last in the list
  {NULL,             "A_NULL"},  // Ty 05/16/98
};

// to hold startup code pointers from INFO.C
actionf_t deh_codeptr[NUMSTATES];

// ====================================================================
// ProcessDehFile
// Purpose: Read and process a DEH or BEX file
// Args:    filename    -- name of the DEH/BEX file
//          outfilename -- output file (DEHOUT.TXT), appended to here
// Returns: void
//
// killough 10/98:
// substantially modified to allow input from wad lumps instead of .deh files.

void ProcessDehFile(char *filename, char *outfilename, int lumpnum)
{
  static FILE *fileout;       // In case -dehout was used
  DEHFILE infile, *filein = &infile;    // killough 10/98
  char inbuffer[DEH_BUFFERMAX];  // Place to put the primary infostring

  // Open output file if we're writing output
  if (outfilename && *outfilename && !fileout)
    {
      static boolean firstfile = true; // to allow append to output log
      if (!strcmp(outfilename, "-"))
        fileout = stdout;
      else
        if (!(fileout=fopen(outfilename, firstfile ? "wt" : "at")))
          {
            printf("Could not open -dehout file %s\n... using stdout.\n",
                   outfilename);
            fileout = stdout;
          }
      firstfile = false;
    }

  // killough 10/98: allow DEH files to come from wad lumps

  if (filename)
    {
      if (!(infile.inp = (void *) fopen(filename,"rt")))
        {
          printf("-deh file %s not found\n",filename);
          return;  // should be checked up front anyway
        }
      infile.lump = NULL;
    }
  else  // DEH file comes from lump indicated by third argument
    {
      infile.size = W_LumpLength(lumpnum);
      infile.inp = infile.lump = W_CacheLumpNum(lumpnum, PU_STATIC);
      filename = "(WAD)";
    }

  printf("Loading DEH file %s\n",filename);
  if (fileout) fprintf(fileout,"\nLoading DEH file %s\n\n",filename);

  {
    static int i;   // killough 10/98: only run once, by keeping index static
    for (; i<NUMSTATES; i++)  // remember what they start as for deh xref
      deh_codeptr[i] = states[i].action;
  }

  // loop until end of file

  while (dehfgets(inbuffer,sizeof(inbuffer),filein))
    {
      int i;

      lfstrip(inbuffer);
      if (fileout) fprintf(fileout,"Line='%s'\n",inbuffer);
      if (!*inbuffer || *inbuffer == '#' || *inbuffer == ' ')
        continue; /* Blank line or comment line */

      // -- If DEH_BLOCKMAX is set right, the processing is independently
      // -- handled based on data in the deh_blocks[] structure array

      // killough 10/98: INCLUDE code rewritten to allow arbitrary nesting,
      // and to greatly simplify code, fix memory leaks, other bugs

      if (!strnicmp(inbuffer,"INCLUDE",7)) // include a file
        {
          // preserve state while including a file
          // killough 10/98: moved to here

          char *nextfile;
          boolean oldnotext = includenotext;       // killough 10/98

          // killough 10/98: exclude if inside wads (only to discourage
          // the practice, since the code could otherwise handle it)

          if (infile.lump)
            {
              if (fileout)
                fprintf(fileout,
                        "No files may be included from wads: %s\n",inbuffer);
              continue;
            }

          // check for no-text directive, used when including a DEH
          // file but using the BEX format to handle strings

          if (!strnicmp(nextfile = ptr_lstrip(inbuffer+7),"NOTEXT",6))
            includenotext = true, nextfile = ptr_lstrip(nextfile+6);

          if (fileout)
            fprintf(fileout,"Branching to include file %s...\n", nextfile);

          // killough 10/98:
          // Second argument must be NULL to prevent closing fileout too soon

          ProcessDehFile(nextfile,NULL,0); // do the included file

          includenotext = oldnotext;
          if (fileout) fprintf(fileout,"...continuing with %s\n",filename);
          continue;
        }

      for (i=0; i<DEH_BLOCKMAX; i++)
        if (!strncasecmp(inbuffer,deh_blocks[i].key,strlen(deh_blocks[i].key)))
          { // matches one
            if (fileout)
              fprintf(fileout,"Processing function [%d] for %s\n",
                      i, deh_blocks[i].key);
            deh_blocks[i].fptr(filein,fileout,inbuffer);  // call function
            break;  // we got one, that's enough for this block
          }
    }

  if (infile.lump)
    Z_ChangeTag(infile.lump, PU_CACHE);       // Mark purgable
  else if(infile.inp)
     fclose((FILE *) infile.inp);              // Close real file

  if (outfilename)   // killough 10/98: only at top recursion level
  {
     if(fileout && fileout != stdout) // haleyjd: don't fclose(NULL)
        fclose(fileout);
     fileout = NULL;
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
void deh_procBexCodePointers(DEHFILE *fpin, FILE* fpout, char *line)
{
  char key[DEH_MAXKEYLEN];
  char inbuffer[DEH_BUFFERMAX];
  int indexnum;
  char mnemonic[DEH_MAXKEYLEN];  // to hold the codepointer mnemonic
  int i; // looper
  boolean found; // know if we found this one during lookup or not

  // Ty 05/16/98 - initialize it to something, dummy!
  strncpy(inbuffer,line,DEH_BUFFERMAX);

  // for this one, we just read 'em until we hit a blank line
  while (!dehfeof(fpin) && *inbuffer && (*inbuffer != ' '))
    {
      if (!dehfgets(inbuffer, sizeof(inbuffer), fpin)) break;
      lfstrip(inbuffer);
      if (!*inbuffer) break;   // killough 11/98: really exit on blank line

      // killough 8/98: allow hex numbers in input:
      if ( (3 != sscanf(inbuffer,"%s %i = %s", key, &indexnum, mnemonic))
           || (stricmp(key,"FRAME")) )  // NOTE: different format from normal
        {
          if (fpout) fprintf(fpout,
                             "Invalid BEX codepointer line - must start with 'FRAME': '%s'\n",
                             inbuffer);
          return;  // early return
        }

      if (fpout) fprintf(fpout,"Processing pointer at index %d: %s\n",
                         indexnum, mnemonic);
      if (indexnum < 0 || indexnum >= NUMSTATES)
        {
          if (fpout) fprintf(fpout,"Bad pointer number %d of %d\n",
                             indexnum, NUMSTATES);
          return; // killough 10/98: fix SegViol
        }
      strcpy(key,"A_");  // reusing the key area to prefix the mnemonic
      strcat(key,ptr_lstrip(mnemonic));

      found = FALSE;
      i= -1; // incremented to start at zero at the top of the loop
      do  // Ty 05/16/98 - fix loop logic to look for null ending entry
        {
          ++i;
          if (!stricmp(key,deh_bexptrs[i].lookup))
            {  // Ty 06/01/98  - add  to states[].action for new djgcc version
              states[indexnum].action = deh_bexptrs[i].cptr; // assign
              if (fpout) fprintf(fpout,
                                 " - applied %p from codeptr[%d] to states[%d]\n",
                                 deh_bexptrs[i].cptr,i,indexnum);
              found = TRUE;
            }
        } while (!found && (deh_bexptrs[i].lookup != NULL));

      if (!found)
        if (fpout) fprintf(fpout,
                           "Invalid frame pointer mnemonic '%s' at %d\n",
                           mnemonic, indexnum);
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

void deh_procThing(DEHFILE *fpin, FILE* fpout, char *line)
{
  char key[DEH_MAXKEYLEN];
  char inbuffer[DEH_BUFFERMAX];
  long value;      // All deh values are ints or longs
  int indexnum;
  int ix;
  int *pix;  // Ptr to int, since all Thing structure entries are ints
  char *strval;

  strncpy(inbuffer,line,DEH_BUFFERMAX);
  if (fpout) fprintf(fpout,"Thing line: '%s'\n",inbuffer);

  // killough 8/98: allow hex numbers in input:
  ix = sscanf(inbuffer,"%s %i",key, &indexnum);
  if (fpout) fprintf(fpout,"count=%d, Thing %d\n",ix, indexnum);

  // Note that the mobjinfo[] array is base zero, but object numbers
  // in the dehacked file start with one.  Grumble.
  --indexnum;

  // now process the stuff
  // Note that for Things we can look up the key and use its offset
  // in the array of key strings as an int offset in the structure

  // get a line until a blank or end of file--it's not
  // blank now because it has our incoming key in it
  while (!dehfeof(fpin) && *inbuffer && (*inbuffer != ' '))
    {
      if (!dehfgets(inbuffer, sizeof(inbuffer), fpin)) break;
      lfstrip(inbuffer);  // toss the end of line

      // killough 11/98: really bail out on blank lines (break != continue)
      if (!*inbuffer) break;  // bail out with blank line between sections
      if (!deh_GetData(inbuffer,key,&value,&strval,fpout)) // returns TRUE if ok
        {
          if (fpout) fprintf(fpout,"Bad data pair in '%s'\n",inbuffer);
          continue;
        }
      for (ix=0; ix < DEH_MOBJINFOMAX; ix++)
        {
          if (!strcasecmp(key,deh_mobjinfo[ix]))  // killough 8/98
            {
              if (!strcasecmp(key,"bits") && !value) // killough 10/98
                {
                  // figure out what the bits are
                  value = 0;

                  // killough 10/98: replace '+' kludge with strtok() loop
                  // Fix error-handling case ('found' var wasn't being reset)
                  //
                  // Use OR logic instead of addition, to allow repetition

                  for (;(strval = strtok(strval,",+| \t\f\r")); strval = NULL)
                    {
                      int iy;
                      for (iy=0; iy < DEH_MOBJFLAGMAX; iy++)
                        if (!strcasecmp(strval,deh_mobjflags[iy].name))
                          {
                            if (fpout)
                              fprintf(fpout, "ORed value 0x%08lx %s\n",
                                      deh_mobjflags[iy].value, strval);
                            value |= deh_mobjflags[iy].value;
                            break;
                          }
                      if (iy >= DEH_MOBJFLAGMAX && fpout)
                        fprintf(fpout, "Could not find bit mnemonic %s\n",
                                strval);
                    }

                  // Don't worry about conversion -- simply print values
                  if (fpout) fprintf(fpout, "Bits = 0x%08lX = %ld \n",
                                     value, value);
                }
              pix = (int *)&mobjinfo[indexnum];
              pix[ix] = (int)value;
              if (fpout) fprintf(fpout,"Assigned %d to %s(%d) at index %d\n",
                                 (int)value, key, indexnum, ix);
            }
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
void deh_procFrame(DEHFILE *fpin, FILE* fpout, char *line)
{
  char key[DEH_MAXKEYLEN];
  char inbuffer[DEH_BUFFERMAX];
  long value;      // All deh values are ints or longs
  int indexnum;

  strncpy(inbuffer,line,DEH_BUFFERMAX);

  // killough 8/98: allow hex numbers in input:
  sscanf(inbuffer,"%s %i",key, &indexnum);
  if (fpout) fprintf(fpout,"Processing Frame at index %d: %s\n",indexnum,key);
  if (indexnum < 0 || indexnum >= NUMSTATES)
    if (fpout) fprintf(fpout,"Bad frame number %d of %d\n",indexnum, NUMSTATES);

  while (!dehfeof(fpin) && *inbuffer && (*inbuffer != ' '))
    {
      if (!dehfgets(inbuffer, sizeof(inbuffer), fpin)) break;
      lfstrip(inbuffer);
      if (!*inbuffer) break;         // killough 11/98
      if (!deh_GetData(inbuffer,key,&value,NULL,fpout)) // returns TRUE if ok
        {
          if (fpout) fprintf(fpout,"Bad data pair in '%s'\n",inbuffer);
          continue;
        }
      if (!strcasecmp(key,deh_state[0]))  // Sprite number
        {
          if (fpout) fprintf(fpout," - sprite = %ld\n",value);
          states[indexnum].sprite = (spritenum_t)value;
        }
      else
        if (!strcasecmp(key,deh_state[1]))  // Sprite subnumber
          {
            if (fpout) fprintf(fpout," - frame = %ld\n",value);
            states[indexnum].frame = value; // long
          }
        else
          if (!strcasecmp(key,deh_state[2]))  // Duration
            {
              if (fpout) fprintf(fpout," - tics = %ld\n",value);
              states[indexnum].tics = value; // long
            }
          else
            if (!strcasecmp(key,deh_state[3]))  // Next frame
              {
                if (fpout) fprintf(fpout," - nextstate = %ld\n",value);
                states[indexnum].nextstate = (statenum_t)value;
              }
            else
              if (!strcasecmp(key,deh_state[4]))  // Codep frame (not set in Frame deh block)
                {
                  if (fpout) fprintf(fpout," - codep, should not be set in Frame section!\n");
                  /* nop */ ;
                }
              else
                if (!strcasecmp(key,deh_state[5]))  // Unknown 1
                  {
                    if (fpout) fprintf(fpout," - misc1 = %ld\n",value);
                    states[indexnum].misc1 = value; // long
                  }
                else
                  if (!strcasecmp(key,deh_state[6]))  // Unknown 2
                    {
                      if (fpout) fprintf(fpout," - misc2 = %ld\n",value);
                      states[indexnum].misc2 = value; // long
                    }
                  else
                    if (fpout) fprintf(fpout,"Invalid frame string index for '%s'\n",key);
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
void deh_procPointer(DEHFILE *fpin, FILE* fpout, char *line) // done
{
  char key[DEH_MAXKEYLEN];
  char inbuffer[DEH_BUFFERMAX];
  long value;      // All deh values are ints or longs
  int indexnum;
  int i; // looper

  strncpy(inbuffer,line,DEH_BUFFERMAX);
  // NOTE: different format from normal

  // killough 8/98: allow hex numbers in input, fix error case:
  if (sscanf(inbuffer,"%*s %*i (%s %i)",key, &indexnum) != 2)
    {
      if (fpout) fprintf(fpout,"Bad data pair in '%s'\n",inbuffer);
      return;
    }

  if (fpout) fprintf(fpout,"Processing Pointer at index %d: %s\n",indexnum, key);
  if (indexnum < 0 || indexnum >= NUMSTATES)
    {
      if (fpout)
        fprintf(fpout,"Bad pointer number %d of %d\n",indexnum, NUMSTATES);
      return;
    }

  while (!dehfeof(fpin) && *inbuffer && (*inbuffer != ' '))
    {
      if (!dehfgets(inbuffer, sizeof(inbuffer), fpin)) break;
      lfstrip(inbuffer);
      if (!*inbuffer) break;       // killough 11/98
      if (!deh_GetData(inbuffer,key,&value,NULL,fpout)) // returns TRUE if ok
        {
          if (fpout) fprintf(fpout,"Bad data pair in '%s'\n",inbuffer);
          continue;
        }

      if (value < 0 || value >= NUMSTATES)
        {
          if (fpout)
            fprintf(fpout,"Bad pointer number %ld of %d\n",value, NUMSTATES);
          return;
        }

      if (!strcasecmp(key,deh_state[4]))  // Codep frame (not set in Frame deh block)
        {
          states[indexnum].action = deh_codeptr[value];
          if (fpout) fprintf(fpout," - applied %p from codeptr[%ld] to states[%d]\n",deh_codeptr[value],value,indexnum);
          // Write BEX-oriented line to match:
          for (i=0;i<NUMSTATES;i++)
            {
              if (deh_bexptrs[i].cptr == deh_codeptr[value])
                {
                  if (fpout) fprintf(fpout,"BEX [CODEPTR] -> FRAME %d = %s\n",
                                     indexnum, &deh_bexptrs[i].lookup[2]);
                  break;
                }
            }
        }
      else
        if (fpout) fprintf(fpout,"Invalid frame pointer index for '%s' at %ld, xref %p\n",
                           key, value, deh_codeptr[value]);
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
void deh_procSounds(DEHFILE *fpin, FILE* fpout, char *line)
{
  char key[DEH_MAXKEYLEN];
  char inbuffer[DEH_BUFFERMAX];
  long value;      // All deh values are ints or longs
  int indexnum;

  strncpy(inbuffer,line,DEH_BUFFERMAX);

  // killough 8/98: allow hex numbers in input:
  sscanf(inbuffer,"%s %i",key, &indexnum);
  if (fpout) fprintf(fpout,"Processing Sounds at index %d: %s\n",
                     indexnum, key);
  if (indexnum < 0 || indexnum >= NUMSFX)
    if (fpout) fprintf(fpout,"Bad sound number %d of %d\n",
                       indexnum, NUMSFX);

  while (!dehfeof(fpin) && *inbuffer && (*inbuffer != ' '))
    {
      if (!dehfgets(inbuffer, sizeof(inbuffer), fpin)) break;
      lfstrip(inbuffer);
      if (!*inbuffer) break;         // killough 11/98
      if (!deh_GetData(inbuffer,key,&value,NULL,fpout)) // returns TRUE if ok
        {
          if (fpout) fprintf(fpout,"Bad data pair in '%s'\n",inbuffer);
          continue;
        }
      if (!strcasecmp(key,deh_sfxinfo[0]))  // Offset
        /* nop */ ;  // we don't know what this is, I don't think
      else
        if (!strcasecmp(key,deh_sfxinfo[1]))  // Zero/One
          S_sfx[indexnum].singularity = value;
        else
          if (!strcasecmp(key,deh_sfxinfo[2]))  // Value
            S_sfx[indexnum].priority = value;
          else
            if (!strcasecmp(key,deh_sfxinfo[3]))  // Zero 1
              S_sfx[indexnum].link = (sfxinfo_t *)value;
            else
              if (!strcasecmp(key,deh_sfxinfo[4]))  // Zero 2
                S_sfx[indexnum].pitch = value;
              else
                if (!strcasecmp(key,deh_sfxinfo[5]))  // Zero 3
                  S_sfx[indexnum].volume = value;
                else
                  if (!strcasecmp(key,deh_sfxinfo[6]))  // Zero 4
                    S_sfx[indexnum].data = (void *) value; // killough 5/3/98: changed cast
                  else
                    if (!strcasecmp(key,deh_sfxinfo[7]))  // Neg. One 1
                      S_sfx[indexnum].usefulness = value;
                    else
                      if (!strcasecmp(key,deh_sfxinfo[8]))  // Neg. One 2
                        S_sfx[indexnum].lumpnum = value;
                      else
                        if (fpout) fprintf(fpout,
                                           "Invalid sound string index for '%s'\n",key);
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
void deh_procAmmo(DEHFILE *fpin, FILE* fpout, char *line)
{
  char key[DEH_MAXKEYLEN];
  char inbuffer[DEH_BUFFERMAX];
  long value;      // All deh values are ints or longs
  int indexnum;

  strncpy(inbuffer,line,DEH_BUFFERMAX);

  // killough 8/98: allow hex numbers in input:
  sscanf(inbuffer,"%s %i",key, &indexnum);
  if (fpout) fprintf(fpout,"Processing Ammo at index %d: %s\n",
                     indexnum, key);
  if (indexnum < 0 || indexnum >= NUMAMMO)
    if (fpout) fprintf(fpout,"Bad ammo number %d of %d\n",
                       indexnum,NUMAMMO);

  while (!dehfeof(fpin) && *inbuffer && (*inbuffer != ' '))
    {
      if (!dehfgets(inbuffer, sizeof(inbuffer), fpin)) break;
      lfstrip(inbuffer);
      if (!*inbuffer) break;       // killough 11/98
      if (!deh_GetData(inbuffer,key,&value,NULL,fpout)) // returns TRUE if ok
        {
          if (fpout) fprintf(fpout,"Bad data pair in '%s'\n",inbuffer);
          continue;
        }
      if (!strcasecmp(key,deh_ammo[0]))  // Max ammo
        maxammo[indexnum] = value;
      else
        if (!strcasecmp(key,deh_ammo[1]))  // Per ammo
          clipammo[indexnum] = value;
        else
          if (fpout) fprintf(fpout,"Invalid ammo string index for '%s'\n",key);
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
void deh_procWeapon(DEHFILE *fpin, FILE* fpout, char *line)
{
  char key[DEH_MAXKEYLEN];
  char inbuffer[DEH_BUFFERMAX];
  long value;      // All deh values are ints or longs
  int indexnum;

  strncpy(inbuffer,line,DEH_BUFFERMAX);

  // killough 8/98: allow hex numbers in input:
  sscanf(inbuffer,"%s %i",key, &indexnum);
  if (fpout) fprintf(fpout,"Processing Weapon at index %d: %s\n",
                     indexnum, key);
  if (indexnum < 0 || indexnum >= NUMWEAPONS)
    if (fpout) fprintf(fpout,"Bad weapon number %d of %d\n",
                       indexnum, NUMAMMO);

  while (!dehfeof(fpin) && *inbuffer && (*inbuffer != ' '))
    {
      if (!dehfgets(inbuffer, sizeof(inbuffer), fpin)) break;
      lfstrip(inbuffer);
      if (!*inbuffer) break;       // killough 11/98
      if (!deh_GetData(inbuffer,key,&value,NULL,fpout)) // returns TRUE if ok
        {
          if (fpout) fprintf(fpout,"Bad data pair in '%s'\n",inbuffer);
          continue;
        }
      if (!strcasecmp(key,deh_weapon[0]))  // Ammo type
        weaponinfo[indexnum].ammo = value;
      else
        if (!strcasecmp(key,deh_weapon[1]))  // Deselect frame
          weaponinfo[indexnum].upstate = value;
        else
          if (!strcasecmp(key,deh_weapon[2]))  // Select frame
            weaponinfo[indexnum].downstate = value;
          else
            if (!strcasecmp(key,deh_weapon[3]))  // Bobbing frame
              weaponinfo[indexnum].readystate = value;
            else
              if (!strcasecmp(key,deh_weapon[4]))  // Shooting frame
                weaponinfo[indexnum].atkstate = value;
              else
                if (!strcasecmp(key,deh_weapon[5]))  // Firing frame
                  weaponinfo[indexnum].flashstate = value;
                else
                  if (fpout) fprintf(fpout,"Invalid weapon string index for '%s'\n",key);
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
void deh_procSprite(DEHFILE *fpin, FILE* fpout, char *line) // Not supported
{
  char key[DEH_MAXKEYLEN];
  char inbuffer[DEH_BUFFERMAX];
  int indexnum;

  // Too little is known about what this is supposed to do, and
  // there are better ways of handling sprite renaming.  Not supported.

  strncpy(inbuffer,line,DEH_BUFFERMAX);

  // killough 8/98: allow hex numbers in input:
  sscanf(inbuffer,"%s %i",key, &indexnum);
  if (fpout) fprintf(fpout,
                     "Ignoring Sprite offset change at index %d: %s\n",indexnum, key);
  while (!dehfeof(fpin) && *inbuffer && (*inbuffer != ' '))
    {
      if (!dehfgets(inbuffer, sizeof(inbuffer), fpin)) break;
      lfstrip(inbuffer);
      if (!*inbuffer) break;      // killough 11/98
      // ignore line
      if (fpout) fprintf(fpout,"- %s\n",inbuffer);
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
void deh_procPars(DEHFILE *fpin, FILE* fpout, char *line) // extension
{
  char key[DEH_MAXKEYLEN];
  char inbuffer[DEH_BUFFERMAX];
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

  strncpy(inbuffer,line,DEH_BUFFERMAX);

  // killough 8/98: allow hex numbers in input:
  sscanf(inbuffer,"%s %i",key, &indexnum);
  if (fpout) fprintf(fpout,
                     "Processing Par value at index %d: %s\n",indexnum, key);
  // indexnum is a dummy entry
  while (!dehfeof(fpin) && *inbuffer && (*inbuffer != ' '))
    {
      if (!dehfgets(inbuffer, sizeof(inbuffer), fpin)) break;
      lfstrip(strlwr(inbuffer)); // lowercase it
      if (!*inbuffer) break;      // killough 11/98
      if (3 != sscanf(inbuffer,"par %i %i %i",&episode, &level, &partime))
        { // not 3
          if (2 != sscanf(inbuffer,"par %i %i",&level, &partime))
            { // not 2
              if (fpout) fprintf(fpout,"Invalid par time setting string: %s\n",inbuffer);
            }
          else
            { // is 2
              // Ty 07/11/98 - wrong range check, not zero-based
              if (level < 1 || level > 32) // base 0 array (but 1-based parm)
                {
                  if (fpout) fprintf(fpout,"Invalid MAPnn value MAP%d\n",level);
                }
              else
                {
                  oldpar = cpars[level-1];
                  if (fpout) fprintf(fpout,"Changed par time for MAP%02d from %d to %d\n",level,oldpar,partime);
                  cpars[level-1] = partime;
                  deh_pars = TRUE;
                }
            }
        }
      else
        { // is 3
          // note that though it's a [4][10] array, the "left" and "top" aren't used,
          // effectively making it a base 1 array.
          // Ty 07/11/98 - level was being checked against max 3 - dumb error
          // Note that episode 4 does not have par times per original design
          // in Ultimate DOOM so that is not supported here.
          if (episode < 1 || episode > 3 || level < 1 || level > 9)
            {
              if (fpout) fprintf(fpout,
                                 "Invalid ExMx values E%dM%d\n",episode, level);
            }
          else
            {
              oldpar = pars[episode][level];
              pars[episode][level] = partime;
              if (fpout) fprintf(fpout,
                                 "Changed par time for E%dM%d from %d to %d\n",
                                 episode,level,oldpar,partime);
              deh_pars = TRUE;
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
void deh_procCheat(DEHFILE *fpin, FILE* fpout, char *line) // done
{
  char key[DEH_MAXKEYLEN];
  char inbuffer[DEH_BUFFERMAX];
  long value;      // All deh values are ints or longs
  char *strval = "";  // pointer to the value area
  int ix, iy;   // array indices
  char *p;  // utility pointer

  if (fpout) fprintf(fpout,"Processing Cheat: %s\n",line);

  strncpy(inbuffer,line,DEH_BUFFERMAX);
  while (!dehfeof(fpin) && *inbuffer && (*inbuffer != ' '))
    {
      if (!dehfgets(inbuffer, sizeof(inbuffer), fpin)) break;
      lfstrip(inbuffer);
      if (!*inbuffer) break;       // killough 11/98
      if (!deh_GetData(inbuffer,key,&value,&strval,fpout)) // returns TRUE if ok
        {
          if (fpout) fprintf(fpout,"Bad data pair in '%s'\n",inbuffer);
          continue;
        }
      // Otherwise we got a (perhaps valid) cheat name,
      // so look up the key in the array

      // killough 4/18/98: use main cheat code table in st_stuff.c now
      for (ix=0; cheat[ix].cheat; ix++)
        if (cheat[ix].deh_cheat)   // killough 4/18/98: skip non-deh
          {
            if (!stricmp(key,cheat[ix].deh_cheat))  // found the cheat, ignored case
              {
                // replace it but don't overflow it.  Use current length as limit.
                // Ty 03/13/98 - add 0xff code
                // Deal with the fact that the cheats in deh files are extended
                // with character 0xFF to the original cheat length, which we don't do.
                for (iy=0; strval[iy]; iy++)
                  strval[iy] = (strval[iy]==(char)0xff) ? '\0' : strval[iy];

                iy = ix;     // killough 4/18/98

                // Ty 03/14/98 - skip leading spaces
                p = strval;
                while (*p == ' ') ++p;
                // Ty 03/16/98 - change to use a strdup and orphan the original
                // Also has the advantage of allowing length changes.
                // strncpy(cheat[iy].cheat,p,strlen(cheat[iy].cheat));
                {    // killough 9/12/98: disable cheats which are prefixes of this one
                  int i;
                  for (i=0; cheat[i].cheat; i++)
                    if (cheat[i].when & not_deh &&
                        !strncasecmp(cheat[i].cheat,
                                     cheat[iy].cheat,
                                     strlen(cheat[i].cheat)) && i != iy)
                      cheat[i].deh_modified = true;
                }
                cheat[iy].cheat = strdup(p);
                if (fpout) fprintf(fpout,
                                   "Assigned new cheat '%s' to cheat '%s'at index %d\n",
                                   p, cheat[ix].deh_cheat, iy); // killough 4/18/98
              }
          }
      if (fpout) fprintf(fpout,"- %s\n",inbuffer);
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
void deh_procMisc(DEHFILE *fpin, FILE* fpout, char *line) // done
{
  char key[DEH_MAXKEYLEN];
  char inbuffer[DEH_BUFFERMAX];
  long value;      // All deh values are ints or longs

  strncpy(inbuffer,line,DEH_BUFFERMAX);
  while (!dehfeof(fpin) && *inbuffer && (*inbuffer != ' '))
    {
      if (!dehfgets(inbuffer, sizeof(inbuffer), fpin)) break;
      lfstrip(inbuffer);
      if (!*inbuffer) break;    // killough 11/98
      if (!deh_GetData(inbuffer,key,&value,NULL,fpout)) // returns TRUE if ok
        {
          if (fpout) fprintf(fpout,"Bad data pair in '%s'\n",inbuffer);
          continue;
        }
      // Otherwise it's ok
      if (fpout) fprintf(fpout,"Processing Misc item '%s'\n", key);

      if (!strcasecmp(key,deh_misc[0]))  // Initial Health
        initial_health = value;
      else
        if (!strcasecmp(key,deh_misc[1]))  // Initial Bullets
          initial_bullets = value;
        else
          if (!strcasecmp(key,deh_misc[2]))  // Max Health
            maxhealth = value;
          else
            if (!strcasecmp(key,deh_misc[3]))  // Max Armor
              max_armor = value;
            else
              if (!strcasecmp(key,deh_misc[4]))  // Green Armor Class
                green_armor_class = value;
              else
                if (!strcasecmp(key,deh_misc[5]))  // Blue Armor Class
                  blue_armor_class = value;
                else
                  if (!strcasecmp(key,deh_misc[6]))  // Max Soulsphere
                    max_soul = value;
                  else
                    if (!strcasecmp(key,deh_misc[7]))  // Soulsphere Health
                      soul_health = value;
                    else
                      if (!strcasecmp(key,deh_misc[8]))  // Megasphere Health
                        mega_health = value;
                      else
                        if (!strcasecmp(key,deh_misc[9]))  // God Mode Health
                          god_health = value;
                        else
                          if (!strcasecmp(key,deh_misc[10]))  // IDFA Armor
                            idfa_armor = value;
                          else
                            if (!strcasecmp(key,deh_misc[11]))  // IDFA Armor Class
                              idfa_armor_class = value;
                            else
                              if (!strcasecmp(key,deh_misc[12]))  // IDKFA Armor
                                idkfa_armor = value;
                              else
                                if (!strcasecmp(key,deh_misc[13]))  // IDKFA Armor Class
                                  idkfa_armor_class = value;
                                else
                                  if (!strcasecmp(key,deh_misc[14]))  // BFG Cells/Shot
                                    bfgcells = value;
                                  else
                                    if (!strcasecmp(key,deh_misc[15]))  // Monsters Infight
                                      /* No such switch in DOOM - nop */ ;
                                    else
                                      if (fpout) fprintf(fpout,
                                                         "Invalid misc item string index for '%s'\n",key);
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
void deh_procText(DEHFILE *fpin, FILE* fpout, char *line)
{
  char key[DEH_MAXKEYLEN];
  char inbuffer[DEH_BUFFERMAX*2];  // can't use line -- double size buffer too.
  int i; // loop variable
  int fromlen, tolen;  // as specified on the text block line
  int usedlen;  // shorter of fromlen and tolen if not matched
  boolean found = FALSE;  // to allow early exit once found
  char* line2 = NULL;   // duplicate line for rerouting

  // Ty 04/11/98 - Included file may have NOTEXT skip flag set
  if (includenotext) // flag to skip included deh-style text
    {
      if (fpout) fprintf(fpout,
                         "Skipped text block because of notext directive\n");
      strcpy(inbuffer,line);
      while (!dehfeof(fpin) && *inbuffer && (*inbuffer != ' '))
        dehfgets(inbuffer, sizeof(inbuffer), fpin);  // skip block
      // Ty 05/17/98 - don't care if this fails
      return; // ************** Early return
    }

  // killough 8/98: allow hex numbers in input:
  sscanf(line,"%s %i %i",key,&fromlen,&tolen);
  if (fpout) fprintf(fpout,
                     "Processing Text (key=%s, from=%d, to=%d)\n",
                     key, fromlen, tolen);

  // killough 10/98: fix incorrect usage of feof
  {
    int c, totlen = 0;
    while (totlen < fromlen + tolen && (c = dehfgetc(fpin)) != EOF)
      inbuffer[totlen++] = c;
    inbuffer[totlen]='\0';
  }

  // if the from and to are 4, this may be a sprite rename.  Check it
  // against the array and process it as such if it matches.  Remember
  // that the original names are (and should remain) uppercase.
  // Future: this will be from a separate [SPRITES] block.
  if (fromlen==4 && tolen==4)
    {
      i=0;
      while (sprnames[i])  // null terminated list in info.c //jff 3/19/98
        {                                                      //check pointer
          if (!strnicmp(sprnames[i],inbuffer,fromlen))         //not first char
            {
              if (fpout) fprintf(fpout,
                                 "Changing name of sprite at index %d from %s to %*s\n",
                                 i,sprnames[i],tolen,&inbuffer[fromlen]);
              // Ty 03/18/98 - not using strdup because length is fixed

              // killough 10/98: but it's an array of pointers, so we must
              // use strdup unless we redeclare sprnames and change all else
              sprnames[i]=strdup(sprnames[i]);

              strncpy(sprnames[i],&inbuffer[fromlen],tolen);
              found = TRUE;
              break;  // only one will match--quit early
            }
          ++i;  // next array element
        }
    }
  else
    if (fromlen < 7 && tolen < 7)  // lengths of music and sfx are 6 or shorter
      {
        usedlen = (fromlen < tolen) ? fromlen : tolen;
        if (fromlen != tolen)
          if (fpout) fprintf(fpout,
                             "Warning: Mismatched lengths from=%d, to=%d, used %d\n",
                             fromlen, tolen, usedlen);
        // Try sound effects entries - see sounds.c
        for (i=1; i<NUMSFX; i++)
          {
            // avoid short prefix erroneous match
            if (strlen(S_sfx[i].name) != fromlen) continue;
            if (!strnicmp(S_sfx[i].name,inbuffer,fromlen))
              {
                if (fpout) fprintf(fpout,
                                   "Changing name of sfx from %s to %*s\n",
                                   S_sfx[i].name,usedlen,&inbuffer[fromlen]);

                S_sfx[i].name = strdup(&inbuffer[fromlen]);
                found = TRUE;
                break;  // only one matches, quit early
              }
          }
        if (!found)  // not yet
          {
            // Try music name entries - see sounds.c
            for (i=1; i<NUMMUSIC; i++)
              {
                // avoid short prefix erroneous match
                if (strlen(S_music[i].name) != fromlen) continue;
                if (!strnicmp(S_music[i].name,inbuffer,fromlen))
                  {
                    if (fpout) fprintf(fpout,
                                       "Changing name of music from %s to %*s\n",
                                       S_music[i].name,usedlen,&inbuffer[fromlen]);

                    S_music[i].name = strdup(&inbuffer[fromlen]);
                    found = TRUE;
                    break;  // only one matches, quit early
                  }
              }
          }  // end !found test
      }

  if (!found) // Nothing we want to handle here--see if strings can deal with it.
    {
      if (fpout) fprintf(fpout,"Checking text area through strings for '%.12s%s' from=%d to=%d\n",inbuffer, (strlen(inbuffer) > 12) ? "..." : "",fromlen,tolen);
      if (fromlen <= strlen(inbuffer))
        {
          line2 = strdup(&inbuffer[fromlen]);
          inbuffer[fromlen] = '\0';
        }

      deh_procStringSub(NULL, inbuffer, line2, fpout);
    }
  free(line2); // may be NULL, ignored by free()
  return;
}

void deh_procError(DEHFILE *fpin, FILE* fpout, char *line)
{
  char inbuffer[DEH_BUFFERMAX];

  strncpy(inbuffer,line,DEH_BUFFERMAX);
  if (fpout) fprintf(fpout,"Unmatched Block: '%s'\n",inbuffer);
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
void deh_procStrings(DEHFILE *fpin, FILE* fpout, char *line)
{
  char key[DEH_MAXKEYLEN];
  char inbuffer[DEH_BUFFERMAX];
  long value;    // All deh values are ints or longs
  char *strval;      // holds the string value of the line
  static int maxstrlen = 128; // maximum string length, bumped 128 at
  // a time as needed
  // holds the final result of the string after concatenation
  static char *holdstring = NULL;
  boolean found = false;  // looking for string continuation

  if (fpout) fprintf(fpout,"Processing extended string substitution\n");

  if (!holdstring) holdstring = malloc(maxstrlen*sizeof(*holdstring));

  *holdstring = '\0';  // empty string to start with
  strncpy(inbuffer,line,DEH_BUFFERMAX);
  // Ty 04/24/98 - have to allow inbuffer to start with a blank for
  // the continuations of C1TEXT etc.
  while (!dehfeof(fpin) && *inbuffer)  /* && (*inbuffer != ' ') */
    {
      if (!dehfgets(inbuffer, sizeof(inbuffer), fpin)) break;
      if (*inbuffer == '#') continue;  // skip comment lines
      lfstrip(inbuffer);
      if (!*inbuffer) break;  // killough 11/98
      if (!*holdstring) // first one--get the key
        {
          if (!deh_GetData(inbuffer,key,&value,&strval,fpout)) // returns TRUE if ok
            {
              if (fpout) fprintf(fpout,"Bad data pair in '%s'\n",inbuffer);
              continue;
            }
        }
      while (strlen(holdstring) + strlen(inbuffer) > maxstrlen) // Ty03/29/98 - fix stupid error
        {
	  // killough 11/98: allocate enough the first time
          maxstrlen += strlen(holdstring) + strlen(inbuffer) - maxstrlen;
          if (fpout) fprintf(fpout,
                             "* increased buffer from to %d for buffer size %d\n",
                             maxstrlen,(int)strlen(inbuffer));
          holdstring = realloc(holdstring,maxstrlen*sizeof(*holdstring));
        }
      // concatenate the whole buffer if continuation or the value iffirst
      strcat(holdstring,ptr_lstrip(((*holdstring) ? inbuffer : strval)));
      rstrip(holdstring);
      // delete any trailing blanks past the backslash
      // note that blanks before the backslash will be concatenated
      // but ones at the beginning of the next line will not, allowing
      // indentation in the file to read well without affecting the
      // string itself.
      if (holdstring[strlen(holdstring)-1] == '\\')
        {
          holdstring[strlen(holdstring)-1] = '\0';
          continue; // ready to concatenate
        }
      if (*holdstring) // didn't have a backslash, trap above would catch that
        {
          // go process the current string
          found = deh_procStringSub(key, NULL, holdstring, fpout);  // supply keyand not search string

          if (!found)
            if (fpout) fprintf(fpout,
                               "Invalid string key '%s', substitution skipped.\n",key);

          *holdstring = '\0';  // empty string for the next one
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
boolean deh_procStringSub(char *key, char *lookfor, char *newstring, FILE *fpout)
{
  boolean found; // loop exit flag
  int i;  // looper

  found = false;
  for (i=0;i<deh_numstrlookup;i++)
    {
      found = lookfor ?
        !stricmp(*deh_strlookup[i].ppstr,lookfor) :
        !stricmp(deh_strlookup[i].lookup,key);

      if (found)
        {
          *deh_strlookup[i].ppstr = strdup(newstring); // orphan originalstring
          found = true;
          // Handle embedded \n's in the incoming string, convert to 0x0a's
          {
            char *s, *t;
            for (s=t=*deh_strlookup[i].ppstr; *s; ++s, ++t)
              {
                if (*s == '\\' && (s[1] == 'n' || s[1] == 'N')) //found one
		  ++s, *t = '\n';  // skip one extra for second character
                else
                  *t = *s;
              }
            *t = '\0';  // cap off the target string
          }

          if (key)
            if (fpout) fprintf(fpout,
                               "Assigned key %s => '%s'\n",key,newstring);

          if (!key)
            if (fpout) fprintf(fpout,
                               "Assigned '%.12s%s' to'%.12s%s' at key %s\n",
                               lookfor, (strlen(lookfor) > 12) ? "..." : "",
                               newstring, (strlen(newstring) > 12) ? "..." :"",
                               deh_strlookup[i].lookup);

          if (!key) // must have passed an old style string so showBEX
            if (fpout) fprintf(fpout,
                               "*BEX FORMAT:\n%s=%s\n*END BEX\n",
                               deh_strlookup[i].lookup,
                               dehReformatStr(newstring));

          break;
        }
    }
  if (!found)
    if (fpout) fprintf(fpout,
                       "Could not find '%.12s'\n",key ? key: lookfor);

  return found;
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
char *dehReformatStr(char *string)
{
  static char buff[DEH_BUFFERMAX]; // only processing the changed string,
  //  don't need double buffer
  char *s, *t;

  s = string;  // source
  t = buff;    // target
  // let's play...

  while (*s)
    {
      if (*s == '\n')
        ++s, *t++ = '\\', *t++ = 'n', *t++ = '\\', *t++='\n'; 
      else
        *t++ = *s++;
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

void lfstrip(char *s)  // strip the \r and/or \n off of a line
{
  char *p = s+strlen(s);
  while (p > s && (*--p=='\r' || *p=='\n'))
    *p = 0;
}

// ====================================================================
// rstrip
// Purpose: Strips trailing blanks off a string
// Args:    s -- the string to work on
// Returns: void -- the string is modified in place
//
void rstrip(char *s)  // strip trailing whitespace
{
  char *p = s+strlen(s);         // killough 4/4/98: same here
  while (p > s && isspace(*--p)) // break on first non-whitespace
    *p='\0';
}

// ====================================================================
// ptr_lstrip
// Purpose: Points past leading whitespace in a string
// Args:    s -- the string to work on
// Returns: char * pointing to the first nonblank character in the
//          string.  The original string is not changed.
//
char *ptr_lstrip(char *p)  // point past leading whitespace
{
  while (isspace(*p))
    p++;
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

boolean deh_GetData(char *s, char *k, long *l, char **strval, FILE *fpout)
{
  char *t;  // current char
  long val; // to hold value of pair
  char buffer[DEH_MAXKEYLEN];  // to hold key in progress
  boolean okrc = TRUE;  // assume good unless we have problems
  int i;  // iterator

  *buffer = '\0';
  val = 0;  // defaults in case not otherwise set
  for (i=0, t=s; *t && i < DEH_MAXKEYLEN; t++, i++)
    {
      if (*t == '=') break;
      buffer[i] = *t;  // copy it
    }
  buffer[--i] = '\0';  // terminate the key before the '='
  if (!*t)  // end of string with no equal sign
    {
      okrc = FALSE;
    }
  else
    {
      if (!*++t)
        {
          val = 0;  // in case "thiskey =" with no value
          okrc = FALSE;
        }
      // we've incremented t
      val = strtol(t,NULL,0);  // killough 8/9/98: allow hex or octal input
    }

  // go put the results in the passed pointers
  *l = val;  // may be a faked zero

  // if spaces between key and equal sign, strip them
  strcpy(k,ptr_lstrip(buffer));  // could be a zero-length string

  if (strval != NULL) // pass NULL if you don't want this back
    *strval = t;      // pointer, has to be somewhere in s,
  // even if pointing at the zero byte.

  return(okrc);
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

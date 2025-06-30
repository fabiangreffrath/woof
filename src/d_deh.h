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
// Description: This file translates the #defined string constants
// to named variables to externalize them for deh/bex changes.
// Should be able to compile with D_FRENCH (for example) and still
// work (untested).
//
//--------------------------------------------------------------------

#ifndef __D_DEH__
#define __D_DEH__

#include "doomtype.h"

extern int deh_maxhealth;
extern boolean deh_set_maxhealth;
extern boolean deh_set_blood_color;
extern boolean deh_pars;

extern char **dehfiles;

extern char **mapnames[];
extern char **mapnames2[];
extern char **mapnamesp[];
extern char **mapnamest[];

//
//      Ty 03/22/98 - note that we are keeping the english versions and
//      comments in this file
//      New string names all start with an extra s_ to avoid conflicts,
//      but are otherwise identical to the original including uppercase.
//      This is partly to keep the changes simple and partly for easier 
//      identification of the locations in which they're used.
//
//      Printed strings for translation
//

//
//      M_Menu.C
//
//#define QUITMSG       "are you sure you want to\nquit this great game?"
extern char *s_QUITMSG;   // = QUITMSG;
extern char *s_QUITMSG1;  // = QUITMSG1;
extern char *s_QUITMSG2;  // = QUITMSG2;
extern char *s_QUITMSG3;  // = QUITMSG3;
extern char *s_QUITMSG4;  // = QUITMSG4;
extern char *s_QUITMSG5;  // = QUITMSG5;
extern char *s_QUITMSG6;  // = QUITMSG6;
extern char *s_QUITMSG7;  // = QUITMSG7;
extern char *s_QUITMSG8;  // = QUITMSG8;
extern char *s_QUITMSG9;  // = QUITMSG9;
extern char *s_QUITMSG10; // = QUITMSG10;
extern char *s_QUITMSG11; // = QUITMSG11;
extern char *s_QUITMSG12; // = QUITMSG12;
extern char *s_QUITMSG13; // = QUITMSG13;
extern char *s_QUITMSG14; // = QUITMSG14;
//#define LOADNET       "you can't do load while in a net game!\n\n"PRESSKEY
extern char *s_LOADNET; // = LOADNET;
//#define QLOADNET      "you can't quickload during a netgame!\n\n"PRESSKEY
extern char *s_QLOADNET; // = QLOADNET;
//#define QSAVESPOT     "you haven't picked a quicksave slot yet!\n\n"PRESSKEY
extern char *s_QSAVESPOT; // = QSAVESPOT;
//#define SAVEDEAD      "you can't save if you aren't playing!\n\n"PRESSKEY
extern char *s_SAVEDEAD; // = SAVEDEAD;
//#define QSPROMPT      "quicksave over your game named\n\n'%s'?\n\n"PRESSYN
extern char *s_QSPROMPT; // = QSPROMPT;
//#define QLPROMPT      "do you want to quickload the game named\n\n'%s'?\n\n"PRESSYN
extern char *s_QLPROMPT; // = QLPROMPT;

extern char *s_NEWGAME; // = NEWGAME;

extern char *s_NIGHTMARE; // = NIGHTMARE;

extern char *s_SWSTRING; // = SWSTRING;

//#define MSGOFF        "Messages OFF"
extern char *s_MSGOFF; // = MSGOFF;
//#define MSGON         "Messages ON"
extern char *s_MSGON; // = MSGON;
//#define NETEND        "you can't end a netgame!\n\n"PRESSKEY
extern char *s_NETEND; // = NETEND;
//#define ENDGAME       "are you sure you want to end the game?\n\n"PRESSYN
extern char *s_ENDGAME; // = ENDGAME;

//#define DOSY          "(press y to quit)"
extern char *s_DOSY; // = DOSY;

//#define DETAILHI      "High detail"
extern char *s_DETAILHI; // = DETAILHI;
//#define DETAILLO      "Low detail"
extern char *s_DETAILLO; // = DETAILLO;
//#define GAMMALVL0     "Gamma correction OFF"
extern char *s_GAMMALVL0; // = GAMMALVL0;
//#define GAMMALVL1     "Gamma correction level 1"
extern char *s_GAMMALVL1; // = GAMMALVL1;
//#define GAMMALVL2     "Gamma correction level 2"
extern char *s_GAMMALVL2; // = GAMMALVL2;
//#define GAMMALVL3     "Gamma correction level 3"
extern char *s_GAMMALVL3; // = GAMMALVL3;
//#define GAMMALVL4     "Gamma correction level 4"
extern char *s_GAMMALVL4; // = GAMMALVL4;
//#define EMPTYSTRING   "empty slot"
extern char *s_EMPTYSTRING; // = EMPTYSTRING;

//
//      P_inter.C
//
//#define GOTARMOR      "Picked up the armor."
extern char *s_GOTARMOR; // = GOTARMOR;
//#define GOTMEGA       "Picked up the MegaArmor!"
extern char *s_GOTMEGA; // = GOTMEGA;
//#define GOTHTHBONUS   "Picked up a health bonus."
extern char *s_GOTHTHBONUS; // = GOTHTHBONUS;
//#define GOTARMBONUS   "Picked up an armor bonus."
extern char *s_GOTARMBONUS; // = GOTARMBONUS;
//#define GOTSTIM       "Picked up a stimpack."
extern char *s_GOTSTIM; // = GOTSTIM;
//#define GOTMEDINEED   "Picked up a medikit that you REALLY need!"
extern char *s_GOTMEDINEED; // = GOTMEDINEED;
//#define GOTMEDIKIT    "Picked up a medikit."
extern char *s_GOTMEDIKIT; // = GOTMEDIKIT;
//#define GOTSUPER      "Supercharge!"
extern char *s_GOTSUPER; // = GOTSUPER;

//#define GOTBLUECARD   "Picked up a blue keycard."
extern char *s_GOTBLUECARD; // = GOTBLUECARD;
//#define GOTYELWCARD   "Picked up a yellow keycard."
extern char *s_GOTYELWCARD; // = GOTYELWCARD;
//#define GOTREDCARD    "Picked up a red keycard."
extern char *s_GOTREDCARD; // = GOTREDCARD;
//#define GOTBLUESKUL   "Picked up a blue skull key."
extern char *s_GOTBLUESKUL; // = GOTBLUESKUL;
//#define GOTYELWSKUL   "Picked up a yellow skull key."
extern char *s_GOTYELWSKUL; // = GOTYELWSKUL;
//#define GOTREDSKULL   "Picked up a red skull key."
extern char *s_GOTREDSKULL; // = GOTREDSKULL;

//#define GOTINVUL      "Invulnerability!"
extern char *s_GOTINVUL; // = GOTINVUL;
//#define GOTBERSERK    "Berserk!"
extern char *s_GOTBERSERK; // = GOTBERSERK;
//#define GOTINVIS      "Partial Invisibility"
extern char *s_GOTINVIS; // = GOTINVIS;
//#define GOTSUIT       "Radiation Shielding Suit"
extern char *s_GOTSUIT; // = GOTSUIT;
//#define GOTMAP        "Computer Area Map"
extern char *s_GOTMAP; // = GOTMAP;
//#define GOTVISOR      "Light Amplification Visor"
extern char *s_GOTVISOR; // = GOTVISOR;
//#define GOTMSPHERE    "MegaSphere!"
extern char *s_GOTMSPHERE; // = GOTMSPHERE;

//#define GOTCLIP       "Picked up a clip."
extern char *s_GOTCLIP; // = GOTCLIP;
//#define GOTCLIPBOX    "Picked up a box of bullets."
extern char *s_GOTCLIPBOX; // = GOTCLIPBOX;
//#define GOTROCKET     "Picked up a rocket."
extern char *s_GOTROCKET; // = GOTROCKET;
//#define GOTROCKBOX    "Picked up a box of rockets."
extern char *s_GOTROCKBOX; // = GOTROCKBOX;
//#define GOTCELL       "Picked up an energy cell."
extern char *s_GOTCELL; // = GOTCELL;
//#define GOTCELLBOX    "Picked up an energy cell pack."
extern char *s_GOTCELLBOX; // = GOTCELLBOX;
//#define GOTSHELLS     "Picked up 4 shotgun shells."
extern char *s_GOTSHELLS; // = GOTSHELLS;
//#define GOTSHELLBOX   "Picked up a box of shotgun shells."
extern char *s_GOTSHELLBOX; // = GOTSHELLBOX;
//#define GOTBACKPACK   "Picked up a backpack full of ammo!"
extern char *s_GOTBACKPACK; // = GOTBACKPACK;

//#define GOTBFG9000    "You got the BFG9000!  Oh, yes."
extern char *s_GOTBFG9000; // = GOTBFG9000;
//#define GOTCHAINGUN   "You got the chaingun!"
extern char *s_GOTCHAINGUN; // = GOTCHAINGUN;
//#define GOTCHAINSAW   "A chainsaw!  Find some meat!"
extern char *s_GOTCHAINSAW; // = GOTCHAINSAW;
//#define GOTLAUNCHER   "You got the rocket launcher!"
extern char *s_GOTLAUNCHER; // = GOTLAUNCHER;
//#define GOTPLASMA     "You got the plasma gun!"
extern char *s_GOTPLASMA; // = GOTPLASMA;
//#define GOTSHOTGUN    "You got the shotgun!"
extern char *s_GOTSHOTGUN; // = GOTSHOTGUN;
//#define GOTSHOTGUN2   "You got the super shotgun!"
extern char *s_GOTSHOTGUN2; // = GOTSHOTGUN2;

// [NS] Beta pickups.
extern char *s_BETA_BONUS3;
extern char *s_BETA_BONUS4;

//
// P_Doors.C
//
//#define PD_BLUEO      "You need a blue key to activate this object"
extern char *s_PD_BLUEO; // = PD_BLUEO;
//#define PD_REDO       "You need a red key to activate this object"
extern char *s_PD_REDO; // = PD_REDO;
//#define PD_YELLOWO    "You need a yellow key to activate this object"
extern char *s_PD_YELLOWO; // = PD_YELLOWO;
//#define PD_BLUEK      "You need a blue key to open this door"
extern char *s_PD_BLUEK; // = PD_BLUEK;
//#define PD_REDK       "You need a red key to open this door"
extern char *s_PD_REDK; // = PD_REDK;
//#define PD_YELLOWK    "You need a yellow key to open this door"
extern char *s_PD_YELLOWK; // = PD_YELLOWK;
//jff 02/05/98 Create messages specific to card and skull keys
//#define PD_BLUEC      "You need a blue card to open this door"
extern char *s_PD_BLUEC; // = PD_BLUEC;
//#define PD_REDC       "You need a red card to open this door"
extern char *s_PD_REDC; // = PD_REDC;
//#define PD_YELLOWC    "You need a yellow card to open this door"
extern char *s_PD_YELLOWC; // = PD_YELLOWC;
//#define PD_BLUES      "You need a blue skull to open this door"
extern char *s_PD_BLUES; // = PD_BLUES;
//#define PD_REDS       "You need a red skull to open this door"
extern char *s_PD_REDS; // = PD_REDS;
//#define PD_YELLOWS    "You need a yellow skull to open this door"
extern char *s_PD_YELLOWS; // = PD_YELLOWS;
//#define PD_ANY        "Any key will open this door"
extern char *s_PD_ANY; // = PD_ANY;
//#define PD_ALL3 "You need all three keys to open this door"
extern char *s_PD_ALL3; // = PD_ALL3;
//#define PD_ALL6 "You need all six keys to open this door"
extern char *s_PD_ALL6; // = PD_ALL6;

//
//      G_game.C
//
//#define GGSAVED       "game saved."
extern char *s_GGSAVED; // = GGSAVED;

//#define HUSTR_SECRETFOUND     "A secret is revealed!"
extern char *s_HUSTR_SECRETFOUND; // = HUSTR_SECRETFOUND;

// The following should NOT be changed unless it seems
// just AWFULLY necessary

//#define HUSTR_PLRGREEN        "Green: "
extern char *s_HUSTR_PLRGREEN; // = HUSTR_PLRGREEN;
//#define HUSTR_PLRINDIGO       "Indigo: "
extern char *s_HUSTR_PLRINDIGO; // = HUSTR_PLRINDIGO;
//#define HUSTR_PLRBROWN        "Brown: "
extern char *s_HUSTR_PLRBROWN; // = HUSTR_PLRBROWN;
//#define HUSTR_PLRRED          "Red: "
extern char *s_HUSTR_PLRRED; // = HUSTR_PLRRED;

// Ty - Note these are chars, not char *, so name is sc_XXX
//#define HUSTR_KEYGREEN        'g'
extern char sc_HUSTR_KEYGREEN; // = HUSTR_KEYGREEN;
//#define HUSTR_KEYINDIGO       'i'
extern char sc_HUSTR_KEYINDIGO; // = HUSTR_KEYINDIGO;
//#define HUSTR_KEYBROWN        'b'
extern char sc_HUSTR_KEYBROWN; // = HUSTR_KEYBROWN;
//#define HUSTR_KEYRED  'r'
extern char sc_HUSTR_KEYRED; // = HUSTR_KEYRED;

//
//      AM_map.C
//

//#define AMSTR_FOLLOWON        "Follow Mode ON"
extern char *s_AMSTR_FOLLOWON; // = AMSTR_FOLLOWON;
//#define AMSTR_FOLLOWOFF       "Follow Mode OFF"
extern char *s_AMSTR_FOLLOWOFF; // = AMSTR_FOLLOWOFF;

//#define AMSTR_GRIDON  "Grid ON"
extern char *s_AMSTR_GRIDON; // = AMSTR_GRIDON;
//#define AMSTR_GRIDOFF "Grid OFF"
extern char *s_AMSTR_GRIDOFF; // = AMSTR_GRIDOFF;

//#define AMSTR_MARKEDSPOT      "Marked Spot"
extern char *s_AMSTR_MARKEDSPOT; // = AMSTR_MARKEDSPOT;
//#define AMSTR_MARKSCLEARED    "All Marks Cleared"
extern char *s_AMSTR_MARKSCLEARED; // = AMSTR_MARKSCLEARED;

extern char *s_AMSTR_OVERLAYON;
extern char *s_AMSTR_OVERLAYOFF;

extern char *s_AMSTR_ROTATEON;
extern char *s_AMSTR_ROTATEOFF;

//
//      ST_stuff.C
//

//#define STSTR_MUS             "Music Change"
extern char *s_STSTR_MUS; // = STSTR_MUS;
//#define STSTR_NOMUS           "IMPOSSIBLE SELECTION"
extern char *s_STSTR_NOMUS; // = STSTR_NOMUS;
//#define STSTR_DQDON           "Degreelessness Mode On"
extern char *s_STSTR_DQDON; // = STSTR_DQDON;
//#define STSTR_DQDOFF  "Degreelessness Mode Off"
extern char *s_STSTR_DQDOFF; // = STSTR_DQDOFF;

//#define STSTR_KFAADDED        "Very Happy Ammo Added"
extern char *s_STSTR_KFAADDED; // = STSTR_KFAADDED;
//#define STSTR_FAADDED "Ammo (no keys) Added"
extern char *s_STSTR_FAADDED; // = STSTR_FAADDED;

//#define STSTR_NCON            "No Clipping Mode ON"
extern char *s_STSTR_NCON; // = STSTR_NCON;
//#define STSTR_NCOFF           "No Clipping Mode OFF"
extern char *s_STSTR_NCOFF; // = STSTR_NCOFF;

//#define STSTR_BEHOLD  "inVuln, Str, Inviso, Rad, Allmap, or Lite-amp"
extern char *s_STSTR_BEHOLD; // = STSTR_BEHOLD;
//#define STSTR_BEHOLDX "Power-up Toggled"
extern char *s_STSTR_BEHOLDX; // = STSTR_BEHOLDX;

//#define STSTR_CHOPPERS        "... doesn't suck - GM"
extern char *s_STSTR_CHOPPERS; // = STSTR_CHOPPERS;
//#define STSTR_CLEV            "Changing Level..."
extern char *s_STSTR_CLEV; // = STSTR_CLEV;

//#define STSTR_COMPON    "Compatibility Mode On"            // phares
extern char *s_STSTR_COMPON; // = STSTR_COMPON;
//#define STSTR_COMPOFF   "Compatibility Mode Off"           // phares
extern char *s_STSTR_COMPOFF; // = STSTR_COMPOFF;

//
//      F_Finale.C
//
extern char *s_E1TEXT; // = E1TEXT;


extern char *s_E2TEXT; // = E2TEXT;


extern char *s_E3TEXT; // = E3TEXT;


extern char *s_E4TEXT; // = E4TEXT;


// after level 6, put this:

extern char *s_C1TEXT; // = C1TEXT;

// After level 11, put this:

extern char *s_C2TEXT; // = C2TEXT;


// After level 20, put this:

extern char *s_C3TEXT; // = C3TEXT;


// After level 29, put this:

extern char *s_C4TEXT; // = C4TEXT;



// Before level 31, put this:

extern char *s_C5TEXT; // = C5TEXT;


// Before level 32, put this:

extern char *s_C6TEXT; // = C6TEXT;

// after map 06 

extern char *s_P1TEXT; // = P1TEXT;

// after map 11

extern char *s_P2TEXT; // = P2TEXT;


// after map 20

extern char *s_P3TEXT; // = P3TEXT;

// after map 30

extern char *s_P4TEXT; // = P4TEXT;

// before map 31

extern char *s_P5TEXT; // = P5TEXT;

// before map 32

extern char *s_P6TEXT; // = P6TEXT;


extern char *s_T1TEXT; // = T1TEXT;


extern char *s_T2TEXT; // = T2TEXT;


extern char *s_T3TEXT; // = T3TEXT;

extern char *s_T4TEXT; // = T4TEXT;


extern char *s_T5TEXT; // = T5TEXT;


extern char *s_T6TEXT; // = T6TEXT;


//
// Character cast strings F_FINALE.C
//
//#define CC_ZOMBIE     "ZOMBIEMAN"
extern char *s_CC_ZOMBIE; // = CC_ZOMBIE;
//#define CC_SHOTGUN    "SHOTGUN GUY"
extern char *s_CC_SHOTGUN; // = CC_SHOTGUN;
//#define CC_HEAVY      "HEAVY WEAPON DUDE"
extern char *s_CC_HEAVY; // = CC_HEAVY;
//#define CC_IMP        "IMP"
extern char *s_CC_IMP; // = CC_IMP;
//#define CC_DEMON      "DEMON"
extern char *s_CC_DEMON; // = CC_DEMON;
//#define CC_LOST       "LOST SOUL"
extern char *s_CC_LOST; // = CC_LOST;
//#define CC_CACO       "CACODEMON"
extern char *s_CC_CACO; // = CC_CACO;
//#define CC_HELL       "HELL KNIGHT"
extern char *s_CC_HELL; // = CC_HELL;
//#define CC_BARON      "BARON OF HELL"
extern char *s_CC_BARON; // = CC_BARON;
//#define CC_ARACH      "ARACHNOTRON"
extern char *s_CC_ARACH; // = CC_ARACH;
//#define CC_PAIN       "PAIN ELEMENTAL"
extern char *s_CC_PAIN; // = CC_PAIN;
//#define CC_REVEN      "REVENANT"
extern char *s_CC_REVEN; // = CC_REVEN;
//#define CC_MANCU      "MANCUBUS"
extern char *s_CC_MANCU; // = CC_MANCU;
//#define CC_ARCH       "ARCH-VILE"
extern char *s_CC_ARCH; // = CC_ARCH;
//#define CC_SPIDER     "THE SPIDER MASTERMIND"
extern char *s_CC_SPIDER; // = CC_SPIDER;
//#define CC_CYBER      "THE CYBERDEMON"
extern char *s_CC_CYBER; // = CC_CYBER;
//#define CC_HERO       "OUR HERO"
extern char *s_CC_HERO; // = CC_HERO;

// Ty 03/30/98 - new substitutions for background textures during int screens
// char*        bgflatE1 = "FLOOR4_8";
extern char *bgflatE1;
// char*        bgflatE2 = "SFLR6_1";
extern char *bgflatE2;
// char*        bgflatE3 = "MFLR8_4";
extern char *bgflatE3;
// char*        bgflatE4 = "MFLR8_3";
extern char *bgflatE4;

// char*        bgflat06 = "SLIME16";
extern char *bgflat06;

// char*        bgflat11 = "RROCK14";
extern char *bgflat11;

// char*        bgflat20 = "RROCK07";
extern char *bgflat20;
// char*        bgflat30 = "RROCK17";
extern char *bgflat30;
// char*        bgflat15 = "RROCK13";
extern char *bgflat15;
// char*        bgflat31 = "RROCK19";
extern char *bgflat31;

// char*        bgcastcall = "BOSSBACK"; // panel behind cast call
extern char *bgcastcall;

// ignored if blank, general purpose startup announcements
// char*        startup1 = "";
extern char *startup1;
// char*        startup2 = "";
extern char *startup2;
// char*        startup3 = "";
extern char *startup3;
// char*        startup4 = "";
extern char *startup4;
// char*        startup5 = "";
extern char *startup5;

// from g_game.c, prefix for savegame name like "boomsav"
extern char *savegamename;

// [FG] obituaries

extern char *s_OB_CRUSH;
extern char *s_OB_SLIME;
extern char *s_OB_LAVA;
extern char *s_OB_KILLEDSELF;
extern char *s_OB_VOODOO;
extern char *s_OB_MONTELEFRAG;
extern char *s_OB_DEFAULT;
extern char *s_OB_MPDEFAULT;
extern char *s_OB_UNDEADHIT;
extern char *s_OB_IMPHIT;
extern char *s_OB_CACOHIT;
extern char *s_OB_DEMONHIT;
extern char *s_OB_SPECTREHIT;
extern char *s_OB_BARONHIT;
extern char *s_OB_KNIGHTHIT;
extern char *s_OB_ZOMBIE;
extern char *s_OB_SHOTGUY;
extern char *s_OB_VILE;
extern char *s_OB_UNDEAD;
extern char *s_OB_FATSO;
extern char *s_OB_CHAINGUY;
extern char *s_OB_SKULL;
extern char *s_OB_IMP;
extern char *s_OB_CACO;
extern char *s_OB_BARON;
extern char *s_OB_KNIGHT;
extern char *s_OB_SPIDER;
extern char *s_OB_BABY;
extern char *s_OB_CYBORG;
extern char *s_OB_WOLFSS;
extern char *s_OB_MPFIST;
extern char *s_OB_MPCHAINSAW;
extern char *s_OB_MPPISTOL;
extern char *s_OB_MPSHOTGUN;
extern char *s_OB_MPSSHOTGUN;
extern char *s_OB_MPCHAINGUN;
extern char *s_OB_MPROCKET;
extern char *s_OB_MPPLASMARIFLE;
extern char *s_OB_MPBFG_BOOM;
extern char *s_OB_MPTELEFRAG;

#endif

//--------------------------------------------------------------------
//
// $Log: d_deh.h,v $
// Revision 1.5  1998/05/04  21:36:33  thldrmn
// commenting, reformatting and savegamename change
//
// Revision 1.4  1998/04/10  06:47:29  killough
// Fix CVS stuff
//   
//
//--------------------------------------------------------------------

//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2014 Fabian Greffrath
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
// Parses [STRINGS] sections in BEX files
//

#include <stdio.h>
#include <string.h>

#include "deh_defs.h"
#include "deh_io.h"
#include "deh_main.h"
#include "deh_strings.h"
#include "doomtype.h"
#include "i_system.h"
#include "info.h"
#include "m_misc.h"

// mnemonic keys table
const bex_string_t bex_mnemonic_table[] =
{
    // part 1 - general initialization and prompts
    {"D_DEVSTR",             D_DEVSTR            },
    {"D_CDROM",              D_CDROM             },
    {"QUITMSG",              QUITMSG             },
    {"LOADNET",              LOADNET             },
    {"QLOADNET",             QLOADNET            },
    {"QSAVESPOT",            QSAVESPOT           },
    {"SAVEDEAD",             SAVEDEAD            },
    {"QSPROMPT",             QSPROMPT            },
    {"QLPROMPT",             QLPROMPT            },
    {"NEWGAME",              NEWGAME             },
    {"NIGHTMARE",            NIGHTMARE           },
    {"SWSTRING",             SWSTRING            },
    {"MSGOFF",               MSGOFF              },
    {"MSGON",                MSGON               },
    {"NETEND",               NETEND              },
    {"ENDGAME",              ENDGAME             },
    {"DETAILHI",             DETAILHI            },
    {"DETAILLO",             DETAILLO            },
    {"GAMMALVL0",            GAMMALVL0           },
    {"GAMMALVL1",            GAMMALVL1           },
    {"GAMMALVL2",            GAMMALVL2           },
    {"GAMMALVL3",            GAMMALVL3           },
    {"GAMMALVL4",            GAMMALVL4           },
    {"EMPTYSTRING",          EMPTYSTRING         },
    {"GGSAVED",              GGSAVED             },
    // part 2 - messages when the player gets things
    {"GOTARMOR",             GOTARMOR            },
    {"GOTMEGA",              GOTMEGA             },
    {"GOTHTHBONUS",          GOTHTHBONUS         },
    {"GOTARMBONUS",          GOTARMBONUS         },
    {"GOTSTIM",              GOTSTIM             },
    {"GOTMEDINEED",          GOTMEDINEED         },
    {"GOTMEDIKIT",           GOTMEDIKIT          },
    {"GOTSUPER",             GOTSUPER            },
    {"GOTBLUECARD",          GOTBLUECARD         },
    {"GOTYELWCARD",          GOTYELWCARD         },
    {"GOTREDCARD",           GOTREDCARD          },
    {"GOTBLUESKUL",          GOTBLUESKUL         },
    {"GOTYELWSKUL",          GOTYELWSKUL         },
    {"GOTREDSKULL",          GOTREDSKULL         },
    {"GOTINVUL",             GOTINVUL            },
    {"GOTBERSERK",           GOTBERSERK          },
    {"GOTINVIS",             GOTINVIS            },
    {"GOTSUIT",              GOTSUIT             },
    {"GOTMAP",               GOTMAP              },
    {"GOTVISOR",             GOTVISOR            },
    {"GOTMSPHERE",           GOTMSPHERE          },
    {"GOTCLIP",              GOTCLIP             },
    {"GOTCLIPBOX",           GOTCLIPBOX          },
    {"GOTROCKET",            GOTROCKET           },
    {"GOTROCKBOX",           GOTROCKBOX          },
    {"GOTCELL",              GOTCELL             },
    {"GOTCELLBOX",           GOTCELLBOX          },
    {"GOTSHELLS",            GOTSHELLS           },
    {"GOTSHELLBOX",          GOTSHELLBOX         },
    {"GOTBACKPACK",          GOTBACKPACK         },
    {"GOTBFG9000",           GOTBFG9000          },
    {"GOTCHAINGUN",          GOTCHAINGUN         },
    {"GOTCHAINSAW",          GOTCHAINSAW         },
    {"GOTLAUNCHER",          GOTLAUNCHER         },
    {"GOTPLASMA",            GOTPLASMA           },
    {"GOTSHOTGUN",           GOTSHOTGUN          },
    {"GOTSHOTGUN2",          GOTSHOTGUN2         },
    // [NS] Beta pickups.
    {"BETA_BONUS3",          BETA_BONUS3         },
    {"BETA_BONUS4",          BETA_BONUS4         },
    // part 3 - messages when keys are needed
    {"PD_BLUEO",             PD_BLUEO            },
    {"PD_REDO",              PD_REDO             },
    {"PD_YELLOWO",           PD_YELLOWO          },
    {"PD_BLUEK",             PD_BLUEK            },
    {"PD_REDK",              PD_REDK             },
    {"PD_YELLOWK",           PD_YELLOWK          },
    // part 3.1 - boom extensions
    {"PD_BLUEC",             PD_BLUEC            },
    {"PD_REDC",              PD_REDC             },
    {"PD_YELLOWC",           PD_YELLOWC          },
    {"PD_BLUES",             PD_BLUES            },
    {"PD_REDS",              PD_REDS             },
    {"PD_YELLOWS",           PD_YELLOWS          },
    {"PD_ANY",               PD_ANY              },
    {"PD_ALL3",              PD_ALL3             },
    {"PD_ALL6",              PD_ALL6             },
    // part 4 - multiplayer messaging
    {"HUSTR_MSGU",           HUSTR_MSGU          },
    {"HUSTR_MESSAGESENT",    HUSTR_MESSAGESENT   },
    {"HUSTR_CHATMACRO0",     HUSTR_CHATMACRO0    },
    {"HUSTR_CHATMACRO1",     HUSTR_CHATMACRO1    },
    {"HUSTR_CHATMACRO2",     HUSTR_CHATMACRO2    },
    {"HUSTR_CHATMACRO3",     HUSTR_CHATMACRO3    },
    {"HUSTR_CHATMACRO4",     HUSTR_CHATMACRO4    },
    {"HUSTR_CHATMACRO5",     HUSTR_CHATMACRO5    },
    {"HUSTR_CHATMACRO6",     HUSTR_CHATMACRO6    },
    {"HUSTR_CHATMACRO7",     HUSTR_CHATMACRO7    },
    {"HUSTR_CHATMACRO8",     HUSTR_CHATMACRO8    },
    {"HUSTR_CHATMACRO9",     HUSTR_CHATMACRO9    },
    {"HUSTR_TALKTOSELF1",    HUSTR_TALKTOSELF1   },
    {"HUSTR_TALKTOSELF2",    HUSTR_TALKTOSELF2   },
    {"HUSTR_TALKTOSELF3",    HUSTR_TALKTOSELF3   },
    {"HUSTR_TALKTOSELF4",    HUSTR_TALKTOSELF4   },
    {"HUSTR_TALKTOSELF5",    HUSTR_TALKTOSELF5   },
    {"HUSTR_PLRGREEN",       HUSTR_PLRGREEN      },
    {"HUSTR_PLRINDIGO",      HUSTR_PLRINDIGO     },
    {"HUSTR_PLRBROWN",       HUSTR_PLRBROWN      },
    {"HUSTR_PLRRED",         HUSTR_PLRRED        },
    // part 4.1 - secrets!
    {"HUSTR_SECRETFOUND",    HUSTR_SECRETFOUND   },
    // part 5 - level names in the automap
    {"HUSTR_E1M1",           HUSTR_E1M1          },
    {"HUSTR_E1M2",           HUSTR_E1M2          },
    {"HUSTR_E1M3",           HUSTR_E1M3          },
    {"HUSTR_E1M4",           HUSTR_E1M4          },
    {"HUSTR_E1M5",           HUSTR_E1M5          },
    {"HUSTR_E1M6",           HUSTR_E1M6          },
    {"HUSTR_E1M7",           HUSTR_E1M7          },
    {"HUSTR_E1M8",           HUSTR_E1M8          },
    {"HUSTR_E1M9",           HUSTR_E1M9          },
    {"HUSTR_E2M1",           HUSTR_E2M1          },
    {"HUSTR_E2M2",           HUSTR_E2M2          },
    {"HUSTR_E2M3",           HUSTR_E2M3          },
    {"HUSTR_E2M4",           HUSTR_E2M4          },
    {"HUSTR_E2M5",           HUSTR_E2M5          },
    {"HUSTR_E2M6",           HUSTR_E2M6          },
    {"HUSTR_E2M7",           HUSTR_E2M7          },
    {"HUSTR_E2M8",           HUSTR_E2M8          },
    {"HUSTR_E2M9",           HUSTR_E2M9          },
    {"HUSTR_E3M1",           HUSTR_E3M1          },
    {"HUSTR_E3M2",           HUSTR_E3M2          },
    {"HUSTR_E3M3",           HUSTR_E3M3          },
    {"HUSTR_E3M4",           HUSTR_E3M4          },
    {"HUSTR_E3M5",           HUSTR_E3M5          },
    {"HUSTR_E3M6",           HUSTR_E3M6          },
    {"HUSTR_E3M7",           HUSTR_E3M7          },
    {"HUSTR_E3M8",           HUSTR_E3M8          },
    {"HUSTR_E3M9",           HUSTR_E3M9          },
    {"HUSTR_E4M1",           HUSTR_E4M1          },
    {"HUSTR_E4M2",           HUSTR_E4M2          },
    {"HUSTR_E4M3",           HUSTR_E4M3          },
    {"HUSTR_E4M4",           HUSTR_E4M4          },
    {"HUSTR_E4M5",           HUSTR_E4M5          },
    {"HUSTR_E4M6",           HUSTR_E4M6          },
    {"HUSTR_E4M7",           HUSTR_E4M7          },
    {"HUSTR_E4M8",           HUSTR_E4M8          },
    {"HUSTR_E4M9",           HUSTR_E4M9          },
    {"HUSTR_1",              HUSTR_1             },
    {"HUSTR_2",              HUSTR_2             },
    {"HUSTR_3",              HUSTR_3             },
    {"HUSTR_4",              HUSTR_4             },
    {"HUSTR_5",              HUSTR_5             },
    {"HUSTR_6",              HUSTR_6             },
    {"HUSTR_7",              HUSTR_7             },
    {"HUSTR_8",              HUSTR_8             },
    {"HUSTR_9",              HUSTR_9             },
    {"HUSTR_10",             HUSTR_10            },
    {"HUSTR_11",             HUSTR_11            },
    {"HUSTR_12",             HUSTR_12            },
    {"HUSTR_13",             HUSTR_13            },
    {"HUSTR_14",             HUSTR_14            },
    {"HUSTR_15",             HUSTR_15            },
    {"HUSTR_16",             HUSTR_16            },
    {"HUSTR_17",             HUSTR_17            },
    {"HUSTR_18",             HUSTR_18            },
    {"HUSTR_19",             HUSTR_19            },
    {"HUSTR_20",             HUSTR_20            },
    {"HUSTR_21",             HUSTR_21            },
    {"HUSTR_22",             HUSTR_22            },
    {"HUSTR_23",             HUSTR_23            },
    {"HUSTR_24",             HUSTR_24            },
    {"HUSTR_25",             HUSTR_25            },
    {"HUSTR_26",             HUSTR_26            },
    {"HUSTR_27",             HUSTR_27            },
    {"HUSTR_28",             HUSTR_28            },
    {"HUSTR_29",             HUSTR_29            },
    {"HUSTR_30",             HUSTR_30            },
    {"HUSTR_31",             HUSTR_31            },
    {"HUSTR_32",             HUSTR_32            },
    {"PHUSTR_1",             PHUSTR_1            },
    {"PHUSTR_2",             PHUSTR_2            },
    {"PHUSTR_3",             PHUSTR_3            },
    {"PHUSTR_4",             PHUSTR_4            },
    {"PHUSTR_5",             PHUSTR_5            },
    {"PHUSTR_6",             PHUSTR_6            },
    {"PHUSTR_7",             PHUSTR_7            },
    {"PHUSTR_8",             PHUSTR_8            },
    {"PHUSTR_9",             PHUSTR_9            },
    {"PHUSTR_10",            PHUSTR_10           },
    {"PHUSTR_11",            PHUSTR_11           },
    {"PHUSTR_12",            PHUSTR_12           },
    {"PHUSTR_13",            PHUSTR_13           },
    {"PHUSTR_14",            PHUSTR_14           },
    {"PHUSTR_15",            PHUSTR_15           },
    {"PHUSTR_16",            PHUSTR_16           },
    {"PHUSTR_17",            PHUSTR_17           },
    {"PHUSTR_18",            PHUSTR_18           },
    {"PHUSTR_19",            PHUSTR_19           },
    {"PHUSTR_20",            PHUSTR_20           },
    {"PHUSTR_21",            PHUSTR_21           },
    {"PHUSTR_22",            PHUSTR_22           },
    {"PHUSTR_23",            PHUSTR_23           },
    {"PHUSTR_24",            PHUSTR_24           },
    {"PHUSTR_25",            PHUSTR_25           },
    {"PHUSTR_26",            PHUSTR_26           },
    {"PHUSTR_27",            PHUSTR_27           },
    {"PHUSTR_28",            PHUSTR_28           },
    {"PHUSTR_29",            PHUSTR_29           },
    {"PHUSTR_30",            PHUSTR_30           },
    {"PHUSTR_31",            PHUSTR_31           },
    {"PHUSTR_32",            PHUSTR_32           },
    {"THUSTR_1",             THUSTR_1            },
    {"THUSTR_2",             THUSTR_2            },
    {"THUSTR_3",             THUSTR_3            },
    {"THUSTR_4",             THUSTR_4            },
    {"THUSTR_5",             THUSTR_5            },
    {"THUSTR_6",             THUSTR_6            },
    {"THUSTR_7",             THUSTR_7            },
    {"THUSTR_8",             THUSTR_8            },
    {"THUSTR_9",             THUSTR_9            },
    {"THUSTR_10",            THUSTR_10           },
    {"THUSTR_11",            THUSTR_11           },
    {"THUSTR_12",            THUSTR_12           },
    {"THUSTR_13",            THUSTR_13           },
    {"THUSTR_14",            THUSTR_14           },
    {"THUSTR_15",            THUSTR_15           },
    {"THUSTR_16",            THUSTR_16           },
    {"THUSTR_17",            THUSTR_17           },
    {"THUSTR_18",            THUSTR_18           },
    {"THUSTR_19",            THUSTR_19           },
    {"THUSTR_20",            THUSTR_20           },
    {"THUSTR_21",            THUSTR_21           },
    {"THUSTR_22",            THUSTR_22           },
    {"THUSTR_23",            THUSTR_23           },
    {"THUSTR_24",            THUSTR_24           },
    {"THUSTR_25",            THUSTR_25           },
    {"THUSTR_26",            THUSTR_26           },
    {"THUSTR_27",            THUSTR_27           },
    {"THUSTR_28",            THUSTR_28           },
    {"THUSTR_29",            THUSTR_29           },
    {"THUSTR_30",            THUSTR_30           },
    {"THUSTR_31",            THUSTR_31           },
    {"THUSTR_32",            THUSTR_32           },
    // part 6 - messages as a result of toggling states
    {"AMSTR_FOLLOWON",       AMSTR_FOLLOWON      },
    {"AMSTR_FOLLOWOFF",      AMSTR_FOLLOWOFF     },
    {"AMSTR_GRIDON",         AMSTR_GRIDON        },
    {"AMSTR_GRIDOFF",        AMSTR_GRIDOFF       },
    {"AMSTR_MARKEDSPOT",     AMSTR_MARKEDSPOT    },
    {"AMSTR_MARKSCLEARED",   AMSTR_MARKSCLEARED  },
    {"STSTR_MUS",            STSTR_MUS           },
    {"STSTR_NOMUS",          STSTR_NOMUS         },
    {"STSTR_DQDON",          STSTR_DQDON         },
    {"STSTR_DQDOFF",         STSTR_DQDOFF        },
    {"STSTR_KFAADDED",       STSTR_KFAADDED      },
    {"STSTR_FAADDED",        STSTR_FAADDED       },
    {"STSTR_NCON",           STSTR_NCON          },
    {"STSTR_NCOFF",          STSTR_NCOFF         },
    {"STSTR_BEHOLD",         STSTR_BEHOLD        },
    {"STSTR_BEHOLDX",        STSTR_BEHOLDX       },
    {"STSTR_CHOPPERS",       STSTR_CHOPPERS      },
    {"STSTR_CLEV",           STSTR_CLEV          },
    // part 6.1 - additional extensions
    {"AMSTR_OVERLAYON",      AMSTR_OVERLAYON     },
    {"AMSTR_OVERLAYOFF",     AMSTR_OVERLAYOFF    },
    {"AMSTR_ROTATEON",       AMSTR_ROTATEON      },
    {"AMSTR_ROTATEOFF",      AMSTR_ROTATEOFF     },
    {"STSTR_COMPON",         STSTR_COMPON        },
    {"STSTR_COMPOFF",        STSTR_COMPOFF       },
    // part 7 - episode intermission texts
    {"E1TEXT",               E1TEXT              },
    {"E2TEXT",               E2TEXT              },
    {"E3TEXT",               E3TEXT              },
    {"E4TEXT",               E4TEXT              },
    {"C1TEXT",               C1TEXT              },
    {"C2TEXT",               C2TEXT              },
    {"C3TEXT",               C3TEXT              },
    {"C4TEXT",               C4TEXT              },
    {"C5TEXT",               C5TEXT              },
    {"C6TEXT",               C6TEXT              },
    {"P1TEXT",               P1TEXT              },
    {"P2TEXT",               P2TEXT              },
    {"P3TEXT",               P3TEXT              },
    {"P4TEXT",               P4TEXT              },
    {"P5TEXT",               P5TEXT              },
    {"P6TEXT",               P6TEXT              },
    {"T1TEXT",               T1TEXT              },
    {"T2TEXT",               T2TEXT              },
    {"T3TEXT",               T3TEXT              },
    {"T4TEXT",               T4TEXT              },
    {"T5TEXT",               T5TEXT              },
    {"T6TEXT",               T6TEXT              },
    // part 8 - creature names for the finale
    {"CC_ZOMBIE",            CC_ZOMBIE           },
    {"CC_SHOTGUN",           CC_SHOTGUN          },
    {"CC_HEAVY",             CC_HEAVY            },
    {"CC_IMP",               CC_IMP              },
    {"CC_DEMON",             CC_DEMON            },
    {"CC_LOST",              CC_LOST             },
    {"CC_CACO",              CC_CACO             },
    {"CC_HELL",              CC_HELL             },
    {"CC_BARON",             CC_BARON            },
    {"CC_ARACH",             CC_ARACH            },
    {"CC_PAIN",              CC_PAIN             },
    {"CC_REVEN",             CC_REVEN            },
    {"CC_MANCU",             CC_MANCU            },
    {"CC_ARCH",              CC_ARCH             },
    {"CC_SPIDER",            CC_SPIDER           },
    {"CC_CYBER",             CC_CYBER            },
    {"CC_HERO",              CC_HERO             },
    // part 8.1 - lor/id24 creature names for the finale
    {"ID24_CC_GHOUL",        ID24_CC_GHOUL       },
    {"ID24_CC_BANSHEE",      ID24_CC_BANSHEE     },
    {"ID24_CC_SHOCKTROOPER", ID24_CC_SHOCKTROOPER},
    {"ID24_CC_MINDWEAVER",   ID24_CC_MINDWEAVER  },
    {"ID24_CC_VASSAGO",      ID24_CC_VASSAGO     },
    {"ID24_CC_TYRANT",       ID24_CC_TYRANT      },
    // part 9 - intermission tiled backgrounds
    {"BGFLATE1",             BGFLATE1            },
    {"BGFLATE2",             BGFLATE2            },
    {"BGFLATE3",             BGFLATE3            },
    {"BGFLATE4",             BGFLATE4            },
    {"BGFLAT06",             BGFLAT06            },
    {"BGFLAT11",             BGFLAT11            },
    {"BGFLAT20",             BGFLAT20            },
    {"BGFLAT30",             BGFLAT30            },
    {"BGFLAT15",             BGFLAT15            },
    {"BGFLAT31",             BGFLAT31            },
    {"BGCASTCALL",           BGCASTCALL          },
    // part 10 boom startup message
    {"STARTUP1",             STARTUP1            },
    {"STARTUP2",             STARTUP2            },
    {"STARTUP3",             STARTUP3            },
    {"STARTUP4",             STARTUP4            },
    {"STARTUP5",             STARTUP5            },
    // part 11 - general obituaris
    {"OB_CRUSH",             OB_CRUSH            },
    {"OB_SLIME",             OB_SLIME            },
    {"OB_LAVA",              OB_LAVA             },
    {"OB_KILLEDSELF",        OB_KILLEDSELF       },
    {"OB_VOODOO",            OB_VOODOO           },
    {"OB_MONTELEFRAG",       OB_MONTELEFRAG      },
    {"OB_DEFAULT",           OB_DEFAULT          },
    {"OB_MPDEFAULT",         OB_MPDEFAULT        },
    // part 11.1 vanilla melee
    {"OB_UNDEADHIT",         OB_UNDEADHIT        },
    {"OB_IMPHIT",            OB_IMPHIT           },
    {"OB_CACOHIT",           OB_CACOHIT          },
    {"OB_DEMONHIT",          OB_DEMONHIT         },
    {"OB_SPECTREHIT",        OB_SPECTREHIT       },
    {"OB_BARONHIT",          OB_BARONHIT         },
    {"OB_KNIGHTHIT",         OB_KNIGHTHIT        },
    // part 11.2 vanilla ranged
    {"OB_ZOMBIE",            OB_ZOMBIE           },
    {"OB_SHOTGUY",           OB_SHOTGUY          },
    {"OB_VILE",              OB_VILE             },
    {"OB_UNDEAD",            OB_UNDEAD           },
    {"OB_FATSO",             OB_FATSO            },
    {"OB_CHAINGUY",          OB_CHAINGUY         },
    {"OB_SKULL",             OB_SKULL            },
    {"OB_IMP",               OB_IMP              },
    {"OB_CACO",              OB_CACO             },
    {"OB_BARON",             OB_BARON            },
    {"OB_KNIGHT",            OB_KNIGHT           },
    {"OB_SPIDER",            OB_SPIDER           },
    {"OB_BABY",              OB_BABY             },
    {"OB_CYBORG",            OB_CYBORG           },
    {"OB_WOLFSS",            OB_WOLFSS           },
    // part 11.3 multiplayer
    {"OB_MPFIST",            OB_MPFIST           },
    {"OB_MPCHAINSAW",        OB_MPCHAINSAW       },
    {"OB_MPPISTOL",          OB_MPPISTOL         },
    {"OB_MPSHOTGUN",         OB_MPSHOTGUN        },
    {"OB_MPSSHOTGUN",        OB_MPSSHOTGUN       },
    {"OB_MPCHAINGUN",        OB_MPCHAINGUN       },
    {"OB_MPROCKET",          OB_MPROCKET         },
    {"OB_MPPLASMARIFLE",     OB_MPPLASMARIFLE    },
    {"OB_MPBFG_BOOM",        OB_MPBFG_BOOM       },
    {"OB_MPTELEFRAG",        OB_MPTELEFRAG       },
};

static const int bex_mnemonic_table_size = arrlen(bex_mnemonic_table);

const char *DEH_StringForMnemonic(const char *mnemonic)
{
    if (!mnemonic)
    {
        I_Error("NULL passed as BEX mnemonic.");
    }

    for (int i = 0; i < bex_mnemonic_table_size; i++)
    {
        if (!strcasecmp(bex_mnemonic_table[i].mnemonic, mnemonic))
        {
            return DEH_String(bex_mnemonic_table[i].original_string);
        }
    }

    I_Error("BEX mnemonic '%s' not found! Check your lumps.", mnemonic);
}

const char * const mapnames[] =
{
    HUSTR_E1M1, HUSTR_E1M2, HUSTR_E1M3, HUSTR_E1M4, HUSTR_E1M5,
    HUSTR_E1M6, HUSTR_E1M7, HUSTR_E1M8, HUSTR_E1M9,

    HUSTR_E2M1, HUSTR_E2M2, HUSTR_E2M3, HUSTR_E2M4, HUSTR_E2M5,
    HUSTR_E2M6, HUSTR_E2M7, HUSTR_E2M8, HUSTR_E2M9,

    HUSTR_E3M1, HUSTR_E3M2, HUSTR_E3M3, HUSTR_E3M4, HUSTR_E3M5,
    HUSTR_E3M6, HUSTR_E3M7, HUSTR_E3M8, HUSTR_E3M9,

    HUSTR_E4M1, HUSTR_E4M2, HUSTR_E4M3, HUSTR_E4M4, HUSTR_E4M5,
    HUSTR_E4M6, HUSTR_E4M7, HUSTR_E4M8, HUSTR_E4M9,
};

const char * const mapnames2[] =
{
    HUSTR_1,  HUSTR_2,  HUSTR_3,  HUSTR_4,  HUSTR_5,
    HUSTR_6,  HUSTR_7,  HUSTR_8,  HUSTR_9,  HUSTR_10,
    HUSTR_11, HUSTR_12, HUSTR_13, HUSTR_14, HUSTR_15,
    HUSTR_16, HUSTR_17, HUSTR_18, HUSTR_19, HUSTR_20,
    HUSTR_21, HUSTR_22, HUSTR_23, HUSTR_24, HUSTR_25,
    HUSTR_26, HUSTR_27, HUSTR_28, HUSTR_29, HUSTR_30,
    HUSTR_31, HUSTR_32,
};

const char * const mapnamesp[] =
{
    PHUSTR_1,  PHUSTR_2,  PHUSTR_3,  PHUSTR_4,  PHUSTR_5,
    PHUSTR_6,  PHUSTR_7,  PHUSTR_8,  PHUSTR_9,  PHUSTR_10,
    PHUSTR_11, PHUSTR_12, PHUSTR_13, PHUSTR_14, PHUSTR_15,
    PHUSTR_16, PHUSTR_17, PHUSTR_18, PHUSTR_19, PHUSTR_20,
    PHUSTR_21, PHUSTR_22, PHUSTR_23, PHUSTR_24, PHUSTR_25,
    PHUSTR_26, PHUSTR_27, PHUSTR_28, PHUSTR_29, PHUSTR_30,
    PHUSTR_31, PHUSTR_32,
};

const char * const mapnamest[] =
{
    THUSTR_1,  THUSTR_2,  THUSTR_3,  THUSTR_4,  THUSTR_5,
    THUSTR_6,  THUSTR_7,  THUSTR_8,  THUSTR_9,  THUSTR_10,
    THUSTR_11, THUSTR_12, THUSTR_13, THUSTR_14, THUSTR_15,
    THUSTR_16, THUSTR_17, THUSTR_18, THUSTR_19, THUSTR_20,
    THUSTR_21, THUSTR_22, THUSTR_23, THUSTR_24, THUSTR_25,
    THUSTR_26, THUSTR_27, THUSTR_28, THUSTR_29, THUSTR_30,
    THUSTR_31, THUSTR_32,
};

const char * const strings_players[] =
{
    HUSTR_PLRGREEN, HUSTR_PLRINDIGO, HUSTR_PLRBROWN, HUSTR_PLRRED,
};

const char * const strings_quit_messages[] = 
{
    QUITMSG,   QUITMSG1,  QUITMSG2,  QUITMSG3, QUITMSG4,  QUITMSG5,
    QUITMSG6,  QUITMSG7,  QUITMSG8,  QUITMSG9, QUITMSG10, QUITMSG11,
    QUITMSG12, QUITMSG13, QUITMSG14,
};

// killough 1/18/98: remove hardcoded limit and replace with var (silly hack):
const int num_quit_mnemonics = arrlen(strings_quit_messages);

// [FG] Obituaries
static boolean HandleExtendedObituary(char *mnemonic, char *string)
{
    boolean found = false;
    int actor = MT_NULL;

    if (sscanf(mnemonic, "Obituary_Deh_Actor_%d", &actor) == 1)
    {
        if (actor >= 0 && actor < num_mobj_types)
        {
            if (M_StringEndsWith(mnemonic, "_Melee"))
            {
                if (!mobjinfo[actor].obituary_melee)
                {
                    mobjinfo[actor].obituary_melee = strdup(string);
                }
            }
            else
            {
                if (!mobjinfo[actor].obituary)
                {
                    mobjinfo[actor].obituary = strdup(string);
                }
            }

            found = true;
        }
    }

    return found;
}

static void *DEH_BEXStringsStart(deh_context_t *context, char *line)
{
    char s[10];

    if (sscanf(line, "%9s", s) == 0 || strcmp("[STRINGS]", s))
    {
        DEH_Warning(context, "Parse error on section start");
    }

    return NULL;
}

static void DEH_BEXStringsParseLine(deh_context_t *context, char *line, void *tag)
{
    char *variable_name, *value;

    if (!DEH_ParseAssignment(line, &variable_name, &value))
    {
        DEH_Warning(context, "Failed to parse assignment");
        return;
    }

    boolean matched = false;
    for (int i = 0; i < arrlen(bex_mnemonic_table); i++)
    {
        if (!strcasecmp(bex_mnemonic_table[i].mnemonic, variable_name))
        {
            DEH_AddStringReplacement(bex_mnemonic_table[i].original_string, value);
            matched = true;
        }
    }

    // [FG] Obituaries
    if (!matched)
    {
        HandleExtendedObituary(variable_name, value);
    }
}

deh_section_t deh_section_bex_strings =
{
    "[STRINGS]",
    NULL,
    DEH_BEXStringsStart,
    DEH_BEXStringsParseLine,
    NULL,
    NULL,
};

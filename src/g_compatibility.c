//
// Copyright(C) 2021 by Ryan Krafnick
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
//  DSDA Compatibility
//

#include <stdio.h>
#include <string.h>

#include "doomstat.h"
#include "doomtype.h"
#include "i_printf.h"
#include "w_wad.h"

#include "md5.h"

typedef struct
{
    int comp;
    int value;
} option_t;

typedef struct
{
    const char* cksum_string;
    const option_t options[];
} comp_record_t;

#define END {-1, 0}

static const comp_record_t eternal_doom_25 = {
  "6da6fcba8089161bdec6a1d3f6c8d60f",
  { {comp_stairs, 1}, {comp_vile, 1}, END }
};

// static const comp_record_t doomsday_of_uac_e1m8 = {
//   "32fc3115a3162b623f0d0f4e7dee6861",
//   { comp_666 }
// };

static const comp_record_t hell_revealed_map19 = {
    "811a0c97777a198bc9b2bb558cb46e6a",
    {{comp_pain, 1}, END}
};

static const comp_record_t roger_ritenour_phobos_map03 = {
    "8fa29398776146189396aa1ac6bb9e13",
    {{comp_floors, 1}, END}
};

static const comp_record_t hell_revealed_map26 = {
    "145c4dfcf843f2b92c73036ba0e1d98a",
    {{comp_vile, 1}, END}
};

static const comp_record_t hell_to_pay_map14 = {
    "5379c080299eb961792b50ad96821543",
    {{comp_vile, 1}, END}
};

static const comp_record_t hell_to_pay_map22 = {
    "7837b5334a277f107515d649bcefb682",
    {{comp_vile, 1}, END}
};

static const comp_record_t icarus_map24 = {
    "2eeb1e12fa9f9545de9d99990a4a78e5",
    {{comp_vile, 1}, END}
};

static const comp_record_t plutonia2_map32 = {
    "65a53a09a09525ae42ea210bf879cd37",
    {{comp_vile, 1}, END}
};

static const comp_record_t requiem_map23 = {
    "2499cf9a9351be9bc4e9c66fc9f291a7",
    {{comp_vile, 1}, END}
};

static const comp_record_t the_waterfront_map01 = {
    "3ca5493feff2e27bfd4181e6c4a3c2bf",
    {{comp_vile, 1}, END}
};

static const comp_record_t gather2_map05_and_darkside_map01 = {
    "cbdfefac579a62de8f1b48ca4a09d381",
    {{comp_vile, 1}, END}
};

static const comp_record_t reverie_map18 = {
    "c7a2fafb0afb2632c50ad625cdb50e51",
    {{comp_vile, 1}, END}
};

static const comp_record_t project_x_map14 = {
    "9e5724bc6135aa6f86ee54fd4d91f1e2",
    {{comp_vile, 1}, END}
};

static const comp_record_t archie_map01 = {
    "01899825ffeae016d39c02a7da4b218f",
    {{comp_vile, 1}, END}
};

static const comp_record_t seej_map01 = {
    "1d9f3afdc2517c2e450491ed13896712",
    {{comp_vile, 1}, END}
};

static const comp_record_t sixpack2_map02 = {
    "0ae745a3ab86d15fb2fb74489962c421",
    {{comp_vile, 1}, END}
};

static const comp_record_t squadron_417_map21 = {
    "2ea635c6b6aec76b6bc77448dab22f9a",
    {{comp_vile, 1}, END}
};

static const comp_record_t mayhem_2013_map05 = {
    "1e998262ee319b7d088e01de782e6b41",
    {{comp_vile, 1}, END}
};

static const comp_record_t imps_are_ghost_gods_map01 = {
    "a81e2734f735a82720d8e0f1442ba0c9",
    {{comp_vile, 1}, END}
};

static const comp_record_t confinement_map31 = {
    "aad7502cb39bc050445e17b15f72356f",
    {{comp_vile, 1}, END}
};

static const comp_record_t conf256b_map07 = {
    "5592ea1ca3b8ee0dbb2cb352aaa00911",
    {{comp_pain, 1}, END}
};

static const comp_record_t conf256b_map12 = {
    "cecedae33b970f2bf7f8b8631da0c8dd",
    {{comp_vile, 1}, END}
};

static const comp_record_t sunlust_map30 = {
    "41efe03223e41935849f64114c5cb471",
    {{comp_telefrag, 1}, END}
};

static const comp_record_t tnt_map30 = {
    "42b68b84ff8e55f264c31e6f4cfea82d",
    {{comp_stairs, 1}, END}
};

static const comp_record_t intercep2_map03 = {
    "86587e4f8c8086991c8fc5c1ccfd30b9",
    {{comp_ledgeblock, 0}, END}
};

static const comp_record_t skulltiverse_map02 = {
    "b3fa4a18b31bd96e724f9aab101776a1",
    {{comp_ledgeblock, 0}, END}
};

static const comp_record_t tntr_map30 = {
    "1d3c6d456bfcf360ce14aeecc155a96c",
    {{comp_telefrag, 1}, END}
};

static const comp_record_t roomblow_e1m1 = {
    "68ffa69f2eaa5ced3dc4da5a300d022a",
    {{comp_stairs, 1}, END}
};

static const comp_record_t esp_map21 = {
    "97088f2849904bc1cd5ae1d92d163b13",
    {{comp_stairs, 1}, END}
};

static const comp_record_t av_map07 = {
    "941e4cb56ee4184e0b1ed43486ab0bbf",
    {{comp_model, 1}, END}
};

static const comp_record_t sin2_9_map02 = {
    "9aa5aa3020434f824624eba88916ee23",
    {{comp_vile, 1}, END}
};

static const comp_record_t d2reload_map09 = {
    "c8de798a4d658ffc94151884c6c2bf37",
    {{comp_vile, 1}, END}
};

static const comp_record_t amoreupho_map02 = {
    "66a8310a0a7d2af99e3a0089b2d6c897",
    {{comp_vile, 1}, END}
};

static const comp_record_t dbp20_dnd_map07 = {
    "e26c1b6f4dfd90bb6533e6381bf61be5",
    {{comp_vile, 1}, END}
};

static const comp_record_t arch_map01 = {
    "1d37cbd32a1ecf4763437631e7b3c29a",
    {{comp_vile, 1}, END}
};

static const comp_record_t ur_map06 = {
    "cfb054683af1ed187d0565942d3dbb8f",
    {{comp_vile, 1}, END}
};

static const comp_record_t *comp_database[] = {
    &eternal_doom_25,
    &hell_revealed_map19,
    &roger_ritenour_phobos_map03,
    &hell_revealed_map26,
    &hell_to_pay_map14,
    &hell_to_pay_map22,
    &icarus_map24,
    &plutonia2_map32,
    &requiem_map23,
    &the_waterfront_map01,
    &gather2_map05_and_darkside_map01,
    &reverie_map18,
    &project_x_map14,
    &archie_map01,
    &seej_map01,
    &sixpack2_map02,
    &squadron_417_map21,
    &mayhem_2013_map05,
    &imps_are_ghost_gods_map01,
    &confinement_map31,
    &conf256b_map07,
    &conf256b_map12,
    &sunlust_map30,
    &tnt_map30,
    &intercep2_map03,
    &skulltiverse_map02,
    &tntr_map30,
    &roomblow_e1m1,
    &esp_map21,
    &av_map07,
    &sin2_9_map02,
    &d2reload_map09,
    &amoreupho_map02,
    &dbp20_dnd_map07,
    &arch_map01,
    &ur_map06,
};

typedef struct
{
    byte bytes[16];
    char string[33];
} md5_checksum_t;

static void MD5UpdateLump(int lump, struct MD5Context *md5)
{
    MD5Update(md5, W_CacheLumpNum(lump, PU_CACHE), W_LumpLength(lump));
}

static void GetLevelCheckSum(int lump, md5_checksum_t* cksum)
{
    struct MD5Context md5;

    MD5Init(&md5);

    MD5UpdateLump(lump + ML_LABEL, &md5);
    MD5UpdateLump(lump + ML_THINGS, &md5);
    MD5UpdateLump(lump + ML_LINEDEFS, &md5);
    MD5UpdateLump(lump + ML_SIDEDEFS, &md5);
    MD5UpdateLump(lump + ML_SECTORS, &md5);

    // ML_BEHAVIOR when it becomes applicable to comp options

    MD5Final(cksum->bytes, &md5);

    for (int i = 0; i < sizeof(cksum->bytes); ++i)
    {
        sprintf(&cksum->string[i * 2], "%02x", cksum->bytes[i]);
    }
    cksum->string[32] = '\0';
}

// For casual players that aren't careful about setting complevels, this
// function will apply comp options to automatically fix some issues that
// appear when playing wads in mbf21 (since this is the default).

void G_ApplyLevelCompatibility(int lump)
{
    if (demorecording || demoplayback || netgame || !mbf21)
    {
        return;
    }

    static boolean restore_comp;
    static int old_comp[COMP_TOTAL];

    if (restore_comp)
    {
        memcpy(comp, old_comp, sizeof(*comp));
        restore_comp = false;
    }

    md5_checksum_t cksum;

    GetLevelCheckSum(lump, &cksum);

    I_Printf(VB_DEBUG, "Level checksum: %s", cksum.string);

    for (int i = 0; i < arrlen(comp_database); ++i)
    {
        const comp_record_t *record = comp_database[i];

        if (!strcmp(record->cksum_string, cksum.string))
        {
            memcpy(old_comp, comp, sizeof(*comp));
            restore_comp = true;

            for (int k = 0; record->options[k].comp != -1; ++k)
            {
                option_t option = record->options[k];

                comp[option.comp] = option.value;

                I_Printf(VB_INFO, "Automatically setting comp option %d to %s.",
                         option.comp, option.value ? "on" : "off");
            }

            return;
        }
    }
}

//
//  Copyright(C) 2021 Roman Fomin
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
// DESCRIPTION:
//      DSDHacked support

#include <stdlib.h>
#include <string.h>

#include "doomtype.h"
#include "dsdhacked.h"

state_t* states;
int num_states;
byte* defined_codeptr_args;
statenum_t* seenstate_tab;
actionf_t* deh_codeptr;

void dsdh_InitTables(void)
{
  int i;

  num_states = NUMSTATES;

  states = malloc(num_states * sizeof(*states));
  memcpy(states, mbf_states, num_states * sizeof(*states));

  seenstate_tab = calloc(num_states, sizeof(*seenstate_tab));

  deh_codeptr = malloc(num_states * sizeof(*deh_codeptr));
  for (i = 0; i < num_states; i++)
    deh_codeptr[i] = states[i].action;

  defined_codeptr_args = calloc(num_states, sizeof(*defined_codeptr_args));
}

void dsdh_CheckStatesCapacity(int limit)
{
  while (limit >= num_states)
  {
    int i;

    int old_num_states = num_states;

    num_states *= 2;

    states = realloc(states, num_states * sizeof(*states));
    memset(states + old_num_states, 0, (num_states - old_num_states) * sizeof(*states));

    deh_codeptr = realloc(deh_codeptr, num_states * sizeof(*deh_codeptr));
    memset(deh_codeptr + old_num_states, 0, (num_states - old_num_states) * sizeof(*deh_codeptr));

    defined_codeptr_args =
      realloc(defined_codeptr_args, num_states * sizeof(*defined_codeptr_args));
    memset(defined_codeptr_args + old_num_states, 0,
      (num_states - old_num_states) * sizeof(*defined_codeptr_args));

    seenstate_tab = realloc(seenstate_tab, num_states * sizeof(*seenstate_tab));
    memset(seenstate_tab + old_num_states, 0,
      (num_states - old_num_states) * sizeof(*seenstate_tab));

    for (i = old_num_states; i < num_states; ++i)
    {
      states[i].sprite = SPR_TNT1;
      states[i].tics = -1;
      states[i].nextstate = i;
    }
  }
}

void dsdh_FreeDehStates(void)
{
  free(defined_codeptr_args);
  free(deh_codeptr);
}

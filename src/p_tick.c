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
// DESCRIPTION:
//      Thinker, Ticker.
//
//-----------------------------------------------------------------------------

#include "d_think.h"
#include "doomdef.h"
#include "doomstat.h"
#include "info.h"
#include "m_arena.h"
#include "p_ambient.h"
#include "p_map.h"
#include "p_mobj.h"
#include "p_tick.h"
#include "p_spec.h"
#include "p_user.h"
#include "s_musinfo.h"

int leveltime;
int oldleveltime;

//
// THINKERS
// All thinkers should be allocated by Z_Malloc
// so they can be operated on uniformly.
// The actual structures will vary in size,
// but the first element must be thinker_t.
//

// Both the head and tail of the thinker list.
thinker_t thinkercap = {0};

// killough 8/29/98: we maintain several separate threads, each containing
// a special class of thinkers, to allow more efficient searches. 

thinker_t thinkerclasscap[NUMTHCLASS] = {0};

int init_thinkers_count = 0;

arena_t *thinkers_arena;

//
// P_InitThinkers
//

void P_InitThinkers(void)
{
  int i;

  for (i=0; i<NUMTHCLASS; i++)  // killough 8/29/98: initialize threaded lists
    thinkerclasscap[i].cprev = thinkerclasscap[i].cnext = &thinkerclasscap[i];

  thinkercap.prev = thinkercap.next  = &thinkercap;

  init_thinkers_count++;
}

//
// P_UpdateThinker
//
// killough 8/29/98:
// 
// We maintain separate threads of friends and enemies, to permit more
// efficient searches.
//
// haleyjd 6/17/08: Import from EE: PrBoom bugfixes, including addition of the
// th_delete thinker class to rectify problems with infinite loops in the AI
// code due to corruption of the th_enemies/th_friends lists when monsters get
// removed at an inopportune moment.
//
void P_UpdateThinker(thinker_t *thinker)
{
   register thinker_t *th;

   // find the class the thinker belongs to
  
   // haleyjd 07/12/03: don't use "class" as a variable name
   int tclass = (thinker->function.p1 == P_RemoveMobjThinkerDelayed) ? th_delete :
     thinker->function.p1 == P_MobjThinker &&
     ((mobj_t *) thinker)->health > 0 && 
     (((mobj_t *) thinker)->flags & MF_COUNTKILL ||
      ((mobj_t *) thinker)->type == MT_SKULL) ?
     ((mobj_t *) thinker)->flags & MF_FRIEND ?
     th_friends : th_enemies : th_misc;

   // Remove from current thread, if in one -- haleyjd: from PrBoom
   if((th = thinker->cnext) != NULL)
      (th->cprev = thinker->cprev)->cnext = th;
  
   // Add to appropriate thread
   th = &thinkerclasscap[tclass];
   th->cprev->cnext = thinker;
   thinker->cnext = th;
   thinker->cprev = th->cprev;
   th->cprev = thinker;
}

//
// P_AddThinker
// Adds a new thinker at the end of the list.
//

void P_AddThinker(thinker_t* thinker)
{
  thinkercap.prev->next = thinker;
  thinker->next = &thinkercap;
  thinker->prev = thinkercap.prev;
  thinkercap.prev = thinker;

  thinker->references = 0;    // killough 11/98: init reference counter to 0

  // killough 8/29/98: set sentinel pointers, and then add to appropriate list
  thinker->cnext = thinker->cprev = thinker;
  P_UpdateThinker(thinker);
}

//
// killough 11/98:
//
// Make currentthinker external, so that P_RemoveThinkerDelayed
// can adjust currentthinker when thinkers self-remove.

static thinker_t *currentthinker;

//
// P_RemoveThinkerDelayed()
//
// Called automatically as part of the thinker loop in P_RunThinkers(),
// on nodes which are pending deletion.
//
// If this thinker has no more pointers referencing it indirectly,
// remove it, and set currentthinker to one node preceeding it, so
// that the next step in P_RunThinkers() will get its successor.
//
inline static void RemoveThinker(thinker_t *thinker)
{
    if (thinker->references)
    {
        return;
    }

    thinker_t *next = thinker->next;
    (next->prev = currentthinker = thinker->prev)->next = next;

    // haleyjd 6/17/08: remove from threaded list now
    (thinker->cnext->cprev = thinker->cprev)->cnext = thinker->cnext;

    arena_free(thinkers_arena, thinker);
} 

void P_RemoveMobjThinkerDelayed(mobj_t *mobj)
{
    RemoveThinker(&mobj->thinker);
}

void P_RemoveCeilingThinkerDelayed(mobj_t *mobj)
{
    RemoveThinker(&mobj->thinker);
}

void P_RemoveDoorThinkerDelayed(mobj_t *mobj)
{
    RemoveThinker(&mobj->thinker);
}

void P_RemoveFloorThinkerDelayed(mobj_t *mobj)
{
    RemoveThinker(&mobj->thinker);
}

void P_RemoveElevatorThinkerDelayed(mobj_t *mobj)
{
    RemoveThinker(&mobj->thinker);
}

void P_RemovePlatThinkerDelayed(mobj_t *mobj)
{
    RemoveThinker(&mobj->thinker);
}

void P_RemoveAmbientThinkerDelayed(mobj_t *mobj)
{
    RemoveThinker(&mobj->thinker);
}

//
// P_RemoveThinker
//
// Deallocation is lazy -- it will not actually be freed
// until its thinking turn comes up.
//
// killough 4/25/98:
//
// Instead of marking the function with -1 value cast to a function pointer,
// set the function to P_RemoveThinkerDelayed(), so that later, it will be
// removed automatically as part of the thinker process.
//
#if 0
void P_RemoveThinker(thinker_t *thinker)
{
   thinker->function.pt = P_RemoveThinkerDelayed;
   
   // killough 8/29/98: remove immediately from threaded list

   // haleyjd 06/17/08: Import from EE:
   // NO! Doing this here was always suspect to me, and
   // sure enough: if a monster's removed at the wrong time, it gets put
   // back into the list improperly and starts causing an infinite loop in
   // the AI code. We'll follow PrBoom's lead and create a th_delete class
   // for thinkers awaiting deferred removal.
   
   // Old code:
   //(thinker->cnext->cprev = thinker->cprev)->cnext = thinker->cnext;
   
   // Move to th_delete class.
   P_UpdateThinker(thinker);
}
#endif

void P_RemoveMobjThinker(mobj_t *mobj)
{
   mobj->thinker.function.p1 = P_RemoveMobjThinkerDelayed;
   P_UpdateThinker(&mobj->thinker);
}

void P_RemoveCeilingThinker(ceiling_t *ceiling)
{
   ceiling->thinker.function.p1 = P_RemoveCeilingThinkerDelayed;
   P_UpdateThinker(&ceiling->thinker);
}

void P_RemoveDoorThinker(vldoor_t *door)
{
   door->thinker.function.p1 = P_RemoveDoorThinkerDelayed;
   P_UpdateThinker(&door->thinker);
}

void P_RemoveFloorThinker(floormove_t *floor)
{
   floor->thinker.function.p1 = P_RemoveFloorThinkerDelayed;
   P_UpdateThinker(&floor->thinker);
}

void P_RemoveElevatorThinker(elevator_t *elevator)
{
   elevator->thinker.function.p1 = P_RemoveElevatorThinkerDelayed;
   P_UpdateThinker(&elevator->thinker);
}

void P_RemovePlatThinker(plat_t *plat)
{
   plat->thinker.function.p1 = P_RemovePlatThinkerDelayed;
   P_UpdateThinker(&plat->thinker);
}

void P_RemoveAmbientThinker(ambient_t *ambient)
{
   ambient->thinker.function.p1 = P_RemoveAmbientThinkerDelayed;
   P_UpdateThinker(&ambient->thinker);
}

//
// P_SetTarget
//
// This function is used to keep track of pointer references to mobj thinkers.
// In Doom, objects such as lost souls could sometimes be removed despite 
// their still being referenced. In Boom, 'target' mobj fields were tested
// during each gametic, and any objects pointed to by them would be prevented
// from being removed. But this was incomplete, and was slow (every mobj was
// checked during every gametic). Now, we keep a count of the number of
// references, and delay removal until the count is 0.
//

void P_SetTarget(mobj_t **mop, mobj_t *targ)
{
  if (*mop)             // If there was a target already, decrease its refcount
    (*mop)->thinker.references--;
  if ((*mop = targ))    // Set new target and if non-NULL, increase its counter
    targ->thinker.references++;
}

//
// P_RunThinkers
//
// killough 4/25/98:
//
// Fix deallocator to stop using "next" pointer after node has been freed
// (a Doom bug).
//
// Process each thinker. For thinkers which are marked deleted, we must
// load the "next" pointer prior to freeing the node. In Doom, the "next"
// pointer was loaded AFTER the thinker was freed, which could have caused
// crashes.
//
// But if we are not deleting the thinker, we should reload the "next"
// pointer after calling the function, in case additional thinkers are
// added at the end of the list.
//
// killough 11/98:
//
// Rewritten to delete nodes implicitly, by making currentthinker
// external and using P_RemoveThinkerDelayed() implicitly.
//

static void P_RunThinkers (void)
{
  for (currentthinker = thinkercap.next;
       currentthinker != &thinkercap;
       currentthinker = currentthinker->next)
    if (currentthinker->function.p1)
      currentthinker->function.p1((mobj_t *)currentthinker);

  // [crispy] support MUSINFO lump (dynamic music changing)
  T_MusInfo();
}

static void P_FrozenTicker (void)
{
  int i;

  P_MapStart();

  if (gamestate == GS_LEVEL)
  {
    thinker_t* th;
    mobj_t* mo;

    for (i = 0; i < MAXPLAYERS; i++)
      if (playeringame[i])
        P_PlayerThink(&players[i]);

    for (i = 0; i < MAXPLAYERS; i++)
      if (playeringame[i])
        P_MobjThinker(players[i].mo);

    for (th = thinkercap.next; th != &thinkercap; th = th->next)
      if (th->function.p1 == P_MobjThinker)
      {
        mo = (mobj_t *) th;

        if (mo->player && mo->player == &players[displayplayer])
          continue;

        mo->interp = 0;
      }
  }

  P_MapEnd();
}

//
// P_Ticker
//

void P_Ticker (void)
{
  int i;

  // pause if in menu and at least one tic has been run
  //
  // killough 9/29/98: note that this ties in with basetic,
  // since G_Ticker does the pausing during recording or
  // playback, and compensates by incrementing basetic.
  // 
  // All of this complicated mess is used to preserve demo sync.

  if (paused || (menuactive && (!demoplayback || menu_pause_demos) && !netgame &&
		 players[consoleplayer].viewz != 1))
    return;

  if (frozen_mode)
  {
    P_FrozenTicker();
  }
  else
  {
  P_MapStart();
  if (gamestate == GS_LEVEL)
  {
  for (i=0; i<MAXPLAYERS; i++)
    if (playeringame[i])
      P_PlayerThink(&players[i]);
  }

  P_RunThinkers();
  P_UpdateSpecials();
  P_RespawnSpecials();
  P_MapEnd();
  }

  leveltime++;                       // for par times
}

//----------------------------------------------------------------------------
//
// $Log: p_tick.c,v $
// Revision 1.7  1998/05/15  00:37:56  killough
// Remove unnecessary crash hack, fix demo sync
//
// Revision 1.6  1998/05/13  22:57:59  killough
// Restore Doom bug compatibility for demos
//
// Revision 1.5  1998/05/03  22:49:01  killough
// Get minimal includes at top
//
// Revision 1.4  1998/04/29  16:19:16  killough
// Fix typo causing game to not pause correctly
//
// Revision 1.3  1998/04/27  01:59:58  killough
// Fix crashes caused by thinkers being used after freed
//
// Revision 1.2  1998/01/26  19:24:32  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:01  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------

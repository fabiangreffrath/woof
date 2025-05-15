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
//      Zone Memory Allocation. Neat.
//
// Neat enough to be rewritten by Lee Killough...
//
// Must not have been real neat :)
//
// Made faster and more general, and added wrappers for all of Doom's
// memory allocation functions, including malloc() and similar functions.
// Added line and file numbers, in case of error. Added performance
// statistics and tunables.
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <string.h>

#include "z_zone.h"

#include "i_system.h"

// Minimum chunk size at which blocks are allocated
#define CHUNK_SIZE sizeof(void *)

// signature for block header
#define ZONEID  0x931d4a11

typedef struct memblock {
  struct memblock *next, *prev;
  size_t size;
  void **user;
  unsigned id;
  pu_tag tag;
} memblock_t;

static const size_t HEADER_SIZE = (sizeof(memblock_t)+CHUNK_SIZE-1) & ~(CHUNK_SIZE-1);

static memblock_t *blockbytag[PU_MAX];

// Z_Malloc
// You can pass a NULL user if the tag is < PU_CACHE.

void *Z_Malloc(size_t size, pu_tag tag, void **user)
{
  memblock_t *block = NULL;

  if (tag == PU_CACHE && !user)
    I_Error ("An owner is required for purgable blocks");

  if (!size)
    return user ? *user = NULL : NULL;           // malloc(0) returns NULL

  while (!(block = malloc(size + HEADER_SIZE)))
  {
    if (!blockbytag[PU_CACHE])
      I_Error ("Failure trying to allocate %lu bytes", (unsigned long) size);
    Z_FreeTag(PU_CACHE);
  }

  if (!blockbytag[tag])
  {
    blockbytag[tag] = block;
    block->next = block->prev = block;
  }
  else
  {
    blockbytag[tag]->prev->next = block;
    block->prev = blockbytag[tag]->prev;
    block->next = blockbytag[tag];
    blockbytag[tag]->prev = block;
  }

  block->size = size;
  block->id = ZONEID;         // signature required in block header
  block->tag = tag;           // tag
  block->user = user;         // user
  block = (memblock_t *)((char *) block + HEADER_SIZE);
  if (user)                   // if there is a user
    *user = block;            // set user to point to new block

  return block;
}

void Z_Free(void *p)
{
  memblock_t *block;

  if (!p)
    return;

  block = (memblock_t *)((char *) p - HEADER_SIZE);

  if (block->id != ZONEID)
    I_Error("freed a pointer without ZONEID");
  block->id = 0;              // Nullify id so another free fails

  if (block->user)            // Nullify user if one exists
    *block->user = NULL;

  if (block == block->next)
    blockbytag[block->tag] = NULL;
  else
    if (blockbytag[block->tag] == block)
      blockbytag[block->tag] = block->next;
  block->prev->next = block->next;
  block->next->prev = block->prev;

  free(block);
}

void Z_FreeTag(pu_tag tag)
{
  memblock_t *block, *end_block;

  if (tag < 0 || tag >= PU_MAX)
    I_Error("Tag %i does not exist", tag);

  block = blockbytag[tag];
  if (!block)
    return;
  end_block = block->prev;
  while (1)
  {
    memblock_t *next = block->next;
    Z_Free((char *) block + HEADER_SIZE);
    if (block == end_block)
      break;
    block = next;               // Advance to next block
  }
}

void Z_ChangeTag(void *ptr, pu_tag tag)
{
  memblock_t *block = (memblock_t *)((char *) ptr - HEADER_SIZE);

  // proff - added sanity check, this can happen when an empty lump is locked
  if (!ptr)
    return;

  // proff - do nothing if tag doesn't differ
  if (tag == block->tag)
    return;

  if (block->id != ZONEID)
    I_Error ("freed a pointer without ZONEID");

  if (tag == PU_CACHE && !block->user)
    I_Error ("an owner is required for purgable blocks\n");

  if (block == block->next)
    blockbytag[block->tag] = NULL;
  else
    if (blockbytag[block->tag] == block)
      blockbytag[block->tag] = block->next;
  block->prev->next = block->next;
  block->next->prev = block->prev;

  if (!blockbytag[tag])
  {
    blockbytag[tag] = block;
    block->next = block->prev = block;
  }
  else
  {
    blockbytag[tag]->prev->next = block;
    block->prev = blockbytag[tag]->prev;
    block->next = blockbytag[tag];
    blockbytag[tag]->prev = block;
  }

  block->tag = tag;
}

void *Z_Realloc(void *ptr, size_t n, pu_tag tag, void **user)
{
  void *p = Z_Malloc(n, tag, user);
  if (ptr)
    {
      memblock_t *block = (memblock_t *)((char *) ptr - HEADER_SIZE);
      memcpy(p, ptr, n <= block->size ? n : block->size);
      Z_Free(ptr);
      if (user) // in case Z_Free nullified same user
        *user=p;
    }
  return p;
}

void *Z_Calloc(size_t n1, size_t n2, pu_tag tag, void **user)
{
  return
    (n1*=n2) ? memset(Z_Malloc(n1, tag, user), 0, n1) : NULL;
}

char *Z_StrDup(const char *orig, pu_tag tag)
{
  size_t size = strlen(orig) + 1;

  char *result = Z_Malloc(size, tag, NULL);

  memcpy(result, orig, size);

  return result;
}

//-----------------------------------------------------------------------------
//
// $Log: z_zone.c,v $
// Revision 1.13  1998/05/12  06:11:55  killough
// Improve memory-related error messages
//
// Revision 1.12  1998/05/03  22:37:45  killough
// beautification
//
// Revision 1.11  1998/04/27  01:49:39  killough
// Add history of malloc/free and scrambler (INSTRUMENTED only)
//
// Revision 1.10  1998/03/28  18:10:33  killough
// Add memory scrambler for debugging
//
// Revision 1.9  1998/03/23  03:43:56  killough
// Make Z_CheckHeap() more diagnostic
//
// Revision 1.8  1998/03/02  11:40:02  killough
// Put #ifdef CHECKHEAP around slow heap checks (debug)
//
// Revision 1.7  1998/02/02  13:27:45  killough
// Additional debug info turned on with #defines
//
// Revision 1.6  1998/01/26  19:25:15  phares
// First rev with no ^Ms
//
// Revision 1.5  1998/01/26  07:15:43  phares
// Added rcsid
//
// Revision 1.4  1998/01/26  06:12:30  killough
// Fix memory usage problems and improve debug stat display
//
// Revision 1.3  1998/01/22  05:57:20  killough
// Allow use of virtual memory when physical memory runs out
//
// ???
//
//-----------------------------------------------------------------------------

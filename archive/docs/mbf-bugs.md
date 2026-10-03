PrBoom - MBF Source Notes 

[Project Homepage](http://sourceforge.net/projects/prboom)

MBF Code Notes
==============

What this is
------------

Unless you're a source port coder working with either the Boom or MBF source code (or code derived from them), you don't want to read this.

As you know, MBF is not maintained by anyone, and like all software it has some bugs, This page is intended as a reference on all the bugs in the last release of MBF, so people working from its code don't have to repeat all my debugging again in order to find them for themselves.

I don't list all the bugs in the Boom source, because there were lots of bugs in Boom that were fixed by MBF; see the MBF docs for a list. You should start from the MBF or PrBoom source if you're starting a new port, not the Boom source. If you are working from Boom (or, indeed, even from original Doom, which is even less advisable) then the list below is still relevant (where noted), but just remember that there are all the fixes from MBF that you need to apply too.

This page used to carry PrBoom bugs, but since PrBoom is active these days there's no need, all bugs that get found, get fixed. Similarly this page no longer contains info on BSP bugs.

Those bugs here which are not otherwise attributed, were found and fixed by myself ([Colin Phipps](mailto:cph@moria.org.uk)). Also I'd like to thank [James "Quasar" Haley](mailto:haleyjd@concentric.net), and [Simon "Fraggle" Howard](mailto:fraggle@alkali.org), who discovered some of these bugs, and are always glad to share this information :-).

Fixed
-----

For a lot of the bugs I now give source fixes. These are in UNIX diff(1) format, which hopefully is clear even to those not familiar with it. Note that the patches are usually generated from the PrBoom or LxDoom CVS, so they probably won't apply cleanly on the original Boom/MBF sources. You're a coder, you can figure it out.

Bugs
----

I've divided them into three sections now:

*   Programming bugs, which cause the game to lose data, crash, or such.
*   Game logic bugs, which cause things in the game to behave wrongly.
*   Demo sync bugs, which cause original Doom demos to fail to play back correctly.

### Programming bugs

#### Minor save glitch

Affects: Boom v2.02, PrBoom v2.02

Symptoms: Highly unlikely to ever occur, but it might show up in a level with a lot of sectors, with a ZONE_ID crash or SEGV saving a game.

Description: Boom v2.02 added a soundtargets field to the data stored with each sector, but the code to check if there was enough space in the savegame buffer was not updated with the extra size. cf PrBoom p_saveg.c, P_ArchiveWorld().

#### VM block size bug

Affects: MBF, Boom v2.02, PrBoom v2.02

Symptoms: Saving a game in a large level, or loading a large savegame (savegame would have to be >64k) for the first time, when memory is tight and fragmented, produces a corrupt savegame file if writing, or loses data when reading. Also, on machines with tight memory, when INSTRUMENTED is defined, the libc heap (not the Doom heap) can become corrupted.

Description: cf MBF, z_zone.c Z_Malloc, line 371. The Z_Malloc code fails to store the block size in the header for memory blocks allocated outside of the normal heap. This field is unused for "vm" blocks (hence the bug is rarely noticed), _except_ when doing a Z_Realloc() (as when enlarging the savegame buffer); data will failed to be copied to the new block. A "vm" memory block is only allocated when no memory block in Doom's own heap is suitable. Fix follows:

```diff
***
@@ -363,13 +447,20 @@ allocated:
   block->vm = 1;

 #ifdef INSTRUMENTED
-  virtual_memory += block->size = size + HEADER_SIZE;
+  virtual_memory += size + HEADER_SIZE;
 #endif
+  /* cph - the next line was lost in the #ifdef above, and also added an
+   *  extra HEADER_SIZE to block->size, which was incorrect */
+  block->size = size;

   goto allocated;
 }
     
```

#### SEGV when falling down long vertical shaft

Affects: MBF, and probably Boom

Symptoms: E.g. MAP26 of Hell Revealed. Play with -nomonsters. Walk north east from the start until you teleport. You are in a weird corridor, walk to the end _slowly_. You teleport again, to a ledge at the top o a long vertical shaft going down. If you walked slowly, you probably won't be thrown off of the ledge. As you walk off, the game may crash, depending on the amount of memory allocated, the position of the ylookup[] array in that memory, and whether the OS detects the SEGV (it does in Linux, but I guess DOS might let it pass).

Description: On x86 machines, an assembly code version of R_DrawColumn is used, which has the instructions reordered to make it faster. It happens that the ylookup[] lookup is done before the pixel count is checked. If the pixel begin or end values are way out of range, this causes a SEGV. The memory accessed by the out-of-range array index is not used (in the out-of-range case), so provided the OS doesn't detect the SEGV there are no ill effects. I've had it confirmed that this does affect MBF in extreme cases.

#### Memory wastage

Affects: Boom, PrBoom v2.02, MBF

Symptoms: Memory is wasted by data from previous games or levels, which should have been purged.

Description: If you add debugging to show used memory immediately after a Z_FreeTags, you'll see that not all blocks with the tags being freed get freed. The algorithm used to walk the heap in Z_FreeTags is buggy. Fix follows:

```diff
@@ -480,9 +572,21 @@ void (Z_FreeTags)(int lowtag, int highta
   do               // Scan through list, searching for tags in range
     if (block->tag >= lowtag && block->tag <= hightag)
       {
-        memblock_t *prev = block->prev;
+        memblock_t *prev = block->prev, *cur = block;
         (Z_Free)((char *) block + HEADER_SIZE, file, line);
-        block = prev->next;
+       /* cph - be more careful here, we were skipping blocks!
+        * If the current block was not merged with the previous,
+        *  cur is still a valid pointer, prev->next == cur, and cur is
+        *  already free so skip to the next.
+        * If the current block was merged with the previous,
+        *  the next block to analyse is prev->next.
+        * Note that the while() below does the actual step forward
+        */
+        block = (prev->next == cur) ? cur : prev;
       }
   while ((block=block->next) != zone);

     
```

#### Poor memory scanning

Affects: Boom, PrBoom v2.02, MBF

Symptoms: The efficiency of memory usage in Boom/MBF rarely exceeds 60%

Description: Sometimes when cache memory is freed in order to make space for new memory allocations, the freed memory is immediately skipped over, so this space is not taken advantage of until the next cycle round the heap. Fixed by being more careful in the block searching code.

```diff
***************
*** 359,365 ****
          {                                   // replacement is roughly FIFO
            start = block->prev;
            Z_Free((char *) block + HEADER_SIZE);
!           block = start = start->next;      // Important: resets start
          }

        if (block->tag == PU_FREE && block->size >= size)   // First-fit
--- 358,369 ----
          {                                   // replacement is roughly FIFO
            start = block->prev;
            Z_Free((char *) block + HEADER_SIZE);
!           /* cph - If start->next == block, we did not merge with the previous
!            *       If !=, we did, so we continue from start.
!            *  Important: we've reset start
!            */
!           if (start->next == block) start = start->next;
!           else block = start;
          }

        if (block->tag == PU_FREE && block->size >= size)   // First-fit
***************
```

#### Format string bugs

Affects: Boom, MBF, PrBoom v2.02, v2.03beta

Symptoms: Malicious WAD/DEH/BEX files could potentially run arbitrary code on your computer (read: trojan WAD files)

Description: Boom and MBF allow strings displayed by the game to be modified by dehacked patch files (.deh or .bex files). The string handling is not as careful as it should be in some places; specifically sometimes these strings are passed as the format arguments to \*printf calls. As widely reported on security mailing lists recently, by supplying malformed strings as a format string it is possible for the person supplying the strings to cause arbitrary code to be run.

So potentially, a malicious person could write a dehacked patch which did nasty things to your computer. So beware malicious .deh or .bex files. Also, MBF will interpret a DEHACKED lump inside a .wad file as a dehacked patch, so beware malicious .wad files too.

Fix is simple, supply a format string correctly. For Boom v2.02:

```diff
--- d_main.c        Sat Oct  9 15:44:42 1999
+++ d_main.c.fixed        Sat Sep 23 18:54:58 2000
@@ -1572,11 +1572,11 @@ void D_DoomMain(void)
   // Ty 04/08/98 - Add 5 lines of misc. data, only if nonblank
   // The expectation is that these will be set in a .bex file
   //jff 9/3/98 use logical output routine
-  if (*startup1) lprintf(LO_INFO,startup1);
-  if (*startup2) lprintf(LO_INFO,startup2);
-  if (*startup3) lprintf(LO_INFO,startup3);
-  if (*startup4) lprintf(LO_INFO,startup4);
-  if (*startup5) lprintf(LO_INFO,startup5);
+  if (*startup1) lprintf(LO_INFO,"%s",startup1);
+  if (*startup2) lprintf(LO_INFO,"%s",startup2);
+  if (*startup3) lprintf(LO_INFO,"%s",startup3);
+  if (*startup4) lprintf(LO_INFO,"%s",startup4);
+  if (*startup5) lprintf(LO_INFO,"%s",startup5);
   // End new startup strings

   //jff 9/3/98 use logical output routine
```

And the following applies to both Boom and MBF

```diff
--- d_deh.c     Sat Oct  9 15:44:42 1999
+++ d_deh.c.fixed       Sat Sep 23 19:13:10 2000
@@ -401,8 +401,10 @@ deh_strs deh_strlookup[] = {
   {&s_QLOADNET,"QLOADNET"},
   {&s_QSAVESPOT,"QSAVESPOT"},
   {&s_SAVEDEAD,"SAVEDEAD"},
+/* cph - disabled to prevent format string attacks
   {&s_QSPROMPT,"QSPROMPT"},
   {&s_QLPROMPT,"QLPROMPT"},
+ */
   {&s_NEWGAME,"NEWGAME"},
   {&s_NIGHTMARE,"NIGHTMARE"},
   {&s_SWSTRING,"SWSTRING"},
--- i_main.c        Tue Feb 15 16:08:02 2000
+++ i_main.c.fixed        Sat Sep 23 18:52:55 2000
@@ -60,7 +60,7 @@ static void handler(int s)
   if (s==SIGSEGV || s==SIGILL || s==SIGFPE)
     Z_DumpHistory(buf);

-  I_Error(buf);
+  I_Error("%s",buf);
 }

 void I_Quit(void);
```

### Game logic bugs

Some of these bug fixes change the game behaviour and so people might not like them. It's your choice.

#### Medikit health

Affects: Doom, Boom, PrBoom v2.02, MBF

Symptoms: The message "You picked up a medikit that you REALLY need!" is never shown

Found by: [Quasar](mailto:haleyjd@concentric.net)

Description: cf MBF line 419. The test to see if the player had very low health is after the health for the medikit is added, so the low-health condition is never met.

#### Player grunt

Affects: Doom, Boom, PrBoom v2.02, MBF

Symptoms: The player grunt sound is heard when hitting the ground, even when the player is already dead (e.g. high archville toss)

Found by: [Quasar](mailto:haleyjd@concentric.net)

Description: There is no test that the player is alive when playing the grunt sound. 'nuff said.

#### 3-key door works with only 2 keys

Affects: MBF

Found by: Fraggle

Symptoms: The generalised line type for a door which needs all 3 keys doesn't work right, it lets you through with only the red and blue keys.

Description: A simple typo introduced by some code cleaning/reformatting in MBF, in p_spec.c:P_CanUnlockGenDoor. Simple fix is:

```diff
--- p_spec.c.orig        Wed Aug 23 23:46:49 2000
+++ p_spec.c        Wed Aug 23 23:47:12 2000
@@ -810,7 +810,7 @@ boolean P_CanUnlockGenDoor(line_t *line,
       if (skulliscard &&
           (!(player->cards[it_redcard] | player->cards[it_redskull]) ||
            !(player->cards[it_bluecard] | player->cards[it_blueskull]) ||
-           !(player->cards[it_yellowcard] | !player->cards[it_yellowskull])))
+           !(player->cards[it_yellowcard] | player->cards[it_yellowskull])))
         {
           player->message = s_PD_ALL3; // Ty 03/27/98 - externalized
           S_StartSound(player->mo,sfx_oof);             // killough 3/20/98
      
```

Although you might prefer a more complicated fix which allowed for compatibility with MBF's buggy behaviour.

#### Spawned monsters respawn at 0,0

Affects: Boom, MBF, PrBoom before v2.1.x

Found by: Quasar

Description: Monsters which aren't on the map to start with (e.g. monsters spawner by a boss cube thrower) which respawn, will appear at 0,0 — which might be a bad place (e.g. solid rock).

The spawnpoint fields in the data structure for a spawned mobj will be 0,0. Hence if the monster gets killed and then respawns, it would appear at 0,0. This might leave the monsters stuck in rock, for instance.

Eternity fixed this by making monsters respawn at the place where they died, and PrBoom did a similar fix. Thanks, Quasar.

```diff
*** p_mobj.c        Sun Apr  1 12:06:31 2001
--- p_mobj.c.orig        Sun Apr  1 12:05:26 2001
*************** void P_NightmareRespawn(mobj_t* mobj)
*** 572,577 ****
--- 572,596 ----
    x = mobj->spawnpoint.x << FRACBITS;
    y = mobj->spawnpoint.y << FRACBITS;

+   /* haleyjd: stupid nightmare respawning bug fix
+    *
+    * 08/09/00: compatibility added, time to ramble :)
+    * This fixes the notorious nightmare respawning bug that causes monsters
+    * that didn't spawn at level startup to respawn at the point (0,0)
+    * regardless of that point's nature. SMMU and Eternity need this for
+    * script-spawned things like Halif Swordsmythe, as well.
+    *
+    * cph - copied from eternity, except comp_respawnfix becomes comp_respawn
+    *   and the logic is reversed (i.e. like the rest of comp_ it *disables*
+    *   the fix)
+    */
+   if(!comp[comp_respawn] && !x && !y)
+   {
+      // spawnpoint was zeroed out, so use point of death instead
+      x = mobj->x;
+      y = mobj->y;
+   }
+
    // something is occupying its position?

    if (!P_CheckPosition (mobj, x, y) )
      
```
(Note that the above requires a new compatibility option, comp_respawn.)

#### DR doors corrupt other actions

Affects: Boom, MBF, PrBoom up to and including v2.2.1

Found by: [Adam Williamson](mailto:adam@scisoft.force9.co.uk)

Description: In original Doom, a DR door would refuse to open if the same sector had a lighting or floor thinker in progress; this behaviour was changed in Boom but not compatibility optioned. cf ep1-0511.lmp (see Compet-n)

In fact, underlying this is a bug in original Doom which is still present (at least in the Boom ports). If you try to trigger a DR door with an existing ceiling action, it assumes the existing action must be a door action and merely changes the direction. But if there's a different type of action going on, it will be corrupting some field in that action.

```diff
Index: p_doors.c
===================================================================
RCS file: /cvsroot/prboom/prboom2/src/p_doors.c,v
retrieving revision 1.6
diff -p -u -r1.6 p_doors.c
--- p_doors.c	2000/09/16 20:20:41	1.6
+++ p_doors.c	2001/06/29 20:45:24
@@ -488,30 +488,39 @@ int EV_VerticalDoor
   sec = sides[line->sidenum[1]].sector;
   secnum = sec-sectors;

-  // if door already has a thinker, use it
-  if (sec->ceilingdata)      //jff 2/22/98
-  {
-    door = sec->ceilingdata; //jff 2/22/98
-    switch(line->special)
-    {
-      case  1: // only for "raise" doors, not "open"s
-      case  26:
-      case  27:
-      case  28:
-      case  117:
-        if (door->direction == -1)
-          door->direction = 1;  // go back up
-        else
-        {
-          if (!thing->player)
-            return 0;           // JDC: bad guys never close doors
-
-          door->direction = -1; // start going down immediately
-        }
-        return 1;
+  /* if door already has a thinker, use it
+   * cph 2001/04/05 -
+   * Original Doom didn't distinguish floor/lighting/ceiling
+   *  actions, so we need to do the same in demo compatibility mode.
+   */
+  door = sec->ceilingdata;
+  if (demo_compatibility) {
+    if (!door) door = sec->floordata;
+    if (!door) door = sec->lightingdata;
+  }
+  if (door) {
+    /* If the current action is a T_VerticalDoor and we're back in
+     * EV_VerticalDoor, it must have been a repeatable line, so I've dropped
+     * that check. For old demos we have to emulate the old buggy behavior and
+     * mess up non-T_VerticalDoor actions.
+     */
+    if (compatibility_level < prboom_4_compatibility ||
+        door->thinker.function == T_VerticalDoor) {
+      /* An already moving repeatable door which is being re-pressed, or a
+       * monster is trying to open a closing door - so change direction
+       */
+      if (door->direction == -1) {
+        door->direction = 1; return 1; /* go back up */
+      } else if (player) {
+        door->direction = -1; return 1; /* go back down */
+      }
     }
+    /* Either we're in prboom >=v2.3 and it's not a door, or it's a door but
+     * we're a monster and don't want to shut it; exit with no action.
+     */
+    return 0;
   }
-
+
   // emit proper sound
   switch(line->special)
   {
```

#### Fast and respawn options not reloaded from savegames correctly

Affects: MBF, PrBoom v2.1.x through v2.2.0

Description: MBF changed the order of some function calls in G_DoLoadGame in order to work around loading of options from PWADs (MBF loads options from PWADs, but needs to override them with the options from the savegame). However this resulted in the previous/old game options being used in setting up the loaded level. Amongst other things this breaks savegames with the fast or respawn options set: these options were being loaded after the level was set up by G_InitNew. This resulted in the respawn parameter being left at the setting from the previous game (or as on the command line), and parts of the setup for -fast mode being left as in the previous game, rather than loaded from the savegame.

The fix is something like this (this does the same thing as the demo code in MBF, reading the game options twice — an ugly kludge). I haven't tried this fix though, as PrBoom doesn't support options in PWADs so the fix was easier for us.

```diff
--- g_game.c    Tue Feb 15 16:06:28 2000
+++ g_game.c.new    Wed May 23 20:47:06 2001
@@ -1347,11 +1347,16 @@ static void G_DoLoadGame(void)
   // killough 11/98: simplify
   idmusnum = *(signed char *) save_p++;

+  /* cph 2001/05/23 - Must read options before we set up the level */
+  G_ReadOptions(save_p);
+
   // load a base level
   G_InitNew(gameskill, gameepisode, gamemap);

   // killough 3/1/98: Read game options
   // killough 11/98: move down to here
+  /* cph - MBF needs to reread the savegame options because G_InitNew
+   * rereads the WAD options. The demo playback code does this too. */
   save_p = G_ReadOptions(save_p);

   // get the times
      
```

#### Compatibility bug in EV_BuildStairs

Affects: Boom, MBF

Description: There are three different ways this function has, during its history, stepped through all the stairs to be triggered by a single switch:

*   original Doom used a linear `P_FindSectorFromLineTag`, but failed to preserve the index of the previous sector found, so instead it would restart its linear search from the last sector of the previous staircase
*   MBF/PrBoom with `comp_stairs` fail to emulate this, because their `P_FindSectorFromLineTag` is a chained hash table implementation. Instead they start following the hash chain from the last sector of the previous staircase, which will (probably) have the wrong tag, so they miss any further stairs
*   Boom fixed the bug, and MBF/PrBoom without `comp_stairs` work right

Fixing MBF's comp_stairs requires an elaborate fix, and remaining compatible with the buggy behavior of old MBF demos is tricky too. See PrBoom's p_floor.c:EV_BuildStairs for my fix.

#### Super shotgun reloads when out of ammo

Affects: MBF, PrBoom up to and including v2.2.4

Description: This is one of those bugs that I miss because I play with the original exe so rarely. In original Doom, if you run out of ammo while firing the super shotgun, the very last firing animation is cut short so that you don't see the marine reloading the weapon, because he's got no ammo to do so.

In Boom, the weapon change logic was moved out of `P_CheckAmmo` into `G_BuildTiccmd`, so that even weapon changes caused by ammo shortages would be transmitted in netgames, so players' weapon preferences would be honoured by all nodes. However, in doing so the code to start lowering the existing weapon was removed from `P_CheckAmmo`, relying on `A_WeaponReady` to call it instead. But in the case of the double shotgun we want the weapon to begin lowering earlier, at `A_CheckReload`; in Boom, this had accidentally been made into a no-op by the `P_CheckAmmo` change. So in Boom, the player has to watch the marine reload a gun for which he has no ammo, before the weapon change occurs.

Fix is to start lowering the weapon in `A_CheckReload`:

```diff
--- p_pspr.c	7 Jul 2001 18:17:10 -0000	1.6
+++ p_pspr.c	8 Aug 2002 20:54:13 -0000
@@ -377,7 +377,14 @@ void A_ReFire(player_t *player, pspdef_t

 void A_CheckReload(player_t *player, pspdef_t *psp)
 {
-  P_CheckAmmo(player);
+  if (!P_CheckAmmo(player) && compatibility_level >= prboom_4_compatibility) {
+    /* cph 2002/08/08 - In old Doom, P_CheckAmmo would start the weapon lowering
+     * immediately. This was lost in Boom when the weapon switching logic was
+     * rewritten. But we must tell Doom that we don't need to complete the
+     * reload frames for the weapon here. G_BuildTiccmd will set ->pendingweapon
+     * for us later on. */
+    P_SetPsprite(player,ps_weapon,weaponinfo[player->readyweapon].downstate);
+  }
 }

 //
    
```

#### Bug with East-West walls in Line of Sight Calculation

Affects: Original Doom, Boom, MBF, PrBoom up to and including 2.2.3

Description: If a line which is considered for blocking a LOS is east-west, and its starting Y coordinate equals either the source or target thing's X coordinate, the line will be incorrectly judged to obstruct the line of sight.

It's a trivial typo in `P_DivlineSide`:

```diff
--- p_sight.c.orig	Mon Aug 19 11:35:14 2002
+++ p_sight.c	Mon Aug 19 17:26:41 2002
@@ -67,7 +67,8 @@ inline static int P_DivlineSide(fixed_t
   fixed_t left, right;
   return
     !node->dx ? x == node->x ? 2 : x <= node->x ? node->dy > 0 : node->dy < 0 :
-    !node->dy ? x == node->y ? 2 : y <= node->y ? node->dx < 0 : node->dx > 0 :
+    !node->dy ? ( compatibility_level < prboom_4_compatibility ? x : y) == node->y ?
+		    2 : y <= node->y ? node->dx < 0 : node->dx > 0 :
     (right = ((y - node->y) >> FRACBITS) * (node->dx >> FRACBITS)) <
     (left  = ((x - node->x) >> FRACBITS) * (node->dy >> FRACBITS)) ? 0 :
     right == left ? 2 : 1;
    
```

#### Extra firing sound when chaingun runs out of ammo

Affects: Original Doom, Boom, MBF, PrBoom up to and including 2.2.3

Description: If a player has an odd number of bullets left, and fires the chaingun until it runs out of ammo, the gun will make one extra pistol noise after the last bullet is fired.

Found by: Ingo van Lil

The chaingun is unusual, because it fires two bullets during its animation cycle, but not simultaneously. The two bullets are fired in consecutive frames of the animation sequence, and no ammo check is done in-between. The actual codepointer that handles firing the bullet does check that there is still ammo left to fire the second shot, but it starts the pistol firing sound before it performs an amo check. So if you have one bullet left, say, and start firing the chaingun, you hear two shots, although only one is actually fired.

It is a trivial coding error in `A_FireCGun`; the checks are performed in the wrong order. A simple fix is to reverse the order; some DM players pointed out, however, that this actually changes the gameplay in DM, because, with this fix, you can tell that your opponent has run out of ammo if his chaingun makes an odd number of bullet noises. So it may be wise to compatibility-option the fix; in PrBoom v2.3.0 I will put this under the new `comp_sound` compatibility option.

```diff
> --- p_pspr.c    20 Jul 2002 18:08:37 -0000      1.5.2.1
> +++ p_pspr.c    6 Jun 2003 22:07:39 -0000       1.5.2.3
> @@ -722,7 +722,8 @@ void A_FireShotgun2(player_t *player, ps
> 
>  void A_FireCGun(player_t *player, pspdef_t *psp)
>  {
> -  S_StartSound(player->mo, sfx_pistol);
> +  if (player->ammo[weaponinfo[player->readyweapon].ammo] || compatibility)
> +    S_StartSound(player->mo, sfx_pistol);
> 
>    if (!player->ammo[weaponinfo[player->readyweapon].ammo])
>      return;
```

#### Other players' weapon pickup sounds are audible

Affects: Boom, MBF, PrBoom up to and including 2.2.3

Description: In original Doom, the weapon pickup sounds are like item pickup sounds — they are considered to be an interface noise (like the clunks in the menus, and the radio beep for mesages), and not part of the game. Players therefore do not hear other players' weapon pickup sounds. Boom changed this behaviour, making these sounds audible to other players. Old school DM players don't like this: it changes the gameplay on tight DM maps.

PrBoom 2.2.4 added a comp_sound option that toggles this and other old sound behaviours.

```diff
> --- ../release-2.2.3/prboom-2.2.3/src/p_inter.c Sun Jul 21 11:15:33 2002
> +++ ./prboom-2.2.4/src/p_inter.c        Tue Aug 19 16:31:25 2003
> @@ -186,6 +182,9 @@ boolean P_GiveWeapon(player_t *player, w
>        P_GiveAmmo(player, weaponinfo[weapon].ammo, deathmatch ? 5 : 2);
> 
>        player->pendingweapon = weapon;
> +      /* cph 20028/10 - for old-school DM addicts, allow old behavior
> +       * where only consoleplayer's pickup sounds are heard */
> +      if (!comp[comp_sound] || player == &players[consoleplayer])
>        S_StartSound (player->mo, sfx_wpnup|PICKUP_SOUND); // killough 4/25/98
>        return false;
>      }
> @@ -604,6 +603,9 @@ void P_TouchSpecialThing(mobj_t *special
>    P_RemoveMobj (special);
>    player->bonuscount += BONUSADD;
> 
> +  /* cph 20028/10 - for old-school DM addicts, allow old behavior
> +   * where only consoleplayer's pickup sounds are heard */
> +  if (!comp[comp_sound] || player == &players[consoleplayer])
>    S_StartSound (player->mo, sound | PICKUP_SOUND);   // killough 4/25/98
>  }
>   
```

#### Flipped sprite offsets are incorrect

Affects: Doom, Boom, MBF, PrBoom up to and including 2.2.3

Description: Fraggle tells me that, with sprites with a large offset that are used flipped, Doom gets the offset wrong for the flipped version. It should be reversed.

```diff
> --- ../release-2.2.3/prboom-2.2.3/src/r_things.c        Sun Jul 21 11:15:59 2002
> +++ ./prboom-2.2.4/src/r_things.c       Sun Aug 10 19:58:50 2003
> @@ -491,8 +487,10 @@ void R_ProjectSprite (mobj_t* thing)
>        flip = (boolean) sprframe->flip[0];
>      }
> 
> -  // calculate edges of the shape
> -  tx -= spriteoffset[lump];
> +  /* calculate edges of the shape
> +   * cph 2003/08/1 - fraggle points out that this offset must be flipped if the
> +   * sprite is flipped; e.g. FreeDoom imp is messed up by this. */
> +  tx -= flip ? spritewidth[lump] - spriteoffset[lump] : spriteoffset[lump];
>    x1 = (centerxfrac + FixedMul(tx,xscale)) >>FRACBITS;
> 
>      // off the right side?
```

#### Blazing door rebound noise

Affects: All prior to PrBoom 2.2.5 and 2.3.1

When a blazing door closes on a monster or player, it reopens, like a normal door, but it makes a normal door opening noise — in contrast to the fast door closing noise. Trivial oversight in p_doors.c:

```diff
--- src/p_doors.c       (revision 1135)
+++ src/p_doors.c       (revision 1136)
@@ -188,6 +188,14 @@
           case close:          // Close types do not bounce, merely wait
             break;
 
+          case blazeRaise:
+          case genBlazeRaise:
+            door->direction = 1;
+           if (!comp[comp_blazing]) {
+             S_StartSound((mobj_t *)&door->sector->soundorg,sfx_bdopn);
+             break;
+           }
+
           default:             // other types bounce off the obstruction
             door->direction = 1;
             S_StartSound((mobj_t *)&door->sector->soundorg,sfx_doropn);
```

#### Spawn fog in wrong location

Affects: All prior to PrBoom 2.2.6 and 2.3.2

Found by: [Ville Vuorinen](mailto:vv@ydin.org)

Doom tries to offset the green fog when a player respawns so that it is in front of them (so that they can see it clearly). It does this by offsetting it at the angle of the player start object. However, when calculating this angle, it accidentally treats the angle as signed instead of unsigned, resulting in a negative offset into the trigonometry arrays. This in turn causes the fog to be spawned completely out of position.

The fix is to force the value in `g_game.c:G_CheckSpot` to be unsigned:

 an = ( ANG45 * ((unsigned)mthing->angle/45) ) >> ANGLETOFINESHIFT;

### Demo sync bugs

This section contains fixes for problems which only (or primarily) cause problems playing back old Doom demos. Most people probably aren't interested in these.

#### Compatibility bug in T_MovePlane

Affects: Boom, MBF

Description: In original doom, a lowering floor will be stuck if there is an object standing on the floor which is stuck in the ceiling. In Boom and MBF, the floor will not be stuck, even in compatibility mode.

Fix is easy, reintroduce the old behaviour in compatibility mode:

```diff
--- p_floor.c   Tue Feb 15 16:20:46 2000
+++ p_floor.c-fixed Sat May 26 20:03:42 2001
@@ -81,42 +81,51 @@ result_e T_MovePlane
       switch(direction)
       {
         case -1:
           // Moving a floor down
           if (sector->floorheight - speed < dest)
           {
             lastpos = sector->floorheight;
             sector->floorheight = dest;
             flag = P_CheckSector(sector,crush); //jff 3/19/98 use faster chk
             if (flag == true)
             {
               sector->floorheight =lastpos;
               P_CheckSector(sector,crush);      //jff 3/19/98 use faster chk
             }
             return pastdest;
           }
           else
           {
             lastpos = sector->floorheight;
             sector->floorheight -= speed;
             flag = P_CheckSector(sector,crush); //jff 3/19/98 use faster chk
+
+            /* cph - make more compatible with original Doom, by
+             *  reintroducing this code. This means floors can't lower
+             *  if objects are stuck in the ceiling */
+            if ((flag == true) && compatibility) {
+              sector->floorheight = lastpos;
+              P_ChangeSector(sector,crush);
+              return crushed;
+            }
           }
           break;

         case 1:
           // Moving a floor up
```

#### Compatibility bug in P_CheckSight

Affects: MBF

Description: MBF added an optimisation in `P_CheckSight` which assumes that all points within one subsector nothing can block a line of sight. That is not strictly true. In Doom, a subsector merely implies that all points in the subsector which are contained in the level can see each other. Solid rock (space outside one-sided lines) may also be part of a subsector but it won't be able to see into the real sector because of the one-sided lines in the way. Things can end up outside the map (e.g. lost souls thrown outside the level by a dying pain elemental) and should not be able to see the player (e.g. hr06-uv.lmp goes out of sync without this patch).

```diff
--- mbf.p_sight.c        Sun Aug 27 17:00:25 2000
+++ fixed.p_sight.c        Sun Aug 27 17:31:01 2000
@@ -258,8 +258,11 @@ H_boolean P_CheckSight(mobj_t *t1, mobj_
          t1->z + t2->height <= sectors[s2->heightsec].ceilingheight))))
     return false;

-  // killough 11/98: shortcut for melee situations
-  if (t1->subsector == t2->subsector) // same subsector? obviously visible
+  /* killough 11/98: shortcut for melee situations
+   * same subsector? obviously visible
+   * cph - compatibility optioned for demo sync, cf HR06-UV.LMP */
+  if ((t1->subsector == t2->subsector) &&
+      !demo_compatibility)
     return true;

   // An unobstructed LOS is possible.
```

#### Compatibility bug in P_CrossSubsector

Affects: MBF

Description: An optimisation in P_CrossSubsector needs to be compatibility optioned, or some demos can desync. The optimisation ought to be 100% safe, but it masks a bug in P_DivlineSide that gives wrong line-of-sight results soemtimes, particularly at Doom 2 MAP02 (30nm4048 desyncs on that map without this fix).

```diff
--- mbf.p_sight.c        Sun Aug 27 17:00:25 2000
+++ fixed.p_sight.c        Sun Aug 27 17:03:48 2000
@@ -117,8 +117,12 @@ static H_boolean P_CrossSubsector(int nu

       line->validcount = validcount;

-      // OPTIMIZE: killough 4/20/98: Added quick bounding-box rejection test
+    /* OPTIMIZE: killough 4/20/98: Added quick bounding-box rejection test
+     * cph - this is causing demo desyncs on original Doom demos.
+     *  Who knows why. Exclude test for those.
+     */

+      if (!demo_compatibility)
       if (line->bbox[BOXLEFT  ] > los->bbox[BOXRIGHT ] ||
           line->bbox[BOXRIGHT ] < los->bbox[BOXLEFT  ] ||
           line->bbox[BOXBOTTOM] > los->bbox[BOXTOP   ] ||
```

#### Compatibility bug in P_SlideMove

Affects: MBF

Description: MBF includes some cruft in P_SlideMove to be backward compatible with Boom. Unfortunately it's only back compatible with Boom v2.01, because in v2.02 the cruft was removed. So the compatibility check needs fixing:

```diff
--- p_map.c        Tue Feb 15 16:17:12 2000
+++ p_map.c.fixed        Sat Sep 23 18:47:22 2000
@@ -1199,7 +1199,7 @@ void P_SlideMove(mobj_t *mo)

           if (!P_TryMove(mo, mo->x, mo->y + mo->momy, true))
             if (!P_TryMove(mo, mo->x + mo->momx, mo->y, true))
-              if (demo_version < 203 && !compatibility)
+              if (demo_version == 201)
                 mo->momx = mo->momy = 0;

           break;
```

#### Overlapping uses of global variables in p_map.c

Affects: Boom, MBF, PrBoom up to v2.2.3.

The global variables `tmthing`, `tmx`, `tmy` and `tmflags` are used by a number of functions in p_map.c. However these functions can also call each other (indirectly), so they can disrupt each other's use of these variables.

The main symptom is that it causes original Doom demos to to desync (cf hruvlmp2.zip hr22-uv.lmp, n4m1-035.lmp). But there are sometimes noticable side-effects, such as the player teleporting and becoming stuck in a monster; this happens because the telefrag iterator gets messed up by the `P_CreateSecNodeList` call after one thing is telefragged, causing others to be missed.

The fix, as far as demos are concerned, is to restore the global variables after any call to a blockmap iterator which is not present in the original Doom; this seems to be only `P_CreateSecNodeList`. For my fix I actually removed the tmflags variable to save work; also this fix is complicated by the fact that I forgot to save the tmx, tmy, and tmbbox variables when I first tried to fix the bug. Note that this fix _improves_ the accuracy of the engine: we are correcting a Boom bug and restoring things to the correct original Doom behaviour, so if you don't care about MBF/Boom demos you can make the fix unconditional.

```diff
*** 50,56 ****
  #include "lprintf.h"

  static mobj_t    *tmthing;
- static uint_64_t tmflags;
  static fixed_t   tmx;
  static fixed_t   tmy;
  static int pe_x; // Pain Elemental position for Lost Soul checks // phares
--- 50,55 ----
*************** boolean P_TeleportMove (mobj_t* thing,fi
*** 233,239 ****
    // kill anything occupying the position

    tmthing = thing;
-   tmflags = thing->flags;

    tmx = x;
    tmy = y;
--- 232,237 ----
*************** static boolean PIT_CheckThing(mobj_t *th
*** 558,564 ****
    if (thing->flags & MF_SPECIAL)
      {
        uint_64_t solid = thing->flags & MF_SOLID;
!       if (tmflags & MF_PICKUP)
  P_TouchSpecialThing(thing, tmthing); // can remove thing
        return !solid;
      }
--- 556,562 ----
    if (thing->flags & MF_SPECIAL)
      {
        uint_64_t solid = thing->flags & MF_SOLID;
!       if (tmthing->flags & MF_PICKUP)
  P_TouchSpecialThing(thing, tmthing); // can remove thing
        return !solid;
      }
*************** boolean P_CheckPosition (mobj_t* thing,f
*** 660,666 ****
    subsector_t*  newsubsec;

    tmthing = thing;
-   tmflags = thing->flags;

    tmx = x;
    tmy = y;
--- 658,663 ----
*************** boolean P_CheckPosition (mobj_t* thing,f
*** 688,694 ****
    validcount++;
    numspechit = 0;

!   if ( tmflags & MF_NOCLIP )
      return true;

    // Check things first, possibly picking things up.
--- 685,691 ----
    validcount++;
    numspechit = 0;

!   if ( tmthing->flags & MF_NOCLIP )
      return true;

    // Check things first, possibly picking things up.
*************** void P_CreateSecNodeList(mobj_t* thing,f
*** 2130,2135 ****
--- 2130,2137 ----
    int bx;
    int by;
    msecnode_t* node;
+   mobj_t* saved_tmthing = tmthing; /* cph - see comment at func end */
+   fixed_t saved_tmx = tmx, saved_tmy = tmy; /* ditto */

    // First, clear out the existing m_thing fields. As each node is
    // added or verified as needed, m_thing will be set properly. When
*************** void P_CreateSecNodeList(mobj_t* thing,f
*** 2133,2139 ****
      }

    tmthing = thing;
-   tmflags = thing->flags;

    tmx = x;
    tmy = y;
--- 2131,2136 ----
*************** void P_CreateSecNodeList(mobj_t* thing,f
*** 2184,2187 ****
--- 2185,2200 ----
      else
        node = node->m_tnext;
      }
+
+   /* cph -
+    * This is the strife we get into for using global variables. tmthing
+    *  is being used by several different functions calling
+    *  P_BlockThingIterator, including functions that can be called *from*
+    *  P_BlockThingIterator. Using a global tmthing is not reentrant.
+    * OTOH for Boom/MBF demos we have to preserve the buggy behavior.
+    *  Fun. We restore its previous value unless we're in a Boom/MBF demo.
+    */
+   if ((compatibility_level < boom_compatibility_compatibility) ||
+       (compatibility_level >= prboom_3_compatibility))
+     tmthing = saved_tmthing;
+   /* And, duh, the same for tmx/y - cph 2002/09/22
+    * And for tmbbox - cph 2003/08/10*/
+   if ((compatibility_level < boom_compatibility_compatibility) ||
+       (compatibility_level >= prboom_4_compatibility)) {
+     tmx = saved_tmx, tmy = saved_tmy;
+     if (tmthing) {
+       tmbbox[BOXTOP]  = tmy + tmthing->radius;
+       tmbbox[BOXBOTTOM] = tmy - tmthing->radius;
+       tmbbox[BOXRIGHT]  = tmx + tmthing->radius;
+       tmbbox[BOXLEFT]   = tmx - tmthing->radius;
+     }
+   }
    }
      
```

#### Bouncing Lost Souls

Affects: All source ports, Final Doom, Doom95

Lost souls that are charging were intended to bounce off of floors and ceilings, but due to a coding error in the original Doom source (the test was placed after the momentum was already set to zero), they did not. This bug was corrected in the source code for Doom95 and Final/Ultimate Doom, and in the v1.10 source that was released and forms the basis for all of the source ports. This fix broke a lot of v1.9 Doom 2 demos.

The same coding error was made with lost souls bouncing off of ceilings and this was never fixed, it seems.

Back then there was no notion of a compatibility mode, but nowadays we can compatibility option the fix so old demos will work. We use the `gamemission` variable to guess whether this demo is Doom2 or Ult/Final Doom — it's crude but works for all the Compet-n demos I've tried.

```diff
--- p_mobj.c    Tue Feb 15 16:16:34 2000
+++ p_mobj.c-fixed  Sat May 26 19:34:52 2001
@@ -471,7 +471,21 @@ floater:
     {
       // hit the floor

-      if (mo->flags & MF_SKULLFLY)
+      /* Note (id):
+       *  somebody left this after the setting momz to 0,
+       *  kinda useless there.
+       * cph - This was the a bug in the linuxdoom-1.10 source which
+       *  caused it not to sync Doom 2 v1.9 demos. Someone
+       *  added the above comment and moved up the following code. So
+       *  demos would desync in close lost soul fights.
+       * Note that this only applies to original Doom 1 or Doom2 demos - not
+       *  Final Doom and Ultimate Doom.  So we test demo_compatibility *and*
+       *  gamemission. (Note we assume that Doom1 is always Ult Doom, which
+       *  seems to hold for most published demos.)
+       */
+      int correct_lost_soul_bounce = !demo_compatibility || (gamemission != doom2);
+
+      if (correct_lost_soul_bounce && mo->flags & MF_SKULLFLY)
    mo->momz = -mo->momz; // the skull slammed into something

       if (mo->momz < 0)
@@ -497,6 +511,14 @@ floater:
    }

       mo->z = mo->floorz;
+
+      /* cph 2001/05/26 -
+       * See lost soul bouncing comment above. We need this here for bug
+       * compatibility with original Doom2 v1.9 - if a soul is charging and
+       * hit by a raising floor this incorrectly reverses its Y momentum.
+       */
+      if (!correct_lost_soul_bounce && mo->flags & MF_SKULLFLY)
+        mo->momz = -mo->momz;

       if (!((mo->flags ^ MF_MISSILE) & (MF_MISSILE | MF_NOCLIP)))
    {
      
```

#### MBF player bobbing rewrite causes demo sync problems

Affects: MBF, PrBoom up to and including v2.2.1

Description: The code that drives the player bobbing was overhauled in MBF, such that the bobbing was driven separately from the player's momentum (this was needed so bobbing doesn't break hopelessly on ice/sludge). However the bobbing affects the height of the player's weapon, which affects how long it takes to change weapon, which affects demo sync. Argh. The easy (albeit slightly cheating) fix is to drive the bobbing from the momentum as Doom used to when playing demos.

```diff
Index: p_user.c
===================================================================
RCS file: /cvsroot/prboom/prboom2/src/p_user.c,v
retrieving revision 1.4
diff -p -u -r1.4 p_user.c
--- p_user.c	2000/09/16 20:20:42	1.4
+++ p_user.c	2001/06/28 22:35:30
@@ -110,8 +110,11 @@ void P_CalcHeight (player_t* player)
    * it causes bobbing jerkiness when the player moves from ice to non-ice,
    * and vice-versa.
    */
-  player->bob = player_bobbing ? (FixedMul(player->momx,player->momx) +
-				  FixedMul(player->momy,player->momy))>>2 : 0;
+  player->bob = demo_compatibility ?
+    (FixedMul (player->mo->momx, player->mo->momx)
+     + FixedMul (player->mo->momy,player->mo->momy))>>2 :
+    player_bobbing ? (FixedMul(player->momx,player->momx) +
+        FixedMul(player->momy,player->momy))>>2 : 0;

   if (player->bob > MAXBOB)
     player->bob = MAXBOB;
```

#### Final Doom teleport demo desyncs

Thanks to Adam Hegyi for pointing me in the direction of this one

Description: It's been known for a long time that Final Doom wasn't compatible with Doom 2 demos and visa versa. The original Doom source release was based on a fork of the Doom 2 code, which didn't contain whatever changes were made to Final Doom, so it doesn't play all FInal Doom demos.

Having fixed all the other demo sync problems, I'd given up hope of finding this one, but Adam Hegyi persisted and uncovered a useful note in an old Compet-n demo by Kai-Uwe Humpert:

> One of my beloved maps, I've played it for the gmcol, and the previous COMPET-N record was mine two. Since the lmp doesn't playback with doom2.exe (because of the different style teleporters work), I had to learn it again.

Given this lead, I quickly spotted an interesting comment in p_telept.c: `thing->z = thing->floorz; // fixme: not needed?`. It turns out that this line was removed in Final Doom, with the result that teleports don't set the Z-coordinate of an object that is teleported; this is very noticable when you know what to look for.

The following patch enables the Final Doom behavior for demo compatibility when a Final Doom IWAD is loaded.

```diff
--- p_telept.c-orig	Sun Jul 22 14:39:36 2001
+++ p_telept.c	Sun Jul 22 14:40:45 2001
@@ -79,7 +79,8 @@ int EV_Teleport(line_t *line, int side,
           if (!P_TeleportMove(thing, m->x, m->y, false)) /* killough 8/9/98 */
             return 0;

-          thing->z = thing->floorz;  // fixme: not needed?
+          if (!(demo_compatibility && gamemission >= pack_tnt))
+            thing->z = thing->floorz;

           if (player)
             player->viewz = thing->z + player->viewheight;
    
```

#### Intermission screen secrets desync

Affects Boom, MBF, PrBoom up to 2.2.4 and 2.3.0.

In Boom, the intermission screen was fixed so that 100% secrets would be shown on any level that had no secrets (to avoid making players think they might have missed some). But this changes the timing of the intermission screen in some instances, because it will take time to count up to 100% if the player does not interrupt it. Therefore it is necessary to skip past the counting-up if playing an old demo.

```diff
Index: src/wi_stuff.c
===================================================================
--- src/wi_stuff.c	(revision 1124)
+++ src/wi_stuff.c	(revision 1125)
@@ -1403,7 +1403,7 @@
 
       // killough 2/22/98: Make secrets = 100% if maxsecret = 0:
 
-      if (cnt_secret[i] >= (wbs->maxsecret ? (plrs[i].ssecret * 100) / wbs->maxsecret : 100))
+      if (cnt_secret[i] >= (wbs->maxsecret ? (plrs[i].ssecret * 100) / wbs->maxsecret : compatibility_level < lxdoom_1_compatibility ? 0 : 100))
         cnt_secret[i] = wbs->maxsecret ? (plrs[i].ssecret * 100) / wbs->maxsecret : 100;
       else
         stillticking = true;
@@ -1622,7 +1622,8 @@
       S_StartSound(0, sfx_pistol);
 
     // killough 2/22/98: Make secrets = 100% if maxsecret = 0:
-    if (cnt_secret[0] >= (wbs->maxsecret ?
+    if ((!wbs->maxsecret && compatibility_level < lxdoom_1_compatibility) ||
+	cnt_secret[0] >= (wbs->maxsecret ?
       (plrs[me].ssecret * 100) / wbs->maxsecret : 100))
     {
       cnt_secret[0] = (wbs->maxsecret ?
@@ -1957,12 +1958,6 @@
   if (!wbs->maxitems)
     wbs->maxitems = 1;
 
-  // killough 2/22/98: Keep maxsecret=0 if it's zero, so
-  // we can detect 0/0 as as a special case and print 100%.
-  //
-  //    if (!wbs->maxsecret)
-  //  wbs->maxsecret = 1;
-
   if ( gamemode != retail )
     if (wbs->epsd > 2)
       wbs->epsd -= 3;
```

Author
------

Colin Phipps <[cph@moria.org.uk](mailto:cph@moria.org.uk)\>

* * *

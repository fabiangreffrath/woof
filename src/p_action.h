//--------------------------------------------------------------------
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
// Description: Action pointers.
//
//--------------------------------------------------------------------

#ifndef __P_ACTION__
#define __P_ACTION__

#include "d_player.h"
#include "p_pspr.h"
#include "p_mobj.h"

// ********************************************************************
// Function addresses or Code Pointers
// ********************************************************************
// These function addresses are the Code Pointers that have been
// modified for years by Dehacked enthusiasts.  The new BEX format
// allows more extensive changes (see d_deh.c)

extern void A_Light0(player_t *player, pspdef_t *psp);
extern void A_WeaponReady(player_t *player, pspdef_t *psp);
extern void A_Lower(player_t *player, pspdef_t *psp);
extern void A_Raise(player_t *player, pspdef_t *psp);
extern void A_Punch(player_t *player, pspdef_t *psp);
extern void A_ReFire(player_t *player, pspdef_t *psp);
extern void A_FirePistol(player_t *player, pspdef_t *psp);
extern void A_Light1(player_t *player, pspdef_t *psp);
extern void A_FireShotgun(player_t *player, pspdef_t *psp);
extern void A_Light2(player_t *player, pspdef_t *psp);
extern void A_FireShotgun2(player_t *player, pspdef_t *psp);
extern void A_CheckReload(player_t *player, pspdef_t *psp);
extern void A_OpenShotgun2(player_t *player, pspdef_t *psp);
extern void A_LoadShotgun2(player_t *player, pspdef_t *psp);
extern void A_CloseShotgun2(player_t *player, pspdef_t *psp);
extern void A_FireCGun(player_t *player, pspdef_t *psp);
extern void A_GunFlash(player_t *player, pspdef_t *psp);
extern void A_FireMissile(player_t *player, pspdef_t *psp);
extern void A_Saw(player_t *player, pspdef_t *psp);
extern void A_FirePlasma(player_t *player, pspdef_t *psp);
extern void A_BFGsound(player_t *player, pspdef_t *psp);
extern void A_FireBFG(player_t *player, pspdef_t *psp);
extern void A_BFGSpray(mobj_t *mo);
extern void A_Explode(mobj_t *thingy);
extern void A_Pain(mobj_t *actor);
extern void A_PlayerScream(mobj_t *mo);
extern void A_Fall(mobj_t *actor);
extern void A_XScream(mobj_t *actor);
extern void A_Look(mobj_t *actor);
extern void A_Chase(mobj_t *actor);
extern void A_FaceTarget(mobj_t *actor);
extern void A_PosAttack(mobj_t *actor);
extern void A_Scream(mobj_t *actor);
extern void A_SPosAttack(mobj_t *actor);
extern void A_VileChase(mobj_t *actor);
extern void A_VileStart(mobj_t *actor);
extern void A_VileTarget(mobj_t *actor);
extern void A_VileAttack(mobj_t *actor);
extern void A_StartFire(mobj_t *actor);
extern void A_Fire(mobj_t *actor);
extern void A_FireCrackle(mobj_t *actor);
extern void A_Tracer(mobj_t *actor);
extern void A_SkelWhoosh(mobj_t *actor);
extern void A_SkelFist(mobj_t *actor);
extern void A_SkelMissile(mobj_t *actor);
extern void A_FatRaise(mobj_t *actor);
extern void A_FatAttack1(mobj_t *actor);
extern void A_FatAttack2(mobj_t *actor);
extern void A_FatAttack3(mobj_t *actor);
extern void A_BossDeath(mobj_t *mo);
extern void A_CPosAttack(mobj_t *actor);
extern void A_CPosRefire(mobj_t *actor);
extern void A_TroopAttack(mobj_t *actor);
extern void A_SargAttack(mobj_t *actor);
extern void A_HeadAttack(mobj_t *actor);
extern void A_BruisAttack(mobj_t *actor);
extern void A_SkullAttack(mobj_t *actor);
extern void A_Metal(mobj_t *mo);
extern void A_SpidRefire(mobj_t *actor);
extern void A_BabyMetal(mobj_t *mo);
extern void A_BspiAttack(mobj_t *actor);
extern void A_Hoof(mobj_t* mo);
extern void A_CyberAttack(mobj_t *actor);
extern void A_PainAttack(mobj_t *actor);
extern void A_PainDie(mobj_t *actor);
extern void A_KeenDie(mobj_t* mo);
extern void A_BrainPain(mobj_t* mo);
extern void A_BrainScream(mobj_t* mo);
extern void A_BrainDie(mobj_t* mo);
extern void A_BrainAwake(mobj_t* mo);
extern void A_BrainSpit(mobj_t* mo);
extern void A_SpawnSound(mobj_t* mo);
extern void A_SpawnFly(mobj_t* mo);
extern void A_BrainExplode(mobj_t* mo);
extern void A_Detonate(mobj_t* mo);        // killough 8/9/98
extern void A_Mushroom(mobj_t *actor);     // killough 10/98
extern void A_Die(mobj_t *actor);          // killough 11/98
extern void A_Spawn(mobj_t *mo);           // killough 11/98
extern void A_Turn(mobj_t *mo);            // killough 11/98
extern void A_Face(mobj_t *mo);            // killough 11/98
extern void A_Scratch(mobj_t *mo);         // killough 11/98
extern void A_PlaySound(mobj_t *mo);       // killough 11/98
extern void A_RandomJump(mobj_t *mo);      // killough 11/98
extern void A_LineEffect(mobj_t *mo);      // killough 11/98

extern void A_FireOldBFG(player_t *player, pspdef_t *psp); // killough 7/19/98: classic BFG firing function
extern void A_BetaSkullAttack(mobj_t *actor); // killough 10/98: beta lost souls attacked different
extern void A_Stop(mobj_t *actor);

// [XA] New mbf21 codepointers

extern void A_SpawnObject(mobj_t *actor);
extern void A_MonsterProjectile(mobj_t *actor);
extern void A_MonsterBulletAttack(mobj_t *actor);
extern void A_MonsterMeleeAttack(mobj_t *actor);
extern void A_RadiusDamage(mobj_t *actor);
extern void A_NoiseAlert(mobj_t *actor);
extern void A_HealChase(mobj_t *actor);
extern void A_SeekTracer(mobj_t *actor);
extern void A_FindTracer(mobj_t *actor);
extern void A_ClearTracer(mobj_t *actor);
extern void A_JumpIfHealthBelow(mobj_t *actor);
extern void A_JumpIfTargetInSight(mobj_t *actor);
extern void A_JumpIfTargetCloser(mobj_t *actor);
extern void A_JumpIfTracerInSight(mobj_t *actor);
extern void A_JumpIfTracerCloser(mobj_t *actor);
extern void A_JumpIfFlagsSet(mobj_t *actor);
extern void A_AddFlags(mobj_t *actor);
extern void A_RemoveFlags(mobj_t *actor);
extern void A_WeaponProjectile(player_t *player, pspdef_t *psp);
extern void A_WeaponBulletAttack(player_t *player, pspdef_t *psp);
extern void A_WeaponMeleeAttack(player_t *player, pspdef_t *psp);
extern void A_WeaponSound(player_t *player, pspdef_t *psp);
extern void A_WeaponAlert(player_t *player, pspdef_t *psp);
extern void A_WeaponJump(player_t *player, pspdef_t *psp);
extern void A_ConsumeAmmo(player_t *player, pspdef_t *psp);
extern void A_CheckAmmo(player_t *player, pspdef_t *psp);
extern void A_RefireTo(player_t *player, pspdef_t *psp);
extern void A_GunFlashTo(player_t *player, pspdef_t *psp);

#endif

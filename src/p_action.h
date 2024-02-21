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
// Description: Action pointers.
//
//--------------------------------------------------------------------

#ifndef __P_ACTION__
#define __P_ACTION__

struct player_s;
struct pspdef_s;
struct mobj_s;

// ********************************************************************
// Function addresses or Code Pointers
// ********************************************************************
// These function addresses are the Code Pointers that have been
// modified for years by Dehacked enthusiasts.  The new BEX format
// allows more extensive changes (see d_deh.c)

extern void A_Light0(struct player_s *player, struct pspdef_s *psp);
extern void A_WeaponReady(struct player_s *player, struct pspdef_s *psp);
extern void A_Lower(struct player_s *player, struct pspdef_s *psp);
extern void A_Raise(struct player_s *player, struct pspdef_s *psp);
extern void A_Punch(struct player_s *player, struct pspdef_s *psp);
extern void A_ReFire(struct player_s *player, struct pspdef_s *psp);
extern void A_FirePistol(struct player_s *player, struct pspdef_s *psp);
extern void A_Light1(struct player_s *player, struct pspdef_s *psp);
extern void A_FireShotgun(struct player_s *player, struct pspdef_s *psp);
extern void A_Light2(struct player_s *player, struct pspdef_s *psp);
extern void A_FireShotgun2(struct player_s *player, struct pspdef_s *psp);
extern void A_CheckReload(struct player_s *player, struct pspdef_s *psp);
extern void A_OpenShotgun2(struct player_s *player, struct pspdef_s *psp);
extern void A_LoadShotgun2(struct player_s *player, struct pspdef_s *psp);
extern void A_CloseShotgun2(struct player_s *player, struct pspdef_s *psp);
extern void A_FireCGun(struct player_s *player, struct pspdef_s *psp);
extern void A_GunFlash(struct player_s *player, struct pspdef_s *psp);
extern void A_FireMissile(struct player_s *player, struct pspdef_s *psp);
extern void A_Saw(struct player_s *player, struct pspdef_s *psp);
extern void A_FirePlasma(struct player_s *player, struct pspdef_s *psp);
extern void A_BFGsound(struct player_s *player, struct pspdef_s *psp);
extern void A_FireBFG(struct player_s *player, struct pspdef_s *psp);
extern void A_BFGSpray(struct mobj_s *mo);
extern void A_Explode(struct mobj_s *thingy);
extern void A_Pain(struct mobj_s *actor);
extern void A_PlayerScream(struct mobj_s *mo);
extern void A_Fall(struct mobj_s *actor);
extern void A_XScream(struct mobj_s *actor);
extern void A_Look(struct mobj_s *actor);
extern void A_Chase(struct mobj_s *actor);
extern void A_FaceTarget(struct mobj_s *actor);
extern void A_PosAttack(struct mobj_s *actor);
extern void A_Scream(struct mobj_s *actor);
extern void A_SPosAttack(struct mobj_s *actor);
extern void A_VileChase(struct mobj_s *actor);
extern void A_VileStart(struct mobj_s *actor);
extern void A_VileTarget(struct mobj_s *actor);
extern void A_VileAttack(struct mobj_s *actor);
extern void A_StartFire(struct mobj_s *actor);
extern void A_Fire(struct mobj_s *actor);
extern void A_FireCrackle(struct mobj_s *actor);
extern void A_Tracer(struct mobj_s *actor);
extern void A_SkelWhoosh(struct mobj_s *actor);
extern void A_SkelFist(struct mobj_s *actor);
extern void A_SkelMissile(struct mobj_s *actor);
extern void A_FatRaise(struct mobj_s *actor);
extern void A_FatAttack1(struct mobj_s *actor);
extern void A_FatAttack2(struct mobj_s *actor);
extern void A_FatAttack3(struct mobj_s *actor);
extern void A_BossDeath(struct mobj_s *mo);
extern void A_CPosAttack(struct mobj_s *actor);
extern void A_CPosRefire(struct mobj_s *actor);
extern void A_TroopAttack(struct mobj_s *actor);
extern void A_SargAttack(struct mobj_s *actor);
extern void A_HeadAttack(struct mobj_s *actor);
extern void A_BruisAttack(struct mobj_s *actor);
extern void A_SkullAttack(struct mobj_s *actor);
extern void A_Metal(struct mobj_s *mo);
extern void A_SpidRefire(struct mobj_s *actor);
extern void A_BabyMetal(struct mobj_s *mo);
extern void A_BspiAttack(struct mobj_s *actor);
extern void A_Hoof(struct mobj_s* mo);
extern void A_CyberAttack(struct mobj_s *actor);
extern void A_PainAttack(struct mobj_s *actor);
extern void A_PainDie(struct mobj_s *actor);
extern void A_KeenDie(struct mobj_s* mo);
extern void A_BrainPain(struct mobj_s* mo);
extern void A_BrainScream(struct mobj_s* mo);
extern void A_BrainDie(struct mobj_s* mo);
extern void A_BrainAwake(struct mobj_s* mo);
extern void A_BrainSpit(struct mobj_s* mo);
extern void A_SpawnSound(struct mobj_s* mo);
extern void A_SpawnFly(struct mobj_s* mo);
extern void A_BrainExplode(struct mobj_s* mo);
extern void A_Detonate(struct mobj_s* mo);        // killough 8/9/98
extern void A_Mushroom(struct mobj_s *actor);     // killough 10/98
extern void A_Die(struct mobj_s *actor);          // killough 11/98
extern void A_Spawn(struct mobj_s *mo);           // killough 11/98
extern void A_Turn(struct mobj_s *mo);            // killough 11/98
extern void A_Face(struct mobj_s *mo);            // killough 11/98
extern void A_Scratch(struct mobj_s *mo);         // killough 11/98
extern void A_PlaySound(struct mobj_s *mo);       // killough 11/98
extern void A_RandomJump(struct mobj_s *mo);      // killough 11/98
extern void A_LineEffect(struct mobj_s *mo);      // killough 11/98

extern void A_FireOldBFG(struct player_s *player, struct pspdef_s *psp); // killough 7/19/98: classic BFG firing function
extern void A_BetaSkullAttack(struct mobj_s *actor); // killough 10/98: beta lost souls attacked different
extern void A_Stop(struct mobj_s *actor);

// [XA] New mbf21 codepointers

extern void A_SpawnObject(struct mobj_s *actor);
extern void A_MonsterProjectile(struct mobj_s *actor);
extern void A_MonsterBulletAttack(struct mobj_s *actor);
extern void A_MonsterMeleeAttack(struct mobj_s *actor);
extern void A_RadiusDamage(struct mobj_s *actor);
extern void A_NoiseAlert(struct mobj_s *actor);
extern void A_HealChase(struct mobj_s *actor);
extern void A_SeekTracer(struct mobj_s *actor);
extern void A_FindTracer(struct mobj_s *actor);
extern void A_ClearTracer(struct mobj_s *actor);
extern void A_JumpIfHealthBelow(struct mobj_s *actor);
extern void A_JumpIfTargetInSight(struct mobj_s *actor);
extern void A_JumpIfTargetCloser(struct mobj_s *actor);
extern void A_JumpIfTracerInSight(struct mobj_s *actor);
extern void A_JumpIfTracerCloser(struct mobj_s *actor);
extern void A_JumpIfFlagsSet(struct mobj_s *actor);
extern void A_AddFlags(struct mobj_s *actor);
extern void A_RemoveFlags(struct mobj_s *actor);
extern void A_WeaponProjectile(struct player_s *player, struct pspdef_s *psp);
extern void A_WeaponBulletAttack(struct player_s *player, struct pspdef_s *psp);
extern void A_WeaponMeleeAttack(struct player_s *player, struct pspdef_s *psp);
extern void A_WeaponSound(struct player_s *player, struct pspdef_s *psp);
extern void A_WeaponAlert(struct player_s *player, struct pspdef_s *psp);
extern void A_WeaponJump(struct player_s *player, struct pspdef_s *psp);
extern void A_ConsumeAmmo(struct player_s *player, struct pspdef_s *psp);
extern void A_CheckAmmo(struct player_s *player, struct pspdef_s *psp);
extern void A_RefireTo(struct player_s *player, struct pspdef_s *psp);
extern void A_GunFlashTo(struct player_s *player, struct pspdef_s *psp);

#endif

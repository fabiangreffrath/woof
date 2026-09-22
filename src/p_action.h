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

typedef struct actionargs_s
{
    struct mobj_s *actor;
    struct pspdef_s *psp;
} actionargs_t;

// ********************************************************************
// Function addresses or Code Pointers
// ********************************************************************
// These function addresses are the Code Pointers that have been
// modified for years by Dehacked enthusiasts.  The new BEX format
// allows more extensive changes (see d_deh.c)

void A_Light0(actionargs_t *);
void A_WeaponReady(actionargs_t *);
void A_Lower(actionargs_t *);
void A_Raise(actionargs_t *);
void A_Punch(actionargs_t *);
void A_ReFire(actionargs_t *);
void A_FirePistol(actionargs_t *);
void A_Light1(actionargs_t *);
void A_FireShotgun(actionargs_t *);
void A_Light2(actionargs_t *);
void A_FireShotgun2(actionargs_t *);
void A_CheckReload(actionargs_t *);
void A_OpenShotgun2(actionargs_t *);
void A_LoadShotgun2(actionargs_t *);
void A_CloseShotgun2(actionargs_t *);
void A_FireCGun(actionargs_t *);
void A_GunFlash(actionargs_t *);
void A_FireMissile(actionargs_t *);
void A_Saw(actionargs_t *);
void A_FirePlasma(actionargs_t *);
void A_BFGsound(actionargs_t *);
void A_FireBFG(actionargs_t *);
void A_BFGSpray(actionargs_t *);
void A_Explode(actionargs_t *);
void A_Pain(actionargs_t *);
void A_PlayerScream(actionargs_t *);
void A_Fall(actionargs_t *);
void A_XScream(actionargs_t *);
void A_Look(actionargs_t *);
void A_Chase(actionargs_t *);
void A_FaceTarget(actionargs_t *);
void A_PosAttack(actionargs_t *);
void A_Scream(actionargs_t *);
void A_SPosAttack(actionargs_t *);
void A_VileChase(actionargs_t *);
void A_VileStart(actionargs_t *);
void A_VileTarget(actionargs_t *);
void A_VileAttack(actionargs_t *);
void A_StartFire(actionargs_t *);
void A_Fire(actionargs_t *);
void A_FireCrackle(actionargs_t *);
void A_Tracer(actionargs_t *);
void A_SkelWhoosh(actionargs_t *);
void A_SkelFist(actionargs_t *);
void A_SkelMissile(actionargs_t *);
void A_FatRaise(actionargs_t *);
void A_FatAttack1(actionargs_t *);
void A_FatAttack2(actionargs_t *);
void A_FatAttack3(actionargs_t *);
void A_BossDeath(actionargs_t *);
void A_CPosAttack(actionargs_t *);
void A_CPosRefire(actionargs_t *);
void A_TroopAttack(actionargs_t *);
void A_SargAttack(actionargs_t *);
void A_HeadAttack(actionargs_t *);
void A_BruisAttack(actionargs_t *);
void A_SkullAttack(actionargs_t *);
void A_Metal(actionargs_t *);
void A_SpidRefire(actionargs_t *);
void A_BabyMetal(actionargs_t *);
void A_BspiAttack(actionargs_t *);
void A_Hoof(actionargs_t *);
void A_CyberAttack(actionargs_t *);
void A_PainAttack(actionargs_t *);
void A_PainDie(actionargs_t *);
void A_KeenDie(actionargs_t *);
void A_BrainPain(actionargs_t *);
void A_BrainScream(actionargs_t *);
void A_BrainDie(actionargs_t *);
void A_BrainAwake(actionargs_t *);
void A_BrainSpit(actionargs_t *);
void A_SpawnSound(actionargs_t *);
void A_SpawnFly(actionargs_t *);
void A_BrainExplode(actionargs_t *);
void A_Detonate(actionargs_t *);    // killough 8/9/98
void A_Mushroom(actionargs_t *); // killough 10/98
void A_Die(actionargs_t *);      // killough 11/98
void A_Spawn(actionargs_t *);       // killough 11/98
void A_Turn(actionargs_t *);        // killough 11/98
void A_Face(actionargs_t *);        // killough 11/98
void A_Scratch(actionargs_t *);     // killough 11/98
void A_PlaySound(actionargs_t *);   // killough 11/98
void A_RandomJump(actionargs_t *);  // killough 11/98
void A_LineEffect(actionargs_t *);  // killough 11/98

// killough 7/19/98: classic BFG firing function
void A_FireOldBFG(actionargs_t *);
// killough 10/98: beta lost souls attacked different
void A_BetaSkullAttack(actionargs_t *);
void A_Stop(actionargs_t *);

// [XA] New mbf21 codepointers

void A_SpawnObject(actionargs_t *);
void A_MonsterProjectile(actionargs_t *);
void A_MonsterBulletAttack(actionargs_t *);
void A_MonsterMeleeAttack(actionargs_t *);
void A_RadiusDamage(actionargs_t *);
void A_NoiseAlert(actionargs_t *);
void A_HealChase(actionargs_t *);
void A_SeekTracer(actionargs_t *);
void A_FindTracer(actionargs_t *);
void A_ClearTracer(actionargs_t *);
void A_JumpIfHealthBelow(actionargs_t *);
void A_JumpIfTargetInSight(actionargs_t *);
void A_JumpIfTargetCloser(actionargs_t *);
void A_JumpIfTracerInSight(actionargs_t *);
void A_JumpIfTracerCloser(actionargs_t *);
void A_JumpIfFlagsSet(actionargs_t *);
void A_AddFlags(actionargs_t *);
void A_RemoveFlags(actionargs_t *);
void A_WeaponProjectile(actionargs_t *);
void A_WeaponBulletAttack(actionargs_t *);
void A_WeaponMeleeAttack(actionargs_t *);
void A_WeaponSound(actionargs_t *);
void A_WeaponAlert(actionargs_t *);
void A_WeaponJump(actionargs_t *);
void A_ConsumeAmmo(actionargs_t *);
void A_CheckAmmo(actionargs_t *);
void A_RefireTo(actionargs_t *);
void A_GunFlashTo(actionargs_t *);

#endif

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

// ********************************************************************
// Function addresses or Code Pointers
// ********************************************************************
// These function addresses are the Code Pointers that have been
// modified for years by Dehacked enthusiasts.  The new BEX format
// allows more extensive changes (see d_deh.c)

extern void A_Light0();
extern void A_WeaponReady();
extern void A_Lower();
extern void A_Raise();
extern void A_Punch();
extern void A_ReFire();
extern void A_FirePistol();
extern void A_Light1();
extern void A_FireShotgun();
extern void A_Light2();
extern void A_FireShotgun2();
extern void A_CheckReload();
extern void A_OpenShotgun2();
extern void A_LoadShotgun2();
extern void A_CloseShotgun2();
extern void A_FireCGun();
extern void A_GunFlash();
extern void A_FireMissile();
extern void A_Saw();
extern void A_FirePlasma();
extern void A_BFGsound();
extern void A_FireBFG();
extern void A_BFGSpray();
extern void A_Explode();
extern void A_Pain();
extern void A_PlayerScream();
extern void A_Fall();
extern void A_XScream();
extern void A_Look();
extern void A_Chase();
extern void A_FaceTarget();
extern void A_PosAttack();
extern void A_Scream();
extern void A_SPosAttack();
extern void A_VileChase();
extern void A_VileStart();
extern void A_VileTarget();
extern void A_VileAttack();
extern void A_StartFire();
extern void A_Fire();
extern void A_FireCrackle();
extern void A_Tracer();
extern void A_SkelWhoosh();
extern void A_SkelFist();
extern void A_SkelMissile();
extern void A_FatRaise();
extern void A_FatAttack1();
extern void A_FatAttack2();
extern void A_FatAttack3();
extern void A_BossDeath();
extern void A_CPosAttack();
extern void A_CPosRefire();
extern void A_TroopAttack();
extern void A_SargAttack();
extern void A_HeadAttack();
extern void A_BruisAttack();
extern void A_SkullAttack();
extern void A_Metal();
extern void A_SpidRefire();
extern void A_BabyMetal();
extern void A_BspiAttack();
extern void A_Hoof();
extern void A_CyberAttack();
extern void A_PainAttack();
extern void A_PainDie();
extern void A_KeenDie();
extern void A_BrainPain();
extern void A_BrainScream();
extern void A_BrainDie();
extern void A_BrainAwake();
extern void A_BrainSpit();
extern void A_SpawnSound();
extern void A_SpawnFly();
extern void A_BrainExplode();
extern void A_Detonate();        // killough 8/9/98
extern void A_Mushroom();        // killough 10/98
extern void A_Die();             // killough 11/98
extern void A_Spawn();           // killough 11/98
extern void A_Turn();            // killough 11/98
extern void A_Face();            // killough 11/98
extern void A_Scratch();         // killough 11/98
extern void A_PlaySound();       // killough 11/98
extern void A_RandomJump();      // killough 11/98
extern void A_LineEffect();      // killough 11/98

extern void A_FireOldBFG();      // killough 7/19/98: classic BFG firing function
extern void A_BetaSkullAttack(); // killough 10/98: beta lost souls attacked different
extern void A_Stop();

// [XA] New mbf21 codepointers

extern void A_SpawnObject();
extern void A_MonsterProjectile();
extern void A_MonsterBulletAttack();
extern void A_MonsterMeleeAttack();
extern void A_RadiusDamage();
extern void A_NoiseAlert();
extern void A_HealChase();
extern void A_SeekTracer();
extern void A_FindTracer();
extern void A_ClearTracer();
extern void A_JumpIfHealthBelow();
extern void A_JumpIfTargetInSight();
extern void A_JumpIfTargetCloser();
extern void A_JumpIfTracerInSight();
extern void A_JumpIfTracerCloser();
extern void A_JumpIfFlagsSet();
extern void A_AddFlags();
extern void A_RemoveFlags();
extern void A_WeaponProjectile();
extern void A_WeaponBulletAttack();
extern void A_WeaponMeleeAttack();
extern void A_WeaponSound();
extern void A_WeaponAlert();
extern void A_WeaponJump();
extern void A_ConsumeAmmo();
extern void A_CheckAmmo();
extern void A_RefireTo();
extern void A_GunFlashTo();

#endif

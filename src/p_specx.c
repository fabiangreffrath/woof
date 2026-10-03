//
//  Copyright (C) 1993-1996 id Software
//  Copyright (C) 1993-2008 Raven Software
//  Copyright (C) 1998-2016 Marisa Heit
//  Copyright (C) 2020-2024 Ryan Krafnick
//  Copyright (C) 2026 Guilherme Miranda
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

#include "d_player.h"
#include "deh_strings.h"
#include "doomstat.h"
#include "p_mobj.h"
#include "p_spec.h"
#include "r_defs.h"
#include "s_sound.h"
#include "sounds.h"

//
// Parameterized actions type definitions
//

typedef int (*ParamSpec)(line_t *line, mobj_t *mobj, bool backSide, int arg[5]);

#define FUNC(a) \
    static int LS_##a(line_t *line, mobj_t *mobj, bool backSide, int arg[5])

static ParamSpec LineSpecials[NUM_SPECIAL];

#define LS(a) [a] = LS_##a

//
// Parameterized actions execution
//

static boolean TestActivateLine(line_t *line, mobj_t *mo, int side, spac_t spac)
{
    spac_t line_spac = line->spac;
    uint32_t line_flags = line->flags;
    int line_special = line->special;
    int arg0 = line->args[0];
    int arg1 = line->args[1];

    if (line_flags & ML_FIRSTSIDEONLY && side)
    {
        return false;
    }

    if (line_special == Teleport && line_spac & SPAC_Cross
        && spac == SPAC_PCross && mo && mo->flags & MF_MISSILE)
    {
        // Let missiles use regular player teleports
        line_spac |= SPAC_PCross;
    }

    if (spac == SPAC_Use || spac == SPAC_UseBack)
    {
        if ((line_flags & ML_CHECKSWITCHRANGE)
            && !P_CheckSwitchRange(line, mo, side))
        {
            return false;
        }
    }

    if (spac == SPAC_Use && line_spac & SPAC_MUse && mo && !mo->player
        && mo->intflags & MIF_CANUSEWALLS)
    {
        return true;
    }

    if (spac == SPAC_Push && line_spac & SPAC_MPush && mo && !mo->player
        && mo->intflags & MIF_PUSHWALL)
    {
        return true;
    }

    if (!(line_spac & spac))
    {
        if (spac != SPAC_MCross || line_spac != SPAC_Cross)
        {
            return false;
        }
    }

    if (spac == SPAC_AnyCross)
    {
        return true;
    }

    if (mo && !mo->player && !(mo->flags & MF_MISSILE)
        && !(line_flags & ML_MONSTERSCANACTIVATE)
        && (spac != SPAC_MCross || !(line_spac & SPAC_MCross)))
    {
        boolean noway = true;

        // never open secret doors
        if (line_flags & ML_SECRET && (spac == SPAC_Use || spac == SPAC_Push))
        {
            return false;
        }

        switch (spac)
        {
            case SPAC_Use:
            case SPAC_Push:
                switch (line_special)
                {
                    case Door_Raise:
                        if (arg0 == 0 && arg1 < 64)
                        {
                            noway = false;
                        }
                        break;
                    case Teleport:
                    case Teleport_NoFog:
                        noway = false;
                        break;
                }
                break;

            case SPAC_MCross:
                if (!(line_spac & SPAC_MCross))
                {
                    switch (line_special)
                    {
                        case Door_Raise:
                            if (arg1 >= 64)
                            {
                                break;
                            }
                        case Teleport:
                        case Teleport_NoFog:
                        case Teleport_Line:
                        case Plat_DownWaitUpStayLip:
                        case Plat_DownWaitUpStay:
                            noway = false;
                            break;
                    }
                }
                else
                {
                    noway = false;
                }
                break;

            default:
                noway = false;
        }
        return !noway;
    }

    if (spac == SPAC_MCross && !(line_spac & SPAC_MCross)
        && !(line_flags & ML_MONSTERSCANACTIVATE))
    {
        return false;
    }

    return true;
}

boolean P_CheckKeys(mobj_t *mo, lockdefs_t lock, boolean legacy)
{
    if (!mo || !mo->player)
    {
        return false;
    }

    const char *message = NULL;
    boolean successful = false;
    player_t *player = mo->player;
    boolean R_Card = player->cards[it_redcard];
    boolean R_Skull = player->cards[it_redskull];
    boolean B_Card = player->cards[it_bluecard];
    boolean B_Skull = player->cards[it_blueskull];
    boolean Y_Card = player->cards[it_yellowcard];
    boolean Y_Skull = player->cards[it_yellowskull];

    switch (lock)
    {
        case Lock_None:
            successful = true;
            break;
        case Lock_RedCard:
            if (!R_Card)
            {
                message = DEH_StringColorized(PD_REDC);
            }
            break;
        case Lock_BlueCard:
            if (!B_Card)
            {
                message = DEH_StringColorized(PD_BLUEC);
            }
            break;
        case Lock_YellowCard:
            if (!Y_Card)
            {
                message = DEH_StringColorized(PD_YELLOWC);
            }
            break;
        case Lock_RedSkull:
            if (!R_Skull)
            {
                message = DEH_StringColorized(PD_REDS);
            }
            break;
        case Lock_BlueSkull:
            if (!B_Skull)
            {
                message = DEH_StringColorized(PD_BLUES);
            }
            break;
        case Lock_YellowSkull:
            if (!Y_Skull)
            {
                message = DEH_StringColorized(PD_YELLOWS);
            }
            break;
        case Lock_Any:
        case Lock_AnyX:
            if (!R_Card && !R_Skull && !B_Card && !B_Skull && !Y_Card
                && !Y_Skull)
            {
                message = DEH_StringColorized(PD_ANY);
            }
            break;
        case Lock_All:
            if (!R_Card || !R_Skull || !B_Card || !B_Skull || !Y_Card
                || !Y_Skull)
            {
                message = DEH_StringColorized(PD_ALL6);
            }
            break;
        case Lock_RedLax:
        case Lock_Red:
            if (!R_Card && !R_Skull)
            {
                message = DEH_StringColorized(PD_REDK);
            }
            break;
        case Lock_BlueLax:
        case Lock_Blue:
            if (!B_Card && !B_Skull)
            {
                message = DEH_StringColorized(PD_BLUEK);
            }
            break;
        case Lock_YellowLax:
        case Lock_Yellow:
            if (!Y_Card && !Y_Skull)
            {
                message = DEH_StringColorized(PD_YELLOWK);
            }
            break;
        case Lock_EachColor:
            if ((!R_Card && !R_Skull) || (!B_Card && !B_Skull)
                || (!Y_Card && !Y_Skull))
            {
                message = DEH_StringColorized(PD_ALL3);
            }
        default:
            successful = true;
            break;
    }

    if (legacy)
    {
        displaymsg("%s", message);
        S_StartSound(mo, sfx_oof);
    }

    return successful;
}

boolean P_ActivateLine(line_t *line, mobj_t *mo, int side, spac_t spac)
{
    if (!TestActivateLine(line, mo, side, spac))
    {
        return false;
    }

    if (line->lock)
    {
        boolean legacy = false;

        switch (line->special)
        {
            case Door_Close:
            case Door_Open:
            case Door_Raise:
            case Door_LockedRaise:
            case Door_WaitRaise:
            case Door_WaitClose:
            case Generic_Door:
            case Door_CloseWaitOpen:
                legacy = true;
                break;
            default:
                break;
        }

        if (!P_CheckKeys(mo, line->lock, legacy))
        {
            return false;
        }
    }

    boolean repeat = (line->flags & ML_REPEATSPECIAL) != 0;

    boolean buttonSuccess =
        P_ExecuteLineSpecial(line, line->special, line->args, mo, side);

    // clear the special on non-retriggerable lines
    if (!repeat && buttonSuccess)
    {
        line->special = 0;
    }

    if (buttonSuccess && line->spac & (SPAC_Use | SPAC_Impact | SPAC_Push))
    {
        P_ChangeSwitchTexture(line, repeat);
    }

    return true;
}

boolean P_ExecuteLineSpecial(line_t *line, int special, int32_t args[5],
                             mobj_t *mo, int side)
{
    if (special < No_Special || special >= NUM_SPECIAL)
    {
        return false;
    }
    return LineSpecials[special](line, mo, side, args);
}

//
// Parameterized actions implementation
//

FUNC(NOP)
{
    return false;
}

FUNC(Polyobj_RotateLeft)
{
    return false; // TODO
}

FUNC(Polyobj_RotateRight)
{
    return false; // TODO
}

FUNC(Polyobj_Move)
{
    return false; // TODO
}

FUNC(Polyobj_MoveTimes8)
{
    return false; // TODO
}

FUNC(Polyobj_DoorSwing)
{
    return false; // TODO
}

FUNC(Polyobj_DoorSlide)
{
    return false; // TODO
}

FUNC(Door_Close)
{
    return false; // TODO
}

FUNC(Door_Open)
{
    return false; // TODO
}

FUNC(Door_Raise)
{
    return false; // TODO
}

FUNC(Door_LockedRaise)
{
    return false; // TODO
}

FUNC(Door_Animated)
{
    return false; // TODO
}

FUNC(Autosave)
{
    return false; // TODO
}

FUNC(Thing_Raise)
{
    return false; // TODO
}

FUNC(StartConversation)
{
    return false; // TODO
}

FUNC(Thing_Stop)
{
    return false; // TODO
}

FUNC(Floor_LowerByValue)
{
    return false; // TODO
}

FUNC(Floor_LowerToLowest)
{
    return false; // TODO
}

FUNC(Floor_LowerToNearest)
{
    return false; // TODO
}

FUNC(Floor_RaiseByValue)
{
    return false; // TODO
}

FUNC(Floor_RaiseToHighest)
{
    return false; // TODO
}

FUNC(Floor_RaiseToNearest)
{
    return false; // TODO
}

FUNC(Stairs_BuildDown)
{
    return false; // TODO
}

FUNC(Stairs_BuildUp)
{
    return false; // TODO
}

FUNC(Floor_RaiseAndCrush)
{
    return false; // TODO
}

FUNC(Pillar_Build)
{
    return false; // TODO
}

FUNC(Pillar_Open)
{
    return false; // TODO
}

FUNC(Stairs_BuildDownSync)
{
    return false; // TODO
}

FUNC(Stairs_BuildUpSync)
{
    return false; // TODO
}

FUNC(ForceField)
{
    return false; // TODO
}

FUNC(ClearForceField)
{
    return false; // TODO
}

FUNC(Floor_RaiseByValueTimes8)
{
    return false; // TODO
}

FUNC(Floor_LowerByValueTimes8)
{
    return false; // TODO
}

FUNC(Floor_MoveToValue)
{
    return false; // TODO
}

FUNC(Ceiling_Waggle)
{
    return false; // TODO
}

FUNC(Teleport_ZombieChanger)
{
    return false; // TODO
}

FUNC(Ceiling_LowerByValue)
{
    return false; // TODO
}

FUNC(Ceiling_RaiseByValue)
{
    return false; // TODO
}

FUNC(Ceiling_CrushAndRaise)
{
    return false; // TODO
}

FUNC(Ceiling_LowerAndCrush)
{
    return false; // TODO
}

FUNC(Ceiling_CrushStop)
{
    return false; // TODO
}

FUNC(Ceiling_CrushRaiseAndStay)
{
    return false; // TODO
}

FUNC(Floor_CrushStop)
{
    return false; // TODO
}

FUNC(Ceiling_MoveToValue)
{
    return false; // TODO
}

FUNC(GlassBreak)
{
    return false; // TODO
}

FUNC(Sector_SetLink)
{
    return false; // TODO
}

FUNC(Scroll_Wall)
{
    return false; // TODO
}

FUNC(Line_SetTextureOffset)
{
    return false; // TODO
}

FUNC(Sector_ChangeFlags)
{
    return false; // TODO
}

FUNC(Line_SetBlocking)
{
    return false; // TODO
}

FUNC(Line_SetTextureScale)
{
    return false; // TODO
}

FUNC(Polyobj_OR_MoveToSpot)
{
    return false; // TODO
}

FUNC(Plat_PerpetualRaise)
{
    return false; // TODO
}

FUNC(Plat_Stop)
{
    return false; // TODO
}

FUNC(Plat_DownWaitUpStay)
{
    return false; // TODO
}

FUNC(Plat_DownByValue)
{
    return false; // TODO
}

FUNC(Plat_UpWaitDownStay)
{
    return false; // TODO
}

FUNC(Plat_UpByValue)
{
    return false; // TODO
}

FUNC(Floor_LowerInstant)
{
    return false; // TODO
}

FUNC(Floor_RaiseInstant)
{
    return false; // TODO
}

FUNC(Floor_MoveToValueTimes8)
{
    return false; // TODO
}

FUNC(Ceiling_MoveToValueTimes8)
{
    return false; // TODO
}

FUNC(Teleport)
{
    return false; // TODO
}

FUNC(Teleport_NoFog)
{
    return false; // TODO
}

FUNC(ThrustThing)
{
    return false; // TODO
}

FUNC(DamageThing)
{
    return false; // TODO
}

FUNC(Teleport_NewMap)
{
    return false; // TODO
}

FUNC(Teleport_EndGame)
{
    return false; // TODO
}

FUNC(TeleportOther)
{
    return false; // TODO
}

FUNC(TeleportGroup)
{
    return false; // TODO
}

FUNC(TeleportInSector)
{
    return false; // TODO
}

FUNC(Thing_SetConversation)
{
    return false; // TODO
}

FUNC(ACS_Execute)
{
    return false; // TODO
}

FUNC(ACS_Suspend)
{
    return false; // TODO
}

FUNC(ACS_Terminate)
{
    return false; // TODO
}

FUNC(ACS_LockedExecute)
{
    return false; // TODO
}

FUNC(ACS_ExecuteWithResult)
{
    return false; // TODO
}

FUNC(ACS_LockedExecuteDoor)
{
    return false; // TODO
}

FUNC(Polyobj_MoveToSpot)
{
    return false; // TODO
}

FUNC(Polyobj_Stop)
{
    return false; // TODO
}

FUNC(Polyobj_MoveTo)
{
    return false; // TODO
}

FUNC(Polyobj_OR_MoveTo)
{
    return false; // TODO
}

FUNC(Polyobj_OR_RotateLeft)
{
    return false; // TODO
}

FUNC(Polyobj_OR_RotateRight)
{
    return false; // TODO
}

FUNC(Polyobj_OR_Move)
{
    return false; // TODO
}

FUNC(Polyobj_OR_MoveTimes8)
{
    return false; // TODO
}

FUNC(Pillar_BuildAndCrush)
{
    return false; // TODO
}

FUNC(FloorAndCeiling_LowerByValue)
{
    return false; // TODO
}

FUNC(FloorAndCeiling_RaiseByValue)
{
    return false; // TODO
}

FUNC(Ceiling_LowerAndCrushDist)
{
    return false; // TODO
}

FUNC(Sector_SetTranslucent)
{
    return false; // TODO
}

FUNC(Floor_RaiseAndCrushDoom)
{
    return false; // TODO
}

FUNC(Ceiling_CrushAndRaiseSilentDist)
{
    return false; // TODO
}

FUNC(Door_WaitRaise)
{
    return false; // TODO
}

FUNC(Door_WaitClose)
{
    return false; // TODO
}

FUNC(Line_SetPortalTarget)
{
    return false; // TODO
}

FUNC(BSP_SpecialEffects)
{
    return false; // TODO
}

FUNC(Light_ForceLightning)
{
    return false; // TODO
}

FUNC(Light_RaiseByValue)
{
    return false; // TODO
}

FUNC(Light_LowerByValue)
{
    return false; // TODO
}

FUNC(Light_ChangeToValue)
{
    return false; // TODO
}

FUNC(Light_Fade)
{
    return false; // TODO
}

FUNC(Light_Glow)
{
    return false; // TODO
}

FUNC(Light_Flicker)
{
    return false; // TODO
}

FUNC(Light_Strobe)
{
    return false; // TODO
}

FUNC(Light_Stop)
{
    return false; // TODO
}

FUNC(Thing_Damage)
{
    return false; // TODO
}

FUNC(Radius_Quake)
{
    return false; // TODO
}

FUNC(ChangePlayerClass)
{
    return false; // TODO
}

FUNC(ChangePlayerClassMenu)
{
    return false; // TODO
}

FUNC(Thing_Move)
{
    return false; // TODO
}

FUNC(Thing_SetSpecial)
{
    return false; // TODO
}

FUNC(ThrustThingZ)
{
    return false; // TODO
}

FUNC(UsePuzzleItem)
{
    return false; // TODO
}

FUNC(Thing_Activate)
{
    return false; // TODO
}

FUNC(Thing_Deactivate)
{
    return false; // TODO
}

FUNC(Thing_Remove)
{
    return false; // TODO
}

FUNC(Thing_Destroy)
{
    return false; // TODO
}

FUNC(Thing_Projectile)
{
    return false; // TODO
}

FUNC(Thing_Spawn)
{
    return false; // TODO
}

FUNC(Thing_ProjectileGravity)
{
    return false; // TODO
}

FUNC(Thing_SpawnNoFog)
{
    return false; // TODO
}

FUNC(Floor_Waggle)
{
    return false; // TODO
}

FUNC(Thing_SpawnFacing)
{
    return false; // TODO
}

FUNC(Sector_ChangeSound)
{
    return false; // TODO
}

FUNC(Teleport_NoStop)
{
    return false; // TODO
}

FUNC(Line_SetPortal)
{
    return false; // TODO
}

FUNC(SetGlobalFogParameter)
{
    return false; // TODO
}

FUNC(FS_Execute)
{
    return false; // TODO
}

FUNC(Sector_SetPlaneReflection)
{
    return false; // TODO
}

FUNC(Ceiling_CrushAndRaiseDist)
{
    return false; // TODO
}

FUNC(Generic_Crusher2)
{
    return false; // TODO
}

FUNC(Sector_SetCeilingScale2)
{
    return false; // TODO
}

FUNC(Sector_SetFloorScale2)
{
    return false; // TODO
}

FUNC(Plat_UpNearestWaitDownStay)
{
    return false; // TODO
}

FUNC(NoiseAlert)
{
    return false; // TODO
}

FUNC(SendToCommunicator)
{
    return false; // TODO
}

FUNC(Thing_ProjectileIntercept)
{
    return false; // TODO
}

FUNC(Thing_ChangeTID)
{
    return false; // TODO
}

FUNC(Thing_Hate)
{
    return false; // TODO
}

FUNC(Thing_ProjectileAimed)
{
    return false; // TODO
}

FUNC(ChangeSkill)
{
    return false; // TODO
}

FUNC(Thing_SetTranslation)
{
    return false; // TODO
}

FUNC(Line_AlignCeiling)
{
    return false; // TODO
}

FUNC(Line_AlignFloor)
{
    return false; // TODO
}

FUNC(Sector_SetRotation)
{
    return false; // TODO
}

FUNC(Sector_SetCeilingPanning)
{
    return false; // TODO
}

FUNC(Sector_SetFloorPanning)
{
    return false; // TODO
}

FUNC(Sector_SetCeilingScale)
{
    return false; // TODO
}

FUNC(Sector_SetFloorScale)
{
    return false; // TODO
}

FUNC(SetPlayerProperty)
{
    return false; // TODO
}

FUNC(Ceiling_LowerToHighestFloor)
{
    return false; // TODO
}

FUNC(Ceiling_LowerInstant)
{
    return false; // TODO
}

FUNC(Ceiling_RaiseInstant)
{
    return false; // TODO
}

FUNC(Ceiling_CrushRaiseAndStayA)
{
    return false; // TODO
}

FUNC(Ceiling_CrushAndRaiseA)
{
    return false; // TODO
}

FUNC(Ceiling_CrushAndRaiseSilentA)
{
    return false; // TODO
}

FUNC(Ceiling_RaiseByValueTimes8)
{
    return false; // TODO
}

FUNC(Ceiling_LowerByValueTimes8)
{
    return false; // TODO
}

FUNC(Generic_Floor)
{
    return false; // TODO
}

FUNC(Generic_Ceiling)
{
    return false; // TODO
}

FUNC(Generic_Door)
{
    return false; // TODO
}

FUNC(Generic_Lift)
{
    return false; // TODO
}

FUNC(Generic_Stairs)
{
    return false; // TODO
}

FUNC(Generic_Crusher)
{
    return false; // TODO
}

FUNC(Plat_DownWaitUpStayLip)
{
    return false; // TODO
}

FUNC(Plat_PerpetualRaiseLip)
{
    return false; // TODO
}

FUNC(TranslucentLine)
{
    return false; // TODO
}

FUNC(Sector_SetColor)
{
    return false; // TODO
}

FUNC(Sector_SetFade)
{
    return false; // TODO
}

FUNC(Sector_SetDamage)
{
    return false; // TODO
}

FUNC(Teleport_Line)
{
    return false; // TODO
}

FUNC(Sector_SetGravity)
{
    return false; // TODO
}

FUNC(Stairs_BuildUpDoom)
{
    return false; // TODO
}

FUNC(Sector_SetWind)
{
    return false; // TODO
}

FUNC(Sector_SetFriction)
{
    return false; // TODO
}

FUNC(Sector_SetCurrent)
{
    return false; // TODO
}

FUNC(Scroll_Texture_Both)
{
    return false; // TODO
}

FUNC(Scroll_Floor)
{
    return false; // TODO
}

FUNC(Scroll_Ceiling)
{
    return false; // TODO
}

FUNC(ACS_ExecuteAlways)
{
    return false; // TODO
}

FUNC(PointPush_SetForce)
{
    return false; // TODO
}

FUNC(Plat_RaiseAndStayTx0)
{
    return false; // TODO
}

FUNC(Thing_SetGoal)
{
    return false; // TODO
}

FUNC(Plat_UpByValueStayTx)
{
    return false; // TODO
}

FUNC(Plat_ToggleCeiling)
{
    return false; // TODO
}

FUNC(Light_StrobeDoom)
{
    return false; // TODO
}

FUNC(Light_MinNeighbor)
{
    return false; // TODO
}

FUNC(Light_MaxNeighbor)
{
    return false; // TODO
}

FUNC(Floor_TransferTrigger)
{
    return false; // TODO
}

FUNC(Floor_TransferNumeric)
{
    return false; // TODO
}

FUNC(ChangeCamera)
{
    return false; // TODO
}

FUNC(Floor_RaiseToLowestCeiling)
{
    return false; // TODO
}

FUNC(Floor_RaiseByValueTxTy)
{
    return false; // TODO
}

FUNC(Floor_RaiseByTexture)
{
    return false; // TODO
}

FUNC(Floor_LowerToLowestTxTy)
{
    return false; // TODO
}

FUNC(Floor_LowerToHighest)
{
    return false; // TODO
}

FUNC(Exit_Normal)
{
    return false; // TODO
}

FUNC(Exit_Secret)
{
    return false; // TODO
}

FUNC(Elevator_RaiseToNearest)
{
    return false; // TODO
}

FUNC(Elevator_MoveToFloor)
{
    return false; // TODO
}

FUNC(Elevator_LowerToNearest)
{
    return false; // TODO
}

FUNC(HealThing)
{
    return false; // TODO
}

FUNC(Door_CloseWaitOpen)
{
    return false; // TODO
}

FUNC(Floor_Donut)
{
    return false; // TODO
}

FUNC(FloorAndCeiling_LowerRaise)
{
    return false; // TODO
}

FUNC(Ceiling_RaiseToNearest)
{
    return false; // TODO
}

FUNC(Ceiling_LowerToLowest)
{
    return false; // TODO
}

FUNC(Ceiling_LowerToFloor)
{
    return false; // TODO
}

FUNC(Ceiling_CrushRaiseAndStaySilA)
{
    return false; // TODO
}

FUNC(Floor_LowerToHighestEE)
{
    return false; // TODO
}

FUNC(Floor_RaiseToLowest)
{
    return false; // TODO
}

FUNC(Floor_LowerToLowestCeiling)
{
    return false; // TODO
}

FUNC(Floor_RaiseToCeiling)
{
    return false; // TODO
}

FUNC(Floor_ToCeilingInstant)
{
    return false; // TODO
}

FUNC(Floor_LowerByTexture)
{
    return false; // TODO
}

FUNC(Ceiling_RaiseToHighest)
{
    return false; // TODO
}

FUNC(Ceiling_ToHighestInstant)
{
    return false; // TODO
}

FUNC(Ceiling_LowerToNearest)
{
    return false; // TODO
}

FUNC(Ceiling_RaiseToLowest)
{
    return false; // TODO
}

FUNC(Ceiling_RaiseToHighestFloor)
{
    return false; // TODO
}

FUNC(Ceiling_ToFloorInstant)
{
    return false; // TODO
}

FUNC(Ceiling_RaiseByTexture)
{
    return false; // TODO
}

FUNC(Ceiling_LowerByTexture)
{
    return false; // TODO
}

FUNC(Stairs_BuildDownDoom)
{
    return false; // TODO
}

FUNC(Stairs_BuildUpDoomSync)
{
    return false; // TODO
}

FUNC(Stairs_BuildDownDoomSync)
{
    return false; // TODO
}

FUNC(Stairs_BuildUpDoomCrush)
{
    return false; // TODO
}

FUNC(Door_AnimatedClose)
{
    return false; // TODO
}

FUNC(Floor_Stop)
{
    return false; // TODO
}

FUNC(Ceiling_Stop)
{
    return false; // TODO
}

FUNC(Sector_SetFloorGlow)
{
    return false; // TODO
}

FUNC(Sector_SetCeilingGlow)
{
    return false; // TODO
}

FUNC(Floor_MoveToValueAndCrush)
{
    return false; // TODO
}

FUNC(Ceiling_MoveToValueAndCrush)
{
    return false; // TODO
}

FUNC(Line_SetAutoMapFlags)
{
    return false; // TODO
}

FUNC(Line_SetAutomapStyle)
{
    return false; // TODO
}

FUNC(Polyobj_StopSound)
{
    return false; // TODO
}

FUNC(Generic_CrusherDist)
{
    return false; // TODO
}

static ParamSpec LineSpecials[NUM_SPECIAL] = {
    [No_Special] = LS_NOP,
    [Polyobj_StartLine] = LS_NOP,
    LS(Polyobj_RotateLeft),
    LS(Polyobj_RotateRight),
    LS(Polyobj_Move),
    [Polyobj_ExplicitLine] = LS_NOP,
    LS(Polyobj_MoveTimes8),
    LS(Polyobj_DoorSwing),
    LS(Polyobj_DoorSlide),
    [Line_Horizon] = LS_NOP,
    LS(Door_Close),
    LS(Door_Open),
    LS(Door_Raise),
    LS(Door_LockedRaise),
    LS(Door_Animated),
    LS(Autosave),
    [Transfer_WallLight] = LS_NOP,
    LS(Thing_Raise),
    LS(StartConversation),
    LS(Thing_Stop),
    LS(Floor_LowerByValue),
    LS(Floor_LowerToLowest),
    LS(Floor_LowerToNearest),
    LS(Floor_RaiseByValue),
    LS(Floor_RaiseToHighest),
    LS(Floor_RaiseToNearest),
    LS(Stairs_BuildDown),
    LS(Stairs_BuildUp),
    LS(Floor_RaiseAndCrush),
    LS(Pillar_Build),
    LS(Pillar_Open),
    LS(Stairs_BuildDownSync),
    LS(Stairs_BuildUpSync),
    LS(ForceField),
    LS(ClearForceField),
    LS(Floor_RaiseByValueTimes8),
    LS(Floor_LowerByValueTimes8),
    LS(Floor_MoveToValue),
    LS(Ceiling_Waggle),
    LS(Teleport_ZombieChanger),
    LS(Ceiling_LowerByValue),
    LS(Ceiling_RaiseByValue),
    LS(Ceiling_CrushAndRaise),
    LS(Ceiling_LowerAndCrush),
    LS(Ceiling_CrushStop),
    LS(Ceiling_CrushRaiseAndStay),
    LS(Floor_CrushStop),
    LS(Ceiling_MoveToValue),
    [Sector_Attach3dMidtex] = LS_NOP,
    LS(GlassBreak),
    [ExtraFloor_LightOnly] = LS_NOP,
    LS(Sector_SetLink),
    LS(Scroll_Wall),
    LS(Line_SetTextureOffset),
    LS(Sector_ChangeFlags),
    LS(Line_SetBlocking),
    LS(Line_SetTextureScale),
    [Sector_SetPortal] = LS_NOP,
    [Sector_CopyScroller] = LS_NOP,
    LS(Polyobj_OR_MoveToSpot),
    LS(Plat_PerpetualRaise),
    LS(Plat_Stop),
    LS(Plat_DownWaitUpStay),
    LS(Plat_DownByValue),
    LS(Plat_UpWaitDownStay),
    LS(Plat_UpByValue),
    LS(Floor_LowerInstant),
    LS(Floor_RaiseInstant),
    LS(Floor_MoveToValueTimes8),
    LS(Ceiling_MoveToValueTimes8),
    LS(Teleport),
    LS(Teleport_NoFog),
    LS(ThrustThing),
    LS(DamageThing),
    LS(Teleport_NewMap),
    LS(Teleport_EndGame),
    LS(TeleportOther),
    LS(TeleportGroup),
    LS(TeleportInSector),
    LS(Thing_SetConversation),
    LS(ACS_Execute),
    LS(ACS_Suspend),
    LS(ACS_Terminate),
    LS(ACS_LockedExecute),
    LS(ACS_ExecuteWithResult),
    LS(ACS_LockedExecuteDoor),
    LS(Polyobj_MoveToSpot),
    LS(Polyobj_Stop),
    LS(Polyobj_MoveTo),
    LS(Polyobj_OR_MoveTo),
    LS(Polyobj_OR_RotateLeft),
    LS(Polyobj_OR_RotateRight),
    LS(Polyobj_OR_Move),
    LS(Polyobj_OR_MoveTimes8),
    LS(Pillar_BuildAndCrush),
    LS(FloorAndCeiling_LowerByValue),
    LS(FloorAndCeiling_RaiseByValue),
    LS(Ceiling_LowerAndCrushDist),
    LS(Sector_SetTranslucent),
    LS(Floor_RaiseAndCrushDoom),
    [Scroll_Texture_Left] = LS_NOP,
    [Scroll_Texture_Down] = LS_NOP,
    [Scroll_Texture_Right] = LS_NOP,
    [Scroll_Texture_Up] = LS_NOP,
    LS(Ceiling_CrushAndRaiseSilentDist),
    LS(Door_WaitRaise),
    LS(Door_WaitClose),
    LS(Line_SetPortalTarget),
    LS(BSP_SpecialEffects),
    LS(Light_ForceLightning),
    LS(Light_RaiseByValue),
    LS(Light_LowerByValue),
    LS(Light_ChangeToValue),
    LS(Light_Fade),
    LS(Light_Glow),
    LS(Light_Flicker),
    LS(Light_Strobe),
    LS(Light_Stop),
    [Plane_Copy] = LS_NOP,
    LS(Thing_Damage),
    LS(Radius_Quake),
    [Line_SetIdentification] = LS_NOP,
    [Line_BlockNetworkVisportal] = LS_NOP,
    LS(ChangePlayerClass),
    LS(ChangePlayerClassMenu),
    LS(Thing_Move),
    [Unused128] = LS_NOP,
    LS(Thing_SetSpecial),
    LS(ThrustThingZ),
    LS(UsePuzzleItem),
    LS(Thing_Activate),
    LS(Thing_Deactivate),
    LS(Thing_Remove),
    LS(Thing_Destroy),
    LS(Thing_Projectile),
    LS(Thing_Spawn),
    LS(Thing_ProjectileGravity),
    LS(Thing_SpawnNoFog),
    LS(Floor_Waggle),
    LS(Thing_SpawnFacing),
    LS(Sector_ChangeSound),
    [Unused141] = LS_NOP,
    [Unused142] = LS_NOP,
    [Unused143] = LS_NOP,
    [Unused144] = LS_NOP,
    [Player_SetTeam] = LS_NOP,
    [Unused146] = LS_NOP,
    [Unused147] = LS_NOP,
    [Unused148] = LS_NOP,
    [Unused149] = LS_NOP,
    [Line_SetHealth] = LS_NOP,
    [Sector_SetHealth] = LS_NOP,
    [Team_Score] = LS_NOP,
    [Team_GivePoints] = LS_NOP,
    LS(Teleport_NoStop),
    [Unused155] = LS_NOP,
    LS(Line_SetPortal),
    LS(SetGlobalFogParameter),
    LS(FS_Execute),
    LS(Sector_SetPlaneReflection),
    [Sector_Set3dFloor] = LS_NOP,
    [Sector_SetContents] = LS_NOP,
    [Unused162] = LS_NOP,
    [Unused163] = LS_NOP,
    [Unused164] = LS_NOP,
    [Unused165] = LS_NOP,
    [Unused166] = LS_NOP,
    [Unused167] = LS_NOP,
    LS(Ceiling_CrushAndRaiseDist),
    LS(Generic_Crusher2),
    LS(Sector_SetCeilingScale2),
    LS(Sector_SetFloorScale2),
    LS(Plat_UpNearestWaitDownStay),
    LS(NoiseAlert),
    LS(SendToCommunicator),
    LS(Thing_ProjectileIntercept),
    LS(Thing_ChangeTID),
    LS(Thing_Hate),
    LS(Thing_ProjectileAimed),
    LS(ChangeSkill),
    LS(Thing_SetTranslation),
    [Plane_Align] = LS_NOP,
    [Line_Mirror] = LS_NOP,
    LS(Line_AlignCeiling),
    LS(Line_AlignFloor),
    LS(Sector_SetRotation),
    LS(Sector_SetCeilingPanning),
    LS(Sector_SetFloorPanning),
    LS(Sector_SetCeilingScale),
    LS(Sector_SetFloorScale),
    [Static_Init] = LS_NOP,
    LS(SetPlayerProperty),
    LS(Ceiling_LowerToHighestFloor),
    LS(Ceiling_LowerInstant),
    LS(Ceiling_RaiseInstant),
    LS(Ceiling_CrushRaiseAndStayA),
    LS(Ceiling_CrushAndRaiseA),
    LS(Ceiling_CrushAndRaiseSilentA),
    LS(Ceiling_RaiseByValueTimes8),
    LS(Ceiling_LowerByValueTimes8),
    LS(Generic_Floor),
    LS(Generic_Ceiling),
    LS(Generic_Door),
    LS(Generic_Lift),
    LS(Generic_Stairs),
    LS(Generic_Crusher),
    LS(Plat_DownWaitUpStayLip),
    LS(Plat_PerpetualRaiseLip),
    LS(TranslucentLine),
    [Transfer_Heights] = LS_NOP,
    [Transfer_FloorLight] = LS_NOP,
    [Transfer_CeilingLight] = LS_NOP,
    LS(Sector_SetColor),
    LS(Sector_SetFade),
    LS(Sector_SetDamage),
    LS(Teleport_Line),
    LS(Sector_SetGravity),
    LS(Stairs_BuildUpDoom),
    LS(Sector_SetWind),
    LS(Sector_SetFriction),
    LS(Sector_SetCurrent),
    LS(Scroll_Texture_Both),
    [Scroll_Texture_Model] = LS_NOP,
    LS(Scroll_Floor),
    LS(Scroll_Ceiling),
    [Scroll_Texture_Offsets] = LS_NOP,
    LS(ACS_ExecuteAlways),
    LS(PointPush_SetForce),
    LS(Plat_RaiseAndStayTx0),
    LS(Thing_SetGoal),
    LS(Plat_UpByValueStayTx),
    LS(Plat_ToggleCeiling),
    LS(Light_StrobeDoom),
    LS(Light_MinNeighbor),
    LS(Light_MaxNeighbor),
    LS(Floor_TransferTrigger),
    LS(Floor_TransferNumeric),
    LS(ChangeCamera),
    LS(Floor_RaiseToLowestCeiling),
    LS(Floor_RaiseByValueTxTy),
    LS(Floor_RaiseByTexture),
    LS(Floor_LowerToLowestTxTy),
    LS(Floor_LowerToHighest),
    LS(Exit_Normal),
    LS(Exit_Secret),
    LS(Elevator_RaiseToNearest),
    LS(Elevator_MoveToFloor),
    LS(Elevator_LowerToNearest),
    LS(HealThing),
    LS(Door_CloseWaitOpen),
    LS(Floor_Donut),
    LS(FloorAndCeiling_LowerRaise),
    LS(Ceiling_RaiseToNearest),
    LS(Ceiling_LowerToLowest),
    LS(Ceiling_LowerToFloor),
    LS(Ceiling_CrushRaiseAndStaySilA),
    LS(Floor_LowerToHighestEE),
    LS(Floor_RaiseToLowest),
    LS(Floor_LowerToLowestCeiling),
    LS(Floor_RaiseToCeiling),
    LS(Floor_ToCeilingInstant),
    LS(Floor_LowerByTexture),
    LS(Ceiling_RaiseToHighest),
    LS(Ceiling_ToHighestInstant),
    LS(Ceiling_LowerToNearest),
    LS(Ceiling_RaiseToLowest),
    LS(Ceiling_RaiseToHighestFloor),
    LS(Ceiling_ToFloorInstant),
    LS(Ceiling_RaiseByTexture),
    LS(Ceiling_LowerByTexture),
    LS(Stairs_BuildDownDoom),
    LS(Stairs_BuildUpDoomSync),
    LS(Stairs_BuildDownDoomSync),
    LS(Stairs_BuildUpDoomCrush),
    LS(Door_AnimatedClose),
    LS(Floor_Stop),
    LS(Ceiling_Stop),
    LS(Sector_SetFloorGlow),
    LS(Sector_SetCeilingGlow),
    LS(Floor_MoveToValueAndCrush),
    LS(Ceiling_MoveToValueAndCrush),
    LS(Line_SetAutoMapFlags),
    LS(Line_SetAutomapStyle),
    LS(Polyobj_StopSound),
    LS(Generic_CrusherDist),
};

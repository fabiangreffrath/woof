# MBF21 Current Status

These are changes / features that are currently implemented.

## Sectors

#### Instant death sector special
- dsda-doom: [Implementation](https://github.com/kraflab/dsda-doom/blob/07639e2f1834c6d6ae5a37c720e01d52c2c95d4d/prboom2/src/p_spec.c#L2437-L2463)
- woof: [PR](https://github.com/fabiangreffrath/woof/pull/204)
- Bit 12 (4096) turns on "alternate damage meaning" for bit 5 & 6:

| Dec | Bit 6-5 | Description                                                   |
|-----|---------|---------------------------------------------------------------|
| 0   | 00      | Kills a player unless they have a rad suit or invulnerability |
| 32  | 01      | Kills a player                                                |
| 64  | 10      | Kills all players and exits the map (normal exit)             |
| 96  | 11      | Kills all players and exits the map (secret exit)             |

#### Kill monsters sector special
- dsda-doom: [PR](https://github.com/kraflab/dsda-doom/pull/18)
- woof: [PR](https://github.com/fabiangreffrath/woof/pull/204)
- Bit 13 turns on "kill monsters" flag for sectors - kills grounded monsters.

## Lines

#### Block land monsters line flag
- dsda-doom: [PR](https://github.com/kraflab/dsda-doom/pull/19)
- woof: [PR](https://github.com/fabiangreffrath/woof/pull/204)
- Uses bit 12 (4096).

#### Block players line flag
- dsda-doom: [commit](https://github.com/kraflab/dsda-doom/commit/687237e3d236056730f58dca27efd45e1774d53e)
- woof: [PR](https://github.com/fabiangreffrath/woof/pull/204)
- Uses bit 13 (8192).

#### Line scroll special variants
- dsda-doom: [PR](https://github.com/kraflab/dsda-doom/pull/29)
- woof: [PR](https://github.com/fabiangreffrath/woof/pull/204)
- Scroll like special 255, but the special line determines the speed / direction with which all tagged lines scroll.
- 1024 is without control sector / acceleration.
- 1025 uses control sector.
- 1026 uses control sector + acceleration.

## Things

#### Dehacked Thing Groups
- dsda-doom: [PR](https://github.com/kraflab/dsda-doom/pull/22), [PR](https://github.com/kraflab/dsda-doom/pull/23), [commit](https://github.com/kraflab/dsda-doom/commit/683fc7213bd64985216d4cf052cf65b63a164317)
- woof: [PR](https://github.com/fabiangreffrath/woof/pull/207)

##### Infighting
- Add `Infighting group = N` in Thing definition.
- `N` is a nonnegative integer.
- Things with the same value of `N` will not target each other after taking damage.

##### Projectile
- Add `Projectile group = M` in Thing definition.
- `M` is an integer.
- Things with the same value of `M` will not deal projectile damage to each other.
- A negative value of `M` means that species has no projectile immunity, even to other things in the same species.

##### Splash
- Add `Splash group = S` in Thing definition.
- `S` is a nonnegative integer.
- If an explosion (or other splash damage source) has the same splash group as the target, it will not deal damage.

##### Examples
```
Thing 12 (Imp)
Projectile group = -1
Splash group = 0

Thing 16 (Baron of Hell)
Projectile group = 2

Thing 18 (Hell Knight)
Infighting group = 1
Projectile group = 1

Thing 21 (Arachnotron)
Infighting group = 1
Projectile group = 2

Thing 31 (Barrel)
Splash group = 0
```

In this example:
- Imp projectiles now damage other Imps (and they will infight with their own kind).
- Barons and Arachnotrons are in the same projectile group: their projectiles will no longer damage each other.
- Barons and Hell Knights are not in the same projectile group: their projectiles will now damage each other, leading to infighting.
- Hell Knights and Arachnotrons are in the same infighting group: they will not infight with each other, despite taking damage from each other's projectiles.
- Imps and Barrels are in the same splash group: barrels no longer deal splash damage to imps.
- Note that the group numbers are separate - being in infighting group 1 doesn't mean you are in projectile group 1.

#### New Thing Flags
- dsda-doom: [commit](https://github.com/kraflab/dsda-doom/commit/10907e5d37dc2337c93f6dd59573fd42c5a8aaf6)
- woof: [PR](https://github.com/fabiangreffrath/woof/pull/207)
- Add `MBF21 Bits = X` in the Thing definition.
- The format is the same as the existing `Bits` field.
- Example: `MBF21 Bits = LOGRAV+DMGIGNORED+MAP07BOSS1`.
- Implementations match between DSDA-Doom and Eternity Engine for labeled flags.

| DSDA-Doom          | Eternity Engine    | Description                                                                                    |
|--------------------|--------------------|------------------------------------------------------------------------------------------------|
| MF2_LOGRAV         | MF2_LOGRAV         | Lower gravity (1/8)                                                                            |
| MF2_SHORTMRANGE    | MF2_SHORTMRANGE    | Short missile range (archvile)                                                                 |
| MF2_DMGIGNORED     | MF3_DMGIGNORED     | Other things ignore its attacks (archvile)                                                     |
| MF2_NORADIUSDMG    | MF4_NORADIUSDMG    | Doesn't take splash damage (cyberdemon, mastermind)                                            |
| MF2_FORCERADIUSDMG | MF4_FORCERADIUSDMG | Thing causes splash damage even if the target shouldn't                                        |
| MF2_HIGHERMPROB    | MF2_HIGHERMPROB    | Higher missile attack probability (cyberdemon)                                                 |
| MF2_RANGEHALF      | MF2_RANGEHALF      | Use half distance for missile attack probability (cyberdemon, mastermind, revenant, lost soul) |
| MF2_NOTHRESHOLD    | MF3_NOTHRESHOLD    | Has no targeting threshold (archvile)                                                          |
| MF2_LONGMELEE      | MF2_LONGMELEE      | Has long melee range (revenant)                                                                |
| MF2_BOSS           | MF2_BOSS           | Full volume see / death sound & splash immunity (cyberdemon, mastermind)                       |
| MF2_MAP07BOSS1     | MF2_MAP07BOSS1     | Tag 666 "boss" on doom 2 map 7 (mancubus)                                                      |
| MF2_MAP07BOSS2     | MF2_MAP07BOSS2     | Tag 667 "boss" on doom 2 map 7 (arachnotron)                                                   |
| MF2_E1M8BOSS       | MF2_E1M8BOSS       | E1M8 boss (baron)                                                                              |
| MF2_E2M8BOSS       | MF2_E2M8BOSS       | E2M8 boss (cyberdemon)                                                                         |
| MF2_E3M8BOSS       | MF2_E3M8BOSS       | E3M8 boss (mastermind)                                                                         |
| MF2_E4M6BOSS       | MF2_E4M6BOSS       | E4M6 boss (cyberdemon)                                                                         |
| MF2_E4M8BOSS       | MF2_E4M8BOSS       | E4M8 boss (mastermind)                                                                         |
| MF2_RIP            | MF3_RIP            | Ripper projectile (does not disappear on impact)                                               |

#### Rip sound
- dsda-doom: [commit](https://github.com/kraflab/dsda-doom/commit/3d9fc1cccc7b85c527331e74802dd25d94a80b10)
- woof: [PR](https://github.com/fabiangreffrath/woof/pull/207)
- When set, this is the sound that plays for ripper projectiles when they rip through something.
- Add `Rip sound = X` in the Thing definition.
- `X` is the sound index, as seen in other sound fields.

#### Fast speed
- dsda-doom: [PR](https://github.com/kraflab/dsda-doom/pull/37), [commit](https://github.com/kraflab/dsda-doom/commit/ad8304b7df2a6fde2c26f6241eb40e00e954cb58)
- woof: [PR](https://github.com/fabiangreffrath/woof/pull/207)
- Sets the thing speed for skill 5 / -fast.
- Add `Fast speed = X` in the Thing definition.
- `X` has the same units as the normal `Speed` field.

#### Melee range
- dsda-doom: [PR](https://github.com/kraflab/dsda-doom/pull/46)
- woof: [PR](https://github.com/fabiangreffrath/woof/pull/207)
- Sets the range at which a monster will initiate a melee attack.
- Also affects the range for vanilla melee attack codepointers, e.g. A_SargAttack, A_TroopAttack, etc.
  - Similarly, adjusting the player mobj's melee range will adjust the range of A_Punch and A_Saw
- Add `Melee range = X` in the Thing definition.
- `X` is in fixed point, like the Radius and Height fields

#### Weapon Flags

- dsda-doom: [PR](https://github.com/kraflab/dsda-doom/pull/27)
- woof: [PR](https://github.com/fabiangreffrath/woof/pull/214)
- Add `MBF21 Bits = X` in the Weapon definition.
- The format is the same as the existing thing `Bits` field.
- Example: `MBF21 Bits = SILENT+NOAUTOFIRE`.

| Name           | Description                                      |
|----------------|--------------------------------------------------|
| NOTHRUST       | Doesn't thrust things                            |
| SILENT         | Weapon is silent                                 |
| NOAUTOFIRE     | Weapon won't autofire when swapped to            |
| FLEEMELEE      | Monsters consider it a melee weapon              |
| AUTOSWITCHFROM | Can be switched away from when ammo is picked up |
| NOAUTOSWITCHTO | Cannot be switched to when ammo is picked up     |

MBF21 defaults:

| Weapon          | Flags                                   |
|-----------------|-----------------------------------------|
| Fist            | FLEEMELEE+AUTOSWITCHFROM+NOAUTOSWITCHTO |
| Pistol          | AUTOSWITCHFROM                          |
| Shotgun         |                                         |
| Chaingun        |                                         |
| Rocket Launcher | NOAUTOFIRE                              |
| Plasma Rifle    |                                         |
| BFG             | NOAUTOFIRE                              |
| Chainsaw        | NOTHRUST+FLEEMELEE+NOAUTOSWITCHTO       |
| Super Shotgun   |                                         |

#### Ammo pickup weapon autoswitch changes

- dsda-doom:[PR](https://github.com/kraflab/dsda-doom/pull/26)
- woof: [PR](https://github.com/fabiangreffrath/woof/pull/214)
- Weapon autoswitch on ammo pickup now accounts for the ammo per shot of a weapon, as well as the `NOAUTOSWITCHTO` and `AUTOSWITCHFROM` weapon flags, allowing more accuracy and customization of this behaviour.
- If the current weapon is enabled for `AUTOSWITCHFROM` and the player picks up ammo for a different weapon, autoswitch will occur for the highest ranking weapon (by index) matching these conditions:
  - player has the weapon
  - weapon is not flagged with `NOAUTOSWITCHTO`
  - weapon uses the ammo that was picked up
  - player did not have enough ammo to fire the weapon before
  - player now has enough ammo to fire the weapon

#### New DEHACKED "Ammo per shot" Weapon field
- dsda-doom: [PR](https://github.com/kraflab/dsda-doom/pull/24)
- woof: [PR](https://github.com/fabiangreffrath/woof/pull/214)
- Add `Ammo per shot = X` in the Weapon definition.
- Value must be a nonnegative integer.
- Tools should assume this value is undefined for all vanilla weapons (i.e. always write it to the patch if the user specifies any valid value)
- Weapons WITH this field set will use the ammo-per-shot value when:
  - Checking if there is enough ammo before firing
  - Determining if the weapon has ammo during weapon auto-switch
  - Deciding how much ammo to subtract in native Doom weapon attack pointers
    - Exceptions: A_Saw and A_Punch will never attempt to subtract ammo.
  - The `amount` param is zero for certain new MBF21 DEHACKED codepointers (see below).
- Weapons WITHOUT this field set will use vanilla Doom semantics for all above behaviors.
- For backwards-compatibility, setting the `BFG cells/shot` misc field will also set the BFG weapon's `Ammo per shot` field (but not vice-versa).

## Miscellaneous

#### Option default changes
- comp_pursuit: 1 (was 0)

#### Demo format / header
- MBF21 occupies complevel 21.
- Demo version is 221.
- longtics are enabled while recording.
- Old option entries removed:
  - variable_friction (always 1)
  - allow_pushers (always 1)
  - demo_insurance (always 0)
  - obsolete / empty bytes
- New option entries:
  - New comp options from above
  - Size of comp option list
    - This could be used to add comp options later without adding a complevel
    - When reading a demo that has a size larger than what you expect, abort
- Header options block:

| Key                   | Bytes |
|-----------------------|--------------|
| monsters_remember     | 1     |
| weapon_recoil         | 1     |
| player_bobbing        | 1     |
| respawnparm           | 1     |
| fastparm              | 1     |
| nomonsters            | 1     |
| rngseed               | 4     |
| monster_infighting    | 1     |
| dogs                  | 1     |
| distfriend            | 2     |
| monster_backing       | 1     |
| monster_avoid_hazards | 1     |
| monster_friction      | 1     |
| help_friends          | 1     |
| dog_jumping           | 1     |
| monkeys               | 1     |
| comp list size (23)   | 1     |
| comp_telefrag         | 1     |
| comp_dropoff          | 1     |
| comp_vile             | 1     |
| comp_pain             | 1     |
| comp_skull            | 1     |
| comp_blazing          | 1     |
| comp_doorlight        | 1     |
| comp_model            | 1     |
| comp_god              | 1     |
| comp_falloff          | 1     |
| comp_floors           | 1     |
| comp_skymap           | 1     |
| comp_pursuit          | 1     |
| comp_doorstuck        | 1     |
| comp_staylift         | 1     |
| comp_zombie           | 1     |
| comp_stairs           | 1     |
| comp_infcheat         | 1     |
| comp_zerotags         | 1     |
| comp_respawn          | 1     |
| comp_soul             | 1     |
| comp_ledgeblock       | 1     |
| comp_friendlyspawn    | 1     |

#### Fixes / adjustments since mbf
- Fix 3 key door bug
  - Already fixed in pr+ / EE / woof.
  - dsda-doom: [code](https://github.com/kraflab/dsda-doom/blob/61eac73ea246b48b17a30bc5a678a46b80d48fa1/prboom2/src/p_spec.c#L1086-L1090)
  - woof: [code](https://github.com/fabiangreffrath/woof/blob/56a04885a090cc28e6330a6ed61877ccaea90e21/Source/p_spec.c#L818-L820)
- Fix T_VerticalDoor mistake
  - Already fixed in pr+ / EE / woof.
  - dsda-doom: [code](https://github.com/kraflab/dsda-doom/blob/cd2ce9f532a80b871f0fdef2ae3ce6331b6e47b4/prboom2/src/p_doors.c#L588-L589)
  - woof: [code](https://github.com/fabiangreffrath/woof/blob/56a04885a090cc28e6330a6ed61877ccaea90e21/Source/p_doors.c#L454-L483)
- Fix buggy comp_stairs implementation
  - Already fixed in pr+ / EE / woof.
  - dsda-doom: [code](https://github.com/kraflab/dsda-doom/blob/6006aa42d3fba0ad2822ea35b144a921678821bf/prboom2/src/p_floor.c#L894-L895) and [code](https://github.com/kraflab/dsda-doom/blob/cd2ce9f532a80b871f0fdef2ae3ce6331b6e47b4/prboom2/src/p_floor.c#L934-L942)
  - woof: [code](https://github.com/fabiangreffrath/woof/blob/56a04885a090cc28e6330a6ed61877ccaea90e21/Source/p_floor.c#L824-L827) and [code](https://github.com/fabiangreffrath/woof/blob/56a04885a090cc28e6330a6ed61877ccaea90e21/Source/p_floor.c#L863-L866)
- P_CreateSecNodeList global tmthing fix
  - Already fixed in pr+ / EE / woof.
  - dsda-doom: [code](https://github.com/kraflab/dsda-doom/blob/cd2ce9f532a80b871f0fdef2ae3ce6331b6e47b4/prboom2/src/p_map.c#L2601-L2603)
- A_CheckReload downstate
  - Already fixed in pr+ / EE.
  - dsda-doom: [code](https://github.com/kraflab/dsda-doom/blob/cd2ce9f532a80b871f0fdef2ae3ce6331b6e47b4/prboom2/src/p_pspr.c#L636-L643)
  - woof: [PR](https://github.com/fabiangreffrath/woof/pull/202)
- Fix P_DivlineSide bug
  - Already fixed in pr+ / EE.
  - dsda-doom: [code](https://github.com/kraflab/dsda-doom/blob/cd2ce9f532a80b871f0fdef2ae3ce6331b6e47b4/prboom2/src/p_sight.c#L408)
  - woof: [PR](https://github.com/fabiangreffrath/woof/pull/202)
- P_InterceptVector precision / overflow fix
  - Already fixed in pr+ / EE.
  - dsda-doom: [code](https://github.com/kraflab/dsda-doom/blob/cd2ce9f532a80b871f0fdef2ae3ce6331b6e47b4/prboom2/src/p_maputl.c#L161-L166)
  - woof: [PR](https://github.com/fabiangreffrath/woof/pull/202)
- Fix generalized crusher walkover lines
  - Already fixed in EE, but not in pr+.
  - dsda-doom: [commit](https://github.com/kraflab/dsda-doom/commit/76776f721b5d1d8a1a0ae95daab525cf8183ce44)
  - woof: [PR](https://github.com/fabiangreffrath/woof/pull/202)
- Fix blockmap issue seen in btsx e2 Map 20
  - Already fixed in EE, but not in pr+.
  - dsda-doom: [commit](https://github.com/kraflab/dsda-doom/commit/c31040e0df9c2bc0c865d84bd496840f8123984a)
  - woof: [PR](https://github.com/fabiangreffrath/woof/pull/202)
- Fix missing dropoff condition
  - Already fixed in pr+, but not in EE.
  - dsda-doom: [code](https://github.com/kraflab/dsda-doom/blob/cd2ce9f532a80b871f0fdef2ae3ce6331b6e47b4/prboom2/src/p_map.c#L1037)
  - EE: [code](https://github.com/team-eternity/eternity/blob/0fc2a38da688d9f5001fef723b40ef92c5db0956/source/p_map.cpp#L1342)
  - woof: won't fix
- P_KillMobj thinker updates
  - Changed in pr+, reverted for mbf21.
  - dsda-doom: [commit](https://github.com/kraflab/dsda-doom/commit/c5d99305ef2aa79983f5e95ac6cdc13ce415b54c)
- A_Mushroom changes
  - Changed in pr+, reverted for mbf21.
  - dsda-doom: [commit](https://github.com/kraflab/dsda-doom/commit/a330db45dee7f255510f6b2c06006e97dc04d578)
- Fix negative ammo counts
  - dsda-doom: [PR](https://github.com/kraflab/dsda-doom/pull/24)
- Fix weapon autoswitch not taking DEHACKED ammotype changes into account
  - dsda-doom: [PR](https://github.com/kraflab/dsda-doom/pull/24)

#### Important Notes
- The default ammopershot value for fist / chainsaw is 1 (matters for backwards compatibility).
- Depending on how your port has modified spawning code, you may have a lingering bug related to the validcount variable. Basically, in vanilla doom, this variable is incremented in the wrong position in P_CheckPosition. The explanation for why this is a problem is complicated, but ripper projectiles (and possibly other cases) will expose this bug and cause desyncs. If your port strives for demo compatibility, I recommend adding an extra validcount increment in P_CheckPosition before running the P_BlockLinesIterator, as was done in dsda-doom (wrap it in a compatibility check for mbf21 if necessary): [code](https://github.com/kraflab/dsda-doom/blob/5ebd1c7860fae536dc46e5a310acb98ca59a3165/prboom2/src/p_map.c#L916-L920). Without that fix, vanilla heretic demos go out of sync, and the same will be true for mbf21 demos with certain dehacked features in play. A port like crispy doom wouldn't be affected because it hasn't modified the spawn code in a way that introduces the bugged validcount. Ports like pr+ and ee do have this problem, for example.

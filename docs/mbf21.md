# MBF21 Current Status

These are changes / features that are currently implemented.

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
  - [commit](https://github.com/kraflab/dsda-doom/commit/c5d99305ef2aa79983f5e95ac6cdc13ce415b54c)
- A_Mushroom changes
  - Changed in pr+, reverted for mbf21.
  - [commit](https://github.com/kraflab/dsda-doom/commit/a330db45dee7f255510f6b2c06006e97dc04d578)
- Fix negative ammo counts
  - [PR](https://github.com/kraflab/dsda-doom/pull/24)
- Fix weapon autoswitch not taking DEHACKED ammotype changes into account
  - [PR](https://github.com/kraflab/dsda-doom/pull/24)

#### Important Notes
- The default ammopershot value for fist / chainsaw is 1 (matters for backwards compatibility).
- Depending on how your port has modified spawning code, you may have a lingering bug related to the validcount variable. Basically, in vanilla doom, this variable is incremented in the wrong position in P_CheckPosition. The explanation for why this is a problem is complicated, but ripper projectiles (and possibly other cases) will expose this bug and cause desyncs. If your port strives for demo compatibility, I recommend adding an extra validcount increment in P_CheckPosition before running the P_BlockLinesIterator, as was done in dsda-doom (wrap it in a compatibility check for mbf21 if necessary): [code](https://github.com/kraflab/dsda-doom/blob/5ebd1c7860fae536dc46e5a310acb98ca59a3165/prboom2/src/p_map.c#L916-L920). Without that fix, vanilla heretic demos go out of sync, and the same will be true for mbf21 demos with certain dehacked features in play. A port like crispy doom wouldn't be affected because it hasn't modified the spawn code in a way that introduces the bugged validcount. Ports like pr+ and ee do have this problem, for example.

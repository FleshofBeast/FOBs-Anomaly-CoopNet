# Native multiplayer source audit

Reference: official TES3MP 0.8.1 checkout, commit
`49be5b6405d6ab427e06ed350cf76c715a1f3bdd`.
The local checkout is research material under ignored `_reference/`.
This document is internal implementation evidence; public release notes describe
CoopNet behavior without naming the reference project.

## Code paths inspected

| Reference implementation | Behavior | X-Ray implementation and remaining gap |
| --- | --- | --- |
| `LocalActor.cpp:55`, `Cell.cpp:35` | Native authority actor updates; separate position, animation flags/play, speech, death, stats, equipment, attack and cell-change batches | Host runs shared NPC AI; guest runs native presentation. `WorldState` carries poses, cycles, health and killer identity. Discrete NPC attack/speech events still need a complete channel. |
| `DedicatedActor.cpp:57`, `:85` | Remote actor retains native object; interpolates small moves, snaps cell changes, feeds movement settings and rotation | `WorldPosePlayback` retargets smoothly, snaps spawn/large moves/death; post-UpdateCL restoration prevents native presentation from replacing received poses. |
| `Cell.cpp:600`, `CellController.cpp:400` | Explicit cell authority determines local versus dedicated actors | CoopNet host owns the party's active location. Authority migration and independent locations are outside the current party-travel design. |
| `LocalActor.cpp:266`, `Cell.cpp:361`, `MechanicsHelper.cpp:346` | Attacks are explicit events, with target identity and result; receiver invokes native `onHit` rather than deriving attacks from movement | Reliable `WorldHit` applies native host `Hit` once; `PlayerHit` delivers host attacks to the owner client's native `Hit`. Replay/generation/epoch validation protects both directions. NPC attack presentation still needs explicit replication. |
| `Cell.cpp:227` | Death state and instant-death collision behavior are separate from movement | Shared death includes killer identity and calls native guest `Die` once. Protocol 34 copies individual native rigid bodies; paired boar fights verified 13-body application and matching settled positions. One-second rendered captures do not establish frame cadence or internet behavior. |
| `LocalPlayer.cpp:101`, `:364`, `:459`, `:508`; `DedicatedPlayer.cpp:77`, `:263` | Local player owns native movement/inventory; remote equipment is applied to a separate native presentation | Native guest owns movement, gear and condition after initial character restoration. Host player presentation uses owner equipment instead of host inventory. |
| `ObjectList.cpp:99`, `:147`, `:168` | Container records include count, condition/charge and explicit changes, applied to native stores | Personal loot uses native item spawn records in the local portable character file. Sources are keyed by world scope, location, native name and section. This intentionally keeps loot personal; the reference uses shared container events. |
| `ObjectList.cpp:406`, `:467`, `:577`, `:696`, `:727`, `:791`, `:811` | Placement, spawning, removal, state, movement, animation and sound are distinct operations | Native NPC lifecycle and anomaly state are covered. Protocol 35 adds anomaly lifecycle with bounded native sphere/box collision geometry; paired electrical creation/damage/removal and extended visual/save regression passed. General object sound events remain incomplete. |

These are executable paths, not evidence that every line in the reference engine
has an equivalent in X-Ray. Morrowind spells, inventory stores and cell references
must be replaced by Anomaly hits, native inventories, ALife and level identities.
Normal-session acceptance now verifies native aimed bullets, shared death,
owner damage, personal loot and electrical collisions without an opt-in flag.
Complete follower/story support and the remaining event channels above are not
established by these fixtures.

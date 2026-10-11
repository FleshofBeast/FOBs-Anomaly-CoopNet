# Guest simulation and TES3MP comparison

The guest should run native presentation locally: skeletal animation, interpolation,
weapon feedback, particles, sound, camera and UI. The host chooses shared-world
outcomes: NPC AI, spawning, shared NPC damage and death. Each player runs native
personal inventory, quests, rewards, loot views and environmental damage.
Guest actions must reach the host and their results must return to every player.
This keeps one host world and does not introduce location workers.

## What TES3MP does

The inspected TES3MP 0.8.1 implementation distinguishes locally authoritative
actors from dedicated remote actors. `Cell::updateLocal` sends actor position,
animation flags and playback, speech, death, stats, equipment, attacks and casts.
`Cell::updateDedicated` updates remote actors locally. Incoming animation commands
invoke native animation playback; position and equipment have separate handlers.
TES3MP can assign a player as the actor authority for a cell. This is authority
delegation, rather than every client independently deciding every actor's actions.

Primary sources:

- [Cell replication implementation](https://github.com/TES3MP/TES3MP/blob/0.8.1/apps/openmw/mwmp/Cell.cpp)
- [Local actor implementation](https://github.com/TES3MP/TES3MP/blob/0.8.1/apps/openmw/mwmp/LocalActor.cpp)
- [Actor authority API](https://docs.tes3mp.com/en/latest/api/ActorFunctions.html)

The architecture is applicable to X-Ray, but OpenMW actor code is not a drop-in
implementation for X-Ray's animation, physics, Lua or ALife systems.

## Current native gameplay behavior

Normal sessions use native guest gameplay. Saved equipment is replayed through
native inventory hooks before the saved active weapon is drawn. Guest native
bullet collisions send reliable hits; the host applies native damage once and
returns health/death. Mirrored guest weapon playback cannot apply duplicate damage.
Host attacks against the player are delivered to the owner's native damage path.
Personal tasks and loot remain local and are included in the portable character.

Protocol 34 adds individual corpse rigid-body state with native physics between
updates. Paired fights in an open arena passed actual host and guest bullet hits,
shared death and 13-body boar ragdoll application. See `NATIVE_WORLD_REWORK.md`
for build/run evidence and visual-review limits. Remote aim pitch now uses the
native spine/shoulder/head factors while keeping the model's root upright.
Guest dynamic world-radio production is suppressed to avoid duplicating received
host broadcasts. This does not establish complete world-event replication.

## Earlier implementation checkpoints

- Protocol 31 gives the guest ownership of ordinary player movement. Its native
  local controls and physics send position, velocity and movement state to the
  host. The host interpolates that representation with a 50 ms buffer rather than
  integrating the guest's movement a second time. Returned host poses do not
  correct the guest's own character. Initial placement, map travel and respawn
  remain explicit host actions; a respawn epoch rejects delayed pre-respawn poses.
  Host health, combat outcomes and shared-world authority remain unchanged.
- Protocol 29 world poses carry up to four native base-channel animation cycles,
  with motion slot/index, phase, speed and stop state. Guests play these cycles
  and advance native animation tracks locally. Matching model and motion assets
  are required on both clients.
- Earlier host-controlled guest weapons generated authoritative hits. The current
  native path uses owner collision reports and disables mirrored weapon damage.
- The replacement combat evaluator retains the stock evaluator's `ignored_zone`
  upvalue table. Previously, the copied function could call `ipairs(nil)` when
  evaluating ordinary NPC enemies.
- Applying a host NPC pose explicitly enables its native visibility.
- Protocol 30 also carries baseline anomaly state and time within that state.
  Guests run a dedicated effects update for native idle lights, awakening and
  accumulation particles, blowout particles/sound/light/wind, grass effects and
  camera effects. They do not run anomaly activation, damage, artifact creation
  or Lua callbacks. Anomalies are excluded from the NPC spawn catalogue.

## Remaining integration work

These changes do not establish complete world simulation. Base-channel skeletal
cycles do not include all attack sounds, particles, additive animations or AI
action events. Dynamically creating/removing anomalies and object-attached entrance
and hit particles still need separate event replication; the current anomaly
effects path targets zones present in the canonical world snapshot.

Validation must include a guest bullet actually damaging a host NPC, a native
electric anomaly damaging a guest without crashing either client, visible
Sidorovich in a fresh join, and moving/attacking NPC animations in rendered scenes.
Ammo consumption, object counts and codec tests alone do not establish these.
Do not publish this checkpoint as a verified fix for all reported failures.

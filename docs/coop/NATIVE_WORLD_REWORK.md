# Native world simulation rework

## Current anomaly lifecycle checkpoint (protocol 35)

Normal native sessions now include anomalies in the reliable lifecycle catalogue,
with up to 64 validated native sphere/box shapes per zone. Baseline zones bind by
unique section and position; new zones are instantiated with native collision
geometry. Authority removal destroys the guest native object. Changes to position,
rotation or shape replace it after native destruction, avoiding stale collision
geometry. That replacement path is implemented but not separately exercised by
the electrical fixture. State/timing snapshots continue through the pose stream.
Both clients require protocol 35.

All 21 suites passed (`native-anomaly-unit3.log`), including shape serialization,
truncation, invalid geometry, and decimal-calibre ammunition persistence. DX11
build 5 passed (`native-anomaly-build5.log`). The paired normal-session restart
run `native-anomaly-paired3.log` passed actual guest bullet damage, automatic saved
pistol drawing, quests, exact personal loot contents, owner hits, native electrical
damage, dynamic anomaly creation/removal and clean portable exit saving. Original
source saves stayed unchanged. The guest applied native electrical power 0.800
and reported health 0.240 before the fixture restored its health.

Paired screenshots at 07:07:02 in `native-world-c890a8296e05411681ca95b38457c09e`
were inspected: the host sees the guest inside the electrical field on the clear
hillside, while the guest receives the native electrical hit overlay. One-second
captures do not establish precise effect cadence or WAN behavior. Each of the
three radio IDs arrived once in this run. Its predecessor passed the same anomaly
interaction but failed personal loot persistence; the validator rejected decimal
calibre sections. Encode/decode now allow dots in section names while rejecting
paths, with regression coverage for 5.45 mm and 11.43 mm ammunition.

Persistent saved followers, general NPC attack/speech events, broader mod anomaly
classes and long-session/internet behavior still need work or validation.

## Expanded native gameplay checkpoint (earlier development history)

The exact source-path comparison is recorded in `NATIVE_SOURCE_AUDIT.md`.
Protocol 33 adds reliable owner-side player damage and owner equipment updates.
Portable character files now include personal native tasks, script variables,
known information, and touched corpse/container contents with native item state.
Guest native death invokes local callbacks once with mapped killer identity;
native frame playback restores received poses and cycles after UpdateCL.

All 21 standalone suites pass (`native-completion-unit5.log`), with additional
killer/name codec coverage in `native-completion-world-state-test.log`. DX11 build
10 passed. The isolated run `native-world-d3a9a899a21a49ab869b4cf2ddc35202`
passed shared hit/death/removal, personal quest restoration, native loot transfer,
electrical collision/damage, portable exit saving and clean shutdown with original
saves unchanged. Its complete acceptance result failed because host player object
zero was rejected as a temporary hit identity. That binding is corrected in source
and requires a rebuilt acceptance run. Portable restart and natural aimed combat
remain pending. This development path is still opt-in and unpublished.

The rebuilt restart acceptance passed (`native-completion-resume1.log`, DX11
build 11). It restored personal tasks and the touched container's single item
with exact saved condition, delivered host attacks through native owner `Hit`,
verified electrical damage and shared lifecycle, and saved both clients cleanly
without changing private source saves. Natural aimed bullet collision remains
pending, as do the source-audit gaps listed separately.

Normal sessions now enable native guest gameplay automatically. An explicit
`-coop_passive_world` option keeps the old path available for regression fixtures.
The selected character's equipped items are replayed through native Ruck/Slot
hooks, stale active-slot state is cleared, and the saved active weapon must finish
drawing before character restoration is considered ready. Native personal quest
rewards are no longer copied to the other player's host-side inventory.

DX11 build 19 and the no-opt-in-flag native acceptance passed
(`native-completion-default-natural2.log`, fixture
`native-world-12d39d1cee1b464090c3d7b2274b094d`). An aimed native pistol bullet
produced an actual enemy collision, exactly one authoritative lethal hit, mapped
guest kill callbacks, and shared death/removal. Personal quests, loot transfer,
electrical collision/damage, host-to-owner hits, portable exit saving and clean
shutdown also passed with private source saves unchanged. The saved active
loadout reconnect passed (`native-completion-default-resume1.log`). This is not
complete story/follower support.

## Paired combat and rigid bodies (protocol 34)

The live fixture uses native trigger input on both clients and an unfrozen hostile
boar in a checked open arena. It requires actual native bullet collisions from
both players, guest damage applied by the host, native enemy death, rendered
frames and guest rigid-body application. Trigger activity alone is insufficient.

DX11 build 30 and all 21 standalone suites passed
(`native-completion-build30.log`, `native-completion-unit6.log`). The paired run
`live-combat-8893465c2fce40b78ea26eae8a10c47f` recorded host bullets and three
guest damage confirmations before native death. The guest applied the boar's
13 native physics bodies. Active bodies update at 10 Hz, settled bodies at 1 Hz;
native physics runs between updates. Body states include transforms, interpolation
history, velocities, forces, torques, enabled flags and bone visibility/root.
Packet size determines world-state chunking. This historical checkpoint used protocol 34.

Paired frames at 01:01:28 and 01:01:35 show the death and corpse downhill from
both viewpoints. The settled root difference was approximately 5 cm. These
one-second captures do not establish animation cadence or network-latency quality.
The same frames exposed incorrect remote aiming. DX11 build 31 and the next
paired run `live-combat-b8b84d929ece4a498506465ff0c3456d` passed actual host and
guest hits and the fought enemy's 13-body application. Paired 01:07:27 frames show
remote downhill aiming using native spine/shoulder/head pitch factors, with an
upright root. Full discrete NPC attack/speech events,
dynamic anomaly lifecycle and portable follower restoration remain incomplete.

Fixture options writes now use each client's own appdata. Guest world-radio
production is suppressed while authoritative broadcasts are received; the live
run received each of its three radio record IDs once. Longer gameplay and
reconnect radio behavior still need review.

The protocol-34 normal-session regression passed
(`native-completion-default-natural3.log`), but its reconnect failed the active
weapon assertion (`native-completion-default-resume2.log`). The portable file
contained an active item; the host restored an older inventory journal instead.
Native joins now retain the supplied personal profile even when a host journal
exists and import it before considering journal inventory or condition. Legacy
passive sessions retain their journal behavior. The damage fixture also explicitly
recovers and draws its test weapon before exit because native impact effects can
drop it. This preparation does not replace the separate automatic-draw assertion
at reconnect.

The first corrected reconnect still failed (`native-completion-default-resume3.log`):
expired session credentials triggered `ClientPump::stop`, clearing the selected
profile before the runtime retried admission. The runtime now caches the selected
profile and reapplies it when restarting admission after expired credentials or
an offline connection. A reconnect must exercise this retry as well as journal
priority before the saved-loadout fix is considered verified.
When recovering a dropped live connection, the runtime refreshes that profile
from the owner's current native inventory before sending it again. This prevents
newly acquired or changed equipment from reverting to the original join profile.
Dropped live-connection recovery still needs separate native acceptance.

The subsequent retry exposed validation during host loading
(`native-completion-default-resume4.log`). Host admission now holds ordered
profile/ready frames until a native host actor exists, with a bounded 120-second
wait. A standalone test holds a selected profile through 20 seconds of loading,
then verifies exactly one validation. All 21 suites passed again
(`native-completion-unit7.log`).

The next run selected the correct profile but import never completed
(`native-completion-default-resume5.log`): an extra fixture bandage made a strict
inventory-count equality impossible. Import now verifies each saved item's
ownership/state independently, allowing other items to arrive. It also waits for
the saved active item and native weapon draw before publishing its first view.
Exit capture waits for personal restoration; pending joins retain the selected
profile, and stale host views cannot replace the native owner's exit snapshot.

## Native pose playback checkpoint

Native guests now retarget shared NPC transforms continuously over 100 ms,
including shortest-path angle interpolation, before native frame playback.
First placement, corrections exceeding five metres, and death poses apply
immediately. Physics character placement follows the presented transform.
Anomaly state and health remain immediate; private followers bypass this path.
Playback storage belongs to the native object and is discarded on destruction
or level teardown. This does not add AI behavior, attack events, or independent
location simulation. Standalone tests cover retarget continuity, angle wrapping,
teleports, explicit snaps and the millisecond counter wrapping.

Validation: all 21 standalone suites and the DX11 CoopNet build passed.
The 120-second isolated native fixture passed in
`_build/native-world-471b50396ffc49869a21bb3584bfe091`: native NPC frames,
shared guest hit/death/removal, personal quest restoration and clean shutdown
passed, with private source saves unchanged. This fixture does not measure
rendered smoothness or establish complete natural combat and persistent loot.

GitHub release titles and notes must describe CoopNet features and validation
without naming the reference multiplayer project, per the user's instruction.

The requested model is a shared session with locally controlled players, native
client simulation, personal quests/rewards, and personal corpse/container loot.
The host supplies shared entity identities, placement, behavior and anomaly
placement. Existing location workers remain excluded.

## TES3MP source audit

The inspected 0.8.1 sources distinguish LocalActor from DedicatedActor. Cell
publishes position, animation flags/playback, speech, death, dynamic stats,
equipment, attacks, casts and cell changes. DedicatedActor advances movement and
animation flags locally and applies dynamic stats. Its AI handler invokes native
travel, wander and target-dependent AI packages. Authority is assigned explicitly;
running the same functions independently is not the synchronization mechanism.

Primary references:

- [Cell update and receive paths](https://github.com/TES3MP/TES3MP/blob/0.8.1/apps/openmw/mwmp/Cell.cpp)
- [LocalActor change capture](https://github.com/TES3MP/TES3MP/blob/0.8.1/apps/openmw/mwmp/LocalActor.cpp)
- [DedicatedActor native playback and AI instructions](https://github.com/TES3MP/TES3MP/blob/0.8.1/apps/openmw/mwmp/DedicatedActor.cpp)

## X-Ray gaps confirmed in the current release

- update_world_replica bypasses virtual UpdateCL, disables character physics and
  advances skeletal tracks through a substitute path.
- schedule_world_replica bypasses native NPC/mutant scheduled behavior.
- Native NPC binders, level scripts, task updates, local hits and inventory
  operations are suppressed or redirected to host transactions.
- Host quest snapshots replace the guest task/info registry; host inventory
  snapshots repeatedly replace guest gear, money and reputation.
- The standalone and fixture checks do not establish complete rendered NPC
  behavior, natural combat, interaction or personal quest progression.

## Development implementation

Confirmed gameplay policy: NPCs and mutants share host-coordinated positions,
behavior and deaths. Each character has separate corpse and container loot,
quests and rewards. Native guest playback must not make conflicting AI decisions.

The first native-world prototype reached connected native stalker frame updates
but crashed in CSoundMemoryManager::update on the guest. This test failed;
restoring native update calls alone is not sufficient and must not be released.

The -coop_native_world development flag retains canonical joined-world identity
but separates it from the passive simulation policy. It restores native frame,
script, hit, quest and inventory operations. Shared NPC scheduled planners remain
host-owned; anomaly and player schedules run locally. The initial character
inventory is imported once per actor incarnation; later host views do not remove
new local loot or overwrite personal money/progress. Personal character capture
reads actual native inventory. Host quest updates are ignored in this mode.
Quest packets arriving before baseline loading are ignored as well. A completed
inventory bootstrap is tracked separately from its revision, so asynchronous
item spawning cannot prematurely stop after changing actor incarnation.

This path is deliberately unpublished while integration continues. It is not a
finished TES3MP equivalent. Required work before making it the normal join path:

- Capture/restore the player's complete native quest and script state before a
  host baseline is loaded; persist it with the character and across travel.
- Relay client-owned health and equipment to shared representations; validate
  guest-originated shared hits in native gameplay.
- Apply host behavior instructions without competing independent NPC decisions;
  interpolate shared poses through the native movement/animation systems.
- Preserve per-character corpse/container contents and claims across rejoin,
  world restarts and location changes.
- Validate natural firing, NPC/mutant combat, electrical damage, rendered
  animations, NPC dialogue, separate quests/rewards and independent looting with
  two native clients before replacing the public package.

## Development checkpoint validation (2026-10-10)

DX11 build passed (`_build/native-world-build7.log`) and all 20 standalone suites
passed (`_build/native-world-unit1.log`). Two 120-second native runs passed after
the lifecycle fixes. The first (`_build/native-world-prototype3.log`) completed
native stalker frames, matched all 56 NPCs including three dynamically spawned
mutants, and closed both clients without changing private source saves.

The expanded run (`_build/native-world-prototype4.log`) verified host-created
enemy spawn/death/removal on the guest, correlated all three events to the same
shared anchor, recreated a visible native trader, excluded host quest snapshots,
and shut down normally with unchanged private saves. These checks exercise
existing lifecycle replication, not guest-originated shared combat or persistent
personal quests/loot. Native frame completion does not establish rendered
animation quality or internet behavior.

Run `powershell -ExecutionPolicy Bypass -File .\test-coopnet-native-world.ps1`
after a DX11 CoopNet build. This uses isolated appdata and the explicit
`-coop_native_world` development flag. Installed clients and release assets have
not been replaced by this checkpoint.

## Shared hit development path (protocol 32 checkpoint)

Native guest GE_HIT events against shared NPCs are intercepted before local
damage. A bounded reliable hit message carries the requesting actor binding,
level, respawn epoch, target anchor/lifetime, damage, direction, bone, impulse,
armor penetration and native aim/wound flags. The host verifies ownership,
generation, level readiness and monotonic sequence, limits hit bursts, then checks
target lifetime, native bone bounds, living actors and range before calling the
target's native Hit implementation. Host weapon playback for guest actors cannot
apply a second hit in this development mode. Normal world snapshots carry health
and death back to clients. Queued hits are cleared on location teardown/respawn.

The host relies on the guest's collision report; independent host trajectory
validation and remote hit-reaction playback are not implemented. The native
fixture injects an explicit GE_HIT event against a host-created enemy to exercise
the actual interception, transport and native damage path; it does not establish
natural aimed firing or internet behavior. This remains opt-in and unpublished.

Validation: the final DX11 build passed (`_build/shared-hit-build2.log`) and all
21 suites passed (`_build/shared-hit-unit3.log`), including malformed/truncated
hits, bounded fields/flags, ownership, replay, sequence wrap, stale generations,
burst limits and budget refill. The 120-second two-client run passed
(`_build/shared-hit-native1.log`, fixture
`native-world-583afdbcb87446279d17c7fabdb28ec5`). The native guest event left local
health unchanged before confirmation, one host hit changed that same enemy from
1.000 to 0.000, and the guest applied its shared death and removal. Native trader
recreation, quest snapshot exclusion, clean shutdown and unchanged private source
save hashes also passed. Complete personal quest/loot persistence and natural
combat remain unverified.

### Personal quests and local quest followers (development)

Canonical loading now captures native tasks, active selection, known information,
and the character's Lua task manager/pstor before disconnect. Restoration runs
before actor script scheduling and task evaluation. The scoped native task format
also preserves completion/failure information conditions and read state, which
the ordinary task serializer omits. Timers are rebased to the new world clock.
NPC references are resolved by unique native name/section; unmatched references
are cleared rather than bound to an unrelated host object with the same numeric ID.
World settings and global simulation state are not imported from the character.

The personal quest script hooks native escort creation and special companion
squads inside task initialization. Private followers are excluded from host world
catalogues and bypass shared pose, hit and scheduled-AI interception locally.
Completion/failure callbacks return existing followers to their stored home and
release their private flag. Quest-created followers remain private when dismissed.
The host's saved private followers are not adopted by guests as their companions.

This is a development foundation, not a completed persistent follower system.
Recreation/remapping of already-active saved followers, followers across party
travel, task effects that recruit companions
outside initialization, and natural escort completion/failure need further work
and validation. Unresolved dynamic task targets also need a suspended-task path.
These cases remain limits of the native gameplay checkpoint; they must be tested
before advertising complete persistent follower or story support.

### Saved loadout reconnect verified (2026-10-11, build 44)

`_build/native-completion-default-resume9.log` passed the normal native gameplay
restart acceptance after repeated failed imports were investigated. The spawn
antifreeze queue discarded saved inventory because its native guest parent is
intentionally outside ALife. Session inventory now follows the immediate native
spawn path. The host imported both portable saved items and selected slot 2;
the guest restored its saved pistol without manual unequip/re-equip and queued
an actual native aimed bullet hit. The run also passed personal quests, exact
personal loot condition/contents, electrical collision/damage, owner-side hits,
portable persistence and clean shutdown with source saves unchanged. This is a
local paired acceptance result, not proof of complete story/follower parity or
internet latency behavior.

The same build subsequently passed `_build/native-live-combat13.log` with native
AI and both actors firing on the clear hillside. Guest hit sequences 1 and 2
reduced host enemy health 0.228 -> 0.015 -> 0.000; the guest applied 13 native
ragdoll bodies for that exact anchor. Paired rendered screenshots at 02:13:55
were inspected and showed the corpse at corresponding hillside locations.
Guest radio IDs 1, 2 and 3 each appeared once in this 150-second run. Stills do
not establish animation cadence, longer-session radio behavior, or WAN quality.

### Retained lifetime recovery (2026-10-11)

Local native destruction now schedules catalogue reconciliation when the current
host snapshot still retains the exact anchor and incarnation. This recovers NPCs
and zones removed by guest cleanup without requiring a new host catalogue
revision. Session shutdown clears catalogue identities, pending native spawns and
dirty state so they cannot be reused by a later session.

`native-recovery-build1.log` and `native-recovery-paired1.log` passed. The fixture
explicitly destroyed only the guest copy of the dynamic electrical anomaly. The
retained host anchor 14941469092277187840 was queued for recovery and appeared
again in the native spawn log with the same authority lifetime. The recreated
zone delivered native power 0.800; the host later retired it and guest native
removal was verified. Saved pistol drawing, real guest bullet damage, quests,
personal loot, portable exit saving and source-save preservation also passed.
Paired 07:22:50 frames were inspected and show the guest in the host's electrical
field and the owning client's native electrical overlay. This verifies a local
recovery case, not full follower support or internet/session-duration parity.

## Respawn destination revision (development, 2026-10-11)

The requested rule now replaces periodic XYZ snapshots: a player voluntarily
respawns at their last map arrival position, independently of spectator selection.
Before a transition checkpoint exists, choose an existing faction start location
on the player's loaded map from `plugins/new_game_start_locations.ltx`. Free
stalkers in Cordon use Rookie Village. Other factions choose the configured
starting point nearest their native session arrival. Do not send a player to a
faction start on a different unloaded map.

The current checkpoint is scoped to the connected player/generation and session;
persistence across reconnects and saves, independent map travel and ownership
handoff are still unfinished. Full-party recovery voting has portable coverage
but has not been wired into gameplay. The additional requested teammate revive
must validate living rescuer, target lifetime, map, body proximity and reliable
owner revival; it remains unfinished. Voluntary spectator mode and respawn remain
part of the requested outcome. No installed build or release was changed.

Validation of this increment: DX11 build `_build/faction-entry-prefix-build.log`
and all 24 portable suites passed. The two-client native death fixture
`_build/death-orbit-627ba2e4ae7a4c1297fb26c9ea74e154` passed after correcting
native `actor_*` community IDs to the configuration's faction section names.
Both host and guest respawned at Rookie Village XYZ
(-221.083, -19.941, -168.182); host position was compared to its recovery point
with another teammate selected. Native movement capsules, equipment retention,
death orbit/menu, normal shutdown and source-save hashes passed. Final logs
contain no BusyHandsDebug, critical Lua or fatal error. Host and guest screenshots
were inspected. This proves the Cordon starting fallback, not every faction's
visual spawn placement, transition persistence, remote corpse visual parity,
squad-vote gameplay or teammate revival.

## 2026-10-12 stopping point: host checkpoint bootstrap and spectator rendering

Downed actors now reject first-person HUD rendering, including the native HUD-view
query used by held items. The death-orbit probe asserts this state. Remote local
animation selection now checks for a live matching blend, restarts missing tracks,
and lets normal actor motions run when an explicit replicated cycle has no active
track. These changes address the detached spectator arms and stale animation
cache paths; reconnect T-posing and remote corpse parity still need dedicated
visual acceptance.

The native acceptance fixture now streams one authenticated host offline-region
checkpoint to the guest over reliable world frames. The guest validates detached
native packets and existing object, inventory and squad registry bindings before
replay. Squad membership is checked through the serialized group member list;
member m_group_id itself is not serialized. Incompatible native topology remains
pending reconciliation without disconnecting the guest. This path is fixture-only,
prefetches a checkpoint, and does not activate a simulation lease.

DX11 build _build/offline-bootstrap-membership-spectator-build.log passed.
Native acceptance _build/offline-bootstrap-membership-spectator-acceptance.log
passed using native-world-90bd642f052b435ebe3f1c1cfa5b1372. The guest restored
438 objects on level 14 (115160 bytes), retaining native registry aliases.
Shared natural combat, native NPC frames, electrical damage, personal quest/loot
and character save checks passed; clients closed normally and source saves were
unchanged. This proves compatible offline bootstrap, not independent map play.

Resume with dedicated reconnect/respawn animation and spectator visual coverage,
remote ragdoll replication, registry reconciliation, then actual host-approved
lease activation/acknowledgement, worker simulation/publication and independent
map travel. Host global weather/story/save authority must remain intact. Gameplay
all-dead voting and nearby teammate revive remain pending. No release or installed
build deployment was performed.

Death verification also passed: _build/spectator-hud-death-acceptance.log,
fixture death-orbit-46d0debef9db4bb0b33d05bdcb623fd3. Both actors had native
11-element ragdolls, anchored orbit/menu, HUD-view suppression and revival with
retained ammunition. Guest screenshot ss_player_10-12-26_00-15-00_(l01_escape).jpg
was inspected: corpse and spectator menu are visible without detached first-person
arms. No critical Lua, BusyHandsDebug or fatal markers appeared in either death
log. Both clients closed normally; source saves remained unchanged. Larger goal
paused at the user's request for the night; resume from the work listed above.

## 2026-10-12 resumed: native restoration confirmation

Offline bootstrap restoration now has a reliable guest-to-host acknowledgement.
The guest queues it only after native restoration succeeds and retries on send
backpressure. The host requires the sending player to have a completed transfer,
matching region key, epoch and checkpoint revision; duplicate confirmations are
idempotent. This confirms prefetch restoration and grants no simulation lease.

DX11 build _build/bootstrap-ack-build.log and native acceptance
_build/bootstrap-ack-acceptance.log passed. Fixture
native-world-1de0d967c0b649c19b48f49ce46bab92 proved host receipt of the native
restoration confirmation alongside natural combat, shared native deaths/removal,
quest/loot, anomaly damage and portable character saving. Both clients closed;
source saves were unchanged. No installed deployment or release was made.

Shortest integration path: prove one guest-owned loaded separate map end to end,
then expand the existing 3x3 ownership lattice. Engine gates requiring integration
are CALifeUpdateManager update/shedule_Update (currently return for all guest
replicas), schedule_world_replica (currently suppresses guest online NPC planners),
and frame playback (currently overrides guest shared entities with host poses).
These need per-region authority and confirmed native bindings before enabling
worker simulation. Preserve separate host-only world-save guards and host clock,
weather and story ownership. Loaded-map native bootstrap, canonical reconciliation,
worker publications and native handoff remain unimplemented; this confirmation
alone does not prove independent maps or single-player parity.

## 2026-10-12 single-loaded-map process memory baseline

Command: test-coopnet-native-world.ps1 -Acceptance -NaturalCombat -NativeDefault
-MemoryCapture. Fixture native-world-2039d16f253844bd95f7ecf68bafb5c0 loaded
Cordon (l01_escape) on both clients. DX11, 1280x720, texture_lod 2 and
r__no_ram_textures on. Samples taken every approximately 500 ms for 150 seconds;
290 samples per process, final-minute average uses 117 samples from 90 seconds.
Raw data: fixture memory-samples.csv; aggregates: memory-summary.json.

| Process | Final-minute resident GiB | Peak resident GiB | Final-minute private commit GiB | Peak private commit GiB |
| --- | ---: | ---: | ---: | ---: |
| Host | 1.727 | 1.785 | 4.183 | 4.201 |
| Guest | 1.753 | 2.074 | 4.209 | 4.218 |

These are total rendered game-process measurements, not incremental map-only
allocations or headless coordinator memory. Working set measures currently
resident RAM; private bytes measure private committed virtual memory, not merely
address-space reservation. GPU VRAM is not included. The final minute is a timed
loaded-gameplay window, not a claim of a stationary statistical steady state.
The fixture passed shared native combat, NPC lifecycle, anomaly collision/damage,
personal state and native bootstrap acknowledgement. Both clients closed normally
and source saves were unchanged (_build/single-map-memory-acceptance.log).

This supports a one-loaded-map-per-client design for the tested configuration:
resident RAM stayed below 4 GiB, while private commitment was slightly above it.
It does not establish heavier-map/mod-pack requirements, per-additional-player
cost, separate-map hosting cost, or 100-player scalability. The host should retain
compact canonical state for foreign cells rather than loading their geometry and
renderable entities. Native host bookkeeping and global persistence still require
memory; packet routing alone is not the full coordinator responsibility.

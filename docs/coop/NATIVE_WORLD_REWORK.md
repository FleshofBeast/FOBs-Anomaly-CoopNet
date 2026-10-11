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

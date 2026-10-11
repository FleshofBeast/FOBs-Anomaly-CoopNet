# Region authority implementation status

Protocol 38 source work, 2026-10-11. This is not a completed release of delegated
native simulation. Installed host and test games remain the tested protocol 37.

Development policy: push source commits to the coopnet branch while this rework
is in progress. Do not publish full releases until a working test version passes
native gameplay acceptance for the requested scope.

## Host-directed world, delegated cell processing

The host remains the source of truth for the shared world: game time, weather
cycles/FX, emissions and global world events, shared story/quest progression,
global ALife decisions, identifiers and persistence. A cell lease delegates
local execution; it does not turn a guest's standalone world into a competing
authority.

The host issues the local simulation checkpoint and authoritative world/event
revision. The cell worker runs native local AI, physics, anomaly behavior and
approved local event hooks against that state. Results return to the host for
ownership, lifetime and event validation before the host commits and relays
them. Host global directives also apply to cells whose workers are on other
levels. Shared events need host-assigned identities and once-only commit rules;
cell handoff must not replay spawns, rewards or event triggers.

Current protocol separation enforces that RegionState carries local object
poses/actions/physics/anomaly state, while WorldClock, WorldRules and shared
quest/event publication remain separate host channels. The cell-worker send API
rejects those global message types. Native local-script execution must enforce
the same separation when delegation is integrated; checkpoint transport alone
does not yet provide that enforcement.

## Implemented

- RegionAuthority partitions by native level ID and X/Z grid coordinates.
- Session grid width comes from live ALife switch_distance, rather than an
  unrelated chunk constant. The lattice remains fixed for the session.
- Each player subscribes to exactly nine cells: their current cell and its
  eight neighbors. Crossing a cell boundary plus a one-eighth-cell margin shifts that 3x3 footprint; the
  overlapping six cells retain their owner and epoch. Native maps/navigation
  remain intact. The older radius subscription helper remains available for
  bounded protocol tests but is no longer used for native player footprints.
- First interested player receives a pending owner identity. Overlapping
  subscriptions retain the owner, rather than repeatedly stealing ownership.
- Dormant, Quiescing, Restoring and Active states separate eligibility from
  permission to run AI. A successor cannot become active before restoring the
  exact checkpoint. Orderly departure requires the predecessor's shutdown
  checkpoint; actual disconnection uses the last retained checkpoint.
- Epochs reject former owners, and per-epoch sequence windows reject duplicate
  and reordered publications, including sequence wrap.
- Checkpoint and region counts, allocation sizes and aggregate retained bytes
  are bounded. Checkpoint chunks support reordering and identical duplicates,
  reject conflicting duplicates, and cannot mix keys, epochs or revisions.
- Protocol messages and HostPump/ClientPump hooks carry leases, restore
  acknowledgements, chunked checkpoints and region world states. These hooks
  require admission and completed baseline/level barriers. Directionality and
  queue backpressure remain explicit.
- Native runtime maps host/guest interest from actual native actor positions
  and current ALife ranges. No AI authority changes occur until native
  checkpoints and handlers are installed.
- On the host's currently loaded level, ALife switching now brings entities
  and anomalies in the union of all players' 3x3 footprints online, including
  diagonal cells outside the previous circular distance. The host still runs
  their native simulation. This does not delegate a different loaded level to
  a guest, or override host-centered Lua spawn checks/global event scripts.

RegionAuthorityTests exercises handoff barriers, overlap, disconnection, stale
epochs, checkpoint revisions, sequence wrap, level isolation, grid sizing and
range hysteresis. RegionNetworkTests exercises two admitted peers through the
actual host/client pumps, chunk transfer, restore acknowledgement and relay,
with owner/duplicate/stale publication rejection.

## Required native integration before activation

1. Capture and restore a canonical per-region object checkpoint, including
   server records, native AI save data, script binder data, inventory children,
   incarnations, anomalies and once-only shared event state. Coordinate native
   ID allocation for new objects published by different owners.
2. Freeze the outgoing owner's native scheduling and script decisions before
   acknowledging quiescence. Restore the new owner before enabling native AI,
   physics and script execution. Guests that only subscribe continue playback.
3. Enforce object/catalogue authorization in addition to region leases. A
   legitimate region owner cannot invent arbitrary bindings or overwrite
   objects outside its assigned authority.
4. Process whole owned cells even where cell boundaries extend beyond a
   player's circular ALife radius; interest and simulation ownership are
   separate. Preserve coherent squad ownership across cell boundaries.
5. Replace party-only travel and host-only actor/world publication with
   independent loaded-level ownership, per-level retained catalogues, and
   validated owner state relay. A guest's level load must not reload the host.
6. Keep global ALife/time/story decisions coordinated. Simply running all
   stock scripts on every client duplicates events and cannot establish parity.

The engine's native save path is a useful starting point: CGameObject::net_Save
serializes native state and the script binder; CAI_Stalker::save also serializes
inventory-owner and brain state. Existing spawn restores client_data through
net_Load. Hot-loading an already bound script object requires separate native
validation; copying transforms or opaque save bytes alone does not prove a
safe handoff.

Acceptance remains two native clients on separate maps, real NPC combat and
scripted events without the host nearby, overlap without duplicate processing,
owner departure/disconnection, save/reload persistence, and visual verification.
Portable tests establish the ownership/protocol foundation only.

## Verified reference grid behavior

Checked the authorized local reference commit
49be5b6405d6ab427e06ed350cf76c715a1f3bdd:
components/misc/constants.hpp defines CellGridRadius = 1 and cell size 8192.
apps/openmw/mwworld/scene.hpp uses that radius for mHalfGridSize.
scene.cpp::changeCellGrid loads both axes inclusively from center-minus-radius
through center-plus-radius, retains overlapping active cells, and unloads the
others. getNewGridCenter retains the center until Chebyshev distance exceeds
half the cell width plus 1024 units: a one-eighth-cell boundary margin.

loadCell registers local scripts and native physics/mechanics. unloadCell
removes native physics/mechanics, local scripts and sound. Multiplayer additions
record LOAD/UNLOAD states and LocalPlayer::sendCellStates reports them to the
server. ProcessorActorAuthority initializes local actor controllers only for
the assigned owner of an active loaded cell; other visitors use remote actors.

This confirms a 3x3 active exterior grid, rather than claiming that merely
subscribing to nine IDs replicates all scripts. Interior cell loading is a
separate scene path. Weather also has separate region authority in the server
scripts; it is not implicitly solved by the actor grid.

The same reference commit's mwmechanics/actors.cpp::updateProcessingRange has
an explicit multiplayer change: maxProcessingRange = 8192 * 50 and minimum
range = half that maximum. This deliberately expands the actor AI range far
beyond the loaded grid. Consequently, copying only the active-grid dimensions
while retaining a small native online-simulation radius would miss a material
part of the reference behavior. The host ALife grid relevance override is the
current Anomaly equivalent on its loaded map; guest delegation remains pending.

## Verification of this source increment

All 23 portable suites passed in `_build/region-rework-verified.log`, including
the new ownership and two-peer protocol suites. DX11 compilation completed in
`_build/region-source-final-build.log`.

The isolated native fixture in
`_build/native-world-8ac328701b214bafb15cdb1c0a414109` passed existing native
combat/world acceptance: NPC frames, hit/death/removal, personal quests and loot,
electrical collision/damage, player hits, character save and aimed bullet
collision. Both clients closed normally and source saves remained unchanged.
Its host log reported grid width 750.000, online 675.000, offline 825.000.
That fixture used the preceding compiled increment before the final checkpoint
copy/allocation hardening; those final changes passed portable tests and native
compilation. No delegated native AI was active in this regression.

The subsequent exact 3x3 footprint plus host ALife relevance build passed the
native acceptance fixture in `_build/native-world-0f36ef69406d4938b19ee022c802bbcb`
(`_build/region-3x3-native-regression.log`). Its host logged the nine-cell grid
with 750 m cells. The source-matched recenter margin then passed all 23 portable
suites in `_build/region-3x3-reference-tests.log` and compiled in
`_build/region-3x3-reference-build.log`. The native acceptance does not establish
distant quest/event-hook parity or independent-level ownership.

Existing weather replication in capture_world_clock /
apply_host_world_clock carries game time, time factor, cycle and weather FX.
HostPump now projects the host's time/weather directive onto each recipient's
loaded level, preserving the recipient load barrier. WorldSettingsTests checks
a host on a different level, loading suppression and weather FX on the guest's
new level. Native independent-level travel and weather rendering verification
remain outstanding; a cell worker cannot choose its own global weather/time.

## Native checkpoint capture increment

NativeRegionCheckpoint.h adds a versioned, bounded envelope for stock native
spawn/save records, including canonical native IDs, lifetime incarnations and
parent ordering. Validation rejects duplicate identities, missing parents,
cycles through parent ordering, truncated records and excessive size. Decoding
does not mutate the output on failure. Empty cells have a valid checkpoint.

capture_native_region_checkpoint is a host-only owner-thread bootstrap adapter.
It selects native NPC, anomaly and container roots in a cell, follows their
inventory children, exports server state and captures stock net_Save script
binder and AI state into server client_data before Spawn_Write. Actors and
personal quest followers are excluded. A missing native child fails capture
rather than silently dropping inventory. This captures online objects only;
offline ALife records, restore, lease activation and cross-map persistence
remain required. No network payload is passed to native load by this increment.

All 24 portable suites passed in _build/native-region-checkpoint-tests.log.
Native compilation and gameplay acceptance are tracked separately; portable
envelope tests do not establish successful native save restoration.

Each object envelope now includes a validated native section name. Host capture
also reads its locally generated Spawn_Write record back through F_entity_Create
and Spawn_Read, checking native ID, parent ID, section and exact client_data.
An isolated native acceptance probe checks checkpoint envelope round-trip using
live cell objects. It does not activate leases or prove restore/handoff.

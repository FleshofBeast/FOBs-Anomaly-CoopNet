# Simulation range and delegated ownership audit

Audit date: 2026-10-11. This document records source findings and the proposed
authority boundary. Delegated region simulation is not enabled by this audit.

## Native ranges

The extracted Anomaly configuration at
`_build/coopnet-config-audit/configs/alife.ltx` specifies `switch_distance = 750`,
`switch_factor = 0.1`, and `auto_switch = false`. No loose alife.ltx was found in
either installed game's configuration directory. These are configuration
findings; scripts can change the live switch distance.

`src/xrGame/alife_switch_manager_inline.h` calculates the online threshold as
distance * (1 - factor), and the offline threshold as distance * (1 + factor).
For this configuration the thresholds are 675 m and 825 m. The difference
prevents repeated online/offline transitions at a single boundary. Individual
objects have additional switching eligibility and same-level requirements.

The stock xr_patch script's 250 m normal / 2,000 m startup values apply only
when auto_switch is enabled. They are not the configured default in this build.

Offline ALife continues a simplified simulation. Switching online creates the
full native game object and its detailed AI/physics; exceeding the distance
does not mean all ALife processing stops.

The 30 m / 60 m CROW_RADIUS values in xrEngine/xr_object.h are camera-relative
object update prioritization, not ALife ownership boundaries. Scheduled updates
also call MakeMeCrow. A networking grid must not use those constants as a hard
simulation cutoff.

## Reference authority behavior

Inspected local client source:
`_reference/TES3MP/apps/openmw/mwmp/Cell.cpp` and
`processors/actor/ProcessorActorAuthority.hpp`.

An explicit server authority packet selects one client. The new authority
removes its remote actor controllers, initializes local actors and enables
local navigation updates. Other visitors remove local actor controllers. Late
authority packets for unloaded cells are ignored.

Inspected server policy:
https://github.com/TES3MP/CoreScripts/blob/0.8.1/scripts/logicHandler.lua
(`LoadCellForPlayer`, `UnloadCellForPlayer`) and
https://github.com/TES3MP/CoreScripts/blob/0.8.1/scripts/cell/base.lua
(`AddVisitor`, `SetAuthority`).

The first loader receives authority when no authority exists. A later visitor
can replace it when their ping is sufficiently lower. When the authority
leaves, a remaining visitor with the lowest ping is selected. Joining clients
receive saved and momentary cell state. This is more than first-entry ownership
or replication of coordinates alone.

## Implementation boundary for Anomaly

Keep compiled maps, collision geometry and navigation graphs intact. Key
ownership by native level ID, with logical spatial regions inside large levels
if needed. Underground/separate levels already have separate identifiers; a
region grid must include the level ID so overlapping coordinates in different
maps cannot share authority accidentally.

Separate three responsibilities: loading a native level, owning simulation,
and subscribing to state within a player's relevance range. A spatial grid
alone cannot make X-Ray load two native levels in one process.

The host coordinates validated leases and persistent shared state. A player
processing a region publishes native actor/action/anomaly state; other visitors
render replicas. Retain a single owner while eligible, apply hysteresis to
subscription changes, and transfer a checkpoint before enabling a successor.
Every publication must carry an owner epoch and sequence so delayed packets
from a previous owner cannot overwrite current state.

Checkpointing must include spawns/deletions, health/death, inventory, scripted
AI state, anomaly state and once-only shared events. Squad AI crossing region
boundaries needs an explicit coherent ownership rule. Global ALife/time/story
state requires one coordinating authority, rather than independently running
every script on all clients.

Current remaining integration barriers include guest world publication being
blocked by world_level_is_replica, host-coordinated party level assignment,
host-centered script spawn checks, and world packets without an authority epoch.
The existing same-level guest-aware ALife distance change does not remove these
barriers and does not implement independent maps.

Acceptance requires two actual clients on separate levels, continued native
AI/combat and scripts while the host is elsewhere, overlap without duplicate
processing, owner departure/disconnect handoff, stale packet rejection, and
save/reload persistence. Portable ownership tests alone cannot establish this.

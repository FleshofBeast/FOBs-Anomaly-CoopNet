# Development checkpoint - 2026-10-11

The full user goal is active and unfinished. Publish development commits only;
no full releases until a working test version passes. Installed host and test
clients retain tested protocol 37. Development is protocol 38 and checkpoint
schema NRC3. Do not install this increment as a completed rework.

## Main priority

Finish host-directed cell ownership and independent guest maps, with a 3x3
processing-range grid per player, distant native NPC/anomaly/script behavior,
host global event/weather/time/story/save authority, safe checkpointed handoff
and two-client native gameplay/visual acceptance.

The native engine still has one loaded level per process. Region transport and
lease barriers exist but native delegation remains inactive. Separate-map capture
is now verified; separate-map processing, restoration and gameplay are not.

## Current checkpoint implementation

NRC3 retains native Spawn_Write and UPDATE_Write packets plus online/offline
state, ID, parent, section and lifetime. Validation permits real ammunition
names containing decimal dots, while rejecting malformed names, duplicate IDs,
missing parents, incomplete packets and excessive size.

Online capture includes saveable ALife props and script objects as well as NPCs,
anomalies and containers. Offline capture walks the ALife registry, maps game
vertices to level IDs, selects cell roots and includes owned inventory children.
Registry add/remove/load/destruction hooks maintain CSE lifetimes across native
online/offline switches and ID/allocation reuse. Locally generated packets are
read back through native classes before retaining them.

The first offline native test crashed while destroying a temporary unregistered
CSE_ALifeOnlineOfflineGroup. Its destructor now clears detached member IDs when
no simulator is assigned. Registered groups retain native member-unregistration
behavior. The corrected separate-map acceptance passed.

## Verification

- All 24 portable suites passed in _build/checkpoint-offline-tests.log.
- Native build passed in _build/checkpoint-detached-group-build.log.
- Native acceptance passed in _build/checkpoint-detached-group-acceptance.log,
  fixture _build/native-world-c5dd32434f2b4a67948df761e755fabe.
- Loaded host cell: level 3, 925 objects, 510 inventory children, 226279 bytes.
- Separate-map offline cell: host level 3, captured level 14, 438 objects,
  115144 bytes; repeated captures retained stable ALife lifetimes.
- Existing native NPC frames, combat/death/removal, electrical damage, personal
  quest/loot restoration, player hits and character-save checks also passed.
  Both owned clients closed and private source saves remained unchanged.
- Death/respawn acceptance passed in _build/death-camera-visual-acceptance.log,
  fixture _build/death-orbit-5c25cbd0d30044d5ad9b093608618c6e. Host/guest screenshots
  were inspected: native corpses, camera above the floor and readable menu.
  Both actors recovered movement capsules; ammunition and all-dead guards passed.

## Paused work checkpoint (2026-10-11)

The user resumed the goal after this pause. The checkpoint commit also
contains group-aware capture in CoopNetNativeCheckpoint.inc: squad groups and
their members are captured together, including members outside the root cell.
The native build passed in _build/checkpoint-squad-closure-build.log. The next
acceptance run failed with "Live native region checkpoint round-trip missing"
in _build/checkpoint-squad-closure-acceptance.log, fixture
_build/native-world-7467b861cb104de88696213c10524dc3. Diagnose this before claiming
the group capture changes are accepted.

CoopNetNativeRestore.inc contains unfinished retained offline replay for existing
registry objects. It checks lifetimes and structural relationships before
replaying saved native state. This addition has not been compiled or tested;
it does not implement fresh-object reconstruction or ownership handoff.

The user reported a duplicate T-posing player model following a sliding death
ragdoll. Host presentation now removes the living guest proxy when native
health reaches zero, leaving the native corpse as its only body. The build and
death acceptance passed in _build/death-duplicate-proxy-build.log and
_build/death-duplicate-proxy-acceptance.log, fixture
_build/death-orbit-82e15608bd6447d090ae907cea84a69c. The test also checks proxy
removal between native death and respawn. Host screenshot inspection showed the
corpse without an upright duplicate. Guest screenshot inspection still showed
the dead host proxy upright: remote player ragdoll replication remains missing.
The indoor fixture does not prove sliding-corpse parity on a slope.

Cell capture now skips unsaveable inventory children as native
CALifeObjectRegistry::save does. The previous failure rejected such a child
before emitting a checkpoint. Native build passed in
_build/checkpoint-unsaveable-children-build.log. Native acceptance passed in
_build/checkpoint-unsaveable-children-acceptance.log, fixture
_build/native-world-91dbb0518526417c890cbfbeae3dfba4. Both fixture clients closed
and source saves remained unchanged. No build or fixture sessions remain live.
Retained offline replay's runtime probe and subsequent corruption fixes are
described below; registry reconstruction and handoff remain unfinished.

## Restore probe and Lua corruption investigation

The new native probe mutates an offline inventory item's condition, rejects
changed parent/lifetime identities before replay, restores the saved condition,
then checks registry aliases and dependency identities through recapture.
Build _build/checkpoint-retained-restore-build.log passed. Acceptance
_build/checkpoint-retained-restore-acceptance.log FAILED: its restore marker
appeared, but the user observed a critical Lua warning in the owned fixture and
host shutdown crashed in the squad destructor. Fixture
_build/native-world-aa3d9d8003564866abb8bde425b28802 is not accepted evidence.

Scripted squad STATE_Read calls init_squad_on_load, which assigns the object to
the simulation board. Temporary read-validation objects must not perform that
live registration. A scoped guard now skips this callback during detached
validation; live reads retain the original callback. The harness also rejects
BusyHandsDebug, SCRIPT ERROR and critical Lua error markers. The guard build
passed in _build/checkpoint-squad-script-guard-build.log. Native acceptance is
FAILED in _build/checkpoint-squad-script-guard-acceptance.log: the user again
observed the same Lua warning and shutdown crashed in the group destructor.
Both owned clients are now terminal. The callback guard alone was insufficient.

The native MEMBERS container is associative_vector, whose insert replaces an
existing pair. Group STATE_Read inserted (ID, null), overwriting live pointers
during replay. STATE_Read now inserts only absent IDs. Fresh loads still bind
their initially null entries through on_after_game_load. Restore checks now
verify actual member pointers against the registry before and after replay.
The fix built in _build/checkpoint-live-squad-bindings-build.log. Native
acceptance passed in _build/checkpoint-live-squad-bindings-acceptance.log,
fixture _build/native-world-407eda7a27be419a978d8b4ef83eda19. Registry restore,
stale parent/lifetime rejection and native gameplay checks passed; both clients
closed normally and source saves remained unchanged. A host screenshot after
restore showed gameplay without the critical Lua warning.

During that run the user manually respawned and observed a living teammate
T-posing afterward. Logs confirm both owner and host actors revived. Revival
now invalidates cached head/torso/leg motion IDs and blend pointers, matching
stock spawn/visual reset behavior, and clears the torso action flag. This is
compiled in _build/respawn-animation-restart-build.log. Native death/respawn
verification passed in _build/respawn-animation-restart-acceptance.log,
fixture _build/death-orbit-1c1c58144a3646e1a5af5164659cb249. Both owned clients
closed and original source saves were unchanged. Visual proof of living
post-respawn animation remains incomplete: the host capture did not frame the
revived teammate. The user also supplied the guest's final all-dead screenshot,
matching our guest capture: it still shows the dead host as an upright proxy.
The full cell objective
and guest-side corpse replication remain unfinished.

The user selected safe-location recovery after a squad wipe, then specified
the exact caption "Everyone is Dead, Press F to Pay Respects". Each connected
player must press F once; this is a unanimous vote, and the host respawns the
whole squad only after all eligible players have voted. Show vote progress and
handle disconnection/roster changes without counting duplicate keypresses.
This outcome is not implemented yet; it replaces the old all-dead denial-only
test expectation and must be integrated and gameplay-verified before acceptance.
No installed client was updated. This remains development work and does not
establish safe worker import, reconstruction or cell ownership handoff.

These checks prove capture and the listed regression behavior, not restore,
lease handoff, remote-map simulation, distant event parity or Internet smoothing.

## Next work

Implement native validated restoration that preserves registry, parent, graph,
story, smart-terrain and group relationships. Group member IDs can reference
records outside ordinary inventory parent chains; audit closure and ordering
before registering restored groups. Bind restore acknowledgement and final
quiescent checkpoints to the existing epoch/lease barriers. Integrate independent
guest travel and separate-map actor/entity catalogues, host global script/event
journals and weather authority, then verify gameplay and visuals with two clients
on separate maps and through ownership handoffs. Do not replace this objective
with capture-only acceptance or enable delegation from transforms alone.

# Dedicated server with portable player characters

Requested behavior: players run their normal CoopNet-enabled clients, select a
saved character, and join one separately hosted world. The machine starting the
server connects through the same client path as every other player.

## Authority and persistence

The server owns the world save, global ALife entity identities and lifetimes,
story events, clock/weather directives, and simulation ownership assignments.
Clients run native simulation for assigned cells. Shared quest actors must reach
the client already processing their destination cell even if the quest holder
is on another map. Offline entities remain represented globally.

Character imports must preserve supported personal state: faction, progression,
money, reputation, inventory and item state, equipped slots, condition, and
personal quest/script data. Existing CharacterSave and PersonalState codecs and
verified character-file replacement provide reusable storage. Their existence
does not prove arbitrary native-save compatibility or complete quest parity.

Independent world histories cannot simply replace the server world. Personal
task bindings and script references need reconciliation to canonical server
entities. A missing or incompatible quest target must produce an explicit
compatibility result, rather than silently dropping progress or duplicating a
story actor. Original solo saves must remain intact during import.

Reconnect within a session must retain player/character identity and advance its
connection generation. Character exports and authoritative world checkpoints
must complete before releasing resumable state. Server restart persistence and
safe interrupted exports require gameplay tests in addition to codec tests.

## Implemented foundation (2026-10-13)

HostSession and HostPump support a dedicated session with no local host actor.
All four player slots are network clients; ready counts start at zero. Roster
messages explicitly distinguish this from a listen server, which still requires
its host actor. Dedicated session tests exercise four ClientPump connections,
capacity, ordinary client departure without ending the server, reconnect identity
and generation, malformed mode markers, and switching back to listen hosting.

This is protocol/session groundwork, not a playable dedicated server. The engine
runtime and menu do not invoke it yet. No installed client is changed.

### Separate-process transport acceptance

`test-coopnet-dedicated-network.ps1` builds the explicit DedicatedNetwork fixture
and launches one server process plus four client processes on localhost UDP
27891. The incoming GNS connection limit is configurable up to four; ordinary
listen hosting retains its three-remote-client default. Each fixture client
uploads a distinct character profile with money, faction, weapon condition,
ammunition and active equipped slot before becoming ready. The first reconnects
without resubmitting its character; the server checks its retained profile and
all clients observe the advanced connection generation. All processes exit
normally after the server observes their disconnections.

Acceptance passed in `_build/dedicated-network-acceptance.log`, with individual
process evidence in
`_build/coopnet-network-tests/dedicated-78b5e5d6b62143a8a50e3a714d540dbf`.
The initial sandbox run timed out; the network-enabled rerun exposed and fixed a
fixture assertion made before the first roster arrived. These are transport
and profile-transfer results, not native save import, disk persistence, world
simulation, rendered appearance, internet connectivity or performance evidence.

### Native dedicated-mode constraints found in source

The existing `g_dedicated_server` path cannot be assumed to retain single-player
simulation: `CAI_Space::init`, `unload`, and patrol-path loading return early in
`src/xrGame/ai_space.cpp`; native script binder reload/reinit is also skipped in
`src/xrGame/GameObject.cpp`. Simply enabling that flag would omit required AI
and script behavior. A coordinator with client simulation must retain those
systems on workers. A fully native headless-world alternative must deliberately
restore and verify them rather than treating a console window as proof.

### Canonical world bootstrap coordinator

`DedicatedBootstrap.h` owns the shared baseline and sequences each player's
profile admission, world transfer, native-load acknowledgement, actor assignment,
level acknowledgement and personal inventory restoration without a local actor.
Its required import callback must perform game-data validation and checkpoint
storage before acceptance. The caller must verify the supplied baseline digest.
An authenticated reconnect uses the retained server profile, so an older local
copy cannot overwrite that checkpoint. A new connection generation repeats the
world-load barrier; another player's delayed load does not block it.

`DedicatedBootstrapTests` exercises two clients with different factions and
personal money/loadout, full baseline transfer, delayed acknowledgements, wrong
map acknowledgement rejection, profile rejection, reconnect and attempted stale
profile rollback. This portable test uses synthetic baseline bytes. It does not
prove native world loading or native equipment readiness. The standalone
executable, verified disk-backed bootstrap and character checkpoints, native
worker integration and production menu path still require implementation.

## Remaining integration and acceptance

1. Extract the coordinator loop and disk-backed world state into a standalone
   executable; configure transport for four remote connections. It must start
   without a rendered player actor and must not advertise gameplay readiness
   until a compatible canonical world is available.
2. Replace native host-actor dependencies in admission, save import, ALife
   catalogues, inventory/dialogue/hit validation and map assignments with server
   state and verified simulation-owner reports.
3. Finish loaded-map bootstrap, canonical identity reconciliation, active leases,
   state publication and ownership handoff. Preserve host-authorized global
   state while permitting independent client maps.
4. Route menu Host through server launch plus ordinary local connection, and
   Join through connect, save selection/import, and acknowledged world loading.
5. Verify two real native clients importing different saves, separated maps,
   cross-map quest spawns, combat/anomalies, equipment readiness, disconnect,
   reconnect and restart persistence. Inspect rendered results and measure
   frame cadence, server CPU/memory and bounded queues. Portable session tests
   alone cannot establish this behavior.

The earlier alternative of one headless native engine simulating every loaded
map is a separate feasibility experiment. A dedicated coordinator with client
simulation does not establish simultaneous multi-map native server physics.

## Standalone native bootstrap (development)

Build with `build-coopnet-server.ps1`. The executable is produced at
`_build/coopnet-server/Release/CoopNetServer.exe`. Its first startup accepts:

```text
CoopNetServer --experimental-bootstrap 1 --data SERVER_DIRECTORY --world NATIVE_SAVE.scop --level NETWORK_LEVEL --port 27888
```

Subsequent startup needs only `--experimental-bootstrap 1 --data
SERVER_DIRECTORY` and the desired port. The seed is copied into a versioned
`world.bootstrap` archive with build identity, level, persistent world scope and
SHA-256 native-save digest. The initial game/mod fingerprints default to the
current menu's values 1/1; explicit `--game` and `--mods` must match clients.
The level is the network level ID, not a map name. Metadata/seed conflicts and
corrupted native payloads stop startup. One exclusive lock prevents concurrent
writers to the same data directory. Original native saves are read only.

Character profiles are read back and verified before atomic replacement, with a
backup on replacement. Existing disk checkpoints take precedence over a fresh
local upload. Current checkpoints contain typed personal inventory/progression
and condition; personal Lua/task state stays on the native client. The bootstrap
does not yet validate item sections against the full game configuration or write
ongoing gameplay updates. Those are required before exposing a production
server. Session credentials currently expire when the server restarts; native
clients have a fresh-admission fallback.

The coordinator relays owner poses after baseline and level acknowledgement.
Dedicated clients report their saved arrival pose through that channel before
waiting for its echo, avoiding a dependency on a native host actor to place them.
This grants no world simulation ownership.
Each transfer receives a fresh baseline ID while retaining the archive's world
scope and digest. Reusing the archive ID caused native reconnects to fail because
the client's existing baseline save name is intentionally protected against
overwrite; native restart testing exposed and verified this fix.

### Native acceptance evidence

`test-coopnet-dedicated-bootstrap.ps1` passed with two ordinary native client
processes and the standalone server, using copied Cordon saves. Both clients
loaded and acknowledged the canonical native world, completed arrival placement
and applied 24-item inventories with 1,000 rubles. Clients closed normally,
original source-save hashes stayed unchanged, and the server reopened its archive.
Evidence: `_build/dedicated-native-bootstrap-acceptance.log` and
`_build/dedicated-native-cd2b4f7238ce4515b8b318ee8f4ed0e5`.

Rendered screenshots from both clients were inspected: both showed loaded Cordon
interiors with first-person knife/hands. These frames do not establish remote
actor animation, combat, anomaly or world simulation parity.

During this bootstrap-only run, process sampling measured server peak resident
memory about 18.1 MiB and average CPU use 0.006 cores; native clients peaked near
1.94 GiB resident and 4.2 GiB private committed memory. The server was not running
native world simulation, so these are not full server performance numbers.
Corrupting a copied native payload was separately verified to reject startup
before listening and retain the damaged file unchanged; evidence:
`_build/server-corruption-8aa99ba34ede41a3bed75d4f661cac1b`.

`test-coopnet-dedicated-restart.ps1` also passed:
both native clients rejoined a new server process, restored the same two disk
character profiles and loaded new baseline transfers; the existing world archive
and profile file hashes remained unchanged. Evidence:
`_build/dedicated-native-restart-acceptance.log`. The first restart harness attempt
stayed in the menu because game shutdown rewrote its startup config; that harness
was corrected before the native baseline-name collision was diagnosed and fixed.

The latest user direction prioritizes complete shared-world behavior over earlier
prescribed architecture details. Bootstrap acceptance remains one prerequisite;
the full NPC/anomaly/combat/quest, separate-map, lifetime/handoff and ongoing
persistence requirements remain unfinished.

### Native script ownership audit

The native path already loads NPC script binders. Although `CGameObject` skips
binders for `world_replica_object`, that predicate deliberately returns false
in native co-op mode. A proposed deferred-script retention change was tested,
found to be unreachable for native NPCs and removed before commit. The failed
diagnostic fixture is `_build/dedicated-native-3fa91f66b03c4507ae25de57b31d40dc`;
its dedicated world/character loading still completed, but no retention marker
appeared. Do not treat this fixture as a passing simulation acceptance.

The actual decision gate is `schedule_world_replica`: it bypasses shared NPC
scheduled AI/script decisions until `native_online_cell_owned` is true. Native
frame playback and binder load are already preserved. The next integration must
grant and restore authenticated simulation leases, then route owner publications
through the dedicated server, rather than adding a second binder load path.

### Experimental cell coordinator integration

`--experimental-simulation 1` enables initial 3x3 cell delegation after canonical
world and character loading. Native clients acknowledge the loaded seed, then
only execute shared NPC scheduling in cells whose Active grant matches that
acknowledgement. Typed region catalogues and owner world states pass through
coordinator ownership, epoch and entity-lifetime checks. The coordinator uses
its own outgoing state sequence; worker sequence spaces cannot rewind observers.
Reliable catalogues are aggregated only after all newly granted cells report,
avoiding a partial snapshot deleting entities in another owner's cells.

Portable tests cover load barriers, observer acknowledgement rejection,
catalogue aggregation, premature/non-owner/duplicate state rejection, observer
relay, departing-cell revocation and refusal to activate a reassigned cell from
the obsolete seed. Native build and gameplay acceptance remain separate gates.
Use `test-coopnet-dedicated-bootstrap.ps1 -Simulation` for copied native clients,
rendered frames, frame-time and process memory/CPU evidence. This is initial
delegation only: fresh native checkpoint handoff, independent-map integration,
canonical global simulation, combat routing and ongoing persistence remain open.

The first native delegation run (`_build/dedicated-native-1a08794e23ab4400beb6fdb80a2b1619`)
accepted 13 catalogues and 281 state packets, but disconnected when walking NPCs
were remapped into different cells. Native roots now remain fixed for the ALife
registry lifetime and are cleared on destruction, world load and connection loss.
The next run (`_build/dedicated-native-18c0a24bdb2f453586839332d384b080`)
accepted 21 catalogues and 1,695 state packets without that disconnect. It still
FAILED acceptance: the owner measured 7.700 FPS / 236.401 ms p95, while the observer
measured 58.890 FPS / 19.785 ms p95. Both rendered first-person Cordon frames,
closed cleanly and preserved original saves. These are diagnostic runs, not
playability or visual parity approval. A client checkpoint polling bug was then
found: the host tick stays zero on clients, making its modulo condition capture
personal state every frame. A wall-time interval replaces that condition, with
additional native frame and publishing cost measurements pending verification.

### Peaceful test environment (2026-10-16)

Earlier `-Simulation` runs downed a player in a bandit raid on Rookie Village.
Reputation was not the cause: bandits are -2000 toward `actor_stalker` in
`communities_relations`, and reputation adds at most +500, so the attitude stays
below the -999 enemy threshold. `new-coopnet-fresh-save.ps1` generates a stock
Loner new game (`coopnet-fresh-stalker`) in a disposable client copy. With
`-Peaceful`, `test-coopnet-dedicated-bootstrap.ps1` passes `-coop_peaceful_probe`:
community goodwill cancels each hostile faction-table relation (sum neutral), host
disposition sync cannot push below neutral, and arrival moves to the nearest
sampled AI vertex at least 150 m from living mutants and faction-hostile humans.
It fails if either player is downed. Normal play never takes these paths.
`_build/dedicated-native-dc74394153d4462aa42d047d92fadf39` passed: 23 factions
neutralized, both clients placed 179 m from the original spawn (150.4 m clearance),
no player downed, 59.0 / 55.7 FPS. Combat synchronization still needs the
non-peaceful fixtures.

### Cell ownership handoff (2026-10-16, paused)

Each accepted owner catalogue is now saved as the cell's RegionAuthority
checkpoint (revisions above the seed's 1). A departing owner's last catalogue
completes the Quiescing barrier; on disconnect the next visitor is offered the
newest checkpoint. The coordinator streams it to the new owner as RegionChunks
after that owner holds the matching lease. The successor adopts the coordinator's
home cells for the checkpointed anchors, applies the records through
`apply_world_object`, then acknowledges that exact epoch/checkpoint. The seed (1)
is refused for any cell that was ever Active, and catalogue revisions restart per
owner epoch. Checkpoints carry the published NPC identity/lifetime/pose/health,
not planner or script internals: the successor resumes native AI from those poses.

Portable: `DedicatedSimulationTests` (graceful handoff, wrong/stale checkpoint
refusal, successor catalogue and state acceptance) and `DedicatedBootstrapTests`
pass. `test-coopnet-dedicated-bootstrap.ps1 -Simulation -Peaceful -Handoff` closes
the seed owner halfway through and requires server handoffs, successor checkpoint
application/acknowledgement and no `SIMULATION_REJECTED`.

Native fixes found along the way: catalogue-bound stand-ins now publish their bound
anchor (they previously published a new ID-derived identity, duplicating NPCs and
breaking home-cell roots), ownership events flush the engine log for the harness,
and the harness no longer hides the original failure behind cleanup errors.

Latest run `_build/dedicated-native-80e7150d6f8042c292e8d0e8b21abeb2` FAILED only on
the successor frame gate: all 9 cells handed off, 37 catalogues / 2,898 states
accepted, no rejection, reconnect or runtime error, but 24.1 FPS / 74.7 ms p95 after
takeover (scheduler 30-37 ms per frame). The cause is pre-existing identity
divergence: runtime spawns get different native IDs on each client, so an observer
holds both its own local copies and catalogue stand-ins of the owner's NPCs (story_id
duplicate warnings appear before any handoff). The publisher now skips 376 such
colliding local copies per catalogue, but they still run native AI once their cell is
owned. Next: give runtime spawns one coordinator-assigned identity (or retire
divergent local copies on observers) so a successor simulates each NPC once, then
re-run the -Handoff acceptance. Cross-map simulation, combat routing and ongoing
persistence remain open. All changes since c064556d are uncommitted.

Resumed and fixed: the successor published its own per-client ALife lifetime
counters as incarnations. When the coordinator's catalogue came back, every bound
native failed the incarnation match and was re-spawned as a stand-in, doubling AI.
Bound objects now keep publishing their shared (catalogue-bound) incarnation.
`_build/dedicated-native-3a76aebf600742568b78d946c6f6b7fd` PASSED with
`-Simulation -Peaceful -Handoff`: owner closed at 63 s, all 9 cells handed off,
36 catalogues / 3,218 states, no rejection or reconnect; successor 53.1 FPS /
26.3 ms p95 after takeover (scheduler 11-15 ms per frame, previously 30-37 ms).
Remaining observer issue (pre-existing, before any handoff): the observer still
spawns ~298 catalogue stand-ins and keeps colliding local copies (296 skipped per
successor catalogue; 7 story_id duplicate warnings). Next: bind observers' native
objects to catalogue anchors instead of replacing them, then retire the collision
skip. Cross-map simulation, combat routing and ongoing persistence remain open.

Observer duplication fixed (2026-10-16): observers do not run ALife switching for
cells they do not own, so NPCs and zones the owner has online were still offline
locally under the same identity; the catalogue then spawned a second copy of each
(~298), which collided with the original once the observer took ownership. The
observer now brings its own offline original online (same anchor, section and
level) and binds it, spawning a stand-in only when no original exists. Revivals
are limited to four per catalogue pass: an unthrottled burst crashed Anomaly's
spawn prefetch thread (`CLevel::ProcessPrefetchEvents`) in
`_build/dedicated-native-f66c5086994f4f8eb208410ece6d0aca`.
`_build/dedicated-native-5396b03a68e348d09d8dfb7dc49be44d` PASSED with
`-Simulation -Peaceful -Handoff`: 9 handoffs, 40 catalogues / 2,839 states, no
rejection; identity collisions 0 (was 296), story_id duplicates 0 (was 7), no
prefetch fault; successor 58.8 FPS / 18.7 ms p95. The collision skip remains as a
guard. Still open: cross-map simulation, combat routing, ongoing persistence,
observers far from the owner (revival is per catalogue entry, not distance-based).

### Combat routing milestone 1: remote player proxies and routed shots (2026-10-17)

Owner clients now spawn a native guest `CActor` proxy for each remote player on
their level (only while they simulate cells), driven from the network pose with
`apply_guest_owner_pose`/`control_guest_actor`; its body is not drawn because the
remote visual already renders that player. Protocol 39: the coordinator routes a
validated `WorldHit` (HostPump already checks shooter ownership, replay and
budget) to the Active owner of the target's home cell, only for the incarnation
that owner currently publishes; owners never receive their own shots. The owner
applies it natively with the shooter's proxy as attacker (`apply_owned_world_hit`).
NPC damage on proxies is absorbed for now: routing it to the real player is
milestone 2, followed by proxy weapons/animation and NPC targeting checks.

Evidence: portable `DedicatedSimulationTests` (stale lifetime, routed delivery,
replay, owner self-shot) and Session/Protocol/Bootstrap suites pass. Native
`-Simulation -Peaceful -RoutedHit` (fixture `-coop_routed_hit_probe`, one shot on
a bound NPC the observer does not own): routed and applied through the proxy with
damage, e.g. Devushka 0.886 -> 0.848 (`dedicated-native-daad42b5...`), and with a
late `-Handoff -HandoffAt 88` in `dedicated-native-cb4b651f...` and
`dedicated-native-211e3f83...` (both PASS).

Open defects:
- Intermittent (2 of ~5 late-handoff runs): right after a successor takes over,
  `sim_squad_scripted` -> `setup_squad_and_group` -> `change_team` reaches a squad
  member through a stale `db.storage` object whose game object was freed
  ([BusyHandsDebug] "Accessing destroyed object", then heap corruption 0xc0000374
  in `dedicated-native-62d4f9ca...`). `lua_error_not_crash` now logs the full Lua
  stack and flushes; fixture builds log `stale script storage after destroy` with
  the object's identity. Not yet caught with the audit enabled.
- Exit crash at `xrCore/log.cpp:114` (logging after string-container teardown),
  also seen on a plain non-CoopNet quit; likely pre-existing, does not affect play.
- Short sessions can miss the 300-frame performance window (false gate failure).

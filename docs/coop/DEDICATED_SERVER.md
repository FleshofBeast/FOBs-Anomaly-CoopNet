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

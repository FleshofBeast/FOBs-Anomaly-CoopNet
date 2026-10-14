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

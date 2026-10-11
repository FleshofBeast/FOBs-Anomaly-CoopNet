# Guest presentation and load handoff audit

Reported against protocol 35 on October 11, 2026. The user's host and guest
logs are preserved in `_build/user-report-20261011`; the supplied recording is
`Desktop 2026.10.11 - 08.53.13.01.mp4`.

## Findings and changes

- The guest first initialized its selected solo save, then loaded the canonical
  host save. That deliberately imports the host world, but exposed the interim
  character location. Retain the loading renderer and defer the completion
  prompt until the world is acknowledged, the owner is placed and the personal
  inventory is restored.
- The stock actor binder activates `loaded_active_slot` on its first update.
  Personal inventory restoration must follow that callback. Replaying native
  slot/belt move hooks and retaining equipment processing alone cannot prevent
  a later canonical-character callback from changing the active slot.
- Remote actors synthesized walking and aiming poses. They now receive the
  native actor's active animation cycle per partition, including motion ID,
  phase, speed and stop mode. The receiver advances the phase between updates
  and never invokes gameplay callbacks from presentation blends.
- An idle authoritative anomaly update could interrupt a guest's local contact
  activation. Preserve the native awakening/blowout until that local cycle
  finishes. Host disabled states continue to apply.
- Dialogue recruitment can occur outside `CGeneralTask.give_task`. Guest
  companion recruitment now isolates those NPCs from shared pose playback and
  scheduler suppression, allowing their native companion planner to run.

## Compatibility and verification

The appearance packet layout changes in protocol 36. Both clients must update.
Protocol validation covers native cycle round trips, truncation, duplicate
partitions, invalid phase values and replication to a connected client.

`test-coopnet-saved-equipment.ps1` attempts to draw each existing saved weapon
through native slot activation. It does not spawn weapons, move them between
inventory locations or directly repair their hidden state. Screenshots from
both clients require inspection; renderer submissions alone are insufficient.

## Completed checks

- All 21 portable suites passed (`_build/player-parity-unit.log`).
- The final paired menu/save test drew the saved knife, PM and BM16 without
  manual re-equipping. The stock Fanatic hunting dialogue recruited the NPC,
  whose native planner moved him toward the guest. Native walking stimulus
  completed (`_build/player-parity-equipment-hunting-walk-native.log`).
- Inspected host screenshots show the remote character holding the BM16 and
  changing walking poses on an open hillside. Still frames do not establish
  smooth motion at every rendered frame or under WAN latency.
- Native world acceptance passed aimed bullet collision, shared damage/death,
  loot transfer and electrical anomaly contact damage. Inspected guest frames
  show electrical activation effects (`_build/player-parity-anomaly-native.log`).
- The main and test installations received matching executable, symbols and
  personal quest script files; destination hashes matched the packaged files.

The complete scripted animation catalogue, other anomaly types, hunting task
completion/save persistence and real WAN latency remain gameplay checks. The
loading change retains the loading presentation across character import and
host-world loading; it does not eliminate those two underlying loads. A
successful fixture does not establish full single-player parity.

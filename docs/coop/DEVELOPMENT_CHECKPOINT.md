# Development checkpoint - 2026-10-11

Resumed at the user's request; the full goal is active again. Publish
development commits only; no full releases until a working test version passes.
Installed host and test games retain the tested protocol 37 binaries. Development
source is protocol 38. Do not install this increment as a completed rework.

## Main priority to resume

Complete host-directed cell ownership and independent guest maps, with a 3x3
processing-range grid, distant native NPC/anomaly/script behavior, host global
event/weather/time/story/save authority, safe handoff and native visual/gameplay
acceptance. This full goal remains unfinished.

Cell checkpoint capture still fails the native acceptance gate. The latest
fixture, _build/native-world-2b9e3f4fa093425d9292db3ac0ff3622, reports repeated
checkpoint probe pending markers after the elapsed-tick scheduling fix, without
a capture success marker or detailed rejection. Inspect the initial capture
guards and currently undiagnosed failure exits next. Do not activate delegation
from transforms alone or label existing combat acceptance as handoff evidence.
The fixture runner also encountered a shutdown race when a process exited
between WaitForExit and Stop-Process; its processes are no longer running.

## Focused death change before returning to the main work

User requested native player death/ragdoll, a spectator orbit anchored at the
death location, and the respawn menu. The former behavior kept the actor alive
at minimal health and moved the spectator camera to the selected teammate.

Development changes use native death shell activation, remember the death
location, allow right-drag/arrow-key orbit, render the full local corpse, and
restore character physics on host-approved revival. Selecting a teammate chooses
the respawn destination without moving the death camera. Movement to a teammate
occurs only after an explicit respawn request (or an isolated automated probe).

Gameplay verification of this death increment is still pending. Run
test-coopnet-death.ps1 against the final build. Its native probes require host
and guest ragdoll elements, no living movement capsule while dead, stable orbit
anchor and corpse coordinates during input, menu rendering, revival, retained
ammunition and denial when all players are dead. Inspect its screenshots and
verify resumed movement/equipment. Fix any failure before treating death as
complete, then return directly to the main cell/map goal.

## Last evidence

- All 24 portable suites passed in _build/native-region-section-tests.log before
  the native death changes.
- Native checkpoint gate failed in
  _build/native-checkpoint-diagnostics-acceptance.log; capture is unverified.
- Death source compiled in _build/death-ragdoll-orbit-final-build.log.
- Final local corpse rendering build is tracked in
  _build/death-body-render-final-build.log. The final source also uses the
  default native in_Die hit path, so recent lethal hits activate the shell;
  rebuild this last one-line adjustment before running death acceptance.
- No death acceptance run was launched before the requested break.

## Resumed work

The first death fixture failed because native-world guests skipped host death
vitals. Native guests now apply the host's lethal transition while retaining
their locally simulated nonlethal condition. Revival clears saved corpse physics
before the normal native spawn path recreates the movement capsule and IK.
Acceptance explicitly requires a living capsule after revival.

Death gameplay acceptance passed in _build/death-authority-acceptance.log,
fixture _build/death-orbit-4852c7a9fb8d4d06ab18bfab872fbde8. Both actors produced
11 native ragdoll elements, stable death anchors under orbit input, host-approved
revival, retained ammunition and all-dead denial. Screenshot inspection caught
an orbit elevation sign error and unrecognized instruction line breaks; both
were corrected. The corrected visual build passed compilation in
_build/death-camera-visual-build.log. Its isolated acceptance run is tracked in
_build/death-camera-visual-acceptance.log, fixture
_build/death-orbit-5c25cbd0d30044d5ad9b093608618c6e. Inspected host and guest
screenshots show native corpses, cameras above the floor and separated menu
instructions. The final runner returned DEATH_ORBIT_PASS. These checks do not
establish complete Internet movement parity.

All 24 portable suites passed in _build/resumed-portable-tests.log. Native
checkpoint capture now logs every guard and failure exit. Next run the main
native world acceptance with these diagnostics, repair capture, then implement
offline capture, validated restore, lease integration and independent map
ownership. The native-world harness also guards the process shutdown race.

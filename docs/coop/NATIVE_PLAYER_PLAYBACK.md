# Native player playback (protocol 37)

## Runtime behavior

The owner runs native controls, character physics, equipment and weapon feedback.
Position, velocity, facing and discrete movement state travel through the pose
channel. The host retains shared AI and combat authority; it does not integrate
the owner's character movement a second time.

Remote player visuals now load X-Ray's `SActorMotions` tables and evaluate
movement using `isActorAccelerated`, the stock crouch/walk/run/sprint variants,
and the received equipment animation slot, zoom and safe mode. Ordinary cycles
are classified against the same stock tables and excluded from appearance
transmission. Local gait changes retain normalized leg phase; starting movement
uses the stock half-cycle starting phase. Native skeleton tracks advance locally.

Nonordinary cycles, such as weapon draw/reload and scripted actions, remain
bounded explicit playback records per partition. They override only their own
partition and cannot run gameplay callbacks. Finishing an explicit action
restores the affected native state-driven partition. Motion assets must match.

Remote position playback can bridge at most 100 ms of missing poses using
reported velocity. Spawn and moves over five metres are not extrapolated;
relocations are discrete rather than interpolated through geometry. An outage
longer than that horizon holds the bounded predicted endpoint.

ALife online/offline hysteresis now uses the nearest living, admitted native
guest as well as the host, restricted to the object's loaded graph level. The
stock thresholds and online eligibility checks remain in use for individual
objects and both native group paths. Other levels retain stock behavior.

## Test tools

`test-coopnet-saved-equipment.ps1 -HuntingFollower -SimulatedWan -GuestRelevance`
uses isolated appdata and engine fixtures. The WAN profile adds outbound 60 ms
delay, exponential jitter (20 ms mean, 40 ms maximum), and two percent random
packet loss in each process. It affects actual UDP delivery, including reliable
traffic. Ordinary sessions never activate this profile.

The fixture draws existing saved weapons, recruits Fanatic through the stock
hunting dialogue, drives native walking, then releases the control override. It
checks a native object remains online near the guest after the host moves beyond
the reduced stock switching radius. Relevance timing begins when the guest
actually reaches the hillside, rather than assuming both loads finish together.
Diagnostic render poses are logged each update and screenshots use unique frame
names, allowing sequences to be inspected without timestamp overwrites.

Portable tests cover missing-packet prediction bounds, spawning, recovery and
relocation, plus a 20 Hz trajectory rendered at 60 Hz under deterministic burst
jitter, reordering and loss. The measured maximum step was 0.2001 m and maximum
trajectory error 0.133432 m. This describes the defined trajectory, not every
possible player action or internet connection.

Real-process UDP admission, roster, owner input, acknowledged snapshots and
disconnect passed the WAN profile (`_build/wan-transport-final.log`). All 21
portable suites passed (`_build/native-animation-impaired-unit.log`). Native
paired gameplay evidence is recorded after the completed run below.

The corrected 180-second paired run passed
(`_build/native-animation-wan-paired-final.log`, fixture
`saved-equipment-37c0a7bf57f5412091d5946e0748e0ec`). Knife, PM and BM16 reached
native idle after draw without manual equipment moves. Native Fanatic following
and guest-aware ALife relevance passed. Host screenshots at frames 1328, 1415,
1483 and 1523 were inspected: the guest holds the firearm, changes leg/torso
poses, and moves on the clear hillside. Knife idle had zero explicit cycles,
confirming ordinary poses were evaluated locally; firearm action captured one.

Render-pose analysis counted 3,846 host updates, including 148 moving intervals.
Those intervals had no frozen position stretches; the maximum frame displacement
was 0.182158 m. The explicit initial hillside placement was excluded as a 90 m
relocation. The guest's view of the stationary host logged 3,779 updates and
no ordinary moving intervals; fixture host teleports were recorded separately.
This measured five seconds of automated movement, not an unrestricted WAN
playthrough or a guarantee at every latency/frame rate.

The earlier run failed the relevance assertion after the test's walking override
remained active when automatic updates stopped. The guest left the tested area.
`coop_movement_probe` now releases that override when switched off. Relevance
timing also waits for actual guest arrival. That failed fixture was not counted
as a passing movement/relevance result.

## Scope

This change concerns native player presentation and guest proximity simulation.
Shared NPC AI still runs on the host. NPC speech, action sounds, arbitrary
script/mod behavior and the full story catalogue require their own event and
gameplay coverage; state-driven player animation does not establish those paths.

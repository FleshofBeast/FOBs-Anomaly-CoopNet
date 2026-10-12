param([switch]$FreshGuest,[switch]$Acceptance,[switch]$NaturalCombat,[string]$TestDirectory,[switch]$Resume,[switch]$NativeDefault,[switch]$VisualCapture)
$ErrorActionPreference='Stop'
$repo=$PSScriptRoot
Set-Location $repo
$root=Join-Path (Join-Path $repo '_build') ('native-world-'+[Guid]::NewGuid().ToString('N'))
if ($TestDirectory) { $root=[IO.Path]::GetFullPath($TestDirectory) }
$source=Join-Path (Split-Path $repo) 'Anomaly-1.5.3/appdata/savedgames'
$hashes=@{};Get-ChildItem $source -Filter 'player - autosave.*' -File | ForEach-Object {$hashes[$_.FullName]=(Get-FileHash $_.FullName).Hash}
$owned=@()
try {
 $owned=@(& "$repo/prepare-coopnet-engine-test.ps1" -Launch -LoadFixture -ReplicaProbe -MovementProbe -WorldProbe -WeaponProbe -SharedWorldProbe -NativeWorldProbe -NativeDefaultProbe:$NativeDefault -NativeAcceptanceProbe:$Acceptance -NativeVisualProbe:$VisualCapture -NaturalCombatProbe:$NaturalCombat -SharedHitProbe -FreshQuestProbe:$FreshGuest -TestDirectory $root)
 if($owned.Count -ne 2){throw 'Expected exactly two fixture processes'}
 $watch=[Diagnostics.Stopwatch]::StartNew()
 while($watch.Elapsed.TotalSeconds -lt 150){foreach($process in $owned){if($process.HasExited){throw "Fixture exited early: $($process.Id) code $($process.ExitCode)"}};Start-Sleep -Milliseconds 500}
} finally {
 $cleanupErrors=@()
 foreach($process in $owned){if(!$process.HasExited){$process.CloseMainWindow()|Out-Null;if(!$process.WaitForExit(30000)){$process.Refresh();if(!$process.HasExited){Stop-Process -Id $process.Id -ErrorAction SilentlyContinue;$cleanupErrors+='Owned fixture did not close normally'}}}}
 if($cleanupErrors.Count){Write-Warning ($cleanupErrors -join '; ')}
 foreach($path in $hashes.Keys){if((Get-FileHash $path).Hash -ne $hashes[$path]){throw 'Private source save was changed'}}
}
$logs=@{};foreach($role in @('host','guest')){$file=Get-ChildItem "$root/$role/appdata/logs" -Filter '*.log'|Sort-Object LastWriteTimeUtc -Descending|Select-Object -First 1;$logs[$role]=Get-Content $file.FullName -Raw;if($logs[$role] -match 'FATAL ERROR|! CoopNet update failed:|UnhandledFilter|BusyHandsDebug|SCRIPT ERROR|critical Lua error|LUA.*(?:RUNTIME ERROR|error:)'){throw "$role runtime failed"};if($logs[$role] -notmatch 'CoopNet session stopped'){throw "$role shutdown missing"}}
if($logs.guest -notmatch 'CoopNet client state: connected' -or $logs.guest -notmatch 'CoopNet native stalker frame completed: object'){throw 'Connected guest native NPC frame completion missing'}
foreach($marker in @('host NPC and quests created','host NPC killed and quests completed/failed','host corpse removed and info withdrawn')){if(!$logs.host.Contains($marker)){throw "Host shared-world fixture incomplete: $marker"}}
foreach($marker in @('CoopNet NPC spawned: section dog_weak','CoopNet NPC death applied:','CoopNet NPC removed:')){if(!$logs.guest.Contains($marker)){throw "Guest shared-world lifecycle missing: $marker"}}
$dogMatch=[regex]::Match($logs.guest,'CoopNet NPC spawned: section dog_weak anchor (\d+)')
if(!$dogMatch.Success){throw 'Shared enemy identity missing'}
$dogAnchor=$dogMatch.Groups[1].Value
if(!$logs.guest.Contains("CoopNet NPC death applied: anchor $dogAnchor") -or !$logs.guest.Contains("CoopNet NPC removed: anchor $dogAnchor section dog_weak")){throw 'Shared enemy death/removal did not match its spawned identity'}
if($logs.guest -notmatch 'CoopNet NPC spawned:[^\r\n]*trader 1 visible 1'){throw 'Native trader recreation missing'}
if($logs.guest.Contains('CoopNet quests applied:')){throw 'Personal-quest mode applied a host quest snapshot'}
$questMarker=if($FreshGuest){'CoopNet personal quest probe: fresh guest has no inherited host tasks'}else{'CoopNet personal quest probe: guest-only task and info restored'}
if(!$logs.guest.Contains($questMarker)){throw 'Personal task/info restoration missing'}
if(!$FreshGuest -and !$logs.guest.Contains('CoopNet personal Lua quest variable restored')){throw 'Personal Lua quest variable restoration missing'}
foreach($role in @('host','guest')){if(!$logs[$role].Contains('CoopNet personal quest follower hooks installed')){throw "$role private follower hooks missing"}}
if ($NaturalCombat) { if (!$logs.guest.Contains("CoopNet natural bullet hit queued: target $dogAnchor")) {throw 'Natural guest bullet collision missing'} } elseif (!$logs.guest.Contains("CoopNet shared hit probe: native guest hit queued without local damage target $dogAnchor")){throw 'Native guest hit interception missing'}
if($Resume -and $NaturalCombat -and $logs.guest -notmatch 'CoopNet personal loadout restored: items \d+ active wpn_pm'){throw 'Saved personal weapon was not re-equipped and drawn before native combat'}
if($Acceptance -and $NaturalCombat -and !$logs.guest.Contains('CoopNet saved loadout fixture prepared: active wpn_pm')){throw 'Saved active weapon fixture was not prepared after native damage'}
$appliedHits=[regex]::Matches($logs.host,"CoopNet shared guest hit applied: target $dogAnchor sequence \d+ health ([\d.-]+) -> ([\d.-]+)")
if($appliedHits.Count -ne 1 -or [double]$appliedHits[0].Groups[2].Value -gt 0){throw 'Guest hit did not kill the shared enemy exactly once on the host'}
if ($Acceptance) {
 if($logs.host -notmatch 'CoopNet native checkpoint capture verified: level \d+ objects [1-9]\d* children \d+ bytes [1-9]\d*'){throw 'Live native region checkpoint round-trip missing'}
 if($logs.host -notmatch 'CoopNet separate-map offline checkpoint verified: host level \d+ captured level \d+ objects [1-9]\d* bytes [1-9]\d* stable ALife lifetimes'){throw 'Separate-map offline checkpoint capture missing'}
 if($logs.host -notmatch 'CoopNet retained offline restore verified: level \d+ objects [1-9]\d* inventory \d+ stale parent and lifetime denied; registry aliases retained'){throw 'Native retained offline restore and rejection checks missing'}
 if($logs.host -notmatch 'CoopNet host offline bootstrap streamed: player \d+ level \d+ bytes [1-9]\d*'){throw 'Host offline native bootstrap streaming missing'}
 if($logs.host -notmatch 'CoopNet host offline bootstrap restoration acknowledged: player \d+ level \d+'){throw 'Host-confirmed native restoration barrier missing'}
 if($logs.guest -notmatch 'CoopNet guest host-issued offline bootstrap restored: level \d+ objects [1-9]\d* bytes [1-9]\d* registry aliases retained'){throw 'Guest native offline registry bootstrap restoration missing'}
 foreach($marker in @('CoopNet dynamic anomaly probe: host spawned native','CoopNet dynamic anomaly probe: host removed native')) {if(!$logs.host.Contains($marker)){throw "Host dynamic anomaly check missing: $marker"}}
 if(!$logs.guest.Contains('CoopNet dynamic anomaly probe: guest native removal verified')){throw 'Guest dynamic anomaly removal missing'}
 if(!$logs.guest.Contains('CoopNet dynamic anomaly probe: retained lifetime recovered')){throw 'Guest native lifetime recovery missing'}
 $lootMarker=if($Resume){'CoopNet personal loot probe: portable contents restored with exact condition'}else{'CoopNet personal loot probe: native transfer captured'}
 foreach($marker in @($lootMarker,'CoopNet native electric collision:','CoopNet native electric probe: local damage observed','CoopNet native acceptance probes completed','CoopNet native player hit applied:')) {if(!$logs.guest.Contains($marker)){throw "Guest native acceptance check missing: $marker"}}
 if($Resume -and !$logs.guest.Contains('CoopNet portable personal state queued:')){throw 'Portable personal quests were not loaded'}
 if(!(Get-ChildItem "$root/guest/appdata/savedgames" -Filter '*.coopchar')){throw 'Guest portable character was not saved'}
}
if($cleanupErrors.Count){throw ($cleanupErrors -join '; ')}
$checks='native NPC frames, shared hit/death/removal, personal quest restoration'
if($Acceptance){$checks+=', personal loot transfer/contents, electrical collision/damage, owner-side player hit, portable character save'}
if($Resume){$checks+=', portable restart restoration'}
if($NaturalCombat){$checks+=', native aimed bullet collision'}
Write-Output "NATIVE_WORLD_PROTOTYPE_PASS: $root; verified $checks; both clients closed, source saves unchanged."



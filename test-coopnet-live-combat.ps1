param([int]$Seconds=120)
$ErrorActionPreference='Stop'
$root=Join-Path "$PSScriptRoot/_build" ('live-combat-'+[Guid]::NewGuid().ToString('N'))
$owned=@()
try {
    $owned=@(& "$PSScriptRoot/prepare-coopnet-engine-test.ps1" -Launch -LiveCombatProbe -TestDirectory $root)
    if($owned.Count -ne 2){throw 'Expected two clients'}
    $watch=[Diagnostics.Stopwatch]::StartNew()
    while($watch.Elapsed.TotalSeconds -lt $Seconds){
        foreach($process in $owned){if($process.HasExited){throw "Client exited early: $($process.Id) code $($process.ExitCode)"}}
        Start-Sleep -Milliseconds 500
    }
} finally {
    foreach($process in $owned){
        if(!$process.HasExited){$process.CloseMainWindow()|Out-Null;if(!$process.WaitForExit(30000)){Stop-Process -Id $process.Id -Force}}
    }
    Write-Output "LIVE_COMBAT_REVIEW: $root (rendered frames require inspection; no visual pass inferred)"
}
$logs=@{}
foreach($role in @('host','guest')) {
    $files=@(Get-ChildItem "$root/$role/appdata/logs" -Filter '*.log' -File)
    $logs[$role]=($files|ForEach-Object {[IO.File]::ReadAllText($_.FullName)}) -join "`n"
    if($logs[$role].Contains('CoopNet update failed:')){throw "Live fight failed on $role; inspect its log at $root"}
    if(!(Get-ChildItem "$root/$role/appdata/screenshots" -File -ErrorAction SilentlyContinue)){throw "No rendered $role frames"}
}
if(!$logs.host.Contains('CoopNet shared probe: host NPC and quests created')){throw 'No enemy spawned'}
if(!$logs.host.Contains('CoopNet shared probe: host NPC killed')){throw 'Enemy did not die through native combat'}
if(!$logs.host.Contains('CoopNet live native bullet hit: actor local')){throw 'Host bullets did not collide with the enemy'}
if(!$logs.guest.Contains('CoopNet natural bullet hit queued:')){throw 'Guest bullets did not collide with the enemy'}
if(!$logs.host.Contains('CoopNet shared guest hit applied:')){throw 'Guest bullet damage did not reach the host'}
$targetAnchor=[regex]::Match($logs.host,'CoopNet shared guest hit applied: target (\d+)').Groups[1].Value
if(!$logs.guest.Contains("CoopNet native ragdoll bodies applied: anchor $targetAnchor bodies ")){throw 'Guest did not apply the fought enemy''s authoritative native ragdoll bodies'}
foreach($role in @('host','guest')) {
    if(!$logs[$role].Contains('CoopNet native local weapon input applied: fire 1')){throw "No native trigger input on $role"}
}
Write-Output 'LIVE_COMBAT_STIMULUS_COMPLETE: native AI and firing exercised; visual synchronization remains a human review.'

param([int]$Seconds=180)
$ErrorActionPreference='Stop'
Set-Location $PSScriptRoot
$fixtureRoot=Join-Path $PSScriptRoot ('_build/death-orbit-'+[Guid]::NewGuid().ToString('N'))
$sourceSaves=Join-Path (Split-Path $PSScriptRoot) 'Anomaly-1.5.3/appdata/savedgames'
$hashes=@{}
Get-ChildItem $sourceSaves -Filter 'player - autosave.*' -File | ForEach-Object {$hashes[$_.FullName]=(Get-FileHash $_.FullName).Hash}
$owned=@()
try {
    $owned=@(& "$PSScriptRoot/prepare-coopnet-engine-test.ps1" -Launch -LoadFixture -ReplicaProbe -MovementProbe -GameplayProbe -WorldProbe -WeaponProbe -RespawnProbe -NativeWorldProbe -NativeDefaultProbe -NativeVisualProbe -TestDirectory $fixtureRoot)
    if($owned.Count -ne 2){throw 'Expected exactly two owned fixture processes'}
    $watch=[Diagnostics.Stopwatch]::StartNew()
    while($watch.Elapsed.TotalSeconds -lt $Seconds) {
        foreach($process in $owned){if($process.HasExited){throw "Death fixture exited early: $($process.Id) code $($process.ExitCode)"}}
        Start-Sleep -Milliseconds 500
    }
} finally {
    foreach($process in $owned) {
        if(!$process.HasExited) {
            $process.CloseMainWindow() | Out-Null
            if(!$process.WaitForExit(30000)) {
                $process.Refresh()
                if(!$process.HasExited){Stop-Process -Id $process.Id -ErrorAction SilentlyContinue;throw 'Owned death fixture did not close normally'}
            }
        }
    }
    foreach($path in $hashes.Keys){if((Get-FileHash $path).Hash -ne $hashes[$path]){throw 'Private source save changed'}}
}
$logs=@{}
foreach($role in @('host','guest')) {
    $file=Get-ChildItem "$fixtureRoot/$role/appdata/logs" -Filter '*.log' | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
    $logs[$role]=Get-Content $file.FullName -Raw
    if($logs[$role] -match 'FATAL ERROR|UnhandledFilter|! CoopNet update failed:'){throw "$role death runtime failed"}
    foreach($marker in @('CoopNet death orbit verified:','CoopNet player respawned:','CoopNet Respawn dialog opened','CoopNet session stopped')) {
        if(!$logs[$role].Contains($marker)){throw "$role missing: $marker"}
    }
    if(!(Get-ChildItem "$fixtureRoot/$role/appdata/screenshots" -File -ErrorAction SilentlyContinue)){throw "$role death screenshots missing"}
}
foreach($marker in @('guest equipment retained; rounds 2','host respawned at guest','all dead; respawn disabled and host denied request')) {
    if(!$logs.host.Contains($marker)){throw "Host respawn acceptance missing: $marker"}
}
if(!$logs.guest.Contains('guest living after host approval')){throw 'Guest did not recover after host approval'}
Write-Output "DEATH_ORBIT_PASS: $fixtureRoot; host/guest native ragdoll, fixed death anchor, orbit input, menu, revival, retained ammunition and all-dead guard; source saves unchanged."

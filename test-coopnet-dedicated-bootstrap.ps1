param([ValidateRange(60,180)][int]$Seconds=100,[ValidateRange(1024,65535)][int]$Port=27893,[switch]$Simulation,[string]$FixtureSave='player - autosave',[switch]$Peaceful)
$ErrorActionPreference='Stop'
$root=Join-Path $PSScriptRoot ('_build/dedicated-native-'+[guid]::NewGuid().ToString('N'))
$source=Join-Path (Split-Path $PSScriptRoot) 'Anomaly-1.5.3/appdata/savedgames'
$seed=Join-Path $source "$FixtureSave.scop"
if(!(Test-Path -LiteralPath $seed)){throw "Fixture save missing: $seed"}
$server=Join-Path $PSScriptRoot '_build/coopnet-server/Release/CoopNetServer.exe'
if(!(Test-Path $server)){throw 'Build the dedicated server first'}
$hashes=@{};Get-ChildItem -LiteralPath $source -Filter "$FixtureSave.*" -File | ForEach-Object {$hashes[$_.FullName]=(Get-FileHash -LiteralPath $_.FullName).Hash}
& "$PSScriptRoot/prepare-coopnet-engine-test.ps1" -LoadFixture -ReplicaProbe -NativeWorldProbe -NativeDefaultProbe -PerformanceProbe:$Simulation -TestDirectory $root -FixtureSave $FixtureSave
foreach($role in @('host','guest')) {
    $config=Join-Path $root "$role/appdata/user.ltx"
    $lines=@(Get-Content -LiteralPath $config | Where-Object {$_ -notmatch '^coop_'})
    @("coop_character_probe 127.0.0.1:$Port")+$lines | Set-Content -LiteralPath $config -Encoding ascii
    if($Simulation){Add-Content -LiteralPath $config -Value 'rs_stats on' -Encoding ascii}
}
$data=Join-Path $root 'server-data';$owned=@();$samples=[Collections.Generic.List[object]]::new()
try {
    $serverArguments=@('--experimental-bootstrap','1','--data',('"'+$data+'"'),'--world',('"'+$seed+'"'),'--level','3','--port',"$Port",'--run-seconds',"$($Seconds+30)")
    if($Simulation){$serverArguments+=@('--experimental-simulation','1')}
    $owned+=Start-Process -FilePath $server -ArgumentList $serverArguments -WindowStyle Hidden -PassThru -RedirectStandardOutput "$root/server.log" -RedirectStandardError "$root/server-error.log"
    $deadline=[datetime]::UtcNow.AddSeconds(10)
    while(!(Select-String -LiteralPath "$root/server.log" -Pattern '^EXPERIMENTAL_BOOTSTRAP_LISTENING' -Quiet)) {
        if($owned[0].HasExited -or [datetime]::UtcNow -gt $deadline){throw 'Standalone server startup failed'}
        Start-Sleep -Milliseconds 50
    }
    foreach($role in @('host','guest')) {
        $clientRoot=Join-Path $root $role
        $clientArguments=@('-silent_error_mode','-noprefetch','-coop_engine_fixture','-coop_native_visual_probe')
        if($Simulation){$clientArguments+='-coop_performance_probe'}
        # Test environment: neutral human factions and an arrival away from hostile squads.
        if($Peaceful){$clientArguments+='-coop_peaceful_probe'}
        $owned+=Start-Process -FilePath "$clientRoot/bin/AnomalyDX11.exe" -WorkingDirectory $clientRoot -ArgumentList $clientArguments -WindowStyle Hidden -PassThru
    }
    $watch=[Diagnostics.Stopwatch]::StartNew()
    while($watch.Elapsed.TotalSeconds -lt $Seconds) {
        for($i=0;$i -lt $owned.Count;$i++) {
            $process=$owned[$i];if($process.HasExited){throw "Owned fixture process exited early: $i"};$process.Refresh()
            $samples.Add([pscustomobject]@{Seconds=$watch.Elapsed.TotalSeconds;Role=@('server','client1','client2')[$i];WorkingBytes=$process.WorkingSet64;PrivateBytes=$process.PrivateMemorySize64;CpuSeconds=$process.TotalProcessorTime.TotalSeconds})
        }
        Start-Sleep -Milliseconds 500
    }
} finally {
    $cleanupErrors=@()
    foreach($process in @($owned | Select-Object -Skip 1)) {
        if(!$process.HasExited){$process.CloseMainWindow()|Out-Null;if(!$process.WaitForExit(15000)){Stop-Process -Id $process.Id;$cleanupErrors+='Native fixture required forced close'}}
    }
    if($owned.Count -gt 0 -and !$owned[0].HasExited) {
        if(!$owned[0].WaitForExit(40000)){Stop-Process -Id $owned[0].Id;$cleanupErrors+='Server did not finish its bounded run'}
    }
    $samples | Export-Csv -LiteralPath "$root/process-samples.csv" -NoTypeInformation
    foreach($path in $hashes.Keys){if((Get-FileHash -LiteralPath $path).Hash -ne $hashes[$path]){throw 'Original source save changed'}}
    if($cleanupErrors.Count){throw ($cleanupErrors -join '; ')}
}
$serverText=Get-Content -LiteralPath "$root/server.log" -Raw
if($serverText -notmatch 'ADMITTED_CLIENTS 2' -or $serverText -notmatch 'SERVER_STOPPED' -or (Get-Content -LiteralPath "$root/server-error.log" -Raw)){throw 'Dedicated server failed to admit both native clients cleanly'}
if($Simulation -and $serverText -notmatch 'SIMULATION_ACCEPTED catalogues [1-9][0-9]* states [1-9][0-9]*'){throw "Native worker publications were not accepted: $root"}
$checkpoints=@(Get-ChildItem -LiteralPath "$data/characters" -Filter '*.coopchar')
if($checkpoints.Count -ne 2){throw 'Two distinct native character checkpoints were not created'}
if($Simulation) {
    $summary=foreach($role in @('server','client1','client2')) {
        $rows=@($samples | Where-Object Role -eq $role)
        $steady=@($rows | Where-Object Seconds -ge ($Seconds-30))
        if($steady.Count -lt 2){throw "Insufficient steady performance samples: $role"}
        $cpu=($steady[-1].CpuSeconds-$steady[0].CpuSeconds)/($steady[-1].Seconds-$steady[0].Seconds)
        $resident=($rows | Measure-Object WorkingBytes -Maximum).Maximum/1MB
        $passed=if($role -eq 'server'){$resident -lt 512 -and $cpu -lt .5}else{$resident -lt 4096 -and $cpu -lt 3}
        [pscustomobject]@{Role=$role;PeakResidentMiB=[math]::Round($resident,1);AverageCpuCores=[math]::Round($cpu,3);Passed=$passed}
    }
    $summary | ConvertTo-Json | Set-Content -LiteralPath "$root/performance-summary.json"
    if(@($summary | Where-Object Passed -eq $false).Count){throw "Process performance gate failed: $root"}
}
foreach($role in @('host','guest')) {
    $file=Get-ChildItem "$root/$role/appdata/logs" -Filter '*.log' | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
    $log=Get-Content -LiteralPath $file.FullName -Raw
    if($log -match 'FATAL ERROR|! CoopNet update failed:|UnhandledFilter|BusyHandsDebug|SCRIPT ERROR|critical Lua error|LUA.*(?:RUNTIME ERROR|error:)'){throw "$role native runtime failed: $root"}
    if($Peaceful -and $log.Contains('CoopNet player downed')){throw "$role was downed in the peaceful test environment: $root"}
    foreach($marker in @('canonical baseline loaded and acknowledged','guest arrival placed:','guest inventory view applied:','CoopNet session stopped')) {
        if(!$log.Contains($marker)){throw "$role missing $marker -- $root"}
    }
    if($Simulation) {
        if($log.Contains('native worker execution fenced after connection loss')){throw "$role lost simulation authority during gameplay: $root"}
        $performance=[regex]::Matches($log,'CoopNet gameplay performance: level (\d+) frames (\d+) fps ([\d.]+) p95_ms ([\d.]+) max_ms ([\d.]+) result (PASS|FAIL)')
        if(!$performance.Count){throw "$role native frame measurements missing: $root"}
        foreach($measurement in $performance) {
            if([int]$measurement.Groups[2].Value -lt 300 -or [double]$measurement.Groups[3].Value -lt 45 -or [double]$measurement.Groups[4].Value -ge 40){throw "$role native frame performance failed: $($measurement.Value) -- $root"}
        }
    }
}
$archive=Join-Path $data 'world.bootstrap';$archiveHash=(Get-FileHash -LiteralPath $archive).Hash
$restart=Start-Process -FilePath $server -ArgumentList '--experimental-bootstrap','1','--data',('"'+$data+'"'),'--port',"$Port",'--run-seconds','1' -WindowStyle Hidden -PassThru -RedirectStandardOutput "$root/restart.log" -RedirectStandardError "$root/restart-error.log"
# Reading Handle before exit keeps ExitCode; with redirected output it is otherwise null.
$null=$restart.Handle
try {if(!$restart.WaitForExit(10000)){Stop-Process -Id $restart.Id;throw 'Restart timed out'};if($restart.ExitCode -ne 0){throw 'Server world archive restart failed'}} finally {$restart.Dispose()}
if((Get-FileHash -LiteralPath $archive).Hash -ne $archiveHash){throw 'Restart replaced canonical bootstrap unexpectedly'}
Write-Output "PASS: two native clients loaded the standalone server baseline and restored characters; server archive restarted. Evidence: $root"
Write-Output 'Bootstrap acceptance only: native shared AI/combat, cross-map simulation, ongoing persistence and visual parity remain unverified.'

$ErrorActionPreference='Stop'
$testRoot=Join-Path $PSScriptRoot ('_build/join-failure-'+[Guid]::NewGuid().ToString('N'))
& (Join-Path $PSScriptRoot 'prepare-coopnet-engine-test.ps1') -LoadFixture -ReplicaProbe -TestDirectory $testRoot
$root=Join-Path $testRoot 'guest'
$client=Join-Path (Split-Path $PSScriptRoot) 'Anomaly-1.5.3'
Copy-Item "$client/gamedata" $root -Recurse
Copy-Item "$PSScriptRoot/gamedata/scripts/*.script" "$root/gamedata/scripts" -Force
$optionsPath=Join-Path $root 'gamedata/configs/axr_options.ltx'
$options=Get-Content $optionsPath -Raw
foreach($entry in @{new_game_faction='stalker';new_game_money='1234';new_game_loadout='wpn_pm,novice_outfit,bandage';new_game_map='factory_complex';new_game_character_name='FailureGuest'}.GetEnumerator()) {
    $options=[regex]::Replace($options,'(?m)^\s*'+$entry.Key+'\s*=.*$',('        '+$entry.Key+' = '+$entry.Value))
}
Set-Content $optionsPath $options -Encoding ascii
New-Item "$root/db" -ItemType Junction -Target "$client/db" | Out-Null
$fs=Get-Content "$root/fsgame.ltx" | Where-Object {$_ -notmatch '^\$fs_root\$'}
@('$fs_root$ = false | false | '+$root+'\')+@($fs) | Set-Content "$root/fsgame.ltx" -Encoding ascii
$config=Join-Path $root 'appdata/user.ltx'
$lines=Get-Content $config | Where-Object {$_ -notmatch '^(coop_|start )'}
@('coop_character_probe 127.0.0.1:27999')+@($lines)+@('start server(all/single/alife/new) client(localhost)') | Set-Content $config -Encoding ascii
$owned=Start-Process "$root/bin/AnomalyDX11.exe" -WorkingDirectory $root -ArgumentList '-silent_error_mode','-noprefetch','-coop_join_failure_probe','-coop_new_character_probe' -WindowStyle Hidden -PassThru
try {
    $deadline=(Get-Date).AddSeconds(150)
    $passed=$false
    while((Get-Date) -lt $deadline) {
        if($owned.HasExited){throw 'Native failure test exited early'}
        # Native log output is buffered; the screenshot is written immediately.
        $capture=Get-ChildItem "$root/appdata/screenshots" -File -ErrorAction SilentlyContinue | Select-Object -First 1
        if($capture){$passed=$true; break}
        Start-Sleep -Seconds 2
    }
    if(!$passed){throw 'Native connection-failure evidence absent'}
} finally {
    if(!$owned.HasExited){$owned.CloseMainWindow() | Out-Null; if(!$owned.WaitForExit(10000)){Stop-Process -Id $owned.Id}}
}
$log=Get-ChildItem "$root/appdata/logs" -Filter '*.log' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
$output=Get-Content $log.FullName -Raw
if($output -notmatch 'selected character initialization completed' -or $output -notmatch 'initial join failed' -or $output -notmatch 'connection failure popup displayed' -or $output -match 'FATAL ERROR|\[SCRIPT ERROR\]|host accepted connection'){throw 'Native failure assertions failed'}
Write-Output "PASS native queued menu join, bounded unreachable-host failure and visible popup: $root"


$ErrorActionPreference='Stop'
$testRoot=Join-Path $PSScriptRoot ('_build/menu-host-'+[Guid]::NewGuid().ToString('N'))
& "$PSScriptRoot/prepare-coopnet-engine-test.ps1" -LoadFixture -ReplicaProbe -TestDirectory $testRoot
$client=Join-Path (Split-Path $PSScriptRoot) 'Anomaly-1.5.3'
foreach($role in @('host','guest')) {
    $root=Join-Path $testRoot $role
    Copy-Item "$client/gamedata" $root -Recurse
    Copy-Item "$PSScriptRoot/gamedata/scripts/*.script" "$root/gamedata/scripts" -Force
    New-Item "$root/db" -ItemType Junction -Target "$client/db" | Out-Null
    $fs=Get-Content "$root/fsgame.ltx" | Where-Object {$_ -notmatch '^\$fs_root\$'}
    @('$fs_root$ = false | false | '+$root+'\')+@($fs) | Set-Content "$root/fsgame.ltx" -Encoding ascii
    $config=Join-Path $root 'appdata/user.ltx'
    $lines=Get-Content $config | Where-Object {$_ -notmatch '^(coop_|start )'}
    @('coop_character_probe 127.0.0.1:27888')+@($lines)+@('start server(coopnet-fixture/single/alife/load) client(localhost)') | Set-Content $config -Encoding ascii
}
$owned=@()
try {
    foreach($role in @('host','guest')) {
        $root=Join-Path $testRoot $role
        $arguments=@('-silent_error_mode','-noprefetch')
        if($role -eq 'host'){$arguments+='-coop_host_menu_probe'}
        $owned+=Start-Process "$root/bin/AnomalyDX11.exe" -WorkingDirectory $root -ArgumentList $arguments -WindowStyle Hidden -PassThru
    }
    $deadline=(Get-Date).AddSeconds(120)
    while((Get-Date) -lt $deadline){foreach($process in $owned){if($process.HasExited){throw 'Native host-menu client exited early'}}; Start-Sleep -Seconds 2}
} finally {
    foreach($process in $owned){if(!$process.HasExited){$process.CloseMainWindow() | Out-Null; if(!$process.WaitForExit(10000)){Stop-Process -Id $process.Id}}}
}
$hostLog=Get-Content "$testRoot/host/appdata/logs/xray_deadparrot.log" -Raw
$guestLog=Get-Content "$testRoot/guest/appdata/logs/xray_deadparrot.log" -Raw
if($hostLog -notmatch 'selected character initialization completed' -or $hostLog -notmatch 'host ready participants: 2' -or $guestLog -notmatch 'canonical baseline loaded and acknowledged' -or ($hostLog+$guestLog) -match 'FATAL ERROR|\[SCRIPT ERROR\]|CoopNet update failed|initial join failed'){throw "Menu-host admission/world loading failed: $testRoot"}
Write-Output "PASS menu host queue after save loading, UDP 27888 admission and guest world acknowledgement: $testRoot"

param([Parameter(Mandatory)][string]$TestDirectory,[ValidateRange(60,180)][int]$Seconds=80,[ValidateRange(1024,65535)][int]$Port=27893)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath($TestDirectory)
$allowed=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '_build'))+[IO.Path]::DirectorySeparatorChar
if(!$root.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)){throw 'Restart fixture must be in repository _build'}
$data=Join-Path $root 'server-data';$server=Join-Path $PSScriptRoot '_build/coopnet-server/Release/CoopNetServer.exe'
$checkpointHashes=@{};Get-ChildItem -LiteralPath "$data/characters" -Filter '*.coopchar' | ForEach-Object {$checkpointHashes[$_.FullName]=(Get-FileHash -LiteralPath $_.FullName).Hash}
if($checkpointHashes.Count -ne 2){throw 'Run the native dedicated bootstrap acceptance first'}
$archiveHash=(Get-FileHash -LiteralPath "$data/world.bootstrap").Hash
foreach($role in @('host','guest')) {
    $config=Join-Path $root "$role/appdata/user.ltx"
    $lines=@(Get-Content -LiteralPath $config | Where-Object {$_ -notmatch '^(coop_|start )'})
    @("coop_character_probe 127.0.0.1:$Port")+$lines+@('start server(coopnet-fixture/single/alife/load) client(localhost)') |
        Set-Content -LiteralPath $config -Encoding ascii
}
$owned=@()
try {
    $owned+=Start-Process -FilePath $server -ArgumentList '--experimental-bootstrap','1','--data',('"'+$data+'"'),'--port',"$Port",'--run-seconds',"$($Seconds+30)" -WindowStyle Hidden -PassThru -RedirectStandardOutput "$root/server-resume.log" -RedirectStandardError "$root/server-resume-error.log"
    $readyDeadline=[datetime]::UtcNow.AddSeconds(10)
    while(!(Select-String -LiteralPath "$root/server-resume.log" -Pattern '^EXPERIMENTAL_BOOTSTRAP_LISTENING' -Quiet)) {
        if($owned[0].HasExited -or [datetime]::UtcNow -gt $readyDeadline){throw 'Server restart failed'}
        Start-Sleep -Milliseconds 50
    }
    foreach($role in @('host','guest')) {
        $clientRoot=Join-Path $root $role
        $owned+=Start-Process -FilePath "$clientRoot/bin/AnomalyDX11.exe" -WorkingDirectory $clientRoot -ArgumentList '-silent_error_mode','-noprefetch','-coop_engine_fixture' -WindowStyle Hidden -PassThru
    }
    $deadline=[datetime]::UtcNow.AddSeconds($Seconds)
    while([datetime]::UtcNow -lt $deadline) {
        foreach($process in $owned){if($process.HasExited){throw 'A restart fixture process exited early'}}
        Start-Sleep -Milliseconds 500
    }
} finally {
    $errors=@()
    foreach($process in @($owned|Select-Object -Skip 1)) {
        if(!$process.HasExited){$process.CloseMainWindow()|Out-Null;if(!$process.WaitForExit(15000)){Stop-Process -Id $process.Id;$errors+='Native restart client required forced close'}}
    }
    if($owned.Count -gt 0 -and !$owned[0].HasExited -and !$owned[0].WaitForExit(40000)){Stop-Process -Id $owned[0].Id;$errors+='Restart server did not stop normally'}
    if($errors.Count){throw ($errors -join '; ')}
}
$output=Get-Content -LiteralPath "$root/server-resume.log" -Raw
if([regex]::Matches($output,'CHARACTER_CHECKPOINT_RESTORED \d+').Count -ne 2 -or $output -notmatch 'ADMITTED_CLIENTS 2' -or $output -notmatch 'SERVER_STOPPED'){throw 'Native clients did not restore both server character checkpoints'}
if(Get-Content -LiteralPath "$root/server-resume-error.log" -Raw){throw 'Restart server logged an error'}
foreach($role in @('host','guest')) {
    $file=Get-ChildItem "$root/$role/appdata/logs" -Filter '*.log'|Sort-Object LastWriteTimeUtc -Descending|Select-Object -First 1
    $log=Get-Content -LiteralPath $file.FullName -Raw
    if($log -match 'FATAL ERROR|! CoopNet update failed:|UnhandledFilter|BusyHandsDebug|SCRIPT ERROR|critical Lua error|LUA.*(?:RUNTIME ERROR|error:)'){throw "$role restart runtime failed"}
    foreach($marker in @('saved session expired; retrying saved character','canonical baseline loaded and acknowledged','guest inventory view applied:','CoopNet session stopped')){if(!$log.Contains($marker)){throw "$role missing restart evidence: $marker"}}
}
if((Get-FileHash -LiteralPath "$data/world.bootstrap").Hash -ne $archiveHash){throw 'Restart changed the world seed'}
foreach($path in $checkpointHashes.Keys){if((Get-FileHash -LiteralPath $path).Hash -ne $checkpointHashes[$path]){throw 'Restart clobbered a stored character'}}
Write-Output "PASS: both native clients rejoined a restarted standalone server; disk character profiles restored, original checkpoints retained. Evidence: $root"
Write-Output 'Session resume credentials currently expire on server restart; verified native fallback re-imports the same saved characters.'

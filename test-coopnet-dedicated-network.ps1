param([ValidateRange(1024,65535)][int]$Port = 27891)
$ErrorActionPreference = 'Stop'
$dependencyRoot = Join-Path $PSScriptRoot '_build\coopnet-deps'
$installed = Join-Path $dependencyRoot 'installed\x64-windows'
$cmake = Get-ChildItem "$dependencyRoot\vcpkg\downloads\tools" -Recurse -Filter cmake.exe |
    Select-Object -First 1 -ExpandProperty FullName
if (!$cmake) { throw 'Run setup-coopnet-deps.ps1 first.' }
$build = Join-Path $PSScriptRoot '_build\coopnet-network-tests'
& $cmake -S "$PSScriptRoot\tests\coopnet" -B $build -G 'Visual Studio 18 2026' -A x64 "-DCMAKE_PREFIX_PATH=$installed"
if ($LASTEXITCODE) { throw 'Dedicated network configure failed' }
& $cmake --build $build --config Release --target DedicatedNetwork --parallel 2
if ($LASTEXITCODE) { throw 'Dedicated network build failed' }
$exe = Join-Path $build 'Release\DedicatedNetwork.exe'
$evidence = Join-Path $build ('dedicated-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $evidence | Out-Null
$savedPath = $env:PATH
$ownedProcesses = @()
try {
    $env:PATH = "$installed\bin;$savedPath"
    $ownedProcesses += Start-Process -FilePath $exe -ArgumentList 'server',"$Port" -WindowStyle Hidden -PassThru -RedirectStandardOutput "$evidence\server.log" -RedirectStandardError "$evidence\server-error.log"
    $readyDeadline = [datetime]::UtcNow.AddSeconds(10)
    while (!(Select-String -LiteralPath "$evidence\server.log" -Pattern '^DEDICATED_LISTENING' -Quiet)) {
        if ($ownedProcesses[0].HasExited -or [datetime]::UtcNow -gt $readyDeadline) { throw 'Dedicated listener did not start' }
        Start-Sleep -Milliseconds 50
    }
    foreach ($character in 200..203) {
        $ownedProcesses += Start-Process -FilePath $exe -ArgumentList 'client',"127.0.0.1:$Port","$character" -WindowStyle Hidden -PassThru -RedirectStandardOutput "$evidence\client-$character.log" -RedirectStandardError "$evidence\client-$character-error.log"
    }
    $deadline = [datetime]::UtcNow.AddSeconds(35)
    while (@($ownedProcesses | Where-Object { !$_.HasExited }).Count) {
        if ([datetime]::UtcNow -gt $deadline) { throw 'Dedicated fixture processes timed out' }
        Start-Sleep -Milliseconds 100
    }
    foreach ($ownedProcess in $ownedProcesses) {
        $ownedProcess.WaitForExit()
        if ($ownedProcess.ExitCode -ne 0) {
            Get-ChildItem -LiteralPath $evidence -Filter '*error.log' | ForEach-Object { Get-Content -LiteralPath $_.FullName }
            throw "Dedicated fixture failed: $evidence"
        }
    }
    if (!(Select-String -LiteralPath "$evidence\server.log" -Pattern '^DEDICATED_NETWORK_PASS' -Quiet)) { throw 'Missing server evidence' }
    foreach ($character in 200..203) {
        if (!(Select-String -LiteralPath "$evidence\client-$character.log" -Pattern '^DEDICATED_CLIENT_PASS' -Quiet)) { throw "Missing client evidence: $character" }
    }
    Get-ChildItem -LiteralPath $evidence -Filter '*.log' | ForEach-Object { Get-Content -LiteralPath $_.FullName }
    Write-Output "Dedicated network evidence: $evidence"
} finally {
    foreach ($ownedProcess in $ownedProcesses) {
        if (!$ownedProcess.HasExited) { Stop-Process -Id $ownedProcess.Id }
        $ownedProcess.Dispose()
    }
    $env:PATH = $savedPath
}

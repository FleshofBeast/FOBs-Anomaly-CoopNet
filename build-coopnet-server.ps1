$ErrorActionPreference = 'Stop'
$dependencyRoot = Join-Path $PSScriptRoot '_build\coopnet-deps'
$installed = Join-Path $dependencyRoot 'installed\x64-windows'
$cmake = Get-ChildItem "$dependencyRoot\vcpkg\downloads\tools" -Recurse -Filter cmake.exe |
    Select-Object -First 1 -ExpandProperty FullName
if (!$cmake) { throw 'Run setup-coopnet-deps.ps1 first.' }
$build = Join-Path $PSScriptRoot '_build\coopnet-server'
& $cmake -S "$PSScriptRoot\src\CoopNet\server" -B $build -G 'Visual Studio 18 2026' -A x64 "-DCMAKE_PREFIX_PATH=$installed"
if ($LASTEXITCODE) { throw 'Dedicated server configure failed' }
& $cmake --build $build --config Release --parallel 2
if ($LASTEXITCODE) { throw 'Dedicated server build failed' }
Copy-Item "$installed\bin\*.dll" "$build\Release" -Force
Write-Output "Dedicated server executable: $build\Release\CoopNetServer.exe"

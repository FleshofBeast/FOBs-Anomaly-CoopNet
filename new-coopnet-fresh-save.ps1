param([string]$Name='coopnet-fresh-stalker',[string]$Faction='stalker',[string]$Map='rookie_village',[ValidateRange(60,600)][int]$TimeoutSeconds=300,[switch]$Force)
# Generates a stock new-game save (neutral reputation and goodwill) for CoopNet fixtures.
# Runs in a disposable client copy; only the finished save is added to the installed savedgames.
$ErrorActionPreference='Stop'
$client=Join-Path (Split-Path $PSScriptRoot) 'Anomaly-1.5.3'
$target=Join-Path $client 'appdata/savedgames'
if((Test-Path -LiteralPath (Join-Path $target "$Name.scop")) -and !$Force){throw "Save already exists: $Name (use -Force to replace)"}
$testRoot=Join-Path $PSScriptRoot ('_build/coopnet-fresh-save-'+[Guid]::NewGuid().ToString('N'))
$protected=@(Get-ChildItem -LiteralPath $target -File | Where-Object {$_.BaseName -ne $Name})+@(Get-Item "$client/gamedata/configs/axr_options.ltx")
$hashes=@{};foreach($file in $protected){$hashes[$file.FullName]=(Get-FileHash -LiteralPath $file.FullName).Hash}
& "$PSScriptRoot/prepare-coopnet-engine-test.ps1" -TestDirectory $testRoot
$root=Join-Path $testRoot 'host'
Copy-Item "$client/gamedata" $root -Recurse
New-Item "$root/db" -ItemType Junction -Target "$client/db" | Out-Null
$fsLines=Get-Content "$root/fsgame.ltx" | Where-Object {$_ -notmatch '^\$fs_root\$'}
@('$fs_root$ = false | false | '+$root+'\')+@($fsLines) | Set-Content "$root/fsgame.ltx" -Encoding ascii
$optionsPath=Join-Path $root 'gamedata/configs/axr_options.ltx'
$options=Get-Content $optionsPath -Raw
foreach($entry in @{new_game_faction=$Faction;new_game_map=$Map;new_game_character_name='FreshStalker'}.GetEnumerator()) {
    $options=[regex]::Replace($options,'(?m)^\s*'+$entry.Key+'\s*=.*$',('        '+$entry.Key+' = '+$entry.Value))
}
Set-Content $optionsPath $options -Encoding ascii
# Disposable copy only: save once stock start-up has settled, then quit.
@"
local saved=false
local function save_then_quit()
	if not saved then
		saved=true
		printf("CoopNet fresh save: writing $Name")
		exec_console_cmd("save $Name")
		ResetTimeEvent("coopnet_fresh_save","save",5)
		return false
	end
	exec_console_cmd("quit")
	return true
end
local function first_update()
	CreateTimeEvent("coopnet_fresh_save","save",20,save_then_quit)
end
function on_game_start()
	RegisterScriptCallback("actor_on_first_update",first_update)
end
"@ | Set-Content "$root/gamedata/scripts/zz_coopnet_fresh_save.script" -Encoding ascii
$lines=Get-Content "$root/appdata/user.ltx" | Where-Object {$_ -notmatch '^(coop_|start )'}
@($lines)+@('start server(all/single/alife/new) client(localhost)') | Set-Content "$root/appdata/user.ltx" -Encoding ascii
$process=Start-Process "$root/bin/AnomalyDX11.exe" -WorkingDirectory $root -ArgumentList '-silent_error_mode','-noprefetch' -WindowStyle Hidden -PassThru
$null=$process.Handle
try {
    if(!$process.WaitForExit($TimeoutSeconds*1000)){throw "New game did not save and quit within $TimeoutSeconds seconds; logs: $testRoot"}
} finally {
    if(!$process.HasExited){Stop-Process -Id $process.Id}
    foreach($path in $hashes.Keys){if((Get-FileHash -LiteralPath $path).Hash -ne $hashes[$path]){throw "Installed file changed: $path"}}
}
$log=Get-ChildItem "$root/appdata/logs" -Filter *.log | Sort-Object LastWriteTime -Descending | Select-Object -First 1
$text=Get-Content -LiteralPath $log.FullName -Raw
if($text -match 'FATAL ERROR|\[SCRIPT ERROR\]|critical Lua error'){throw "Engine reported an error; logs: $testRoot"}
if($text -notmatch 'New game is successfully created'){throw "New game was not created; logs: $testRoot"}
$produced=@(Get-ChildItem "$root/appdata/savedgames" -File | Where-Object BaseName -eq $Name)
if(!($produced | Where-Object Extension -eq '.scop')){throw "Save file was not written; logs: $testRoot"}
foreach($file in $produced){Copy-Item -LiteralPath $file.FullName -Destination $target -Force}
Write-Output "PASS: fresh $Faction new game at $Map saved as '$Name' ($(($produced | ForEach-Object Name) -join ', ')). Logs: $testRoot"
Write-Output "Use: test-coopnet-dedicated-bootstrap.ps1 -Simulation -FixtureSave '$Name'"

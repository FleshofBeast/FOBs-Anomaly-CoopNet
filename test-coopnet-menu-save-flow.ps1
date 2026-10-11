$ErrorActionPreference='Stop'
$testRoot=Join-Path $PSScriptRoot ('_build/menu-save-flow-'+[Guid]::NewGuid().ToString('N'))
& "$PSScriptRoot/prepare-coopnet-engine-test.ps1" -LoadFixture -ReplicaProbe -NativeWorldProbe -NativeDefaultProbe -TestDirectory $testRoot
foreach($role in @('host','guest')) {
    $root=Join-Path $testRoot $role
    $config=Join-Path $root 'appdata/user.ltx'
    Get-Content $config | Where-Object {$_ -notmatch '^(coop_|start )'} | Set-Content "$config.tmp" -Encoding ascii
    Move-Item -LiteralPath "$config.tmp" -Destination $config -Force
    Add-Content "$root/scripts/ui_load_dialog.script" @"
function UILoadDialog:Update()
    CUIScriptWnd.Update(self)
    if self:IsShown() and self.list_box:GetSize() > 0 and not self.coop_test_loaded then
        self.coop_test_frames = (self.coop_test_frames or 0) + 1
        if self.coop_test_frames >= 30 then
            self.coop_test_loaded = true
            self.list_box:SetSelectedIndex(0)
            self:OnButton_load_clicked()
        end
    end
end
"@ -Encoding ascii
}
$owned=@()
try {
    foreach($role in @('host','guest')) {
        $root=Join-Path $testRoot $role
        $flag=if($role -eq 'host'){'-coop_host_selector_probe'}else{'-coop_join_menu_save_probe'}
        $owned+=Start-Process "$root/bin/AnomalyDX11.exe" -WorkingDirectory $root -ArgumentList '-silent_error_mode','-noprefetch',$flag -WindowStyle Hidden -PassThru
    }
    $deadline=(Get-Date).AddSeconds(120)
    while((Get-Date) -lt $deadline){foreach($process in $owned){if($process.HasExited){throw 'Menu save flow client exited early'}}; Start-Sleep -Seconds 2}
} finally {
    foreach($process in $owned){if(!$process.HasExited){$process.CloseMainWindow() | Out-Null; if(!$process.WaitForExit(10000)){Stop-Process -Id $process.Id}}}
}
$h=Get-Content "$testRoot/host/appdata/logs/xray_deadparrot.log" -Raw
$g=Get-Content "$testRoot/guest/appdata/logs/xray_deadparrot.log" -Raw
if($h -notmatch 'Host game save selection opened' -or $h -notmatch 'host ready participants: 2' -or $g -notmatch 'character selection opened: load' -or $g -notmatch 'canonical baseline loaded and acknowledged' -or ($h+$g) -match 'FATAL ERROR|\[SCRIPT ERROR\]|CoopNet update failed|initial join failed'){throw "Actual save-picker host/join flow failed: $testRoot"}
Write-Output "PASS actual Host game and address/Connect/Load save callbacks, stock save loading, admission and shared world: $testRoot"

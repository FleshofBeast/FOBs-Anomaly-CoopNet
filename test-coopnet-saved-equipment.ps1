param([int]$Seconds=150,[switch]$HuntingFollower,[switch]$SimulatedWan,[switch]$GuestRelevance)
$ErrorActionPreference='Stop'
$testRoot=Join-Path $PSScriptRoot ('_build/saved-equipment-'+[Guid]::NewGuid().ToString('N'))
& "$PSScriptRoot/prepare-coopnet-engine-test.ps1" -LoadFixture -ReplicaProbe -NativeWorldProbe -NativeDefaultProbe -WorldProbe -TestDirectory $testRoot
foreach($role in @('host','guest')) {
    $root=Join-Path $testRoot $role
    $config=Join-Path $root 'appdata/user.ltx'
    Get-Content $config | Where-Object {$_ -notmatch '^(coop_|start )'} | Set-Content "$config.tmp" -Encoding ascii
    Move-Item -LiteralPath "$config.tmp" -Destination $config -Force
    Add-Content "$root/scripts/ui_load_dialog.script" @'
function UILoadDialog:Update()
    CUIScriptWnd.Update(self)
    if self:IsShown() and self.list_box:GetSize()>0 and not self.coop_test_loaded then
        self.coop_test_frames=(self.coop_test_frames or 0)+1
        if self.coop_test_frames>=30 then
            self.coop_test_loaded=true self.list_box:SetSelectedIndex(0)
            self:OnButton_load_clicked()
        end
    end
end
'@ -Encoding ascii
}
# Exercise the existing saved slots through native activation only. This test
# never moves an item, spawns equipment or directly repairs a weapon state.
$probe=@'
local started, requested, index, saved_slot
local slots={}
local done=false
local follower,follow_started,follow_origin,walk_started,walk_done
function check_follower()
    if not COOP_TEST_HUNTING_FOLLOWER then return end
    local now=time_global()
    if not follow_started then
        follower=get_story_object('esc_2_12_stalker_fanat')
        assert(follower,'Fanatic must be online for the hunting dialogue fixture')
        dialogs_escape.set_esc_2_12_stalker_fanat_as_companion(db.actor,follower)
        follower:force_set_position(vector():set(-269.5,-9.5,-232.3))
        follow_origin=follower:position()
        follow_started=now return
    end
    if follow_started<0 then
        if walk_done then return end
        if not walk_started then
            walk_started=now get_console():execute('coop_movement_probe auto')
        elseif now-walk_started>=5000 then
            get_console():execute('coop_movement_probe')
            walk_done=true printf('* CoopNet native walking visual stimulus completed')
        end
        return
    end
    local current=follower:position()
    if current:distance_to(follow_origin)>2 and current:distance_to(db.actor:position())<10 then
        printf('* CoopNet hunting follower probe passed: native Fanatic followed guest')
        follow_started=-1 return
    end
    assert(now-follow_started<30000,'Fanatic did not follow the guest after native hunting dialogue recruitment')
end
function update()
    if not db.actor or not level.coop_world_replica() then return end
    if done then check_follower() return end
    local now=time_global()
    if not started then started=now return end
    if now-started<20000 then return end
    if not index then
        local current=db.actor:active_item()
        saved_slot=65535
        for slot=1,16 do
            local item=db.actor:item_in_slot(slot)
            if current and item and item:id()==current:id() then saved_slot=slot end
        end
        for slot=1,3 do
            local item=db.actor:item_in_slot(slot)
            if item and item:section():find('wpn_',1,true)==1 then slots[#slots+1]=slot end
        end
        assert(#slots>=2,'Saved fixture requires multiple equipped weapons')
        db.actor:force_set_position(vector():set(-269.5,-9.5,-244.3))
        index=1
    end
    local slot=slots[index]
    if not slot then
        db.actor:activate_slot(saved_slot)
        printf('* CoopNet saved equipment probe completed: slots %s',#slots)
        done=true return
    end
    local item=db.actor:item_in_slot(slot)
    assert(item,'Saved equipped weapon disappeared')
    if not requested then db.actor:activate_slot(slot) requested=now return end
    local active=db.actor:active_item()
    if active and active:id()==item:id() and item:get_state()==0 and now-requested>=1500 then
        printf('* CoopNet saved equipment slot ready: slot %s section %s',slot,item:section())
        index=index+1 requested=nil return
    end
    assert(now-requested<10000,'Saved weapon did not draw without re-equipping')
end
function on_game_start() RegisterScriptCallback('actor_on_update',update) end
'@
[IO.File]::WriteAllText("$testRoot/guest/scripts/zz_coop_saved_equipment_probe.script",$probe)
if($HuntingFollower){Add-Content "$testRoot/guest/scripts/zz_coop_saved_equipment_probe.script" "`nCOOP_TEST_HUNTING_FOLLOWER=true" -Encoding ascii}
$observer=@'
local first,moved
function update()
    if not db.actor or level.coop_world_replica() then return end
    local now=time_global()
    first=first or now
    if now-first<40000 or moved then return end
    if now-first>=40000 then
        db.actor:force_set_position(vector():set(-269.5,-9.5,-248.3))
        moved=true
    end
end
function on_game_start() RegisterScriptCallback('actor_on_update',update) end
'@
[IO.File]::WriteAllText("$testRoot/host/scripts/zz_coop_remote_observer_probe.script",$observer)
if($GuestRelevance) {
    Add-Content "$testRoot/host/scripts/zz_coop_remote_observer_probe.script" @'

local relevance_id,relevance_started,relevance_done,relevance_guest,relevance_scan
function check_guest_relevance()
    if not db.actor or relevance_done then return end
    local now=time_global()
    if not relevance_guest then
        relevance_scan=relevance_scan or 0
        for id=relevance_scan,math.min(relevance_scan+511,65534) do
            local candidate=level.object_by_id(id)
            if candidate and candidate:id()~=db.actor:id() and (candidate:clsid()==clsid.actor or candidate:clsid()==clsid.script_actor) then relevance_guest=candidate break end
        end
        relevance_scan=(relevance_scan+512)%65535
        return
    end
    if not relevance_started then
        if relevance_guest:position():distance_to(vector():set(-269.5,-9.5,-244.3))<15 then relevance_started=now end
        return
    end
    if now-relevance_started<45000 then return end
    if not relevance_id then
        local point=vector():set(-271,-9.5,-244.3)
        local vertex=level.vertex_id(point)
        point=level.vertex_position(vertex)
        local object=alife():create('bread',point,vertex,db.actor:game_vertex_id())
        assert(object,'Guest relevance fixture spawn failed')
        relevance_id=object.id
        alife():set_switch_distance(30)
        db.actor:force_set_position(vector():set(-222.6,-19.87,-168))
        relevance_started=now-45000
        return
    end
    if now-relevance_started<65000 then return end
    local object=alife():object(relevance_id)
    assert(object and level.object_by_id(relevance_id),'Object near guest went offline when host moved away')
    assert(db.actor:position():distance_to(object.position)>60,'Host must be outside stock relevance radius')
    printf('* CoopNet guest ALife relevance passed: host distant, guest-near object remains online')
    relevance_done=true
end
local stock_on_game_start=on_game_start
function on_game_start()
    stock_on_game_start()
    RegisterScriptCallback('actor_on_update',check_guest_relevance)
end
'@ -Encoding ascii
}
$owned=@()
try {
    foreach($role in @('host','guest')) {
        $root=Join-Path $testRoot $role
        $flag=if($role-eq 'host'){'-coop_host_selector_probe'}else{'-coop_join_menu_save_probe'}
        $launchArgs=@('-silent_error_mode','-noprefetch','-coop_engine_fixture','-coop_remote_visual_probe','-coop_native_visual_probe',$flag)
        if($SimulatedWan){$launchArgs+='-coop_wan_simulation_probe'}
        $owned+=Start-Process "$root/bin/AnomalyDX11.exe" -WorkingDirectory $root -ArgumentList $launchArgs -WindowStyle Hidden -PassThru
    }
    $deadline=(Get-Date).AddSeconds($Seconds)
    while((Get-Date)-lt $deadline){foreach($process in $owned){if($process.HasExited){throw 'Saved equipment client exited early'}};Start-Sleep -Seconds 1}
} finally {
    foreach($process in $owned){if(!$process.HasExited){$process.CloseMainWindow()|Out-Null;if(!$process.WaitForExit(10000)){Stop-Process -Id $process.Id}}}
}
$hostLog=Get-Content "$testRoot/host/appdata/logs/xray_deadparrot.log" -Raw
$guestLog=Get-Content "$testRoot/guest/appdata/logs/xray_deadparrot.log" -Raw
if($SimulatedWan -and ($hostLog-notmatch 'simulated WAN enabled' -or $guestLog-notmatch 'simulated WAN enabled')){throw 'Simulated WAN profile was not enabled in both clients'}
if(($hostLog+$guestLog)-match 'FATAL ERROR|\[SCRIPT ERROR\]|CoopNet update failed|Failed to load script zz_coop'){throw "Saved equipment runtime failed: $testRoot"}
if($guestLog-notmatch 'saved equipment probe completed: slots [2-3]' -or $hostLog-notmatch 'remote appearance applied: .*wpn_pm' -or $hostLog-notmatch 'appearance probe screenshot: .*wpn_(pm|bm16).*local native animation enabled'){throw "Saved equipment activation or remote native animation capture missing: $testRoot"}
if($HuntingFollower -and $guestLog-notmatch 'hunting follower probe passed: native Fanatic followed guest'){throw "Native hunting follower did not pass: $testRoot"}
if($HuntingFollower -and $guestLog-notmatch 'native walking visual stimulus completed'){throw "Native walking stimulus did not finish: $testRoot"}
if($GuestRelevance -and $hostLog-notmatch 'guest ALife relevance passed'){throw "Guest ALife relevance did not pass: $testRoot"}
Write-Output "PASS native saved-slot activation without unequip/re-equip and remote native cycle capture: $testRoot; inspect rendered screenshots for visual correctness."

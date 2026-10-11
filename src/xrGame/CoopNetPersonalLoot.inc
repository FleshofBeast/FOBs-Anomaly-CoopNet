// Owner-thread personal corpse/container snapshots, keyed by persistent world
// scope and native name. These native records never cross the network.
namespace {
std::uint64_t personal_world_scope=0;
xr_set<u16> personal_loot_dirty;
struct LootRestore {unsigned stage=0;xr_vector<u16> created;};
xr_map<const CGameObject*,LootRestore> personal_loot_restores;
void reset_personal_loot() {personal_world_scope=0;personal_loot_dirty.clear();personal_loot_restores.clear();}
bool loot_items(CGameObject* source,TIItemContainer& items) {
    if(auto* box=smart_cast<CInventoryBox*>(source)) {
        for(auto id:box->m_items) {
            auto* item=smart_cast<CInventoryItem*>(Level().Objects.net_Find(id));
            if(!item || item->object().getDestroy()) return false;
            items.push_back(item);
        }
        return true;
    }
    auto* owner=smart_cast<CInventoryOwner*>(source);
    if(!owner || source->cast_actor() || owner->is_alive()) return false;
    items=owner->inventory().m_all;return true;
}
bool capture_personal_loot() {
    if(!native_world_requested() || !g_pGameLevel || !Level().bReady || !Level().Server || !personal_world_scope) return true;
    LocalActorPose local;if(!capture_local_actor(local)) return false;
    for(auto id:personal_loot_dirty) {
        auto* source=smart_cast<CGameObject*>(Level().Objects.net_Find(id));
        if(!source || source->getDestroy()) continue;
        const auto restoring=personal_loot_restores.find(source);
        if(restoring!=personal_loot_restores.end() && restoring->second.stage<2) return false;
        TIItemContainer items;if(!loot_items(source,items) || items.size()>256) return false;
        PersonalLoot snapshot;snapshot.scope=personal_world_scope;snapshot.level=local.level;
        snapshot.name=source->cName().c_str();snapshot.section=source->cNameSect().c_str();
        for(auto* item:items) {
            auto& object=item->object();auto* server=Level().Server->ID_to_entity(object.ID());
            if(object.getDestroy() || object.H_Parent()!=source || !server || server->ID_Parent!=source->ID()) return false;
            auto* native=smart_cast<CSE_ALifeInventoryItem*>(server);if(!native) return false;
            NET_Packet update;update.write_start();object.net_Export(update);update.read_start();server->UPDATE_Read(update);if(!update.r_eof()) return false;
            NET_Packet saved;saved.write_start();object.net_Save(saved);saved.read_start();server->load(saved);if(!saved.r_eof()) return false;
            native->m_fCondition=item->GetCondition();
            NET_Packet spawn;server->Spawn_Write(spawn,TRUE);
            PersonalLootItem record;record.section=object.cNameSect().c_str();record.spawn.assign(spawn.B.data,spawn.B.data+spawn.B.count);
            snapshot.items.push_back(std::move(record));
        }
        const auto found=std::find_if(personal_loot.begin(),personal_loot.end(),[&](const PersonalLoot& s){
            return s.scope==snapshot.scope && s.level==snapshot.level && s.name==snapshot.name && s.section==snapshot.section;
        });
        if(found==personal_loot.end()) personal_loot.push_back(std::move(snapshot));else *found=std::move(snapshot);
    }
    personal_loot_dirty.clear();return true;
}
}
void set_personal_world_scope(std::uint64_t scope) {if(scope) personal_world_scope=scope;}
void personal_loot_changed(CGameObject* source) {
    if(!native_world_requested() || !g_pGameLevel || !Level().bReady || !personal_world_scope || !source || source->getDestroy() || source->cast_actor()) return;
    const auto restoring=personal_loot_restores.find(source);
    if(restoring!=personal_loot_restores.end() && restoring->second.stage<2) return;
    // Native initialization/death can populate a source before its portable
    // contents have been restored. Those callbacks must not claim ownership
    // of this incarnation and suppress the saved snapshot.
    if(guest_native_world() && restoring==personal_loot_restores.end()) {
        LocalActorPose local;if(!capture_local_actor(local)) return;
        const auto saved=std::find_if(personal_loot.begin(),personal_loot.end(),[&](const PersonalLoot& s) {
            return s.scope==personal_world_scope && s.level==local.level && s.name==source->cName().c_str() && s.section==source->cNameSect().c_str();
        });
        if(saved!=personal_loot.end()) return;
    }
    TIItemContainer items;if(!loot_items(source,items)) return;
    personal_loot_dirty.insert(source->ID());
    // An active instance's current contents are already personal. Restore only
    // after a later canonical load, never undo a transfer in this incarnation.
    personal_loot_restores[source].stage=2;
}
void update_personal_loot() {
    if(!guest_native_world() || !g_pGameLevel || !Level().bReady || !Level().Server || !personal_world_scope) return;
    LocalActorPose local;if(!capture_local_actor(local)) return;
    for(const auto& snapshot:personal_loot) {
        if(snapshot.scope!=personal_world_scope || snapshot.level!=local.level) continue;
        CGameObject* source=nullptr;
        for(const auto& binding:world_objects) {
            auto* candidate=const_cast<CGameObject*>(binding.first);
            if(candidate->getDestroy() || snapshot.name!=candidate->cName().c_str() || snapshot.section!=candidate->cNameSect().c_str()) continue;
            if(source) {source=nullptr;break;}source=candidate;
        }
        if(!source) continue;
        TIItemContainer items;if(!loot_items(source,items)) continue;
        auto& restore=personal_loot_restores[source];if(restore.stage>=2) continue;
        if(!restore.stage) {
            restore.stage=1;
            for(auto* item:items) {NET_Packet destroy;CGameObject::u_EventGen(destroy,GE_DESTROY,item->object().ID());CGameObject::u_EventSend(destroy);}
            continue;
        }
        if(restore.created.empty() && !items.empty()) continue;
        if(restore.created.empty()) {
            // Preflight the complete batch before spawning any item.
            xr_vector<CSE_Abstract*> prepared;bool valid=true;
            for(const auto& item:snapshot.items) {
                auto* abstract=pSettings->section_exist(item.section.c_str()) ? F_entity_Create(item.section.c_str()) : nullptr;
                if(!abstract) {valid=false;break;}
                prepared.push_back(abstract);
                NET_Packet packet;packet.B.count=static_cast<u32>(item.spawn.size());packet.r_pos=0;memcpy(packet.B.data,item.spawn.data(),item.spawn.size());
                if(!smart_cast<CSE_ALifeInventoryItem*>(abstract) || !abstract->Spawn_Read(packet) || item.section!=abstract->s_name.c_str()) {valid=false;break;}
                abstract->ID=abstract->ID_Phantom=0xffff;abstract->ID_Parent=source->ID();abstract->m_bALifeControl=false;abstract->o_Position=source->Position();
                if(auto* life=smart_cast<CSE_ALifeObject*>(abstract)) {
                    life->m_tNodeID=source->ai_location().level_vertex_id();
                    if(auto* parent=smart_cast<CSE_ALifeObject*>(Level().Server->ID_to_entity(source->ID()))) life->m_tGraphID=parent->m_tGraphID;
                }
            }
            if(!valid) {for(auto* abstract:prepared) F_entity_Destroy(abstract);throw std::runtime_error("Personal loot restoration preflight failed");}
            for(auto* abstract:prepared) {
                NET_Packet packet;abstract->Spawn_Write(packet,TRUE);u16 type;packet.r_begin(type);
                auto* created=Level().Server->Process_spawn(packet,Level().Server->GetServerClient()->ID,FALSE,nullptr,true);
                F_entity_Destroy(abstract);if(!created) throw std::runtime_error("Personal loot restoration spawn failed");restore.created.push_back(created->ID);
            }
            if(!snapshot.items.empty()) continue;
        }
        bool ready=true;
        for(auto id:restore.created) {auto* item=Level().Objects.net_Find(id);if(!item || item->H_Parent()!=source) ready=false;}
        if(ready) {restore.stage=2;Msg("* CoopNet personal loot restored: source %s items %u",snapshot.name.c_str(),static_cast<unsigned>(snapshot.items.size()));}
    }
}

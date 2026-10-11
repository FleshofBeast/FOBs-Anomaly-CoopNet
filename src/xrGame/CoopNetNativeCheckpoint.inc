namespace {
bool capture_region_state(const coopnet::RegionKey& key,double width,bool offline_only,
    coopnet::NativeRegionCheckpoint& output) {
    const auto reject=[](const char* reason,u16 id) {
        if(strstr(Core.Params,"-coop_engine_fixture") && strstr(Core.Params,"-coop_native_acceptance_probe"))
            Msg("! CoopNet native checkpoint capture rejected: %s object %u",reason,id);
        return false;
    };
    LocalActorPose local;
    if(!collect_world_objects || !capture_local_actor(local) || world_level_is_replica() ||
        !ai().get_alife() || !Level().Server || !coopnet::valid_region(key) ||
        !std::isfinite(width) || width<10 || width>4096) return reject("capture context",0xffff);
    const auto& registry=ai().alife().objects();
    std::vector<CSE_Abstract*> pending;
    std::set<u16> selected;
    const auto add=[&](CSE_Abstract* server) {
        if(selected.insert(server->ID).second) pending.push_back(server);
    };
    // A squad is one dependency set. Members follow the group's cell even when
    // their cached positions straddle a boundary; inventory follows its parent.
    for(const auto& entry:registry.objects()) {
        auto* server=entry.second;
        if(server->ID_Parent!=0xffff || !server->can_save() || server->redundant() ||
            (offline_only && server->m_bOnline) || smart_cast<CSE_ALifeCreatureActor*>(server) ||
            private_quest_followers.count(server->ID)) continue;
        if(auto* member=smart_cast<CSE_ALifeMonsterAbstract*>(server)) if(member->m_group_id!=0xffff) {
            const auto* squad=smart_cast<CSE_ALifeOnlineOfflineGroup*>(registry.object(member->m_group_id,true));
            if(!squad || !squad->can_save() || squad->squad_members().find(member->ID)==squad->squad_members().end()) return reject("squad root binding",member->ID);
            continue;
        }
        if(auto* group=smart_cast<CSE_ALifeOnlineOfflineGroup*>(server)) group->synchronize_location();
        if(!ai().game_graph().valid_vertex_id(server->m_tGraphID)) return reject("root graph",server->ID);
        const auto level=static_cast<std::uint32_t>(ai().game_graph().vertex(server->m_tGraphID)->level_id())+1;
        if(level!=key.level) continue;
        const auto* native=level==local.level ? smart_cast<CGameObject*>(Level().Objects.net_Find(server->ID)) : nullptr;
        const auto& position=native ? native->Position() : server->o_Position;
        for(unsigned axis=0;axis<3;++axis) if(!std::isfinite(position[axis]) || std::abs(position[axis])>1000000.f) return reject("root position",server->ID);
        if(coopnet::region_at(level,{position.x,position.y,position.z},width)==key) add(server);
    }
    // Transient native fixture/mission objects need not have ALife registration.
    if(!offline_only && key.level==local.level) for(const auto& entry:world_objects) {
        auto* object=const_cast<CGameObject*>(entry.first);
        if(entry.second.replica || object->cast_actor() || object->getDestroy() || object->H_Parent() ||
            private_quest_followers.count(object->ID()) || registry.object(object->ID(),true)) continue;
        if(!smart_cast<CEntityAlive*>(object) && !smart_cast<CCustomZone*>(object) && !smart_cast<CInventoryBox*>(object)) continue;
        auto* server=Level().Server->ID_to_entity(object->ID());
        if(!server || server->ID_Parent!=0xffff) return reject("transient root",object->ID());
        const auto& position=object->Position();
        for(unsigned axis=0;axis<3;++axis) if(!std::isfinite(position[axis]) || std::abs(position[axis])>1000000.f) return reject("transient position",server->ID);
        if(coopnet::region_at(key.level,{position.x,position.y,position.z},width)==key) add(server);
    }
    coopnet::NativeRegionCheckpoint checkpoint;checkpoint.key=key;
    std::size_t bytes=18;
    std::map<u16,std::vector<u16>> groups;
    for(std::size_t index=0;index<pending.size();++index) {
        auto* server=pending[index];
        if(pending.size()>4096 || smart_cast<CSE_ALifeCreatureActor*>(server) || private_quest_followers.count(server->ID)) return reject("object scope",server->ID);
        auto* dynamic=smart_cast<CSE_ALifeDynamicObject*>(server);
        if(dynamic && !dynamic->can_save()) return reject("unsaveable dependency",server->ID);
        const bool online=!dynamic || dynamic->m_bOnline;
        if(offline_only && online) return reject("online dependency in offline cell",server->ID);
        auto* group=smart_cast<CSE_ALifeOnlineOfflineGroup*>(server);
        auto* native=key.level==local.level ? smart_cast<CGameObject*>(Level().Objects.net_Find(server->ID)) : nullptr;
        const auto world=native ? world_objects.find(native) : world_objects.end();
        if(online && !group && (!native || native->getDestroy() || world==world_objects.end() || world->second.replica ||
            Level().Server->ID_to_entity(server->ID)!=server)) return reject("online binding",server->ID);
        for(auto child:server->children) {
            auto* child_server=registry.object(child,true);
            if(!child_server) child_server=smart_cast<CSE_ALifeDynamicObject*>(Level().Server->ID_to_entity(child));
            if(!child_server || child_server->ID_Parent!=server->ID || selected.count(child)) return reject("inventory ownership",child);
            add(child_server);
        }
        if(group) for(const auto& member:group->squad_members()) {
            auto* member_server=registry.object(member.first,true);
            auto* monster=member_server ? smart_cast<CSE_ALifeMonsterAbstract*>(member_server) : nullptr;
            if(!monster || monster->m_group_id!=server->ID || monster->ID_Parent!=0xffff ||
                monster->m_bOnline!=online || private_quest_followers.count(monster->ID)) return reject("squad ownership",member.first);
            groups[server->ID].push_back(member.first);add(member_server);
        }
        if(native && !group) {
            if(native->H_Parent() && native->H_Parent()->ID()!=server->ID_Parent) return reject("native inventory parent",server->ID);
            NET_Packet exported;exported.B.count=0;exported.r_pos=0;
            native->net_Export(exported);server->UPDATE_Read(exported);
            if(!exported.r_eof()) return reject("native update consumption",server->ID);
            NET_Packet saved;saved.B.count=0;saved.r_pos=0;
            native->net_Save(saved);server->load(saved);
            if(!saved.r_eof()) return reject("native save consumption",server->ID);
            if(auto* item=smart_cast<CInventoryItem*>(native))
                if(auto* inventory=smart_cast<CSE_ALifeInventoryItem*>(server)) inventory->m_fCondition=item->GetCondition();
        }
        coopnet::NativeRegionObject record;
        record.object=server->ID;record.parent=server->ID_Parent;record.online=online;record.section=server->s_name.c_str();
        const auto lifetime=alife_lifetimes.find(server);
        if(lifetime!=alife_lifetimes.end()) record.incarnation=lifetime->second;
        else if(world!=world_objects.end()) record.incarnation=world->second.incarnation;
        else return reject("native lifetime",server->ID);
        NET_Packet spawn,update;server->Spawn_Write(spawn,TRUE);update.w_begin(M_UPDATE);server->UPDATE_Write(update);
        record.spawn.assign(spawn.B.data,spawn.B.data+spawn.B.count);record.update.assign(update.B.data,update.B.data+update.B.count);
        bytes+=22+record.section.size()+record.spawn.size()+record.update.size();
        if(bytes>coopnet::region_checkpoint_limit) return reject("byte capacity",server->ID);
        auto* verify=F_entity_Create(record.section.c_str());if(!verify) return reject("native class",server->ID);
        spawn.r_pos=0;
        bool valid=verify->Spawn_Read(spawn) && spawn.r_eof() && verify->ID==record.object && verify->ID_Parent==record.parent &&
            xr_strcmp(verify->s_name.c_str(),record.section.c_str())==0 && verify->client_data==server->client_data;
        if(valid) {u16 message;update.r_begin(message);if(message!=M_UPDATE) valid=false;else {verify->UPDATE_Read(update);valid=update.r_eof();}}
        if(valid && group) {
            const auto* copied=smart_cast<CSE_ALifeOnlineOfflineGroup*>(verify);
            valid=copied && copied->squad_members().size()==groups[server->ID].size();
            if(valid) for(auto id:groups[server->ID]) if(copied->squad_members().find(id)==copied->squad_members().end()) {valid=false;break;}
        }
        F_entity_Destroy(verify);if(!valid) return reject("native spawn/update round-trip",server->ID);
        checkpoint.objects.push_back(std::move(record));
    }
    for(const auto& group:groups) for(auto member:group.second) if(!selected.count(member)) return reject("squad closure",member);
    if(!coopnet::valid_native_region_checkpoint(checkpoint)) return reject("checkpoint envelope",0xffff);
    output=std::move(checkpoint);return true;
}
}
bool capture_offline_region_checkpoint(const coopnet::RegionKey& key,double width,coopnet::NativeRegionCheckpoint& output) {
    return capture_region_state(key,width,true,output);
}
bool capture_native_region_checkpoint(const coopnet::RegionKey& key,double width,coopnet::NativeRegionCheckpoint& output) {
    return capture_region_state(key,width,false,output);
}

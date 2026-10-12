class RetainedOfflineRegion {
    struct Target {
        CSE_ALifeDynamicObject* object=nullptr;
        CLASS_ID type=0;
        u32 graph=0,story=0,spawn_story=0,spawn=0;
        u16 squad=0xffff;
        std::vector<u16> children;
        std::set<u16> members;
    };
    coopnet::NativeRegionCheckpoint checkpoint_;
    std::vector<Target> targets_;
    bool replica_=false;
    RetainedOfflineRegion()=default;
    friend std::shared_ptr<const RetainedOfflineRegion> retain_offline_region(const coopnet::RegionKey&,double);
    friend bool restore_retained_offline_region(const std::shared_ptr<const RetainedOfflineRegion>&);
    friend bool restore_host_offline_region(const coopnet::NativeRegionCheckpoint&);
};
std::shared_ptr<const RetainedOfflineRegion> retain_offline_region(const coopnet::RegionKey& key,double width) {
    auto retained=std::shared_ptr<RetainedOfflineRegion>(new RetainedOfflineRegion);
    if(!capture_offline_region_checkpoint(key,width,retained->checkpoint_)) return {};
    for(const auto& record:retained->checkpoint_.objects) {
        auto* server=ai().alife().objects().object(record.object,true);
        if(!server || record.online || server->m_bOnline) return {};
        RetainedOfflineRegion::Target target;
        target.object=server;target.type=server->m_tClassID;target.graph=server->m_tGraphID;
        target.story=server->m_story_id;target.spawn_story=server->m_spawn_story_id;target.spawn=server->m_tSpawnID;
        target.children.assign(server->children.begin(),server->children.end());
        if(auto* member=smart_cast<CSE_ALifeMonsterAbstract*>(server)) target.squad=member->m_group_id;
        if(auto* group=smart_cast<CSE_ALifeOnlineOfflineGroup*>(server))
            for(const auto& member:group->squad_members()) target.members.insert(member.first);
        retained->targets_.push_back(std::move(target));
    }
    return retained;
}
bool restore_retained_offline_region(const std::shared_ptr<const RetainedOfflineRegion>& retained) {
    if(!retained || !ai().get_alife() || world_level_is_replica()!=retained->replica_ ||
        retained->targets_.size()!=retained->checkpoint_.objects.size()) return false;
    const auto& registry=ai().alife().objects();
    // Check every target before replay. Structural changes require the registry
    // reconciliation path; this operation keeps all existing native aliases.
    for(std::size_t i=0;i<retained->targets_.size();++i) {
        const auto& target=retained->targets_[i];const auto& record=retained->checkpoint_.objects[i];
        auto* server=registry.object(record.object,true);
        if(!server || server!=target.object || server->m_bOnline || server->m_tClassID!=target.type ||
            server->ID_Parent!=record.parent || xr_strcmp(server->s_name.c_str(),record.section.c_str()) ||
            server->m_tGraphID!=target.graph || server->m_story_id!=target.story ||
            server->m_spawn_story_id!=target.spawn_story || server->m_tSpawnID!=target.spawn ||
            server->children.size()!=target.children.size() ||
            !std::equal(server->children.begin(),server->children.end(),target.children.begin())) return false;
        const auto lifetime=alife_lifetimes.find(server);
        if(lifetime==alife_lifetimes.end() || lifetime->second!=record.incarnation) return false;
        if(auto* member=smart_cast<CSE_ALifeMonsterAbstract*>(server)) if(member->m_group_id!=target.squad) return false;
        if(auto* group=smart_cast<CSE_ALifeOnlineOfflineGroup*>(server)) {
            if(group->squad_members().size()!=target.members.size()) return false;
            for(auto id:target.members) {
                const auto member=group->squad_members().find(id);
                if(member==group->squad_members().end() || !member->second ||
                    member->second!=registry.object(id,true)) return false;
            }
        }
    }
    // Replay only immutable tickets validated by local capture or the authenticated
    // host import path. Worker publications must not enter this native reader.
    for(std::size_t i=0;i<retained->targets_.size();++i) {
        const auto& record=retained->checkpoint_.objects[i];auto* server=retained->targets_[i].object;
        NET_Packet spawn,update;spawn.B.count=static_cast<u32>(record.spawn.size());spawn.r_pos=0;
        std::memcpy(spawn.B.data,record.spawn.data(),record.spawn.size());
        update.B.count=static_cast<u32>(record.update.size());update.r_pos=0;
        std::memcpy(update.B.data,record.update.data(),record.update.size());
        if(!server->Spawn_Read(spawn) || !spawn.r_eof()) return false;
        u16 message;update.r_begin(message);if(message!=M_UPDATE) return false;
        server->UPDATE_Read(update);if(!update.r_eof()) return false;
        if(auto* group=smart_cast<CSE_ALifeOnlineOfflineGroup*>(server))
            for(const auto& member:group->squad_members())
                if(!member.second || member.second!=registry.object(member.first,true)) return false;
    }
    return true;
}
// Called only for checkpoints received through the authenticated host transport.
// A worker publication must never enter this native Spawn_Read path on the host.
bool restore_host_offline_region(const coopnet::NativeRegionCheckpoint& checkpoint) {
    if(!world_level_is_replica() || !ai().get_alife() ||
        !coopnet::valid_native_region_checkpoint(checkpoint) || !install_checkpoint_squad_guard()) return false;
    auto retained=std::shared_ptr<RetainedOfflineRegion>(new RetainedOfflineRegion);
    retained->replica_=true;retained->checkpoint_=checkpoint;
    const auto& registry=ai().alife().objects();
    std::set<u16> included;
    std::vector<std::pair<CSE_ALifeInventoryItem*,float>> conditions;
    for(const auto& record:checkpoint.objects) included.insert(record.object);
    // Stage every record before modifying any registry object. Existing native
    // aliases and topology are retained; creation/removal needs reconciliation.
    for(auto& record:retained->checkpoint_.objects) {
        auto* server=registry.object(record.object,true);
        if(record.online || !server || server->m_bOnline || !server->can_save() || server->redundant() ||
            smart_cast<CSE_ALifeCreatureActor*>(server) || private_quest_followers.count(server->ID) ||
            server->ID_Parent!=record.parent || xr_strcmp(server->s_name.c_str(),record.section.c_str())) {
            Msg("! CoopNet offline bootstrap binding mismatch: object %u host section %s parent %u native section %s parent %u online %u",
                record.object,record.section.c_str(),record.parent,server ? server->s_name.c_str() : "missing",
                server ? server->ID_Parent : 0xffff,server ? unsigned(server->m_bOnline) : 0);return false;
        }
        const auto lifetime=alife_lifetimes.find(server);if(lifetime==alife_lifetimes.end()) return false;
        RetainedOfflineRegion::Target target;
        target.object=server;target.type=server->m_tClassID;target.graph=server->m_tGraphID;
        target.story=server->m_story_id;target.spawn_story=server->m_spawn_story_id;target.spawn=server->m_tSpawnID;
        target.children.assign(server->children.begin(),server->children.end());
        for(auto child:server->children) {
            const auto* object=registry.object(child,true);
            if(!object || (object->can_save() && !included.count(child))) {
                Msg("! CoopNet offline bootstrap child mismatch: parent %u child %u",server->ID,child);return false;
            }
        }
        if(auto* member=smart_cast<CSE_ALifeMonsterAbstract*>(server)) {
            target.squad=member->m_group_id;
            if(target.squad!=0xffff) {
                const auto* group=smart_cast<CSE_ALifeOnlineOfflineGroup*>(registry.object(target.squad,true));
                if(!included.count(target.squad) || !group || group->squad_members().find(server->ID)==group->squad_members().end()) return false;
            }
        }
        if(auto* group=smart_cast<CSE_ALifeOnlineOfflineGroup*>(server)) for(const auto& member:group->squad_members()) {
            if(!member.second || member.second!=registry.object(member.first,true) || !included.count(member.first)) {
                Msg("! CoopNet offline bootstrap squad mismatch: group %u member %u",server->ID,member.first);return false;
            }
            target.members.insert(member.first);
        }
        DetachedCheckpointRead detached;
        auto* copy=F_entity_Create(record.section.c_str());if(!copy) return false;
        NET_Packet spawn,update;spawn.B.count=static_cast<u32>(record.spawn.size());spawn.r_pos=0;
        std::memcpy(spawn.B.data,record.spawn.data(),record.spawn.size());
        update.B.count=static_cast<u32>(record.update.size());update.r_pos=0;
        std::memcpy(update.B.data,record.update.data(),record.update.size());
        bool valid=copy->Spawn_Read(spawn) && spawn.r_eof();
        if(valid) {u16 message;update.r_begin(message);valid=message==M_UPDATE;if(valid){copy->UPDATE_Read(update);valid=update.r_eof();}}
        const auto* dynamic=smart_cast<CSE_ALifeDynamicObject*>(copy);
        valid=valid && dynamic && copy->m_tClassID==target.type && copy->ID==record.object && copy->ID_Parent==record.parent &&
            !xr_strcmp(copy->s_name.c_str(),record.section.c_str()) && dynamic->m_tGraphID==target.graph &&
            dynamic->m_story_id==target.story && dynamic->m_spawn_story_id==target.spawn_story;
        if(valid && (!ai().game_graph().valid_vertex_id(dynamic->m_tGraphID) ||
            static_cast<std::uint32_t>(ai().game_graph().vertex(dynamic->m_tGraphID)->level_id())+1!=checkpoint.key.level)) valid=false;
        if(valid) {
            // m_group_id is a runtime registry binding, not a serialized member
            // field. The native group's serialized member set verifies ownership.
            const auto* group=smart_cast<CSE_ALifeOnlineOfflineGroup*>(copy);
            if(group) {
                if(group->squad_members().size()!=target.members.size()) valid=false;
                for(auto id:target.members) if(group->squad_members().find(id)==group->squad_members().end()) valid=false;
            }
        }
        if(valid) if(const auto* expected=smart_cast<CSE_ALifeInventoryItem*>(copy)) {
            auto* item=smart_cast<CSE_ALifeInventoryItem*>(server);
            if(!item || !std::isfinite(expected->m_fCondition) || expected->m_fCondition<0 || expected->m_fCondition>1) valid=false;
            else conditions.emplace_back(item,expected->m_fCondition);
        }
        if(!valid) Msg("! CoopNet offline bootstrap native state mismatch: object %u graph %u expected %u story %u expected %u spawn story %u expected %u",
            record.object,dynamic ? dynamic->m_tGraphID : 0xffff,target.graph,dynamic ? dynamic->m_story_id : 0xffffffff,target.story,
            dynamic ? dynamic->m_spawn_story_id : 0xffffffff,target.spawn_story);
        F_entity_Destroy(copy);if(!valid) return false;
        // Host canonical incarnations remain in the incoming catalogue. The
        // replay ticket separately binds this process's existing native lifetime.
        record.incarnation=lifetime->second;retained->targets_.push_back(std::move(target));
    }
    if(!restore_retained_offline_region(retained)) return false;
    for(const auto& item:conditions) if(std::abs(item.first->m_fCondition-item.second)>.0001f) return false;
    return true;
}

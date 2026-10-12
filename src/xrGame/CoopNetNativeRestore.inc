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
    RetainedOfflineRegion()=default;
    friend std::shared_ptr<const RetainedOfflineRegion> retain_offline_region(const coopnet::RegionKey&,double);
    friend bool restore_retained_offline_region(const std::shared_ptr<const RetainedOfflineRegion>&);
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
    if(!retained || !ai().get_alife() || world_level_is_replica() ||
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
    // Bytes are immutable, locally produced, and native-read validated during
    // capture. No network-supplied blob reaches Spawn_Read through this API.
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

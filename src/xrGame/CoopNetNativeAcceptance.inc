// Disposable fixture stimulus only. Normal gameplay never enters this path.
void exercise_native_acceptance_probe(double elapsed) {
    if(!native_world_requested() || !g_pGameLevel || !Level().bReady || !g_actor) return;
    // Capture real rendered frames for human review; these are diagnostic evidence,
    // never a substitute for gameplay or a claim of matching physics simulation.
    if(strstr(Core.Params,"-coop_native_visual_probe")) {
        static double capture_wait=0;static unsigned captures=0;
        capture_wait+=elapsed;
        if(captures<120 && capture_wait>=1) {
            capture_wait=0;++captures;
            Render->Screenshot();
            Msg("* CoopNet visual capture: frame %u role %s actor %.3f %.3f %.3f",captures,
                world_level_is_replica() ? "guest" : "host",g_actor->Position().x,g_actor->Position().y,g_actor->Position().z);
            for(const auto& binding:world_objects) {
                auto* target=smart_cast<CEntityAlive*>(const_cast<CGameObject*>(binding.first));
                if(!target || xr_strcmp(target->cName().c_str(),"coopnet_natural_target")) continue;
                Msg("* CoopNet visual target: frame %u position %.3f %.3f %.3f health %.3f dead %u",captures,
                    target->Position().x,target->Position().y,target->Position().z,target->GetfHealth(),target->AlreadyDie());
            }
        }
    }
    if(!strstr(Core.Params,"-coop_native_acceptance_probe")) return;
    if(!world_level_is_replica()) {
        static unsigned anomaly_stage=0; static double anomaly_wait=0; static u16 anomaly_id=0xffff;
        static Fmatrix anomaly_host_origin;
        bool guest_ready=false;
        for(unsigned index=0;index<Level().Objects.o_count();++index) {
            auto* actor=smart_cast<CActor*>(Level().Objects.o_get_by_iterator(index));
            if(actor && actor->is_coopnet_guest() && guest_character_identity(actor->ID())) guest_ready=true;
        }
        if(guest_ready && anomaly_stage<2) {
            anomaly_wait+=elapsed;
            if(!anomaly_stage && anomaly_wait>=20) {
                CCustomZone* source=nullptr;
                for(const auto& binding:world_objects) {
                    auto* zone=smart_cast<CCustomZone*>(const_cast<CGameObject*>(binding.first));
                    if(zone && !zone->getDestroy() && zone->GetHitType()==ALife::eHitTypeShock) {source=zone;break;}
                }
                if(!source) throw std::runtime_error("Dynamic anomaly fixture source missing");
                Fvector position; position.set(-269.5f,-9.5f,-244.3f);
                auto* abstract=Level().spawn_item(source->cNameSect().c_str(),position,3975,0xffff,true);
                auto* zone=smart_cast<CSE_ALifeCustomZone*>(abstract);
                if(!zone) {F_entity_Destroy(abstract);throw std::runtime_error("Dynamic anomaly fixture server class missing");}
                zone->set_name_replace("coopnet_dynamic_anomaly"); zone->m_bALifeControl=false;zone->m_owner_id=u32(-1);
                zone->shapes.clear();CShapeData::shape_def sphere{};sphere.type=CShapeData::cfSphere;sphere.data.sphere.P.set(0,0,0);sphere.data.sphere.R=2.5f;zone->shapes.push_back(sphere);
                NET_Packet packet;zone->Spawn_Write(packet,TRUE);u16 type;packet.r_begin(type);
                auto* created=Level().Server->Process_spawn(packet,Level().Server->GetServerClient()->ID,FALSE,nullptr,true);F_entity_Destroy(abstract);
                if(!created) throw std::runtime_error("Dynamic anomaly fixture spawn failed");
                anomaly_id=created->ID;anomaly_stage=1;anomaly_wait=0;
                if(strstr(Core.Params,"-coop_native_visual_probe")) {
                    anomaly_host_origin=g_actor->XFORM();Fmatrix observer;observer.identity();observer.c=position;observer.c.z-=4.f;
                    g_actor->ForceTransform(observer);set_local_movement_probe(0,0,0);
                }
                Msg("* CoopNet dynamic anomaly probe: host spawned native %u",anomaly_id);
            } else if(anomaly_stage==1 && anomaly_wait>=45) {
                NET_Packet packet;CGameObject::u_EventGen(packet,GE_DESTROY,anomaly_id);CGameObject::u_EventSend(packet);
                anomaly_stage=2;Msg("* CoopNet dynamic anomaly probe: host removed native %u",anomaly_id);
                if(strstr(Core.Params,"-coop_native_visual_probe")) g_actor->ForceTransform(anomaly_host_origin);
            }
        }
        static bool sent=false;static double wait=0;
        if(sent) return;
        for(unsigned i=0;i<Level().Objects.o_count();++i) {
            auto* actor=smart_cast<CActor*>(Level().Objects.o_get_by_iterator(i));
            if(!actor || !actor->is_coopnet_guest() || !guest_character_identity(actor->ID())) continue;
            wait+=elapsed;if(wait<15) return;
            Fvector direction,point;direction.set(0,0,1);point.set(0,0,0);
            SHit hit(.2f,direction,g_actor,0,point,0,ALife::eHitTypeFireWound,.5f,false);
            actor->Hit(&hit);sent=true;Msg("* CoopNet native player damage probe: host attack emitted");return;
        }
        return;
    }
    if(!native_character_ready()) return;
    static unsigned stage=0;static double wait=0;
    static u16 source_id=0xffff,ammo_id=0xffff,weapon_id=0xffff,zone_id=0xffff;
    static Fmatrix origin;static float health=1;
    if(stage>=7) return;wait+=elapsed;
    if(stage==6) {
        if(Level().Objects.net_Find(zone_id)) {
            if(wait>60) throw std::runtime_error("Dynamic anomaly guest native removal timed out");
            return;
        }
        stage=7;Msg("* CoopNet dynamic anomaly probe: guest native removal verified");return;
    }
    if(!stage) {
        if(wait<3) return;
        for(const auto& snapshot:personal_loot) {
            if(snapshot.scope!=personal_world_scope || snapshot.items.size()!=1 || snapshot.items[0].section!="wpn_pm") continue;
            for(const auto& binding:world_objects) {
                auto* source=smart_cast<CInventoryBox*>(const_cast<CGameObject*>(binding.first));
                if(!source || source->getDestroy() || snapshot.name!=source->cName().c_str()) continue;
                const auto restored=personal_loot_restores.find(source);
                if(restored==personal_loot_restores.end() || restored->second.stage!=2) return;
                if(source->m_items.size()!=1) throw std::runtime_error("Portable personal loot count changed");
                auto* weapon=smart_cast<CInventoryItem*>(Level().Objects.net_Find(source->m_items.front()));
                if(!weapon || xr_strcmp(weapon->object().cNameSect().c_str(),"wpn_pm") || fabsf(weapon->GetCondition()-.37f)>.001f)
                    throw std::runtime_error("Portable personal loot item state changed");
                Msg("* CoopNet personal loot probe: portable contents restored with exact condition");stage=3;wait=0;return;
            }
        }
        CInventoryBox* source=nullptr;
        for(const auto& binding:world_objects) {
            auto* box=smart_cast<CInventoryBox*>(const_cast<CGameObject*>(binding.first));
            if(box && !box->getDestroy() && box->m_items.empty()) {source=box;break;}
        }
        if(!source) throw std::runtime_error("Personal loot fixture empty baseline container missing");
        source_id=source->ID();source->set_in_use(true);
        for(const auto* section:{"ammo_9x18_fmj","wpn_pm"}) {
            auto* abstract=Level().spawn_item(section,source->Position(),source->ai_location().level_vertex_id(),source_id,true);
            abstract->m_bALifeControl=false;
            if(!xr_strcmp(section,"wpn_pm")) smart_cast<CSE_ALifeInventoryItem*>(abstract)->m_fCondition=.37f;
            NET_Packet packet;abstract->Spawn_Write(packet,TRUE);u16 type;packet.r_begin(type);
            auto* created=Level().Server->Process_spawn(packet,Level().Server->GetServerClient()->ID,FALSE,nullptr,true);F_entity_Destroy(abstract);
            if(!created) throw std::runtime_error("Personal loot fixture item spawn failed");
            if(!xr_strcmp(section,"wpn_pm")) weapon_id=created->ID;else ammo_id=created->ID;
        }
        stage=1;return;
    }
    if(stage==1) {
        auto* source=smart_cast<CInventoryBox*>(Level().Objects.net_Find(source_id));
        auto* ammo=Level().Objects.net_Find(ammo_id);auto* weapon=Level().Objects.net_Find(weapon_id);
        if(!source || !ammo || !weapon || ammo->H_Parent()!=source || weapon->H_Parent()!=source) return;
        NET_Packet reject;CGameObject::u_EventGen(reject,GE_OWNERSHIP_REJECT,source_id);reject.w_u16(ammo_id);CGameObject::u_EventSend(reject);
        NET_Packet take;CGameObject::u_EventGen(take,GE_OWNERSHIP_TAKE,g_actor->ID());take.w_u16(ammo_id);CGameObject::u_EventSend(take);
        stage=2;return;
    }
    if(stage==2) {
        auto* source=smart_cast<CInventoryBox*>(Level().Objects.net_Find(source_id));auto* ammo=Level().Objects.net_Find(ammo_id);
        if(!source || !ammo || ammo->H_Parent()!=g_actor || source->m_items.size()!=1) return;
        std::vector<std::uint8_t> saved;if(!capture_personal_state(saved)) return;
        PersonalState state;if(!decode_personal_state(saved,state)) throw std::runtime_error("Personal loot fixture capture decode failed");
        const auto snapshot=std::find_if(state.loot.begin(),state.loot.end(),[&](const PersonalLoot& s){return s.name==source->cName().c_str() && s.scope==personal_world_scope;});
        if(snapshot==state.loot.end() || snapshot->items.size()!=1 || snapshot->items[0].section!="wpn_pm") throw std::runtime_error("Personal loot native transfer not captured");
        source->set_in_use(false);Msg("* CoopNet personal loot probe: native transfer captured");stage=3;wait=0;return;
    }
    if(stage==3) {
        static std::uint64_t evicted_incarnation=0;
        if(wait<20) return;
        for(const auto& binding:world_objects) {
            auto* zone=smart_cast<CCustomZone*>(const_cast<CGameObject*>(binding.first));
            if(!zone || zone->getDestroy() || xr_strcmp(zone->cName().c_str(),"coopnet_dynamic_anomaly") || zone->GetHitType()!=ALife::eHitTypeShock || !zone->IsEnabled()) continue;
            if(!evicted_incarnation) {
                evicted_incarnation=binding.second.incarnation;
                NET_Packet packet;CGameObject::u_EventGen(packet,GE_DESTROY,zone->ID());CGameObject::u_EventSend(packet);
                Msg("* CoopNet dynamic anomaly probe: guest local eviction emitted");return;
            }
            if(binding.second.incarnation==evicted_incarnation) return;
            Msg("* CoopNet dynamic anomaly probe: retained lifetime recovered");
            const auto node=ai().level_graph().vertex(g_actor->ai_location().level_vertex_id(),zone->Position());
            if(!ai().level_graph().valid_vertex_id(node)) continue;
            origin=g_actor->XFORM();health=g_actor->GetfHealth();g_actor->conditions().SetHealth(1.f);
            zone_id=zone->ID();stage=4;wait=0;Msg("* CoopNet native electric probe: entering section %s",zone->cNameSect().c_str());return;
        }
        if(wait<50) return;
        throw std::runtime_error("Native electric fixture zone missing");
    }
    if(stage==4) {
        static bool damage_observed=false;
        auto* zone=smart_cast<CCustomZone*>(Level().Objects.net_Find(zone_id));if(!zone) throw std::runtime_error("Native electric fixture zone disappeared");
        if(g_actor->GetfHealth()<.999f) {
            if(!damage_observed) {damage_observed=true;Msg("* CoopNet native electric probe: local damage observed health %.3f",g_actor->GetfHealth());}
            // Keep the owner in the volume long enough for snapshots, host touch
            // processing and paired rendered captures. This is fixture-only.
            g_actor->conditions().SetHealth(1.f);
        }
        if(damage_observed && wait>=3) {
            g_actor->ForceTransform(origin);g_actor->conditions().SetHealth(health);stage=5;wait=0;return;
        }
        Fmatrix placement=g_actor->XFORM();placement.c=zone->Position();g_actor->ForceTransform(placement);set_local_movement_probe(0,0,0);
        if(wait>15) throw std::runtime_error("Native electrical collision produced no damage");return;
    }
    if(stage==5 && wait>3) {
        // Native impact effects can randomly drop the active weapon. Prepare an
        // explicit equipped exit state for the separate saved-loadout test.
        if(strstr(Core.Params,"-coop_native_combat_probe")) {
            auto* weapon=smart_cast<CWeapon*>(Level().Objects.net_Find(native_combat_fixture_weapon));
            if(!weapon || weapon->getDestroy()) throw std::runtime_error("Combat fixture weapon disappeared before saved-loadout preparation");
            if(weapon->H_Parent()!=g_actor) {
                if(weapon->H_Parent()) throw std::runtime_error("Combat fixture weapon has an unexpected owner");
                NET_Packet take;CGameObject::u_EventGen(take,GE_OWNERSHIP_TAKE,g_actor->ID());take.w_u16(weapon->ID());CGameObject::u_EventSend(take);return;
            }
            if(weapon->CurrPlace()!=eItemPlaceSlot) g_actor->inventory().Slot(weapon->BaseSlot(),weapon,true);
            if(g_actor->inventory().ActiveItem()!=weapon) {g_actor->inventory().Activate(weapon->CurrSlot(),true);return;}
            if(weapon->IsPending() || weapon->GetState()!=CWeapon::eIdle) return;
            coopnet::InventoryView saved;
            if(!capture_owned_character(saved) || !saved.active) throw std::runtime_error("Combat fixture equipped state was not captured");
            Msg("* CoopNet saved loadout fixture prepared: active %s",weapon->cNameSect().c_str());
        }
        stage=6;wait=0;Msg("* CoopNet native acceptance probes completed");
    }
}

#include "../../src/CoopNet/DedicatedBootstrap.h"
#include "../../src/CoopNet/ClientPump.h"
#include <iostream>
using namespace coopnet;
void check(bool value,int line) {if(!value)throw std::runtime_error("Dedicated simulation failure at "+std::to_string(line));}
#define require(value) check((value),__LINE__)
int main() {
    DedicatedBootstrap server;Identity token=1000;
    auto bytes=std::make_shared<const std::vector<std::uint8_t>>(std::size_t(12),std::uint8_t(42));
    server.start(900,{1,1},{800,3,12,{},700},bytes,[&]{return ++token;},[](Identity,InventoryView&){return true;});
    server.enable_experimental_simulation();
    ClientPump owner,observer;std::map<RegionKey,RegionLease> owned,observed;
    std::vector<NPCRecord> catalogue;unsigned rendered=0;
    auto connect=[&](ClientPump& client,Identity character,Connection connection) {
        auto link=MemoryTransport::pair();require(server.host().attach(connection,std::move(link.second)));
        client.start(std::move(link.first),character,{1,1});
        InventoryView view;view.actor=character;view.generation=1;view.level=1;view.revision=1;view.community="stalker";
        client.set_character_profile(view);
        client.set_baseline_sink([](const WorldBaseline&,const std::vector<std::uint8_t>&){return true;});
    };
    owner.set_region_sink([&](const Frame& frame){RegionLease lease;if(!decode_region_lease(frame.payload,lease))return false;owned[lease.key]=lease;return true;});
    observer.set_region_sink([&](const Frame& frame){RegionLease lease;if(!decode_region_lease(frame.payload,lease))return false;observed[lease.key]=lease;return true;});
    observer.set_shared_world_sink([&](SharedKind kind,std::uint32_t level,const std::vector<std::uint8_t>& data){if(kind==SharedKind::NPC){require(level==3);require(decode_npcs(data,catalogue));}});
    observer.set_world_sink([&](const WorldState& state){require(state.objects.size()==1 && state.objects[0].anchor==999);++rendered;});
    connect(owner,200,100);connect(observer,201,101);
    auto pump=[&](unsigned count=10){for(unsigned i=0;i<count;++i){owner.update(.01);observer.update(.01);server.update(.01);}};
    pump(30);require(owned.empty());
    require(owner.acknowledge_baseline() && observer.acknowledge_baseline());pump();
    require(owner.acknowledge_level(3) && observer.acknowledge_level(3));pump();
    auto input=[&](ClientPump& client,std::uint32_t sequence,float x){ActorInput value;const auto welcome=client.session().welcome();value.entity=welcome.player;value.generation=welcome.generation;value.level=3;value.sequence=sequence;value.owner_pose=true;value.position={x,0,50};require(client.send_input(value)==SendResult::Sent);};
    input(owner,1,50);pump();input(observer,1,50);pump();
    require(owned.size()==9 && observed.size()==9);
    const auto player=owner.session().welcome().player;
    const RegionKey key{3,0,0};const auto epoch=owned.at(key).epoch;
    auto state=[&](ClientPump& client,std::uint32_t sequence,Identity incarnation=7){
        RegionState value{key,epoch,{3,sequence,{{999,incarnation,{50,0,50},{},1}}}};
        require(client.send_region_frame({Message::RegionState,Channel::AI,Delivery::UnreliableSequenced,sequence,encode_region_state(value)})==SendResult::Sent);pump();
    };
    state(owner,1);require(!server.simulation()->accepted_states());
    // An observer cannot acknowledge another client's restoration.
    for(const auto& entry:observed)require(observer.send_region_frame({Message::RegionAck,Channel::World,Delivery::ReliableOrdered,0,encode_region_ack({entry.first,entry.second.epoch,1})})==SendResult::Sent);
    pump();require(owned.at(key).stage==RegionStage::Restoring);
    for(const auto& entry:owned){require(entry.second.owner==player);require(owner.send_region_frame({Message::RegionAck,Channel::World,Delivery::ReliableOrdered,0,encode_region_ack({entry.first,entry.second.epoch,1})})==SendResult::Sent);}
    pump();require(owned.at(key).stage==RegionStage::Active);
    state(owner,2);require(!server.simulation()->accepted_states());
    NPCRecord npc;npc.pose={999,7,{50,0,50},{},1};npc.section="sim_default_stalker";
    auto publish=[&](const RegionLease& lease,const std::vector<NPCRecord>& records,std::uint32_t revision=1){
        const auto payload=encode_region_catalogue(records);
        RegionChunk chunk{lease.key,lease.epoch,revision,static_cast<std::uint32_t>(payload.size()),0,payload};
        require(owner.send_region_frame({Message::RegionChunk,Channel::World,Delivery::ReliableOrdered,0,encode_region_chunk(chunk)})==SendResult::Sent);
    };
    publish(owned.at(key),{npc});pump();require(catalogue.empty());
    for(const auto& entry:owned)if(!(entry.first==key))publish(entry.second,{});
    pump(30);require(catalogue.size()==1 && server.simulation()->accepted_catalogues()==9);
    state(observer,3);require(!server.simulation()->accepted_states());
    state(owner,3,8);require(!server.simulation()->accepted_states());
    require(owner.session().state()==ClientState::Connected); // reordered/unbound state is dropped
    state(owner,3);require(server.simulation()->accepted_states()==1 && rendered==1);
    // A native planner walking across a boundary remains attached to its home
    // cell until an explicit checkpoint handoff changes simulation ownership.
    npc.pose.position[0]=800;publish(owned.at(key),{npc},2);pump();
    require(server.simulation()->accepted_catalogues()==10 && catalogue.at(0).pose.position[0]==800);
    state(owner,3);require(server.simulation()->accepted_states()==1 && rendered==1);
    // Departing cells revoke the old worker even after it leaves their interest.
    input(owner,2,3000);pump();require(owned.at(key).stage==RegionStage::Quiescing);
    state(owner,4);require(server.simulation()->accepted_states()==1);
    owner.stop();pump();require(observed.at(key).owner==observer.session().welcome().player);
    require(observed.at(key).stage==RegionStage::Restoring);
    // The original seed cannot authorize replaying stale AI after reassignment.
    const auto lease=observed.at(key);
    require(observer.send_region_frame({Message::RegionAck,Channel::World,Delivery::ReliableOrdered,0,encode_region_ack({key,lease.epoch,1})})==SendResult::Sent);
    pump();require(observed.at(key).stage==RegionStage::Restoring);
    observer.stop();server.stop();
    std::cout<<"Dedicated simulation: PASS (load barriers, 3x3 leases, catalogue aggregation, validated relay, revocation and stale seed fence)\n";
}

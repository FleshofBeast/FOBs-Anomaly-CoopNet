#include "../../src/CoopNet/HostPump.h"
#include "../../src/CoopNet/ClientPump.h"
#include <iostream>
using namespace coopnet;
void check(bool value,int line) {if(!value) {std::cerr<<"Region network failed at "<<line<<'\n';std::exit(1);}}
#define require(value) check((value),__LINE__)
int main() {
    HostPump host;ClientPump owner,observer;
    auto first=MemoryTransport::pair(),second=MemoryTransport::pair();Identity token=100;
    host.start(55,1,{1,1},[&] {return ++token;});
    require(host.attach(1,std::move(first.second)) && host.attach(2,std::move(second.second)));
    owner.start(std::move(first.first),2,{1,1});observer.start(std::move(second.first),3,{1,1});
    auto pump=[&] {host.update(.01);owner.update(.01);observer.update(.01);};
    for(unsigned n=0;n<20;++n) pump();
    const auto player=owner.session().welcome().player,other=observer.session().welcome().player;
    auto baseline=std::make_shared<std::vector<std::uint8_t>>(std::size_t(12),std::uint8_t(0));
    owner.set_baseline_sink([](const WorldBaseline&,const std::vector<std::uint8_t>&) {return true;});
    observer.set_baseline_sink([](const WorldBaseline&,const std::vector<std::uint8_t>&) {return true;});
    require(host.send_baseline(player,{71,1,12,{}},baseline));
    require(host.send_baseline(other,{72,1,12,{}},baseline));
    for(unsigned n=0;n<20;++n) pump();
    require(owner.acknowledge_baseline() && observer.acknowledge_baseline());pump();
    require(host.assign_level(player,1,81) && host.assign_level(other,1,82));pump();
    require(owner.acknowledge_level(1) && observer.acknowledge_level(1));pump();
    RegionAuthority regions(750);const auto key=regions.key_at(1,{50,0,50});
    require(regions.update_interest(player,1,{50,0,50},0,50));
    require(regions.update_interest(other,1,{50,0,50},0,50));
    auto native=std::make_shared<const std::vector<std::uint8_t>>(std::size_t(17000),std::uint8_t(42));
    require(regions.seed(key,{1,native}));
    RegionLease owner_lease,observer_lease;RegionAssembly assembly;
    unsigned rendered=0,accepted=0,stale=0;
    owner.set_region_sink([&](const Frame& frame) {
        if(frame.message==Message::RegionLease) return decode_region_lease(frame.payload,owner_lease);
        if(frame.message==Message::RegionChunk) {RegionChunk chunk;return decode_region_chunk(frame.payload,chunk) && assembly.accept(chunk);}
        return false;
    });
    observer.set_region_sink([&](const Frame& frame) {
        if(frame.message==Message::RegionLease) return decode_region_lease(frame.payload,observer_lease);
        RegionState state;if(!decode_region_state(frame.payload,state)) return false;
        if(state.epoch==observer_lease.epoch && observer_lease.stage==RegionStage::Active) ++rendered;
        return true;
    });
    host.set_region_sink([&](Identity sender,const Frame& frame) {
        if(frame.message==Message::RegionAck) {
            RegionAck ack;if(!decode_region_ack(frame.payload,ack)) return false;
            return regions.restored(sender,ack.key,ack.epoch,ack.checkpoint);
        }
        RegionState state;if(!decode_region_state(frame.payload,state) || frame.sequence!=state.world.tick) return false;
        // Native catalogue authorization is separate from ownership: an owner
        // still cannot invent arbitrary object bindings.
        if(state.world.objects.size()!=1 || state.world.objects[0].anchor!=999 || state.world.objects[0].incarnation!=7) return false;
        if(!regions.accepts(sender,state.key,state.epoch,state.world.tick)) {++stale;return true;}
        ++accepted;
        require(host.send_region_frame(other,frame)==SendResult::Sent);return true;
    });
    auto announce=[&] {
        const auto lease=*regions.lease(key);
        Frame frame{Message::RegionLease,Channel::World,Delivery::ReliableOrdered,lease.checkpoint,encode_region_lease(lease)};
        require(host.send_region_frame(player,frame)==SendResult::Sent);
        require(host.send_region_frame(other,frame)==SendResult::Sent);pump();
    };
    announce();
    RegionState state{key,owner_lease.epoch,{1,1,{{999,7,{50,0,50},{},1}}}};
    Frame publication{Message::RegionState,Channel::AI,Delivery::UnreliableSequenced,1,encode_region_state(state)};
    require(owner.send_region_frame(publication)==SendResult::Sent);pump();require(!accepted && stale==1);
    for(std::size_t offset=0;offset<native->size();offset+=region_chunk_size) {
        const auto end=(std::min)(native->size(),offset+region_chunk_size);
        RegionChunk chunk{key,owner_lease.epoch,1,static_cast<std::uint32_t>(native->size()),static_cast<std::uint32_t>(offset),{native->begin()+offset,native->begin()+end}};
        require(host.send_region_frame(player,{Message::RegionChunk,Channel::World,Delivery::ReliableOrdered,0,encode_region_chunk(chunk)})==SendResult::Sent);pump();
    }
    require(assembly.complete() && assembly.bytes()==*native);
    const auto ack=assembly.context();
    require(owner.send_region_frame({Message::RegionAck,Channel::World,Delivery::ReliableOrdered,0,encode_region_ack(ack)})==SendResult::Sent);pump();
    announce();require(observer_lease.stage==RegionStage::Active);
    require(owner.send_region_frame(publication)==SendResult::Sent);pump();require(accepted==1 && rendered==1);
    require(owner.send_region_frame(publication)==SendResult::Sent);pump();require(accepted==1 && rendered==1 && stale==2);
    state.world.tick=2;publication.sequence=2;publication.payload=encode_region_state(state);
    require(observer.send_region_frame(publication)==SendResult::Sent);pump();require(accepted==1 && stale==3);
    regions.disconnect(player);
    require(owner.send_region_frame(publication)==SendResult::Sent);pump();require(accepted==1 && stale==4);
    require(regions.lease(key)->owner==other && regions.lease(key)->stage==RegionStage::Restoring);
    // Directionality is enforced even on already admitted connections.
    require(owner.send_region_frame({Message::RegionLease,Channel::World,Delivery::ReliableOrdered,0,encode_region_lease(owner_lease)})==SendResult::Invalid);
    // A cell worker's publication channel cannot change host-directed weather,
    // time, global quest/event snapshots or the session's world rules.
    require(owner.send_region_frame({Message::WorldClock,Channel::World,Delivery::UnreliableSequenced,1,{}})==SendResult::Invalid);
    require(owner.send_region_frame({Message::SharedWorld,Channel::World,Delivery::ReliableOrdered,1,{}})==SendResult::Invalid);
    require(owner.send_region_frame({Message::WorldRules,Channel::Control,Delivery::ReliableOrdered,1,{}})==SendResult::Invalid);
    owner.stop();observer.stop();host.stop();
    std::cout<<"Region network: PASS (admitted peers, checkpoint chunks, restore barrier, state relay, stale/non-owner rejection)\n";
}

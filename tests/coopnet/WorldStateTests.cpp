#include "../../src/CoopNet/HostPump.h"
#include "../../src/CoopNet/ClientPump.h"
#include <iostream>
#include <limits>
#include "../../src/CoopNet/WorldPosePlayback.h"
using namespace coopnet;
void check(bool value,int line) { if (!value) { std::cerr<<"World state failed at "<<line<<'\n'; std::exit(1); } }
#define require(value) check((value),__LINE__)
int main() {
    WorldPosePlayback playback;
    float p[3]={0,0,0},r[3]={0,3.1f,0};
    std::array<float,3> point{},angles{};
    playback.receive(0,p,r); playback.sample(0,point,angles); require(point[0]==0);
    p[0]=2; r[1]=-3.1f; playback.receive(100,p,r); playback.sample(150,point,angles);
    require(std::abs(point[0]-1.f)<.001f && std::abs(angles[1])>3.f);
    playback.sample(300,point,angles); require(point[0]==2);
    p[0]=3; playback.receive(300,p,r); playback.sample(350,point,angles); require(point[0]==2.5f);
    p[0]=4; playback.receive(350,p,r); playback.sample(350,point,angles); require(point[0]==2.5f);
    p[0]=100; playback.receive(360,p,r); playback.sample(360,point,angles); require(point[0]==100);
    p[0]=101; playback.receive(370,p,r,true); playback.sample(370,point,angles); require(point[0]==101);
    WorldPosePlayback wrapped; p[0]=0; wrapped.receive(0xffffffce,p,r);
    p[0]=2; wrapped.receive(0xffffffce,p,r); wrapped.sample(0,point,angles); require(point[0]==1);
    std::set<Identity> anchors;
    for (unsigned id=0;id<65536;++id) require(anchors.insert(world_anchor(123,static_cast<std::uint16_t>(id))).second);
    require(!anchors.count(0) && world_anchor(123,5)!=world_anchor(124,5));
    WorldState state{10,1,{{world_anchor(123,5),7,{1,2,3},{0,1,0},.75f}}},decoded;
    auto encoded=encode_world_state(state);
    require(decode_world_state(encoded,decoded) && decoded.objects[0].health==.75f);
    for (std::size_t n=0;n<encoded.size();++n) require(!decode_world_state({encoded.begin(),encoded.begin()+n},decoded));
    encoded.push_back(0); require(!decode_world_state(encoded,decoded));
    auto invalid=state; invalid.objects.push_back(invalid.objects.front()); require(!valid_world_state(invalid));
    invalid=state; invalid.objects.front().health=std::numeric_limits<float>::quiet_NaN(); require(!valid_world_state(invalid));
    invalid=state; invalid.objects.resize(129); require(!valid_world_state(invalid));
    auto zone=state; zone.objects.front().zone_state=2; zone.objects.front().zone_time=750;
    encoded=encode_world_state(zone); require(decode_world_state(encoded,decoded));
    require(decoded.objects.front().zone_state==2 && decoded.objects.front().zone_time==750);
    invalid=zone; invalid.objects.front().zone_state=5; require(!valid_world_state(invalid));
    invalid=zone; invalid.objects.front().zone_state=255; require(!valid_world_state(invalid));
    invalid=zone; invalid.objects.front().zone_time=0xffffffff; require(!valid_world_state(invalid));
    invalid=zone; invalid.objects.front().animations.push_back({}); require(!valid_world_state(invalid));
    auto killed=state;killed.objects.front().health=0;killed.objects.front().killer=17;killed.objects.front().killer_player=true;
    require(decode_world_state(encode_world_state(killed),decoded));
    require(decoded.objects.front().killer==17 && decoded.objects.front().killer_player);
    auto ragdoll=killed;
    ragdoll.objects.front().physics.visible_bones=7;ragdoll.objects.front().physics.root=0;
    WorldPhysicsElement body;body.values[21]=body.values[25]=1;body.values[12]=3.25f;body.values[0]=4;body.enabled=true;
    ragdoll.objects.front().physics.elements.assign(3,body);
    require(decode_world_state(encode_world_state(ragdoll),decoded));
    require(decoded.objects.front().physics.elements.size()==3 && decoded.objects.front().physics.elements.front().values==body.values && decoded.objects.front().physics.elements.front().enabled);
    encoded=encode_world_state(ragdoll);
    for(std::size_t n=0;n<encoded.size();++n) require(!decode_world_state({encoded.begin(),encoded.begin()+n},decoded));
    invalid=ragdoll;invalid.objects.front().physics.elements.front().values[21]=0;require(!valid_world_state(invalid));
    invalid=ragdoll;invalid.objects.front().physics.elements.front().values[12]=std::numeric_limits<float>::infinity();require(!valid_world_state(invalid));
    invalid=ragdoll;invalid.objects.front().physics.root=64;require(!valid_world_state(invalid));
    invalid=ragdoll;invalid.objects.front().physics.elements.resize(65,body);require(!valid_world_state(invalid));
    invalid=ragdoll;invalid.objects.front().health=1;require(!valid_world_state(invalid));
    invalid=ragdoll;invalid.objects.front().physics.elements.resize(64,body);
    for(unsigned i=0;i<3;++i){auto copy=invalid.objects.front();copy.anchor+=i+1;invalid.objects.push_back(copy);}
    require(!valid_world_state(invalid)); // Oversized corpse batches must be split before transport.
    invalid=killed;invalid.objects.front().killer=0;require(!valid_world_state(invalid));
    invalid=zone;invalid.objects.front().killer=17;require(!valid_world_state(invalid));
    auto catalogue=NPCRecord{killed.objects.front(),"dog_weak","","dog_quest_target"};
    std::vector<NPCRecord> npc_roundtrip;
    require(decode_npcs(encode_npcs({catalogue}),npc_roundtrip));
    require(npc_roundtrip.front().pose.killer==17 && npc_roundtrip.front().pose.killer_player && npc_roundtrip.front().name==catalogue.name);
    catalogue.name.assign(193,'x');require(!valid_npc(catalogue));
    auto animated=state; animated.objects.front().animations={{0,2,17,.5f,1.25f,false},{3,0,41,2.f,.75f,true}};
    encoded=encode_world_state(animated);
    require(decode_world_state(encoded,decoded));
    require(decoded.objects.front().animations.size()==2);
    const auto& motion=decoded.objects.front().animations.back();
    require(motion.part==3 && motion.slot==0 && motion.motion==41 && motion.time==2.f && motion.speed==.75f && motion.stop);
    for (std::size_t n=0;n<encoded.size();++n) require(!decode_world_state({encoded.begin(),encoded.begin()+n},decoded));
    invalid=animated; invalid.objects.front().animations.back().part=0; require(!valid_world_state(invalid));
    invalid=animated; invalid.objects.front().animations.front().time=std::numeric_limits<float>::quiet_NaN(); require(!valid_world_state(invalid));
    invalid=animated; invalid.objects.front().animations.front().speed=-1.f; require(!valid_world_state(invalid));
    invalid=animated; invalid.objects.front().animations.front().slot=256; require(!valid_world_state(invalid));
    invalid=animated; invalid.objects.front().animations.front().motion=65535; require(!valid_world_state(invalid));
    encoded.back()=2; require(!decode_world_state(encoded,decoded));
    auto maximum=state; maximum.objects.clear();
    for (unsigned i=0;i<128;++i) {
        auto object=state.objects.front(); object.anchor=world_anchor(123,static_cast<std::uint16_t>(i));
        for (unsigned part=0;part<4;++part) object.animations.push_back({static_cast<std::uint8_t>(part),0,1,0.f,1.f,false});
        maximum.objects.push_back(object);
    }
    encoded=encode_world_state(maximum); require(encoded.size()<16384 && decode_world_state(encoded,decoded));
    HostPump host; ClientPump client; auto links=MemoryTransport::pair(); Identity token=10;
    host.start(123,1,{1,1},[&] { return ++token; }); require(host.attach(1,std::move(links.second)));
    client.start(std::move(links.first),2,{1,1}); auto pump=[&] { host.update(.01); client.update(.01); };
    unsigned applied=0; client.set_world_sink([&](const WorldState& value) { applied+=static_cast<unsigned>(value.objects.size()); });
    for (unsigned n=0;n<10;++n) pump();
    require(host.publish_world_state(state)); pump(); require(!applied);
    auto bytes=std::make_shared<std::vector<std::uint8_t>>(std::size_t(12),std::uint8_t(0)); WorldBaseline baseline{555,10,12,{}};
    client.set_baseline_sink([](const WorldBaseline&,const std::vector<std::uint8_t>&) { return true; });
    const auto player=client.session().welcome().player;
    require(host.send_baseline(player,baseline,bytes)); for (unsigned n=0;n<10;++n) pump();
    require(client.acknowledge_baseline()); pump();
    require(host.assign_level(player,10,666)); pump();
    require(host.publish_world_state(state)); pump(); require(!applied);
    require(client.acknowledge_level(10)); pump();
    require(host.publish_world_state(state)); pump(); require(applied==1);
    require(host.publish_world_state(state)); pump(); require(applied==1);
    state.tick=2; require(host.publish_world_state(state)); pump(); require(applied==2);
    state.tick=3; state.objects.front().incarnation=8; require(host.publish_world_state(state)); pump(); require(applied==2);
    state.level=11; state.objects.front().incarnation=7; require(host.publish_world_state(state)); pump(); require(applied==2);
    NPCRecord npc; npc.section="dog_weak"; npc.pose=state.objects.front();
    require(host.publish_shared_world(SharedKind::NPC,10,1,encode_npcs({npc}))); for (unsigned n=0;n<10;++n) pump();
    zone.tick=4; zone.objects.front().anchor=world_anchor(123,44);
    require(host.publish_world_state(zone)); pump(); require(applied==3);
    require(host.publish_world_state(zone)); pump(); require(applied==3);
    require(host.publish_shared_world(SharedKind::NPC,10,2,encode_npcs({npc}))); for (unsigned n=0;n<10;++n) pump();
    require(host.publish_world_state(zone)); pump(); require(applied==3);
    zone.tick=5; zone.objects.front().incarnation=8; require(host.publish_world_state(zone)); pump(); require(applied==3);
    zone.objects.front().incarnation=7; require(host.publish_world_state(zone)); pump(); require(applied==4);
    client.stop(); host.stop();
    std::cout<<"CoopNet NPC codec, anchor uniqueness, loading barrier, level isolation, duplicate and incarnation rejection passed\n";
}


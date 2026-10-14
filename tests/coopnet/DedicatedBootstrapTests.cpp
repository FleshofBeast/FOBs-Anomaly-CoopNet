#include "../../src/CoopNet/DedicatedBootstrap.h"
#include "../../src/CoopNet/ClientPump.h"
#include <iostream>
using namespace coopnet;
void require(bool value) {if(!value) throw std::runtime_error("Dedicated bootstrap test failed");}
InventoryView character(Identity id) {
    InventoryView view;view.actor=id;view.generation=1;view.level=1;view.revision=1;
    view.money=static_cast<std::uint32_t>(id);view.community="stalker";
    if(id==201) view.community="dolg";
    view.active=id*10;
    InventoryViewItem weapon;
    weapon.item=view.active;weapon.revision=1;weapon.section="wpn_pm";
    weapon.condition=.5f;weapon.place=2;weapon.slot=2;weapon.kind=1;weapon.ammo=7;
    view.items.push_back(weapon);
    return view;
}
int main() {
    DedicatedBootstrap server;
    auto bytes=std::make_shared<const std::vector<std::uint8_t>>(std::size_t(18000),std::uint8_t(42));
    WorldBaseline world{800,4,18000,{},700};
    Identity token=1000;unsigned checkpoints=0;
    server.start(900,{10,20},world,bytes,[&]{return ++token;},[&](Identity,const InventoryView& view) {
        ++checkpoints;return view.actor!=202;
    });
    ClientPump first,second,rejected;
    bool received_first=false,received_second=false,inventory_first=false,inventory_second=false;
    auto connect=[&](ClientPump& client,Identity id,Connection connection,const Welcome* saved=nullptr) {
        auto link=MemoryTransport::pair();require(server.host().attach(connection,std::move(link.second)));
        client.start(std::move(link.first),id,{10,20},saved);client.set_character_profile(character(id));
    };
    first.set_baseline_sink([&](const WorldBaseline& manifest,const std::vector<std::uint8_t>& data) {
        require(same_baseline(manifest,world) && data==*bytes);received_first=true;return true;
    });
    second.set_baseline_sink([&](const WorldBaseline&,const std::vector<std::uint8_t>& data) {received_second=data==*bytes;return true;});
    first.set_inventory_view_sink([&](const InventoryView& view) {
        require(view.money==200 && view.actor!=200 && view.level==4 && view.items.size()==1);
        require(view.active==2000 && view.items[0].slot==2 && view.items[0].place==2 && view.items[0].condition==.5f && view.items[0].ammo==7);
        inventory_first=true;
    });
    second.set_inventory_view_sink([&](const InventoryView& view) {require(view.money==201 && view.level==4 && view.community=="dolg");inventory_second=true;});
    connect(first,200,100);connect(second,201,101);connect(rejected,202,102);
    auto pump=[&] {first.update(.01);second.update(.01);rejected.update(.01);server.update(.01);};
    for(unsigned i=0;i<25;++i) pump();
    require(received_first && received_second && !inventory_first && !inventory_second);
    require(rejected.session().state()==ClientState::Rejected && checkpoints==3);
    require(!first.assignment().ticket && !second.assignment().ticket);
    require(first.acknowledge_baseline());
    for(unsigned i=0;i<10;++i) pump();
    require(first.assignment().ticket && !second.assignment().ticket && !inventory_first);
    require(!first.acknowledge_level(9));require(first.acknowledge_level(4));
    for(unsigned i=0;i<10;++i) pump();
    require(inventory_first && !inventory_second);
    require(second.acknowledge_baseline());for(unsigned i=0;i<10;++i) pump();
    require(second.acknowledge_level(4));for(unsigned i=0;i<10;++i) pump();
    require(inventory_second);
    const auto saved=first.session().welcome();first.stop();server.update(.01);
    received_first=false;inventory_first=false;
    connect(first,200,103,&saved);
    auto stale=character(200);stale.money=1;first.set_character_profile(stale);
    for(unsigned i=0;i<25;++i) pump();
    require(received_first && !inventory_first && checkpoints==3);
    require(first.session().welcome().generation==2 && !first.assignment().ticket);
    require(first.acknowledge_baseline());for(unsigned i=0;i<10;++i)pump();
    require(first.acknowledge_level(4));for(unsigned i=0;i<10;++i)pump();
    require(inventory_first);
    server.stop();
    std::cout<<"Dedicated canonical world streaming, independent load barriers, profile rejection and reconnect checkpoint protection passed\n";
}

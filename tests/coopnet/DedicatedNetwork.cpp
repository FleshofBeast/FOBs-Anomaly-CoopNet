#include "../../src/CoopNet/GnsTransport.h"
#include "../../src/CoopNet/HostPump.h"
#include "../../src/CoopNet/ClientPump.h"
#include <chrono>
#include <thread>
#include <iostream>
using namespace coopnet;
InventoryView fixture_character(Identity character) {
    InventoryView view;
    view.actor=character;view.generation=1;view.level=1;view.revision=1;
    view.money=static_cast<std::uint32_t>(10000+character);view.community="stalker";
    view.active=character*10;
    InventoryViewItem weapon;
    weapon.item=view.active;weapon.revision=1;weapon.section="wpn_pm";
    weapon.condition=.75f;weapon.place=2;weapon.slot=2;weapon.kind=1;weapon.ammo=7;
    view.items.push_back(weapon);
    return view;
}
std::vector<std::uint8_t> profile_bytes(const InventoryView& view) {
    InventoryViewChunk chunk;chunk.view=view;chunk.total=static_cast<std::uint16_t>(view.items.size());
    return encode_view_chunk(chunk);
}
// Explicit transport fixture, not an engine world server. No world readiness,
// native simulation, persistence or save-import success is advertised here.
int main(int argc, char** argv) {
    try {
        if (argc<3) throw std::invalid_argument("Usage: DedicatedNetwork server port | client endpoint character");
        const bool server_mode=std::string(argv[1])=="server";
        if(!server_mode && (std::string(argv[1])!="client" || argc!=4)) throw std::invalid_argument("Invalid fixture mode");
        GnsRuntime network(server_mode ? 4 : 3);
        HostPump server; ClientPump client;
        const Identity character=server_mode ? 0 : std::stoull(argv[3]);
        if(!server_mode && (character<200 || character>203)) throw std::invalid_argument("Invalid fixture character");
        Identity next_token=10000;
        std::map<Identity,Identity> imported_characters;
        if(server_mode) {
            const auto port=std::stoul(argv[2]);
            if(!port || port>65535 || !network.listen(static_cast<std::uint16_t>(port))) throw std::runtime_error("Listen failed");
            server.start_dedicated(900,{10,20},[&]{return ++next_token;});
            server.require_character_profile(true);
            server.set_character_handler([&](Identity player,const InventoryView& profile) {
                if(profile.actor<200 || profile.actor>203 || !valid_inventory_view(profile) ||
                    profile_bytes(profile)!=profile_bytes(fixture_character(profile.actor))) return false;
                imported_characters[player]=profile.actor;
                return true;
            });
            server.set_resume_character_handler([&](Identity player,Identity saved_character) {
                const auto found=imported_characters.find(player);
                return found!=imported_characters.end() && found->second==saved_character;
            });
            std::cout<<"DEDICATED_LISTENING\n"<<std::flush;
        } else {
            const auto connection=network.connect(argv[2]);
            if(connection==k_HSteamNetConnection_Invalid) throw std::runtime_error("Connect failed");
            client.start(std::make_unique<GnsTransport>(network,connection),character,{10,20});
            client.set_character_profile(fixture_character(character));
        }
        bool four_ready=false, resumed=false, reconnect_started=false;
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(25);
        auto previous=std::chrono::steady_clock::now();
        while(std::chrono::steady_clock::now()<deadline) {
            const auto now=std::chrono::steady_clock::now();
            const double elapsed=std::chrono::duration<double>(now-previous).count();previous=now;
            network.poll();
            if(server_mode) {
                for(auto pending=network.take_pending();pending!=k_HSteamNetConnection_Invalid;pending=network.take_pending())
                    if(!server.attach(pending,std::make_unique<GnsTransport>(network,pending))) throw std::runtime_error("Attach failed");
                server.update(elapsed);
                if(server.ready_participants()==4) four_ready=true;
                for(const auto& p:server.session().players()) if(p.character==200 && p.generation==2) resumed=true;
                if(four_ready && resumed && server.ready_participants()==0) {
                    if(imported_characters.size()!=4) throw std::runtime_error("Missing character imports");
                    std::cout<<"DEDICATED_NETWORK_PASS four remote clients, saved loadout profiles, no local actor, disconnect and resume\n";
                    return 0;
                }
            } else {
                client.update(elapsed);
                if(client.session().state()==ClientState::Rejected) throw std::runtime_error("Client rejected");
                if(client.roster() && client.roster()->current().session) {
                    const auto roster=client.roster()->current();
                    if(!roster.dedicated) throw std::runtime_error("Unexpected local host roster");
                    unsigned connected=0; bool generation_two=false;
                    for(const auto& p:roster.participants) {
                        if(p.player==1) throw std::runtime_error("Synthetic local player present");
                        if(p.connected) ++connected;
                        if(p.character==200 && p.generation==2) generation_two=true;
                    }
                    if(character==200 && connected==4 && !reconnect_started) {
                        const auto welcome=client.session().welcome();
                        client.stop();
                        const auto connection=network.connect(argv[2]);
                        if(connection==k_HSteamNetConnection_Invalid) throw std::runtime_error("Reconnect failed");
                        client.start(std::make_unique<GnsTransport>(network,connection),character,{10,20},&welcome);
                        reconnect_started=true;
                    } else if(roster.participants.size()==4 && generation_two) {
                        std::cout<<"DEDICATED_CLIENT_PASS character "<<character<<" observed resumed shared roster\n";
                        client.stop();return 0;
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        if(server_mode) std::cerr<<"ready="<<server.ready_participants()<<" four_ready="<<four_ready<<" resumed="<<resumed<<'\n';
        else std::cerr<<"state="<<static_cast<unsigned>(client.session().state())<<" roster="<<(client.roster()!=nullptr)<<" reconnect="<<reconnect_started<<'\n';
        std::cerr<<"transport="<<network.last_failure()<<'\n';
        throw std::runtime_error("Dedicated network fixture timed out");
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}

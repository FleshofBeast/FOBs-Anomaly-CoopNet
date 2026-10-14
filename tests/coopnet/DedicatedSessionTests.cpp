#include "../../src/CoopNet/HostPump.h"
#include "../../src/CoopNet/ClientPump.h"
#include <iostream>
using namespace coopnet;
void require(bool value) { if (!value) throw std::runtime_error("Dedicated session test failed"); }
int main() {
    HostPump server;
    Identity token = 1000;
    server.start_dedicated(900, {10,20}, [&] { return ++token; });
    require(server.session().dedicated() && server.ready_participants() == 0);
    auto empty = make_roster(server.session(), 900, 1);
    Roster decoded;
    require(empty.participants.empty() && decode_roster(encode_roster(empty), decoded) && decoded.dedicated);
    std::array<ClientPump,4> clients;
    for (unsigned i=0;i<clients.size();++i) {
        auto link = MemoryTransport::pair();
        require(server.attach(100+i, std::move(link.second)));
        clients[i].start(std::move(link.first), 200+i, {10,20});
        for (unsigned tick=0;tick<8;++tick) {
            for (unsigned c=0;c<=i;++c) clients[c].update(.01);
            server.update(.01);
        }
        require(server.ready_participants() == i+1);
    }
    for (auto& client:clients) {
        client.update(.01);
        require(client.session().state() == ClientState::Connected);
        require(client.roster() && client.roster()->current().dedicated);
        require(client.roster()->current().participants.size() == 4);
        for (const auto& p:client.roster()->current().participants) require(p.player >= 2 && p.connected);
    }
    auto extra = MemoryTransport::pair();
    require(!server.attach(104, std::move(extra.second)));
    // A dedicated roster requires its explicit mode marker; it cannot pass as
    // a valid listen-server roster with an accidentally missing host actor.
    auto wire = encode_roster(clients[0].roster()->current());
    wire.pop_back(); require(!decode_roster(wire, decoded));
    wire.push_back(2); require(!decode_roster(wire, decoded));
    // A server has no local player whose departure ends the session.
    const auto previous = clients[0].session().welcome();
    clients[0].stop(); server.update(.01);
    require(server.ready_participants() == 3 && server.session().mode() == Mode::Host);
    auto resume = MemoryTransport::pair();
    require(server.attach(105, std::move(resume.second)));
    clients[0].start(std::move(resume.first), 200, {10,20}, &previous);
    for (unsigned tick=0;tick<8;++tick) { for(auto& c:clients)c.update(.01);server.update(.01); }
    require(server.ready_participants() == 4);
    require(clients[0].session().welcome().player == previous.player);
    require(clients[0].session().welcome().generation == previous.generation+1);
    server.stop();
    require(!server.session().dedicated() && server.ready_participants() == 0);
    server.start(901, 999, {10,20}, [&] { return ++token; });
    require(!server.session().dedicated() && server.ready_participants() == 1);
    require(make_roster(server.session(),901,1).participants[0].player == 1);
    std::cout << "Dedicated admission, four remote players, roster and reconnect tests passed\n";
}

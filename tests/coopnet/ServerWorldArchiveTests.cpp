#include "../../src/CoopNet/ServerWorldArchive.h"
#include <iostream>
using namespace coopnet;
void require(bool value) {if(!value)throw std::runtime_error("Server world archive test failed");}
int main() {
    ServerWorldArchive archive;archive.build={10,20};archive.bytes.assign(120,std::uint8_t(42));
    archive.world={100,3,120,{},200};archive.world.digest[0]=99;
    auto wire=encode_server_world_archive(archive);ServerWorldArchive decoded;
    require(wire.size()==server_world_header_bytes+archive.bytes.size());
    require(decode_server_world_archive(wire,decoded) && same_baseline(decoded.world,archive.world) && decoded.bytes==archive.bytes);
    require(decoded.build.game==10 && decoded.build.mods==20);
    for(std::size_t size=0;size<wire.size();++size)require(!decode_server_world_archive({wire.begin(),wire.begin()+size},decoded));
    auto bad=wire;bad.push_back(0);require(!decode_server_world_archive(bad,decoded));
    bad=wire;bad[0]=0;require(!decode_server_world_archive(bad,decoded));
    bad=wire;for(unsigned i=4;i<12;++i)bad[i]=0;require(!decode_server_world_archive(bad,decoded));
    bad=wire;for(unsigned i=68;i<76;++i)bad[i]=0;require(!decode_server_world_archive(bad,decoded));
    require(decoded.bytes==archive.bytes); // failed parsing cannot clobber a previous valid world
    std::cout<<"Server world archive version, identity, length, truncation and atomic decode tests passed\n";
}

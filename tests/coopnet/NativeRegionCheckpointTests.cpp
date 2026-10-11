#include "../../src/CoopNet/NativeRegionCheckpoint.h"
#include <iostream>
using namespace coopnet;
void check(bool value,int line) {if(!value) {std::cerr<<"Native checkpoint failed at "<<line<<'\n';std::exit(1);}}
#define require(value) check((value),__LINE__)
int main() {
    NativeRegionCheckpoint original{{12,-2,3},{{7,0xffff,8,{1,2,3},"stalker",{4,5}},{9,7,10,std::vector<std::uint8_t>(12000,5),"wpn_pm",{6,7,8}}}},decoded;
    const auto bytes=encode_native_region_checkpoint(original);
    require(decode_native_region_checkpoint(bytes,decoded));
    require(decoded.key==original.key && decoded.objects.size()==2 && decoded.objects[1].spawn==original.objects[1].spawn);
    require(decoded.objects[0].update==original.objects[0].update && decoded.objects[1].update==original.objects[1].update);
    auto offline=original;offline.objects[0].online=false;offline.objects[1].online=false;
    require(decode_native_region_checkpoint(encode_native_region_checkpoint(offline),decoded));
    require(!decoded.objects[0].online && !decoded.objects[1].online);
    require(decode_native_region_checkpoint(bytes,decoded));
    for(std::size_t length=0;length<bytes.size();++length) {
        require(!decode_native_region_checkpoint(std::vector<std::uint8_t>(bytes.begin(),bytes.begin()+length),decoded));
        require(decoded.objects.size()==2);
    }
    auto corrupt=bytes;corrupt.push_back(0);require(!decode_native_region_checkpoint(corrupt,decoded));
    corrupt=bytes;corrupt[0]=0;require(!decode_native_region_checkpoint(corrupt,decoded));
    corrupt=bytes;corrupt[3]='1';require(!decode_native_region_checkpoint(corrupt,decoded));
    corrupt=bytes;corrupt[3]='2';require(!decode_native_region_checkpoint(corrupt,decoded));
    corrupt=bytes;corrupt[38]=2;require(!decode_native_region_checkpoint(corrupt,decoded));
    auto bad=original;bad.objects[1].parent=9;require(!valid_native_region_checkpoint(bad));
    bad=original;bad.objects[1].parent=11;require(!valid_native_region_checkpoint(bad));
    bad=original;bad.objects[1].object=7;require(!valid_native_region_checkpoint(bad));
    bad=original;bad.objects[0].incarnation=0;require(!valid_native_region_checkpoint(bad));
    bad=original;bad.objects[1].spawn.resize(16385);require(!valid_native_region_checkpoint(bad));
    bad=original;bad.objects[1].spawn.clear();require(!valid_native_region_checkpoint(bad));
    bad=original;bad.objects[1].update.clear();require(!valid_native_region_checkpoint(bad));
    bad=original;bad.objects[1].update.resize(16385);require(!valid_native_region_checkpoint(bad));
    bad=original;bad.objects[1].section="../wpn_pm";require(!valid_native_region_checkpoint(bad));
    bad=original;bad.objects[1].section="ammo_5.45x39_ap";
    NativeRegionCheckpoint ammunition;
    require(decode_native_region_checkpoint(encode_native_region_checkpoint(bad),ammunition));
    require(ammunition.objects[1].section=="ammo_5.45x39_ap");
    bad=original;bad.objects[1].section="ammo..ap";require(!valid_native_region_checkpoint(bad));
    bad=original;bad.objects[1].section=".ammo";require(!valid_native_region_checkpoint(bad));
    bad=original;bad.objects[1].section="ammo\\ap";require(!valid_native_region_checkpoint(bad));
    bad=original;bad.objects[1].section.clear();require(!valid_native_region_checkpoint(bad));
    NativeRegionCheckpoint empty{{12,0,0},{}};require(decode_native_region_checkpoint(encode_native_region_checkpoint(empty),decoded));
    NativeRegionCheckpoint large{{12,0,0},{}};
    for(unsigned i=0;i<600;++i) large.objects.push_back({static_cast<std::uint16_t>(i),0xffff,1,std::vector<std::uint8_t>(16384,7),"stalker",{1,2}});
    require(!valid_native_region_checkpoint(large));
    std::cout<<"Native region checkpoint tests passed\n";
}

#include "../../src/CoopNet/RegionMessages.h"
#include <iostream>
using namespace coopnet;
void check(bool value,int line) {if(!value) {std::cerr<<"Region authority failed at "<<line<<'\n';std::exit(1);}}
#define require(value) check((value),__LINE__)
RegionCheckpoint state(std::uint32_t revision) {return {revision,std::make_shared<const std::vector<std::uint8_t>>(32,static_cast<std::uint8_t>(revision))};}
int main() {
    RegionAuthority grid(750);
    require(grid.update_neighborhood(21,1,{375,0,375}) && grid.interest_count(21)==9);
    for(std::int32_t x=-1;x<=1;++x) for(std::int32_t z=-1;z<=1;++z) require(grid.interested(21,{1,x,z}));
    require(!grid.interested(21,{1,2,0}) && !grid.interested(21,{2,0,0}));
    require(grid.update_neighborhood(22,1,{375,0,375}) && grid.interest_count(22)==9);
    const RegionKey corner{1,-1,-1};
    require(grid.seed(corner,state(1)));
    const auto original=*grid.lease(corner);
    require(grid.restored(21,corner,original.epoch,1));
    require(grid.update_neighborhood(21,1,{749.99f,0,375}));
    require(grid.lease(corner)->epoch==original.epoch && grid.interest_count(21)==9);
    const auto retained=*grid.lease({1,0,0});
    require(grid.update_neighborhood(21,1,{750,0,375}) && grid.interest_count(21)==9 && grid.interested(21,corner));
    require(grid.update_neighborhood(21,1,{843.75f,0,375}) && grid.interested(21,corner));
    require(grid.update_neighborhood(21,1,{843.76f,0,375}) && grid.interest_count(21)==9);
    require(!grid.interested(21,corner) && grid.interested(21,{1,2,1}));
    require(grid.lease({1,0,0})->epoch==retained.epoch);
    require(grid.lease(corner)->stage==RegionStage::Quiescing);
    require(grid.quiesced(21,corner,grid.lease(corner)->epoch,state(2)) && grid.lease(corner)->owner==22);
    require(grid.update_neighborhood(21,2,{-1,0,-1}) && grid.interest_count(21)==9);
    require(grid.interested(21,{2,-2,-2}) && !grid.interested(21,{1,0,0}));
    require(!grid.update_neighborhood(21,2,{std::numeric_limits<float>::infinity(),0,0}) && grid.interest_count(21)==9);
    grid.disconnect(21);require(grid.interest_count(21)==0 && grid.interest_count(22)==9);
    RegionAuthority authority;
    const RegionKey key=region_at(1,{20,0,20});
    require(region_at(1,{-1,0,-1}).x==-1);
    require(!(region_at(1,{20,0,20})==region_at(2,{20,0,20})));
    require(authority.update_interest(11,1,{20,0,20},0,50));
    require(authority.lease(key)->stage==RegionStage::Dormant);
    require(!authority.accepts(11,key,authority.lease(key)->epoch,1));
    require(authority.seed(key,state(1)));
    const auto first=*authority.lease(key);
    require(first.owner==11 && first.stage==RegionStage::Restoring);
    require(!authority.restored(12,key,first.epoch,1));
    require(!authority.restored(11,key,first.epoch,2));
    require(authority.restored(11,key,first.epoch,1));
    require(authority.accepts(11,key,first.epoch,0xfffffffe));
    require(authority.accepts(11,key,first.epoch,0xffffffff));
    require(authority.accepts(11,key,first.epoch,0));
    require(!authority.accepts(11,key,first.epoch,0xffffffff));
    require(authority.update_interest(12,1,{20,0,20},0,50));
    require(authority.lease(key)->owner==11);
    require(!authority.accepts(12,key,first.epoch,1));
    // Retain interest outside the entry radius until the exit radius is crossed.
    require(authority.update_interest(11,1,{764,0,20},0,50));
    require(authority.lease(key)->owner==11);
    require(authority.save(11,key,first.epoch,state(2)));
    require(!authority.save(11,key,first.epoch,state(2)));
    require(authority.update_interest(11,1,{900,0,20},0,50));
    const auto revoke=*authority.lease(key);
    require(revoke.stage==RegionStage::Quiescing && revoke.previous==11 && revoke.owner==0);
    require(!authority.accepts(11,key,first.epoch,1));
    require(!authority.restored(12,key,revoke.epoch,2));
    require(!authority.quiesced(12,key,revoke.epoch,state(3)));
    require(!authority.quiesced(11,key,first.epoch,state(3)));
    require(authority.quiesced(11,key,revoke.epoch,state(3)));
    require(authority.lease(key)->owner==12 && authority.lease(key)->stage==RegionStage::Restoring);
    require(authority.restored(12,key,revoke.epoch,3));
    require(authority.accepts(12,key,revoke.epoch,1));
    require(!authority.accepts(12,key,revoke.epoch,1));
    require(authority.update_interest(13,1,{20,0,20},0,50));
    authority.disconnect(12);
    const auto crash=*authority.lease(key);
    require(crash.owner==13 && crash.epoch!=revoke.epoch && crash.stage==RegionStage::Restoring);
    require(!authority.accepts(12,key,revoke.epoch,2));
    require(authority.restored(13,key,crash.epoch,3));
    // A departing successor cannot acknowledge after a second transfer.
    require(authority.update_interest(14,1,{20,0,20},0,50));
    authority.disconnect(13);
    const auto successor=*authority.lease(key);
    authority.disconnect(14);
    require(!authority.restored(14,key,successor.epoch,3));
    require(authority.lease(key)->stage==RegionStage::Dormant);
    require(authority.checkpoint(key)->revision==3);
    require(authority.update_interest(15,1,{20,0,20},0,50));
    require(authority.restored(15,key,authority.lease(key)->epoch,3));
    // No owner shutdown acknowledgement is required after actual disconnection.
    require(authority.update_interest(16,1,{20,0,20},0,50));
    require(authority.update_interest(15,2,{20,0,20},0,50));
    require(authority.lease(key)->stage==RegionStage::Quiescing);
    authority.disconnect(15);
    require(authority.lease(key)->owner==16 && authority.lease(key)->stage==RegionStage::Restoring);
    const auto before=*authority.lease(key);
    require(!authority.update_interest(16,1,{0,0,0},200,100));
    require(authority.lease(key)->epoch==before.epoch && authority.interested(16,key));
    require(!authority.update_interest(16,1,{std::numeric_limits<float>::quiet_NaN(),0,0},0,50));
    require(!authority.seed(key,state(4)));
    RegionAuthority small(250);
    require(small.width()==250 && small.key_at(1,{251,0,-1}).x==1);
    require(authority.width()==750 && authority.key_at(1,{751,0,-1}).x==1);
    RegionAuthority tiny(10);
    require(!tiny.update_interest(1,1,{0,0,0},4096,4096) && tiny.leases().empty());
    require(tiny.update_interest(1,1,{1,0,1},0,1));
    const auto tiny_key=tiny.key_at(1,{1,0,1});
    auto mutable_capture=std::make_shared<std::vector<std::uint8_t>>(std::size_t(8),std::uint8_t(9));
    require(tiny.seed(tiny_key,{1,mutable_capture}));
    mutable_capture->assign(8,0);
    require(tiny.checkpoint(tiny_key)->bytes->front()==9);
    RegionLease decoded;
    const auto encoded=encode_region_lease(*authority.lease(key));
    require(decode_region_lease(encoded,decoded) && decoded.key==key && decoded.epoch==authority.lease(key)->epoch);
    for(std::size_t n=0;n<encoded.size();++n) require(!decode_region_lease({encoded.begin(),encoded.begin()+n},decoded));
    RegionAck ack{key,decoded.epoch,3},ack_roundtrip;
    require(decode_region_ack(encode_region_ack(ack),ack_roundtrip) && ack_roundtrip.checkpoint==3);
    std::vector<std::uint8_t> native(20000);
    for(std::size_t n=0;n<native.size();++n) native[n]=static_cast<std::uint8_t>(n);
    RegionAssembly assembly;std::vector<RegionChunk> chunks;
    for(std::size_t offset=0;offset<native.size();offset+=region_chunk_size) {
        const auto end=(std::min)(native.size(),offset+region_chunk_size);
        RegionChunk chunk{key,decoded.epoch,3,static_cast<std::uint32_t>(native.size()),static_cast<std::uint32_t>(offset),{native.begin()+offset,native.begin()+end}},roundtrip;
        require(decode_region_chunk(encode_region_chunk(chunk),roundtrip) && roundtrip.bytes==chunk.bytes);chunks.push_back(std::move(chunk));
    }
    require(assembly.accept(chunks[2]) && !assembly.complete());
    require(assembly.accept(chunks[0]) && assembly.accept(chunks[0]));
    auto corrupted=chunks[0];corrupted.bytes[0]^=1;require(!assembly.accept(corrupted));
    corrupted=chunks[1];++corrupted.epoch;require(!assembly.accept(corrupted));
    require(assembly.accept(chunks[1]) && assembly.complete() && assembly.bytes()==native);
    corrupted=chunks[1];corrupted.offset=1;require(!valid_region_chunk(corrupted));
    corrupted=chunks[1];corrupted.total=region_checkpoint_limit+1;require(!valid_region_chunk(corrupted));
    RegionState publication{key,decoded.epoch,{1,9,{{77,88,{20,0,20},{},1}}}},publication_roundtrip;
    require(decode_region_state(encode_region_state(publication),publication_roundtrip) && publication_roundtrip.world.tick==9);
    publication.key.level=2;
    bool rejected=false;try {encode_region_state(publication);} catch(const std::invalid_argument&) {rejected=true;}
    require(rejected);
    std::cout<<"Region authority: PASS (ownership, overlap, handoff barriers, crash recovery, stale epochs, sequence wrap, levels, hysteresis)\n";
}

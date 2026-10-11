#pragma once
#include "RegionAuthority.h"
#include "WorldState.h"
namespace coopnet {
inline bool valid_region(const RegionKey& key) {return key.level && key.x>=-100001 && key.x<=100000 && key.z>=-100001 && key.z<=100000;}
inline void write_region(Writer& writer,const RegionKey& key) {
    writer.integer(key.level,4);writer.integer(static_cast<std::uint32_t>(key.x),4);writer.integer(static_cast<std::uint32_t>(key.z),4);
}
inline bool read_region(Reader& reader,RegionKey& key) {
    std::uint64_t level,x,z;
    if(!reader.integer(level,4) || !reader.integer(x,4) || !reader.integer(z,4)) return false;
    key.level=static_cast<std::uint32_t>(level);
    const auto raw_x=static_cast<std::uint32_t>(x),raw_z=static_cast<std::uint32_t>(z);
    std::memcpy(&key.x,&raw_x,4);std::memcpy(&key.z,&raw_z,4);return valid_region(key);
}
inline bool valid_region_lease(const RegionLease& lease) {
    if(!valid_region(lease.key) || !lease.epoch) return false;
    switch(lease.stage) {
    case RegionStage::Dormant:return !lease.previous;
    case RegionStage::Quiescing:return !lease.owner && lease.previous && lease.checkpoint;
    case RegionStage::Restoring:
    case RegionStage::Active:return lease.owner && !lease.previous && lease.checkpoint;
    }
    return false;
}
inline std::vector<std::uint8_t> encode_region_lease(const RegionLease& lease) {
    if(!valid_region_lease(lease)) throw std::invalid_argument("Invalid region lease");
    Writer writer;write_region(writer,lease.key);writer.integer(lease.epoch,8);writer.integer(lease.owner,8);
    writer.integer(lease.previous,8);writer.integer(lease.checkpoint,4);writer.integer(static_cast<unsigned>(lease.stage),1);return writer.bytes;
}
inline bool decode_region_lease(const std::vector<std::uint8_t>& bytes,RegionLease& output) {
    Reader reader(bytes);RegionLease lease;std::uint64_t revision,stage;
    if(!read_region(reader,lease.key) || !reader.integer(lease.epoch,8) || !reader.integer(lease.owner,8) ||
        !reader.integer(lease.previous,8) || !reader.integer(revision,4) || !reader.integer(stage,1) || stage>3 || reader.remaining()) return false;
    lease.checkpoint=static_cast<std::uint32_t>(revision);lease.stage=static_cast<RegionStage>(stage);
    if(!valid_region_lease(lease)) return false;output=lease;return true;
}
struct RegionAck {RegionKey key;Identity epoch=0;std::uint32_t checkpoint=0;};
inline std::vector<std::uint8_t> encode_region_ack(const RegionAck& ack) {
    if(!valid_region(ack.key) || !ack.epoch || !ack.checkpoint) throw std::invalid_argument("Invalid region acknowledgement");
    Writer writer;write_region(writer,ack.key);writer.integer(ack.epoch,8);writer.integer(ack.checkpoint,4);return writer.bytes;
}
inline bool decode_region_ack(const std::vector<std::uint8_t>& bytes,RegionAck& output) {
    Reader reader(bytes);RegionAck ack;std::uint64_t revision;
    if(!read_region(reader,ack.key) || !reader.integer(ack.epoch,8) || !ack.epoch || !reader.integer(revision,4) || !revision || reader.remaining()) return false;
    ack.checkpoint=static_cast<std::uint32_t>(revision);output=ack;return true;
}
constexpr std::uint32_t region_checkpoint_limit=8*1024*1024;
constexpr std::uint32_t region_chunk_size=8192;
struct RegionChunk {
    RegionKey key;Identity epoch=0;
    std::uint32_t revision=0,total=0,offset=0;
    std::vector<std::uint8_t> bytes;
};
inline bool valid_region_chunk(const RegionChunk& chunk) {
    return valid_region(chunk.key) && chunk.epoch && chunk.revision && chunk.total && chunk.total<=region_checkpoint_limit &&
        chunk.offset<chunk.total && chunk.offset%region_chunk_size==0 &&
        chunk.bytes.size()==(std::min)(region_chunk_size,chunk.total-chunk.offset);
}
inline std::vector<std::uint8_t> encode_region_chunk(const RegionChunk& chunk) {
    if(!valid_region_chunk(chunk)) throw std::invalid_argument("Invalid region checkpoint chunk");
    Writer writer;write_region(writer,chunk.key);writer.integer(chunk.epoch,8);writer.integer(chunk.revision,4);
    writer.integer(chunk.total,4);writer.integer(chunk.offset,4);writer.bytes.insert(writer.bytes.end(),chunk.bytes.begin(),chunk.bytes.end());return writer.bytes;
}
inline bool decode_region_chunk(const std::vector<std::uint8_t>& bytes,RegionChunk& output) {
    Reader reader(bytes);RegionChunk chunk;std::uint64_t revision,total,offset;
    if(!read_region(reader,chunk.key) || !reader.integer(chunk.epoch,8) || !reader.integer(revision,4) ||
        !reader.integer(total,4) || !reader.integer(offset,4)) return false;
    chunk.revision=static_cast<std::uint32_t>(revision);chunk.total=static_cast<std::uint32_t>(total);chunk.offset=static_cast<std::uint32_t>(offset);
    if(reader.remaining()>region_chunk_size) return false;
    chunk.bytes.assign(bytes.end()-reader.remaining(),bytes.end());if(!valid_region_chunk(chunk)) return false;output=std::move(chunk);return true;
}
class RegionAssembly {
    RegionAck context_;
    std::uint32_t total_=0,received_=0;
    std::vector<std::uint8_t> bytes_;
    std::vector<bool> chunks_;
public:
    void clear() {*this=RegionAssembly{};}
    bool accept(const RegionChunk& chunk) {
        if(!valid_region_chunk(chunk)) return false;
        if(!total_) {
            context_={chunk.key,chunk.epoch,chunk.revision};total_=chunk.total;bytes_.resize(total_);
            chunks_.resize((total_+region_chunk_size-1)/region_chunk_size);
        }
        if(!(chunk.key==context_.key) || chunk.epoch!=context_.epoch || chunk.revision!=context_.checkpoint || chunk.total!=total_) return false;
        const auto index=chunk.offset/region_chunk_size;
        if(chunks_[index]) return std::equal(chunk.bytes.begin(),chunk.bytes.end(),bytes_.begin()+chunk.offset);
        std::copy(chunk.bytes.begin(),chunk.bytes.end(),bytes_.begin()+chunk.offset);chunks_[index]=true;
        received_+=static_cast<std::uint32_t>(chunk.bytes.size());return true;
    }
    bool complete() const {return total_ && total_==received_;}
    const RegionAck& context() const {return context_;}
    const std::vector<std::uint8_t>& bytes() const {return bytes_;}
};
struct RegionState {RegionKey key;Identity epoch=0;WorldState world;};
inline std::vector<std::uint8_t> encode_region_state(const RegionState& state) {
    if(!valid_region(state.key) || !state.epoch || state.world.level!=state.key.level) throw std::invalid_argument("Invalid region state");
    const auto payload=encode_world_state(state.world);
    if(payload.size()+20>max_payload-16) throw std::length_error("Region world payload limit");
    Writer writer;write_region(writer,state.key);writer.integer(state.epoch,8);writer.bytes.insert(writer.bytes.end(),payload.begin(),payload.end());return writer.bytes;
}
inline bool decode_region_state(const std::vector<std::uint8_t>& bytes,RegionState& output) {
    Reader reader(bytes);RegionState state;
    if(bytes.size()>max_payload-16 || !read_region(reader,state.key) || !reader.integer(state.epoch,8) || !state.epoch ||
        !decode_world_state({bytes.end()-reader.remaining(),bytes.end()},state.world) || state.world.level!=state.key.level) return false;
    output=std::move(state);return true;
}
}

#pragma once
#include "Session.h"
#include "WorldBaseline.h"
namespace coopnet {
struct ServerWorldArchive {
    BuildIdentity build;
    WorldBaseline world;
    std::vector<std::uint8_t> bytes;
};
constexpr std::size_t server_world_header_bytes=76;
inline bool valid_server_world_archive(const ServerWorldArchive& archive) {
    return archive.build.game && archive.build.mods && valid_baseline(archive.world) && archive.world.scope &&
        archive.bytes.size()==archive.world.size;
}
// Administrative disk format only. The reader must also verify SHA-256 before
// use. It contains no local paths and is never accepted from a network peer.
inline std::vector<std::uint8_t> encode_server_world_archive(const ServerWorldArchive& archive) {
    if(!valid_server_world_archive(archive)) throw std::invalid_argument("Invalid server world archive");
    Writer writer;writer.integer(0x31574e43,4);writer.integer(archive.build.game,8);writer.integer(archive.build.mods,8);
    const auto manifest=encode_baseline(archive.world);
    writer.bytes.insert(writer.bytes.end(),manifest.begin(),manifest.end());
    writer.bytes.insert(writer.bytes.end(),archive.bytes.begin(),archive.bytes.end());
    return writer.bytes;
}
inline bool decode_server_world_archive(const std::vector<std::uint8_t>& bytes,ServerWorldArchive& output) {
    if(bytes.size()<server_world_header_bytes || bytes.size()>max_baseline_bytes+server_world_header_bytes) return false;
    Reader reader(bytes);std::uint64_t magic;ServerWorldArchive archive;
    if(!reader.integer(magic,4) || magic!=0x31574e43 || !reader.integer(archive.build.game,8) || !reader.integer(archive.build.mods,8) ||
        !decode_baseline({bytes.begin()+20,bytes.begin()+server_world_header_bytes},archive.world) ||
        bytes.size()-server_world_header_bytes!=archive.world.size) return false;
    archive.bytes.assign(bytes.begin()+server_world_header_bytes,bytes.end());
    if(!valid_server_world_archive(archive)) return false;
    output=std::move(archive);return true;
}
}

#pragma once
#include "RegionMessages.h"
#include "SharedWorld.h"
namespace coopnet {
// Worker -> coordinator RegionChunk payload. Distinct from opaque native
// checkpoint packets: this contains bounded, validated entity records only.
inline std::vector<std::uint8_t> encode_region_catalogue(const std::vector<NPCRecord>& records) {
    const auto payload=encode_npcs(records);Writer header;header.integer(0x31434e57,4);
    header.bytes.insert(header.bytes.end(),payload.begin(),payload.end());return header.bytes;
}
inline bool decode_region_catalogue(const std::vector<std::uint8_t>& bytes,std::vector<NPCRecord>& records) {
    if(bytes.size()<4 || bytes.size()>shared_limit+4) return false;
    Reader reader(bytes);std::uint64_t magic;
    return reader.integer(magic,4) && magic==0x31434e57 && decode_npcs({bytes.begin()+4,bytes.end()},records);
}
}

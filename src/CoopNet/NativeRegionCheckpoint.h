#pragma once
#include "RegionMessages.h"
#include <set>
namespace coopnet {
// Native spawn records contain server state and the stock net_Save client data.
// Validation here checks the envelope only; restore must validate native classes,
// map bindings and canonical identities against the host catalogue first.
struct NativeRegionObject {
    std::uint16_t object=0xffff,parent=0xffff;
    Identity incarnation=0;
    std::vector<std::uint8_t> spawn;
    std::string section;
    std::vector<std::uint8_t> update;
    bool online=true;
};
struct NativeRegionCheckpoint {
    RegionKey key;
    std::vector<NativeRegionObject> objects;
};
inline bool valid_native_region_checkpoint(const NativeRegionCheckpoint& checkpoint) {
    if(!valid_region(checkpoint.key) || checkpoint.objects.size()>4096) return false;
    std::set<std::uint16_t> seen;
    std::size_t size=18;
    for(const auto& object:checkpoint.objects) {
        if(object.object==0xffff || !object.incarnation || object.spawn.empty() || object.spawn.size()>16384 ||
            object.update.empty() || object.update.size()>16384 ||
            object.section.empty() || object.section.size()>128 ||
            seen.count(object.object) || (object.parent!=0xffff && !seen.count(object.parent))) return false;
        if(object.section.front()=='.' || object.section.find("..")!=std::string::npos) return false;
        for(unsigned char c:object.section)
            if(!((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_' || c=='-' || c=='.')) return false;
        seen.insert(object.object);
        size+=22+object.section.size()+object.spawn.size()+object.update.size();
        if(size>region_checkpoint_limit) return false;
    }
    return true;
}
inline std::vector<std::uint8_t> encode_native_region_checkpoint(const NativeRegionCheckpoint& checkpoint) {
    if(!valid_native_region_checkpoint(checkpoint)) throw std::invalid_argument("Invalid native region checkpoint");
    std::vector<std::uint8_t> bytes;
    auto integer=[&](std::uint64_t value,unsigned width) {
        for(unsigned i=0;i<width;++i) bytes.push_back(static_cast<std::uint8_t>(value>>(8*i)));
    };
    integer(0x3343524e,4); // NRC3: native spawn/update plus online/offline state.
    integer(checkpoint.key.level,4);integer(static_cast<std::uint32_t>(checkpoint.key.x),4);
    integer(static_cast<std::uint32_t>(checkpoint.key.z),4);integer(checkpoint.objects.size(),2);
    for(const auto& object:checkpoint.objects) {
        integer(object.object,2);integer(object.parent,2);integer(object.incarnation,8);integer(object.spawn.size(),4);
        integer(object.update.size(),4);
        integer(object.online ? 1 : 0,1);
        integer(object.section.size(),1);bytes.insert(bytes.end(),object.section.begin(),object.section.end());
        bytes.insert(bytes.end(),object.spawn.begin(),object.spawn.end());
        bytes.insert(bytes.end(),object.update.begin(),object.update.end());
    }
    return bytes;
}
inline bool decode_native_region_checkpoint(const std::vector<std::uint8_t>& bytes,NativeRegionCheckpoint& output) {
    if(bytes.size()>region_checkpoint_limit) return false;
    Reader reader(bytes);NativeRegionCheckpoint checkpoint;std::uint64_t magic,count;
    if(!reader.integer(magic,4) || magic!=0x3343524e || !read_region(reader,checkpoint.key) ||
        !reader.integer(count,2) || count>4096) return false;
    for(std::uint64_t i=0;i<count;++i) {
        NativeRegionObject object;std::uint64_t id,parent,length,update_length,online,section;
        if(!reader.integer(id,2) || !reader.integer(parent,2) || !reader.integer(object.incarnation,8) ||
            !reader.integer(length,4) || !length || length>16384 ||
            !reader.integer(update_length,4) || !update_length || update_length>16384 ||
            !reader.integer(online,1) || online>1 || !reader.integer(section,1) ||
            !section || section>128 || section+length+update_length>reader.remaining()) return false;
        object.object=static_cast<std::uint16_t>(id);object.parent=static_cast<std::uint16_t>(parent);
        object.online=online!=0;
        for(std::uint64_t j=0;j<section;++j) {std::uint64_t value;if(!reader.integer(value,1)) return false;object.section.push_back(static_cast<char>(value));}
        object.spawn.reserve(static_cast<std::size_t>(length));
        for(std::uint64_t j=0;j<length;++j) {std::uint64_t value;if(!reader.integer(value,1)) return false;object.spawn.push_back(static_cast<std::uint8_t>(value));}
        object.update.reserve(static_cast<std::size_t>(update_length));
        for(std::uint64_t j=0;j<update_length;++j) {std::uint64_t value;if(!reader.integer(value,1)) return false;object.update.push_back(static_cast<std::uint8_t>(value));}
        checkpoint.objects.push_back(std::move(object));
    }
    if(reader.remaining() || !valid_native_region_checkpoint(checkpoint)) return false;
    output=std::move(checkpoint);return true;
}
}

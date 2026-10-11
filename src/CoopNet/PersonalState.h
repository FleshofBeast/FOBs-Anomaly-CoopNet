#pragma once
#include "SharedWorld.h"
#include <tuple>
namespace engine_coopnet {
struct PersonalLootItem {std::string section;std::vector<std::uint8_t> spawn;};
inline bool personal_loot_section(const std::string& section,std::size_t limit) {
    // Native ammunition sections include decimal calibres, not filesystem paths.
    return coopnet::shared_name(section,limit,true) && section.find('/')==std::string::npos && section.find('\\')==std::string::npos;
}
struct PersonalLoot {
    std::uint64_t scope=0;std::uint32_t level=0;
    std::string name,section;
    std::vector<PersonalLootItem> items;
};
// Local character data only. Never accept native tasks or Lua state from peers.
struct PersonalState {
    std::uint64_t clock=0;
    std::string active;
    std::vector<std::string> infos;
    std::vector<std::uint8_t> tasks,script;
    std::vector<PersonalLoot> loot;
};
inline std::vector<std::uint8_t> encode_personal_state(const PersonalState& s) {
    if(s.infos.size()>32768 || s.tasks.empty() || s.tasks.size()>512*1024 || s.script.size()>512*1024)
        throw std::length_error("Personal state limit");
    if(s.loot.size()>4096) throw std::length_error("Personal loot limit");
    coopnet::SharedWriter w; w.integer(2,4); w.integer(s.clock,8); w.string(s.active); w.integer(s.infos.size(),4);
    for(const auto& info:s.infos) w.string(info);
    for(const auto* blob:{&s.tasks,&s.script}) {w.integer(blob->size(),4);for(auto b:*blob) w.integer(b,1);}
    w.integer(s.loot.size(),2);
    for(const auto& source:s.loot) {
        if(!source.scope || !source.level || source.name.empty() || !personal_loot_section(source.section,96) || source.items.size()>256) throw std::invalid_argument("Personal loot source");
        w.integer(source.scope,8);w.integer(source.level,4);w.string(source.name);w.string(source.section);w.integer(source.items.size(),2);
        for(const auto& item:source.items) {
            if(!personal_loot_section(item.section,128) || item.spawn.empty() || item.spawn.size()>=16384) throw std::invalid_argument("Personal loot item: "+item.section+" bytes "+std::to_string(item.spawn.size()));
            w.string(item.section);w.integer(item.spawn.size(),2);for(auto b:item.spawn) w.integer(b,1);
        }
    }
    return w.bytes;
}
inline bool decode_personal_state(const std::vector<std::uint8_t>& bytes,PersonalState& output) {
    if(bytes.size()>coopnet::shared_limit) return false;
    coopnet::Reader r(bytes); PersonalState s; std::uint64_t n,c;
    if(!r.integer(n,4) || (n!=1 && n!=2)) return false;const bool modern=n==2;
    if(!r.integer(s.clock,8) || !coopnet::shared_string(r,s.active,4096) || !r.integer(n,4) || n>32768) return false;
    for(unsigned i=0;i<n;++i) {std::string info;if(!coopnet::shared_string(r,info,4096) || info.empty()) return false;s.infos.push_back(std::move(info));}
    for(auto* blob:{&s.tasks,&s.script}) {
        if(!r.integer(n,4) || n>512*1024 || n>r.remaining()) return false;
        blob->reserve(static_cast<std::size_t>(n));
        for(unsigned i=0;i<n;++i) {if(!r.integer(c,1)) return false;blob->push_back(static_cast<std::uint8_t>(c));}
    }
    if(modern) {
        if(!r.integer(n,2) || n>4096) return false;const auto count=n;
        std::set<std::tuple<std::uint64_t,std::uint32_t,std::string,std::string>> unique;
        for(unsigned i=0;i<count;++i) {
            PersonalLoot source;
            if(!r.integer(source.scope,8) || !source.scope || !r.integer(n,4) || !n) return false;source.level=static_cast<std::uint32_t>(n);
            if(!coopnet::shared_string(r,source.name,192) || source.name.empty() || !coopnet::shared_string(r,source.section,96) || !personal_loot_section(source.section,96) ||
                !unique.emplace(source.scope,source.level,source.name,source.section).second || !r.integer(n,2) || n>256) return false;
            const auto items=n;
            for(unsigned j=0;j<items;++j) {
                PersonalLootItem item;
                if(!coopnet::shared_string(r,item.section,128) || !personal_loot_section(item.section,128) || !r.integer(n,2) || !n || n>=16384 || n>r.remaining()) return false;
                const auto length=n;for(unsigned k=0;k<length;++k) {if(!r.integer(c,1)) return false;item.spawn.push_back(static_cast<std::uint8_t>(c));}
                source.items.push_back(std::move(item));
            }
            s.loot.push_back(std::move(source));
        }
    }
    if(r.remaining() || s.tasks.size()<4 || s.script.empty()) return false;
    output=std::move(s); return true;
}
}

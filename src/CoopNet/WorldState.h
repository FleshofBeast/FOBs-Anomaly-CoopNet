#pragma once
#include "ActorSnapshot.h"
#include <set>
namespace coopnet {
inline void write_world_float(Writer& writer,float value) { std::uint32_t bits; std::memcpy(&bits,&value,4); writer.integer(bits,4); }
inline bool read_world_float(Reader& reader,float& value) { std::uint64_t bits; if (!reader.integer(bits,4)) return false; const auto raw=static_cast<std::uint32_t>(bits); std::memcpy(&value,&raw,4); return true; }
// Baseline-local anchors are session-scoped identities, never engine object IDs.
inline Identity world_anchor(Identity session, std::uint16_t saved_object) {
    auto value=(session | (Identity(1)<<63)) ^ Identity(saved_object);
    value=(value^(value>>30))*0xbf58476d1ce4e5b9ULL;
    value=(value^(value>>27))*0x94d049bb133111ebULL;
    return value^(value>>31);
}
struct WorldAnimation {
    std::uint8_t part=0; std::uint16_t slot=0,motion=0;
    float time=0,speed=1; bool stop=false;
};
// Native rigid bodies, not just an entity's root transform. The six vectors
// are velocity, angular velocity, force, torque, position and previous position;
// the final eight values are current and previous quaternions.
struct WorldPhysicsElement {
    std::array<float,26> values{};
    bool enabled=false;
};
struct WorldPhysics {
    std::uint64_t visible_bones=0;
    std::uint16_t root=0xffff;
    std::vector<WorldPhysicsElement> elements;
};
inline bool valid_world_physics(const WorldPhysics& physics) {
    if(physics.elements.empty()) return !physics.visible_bones && physics.root==0xffff;
    if(physics.elements.size()>64 || !physics.visible_bones || physics.root>=64) return false;
    for(const auto& body:physics.elements) {
        for(float value:body.values) if(!std::isfinite(value) || std::abs(value)>100000000.f) return false;
        for(unsigned offset:{18u,22u}) {
            float norm=0;for(unsigned i=0;i<4;++i) norm+=body.values[offset+i]*body.values[offset+i];
            if(norm<.5f || norm>1.5f) return false;
        }
        for(unsigned i=12;i<18;++i) if(std::abs(body.values[i])>1000000.f) return false;
    }
    return true;
}
inline bool valid_world_animation(const WorldAnimation& a) {
    return a.part<4 && a.slot<256 && a.motion<65535 && std::isfinite(a.time) && a.time>=0 && a.time<=3600 &&
        std::isfinite(a.speed) && a.speed>=0 && a.speed<=10;
}
struct WorldPose {
    Identity anchor=0, incarnation=0;
    std::array<float,3> position{}, rotation{};
    float health=0;
    std::vector<WorldAnimation> animations;
    std::uint8_t zone_state=255;
    std::uint32_t zone_time=0;
    Identity killer=0;
    bool killer_player=false;
    WorldPhysics physics;
};
inline std::size_t world_pose_wire_size(const WorldPose& pose) {
    return 60+pose.animations.size()*14+(pose.physics.elements.empty() ? 0 : 10+pose.physics.elements.size()*105);
}
struct WorldState {
    std::uint32_t level=0, tick=0;
    std::vector<WorldPose> objects;
};
inline bool valid_world_state(const WorldState& state) {
    if (!state.level || state.objects.empty() || state.objects.size()>128) return false;
    std::set<Identity> unique;std::size_t size=10;
    for (const auto& object:state.objects) {
        if (!object.anchor || !object.incarnation || !unique.insert(object.anchor).second ||
            !std::isfinite(object.health) || object.health < -1 || object.health>1) return false;
        for (const auto& vector:{object.position,object.rotation})
            for (const auto value:vector) if (!std::isfinite(value) || std::abs(value)>1000000) return false;
        if (object.animations.size()>4) return false;
        if(!valid_world_physics(object.physics) || (!object.physics.elements.empty() && (object.health>0 || object.zone_state!=255))) return false;
        size+=world_pose_wire_size(object);if(size>max_payload-16) return false;
        if ((object.zone_state!=255 && object.zone_state>4) || object.zone_time>0x7fffffff ||
            (object.zone_state==255 && object.zone_time) || (object.zone_state!=255 && (!object.animations.empty() || object.killer)) || (object.killer_player && !object.killer)) return false;
        std::set<unsigned> parts;
        for (const auto& animation:object.animations) if (!valid_world_animation(animation) || !parts.insert(animation.part).second) return false;
    }
    return true;
}
inline std::vector<std::uint8_t> encode_world_state(const WorldState& state) {
    if (!valid_world_state(state)) throw std::invalid_argument("Invalid world state");
    Writer writer; writer.integer(state.level,4); writer.integer(state.tick,4); writer.integer(state.objects.size(),2);
    for (const auto& object:state.objects) {
        writer.integer(object.anchor,8); writer.integer(object.incarnation,8);
        for (const auto& vector:{object.position,object.rotation}) for (const auto value:vector) {
            std::uint32_t bits; std::memcpy(&bits,&value,4); writer.integer(bits,4);
        }
        std::uint32_t bits; std::memcpy(&bits,&object.health,4); writer.integer(bits,4);
        writer.integer(object.zone_state,1); writer.integer(object.zone_time,4);
        writer.integer(object.killer,8);writer.integer(object.killer_player ? 1 : 0,1);
        writer.integer(object.animations.size(),1);
        for (const auto& a:object.animations) {
            writer.integer(a.part,1); writer.integer(a.slot,2); writer.integer(a.motion,2);
            write_world_float(writer,a.time); write_world_float(writer,a.speed); writer.integer(a.stop,1);
        }
        writer.integer(object.physics.elements.size(),1);
        if(!object.physics.elements.empty()) {
            writer.integer(object.physics.visible_bones,8);writer.integer(object.physics.root,2);
            for(const auto& body:object.physics.elements) {
                for(float value:body.values) write_world_float(writer,value);
                writer.integer(body.enabled ? 1 : 0,1);
            }
        }
    }
    return writer.bytes;
}
inline bool decode_world_state(const std::vector<std::uint8_t>& bytes,WorldState& output) {
    Reader reader(bytes); std::uint64_t level,tick,count;
    if (!reader.integer(level,4) || !reader.integer(tick,4) || !reader.integer(count,2) || !count || count>128 ||
        reader.remaining()<count*60) return false;
    WorldState state; state.level=static_cast<std::uint32_t>(level); state.tick=static_cast<std::uint32_t>(tick);
    state.objects.resize(static_cast<std::size_t>(count));
    for (auto& object:state.objects) {
        if (!reader.integer(object.anchor,8) || !reader.integer(object.incarnation,8)) return false;
        for (auto* vector:{&object.position,&object.rotation}) for (auto& value:*vector) {
            std::uint64_t bits; if (!reader.integer(bits,4)) return false;
            const auto number=static_cast<std::uint32_t>(bits); std::memcpy(&value,&number,4);
        }
        std::uint64_t bits; if (!reader.integer(bits,4)) return false;
        const auto number=static_cast<std::uint32_t>(bits); std::memcpy(&object.health,&number,4);
        std::uint64_t zone,state_time;
        if (!reader.integer(zone,1) || !reader.integer(state_time,4)) return false;
        object.zone_state=static_cast<std::uint8_t>(zone); object.zone_time=static_cast<std::uint32_t>(state_time);
        if(!reader.integer(object.killer,8) || !reader.integer(bits,1) || bits>1) return false;object.killer_player=bits!=0;
        std::uint64_t animations; if (!reader.integer(animations,1) || animations>4) return false;
        for (unsigned i=0;i<animations;++i) {
            WorldAnimation a; std::uint64_t part,slot,motion,stop;
            if (!reader.integer(part,1) || !reader.integer(slot,2) || !reader.integer(motion,2) ||
                !read_world_float(reader,a.time) || !read_world_float(reader,a.speed) || !reader.integer(stop,1) || stop>1) return false;
            a.part=static_cast<std::uint8_t>(part); a.slot=static_cast<std::uint16_t>(slot); a.motion=static_cast<std::uint16_t>(motion); a.stop=stop!=0;
            object.animations.push_back(a);
        }
        std::uint64_t bodies,root,enabled;
        if(!reader.integer(bodies,1) || bodies>64) return false;
        if(bodies) {
            if(!reader.integer(object.physics.visible_bones,8) || !reader.integer(root,2) || root>=64 || reader.remaining()<bodies*105) return false;
            object.physics.root=static_cast<std::uint16_t>(root);object.physics.elements.resize(static_cast<std::size_t>(bodies));
            for(auto& body:object.physics.elements) {
                for(auto& value:body.values) if(!read_world_float(reader,value)) return false;
                if(!reader.integer(enabled,1) || enabled>1) return false;body.enabled=enabled!=0;
            }
        }
    }
    if (reader.remaining() || !valid_world_state(state)) return false;
    output=std::move(state); return true;
}
}

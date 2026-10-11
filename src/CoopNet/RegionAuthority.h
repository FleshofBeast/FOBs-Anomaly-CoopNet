#pragma once
#include "Protocol.h"
#include <array>
#include <map>
#include <set>
#include <memory>
#include <limits>
#include <algorithm>

namespace coopnet {
// Host-owned coordinator: leases delegate local simulation work only. They do
// not grant authority over weather, world time, global story/events or saves.
// This grid partitions authority, not native geometry or navigation graphs.
struct RegionKey {
    std::uint32_t level=0;
    std::int32_t x=0,z=0;
    bool operator<(const RegionKey& other) const {
        if(level!=other.level) return level<other.level;
        return x!=other.x ? x<other.x : z<other.z;
    }
    bool operator==(const RegionKey& other) const {return level==other.level && x==other.x && z==other.z;}
};
// Default matches the audited configuration. The runtime must supply the live
// native switch distance when establishing a session; the lattice then stays fixed.
constexpr double default_region_width=750;
inline bool valid_region_width(double width) {return std::isfinite(width) && width>=10 && width<=4096;}
inline RegionKey region_at(std::uint32_t level,const std::array<float,3>& position,double width=default_region_width) {
    if(!valid_region_width(width)) throw std::invalid_argument("Invalid region width");
    if(!level) throw std::invalid_argument("Missing region level");
    for(float value:position) if(!std::isfinite(value) || std::abs(value)>1000000)
        throw std::invalid_argument("Invalid region position");
    return {level,static_cast<std::int32_t>(std::floor(position[0]/width)),
        static_cast<std::int32_t>(std::floor(position[2]/width))};
}
inline double region_distance_squared(const RegionKey& key,const std::array<float,3>& position,double width=default_region_width) {
    const double dx=(std::max)({key.x*width-position[0],0.,position[0]-(key.x+1.)*width});
    const double dz=(std::max)({key.z*width-position[2],0.,position[2]-(key.z+1.)*width});
    return dx*dx+dz*dz;
}
enum class RegionStage : std::uint8_t { Dormant, Quiescing, Restoring, Active };
struct RegionLease {
    RegionKey key;
    Identity epoch=0,owner=0,previous=0;
    std::uint32_t checkpoint=0;
    RegionStage stage=RegionStage::Dormant;
};
struct RegionCheckpoint {
    std::uint32_t revision=0;
    // Opaque canonical native state, not just transforms. An adapter must restore
    // script/AI, lifetime and inventory state before acknowledging the lease.
    std::shared_ptr<const std::vector<std::uint8_t>> bytes;
};
class RegionAuthority {
    double width_;
    struct Region {
        RegionLease lease;
        RegionCheckpoint checkpoint;
        std::map<Identity,std::uint64_t> visitors;
        SequenceWindow publications;
    };
    std::map<RegionKey,Region> regions_;
    std::map<Identity,std::set<RegionKey>> interests_;
    std::map<Identity,RegionKey> neighborhood_centers_;
    std::uint64_t arrival_=0;
    Identity epoch_=0;
    std::size_t checkpoint_bytes_=0;
    static constexpr std::size_t max_regions=4096,max_checkpoint=8*1024*1024,max_total=64*1024*1024;
    Identity next_epoch() {
        if(epoch_==std::numeric_limits<Identity>::max()) throw std::overflow_error("Region epoch exhausted");
        return ++epoch_;
    }
    static Identity first(const Region& region) {
        Identity result=0;std::uint64_t earliest=std::numeric_limits<std::uint64_t>::max();
        for(const auto& visitor:region.visitors) if(visitor.second<earliest) {result=visitor.first;earliest=visitor.second;}
        return result;
    }
    void restore_or_sleep(Region& region) {
        region.lease.owner=first(region);region.lease.previous=0;
        region.lease.checkpoint=region.checkpoint.revision;
        region.lease.stage=region.lease.owner && region.checkpoint.bytes ? RegionStage::Restoring : RegionStage::Dormant;
        region.publications={};
    }
    void reconcile(Region& region,bool disconnected=false) {
        auto& lease=region.lease;
        if(lease.stage==RegionStage::Quiescing) {
            if(disconnected && !region.visitors.count(lease.previous)) restore_or_sleep(region);
            return;
        }
        if(lease.owner && region.visitors.count(lease.owner)) return;
        const auto old=lease.owner;
        const bool was_active=lease.stage==RegionStage::Active;
        lease.epoch=next_epoch();
        if(was_active && old && !disconnected) {
            lease.previous=old;lease.owner=0;lease.stage=RegionStage::Quiescing;
        } else restore_or_sleep(region);
    }
    bool store(Region& region,const RegionCheckpoint& checkpoint) {
        if(!checkpoint.revision || !checkpoint.bytes || checkpoint.bytes->empty() || checkpoint.bytes->size()>max_checkpoint ||
            checkpoint.revision<=region.checkpoint.revision) return false;
        const auto old=region.checkpoint.bytes ? region.checkpoint.bytes->size() : 0;
        if(checkpoint.bytes->size()>max_total-(checkpoint_bytes_-old)) return false;
        // A const shared_ptr can still alias a producer's mutable vector. Retain
        // our own immutable copy so an in-flight capture cannot rewrite a lease.
        auto retained=std::make_shared<const std::vector<std::uint8_t>>(*checkpoint.bytes);
        checkpoint_bytes_=checkpoint_bytes_-old+retained->size();region.checkpoint={checkpoint.revision,std::move(retained)};return true;
    }
    bool replace_interest(Identity player,std::set<RegionKey> desired) {
        std::size_t additions=0;for(const auto& key:desired) if(!regions_.count(key)) ++additions;
        if(additions>max_regions-regions_.size()) return false;
        auto& previous=interests_[player];
        for(const auto& key:desired) if(!previous.count(key)) {
            auto& region=regions_[key];region.lease.key=key;region.visitors.emplace(player,++arrival_);reconcile(region);
        }
        for(const auto& key:previous) if(!desired.count(key)) {
            auto& region=regions_.at(key);region.visitors.erase(player);reconcile(region);
        }
        previous=std::move(desired);return true;
    }
public:
    explicit RegionAuthority(double processing_distance=default_region_width) : width_(processing_distance) {
        if(!valid_region_width(width_)) throw std::invalid_argument("Invalid native processing distance");
    }
    double width() const {return width_;}
    RegionKey key_at(std::uint32_t level,const std::array<float,3>& position) const {return region_at(level,position,width_);}
    const RegionLease* lease(const RegionKey& key) const {
        const auto found=regions_.find(key);return found==regions_.end() ? nullptr : &found->second.lease;
    }
    const RegionCheckpoint* checkpoint(const RegionKey& key) const {
        const auto found=regions_.find(key);return found==regions_.end() ? nullptr : &found->second.checkpoint;
    }
    bool interested(Identity player,const RegionKey& key) const {
        const auto found=interests_.find(player);return found!=interests_.end() && found->second.count(key)!=0;
    }
    std::size_t interest_count(Identity player) const {
        const auto found=interests_.find(player);return found==interests_.end() ? 0 : found->second.size();
    }
    // Exact simulation footprint: current cell plus all eight neighbors.
    // Recenter beyond a boundary margin. Overlapping cells
    // retain their owner and epoch; departed cells follow the handoff barrier.
    bool update_neighborhood(Identity player,std::uint32_t level,const std::array<float,3>& position) {
        if(!player || (!interests_.count(player) && interests_.size()>=64)) return false;
        RegionKey center;try {center=key_at(level,position);} catch(const std::invalid_argument&) {return false;}
        const auto old=neighborhood_centers_.find(player);
        if(old!=neighborhood_centers_.end() && old->second.level==level) {
            const double x=(old->second.x+.5)*width_,z=(old->second.z+.5)*width_;
            // Reference: half a cell plus 1024/8192 of one cell. Scale the
            // margin to Anomaly's processing-range cell width, not game units.
            if((std::max)(std::abs(position[0]-x),std::abs(position[2]-z))<=width_*.625) center=old->second;
        }
        std::set<RegionKey> desired;
        for(std::int32_t dx=-1;dx<=1;++dx) for(std::int32_t dz=-1;dz<=1;++dz)
            desired.insert({center.level,center.x+dx,center.z+dz});
        if(!replace_interest(player,std::move(desired))) return false;
        neighborhood_centers_[player]=center;return true;
    }
    std::vector<RegionLease> leases() const {
        std::vector<RegionLease> result;for(const auto& region:regions_) result.push_back(region.second.lease);return result;
    }
    bool update_interest(Identity player,std::uint32_t level,const std::array<float,3>& position,
        double enter_radius,double leave_radius) {
        if(!player || !std::isfinite(enter_radius) || !std::isfinite(leave_radius) || enter_radius<0 ||
            enter_radius>leave_radius || leave_radius>4096 || (!interests_.count(player) && interests_.size()>=64)) return false;
        RegionKey center;try {center=key_at(level,position);} catch(const std::invalid_argument&) {return false;}
        std::set<RegionKey> desired;
        const auto extent=static_cast<std::int32_t>(std::ceil(leave_radius/width_))+1;
        if(static_cast<std::uint64_t>(2*extent+1)*static_cast<std::uint64_t>(2*extent+1)>max_regions*2) return false;
        for(auto x=center.x-extent;x<=center.x+extent;++x) for(auto z=center.z-extent;z<=center.z+extent;++z) {
            const RegionKey key{level,x,z};const double radius=interested(player,key) ? leave_radius : enter_radius;
            if(region_distance_squared(key,position,width_)<=radius*radius) desired.insert(key);
        }
        if(!replace_interest(player,std::move(desired))) return false;
        neighborhood_centers_.erase(player);return true;
    }
    void disconnect(Identity player) {
        interests_.erase(player);
        neighborhood_centers_.erase(player);
        // Also release pending quiescence for a player that already left its region.
        for(auto& entry:regions_) {
            auto& region=entry.second;region.visitors.erase(player);
            if(region.lease.owner==player || region.lease.previous==player) reconcile(region,true);
        }
    }
    bool seed(const RegionKey& key,const RegionCheckpoint& checkpoint) {
        const auto found=regions_.find(key);if(found==regions_.end() || found->second.lease.stage!=RegionStage::Dormant) return false;
        auto& region=found->second;if(!store(region,checkpoint)) return false;
        restore_or_sleep(region);return true;
    }
    bool quiesced(Identity player,const RegionKey& key,Identity epoch,const RegionCheckpoint& checkpoint) {
        const auto found=regions_.find(key);if(found==regions_.end()) return false;
        auto& region=found->second;
        if(region.lease.stage!=RegionStage::Quiescing || region.lease.previous!=player || region.lease.epoch!=epoch || !store(region,checkpoint)) return false;
        restore_or_sleep(region);return true;
    }
    bool restored(Identity player,const RegionKey& key,Identity epoch,std::uint32_t checkpoint_revision) {
        const auto found=regions_.find(key);if(found==regions_.end()) return false;
        auto& region=found->second;
        if(region.lease.stage!=RegionStage::Restoring || region.lease.owner!=player || region.lease.epoch!=epoch ||
            !region.visitors.count(player) || region.checkpoint.revision!=checkpoint_revision) return false;
        region.lease.stage=RegionStage::Active;return true;
    }
    bool accepts(Identity player,const RegionKey& key,Identity epoch,std::uint32_t sequence) {
        const auto found=regions_.find(key);if(found==regions_.end()) return false;
        auto& region=found->second;
        return region.lease.stage==RegionStage::Active && region.lease.owner==player && region.lease.epoch==epoch &&
            region.publications.accept(sequence);
    }
    bool save(Identity player,const RegionKey& key,Identity epoch,const RegionCheckpoint& checkpoint) {
        const auto found=regions_.find(key);if(found==regions_.end()) return false;
        auto& region=found->second;
        if(region.lease.stage!=RegionStage::Active || region.lease.owner!=player || region.lease.epoch!=epoch || !store(region,checkpoint)) return false;
        region.lease.checkpoint=checkpoint.revision;return true;
    }
};
}

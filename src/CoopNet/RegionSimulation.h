#pragma once
#include "RegionMessages.h"
namespace coopnet {
// Local native execution fence. Receiving assets or a checkpoint never grants
// execution: both native restoration and the host's Active lease are required.
class RegionSimulation {
    Identity player_=0;
    std::uint32_t level_=0;
    double width_=default_region_width;
    bool coordinator_=false;
    std::map<RegionKey,RegionLease> leases_;
    std::map<RegionKey,RegionAck> restored_;
public:
    bool configure(Identity player,std::uint32_t level,double width,bool coordinator) {
        if(!player || !level || !valid_region_width(width)) return false;
        if(player_!=player || level_!=level || width_!=width || coordinator_!=coordinator) {
            leases_.clear();restored_.clear();
        }
        player_=player;level_=level;width_=width;coordinator_=coordinator;return true;
    }
    bool configured() const {return player_!=0;}
    void clear() {*this=RegionSimulation{};}
    bool observe(const RegionLease& lease) {
        if(!configured() || !valid_region_lease(lease) || (!coordinator_ && lease.key.level!=level_)) return false;
        const auto old=leases_.find(lease.key);
        if(old!=leases_.end()) {
            const auto& previous=old->second;
            if(lease.epoch<previous.epoch) return false;
            if(lease.epoch==previous.epoch) {
                if(lease.checkpoint<previous.checkpoint) return false;
                const bool barrier_finished=previous.stage==RegionStage::Quiescing &&
                    (lease.stage==RegionStage::Restoring || lease.stage==RegionStage::Dormant);
                if(!barrier_finished && (lease.owner!=previous.owner || lease.previous!=previous.previous)) return false;
                if(lease.stage!=previous.stage && !barrier_finished &&
                    !(previous.stage==RegionStage::Dormant && lease.stage==RegionStage::Restoring) &&
                    !(previous.stage==RegionStage::Restoring && lease.stage==RegionStage::Active)) return false;
            } else restored_.erase(lease.key);
        } else if(leases_.size()>=4096) return false;
        leases_[lease.key]=lease;return true;
    }
    bool native_restored(const RegionAck& ack) {
        const auto found=leases_.find(ack.key);
        if(found==leases_.end()) return false;
        const auto& lease=found->second;
        if(lease.stage!=RegionStage::Restoring || lease.owner!=player_ || lease.epoch!=ack.epoch || lease.checkpoint!=ack.checkpoint) return false;
        restored_[ack.key]=ack;return true;
    }
    bool owns(const RegionKey& key) const {
        const auto lease=leases_.find(key);const auto restored=restored_.find(key);
        return lease!=leases_.end() && restored!=restored_.end() && lease->second.stage==RegionStage::Active &&
            lease->second.owner==player_ && lease->second.epoch==restored->second.epoch && lease->second.checkpoint>=restored->second.checkpoint;
    }
    bool has_owned() const {for(const auto& lease:leases_) if(owns(lease.first)) return true;return false;}
    bool permits(std::uint32_t level,const std::array<float,3>& position) const {
        if(!configured()) return false;
        if(level!=level_ && !coordinator_) return false;
        RegionKey key;try {key=region_at(level,position,width_);} catch(const std::invalid_argument&) {return false;}
        const auto lease=leases_.find(key);
        if(lease==leases_.end() || lease->second.stage==RegionStage::Dormant) return coordinator_;
        return owns(key);
    }
};
}

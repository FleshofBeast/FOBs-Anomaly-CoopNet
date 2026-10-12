#pragma once
#include "Respawn.h"
#include <set>
namespace coopnet {
// Host-owned unanimous wipe vote. The round survives departures, but new
// members must vote themselves; a living player cancels the wipe entirely.
class SquadRecoveryVote {
    Identity round_=0;
    bool active_=false;
    std::set<Identity> members_,votes_;
public:
    bool update(const std::vector<Identity>& connected,bool everyone_dead) {
        std::set<Identity> next(connected.begin(),connected.end());
        if(next.size()!=connected.size() || next.size()>4 || next.count(0)) return false;
        if(!everyone_dead || next.empty()) {active_=false;members_.clear();votes_.clear();return true;}
        if(!active_) {
            if(round_==std::numeric_limits<Identity>::max()) return false;
            ++round_;active_=true;votes_.clear();
        }
        members_=std::move(next);
        for(auto vote=votes_.begin();vote!=votes_.end();) {
            if(!members_.count(*vote)) vote=votes_.erase(vote);else ++vote;
        }
        return true;
    }
    bool vote(Identity player,Identity round) {
        if(!active_ || round!=round_ || !members_.count(player)) return false;
        votes_.insert(player);return true;
    }
    bool unanimous() const {return active_ && !members_.empty() && votes_.size()==members_.size();}
    bool active() const {return active_;}
    Identity round() const {return round_;}
    const std::set<Identity>& members() const {return members_;}
    const std::set<Identity>& votes() const {return votes_;}
    bool finish(Identity round) {
        if(round!=round_ || !unanimous()) return false;
        active_=false;members_.clear();votes_.clear();return true;
    }
};
}

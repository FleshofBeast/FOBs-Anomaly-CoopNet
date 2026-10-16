#pragma once
#include "HostPump.h"
#include "RegionCatalogue.h"
namespace coopnet {
// Initial native-world delegation. Reassignment after an owner leaves remains
// fenced until a fresh native checkpoint is integrated; the seed is never
// misrepresented as a current checkpoint for a previously simulated cell.
class DedicatedSimulation {
    struct Cell {
        Identity bootstrap_epoch=0,catalogue_epoch=0;
        std::uint32_t revision=0;
        RegionAssembly incoming;
        std::vector<NPCRecord> records;
    };
    HostPump& host_;
    RegionAuthority regions_;
    WorldBaseline seed_;
    std::shared_ptr<const std::vector<std::uint8_t>> seed_reference_;
    std::map<RegionKey,Cell> cells_;
    std::map<Identity,RegionKey> roots_;
    std::set<Identity> participants_;
    std::map<Identity,std::uint32_t> generations_;
    std::map<std::pair<Identity,RegionKey>,std::vector<std::uint8_t>> announced_;
    std::uint32_t catalogue_revision_=0;
    std::uint32_t state_sequence_=0;
    bool dirty_=false;
    std::uint64_t accepted_states_=0,accepted_catalogues_=0;
public:
    DedicatedSimulation(HostPump& host,const WorldBaseline& seed) :host_(host),seed_(seed),
        seed_reference_(std::make_shared<const std::vector<std::uint8_t>>(encode_baseline(seed))) {}
    const RegionAuthority& regions() const {return regions_;}
    std::uint64_t accepted_states() const {return accepted_states_;}
    std::uint64_t accepted_catalogues() const {return accepted_catalogues_;}
    bool interest(Identity player,const ActorInput& input) {
        for(const auto& connected:host_.session().players()) if(connected.id==player) {
            auto& generation=generations_[player];
            if(generation && generation!=connected.generation) {
                regions_.disconnect(player);participants_.erase(player);dirty_=true;
                for(auto sent=announced_.begin();sent!=announced_.end();)
                    if(sent->first.first==player)sent=announced_.erase(sent);else ++sent;
            }
            generation=connected.generation;break;
        }
        if(input.level!=seed_.level || !input.owner_pose || !regions_.update_neighborhood(player,input.level,input.position)) return false;
        participants_.insert(player);
        for(const auto& lease:regions_.leases()) if(lease.stage==RegionStage::Dormant && lease.owner && !cells_.count(lease.key)) {
            if(!regions_.seed(lease.key,{1,seed_reference_})) return false;
            cells_[lease.key].bootstrap_epoch=regions_.lease(lease.key)->epoch;
        }
        return true;
    }
    bool receive(Identity sender,const Frame& frame) {
        if(frame.message==Message::RegionAck) {
            RegionAck ack;if(frame.sequence || !decode_region_ack(frame.payload,ack))return false;
            const auto cell=cells_.find(ack.key);const auto* lease=regions_.lease(ack.key);
            if(cell==cells_.end() || !lease || lease->owner!=sender || ack.epoch!=cell->second.bootstrap_epoch || ack.checkpoint!=1) return true;
            if(lease->stage==RegionStage::Active)return true; // reliable duplicate
            return regions_.restored(sender,ack.key,ack.epoch,ack.checkpoint);
        }
        if(frame.message==Message::RegionChunk) {
            RegionChunk chunk;if(frame.sequence || !decode_region_chunk(frame.payload,chunk))return false;
            if(chunk.total>shared_limit+4)return false;
            const auto* lease=regions_.lease(chunk.key);
            if(!lease || lease->owner!=sender || lease->epoch!=chunk.epoch || lease->stage!=RegionStage::Active)return true;
            auto& cell=cells_.at(chunk.key);
            if(chunk.revision<=cell.revision)return true;
            if(!chunk.offset && (cell.incoming.complete() || cell.incoming.context().checkpoint!=chunk.revision))cell.incoming.clear();
            if(!cell.incoming.accept(chunk))return false;
            if(!cell.incoming.complete())return true;
            std::vector<NPCRecord> records;if(!decode_region_catalogue(cell.incoming.bytes(),records))return false;
            for(const auto& record:records) {
                const auto root=roots_.find(record.pose.anchor);
                if(root!=roots_.end() && !(root->second==chunk.key))return false;
                if(root==roots_.end() && !(regions_.key_at(chunk.key.level,record.pose.position)==chunk.key))return false;
            }
            for(const auto& record:records)roots_[record.pose.anchor]=chunk.key;
            cell.records=std::move(records);cell.catalogue_epoch=chunk.epoch;cell.revision=chunk.revision;cell.incoming.clear();
            dirty_=true;++accepted_catalogues_;return true;
        }
        if(frame.message==Message::RegionState) {
            RegionState state;if(!decode_region_state(frame.payload,state) || frame.sequence!=state.world.tick)return false;
            const auto* lease=regions_.lease(state.key);const auto cell=cells_.find(state.key);
            if(!lease || lease->owner!=sender || lease->epoch!=state.epoch || lease->stage!=RegionStage::Active ||
                cell==cells_.end() || cell->second.catalogue_epoch!=state.epoch)return true;
            for(const auto& pose:state.world.objects) {
                const auto found=std::find_if(cell->second.records.begin(),cell->second.records.end(),[&](const NPCRecord& n){return n.pose.anchor==pose.anchor && n.pose.incarnation==pose.incarnation;});
                // Unreliable state can overtake its reliable spawn catalogue.
                // Drop that packet without disconnecting a healthy worker.
                if(found==cell->second.records.end())return true;
            }
            if(!regions_.accepts(sender,state.key,state.epoch,state.world.tick))return true;
            // Worker epochs have independent sequence spaces. Observers receive
            // one coordinator sequence so future handoffs cannot rewind playback.
            state.world.tick=++state_sequence_;
            ++accepted_states_;return host_.publish_world_state(state.world,sender);
        }
        return false;
    }
    void update() {
        for(auto it=participants_.begin();it!=participants_.end();) {
            if(!host_.participant_ready(*it)) {
                const auto player=*it;regions_.disconnect(player);
                for(auto sent=announced_.begin();sent!=announced_.end();)
                    if(sent->first.first==player)sent=announced_.erase(sent);else ++sent;
                it=participants_.erase(it);dirty_=true;
            }else ++it;
        }
        const auto leases=regions_.leases();
        for(const auto& player:host_.session().players()) if(player.connected && host_.level_ready(player.id,seed_.level))
            // Previous observers also need the revocation after leaving a cell.
            for(const auto& lease:leases) if(regions_.interested(player.id,lease.key) || announced_.count({player.id,lease.key})) {
                const auto payload=encode_region_lease(lease);auto& sent=announced_[{player.id,lease.key}];
                if(sent!=payload && host_.send_region_frame(player.id,{Message::RegionLease,Channel::World,Delivery::ReliableOrdered,0,payload})==SendResult::Sent)sent=payload;
            }
        if(!dirty_)return;
        std::vector<NPCRecord> records;
        for(const auto& lease:leases) if(lease.owner && (lease.stage==RegionStage::Restoring || lease.stage==RegionStage::Active)) {
            const auto cell=cells_.find(lease.key);
            if(lease.stage!=RegionStage::Active || cell==cells_.end() || cell->second.catalogue_epoch!=lease.epoch)return;
            records.insert(records.end(),cell->second.records.begin(),cell->second.records.end());
        }
        if(host_.publish_shared_world(SharedKind::NPC,seed_.level,catalogue_revision_+1,encode_npcs(records))) {++catalogue_revision_;dirty_=false;}
    }
};
}

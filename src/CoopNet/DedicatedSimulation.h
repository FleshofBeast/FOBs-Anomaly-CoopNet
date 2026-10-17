#pragma once
#include "HostPump.h"
#include "RegionCatalogue.h"
namespace coopnet {
// Native-world delegation with ownership handoff. Each accepted owner catalogue
// becomes the cell's checkpoint; a reassigned owner must restore that checkpoint,
// never the seed, before simulating. The checkpoint carries the last published
// NPC records (identity, lifetime, pose, health), not internal planner/script
// state: the new owner resumes native AI from those poses.
class DedicatedSimulation {
    struct Cell {
        Identity bootstrap_epoch=0,catalogue_epoch=0;
        std::uint32_t revision=0;
        bool simulated=false; // Seed is obsolete once any owner went Active.
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
    // Checkpoint delivery progress per cell: lease epoch/revision and next offset.
    struct CheckpointDelivery {Identity epoch=0;std::uint32_t checkpoint=0,offset=0;};
    std::map<RegionKey,CheckpointDelivery> deliveries_;
    std::uint32_t checkpoint_revision_=1; // Revision 1 is the shared seed.
    std::uint64_t handoffs_=0;
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
    std::uint64_t handoffs() const {return handoffs_;}
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
            if(cell==cells_.end() || !lease || lease->owner!=sender || ack.epoch!=lease->epoch || ack.checkpoint!=lease->checkpoint) return true;
            // The original seed cannot authorize replaying stale AI after reassignment.
            if(ack.checkpoint==1 && cell->second.simulated) return true;
            if(lease->stage==RegionStage::Active)return true; // reliable duplicate
            if(!regions_.restored(sender,ack.key,ack.epoch,ack.checkpoint)) return true;
            if(cell->second.simulated) ++handoffs_;
            cell->second.simulated=true;return true;
        }
        if(frame.message==Message::RegionChunk) {
            RegionChunk chunk;if(frame.sequence || !decode_region_chunk(frame.payload,chunk))return false;
            if(chunk.total>shared_limit+4)return false;
            const auto* lease=regions_.lease(chunk.key);
            if(!lease || lease->owner!=sender || lease->epoch!=chunk.epoch || lease->stage!=RegionStage::Active)return true;
            auto& cell=cells_.at(chunk.key);
            // Each owner epoch numbers its catalogues independently.
            if(chunk.epoch==cell.catalogue_epoch && chunk.revision<=cell.revision)return true;
            if(!chunk.offset && (cell.incoming.complete() || cell.incoming.context().checkpoint!=chunk.revision ||
                cell.incoming.context().epoch!=chunk.epoch))cell.incoming.clear();
            if(!cell.incoming.accept(chunk))return false;
            if(!cell.incoming.complete())return true;
            std::vector<NPCRecord> records;if(!decode_region_catalogue(cell.incoming.bytes(),records))return false;
            for(const auto& record:records) {
                const auto root=roots_.find(record.pose.anchor);
                if(root!=roots_.end() && !(root->second==chunk.key))return false;
                if(root==roots_.end() && !(regions_.key_at(chunk.key.level,record.pose.position)==chunk.key))return false;
            }
            // Retain the accepted catalogue as the cell's handoff checkpoint.
            if(checkpoint_revision_==std::numeric_limits<std::uint32_t>::max() ||
                !regions_.save(sender,chunk.key,chunk.epoch,{checkpoint_revision_+1,
                    std::make_shared<const std::vector<std::uint8_t>>(cell.incoming.bytes())}))return true;
            ++checkpoint_revision_;
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
        // A departing owner's last accepted catalogue completes its handoff barrier;
        // its AI is no longer accepted once the lease leaves Active. Without a
        // published catalogue the seed stays obsolete and the cell stays fenced.
        for(const auto& lease:regions_.leases()) if(lease.stage==RegionStage::Quiescing) {
            const auto* checkpoint=regions_.checkpoint(lease.key);
            if(!checkpoint || !checkpoint->bytes || checkpoint->revision<=1 ||
                checkpoint_revision_==std::numeric_limits<std::uint32_t>::max())continue;
            if(regions_.quiesced(lease.previous,lease.key,lease.epoch,{checkpoint_revision_+1,checkpoint->bytes})){++checkpoint_revision_;dirty_=true;}
        }
        const auto leases=regions_.leases();
        for(const auto& player:host_.session().players()) if(player.connected && host_.level_ready(player.id,seed_.level))
            // Previous observers also need the revocation after leaving a cell.
            for(const auto& lease:leases) if(regions_.interested(player.id,lease.key) || announced_.count({player.id,lease.key})) {
                const auto payload=encode_region_lease(lease);auto& sent=announced_[{player.id,lease.key}];
                if(sent!=payload && host_.send_region_frame(player.id,{Message::RegionLease,Channel::World,Delivery::ReliableOrdered,0,payload})==SendResult::Sent)sent=payload;
            }
        // Stream a handoff checkpoint to its new owner after that owner holds the
        // matching lease; reliable ordering then delivers lease before checkpoint.
        for(const auto& lease:leases) {
            if(lease.stage!=RegionStage::Restoring || !lease.owner || lease.checkpoint<=1){deliveries_.erase(lease.key);continue;}
            const auto* checkpoint=regions_.checkpoint(lease.key);const auto sent=announced_.find({lease.owner,lease.key});
            if(!checkpoint || !checkpoint->bytes || checkpoint->revision!=lease.checkpoint || sent==announced_.end() ||
                sent->second!=encode_region_lease(lease))continue;
            auto& delivery=deliveries_[lease.key];
            if(delivery.epoch!=lease.epoch || delivery.checkpoint!=lease.checkpoint)delivery={lease.epoch,lease.checkpoint,0};
            const auto& bytes=*checkpoint->bytes;const auto total=static_cast<std::uint32_t>(bytes.size());
            for(unsigned chunks=0;delivery.offset<total && chunks<8;++chunks) {
                const auto end=(std::min)(total,delivery.offset+region_chunk_size);
                RegionChunk chunk{lease.key,lease.epoch,lease.checkpoint,total,delivery.offset,{bytes.begin()+delivery.offset,bytes.begin()+end}};
                if(host_.send_region_frame(lease.owner,{Message::RegionChunk,Channel::World,Delivery::ReliableOrdered,0,encode_region_chunk(chunk)})!=SendResult::Sent)break;
                delivery.offset=end;
            }
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

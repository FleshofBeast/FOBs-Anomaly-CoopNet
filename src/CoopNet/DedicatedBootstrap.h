#pragma once
#include "HostPump.h"
#include "DedicatedSimulation.h"
#include <memory>
namespace coopnet {
// Engine-independent bootstrap for a canonical world. The caller must verify
// the world digest and provide game-data-aware profile validation/checkpointing.
// Loading a baseline is not a grant of cell simulation authority.
class DedicatedBootstrap {
    struct Loading {std::uint32_t generation=0;unsigned stage=0;InventoryView character;bool input_seen=false;std::uint32_t input_sequence=0;};
    HostPump host_;
    std::unique_ptr<DedicatedSimulation> simulation_;
    WorldBaseline world_;
    std::shared_ptr<const std::vector<std::uint8_t>> bytes_;
    std::map<Identity,InventoryView> characters_;
    std::map<Identity,Loading> loading_;
    std::function<Identity()> tokens_;
    std::uint64_t time_us_=0;
    std::uint32_t tick_=0;
public:
    DedicatedBootstrap() = default;
    DedicatedBootstrap(const DedicatedBootstrap&) = delete;
    DedicatedBootstrap& operator=(const DedicatedBootstrap&) = delete;
    HostPump& host() {return host_;}
    const HostPump& host() const {return host_;}
    const DedicatedSimulation* simulation() const {return simulation_.get();}
    void enable_experimental_simulation(std::function<void(Identity,const Frame&)> rejected={}) {
        if(!bytes_) throw std::logic_error("Dedicated world must be started before simulation");
        if(simulation_) throw std::logic_error("Dedicated simulation already enabled");
        simulation_=std::make_unique<DedicatedSimulation>(host_,world_);
        host_.set_region_sink([this,rejected](Identity sender,const Frame& frame) {
            const bool accepted=simulation_->receive(sender,frame);
            if(!accepted && rejected)rejected(sender,frame);
            return accepted;
        });
    }
    void start(Identity session,BuildIdentity build,const WorldBaseline& world,
        std::shared_ptr<const std::vector<std::uint8_t>> bytes,std::function<Identity()> tokens,
        std::function<bool(Identity,InventoryView&)> import_checkpoint) {
        if(!valid_baseline(world) || !world.scope || !bytes || bytes->size()!=world.size || !tokens || !import_checkpoint)
            throw std::invalid_argument("Incomplete dedicated world bootstrap");
        host_.start_dedicated(session,build,tokens);
        world_=world;bytes_=std::move(bytes);tokens_=std::move(tokens);
        host_.require_character_profile(true);
        host_.set_character_handler([this,checkpoint=std::move(import_checkpoint)](Identity player,const InventoryView& character) {
            if(!valid_inventory_view(character) || !character.npc_disposition.empty()) return false;
            // A resumed client's older local copy cannot roll back the server's
            // accepted checkpoint. New progress will use explicit transactions.
            if(characters_.count(character.actor)) return true;
            auto accepted=character;
            if(!checkpoint(player,accepted) || accepted.actor!=character.actor || accepted.generation!=1 || accepted.level!=1 ||
                accepted.revision!=1 || !valid_inventory_view(accepted) || !accepted.npc_disposition.empty()) return false;
            characters_[character.actor]=std::move(accepted);
            return true;
        });
        host_.set_resume_character_handler([this](Identity,Identity character) {return characters_.count(character)!=0;});
        host_.set_owner_appearance_handler([this](Identity,const ActorAppearance& appearance) {host_.publish_appearance(appearance);});
    }
    void update(double elapsed) {
        host_.update(elapsed);
        time_us_+=static_cast<std::uint64_t>((std::min)(elapsed,1.0)*1000000.0);
        // Retire transfer-failure notifications; disconnected players retain
        // their profile and get a fresh baseline barrier on reconnect.
        LevelFailure failure;
        while(host_.take_level_failure(failure)) {
            if(failure.reason!=TransferFailure::Disconnected) throw std::runtime_error("Dedicated world load failed");
        }
        for(const auto& player:host_.session().players()) {
            if(!player.id || !player.connected || !host_.participant_ready(player.id)) continue;
            auto& loading=loading_[player.id];
            if(loading.generation!=player.generation) loading={player.generation,0,{}};
            if(!loading.stage) {
                const auto found=characters_.find(player.character);
                if(found==characters_.end()) throw std::logic_error("Missing admitted character");
                loading.character=found->second;
                loading.character.actor=player.id;loading.character.generation=player.generation;
                loading.character.level=world_.level;loading.character.revision=1;
                // The archive identifies persistent world state; each transfer
                // needs its own native save name so reconnect never collides
                // with a baseline already stored in that client's saves folder.
                auto transfer=world_;transfer.id=tokens_();
                if(!host_.send_baseline(player.id,transfer,bytes_)) throw std::runtime_error("Dedicated baseline queue failed");
                loading.stage=1;
            }
            if(loading.stage==1 && host_.baseline_received(player.id)) {
                if(!host_.create_actor({player.id,player.id,player.character,player.generation,world_.level}) ||
                    !host_.assign_level(player.id,world_.level,tokens_())) throw std::runtime_error("Dedicated actor assignment failed");
                loading.stage=2;
            }
            if(loading.stage==2 && host_.level_ready(player.id,world_.level)) {
                if(host_.publish_inventory_view(player.id,loading.character)) loading.stage=3;
            }
            if(loading.stage==3) {
                ActorInput input;
                if(host_.latest_input(player.id,input) && input.owner_pose && (!loading.input_seen || input.sequence!=loading.input_sequence)) {
                    ActorSnapshot snapshot{player.id,player.generation,world_.level,++tick_,time_us_,input.position,input.velocity,
                        {input.pitch,input.yaw,0},input.movement,0,input.sequence};
                    if(!host_.publish_snapshot(snapshot)) throw std::runtime_error("Dedicated owner pose publication failed");
                    loading.input_seen=true;loading.input_sequence=input.sequence;
                    if(simulation_ && !simulation_->interest(player.id,input))
                        throw std::runtime_error("Dedicated simulation interest failed");
                }
            }
        }
        if(simulation_) simulation_->update();
    }
    void stop() {host_.stop();simulation_.reset();world_={};bytes_.reset();characters_.clear();loading_.clear();tokens_={};time_us_=0;tick_=0;}
};
}

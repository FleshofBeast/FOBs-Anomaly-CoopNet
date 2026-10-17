#pragma once
#include <cstdint>
#include <vector>
#include <memory>
#include "WorldSettings.h"
#include "SharedWorld.h"
#include "Radio.h"
#include "Dialogue.h"
#include "WorldHit.h"
#include "NativeRegionCheckpoint.h"
class CObject;
class CGameObject;
class CSE_Abstract;
class ISheduled;
struct SHit;
struct GAME_NEWS_DATA;
namespace engine_coopnet {
bool intercept_world_hit(CGameObject* target,const SHit& hit);
bool intercept_player_hit(CGameObject* target,const SHit& hit);
bool peek_player_hit(std::uint16_t& object,coopnet::WorldHit& hit);
void discard_player_hit();
bool apply_player_hit(std::uint64_t session,const coopnet::WorldHit& hit);
bool peek_world_hit(coopnet::WorldHit& hit);
void discard_world_hit();
bool apply_world_hit(std::uint64_t session,std::uint16_t actor,const coopnet::WorldHit& hit);
bool apply_owned_world_hit(std::uint64_t session,std::uint16_t shooter,const coopnet::WorldHit& hit);
void exercise_shared_hit_probe();
// Owner-thread save/load adapter. Names originate locally, never from network payloads.
bool capture_world_baseline(const char* name,std::uint32_t& level,std::vector<std::uint8_t>& bytes);
bool store_world_baseline(const char* name,const std::vector<std::uint8_t>& bytes);
bool load_world_baseline(const char* name);
bool world_baseline_loaded(const char* name);
bool read_join_profile_file(std::vector<std::uint8_t>& bytes);
bool write_join_profile_file(const std::vector<std::uint8_t>& bytes);
void export_settings_audit();
bool capture_world_rules(std::vector<coopnet::WorldRule>& rules);
void queue_host_world_rules(std::uint32_t revision,const std::vector<coopnet::WorldRule>& rules);
void update_host_world_rules();
void clear_host_world_rules();
bool capture_world_clock(coopnet::WorldClock& clock);
bool apply_host_world_clock(const coopnet::WorldClock& clock);
void exercise_world_settings_probe();
void begin_world_replication();
void end_world_replication();
struct NativePartyExit {
    std::uint16_t object=0xffff;
    std::uint32_t destination=0;
    unsigned present=0;
};
bool capture_party_exit(const std::vector<std::uint16_t>& actors,NativePartyExit& exit);
bool perform_party_transition(std::uint16_t exit);
bool prepare_party_probe(std::uint16_t& exit,float* origin);
bool position_party_probe(std::uint16_t actor,std::uint16_t exit,const float* origin,bool at_exit);
void display_party_status(unsigned stage,unsigned present,unsigned required,std::uint32_t destination);
struct NativeWorldPose {
    coopnet::RegionKey simulation_region;
    std::uint16_t object=0xffff;
    // Network identity bound by a received catalogue (0: derive from object).
    // Native IDs of runtime spawns differ per client, so this must be preserved.
    std::uint64_t anchor=0;
    std::uint64_t incarnation=0;
    float position[3]{},rotation[3]{},health=0;
    std::string section,visual,name;
    std::vector<coopnet::WorldAnimation> animations;
    std::uint8_t zone_state=255;
    std::uint32_t zone_time=0;
    std::uint16_t killer=0xffff;
    coopnet::WorldPhysics physics;
    std::vector<coopnet::ZoneShape> shapes;
};
void queue_npc_catalogue(std::uint64_t session,std::uint32_t level,const std::vector<coopnet::NPCRecord>& records);
void update_npc_catalogue();
bool capture_containers(std::uint64_t session,std::uint32_t& level,std::vector<coopnet::ContainerRecord>& records);
void queue_container_catalogue(std::uint64_t session,std::uint32_t level,const std::vector<coopnet::ContainerRecord>& records);
void update_container_catalogue();
void capture_radio_news(const GAME_NEWS_DATA& news);
bool capture_radio_history(std::uint32_t& level,std::vector<coopnet::RadioRecord>& news);
bool apply_radio_history(std::uint64_t session,std::uint32_t level,const std::vector<coopnet::RadioRecord>& news);
bool capture_shared_quests(std::uint64_t session,std::uint32_t& level,coopnet::QuestState& quests);
// Topic discovery evaluates native preconditions on the host for the requesting
// guest. A selected topic retains native speaker bindings and offers its root;
// phrase/reward actions are not executed by this adapter yet.
bool capture_native_dialogue_topics(std::uint64_t session,std::uint16_t actor,
    const coopnet::DialogueRequest& request,std::uint32_t revision,coopnet::DialogueView& view);
// Scoped owner-thread redirection for NPC replies during remote native dialogs.
class NativeDialogueOutput {
    coopnet::DialogueView* previous_;
public:
    explicit NativeDialogueOutput(coopnet::DialogueView& view);
    ~NativeDialogueOutput();
    NativeDialogueOutput(const NativeDialogueOutput&)=delete;
    NativeDialogueOutput& operator=(const NativeDialogueOutput&)=delete;
};
bool remote_dialogue_output_active();
bool capture_remote_dialogue_answer(const char* text,bool player);
bool exercise_native_dialogue_topics_probe(std::uint64_t session,std::uint16_t actor,
    std::uint64_t entity,std::uint32_t generation,std::uint32_t level);
bool apply_shared_quests(std::uint64_t session,std::uint32_t level,const coopnet::QuestState& quests);
void exercise_shared_world_probe(double elapsed,unsigned& phase,double& wait,std::uint16_t& object);
bool capture_world_objects(std::uint32_t& level,std::vector<NativeWorldPose>& objects);
// Owner-thread host bootstrap only. Captures native server/client save state;
// this does not grant a lease or enable guest simulation.
bool capture_native_region_checkpoint(const coopnet::RegionKey& key,double width,
    coopnet::NativeRegionCheckpoint& checkpoint);
bool capture_offline_region_checkpoint(const coopnet::RegionKey& key,double width,
    coopnet::NativeRegionCheckpoint& checkpoint);
bool exercise_offline_checkpoint_probe(double width);
class RetainedOfflineRegion;
// Minted from a local native capture, never from a worker-supplied packet.
std::shared_ptr<const RetainedOfflineRegion> retain_offline_region(const coopnet::RegionKey& key,double width);
bool restore_retained_offline_region(const std::shared_ptr<const RetainedOfflineRegion>& checkpoint);
// Guest bootstrap from an authenticated host checkpoint, preserving offline aliases.
bool restore_host_offline_region(const coopnet::NativeRegionCheckpoint& checkpoint);
bool capture_bootstrap_offline_checkpoint(double width,coopnet::NativeRegionCheckpoint& checkpoint);
bool apply_world_object(std::uint64_t session,std::uint64_t anchor,std::uint64_t incarnation,
    const float* position,const float* rotation,float health,const std::vector<coopnet::WorldAnimation>& animations={},
    std::uint8_t zone_state=255,std::uint32_t zone_time=0,std::uint64_t killer=0,bool killer_player=false,const coopnet::WorldPhysics& physics={});
// Owner-thread cell execution state. Native restore and host activation are
// separate barriers; prefetched foreign maps never become execution owners.
bool prepare_native_acceptance_container();
bool relocate_fixture_map_actor(CSE_Abstract* source);
bool native_gameplay_frame_ready();
void configure_cell_simulation(std::uint64_t player,std::uint32_t level,double width,bool coordinator);
bool observe_cell_lease(const coopnet::RegionLease& lease);
bool confirm_cell_native_restore(const coopnet::RegionKey& key,std::uint64_t epoch,std::uint32_t revision);
// Adopt the coordinator's home cell for each checkpointed anchor; returns how many matched.
unsigned adopt_cell_roots(std::uint64_t session,const coopnet::RegionKey& key,const std::vector<std::uint64_t>& anchors);
bool has_owned_cell_simulation();
bool native_alife_authorized(const CSE_Abstract* object);
bool world_level_is_replica();
void audit_script_storage_on_destroy(CGameObject* object);
bool capture_death_camera(float* position,float* rotation);
void orbit_death_camera(float yaw,float pitch);
bool exercise_death_orbit_probe();
// Development migration: retain joined-world identity while restoring local native simulation.
bool guest_native_world();
bool restore_personal_quests();
bool capture_personal_state(std::vector<std::uint8_t>& bytes);
bool queue_personal_state(const std::vector<std::uint8_t>& bytes);
void reset_personal_character();
void set_personal_world_scope(std::uint64_t scope);
void personal_loot_changed(CGameObject* source);
void update_personal_loot();
void exercise_native_acceptance_probe(double elapsed);
bool native_character_ready();
bool set_private_quest_follower(std::uint16_t object, bool private_follower);
bool native_world_requested();
void set_native_world_policy(bool active);
bool passive_world_replica();
void world_object_spawned(CGameObject* object,const CSE_Abstract* source);
void world_object_destroyed(CGameObject* object);
bool world_replica_object(const CGameObject* object);
bool update_world_replica(CObject* object);
void finalize_world_replica(CObject* object);
bool schedule_world_replica(ISheduled* object,std::uint32_t elapsed);
void world_level_stopped();
}

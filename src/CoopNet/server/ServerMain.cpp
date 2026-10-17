#include <windows.h>
#include <bcrypt.h>
#include "../GnsTransport.h"
#include "../DedicatedBootstrap.h"
#include "../ServerWorldArchive.h"
#include "../CharacterSave.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
namespace fs=std::filesystem;
using namespace coopnet;
namespace {
std::atomic<bool> stopping{false};
BOOL WINAPI interrupted(DWORD event) {
    if(event==CTRL_C_EVENT || event==CTRL_BREAK_EVENT || event==CTRL_CLOSE_EVENT) {stopping=true;return TRUE;}
    return FALSE;
}
struct Lock {
    HANDLE handle=INVALID_HANDLE_VALUE;
    explicit Lock(const fs::path& path) {handle=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(handle==INVALID_HANDLE_VALUE)throw std::runtime_error("Server data directory already in use or inaccessible");}
    ~Lock(){if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);}
};
Identity random_identity() {
    Identity value=0;
    do {if(BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&value),sizeof(value),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)throw std::runtime_error("Server random source failed");}while(!value);
    return value;
}
BaselineDigest digest(const std::vector<std::uint8_t>& bytes) {
    BaselineDigest output{};
    if(bytes.size()>max_baseline_bytes || BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,const_cast<PUCHAR>(bytes.data()),static_cast<ULONG>(bytes.size()),output.data(),static_cast<ULONG>(output.size()))<0)
        throw std::runtime_error("World SHA-256 failed");
    return output;
}
std::vector<std::uint8_t> read_file(const fs::path& path,std::size_t limit) {
    std::ifstream input(path,std::ios::binary|std::ios::ate);
    if(!input)throw std::runtime_error("Cannot read server checkpoint: "+path.string());
    const auto length=input.tellg();
    if(length<0 || static_cast<std::uint64_t>(length)>limit)throw std::runtime_error("Invalid checkpoint size");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));input.seekg(0);
    if(!bytes.empty() && !input.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())))throw std::runtime_error("Checkpoint read failed");
    return bytes;
}
void write_verified(const fs::path& path,const std::vector<std::uint8_t>& bytes) {
    auto temporary=path;temporary+=L".partial";
    {std::ofstream output(temporary,std::ios::binary|std::ios::trunc);if(!output || !output.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())))throw std::runtime_error("Checkpoint write failed");output.flush();if(!output)throw std::runtime_error("Checkpoint flush failed");}
    if(read_file(temporary,bytes.size())!=bytes)throw std::runtime_error("Checkpoint readback failed");
    if(fs::exists(path)) {auto backup=path;backup+=L".bak";if(!CopyFileW(path.c_str(),backup.c_str(),FALSE))throw std::runtime_error("Checkpoint backup failed");}
    if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Checkpoint replacement failed");
}
std::uint64_t number(const std::map<std::string,std::string>& options,const std::string& name,std::uint64_t fallback) {
    const auto found=options.find(name);if(found==options.end())return fallback;
    if(found->second.empty() || found->second.find_first_not_of("0123456789")!=std::string::npos)throw std::invalid_argument("Invalid numeric option: "+name);
    std::size_t consumed=0;const auto value=std::stoull(found->second,&consumed);
    if(consumed!=found->second.size())throw std::invalid_argument("Invalid numeric option: "+name);return value;
}
}
int main(int argc,char** argv) {
    try {
        std::map<std::string,std::string> options;
        const std::set<std::string> allowed{"--data","--world","--level","--game","--mods","--port","--run-seconds","--experimental-bootstrap","--experimental-simulation"};
        for(int i=1;i<argc;i+=2) {
            if(i+1>=argc || !allowed.count(argv[i]) || !options.emplace(argv[i],argv[i+1]).second)throw std::invalid_argument("Invalid or duplicate server option");
        }
        if(!options.count("--data") || number(options,"--experimental-bootstrap",0)!=1)
            throw std::invalid_argument("Development server: --experimental-bootstrap 1 --data DIR [--world SAVE.scop --level NETWORK_LEVEL] [--port 27888]");
        const auto data=fs::absolute(fs::u8path(options.at("--data")));fs::create_directories(data);
        Lock lock(data/"server.lock");
        const auto archive_path=data/"world.bootstrap";
        ServerWorldArchive archive;
        if(fs::exists(archive_path)) {
            if(!decode_server_world_archive(read_file(archive_path,max_baseline_bytes+server_world_header_bytes),archive) || digest(archive.bytes)!=archive.world.digest)
                throw std::runtime_error("Canonical world archive is corrupt; previous data retained");
            if(options.count("--world") && digest(read_file(fs::u8path(options.at("--world")),max_baseline_bytes))!=archive.world.digest)
                throw std::runtime_error("Requested seed differs from this server's canonical world");
        } else {
            if(!options.count("--world"))throw std::invalid_argument("First startup requires --world and --level");
            const auto level=number(options,"--level",0);if(!level || level>UINT32_MAX)throw std::invalid_argument("Invalid bootstrap level");
            archive.bytes=read_file(fs::u8path(options.at("--world")),max_baseline_bytes);
            if(archive.bytes.size()<12)throw std::runtime_error("Invalid native world seed");
            std::uint32_t header=0;std::memcpy(&header,archive.bytes.data(),4);
            if(header!=UINT32_MAX)throw std::runtime_error("World seed is not a native ALife save");
            archive.build={number(options,"--game",1),number(options,"--mods",1)};
            archive.world={random_identity(),static_cast<std::uint32_t>(level),static_cast<std::uint32_t>(archive.bytes.size()),digest(archive.bytes),random_identity()};
            write_verified(archive_path,encode_server_world_archive(archive));
        }
        if((options.count("--game") && number(options,"--game",0)!=archive.build.game) ||
            (options.count("--mods") && number(options,"--mods",0)!=archive.build.mods) ||
            (options.count("--level") && number(options,"--level",0)!=archive.world.level))throw std::invalid_argument("Options conflict with canonical world metadata");
        const auto port=number(options,"--port",27888);if(!port || port>65535)throw std::invalid_argument("Invalid server port");
        const auto seconds=number(options,"--run-seconds",0);
        const auto simulation=number(options,"--experimental-simulation",0);
        if(simulation>1)throw std::invalid_argument("Invalid experimental simulation option");
        const auto character_directory=data/"characters";fs::create_directories(character_directory);
        GnsRuntime network(4);DedicatedBootstrap coordinator;
        auto baseline=std::make_shared<const std::vector<std::uint8_t>>(archive.bytes);
        coordinator.start(random_identity(),archive.build,archive.world,baseline,random_identity,[&](Identity,InventoryView& incoming) {
            const auto path=character_directory/(std::to_string(archive.world.scope)+"-"+std::to_string(incoming.actor)+".coopchar");
            if(fs::exists(path)) {
                engine_coopnet::CharacterSave saved;
                if(!engine_coopnet::decode_character_save(read_file(path,shared_limit),saved) || saved.character!=incoming.actor || saved.inventory.actor!=incoming.actor)
                    throw std::runtime_error("Character checkpoint corrupt; previous data retained");
                incoming=saved.inventory;
                std::cout<<"CHARACTER_CHECKPOINT_RESTORED "<<incoming.actor<<'\n'<<std::flush;
            } else {
                engine_coopnet::CharacterSave saved;saved.character=incoming.actor;saved.name="Player";saved.inventory=incoming;
                if(incoming.has_condition)saved.condition={incoming.health,incoming.power,incoming.radiation};
                write_verified(path,engine_coopnet::encode_character_save(saved));
            }
            std::cout<<"CHARACTER_CHECKPOINT_ACCEPTED "<<incoming.actor<<" items "<<incoming.items.size()<<'\n'<<std::flush;
            return true;
        });
        if(simulation)coordinator.enable_experimental_simulation([](Identity player,const Frame& frame){std::cout<<"SIMULATION_REJECTED player "<<player<<" message "<<static_cast<unsigned>(frame.message)<<" sequence "<<frame.sequence<<'\n'<<std::flush;});
        if(!network.listen(static_cast<std::uint16_t>(port)))throw std::runtime_error("Server listen failed");
        SetConsoleCtrlHandler(interrupted,TRUE);
        std::cout<<"EXPERIMENTAL_BOOTSTRAP_LISTENING port "<<port<<" level "<<archive.world.level<<" bytes "<<archive.world.size<<" scope "<<archive.world.scope<<"\nExperimental simulation coordinator "<<simulation<<". Native worker integration and ongoing checkpoints remain under development.\n"<<std::flush;
        auto previous=std::chrono::steady_clock::now();const auto began=previous;
        unsigned ready=0;
        while(!stopping) {
            const auto now=std::chrono::steady_clock::now();
            if(seconds && std::chrono::duration<double>(now-began).count()>=static_cast<double>(seconds))break;
            const double elapsed=std::chrono::duration<double>(now-previous).count();previous=now;
            network.poll();
            for(auto pending=network.take_pending();pending!=k_HSteamNetConnection_Invalid;pending=network.take_pending())
                if(!coordinator.host().attach(pending,std::make_unique<GnsTransport>(network,pending)))throw std::runtime_error("Server attach failed");
            coordinator.update(elapsed);
            if(ready!=coordinator.host().ready_participants()) {ready=coordinator.host().ready_participants();std::cout<<"ADMITTED_CLIENTS "<<ready<<'\n'<<std::flush;}
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        if(const auto* cells=coordinator.simulation())std::cout<<"SIMULATION_ACCEPTED catalogues "<<cells->accepted_catalogues()<<" states "<<cells->accepted_states()<<" handoffs "<<cells->handoffs()<<" routed_hits "<<cells->routed_hits()<<'\n';
        coordinator.stop();std::cout<<"SERVER_STOPPED\n";return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}

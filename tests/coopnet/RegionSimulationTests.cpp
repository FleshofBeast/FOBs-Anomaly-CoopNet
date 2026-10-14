#include "../../src/CoopNet/RegionSimulation.h"
#include <iostream>
#include <stdexcept>
using namespace coopnet;
static void require(bool value) {if(!value) throw std::runtime_error("Native execution fence failed");}
int main() {
    RegionSimulation guest,host;
    require(guest.configure(2,14,750,false));require(host.configure(1,14,750,true));
    const RegionKey key{14,0,0};const std::array<float,3> point{10,0,10};
    require(!guest.permits(14,point) && host.permits(14,point));
    RegionLease lease{key,1,2,0,1,RegionStage::Restoring};
    require(guest.observe(lease) && host.observe(lease));
    require(!guest.permits(14,point) && !host.permits(14,point));
    require(!guest.native_restored({key,2,1}) && !guest.native_restored({key,1,2}));
    require(guest.native_restored({key,1,1}));require(!guest.has_owned());
    lease.stage=RegionStage::Active;require(guest.observe(lease) && host.observe(lease));
    require(guest.has_owned() && guest.permits(14,point) && !host.permits(14,point));
    require(!guest.permits(15,point) && host.permits(15,point));
    const RegionLease foreign{{15,0,0},7,3,0,1,RegionStage::Active};
    require(host.observe(foreign) && !host.permits(15,point)); // Host offline AI must not duplicate a foreign worker.
    require(!guest.observe(foreign));
    require(!guest.permits(14,{800,0,10})); // Adjacent cells require their own lease.
    RegionSimulation unprepared;require(unprepared.configure(2,14,750,false));
    require(unprepared.observe(lease) && !unprepared.has_owned()); // Active alone is insufficient.
    RegionLease quiescing{key,2,0,2,1,RegionStage::Quiescing};
    require(guest.observe(quiescing));require(!guest.permits(14,point));
    require(!guest.observe(lease)); // Delayed Active cannot revive a revoked epoch.
    lease={key,2,3,0,2,RegionStage::Restoring};
    require(guest.observe(lease)); // Host completes its quiescence barrier in this epoch.
    lease.epoch=3;require(guest.observe(lease));require(!guest.native_restored({key,3,2}));
    require(guest.configure(2,15,750,false));require(!guest.observe(lease) && !guest.has_owned());
    require(!guest.native_restored({key,1,1})); // Prefetched foreign maps cannot execute.
    require(guest.configure(2,14,750,false));require(!guest.has_owned());
    std::cout<<"Region simulation: PASS (native restore/host activation barrier, map isolation, revocation and stale ownership)\n";
}

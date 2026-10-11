#pragma once
class CSE_Abstract;
namespace engine_coopnet {
// ALife registry lifetimes survive native online/offline switches. A reused
// object ID or allocation gets a new identity when it is registered again.
void alife_lifetime_started(const CSE_Abstract* object);
void alife_lifetime_ended(const CSE_Abstract* object);
}

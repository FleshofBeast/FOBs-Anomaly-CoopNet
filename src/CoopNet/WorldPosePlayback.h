#pragma once
#include <array>
#include <cmath>
#include <cstdint>
namespace coopnet {
// Presentation only: never extrapolate AI decisions or delay gameplay state.
class WorldPosePlayback {
    std::array<float,3> from_position_{},to_position_{},from_rotation_{},to_rotation_{};
    std::uint32_t started_=0;
    bool ready_=false;
public:
    void sample(std::uint32_t now,std::array<float,3>& position,std::array<float,3>& rotation) const {
        const auto elapsed=static_cast<std::uint32_t>(now-started_);
        const float t=elapsed>=100 ? 1.f : static_cast<float>(elapsed)/100.f;
        for (unsigned i=0;i<3;++i) {
            position[i]=from_position_[i]+(to_position_[i]-from_position_[i])*t;
            rotation[i]=from_rotation_[i]+std::remainder(to_rotation_[i]-from_rotation_[i],6.28318530718f)*t;
        }
    }
    void receive(std::uint32_t now,const float* position,const float* rotation,bool snap=false) {
        std::array<float,3> current{},angles{};
        if (ready_) sample(now,current,angles);
        float distance=0;
        for (unsigned i=0;i<3;++i) distance+=(position[i]-current[i])*(position[i]-current[i]);
        snap=snap || !ready_ || distance>25.f;
        for (unsigned i=0;i<3;++i) {
            from_position_[i]=snap ? position[i] : current[i]; to_position_[i]=position[i];
            from_rotation_[i]=snap ? rotation[i] : angles[i]; to_rotation_[i]=rotation[i];
        }
        started_=now; ready_=true;
    }
    bool ready() const { return ready_; }
};
}

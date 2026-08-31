#pragma once
#include "common.h"
namespace d3dref9 {
class InstantSniper {
public: static InstantSniper& Instance(); void Configure(DWORD vk); void OnKey(DWORD vk,bool down); void ToggleMode(); void ToggleAimFire(); bool Busy()const{return busy_.load();} bool SniperMode()const{return sniperMode_.load();} bool AimFireEnabled()const; uint32_t CooldownLeft()const;
private: InstantSniper()=default; std::atomic<DWORD> trigger_{VK_F6}; std::atomic<bool> busy_{false},sniperMode_{false}; std::atomic<uint64_t> cooldownEnd_{}; std::atomic<uint64_t> aimToggleEpoch_{}; void Run();
};
}

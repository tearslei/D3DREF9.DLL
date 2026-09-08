#pragma once
#include "common.h"
namespace d3dref9 {
class InstantSniper {
public: static InstantSniper& Instance(); void OnKey(DWORD vk,bool down); void OnMouseButton(DWORD button,bool down); void ToggleMode(); void ToggleAimFire(); bool Busy()const{return busy_.load();} bool SniperMode()const{return sniperMode_.load();} bool AimFireEnabled()const; uint32_t CooldownLeft()const;
private: InstantSniper()=default; std::atomic<bool> busy_{false},sniperMode_{false}; std::atomic<uint64_t> cooldownEnd_{}; std::atomic<uint64_t> aimToggleEpoch_{}; void Run();
 uint32_t scopeDelayMin_{15}, scopeDelayMax_{30};
 uint32_t fireHoldMin_{8}, fireHoldMax_{15};
 uint32_t unscopeDelayMin_{10}, unscopeDelayMax_{25};
 uint32_t recoveryMin_{30}, recoveryMax_{80};
 uint32_t cooldownMin_{180}, cooldownMax_{300};
 bool pauseAim_{true};
 bool pauseAutoFire_{true};
 void LoadConfig();
};
}

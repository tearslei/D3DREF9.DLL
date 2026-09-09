#pragma once
#include "common.h"
namespace d3dref9 {
class InputRouter;
class InstantSniper {
public: static InstantSniper& Instance(); void OnKey(DWORD vk,bool down); void OnMouseButton(DWORD button,bool down); void ToggleMode(); void ToggleAimFire(); bool Busy()const{return busy_.load();} bool SniperMode()const{return sniperMode_.load();} bool AimFireEnabled()const; uint32_t CooldownLeft()const;
private: InstantSniper()=default; std::atomic<bool> busy_{false},sniperMode_{false}; std::atomic<uint64_t> cooldownEnd_{}; std::atomic<uint64_t> aimToggleEpoch_{}; void Run();
 uint32_t scopeDelayMin_{15}, scopeDelayMax_{30};
 uint32_t fireHoldMin_{8}, fireHoldMax_{15};
 uint32_t unscopeDelayMin_{10}, unscopeDelayMax_{25};
 uint32_t recoveryMin_{30}, recoveryMax_{80};
 uint32_t cooldownMin_{180}, cooldownMax_{300};
 uint32_t switch3DelayMin_{19}, switch3DelayMax_{21};
 uint32_t switch3HoldMin_{45}, switch3HoldMax_{45};
 uint32_t switch1DelayMin_{100}, switch1DelayMax_{100};
 uint32_t switch1HoldMin_{25}, switch1HoldMax_{25};
 uint32_t postSwitchCooldownMs_{300};
 uint32_t movementBrakeMs_{25};
 uint32_t movementRestoreDelayMs_{35};
 bool movementBrake_{true};
 bool pauseAim_{true};
 bool pauseAutoFire_{true};
 void LoadConfig();
 bool BrakeMovement(InputRouter& r, std::array<WORD, 4>& released, size_t& count);
 void RestoreMovement(InputRouter& r, const std::array<WORD, 4>& released, size_t count);
};
}

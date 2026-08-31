#pragma once
#include "common.h"
namespace d3dref9 {
class MacroEngine {
public:
 static MacroEngine& Instance();
 void Start(); void Stop(); void Toggle(); void EmergencyStop();
 bool Enabled()const{return enabled_.load();} bool Running()const{return running_.load();} uint32_t Action()const{return action_.load();} uint32_t Count()const{return count_.load();}
 void OnKey(DWORD vk,bool down);
private:
 MacroEngine()=default; std::atomic<bool> enabled_{true}, emergency_{false}, running_{false}; std::atomic<uint32_t> action_{0},count_{0}; std::atomic<uint64_t> generation_{0};
 std::mutex keyMu_; std::array<bool,256> down_{};
 bool Begin(uint32_t a); bool Alive(uint32_t a,uint64_t g)const; void End(uint32_t a); void Launch(uint32_t a,void(MacroEngine::*fn)(uint32_t,uint64_t));
 void LCtrl(uint32_t,uint64_t); void WF(uint32_t,uint64_t); void WC(uint32_t,uint64_t); void WLAlt(uint32_t,uint64_t); void SF(uint32_t,uint64_t); void SC(uint32_t,uint64_t); void SLAlt(uint32_t,uint64_t); void ZX(uint32_t,uint64_t); void ZC(uint32_t,uint64_t); void TurnLeft(uint32_t,uint64_t); void TurnRight(uint32_t,uint64_t); void Turn(uint32_t,uint64_t,bool);
};
}

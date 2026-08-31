#include "instant_sniper.h"
#include "input_router.h"
#include "feature_manager.h"
#include "aim_controller.h"
namespace d3dref9 {
InstantSniper& InstantSniper::Instance(){static InstantSniper s;return s;}
void InstantSniper::Configure(DWORD vk){trigger_=vk;}
bool InstantSniper::AimFireEnabled()const{return Features().IsOn(Feature::AimAutoFire);}
void InstantSniper::ToggleMode(){sniperMode_=!sniperMode_.load();}
void InstantSniper::ToggleAimFire(){aimToggleEpoch_++;Features().Toggle(Feature::AimAutoFire);}
void InstantSniper::OnKey(DWORD vk,bool down){auto&r=InputRouter::Instance();if(down&&vk=='L'){ToggleMode();return;}if(down&&vk=='Z'&&r.Physical(VK_LMENU)){ToggleAimFire();return;}if(!down||!sniperMode_.load()||vk!=trigger_.load()||busy_.exchange(true))return;std::thread(&InstantSniper::Run,this).detach();}
uint32_t InstantSniper::CooldownLeft()const{auto e=cooldownEnd_.load();auto n=GetTickCount64();return e>n?(uint32_t)(e-n):0;}
void InstantSniper::Run(){auto&r=InputRouter::Instance();std::mt19937 gen((uint32_t)GetTickCount());std::uniform_int_distribution<int>dist(0,100);int r1=dist(gen),r2=dist(gen);uint64_t epoch=aimToggleEpoch_.load();bool wasAim=Features().IsOn(Feature::AimAutoFire);if(wasAim)Features().Set(Feature::AimAutoFire,false);AimBone selected=AimBone::Neck; // 运行时目标扫描器会用 SelectNearest() 返回头/颈/胸/中腰/臀最近点
r.MouseButton(MOUSEEVENTF_RIGHTDOWN,true,InputOwner::Sniper);SleepMs((uint32_t)r1);r.MouseButton(MOUSEEVENTF_LEFTDOWN,true,InputOwner::Sniper);SleepMs((uint32_t)r2);r.KeyScan(0x004,true,InputOwner::Sniper);r.KeyScan(0x004,false,InputOwner::Sniper);SleepMs(35);r.KeyScan(0x002,true,InputOwner::Sniper);r.KeyScan(0x002,false,InputOwner::Sniper);r.MouseButton(MOUSEEVENTF_LEFTDOWN,false,InputOwner::Sniper);r.MouseButton(MOUSEEVENTF_RIGHTDOWN,false,InputOwner::Sniper);uint32_t cd=1565u-(uint32_t)r1-(uint32_t)r2;cooldownEnd_=GetTickCount64()+cd;SleepMs(cd);if(wasAim&&aimToggleEpoch_.load()==epoch)Features().Set(Feature::AimAutoFire,true);busy_=false;}
}


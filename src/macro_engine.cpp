#include "macro_engine.h"
#include "input_router.h"
#include <algorithm>
#include <cmath>

namespace d3dref9 {
namespace {
constexpr WORD SC_W=0x011, SC_S=0x01F, SC_A=0x01E, SC_D=0x020;
constexpr WORD SC_SPACE=0x039, SC_CTRL=0x01D, SC_SHIFT=0x02A;
void MacroLog(const char*,uint32_t=0,uint32_t=0) {}
bool FeatureModifierHeld(const InputRouter& r){return r.Physical(VK_F7)||r.Physical(VK_F9)||r.Physical(VK_F10);}
void MoveRelativeSmooth(int logicalDx,int logicalDy){
 HDC dc=GetDC(nullptr); int dpi=dc?GetDeviceCaps(dc,LOGPIXELSX):96; if(dc)ReleaseDC(nullptr,dc);
 double scale=96.0/(double)(std::max)(dpi,96); int dx=(int)std::lround(logicalDx*scale); int dy=(int)std::lround(logicalDy*scale);
 if(logicalDx!=0)dy+=(int)(GetTickCount()%3u)-1;
 int oldMouse[3]{}; UINT oldSpeed=0; bool haveMouse=SystemParametersInfoA(SPI_GETMOUSE,0,oldMouse,0)!=FALSE; bool haveSpeed=SystemParametersInfoA(SPI_GETMOUSESPEED,0,&oldSpeed,0)!=FALSE;
 if(haveMouse){int off[3]={0,0,0};SystemParametersInfoA(SPI_SETMOUSE,0,off,SPIF_SENDCHANGE);} if(haveSpeed){UINT s=10;SystemParametersInfoA(SPI_SETMOUSESPEED,0,(void*)(ULONG_PTR)s,SPIF_SENDCHANGE);}
 INPUT in{};in.type=INPUT_MOUSE;in.mi.dx=dx;in.mi.dy=dy;in.mi.dwFlags=MOUSEEVENTF_MOVE;SendInput(1,&in,sizeof(in));
 if(haveSpeed)SystemParametersInfoA(SPI_SETMOUSESPEED,0,(void*)(ULONG_PTR)oldSpeed,SPIF_SENDCHANGE); if(haveMouse)SystemParametersInfoA(SPI_SETMOUSE,0,oldMouse,SPIF_SENDCHANGE);
}
}
MacroEngine& MacroEngine::Instance(){static MacroEngine m;return m;}
void MacroEngine::Start(){emergency_=false;enabled_=true;MacroLog("start");}
void MacroEngine::Stop(){emergency_=true;enabled_=false;generation_++;action_=0;running_=false;InputRouter::Instance().ReleaseOwned(InputOwner::Macro);MacroLog("stop");}
void MacroEngine::Toggle(){if(enabled_){enabled_=false;emergency_=false;generation_++;action_=0;running_=false;InputRouter::Instance().ReleaseOwned(InputOwner::Macro);MacroLog("pause");}else Start();}
void MacroEngine::EmergencyStop(){Stop();}
bool MacroEngine::Alive(uint32_t a,uint64_t g)const{return enabled_&&!emergency_&&running_&&action_==a&&generation_==g;}
bool MacroEngine::Wait(uint32_t a,uint64_t g,double ms){auto ns=(uint64_t)((std::max)(0.0,ms)*1000000.0);auto deadline=std::chrono::steady_clock::now()+std::chrono::nanoseconds(ns);for(;;){if(!Alive(a,g))return false;auto left=deadline-std::chrono::steady_clock::now();if(left<=std::chrono::nanoseconds::zero())return true;if(left>std::chrono::milliseconds(2))::Sleep(1);else std::this_thread::yield();}}
void MacroEngine::End(uint32_t a,uint64_t g){if(action_!=a||generation_!=g)return;InputRouter::Instance().ReleaseOwned(InputOwner::Macro);action_=0;running_=false;MacroLog("end",a,count_);}
void MacroEngine::Launch(uint32_t a,void(MacroEngine::*fn)(uint32_t,uint64_t)){if(!enabled_||emergency_)return;auto g=++generation_;InputRouter::Instance().ReleaseOwned(InputOwner::Macro);action_=a;count_=0;running_=true;std::thread([this,a,g,fn]{(this->*fn)(a,g);End(a,g);}).detach();}
void MacroEngine::OnKey(DWORD vk,bool down){if(vk<down_.size()){std::lock_guard<std::mutex>l(keyMu_);down_[vk]=down;}auto&r=InputRouter::Instance();if(vk==VK_OEM_4&&down){Toggle();return;}if(vk==VK_OEM_MINUS&&down){Stop();return;}if(!enabled_||emergency_||FeatureModifierHeld(r))return;auto physical=[&r](DWORD k){return r.Physical(k);};if(!down)return;if(vk=='F'&&physical('W'))Launch(2,&MacroEngine::WF);else if(vk=='C'&&physical('W'))Launch(3,&MacroEngine::WC);else if(vk==VK_LMENU&&physical('W'))Launch(4,&MacroEngine::WLAlt);else if(vk=='X')Launch(10,&MacroEngine::TurnLeft);else if(vk=='V')Launch(11,&MacroEngine::TurnRight);}
void MacroEngine::WF(uint32_t a,uint64_t g){auto&r=InputRouter::Instance();if(!r.Physical('W'))return;auto tap=[&](WORD s,DWORD vk,double h,double gap){bool own=!r.Physical(vk);if(own)r.KeyScan(s,true,InputOwner::Macro);if(!Wait(a,g,h))return false;if(own)r.KeyScan(s,false,InputOwner::Macro);return Wait(a,g,gap);};if(!tap(SC_SPACE,VK_SPACE,99.76,99.76))return;bool ownCtrl=!r.Physical(VK_LCONTROL);if(ownCtrl)r.KeyScan(SC_CTRL,true,InputOwner::Macro);if(!Wait(a,g,100.0))return;for(uint32_t i=0;i<300&&Alive(a,g)&&r.Physical('W');++i){if(!tap(SC_SPACE,VK_SPACE,9.76,9.76))return;count_=i+1;}if(ownCtrl)r.KeyScan(SC_CTRL,false,InputOwner::Macro);}
void MacroEngine::WC(uint32_t a,uint64_t g){auto&r=InputRouter::Instance();if(!r.Physical('W'))return;auto tap=[&](WORD s,DWORD vk,double h,double gap){bool own=!r.Physical(vk);if(own)r.KeyScan(s,true,InputOwner::Macro);if(!Wait(a,g,h))return false;if(own)r.KeyScan(s,false,InputOwner::Macro);return Wait(a,g,gap);};for(uint32_t i=0;i<40&&Alive(a,g);++i){if(!tap(SC_SPACE,VK_SPACE,9.76,9.76))return;count_=i+1;}tap(SC_CTRL,VK_LCONTROL,99.76,19.76);}
void MacroEngine::WLAlt(uint32_t a,uint64_t g){auto&r=InputRouter::Instance();auto tap=[&](WORD s,DWORD vk,double h,double gap){bool own=!r.Physical(vk);if(own)r.KeyScan(s,true,InputOwner::Macro);if(!Wait(a,g,h))return false;if(own)r.KeyScan(s,false,InputOwner::Macro);return Wait(a,g,gap);};if(!tap(SC_SPACE,VK_SPACE,39.76,19.76))return;if(r.Physical(VK_LBUTTON)){if(!tap(SC_CTRL,VK_LCONTROL,199.76,9.76))return;}else if(!Wait(a,g,210.0))return;uint32_t n=0;while(Alive(a,g)&&r.Physical(VK_LMENU)){++n;if(n<=10){if(!tap(SC_CTRL,VK_LCONTROL,29.76,29.76))return;}else{bool own=!r.Physical(VK_LCONTROL);if(own)r.KeyScan(SC_CTRL,true,InputOwner::Macro);while(Alive(a,g)&&r.Physical(VK_LMENU)){if(!Wait(a,g,30.0))return;}if(own)r.KeyScan(SC_CTRL,false,InputOwner::Macro);}count_=n;}}
void MacroEngine::Turn(uint32_t a,uint64_t g,bool right){auto&r=InputRouter::Instance();DWORD sideVk=right?'A':'D';WORD sideScan=right?SC_A:SC_D;bool ownS=!r.Physical('S'),ownSide=!r.Physical(sideVk),ownCtrl=!r.Physical(VK_LCONTROL);if(ownS)r.KeyScan(SC_S,true,InputOwner::Macro);if(!Wait(a,g,180.0))return;auto tapSpace=[&](){bool own=!r.Physical(VK_SPACE);if(own)r.KeyScan(SC_SPACE,true,InputOwner::Macro);if(!Wait(a,g,29.76))return false;if(own)r.KeyScan(SC_SPACE,false,InputOwner::Macro);return Wait(a,g,29.76);};if(!tapSpace())return;MoveRelativeSmooth(right?400:-400,0);if(ownSide)r.KeyScan(sideScan,true,InputOwner::Macro);if(ownCtrl)r.KeyScan(SC_CTRL,true,InputOwner::Macro);if(!Wait(a,g,r.Physical(VK_LBUTTON)?240.0:540.0))return;if(!tapSpace())return;MoveRelativeSmooth(-400,0);Wait(a,g,r.Physical(VK_LBUTTON)?400.0:880.0);if(ownSide)r.KeyScan(sideScan,false,InputOwner::Macro);if(ownS)r.KeyScan(SC_S,false,InputOwner::Macro);if(ownCtrl)r.KeyScan(SC_CTRL,false,InputOwner::Macro);}
void MacroEngine::TurnLeft(uint32_t a,uint64_t g){Turn(a,g,false);}void MacroEngine::TurnRight(uint32_t a,uint64_t g){Turn(a,g,true);}
}

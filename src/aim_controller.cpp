#include "aim_controller.h"
namespace d3dref9 {
AimController& AimController::Instance(){static AimController a;return a;}
void AimController::CycleNormal(){auto n=(uint8_t)normal_.load();normal_=(AimBone)((n+1)%3);}
void AimController::OnKey(DWORD,bool,bool){}
AimBone AimController::SelectNearest(const std::vector<AimPoint>& p,float cx,float cy)const{float best=1e30f;AimBone b=AimBone::Neck;for(const auto&t:p){if(t.bone!=AimBone::Head&&t.bone!=AimBone::Neck&&t.bone!=AimBone::Chest&&t.bone!=AimBone::Waist&&t.bone!=AimBone::Butt)continue;float dx=t.x-cx,dy=t.y-cy,d=dx*dx+dy*dy;if(d<best){best=d;b=t.bone;}}return p.empty()?AimBone::Neck:b;}
const wchar_t* AimController::NormalLabel()const{switch(normal_.load()){case AimBone::Head:return L"头部";case AimBone::Chest:return L"胸部";default:return L"颈部";}}
}

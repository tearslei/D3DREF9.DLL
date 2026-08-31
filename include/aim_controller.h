#pragma once
#include "common.h"
namespace d3dref9 {
enum class AimBone:uint8_t { Neck=0, Head=1, Chest=2, Waist=3, Butt=4 };
struct AimPoint { AimBone bone; float x; float y; };
class AimController {
public:
 static AimController& Instance();
 void OnKey(DWORD vk,bool down,bool sniperMode);
 void CycleNormal();
 AimBone NormalBone()const{return normal_.load();}
 AimBone SelectNearest(const std::vector<AimPoint>& points,float cx,float cy) const;
 const wchar_t* NormalLabel()const;
private: std::atomic<AimBone> normal_{AimBone::Neck};
};
}

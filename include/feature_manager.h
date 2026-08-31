#pragma once
#include "common.h"
namespace d3dref9 {
enum class Feature : uint32_t {
 OptimizeProcess, RoomStay, OldNoDamage,
 Radio, PlayerEsp, NoRecoil, InstantReload, BulletWall, Headshot, ThirdPerson,
 BunnyHop, PlayerNoclip, TeleportGround,
 FallNoDamage, InfiniteBackpack, AimAutoFire, Count
};
struct FeatureInfo { const wchar_t* label; bool visible; bool defaultOn; };
class FeatureManager {
public:
 FeatureManager();
 bool Toggle(Feature f); void Set(Feature f,bool on); bool IsOn(Feature f) const;
 std::vector<std::pair<std::wstring,bool>> PanelSnapshot() const;
 void ApplyStartup();
 void ApplyDelayedStartup();
 Feature VisibleFeatureAt(size_t row) const;
private: mutable std::mutex mu_; std::array<bool,(size_t)Feature::Count> state_{};
};
FeatureManager& Features();
const FeatureInfo& FeatureMeta(Feature f);
}

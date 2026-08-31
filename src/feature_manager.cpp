#include "feature_manager.h"
namespace d3dref9 {
static const std::array<FeatureInfo,(size_t)Feature::Count> kMeta{{
 {L"刷无线电",true,false},{L"人物透视",true,false},{L"无后坐力",true,false},{L"零秒换弹",true,false},{L"子弹穿墙",true,false},{L"刀枪爆头",true,false},{L"第三人称",true,false},{L"空格连跳",true,false},{L"人物穿墙",true,false},{L"瞬移通地",true,false},{L"摔不掉血",true,true},{L"无限背包",true,true},{L"自动瞄准+自动开枪",true,false}}};
FeatureManager::FeatureManager(){ ApplyStartup(); }
void FeatureManager::ApplyStartup(){ std::lock_guard<std::mutex> l(mu_); for(size_t i=0;i<state_.size();++i) state_[i]=kMeta[i].defaultOn; }
bool FeatureManager::Toggle(Feature f){std::lock_guard<std::mutex> l(mu_); auto&i=state_[(size_t)f]; i=!i; return i;}
void FeatureManager::Set(Feature f,bool on){std::lock_guard<std::mutex> l(mu_); state_[(size_t)f]=on;}
bool FeatureManager::IsOn(Feature f)const{std::lock_guard<std::mutex> l(mu_); return state_[(size_t)f];}
std::vector<std::pair<std::wstring,bool>> FeatureManager::PanelSnapshot()const{std::lock_guard<std::mutex> l(mu_);std::vector<std::pair<std::wstring,bool>> v;for(size_t i=0;i<state_.size();++i)if(kMeta[i].visible)v.push_back({kMeta[i].label,state_[i]});return v;}
FeatureManager& Features(){static FeatureManager f;return f;} const FeatureInfo& FeatureMeta(Feature f){return kMeta[(size_t)f];}
}

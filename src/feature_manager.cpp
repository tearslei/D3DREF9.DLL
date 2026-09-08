#include "feature_manager.h"
#include "game_handlers.h"
namespace d3dref9 {
static const std::array<FeatureInfo,(size_t)Feature::Count> kMeta = {{
 {L"两键优化游戏进程",false,false},{L"房间挂房不卡",false,false},
 {L"刷无线电",true,false},{L"人物透视",true,false},{L"无后坐力",true,false},{L"零秒换弹",true,false},{L"子弹穿墙",true,false},{L"刀枪爆头",true,false},{L"第三人称",true,false},{L"空格连跳",true,false},{L"人物穿墙",true,false},{L"瞬移通地",true,false},
 // 普通自瞄/自动开火必须由左 Alt+Z 显式开启；启动时保持关闭。
 {L"摔不掉血",false,false},{L"自动瞄准+自动开枪",false,false}}};

FeatureManager::FeatureManager(){ ApplyStartup(); }
void FeatureManager::ApplyStartup(){ std::lock_guard<std::mutex> l(mu_); for(size_t i=0;i<state_.size();++i) state_[i]=kMeta[i].defaultOn; master_=true; }
void FeatureManager::ApplyDelayedStartup(){ Set(Feature::OptimizeProcess,true); }
void FeatureManager::ApplyStartupHandlers(){
    Set(Feature::RoomStay,true);
    Set(Feature::FallNoDamage,true);
}
bool FeatureManager::Toggle(Feature f){
    if(f==Feature::Count) return false;
    bool v; {std::lock_guard<std::mutex> l(mu_); auto&i=state_[(size_t)f]; i=!i; v=i;}
    GameHandlers::Instance().Apply(f,v&&master_.load()); return v;
}
void FeatureManager::Set(Feature f,bool on){
    if(f==Feature::Count) return;
    {std::lock_guard<std::mutex> l(mu_); state_[(size_t)f]=on;}
    GameHandlers::Instance().Apply(f,on&&master_.load());
}
bool FeatureManager::IsOn(Feature f)const{
    if(f==Feature::Count) return false;
    std::lock_guard<std::mutex> l(mu_); return master_.load()&&state_[(size_t)f];
}
bool FeatureManager::ToggleMaster(){ bool on=!master_.load(); SetMaster(on); return on; }
void FeatureManager::SetMaster(bool on){ master_=on; for(size_t i=0;i<state_.size();++i){ bool enabled; {std::lock_guard<std::mutex> l(mu_); enabled=state_[i];} GameHandlers::Instance().Apply(static_cast<Feature>(i),on&&enabled); } }
bool FeatureManager::MasterEnabled()const{ return master_.load(); }
std::vector<std::pair<std::wstring,bool>> FeatureManager::PanelSnapshot()const{std::lock_guard<std::mutex> l(mu_);std::vector<std::pair<std::wstring,bool>> v;for(size_t i=0;i<state_.size();++i)if(kMeta[i].visible)v.push_back({kMeta[i].label,state_[i]});return v;}
Feature FeatureManager::VisibleFeatureAt(size_t row) const { size_t n=0; for(size_t i=0;i<kMeta.size();++i) if(kMeta[i].visible && n++==row) return (Feature)i; return Feature::Count; }
FeatureManager& Features(){static FeatureManager f;return f;} const FeatureInfo& FeatureMeta(Feature f){return kMeta[(size_t)f];}
}

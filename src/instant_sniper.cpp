#include "instant_sniper.h"
#include "input_router.h"
#include "feature_manager.h"
#include "aim_controller.h"
#include "game_handlers.h"
#include <cwctype>
#include <algorithm>
namespace d3dref9 {
namespace {
uint32_t ReadMs(const wchar_t* section, const wchar_t* key, uint32_t fallback,
                const std::wstring& path) {
    const auto v = GetPrivateProfileIntW(section, key, static_cast<INT>(fallback), path.c_str());
    return v < 0 ? fallback : static_cast<uint32_t>(v);
}
bool ReadBool(const wchar_t* section, const wchar_t* key, bool fallback,
             const std::wstring& path) {
    wchar_t value[16]{};
    GetPrivateProfileStringW(section, key, fallback ? L"true" : L"false",
                             value, static_cast<DWORD>(std::size(value)), path.c_str());
    return _wcsicmp(value, L"1") == 0 || _wcsicmp(value, L"true") == 0 ||
           _wcsicmp(value, L"yes") == 0 || _wcsicmp(value, L"on") == 0;
}
void NormaliseRange(uint32_t& lo, uint32_t& hi) { if (hi < lo) std::swap(lo, hi); }
}
InstantSniper& InstantSniper::Instance(){static InstantSniper s;static const bool loaded=(s.LoadConfig(),true);(void)loaded;return s;}
void InstantSniper::LoadConfig(){
    HMODULE hm=nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                       reinterpret_cast<LPCWSTR>(&InstantSniper::Instance),&hm);
    wchar_t module[MAX_PATH]{};
    if(!hm||!GetModuleFileNameW(hm,module,MAX_PATH))return;
    std::wstring path(module); auto slash=path.find_last_of(L"\\/");
    if(slash==std::wstring::npos)return; path.resize(slash);
    path+=L"\\config\\d3dref9自治.ini";
    // 每个阶段都支持 *_min_ms / *_max_ms；若只配置单值 *_ms，则该值
    // 同时作为上下界，便于在不改代码的情况下调参。
    auto range = [&](const wchar_t* base, uint32_t defLo, uint32_t defHi,
                     uint32_t& lo, uint32_t& hi) {
        std::wstring one=std::wstring(base)+L"_ms";
        std::wstring minKey=std::wstring(base)+L"_min_ms";
        std::wstring maxKey=std::wstring(base)+L"_max_ms";
        const uint32_t single=ReadMs(L"instant_sniper",one.c_str(),defLo,path);
        lo=ReadMs(L"instant_sniper",minKey.c_str(),single,path);
        hi=ReadMs(L"instant_sniper",maxKey.c_str(),single,path);
        NormaliseRange(lo,hi);
    };
    range(L"scope_delay",15,30,scopeDelayMin_,scopeDelayMax_);
    fireExtraDelayMs_=ReadMs(L"instant_sniper",L"fire_extra_delay_ms",70,path);
    range(L"fire_hold",8,15,fireHoldMin_,fireHoldMax_);
    range(L"unscope_delay",10,25,unscopeDelayMin_,unscopeDelayMax_);
    range(L"recovery",30,80,recoveryMin_,recoveryMax_);
    range(L"cooldown",180,300,cooldownMin_,cooldownMax_);
    range(L"switch3_delay",19,21,switch3DelayMin_,switch3DelayMax_);
    range(L"switch3_hold",45,45,switch3HoldMin_,switch3HoldMax_);
    range(L"switch1_delay",100,100,switch1DelayMin_,switch1DelayMax_);
    range(L"switch1_hold",25,25,switch1HoldMin_,switch1HoldMax_);
    postSwitchCooldownMs_=ReadMs(L"instant_sniper",L"post_switch_cooldown_ms",400,path);
    movementBrakeMs_=ReadMs(L"instant_sniper",L"movement_brake_ms",25,path);
    movementRestoreDelayMs_=ReadMs(L"instant_sniper",L"movement_restore_delay_ms",35,path);
    movementBrake_=ReadBool(L"instant_sniper",L"movement_brake",true,path);
    pauseAim_=ReadBool(L"instant_sniper",L"pause_aim",true,path);
    pauseAutoFire_=ReadBool(L"instant_sniper",L"pause_auto_fire",true,path);
}
bool InstantSniper::AimFireEnabled()const{return Features().IsOn(Feature::AimAutoFire);}
void InstantSniper::ToggleMode(){sniperMode_=!sniperMode_.load();}
void InstantSniper::ToggleAimFire(){aimToggleEpoch_++;Features().Toggle(Feature::AimAutoFire);}
void InstantSniper::OnKey(DWORD vk,bool down){
    auto& r=InputRouter::Instance();
    if(!down)return;
    if(vk=='L'){ToggleMode();return;}
    // Alt+Z is the sole keyboard switch for ordinary aim/fire. Alt+1 and F6
    // are intentionally not registered.
    if(vk=='Z' && (r.Physical(VK_LMENU) || (GetAsyncKeyState(VK_LMENU)&0x8000))){ToggleAimFire();return;}
}
void InstantSniper::OnMouseButton(DWORD button,bool down){
    // In sniper mode only the physical right button is the trigger. Ordinary
    // mode uses the physical left button as a hold-to-enable gate.
    if(!down||!Features().MasterEnabled()||!sniperMode_.load()||
       button!=VK_RBUTTON)return;
    if(busy_.exchange(true))return;
    std::thread(&InstantSniper::Run,this).detach();
}
uint32_t InstantSniper::CooldownLeft()const{auto e=cooldownEnd_.load();auto n=GetTickCount64();return e>n?(uint32_t)(e-n):0;}
bool InstantSniper::BrakeMovement(InputRouter& r, std::array<WORD, 4>& released, size_t& count) {
    count = 0;
    if (!movementBrake_) return true;
    // Release only keys physically held by the player. Macro-owned keys are
    // not touched, so W+F/W+C cannot be corrupted by the sniper brake.
    struct MoveKey { DWORD vk; WORD scan; };
    static constexpr MoveKey keys[] = {{'W',0x11},{'A',0x1E},{'S',0x1F},{'D',0x20}};
    for (const auto& k : keys) {
        if (r.Physical(k.vk) && !r.IsOwned(k.scan, InputOwner::Macro)) {
            r.KeyScan(k.scan, false, InputOwner::Sniper);
            released[count++] = k.scan;
        }
    }
    if (count) SleepMs(movementBrakeMs_);
    return r.Physical(VK_RBUTTON) && Features().MasterEnabled() && sniperMode_.load();
}
void InstantSniper::RestoreMovement(InputRouter& r, const std::array<WORD, 4>& released, size_t count) {
    if (!count) return;
    if (movementRestoreDelayMs_) SleepMs(movementRestoreDelayMs_);
    for (size_t i = 0; i < count; ++i) {
        DWORD vk = 0;
        switch (released[i]) {
        case 0x11: vk = 'W'; break;
        case 0x1E: vk = 'A'; break;
        case 0x1F: vk = 'S'; break;
        case 0x20: vk = 'D'; break;
        default: break;
        }
        // The player may have released a movement key during the shot. Do
        // not re-press it in that case.
        if (vk && r.Physical(vk)) r.KeyScan(released[i], true, InputOwner::Sniper);
    }
}
void InstantSniper::Run(){
    auto&r=InputRouter::Instance();
    std::mt19937 gen((uint32_t)GetTickCount64());
    auto delay=[&](uint32_t lo,uint32_t hi){
        std::uniform_int_distribution<uint32_t> d(lo,hi); return d(gen);
    };
    // Keep every stage interruptible.  A physical right-button release must
    // stop the sequence within a few milliseconds, including while the
    // scope/fire/recovery delays are in progress.
    auto held = [&]() {
        return Features().MasterEnabled() && sniperMode_.load() &&
               r.Physical(VK_RBUTTON);
    };
    auto waitHeld = [&](uint32_t ms) {
        uint32_t left = ms;
        while (left) {
            if (!held()) return false;
            const uint32_t step = (std::min)(left, 5u);
            SleepMs(step);
            left -= step;
        }
        return held();
    };
    // A physical RMB hold represents one continuous sniper session.  Send
    // exactly one synthetic RIGHTDOWN at session start and keep it held while
    // polling for a visible target.  Releasing/re-pressing RIGHT between
    // shots toggles CF's scope magnification and was the cause of the old
    // flashing-scope behaviour.
    if (!held()) { busy_ = false; return; }
    const uint64_t epoch=aimToggleEpoch_.load();
    const bool wasAim=Features().IsOn(Feature::AimAutoFire);
    std::array<WORD, 4> releasedMove{}; size_t releasedCount = 0;
    AimBone selected=AimBone::Neck; EntitySnapshot target{};
    bool rightDown=false,leftDown=false,pausedAim=false;
    auto restoreAim=[&]{
        if(pausedAim && wasAim && aimToggleEpoch_.load()==epoch)
            Features().Set(Feature::AimAutoFire,true);
        pausedAim=false;
    };
    auto cleanup=[&]{
        if(leftDown)r.MouseButton(MOUSEEVENTF_LEFTDOWN,false,InputOwner::Sniper);
        if(rightDown)r.MouseButton(MOUSEEVENTF_RIGHTDOWN,false,InputOwner::Sniper);
        restoreAim();
        RestoreMovement(r, releasedMove, releasedCount);
        releasedCount = 0;
    };
    r.MouseButton(MOUSEEVENTF_RIGHTDOWN,true,InputOwner::Sniper); rightDown=true;
    if (!waitHeld(delay(scopeDelayMin_,scopeDelayMax_))) { cleanup(); busy_=false; return; }

    while (held()) {
        const uint64_t cooldown = CooldownLeft();
        if (cooldown) {
            if (!waitHeld((std::min<uint64_t>)(5u, cooldown))) break;
            continue;
        }

        // No target is not an error: remain scoped and keep sampling until an
        // enemy enters the medium window and passes the visibility test.
        bool aimed = false;
        do {
            EntitySnapshot candidate{}; AimBone candidateBone = AimBone::Neck;
            if (GameHandlers::Instance().AcquireTarget(candidate, &candidateBone, true) &&
                GameHandlers::Instance().AimTarget(candidate, candidateBone)) {
                target = candidate; selected = candidateBone; aimed = true;
                break;
            }
            if (!waitHeld(5u)) break;
        } while (held());
        if (!aimed || !held()) break;

        // Keep updating the angle during the configured pre-fire delay so a
        // moving target cannot leave the aim point between scope and click.
        const uint64_t aimDeadline = GetTickCount64() + fireExtraDelayMs_;
        do {
            EntitySnapshot candidate{}; AimBone candidateBone = AimBone::Neck;
            if (GameHandlers::Instance().AcquireTarget(candidate, &candidateBone, true) &&
                GameHandlers::Instance().AimTarget(candidate, candidateBone)) {
                target = candidate; selected = candidateBone;
            }
            const uint64_t pollNow = GetTickCount64();
            if (!fireExtraDelayMs_ || pollNow >= aimDeadline) break;
            if (!waitHeld((std::min<uint32_t>)(5u,
                    static_cast<uint32_t>(aimDeadline - pollNow)))) break;
        } while (held());
        if (!held()) break;

        // Brake movement only for the actual shot, then reacquire once more
        // after the brake so the written angle corresponds to the final pose.
        releasedCount = 0;
        if (!BrakeMovement(r, releasedMove, releasedCount)) break;
        if (!GameHandlers::Instance().AcquireTarget(target, &selected, true) ||
            !GameHandlers::Instance().AimTarget(target, selected)) {
            RestoreMovement(r, releasedMove, releasedCount); releasedCount=0; continue;
        }
        // Let at least one game-frame consume the post-brake angle before the
        // trigger is sent.  This prevents a moving player's last WASD update
        // from winning over the freshly written pitch/yaw.
        if (!waitHeld(GameHandlers::Instance().AimSettleMs())) {
            cleanup(); break;
        }
        if (pauseAim_ || pauseAutoFire_) { Features().Set(Feature::AimAutoFire,false); pausedAim=true; }
        r.MouseButton(MOUSEEVENTF_LEFTDOWN,true,InputOwner::Sniper); leftDown=true;
        if (!waitHeld(delay(fireHoldMin_,fireHoldMax_))) { cleanup(); break; }
        r.MouseButton(MOUSEEVENTF_LEFTDOWN,false,InputOwner::Sniper); leftDown=false;
        if (!waitHeld(delay(unscopeDelayMin_,unscopeDelayMax_))) { cleanup(); break; }
        if (!waitHeld(delay(recoveryMin_,recoveryMax_))) { cleanup(); break; }
        // Legacy TCII firing chain: after the scoped shot, switch to weapon 3,
        // then back to weapon 1.  RIGHT remains held; no scope toggle is sent.
        if (!waitHeld(delay(switch3DelayMin_,switch3DelayMax_))) { cleanup(); break; }
        r.KeyScan(0x04, true, InputOwner::Sniper); // 3
        if (!waitHeld(delay(switch3HoldMin_,switch3HoldMax_))) { r.KeyScan(0x04,false,InputOwner::Sniper); cleanup(); break; }
        r.KeyScan(0x04, false, InputOwner::Sniper);
        if (!waitHeld(delay(switch1DelayMin_,switch1DelayMax_))) { cleanup(); break; }
        r.KeyScan(0x02, true, InputOwner::Sniper); // 1
        if (!waitHeld(delay(switch1HoldMin_,switch1HoldMax_))) { r.KeyScan(0x02,false,InputOwner::Sniper); cleanup(); break; }
        r.KeyScan(0x02, false, InputOwner::Sniper);
        RestoreMovement(r, releasedMove, releasedCount); releasedCount=0;
        cooldownEnd_=GetTickCount64()+postSwitchCooldownMs_;
        // Keep the scope session alive through the cooldown.  If RMB is still
        // held, the loop resumes target acquisition without another RIGHTDOWN.
    }
    cleanup();
    busy_=false;
}
}

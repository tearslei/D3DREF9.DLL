#include "input_router.h"
#include "macro_engine.h"
#include "instant_sniper.h"
#include "home_panel.h"
#include "aim_controller.h"
#include "feature_manager.h"
#include <cstdio>
#include <cstring>
namespace d3dref9 {
namespace {
void LogInputEvent(DWORD vk, bool down, bool injected, bool handled) {
    // Keep diagnostics ASCII so the path is stable under CP936/UTF-8 hosts.
    if (!(vk == 'O' || vk == VK_RETURN || vk == VK_HOME || vk == VK_F7 || vk == VK_F9 ||
          vk == VK_F10 || vk == 'Y' || vk == '3' || vk == '5' || vk == '6' ||
          vk == '8' || vk == '9' || vk == '0' || vk == 'Q' || vk == '1' ||
          vk == 'D' || vk == 'E' || vk == 'F' || vk == 'H' || vk == 'J' || vk == 'L' || vk == 'V' ||
          vk == 'X' || vk == 'W' || vk == 'S' || vk == 'Z' || vk == 'C' ||
          vk == VK_LCONTROL || vk == VK_LMENU || vk == VK_SPACE ||
          vk == VK_OEM_COMMA || vk == VK_OEM_PERIOD || vk == VK_OEM_102)) return;
    char path[MAX_PATH]{}; GetTempPathA(sizeof(path), path);
    std::strcat(path, "d3dref9_input.log");
    FILE* fp = nullptr; fopen_s(&fp, path, "a");
    if (!fp) return;
    std::fprintf(fp, "%llu vk=0x%02X down=%u injected=%u handled=%u f7=%u f9=%u f10=%u\n",
        static_cast<unsigned long long>(GetTickCount64()), static_cast<unsigned>(vk),
        down ? 1u : 0u, injected ? 1u : 0u, handled ? 1u : 0u,
        InputRouter::Instance().Physical(VK_F7) ? 1u : 0u,
        InputRouter::Instance().Physical(VK_F9) ? 1u : 0u,
        InputRouter::Instance().Physical(VK_F10) ? 1u : 0u);
    std::fclose(fp);
}
}
InputRouter& InputRouter::Instance(){static InputRouter r;return r;}
bool InputRouter::HandleFeatureHotkey(DWORD vk){
 const auto held=[this](DWORD key){return Physical(key)||(GetAsyncKeyState(static_cast<int>(key))&0x8000)!=0;};
 DWORD mod=held(VK_F7)?VK_F7:(held(VK_F9)?VK_F9:(held(VK_F10)?VK_F10:0));
 if(!mod) return false; Feature f=Feature::Count;
 if(mod==VK_F7){ if(vk=='Y')f=Feature::OptimizeProcess; else if(vk=='3')f=Feature::RoomStay; else if(vk=='6')f=Feature::PlayerEsp; else if(vk=='8')f=Feature::NoRecoil; else if(vk=='9')f=Feature::InstantReload; else if(vk=='0')f=Feature::BulletWall; }
 else if(mod==VK_F9){
     if(vk=='H'){ HomePanel::Instance().Toggle(); return true; }
     if(vk=='1')f=Feature::Headshot; else if(vk=='D')f=Feature::BunnyHop; else if(vk=='E')f=Feature::PlayerNoclip; else if(vk=='L')f=Feature::ThirdPerson; else if(vk=='V')f=Feature::FallNoDamage;
 }
 else if(mod==VK_F10){
     if(vk=='X')f=Feature::Radio;
     else if(vk=='W')f=Feature::TeleportGround;
     else if(vk=='6'){ AimController::Instance().CycleNormal(); return true; }
 }
 if(f==Feature::Count) return false; Features().Toggle(f); return true;
}

bool InputRouter::Install(){
    if(hook_&&mouseHook_)return true;
    HMODULE mod=nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,(LPCWSTR)(void*)&HookProc,&mod);
    hook_=SetWindowsHookExW(WH_KEYBOARD_LL,HookProc,mod,0);
    mouseHook_=SetWindowsHookExW(WH_MOUSE_LL,MouseHookProc,mod,0);
    char path[MAX_PATH]{}; GetTempPathA(sizeof(path),path); std::strcat(path,"d3dref9_input.log");
    FILE* fp=nullptr; fopen_s(&fp,path,"a");
    if(fp){std::fprintf(fp,"%llu install keyboard=%p mouse=%p ok=%u\n",
        static_cast<unsigned long long>(GetTickCount64()),static_cast<void*>(hook_),
        static_cast<void*>(mouseHook_),(hook_&&mouseHook_)?1u:0u);std::fclose(fp);}
    return hook_&&mouseHook_;
}
void InputRouter::Uninstall(){if(hook_){UnhookWindowsHookEx(hook_);hook_=nullptr;}if(mouseHook_){UnhookWindowsHookEx(mouseHook_);mouseHook_=nullptr;} ReleaseOwned(InputOwner::Macro);ReleaseOwned(InputOwner::Sniper);ReleaseOwned(InputOwner::Internal);}
void InputRouter::OnKey(DWORD vk,bool down,bool injected){
    bool changed=true;
    if(vk<physical_.size()&&!injected){std::lock_guard<std::mutex>l(mu_);changed=physical_[vk]!=down;physical_[vk]=down;}
    if(!injected){
        // Ignore typematic repeats and races between the LL hook and the
        // GetAsyncKeyState fallback.  Each physical edge must toggle once.
        if(!changed)return;
        bool handled=false;
        if(down)handled=HandleFeatureHotkey(vk);
        LogInputEvent(vk,down,injected,handled);
        MacroEngine::Instance().OnKey(vk,down);
        // L is the standalone normal/sniper mode selector.  F9+L is the
        // third-person feature hotkey and must not also toggle sniper mode;
        // similarly ignore a selector key while any feature modifier is held.
        const bool featureModifier = Physical(VK_F7) || Physical(VK_F9) || Physical(VK_F10);
        if (!(down && vk == 'L' && featureModifier))
            InstantSniper::Instance().OnKey(vk,down);
        AimController::Instance().OnKey(vk,down,InstantSniper::Instance().SniperMode());
    }
}
void InputRouter::PollFallback(){
    // DirectInput/full-screen paths can prevent selected keys (notably F9+H
    // and the F7/F9/F10 modifiers) from reaching WH_KEYBOARD_LL.  Poll only
    // keys owned by this DLL, and synthesize an internal edge only when the
    // LL hook did not already update the shared state.
    static constexpr DWORD keys[]={
        VK_RETURN,VK_F7,VK_F9,VK_F10,'Y','3','5','6','8','9','0','Q','1',
        'D','E','H','J','L','V','X','W','S','Z','C','F',VK_LCONTROL,VK_LMENU,
        VK_SPACE,VK_OEM_4,VK_OEM_MINUS
    };
    for(const auto vk:keys){
        const bool down=(GetAsyncKeyState(static_cast<int>(vk))&0x8000)!=0;
        if(down!=Physical(vk))OnKey(vk,down,false);
    }
}
bool InputRouter::Physical(DWORD vk)const{if(vk>=physical_.size())return false;std::lock_guard<std::mutex>l(mu_);return physical_[vk];}
void InputRouter::KeyScan(WORD scan,bool down,InputOwner owner){
    INPUT in{};in.type=INPUT_KEYBOARD;in.ki.wScan=scan;in.ki.dwFlags=KEYEVENTF_SCANCODE|(down?0:KEYEVENTF_KEYUP);SendInput(1,&in,sizeof(in));
    // B is scan code 0x30.  None of the migrated 11 macros should emit it;
    // record only this code to distinguish a DLL-generated event from an
    // external AHK/game input loop when diagnosing the backpack prompt.
    if(scan==0x30){char path[MAX_PATH]{};GetTempPathA(sizeof(path),path);std::strcat(path,"d3dref9_input.log");FILE*fp=nullptr;fopen_s(&fp,path,"a");if(fp){std::fprintf(fp,"%llu scan=0x%02X down=%u owner=%u\n",static_cast<unsigned long long>(GetTickCount64()),scan,down?1u:0u,static_cast<unsigned>(owner));std::fclose(fp);}}
    std::lock_guard<std::mutex>l(mu_);if(down)keys_[scan]=owner;else keys_.erase(scan);
}
void InputRouter::MouseMove(int dx,int dy){INPUT in{};in.type=INPUT_MOUSE;in.mi.dx=dx;in.mi.dy=dy;in.mi.dwFlags=MOUSEEVENTF_MOVE;SendInput(1,&in,sizeof(in));}
void InputRouter::MouseButton(DWORD flag,bool down,InputOwner){DWORD event=flag; if(!down){if(flag==MOUSEEVENTF_LEFTDOWN)event=MOUSEEVENTF_LEFTUP;else if(flag==MOUSEEVENTF_RIGHTDOWN)event=MOUSEEVENTF_RIGHTUP;} mouse_event(event,0,0,0,0);}
void InputRouter::ReleaseOwned(InputOwner owner){std::vector<WORD> rel;{std::lock_guard<std::mutex>l(mu_);for(auto&i:keys_)if(i.second==owner)rel.push_back(i.first);}for(auto s:rel)KeyScan(s,false,owner);}
LRESULT CALLBACK InputRouter::HookProc(int code,WPARAM wp,LPARAM lp){if(code==HC_ACTION){auto*k=(KBDLLHOOKSTRUCT*)lp;bool down=(wp==WM_KEYDOWN||wp==WM_SYSKEYDOWN);bool up=(wp==WM_KEYUP||wp==WM_SYSKEYUP);if(down||up)Instance().OnKey(k->vkCode,down,(k->flags&LLKHF_INJECTED)!=0);}return CallNextHookEx(Instance().hook_,code,wp,lp);}
LRESULT CALLBACK InputRouter::MouseHookProc(int code,WPARAM wp,LPARAM lp){if(code==HC_ACTION){auto*m=(MSLLHOOKSTRUCT*)lp;bool injected=(m->flags&LLMHF_INJECTED)!=0;DWORD vk=0;bool down=false;switch(wp){case WM_LBUTTONDOWN:vk=VK_LBUTTON;down=true;break;case WM_LBUTTONUP:vk=VK_LBUTTON;break;case WM_RBUTTONDOWN:vk=VK_RBUTTON;down=true;break;case WM_RBUTTONUP:vk=VK_RBUTTON;break;}if(vk&&!injected){std::lock_guard<std::mutex>l(Instance().mu_);Instance().physical_[vk]=down;}if(vk&&!injected)InstantSniper::Instance().OnMouseButton(vk,down);}return CallNextHookEx(Instance().mouseHook_,code,wp,lp);}
}

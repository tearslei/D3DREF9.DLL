#include "input_router.h"
#include "macro_engine.h"
#include "instant_sniper.h"
#include "home_panel.h"
#include "aim_controller.h"
namespace d3dref9 {
InputRouter& InputRouter::Instance(){static InputRouter r;return r;}
bool InputRouter::Install(){if(hook_&&mouseHook_)return true;HMODULE mod=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,(LPCWSTR)(void*)&HookProc,&mod);hook_=SetWindowsHookExW(WH_KEYBOARD_LL,HookProc,mod,0);mouseHook_=SetWindowsHookExW(WH_MOUSE_LL,MouseHookProc,mod,0);return hook_&&mouseHook_;}
void InputRouter::Uninstall(){if(hook_){UnhookWindowsHookEx(hook_);hook_=nullptr;}if(mouseHook_){UnhookWindowsHookEx(mouseHook_);mouseHook_=nullptr;} ReleaseOwned(InputOwner::Macro);ReleaseOwned(InputOwner::Sniper);ReleaseOwned(InputOwner::Internal);}
void InputRouter::OnKey(DWORD vk,bool down,bool injected){if(vk<physical_.size()&&!injected){std::lock_guard<std::mutex>l(mu_);physical_[vk]=down;} if(!injected){ if(down&&vk==VK_HOME)HomePanel::Instance().Toggle(); MacroEngine::Instance().OnKey(vk,down); InstantSniper::Instance().OnKey(vk,down); AimController::Instance().OnKey(vk,down,InstantSniper::Instance().SniperMode()); }}
bool InputRouter::Physical(DWORD vk)const{if(vk>=physical_.size())return false;std::lock_guard<std::mutex>l(mu_);return physical_[vk];}
void InputRouter::KeyScan(WORD scan,bool down,InputOwner owner){INPUT in{};in.type=INPUT_KEYBOARD;in.ki.wScan=scan;in.ki.dwFlags=KEYEVENTF_SCANCODE|(down?0:KEYEVENTF_KEYUP);SendInput(1,&in,sizeof(in));std::lock_guard<std::mutex>l(mu_);if(down)keys_[scan]=owner;else keys_.erase(scan);}
void InputRouter::MouseMove(int dx,int dy){INPUT in{};in.type=INPUT_MOUSE;in.mi.dx=dx;in.mi.dy=dy;in.mi.dwFlags=MOUSEEVENTF_MOVE;SendInput(1,&in,sizeof(in));}
void InputRouter::MouseButton(DWORD flag,bool down,InputOwner){INPUT in{};in.type=INPUT_MOUSE;DWORD event=flag; if(!down){if(flag==MOUSEEVENTF_LEFTDOWN)event=MOUSEEVENTF_LEFTUP;else if(flag==MOUSEEVENTF_RIGHTDOWN)event=MOUSEEVENTF_RIGHTUP;}in.mi.dwFlags=event;SendInput(1,&in,sizeof(in));}
void InputRouter::ReleaseOwned(InputOwner owner){std::vector<WORD> rel;{std::lock_guard<std::mutex>l(mu_);for(auto&i:keys_)if(i.second==owner)rel.push_back(i.first);}for(auto s:rel)KeyScan(s,false,owner);}
LRESULT CALLBACK InputRouter::HookProc(int code,WPARAM wp,LPARAM lp){if(code==HC_ACTION){auto*k=(KBDLLHOOKSTRUCT*)lp;bool down=(wp==WM_KEYDOWN||wp==WM_SYSKEYDOWN);bool up=(wp==WM_KEYUP||wp==WM_SYSKEYUP);if(down||up)Instance().OnKey(k->vkCode,down,(k->flags&LLKHF_INJECTED)!=0);}return CallNextHookEx(Instance().hook_,code,wp,lp);}
LRESULT CALLBACK InputRouter::MouseHookProc(int code,WPARAM wp,LPARAM lp){if(code==HC_ACTION){auto*m=(MSLLHOOKSTRUCT*)lp;bool injected=(m->flags&LLMHF_INJECTED)!=0;DWORD vk=0;bool down=false,up=false;switch(wp){case WM_LBUTTONDOWN:vk=VK_LBUTTON;down=true;break;case WM_LBUTTONUP:vk=VK_LBUTTON;up=true;break;case WM_RBUTTONDOWN:vk=VK_RBUTTON;down=true;break;case WM_RBUTTONUP:vk=VK_RBUTTON;up=true;break;}if(vk&&!injected){std::lock_guard<std::mutex>l(Instance().mu_);Instance().physical_[vk]=down;}}return CallNextHookEx(Instance().mouseHook_,code,wp,lp);}
}

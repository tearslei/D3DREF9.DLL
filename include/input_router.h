#pragma once
#include "common.h"
namespace d3dref9 {
enum class InputOwner:uint8_t{Macro,Sniper,Internal};
class InputRouter {
public:
 static InputRouter& Instance();
 bool Install(); void Uninstall();
 void OnKey(DWORD vk,bool down,bool injected);
 bool Physical(DWORD vk) const;
 void KeyScan(WORD scan,bool down,InputOwner owner);
 void MouseMove(int dx,int dy); void MouseButton(DWORD flag,bool down,InputOwner owner);
 void ReleaseOwned(InputOwner owner);
 static LRESULT CALLBACK HookProc(int code,WPARAM wp,LPARAM lp); static LRESULT CALLBACK MouseHookProc(int code,WPARAM wp,LPARAM lp);
private:
 bool HandleFeatureHotkey(DWORD vk);
 InputRouter()=default; HHOOK hook_{},mouseHook_{}; mutable std::mutex mu_; std::array<bool,256> physical_{}; std::unordered_map<WORD,InputOwner> keys_;
};
}

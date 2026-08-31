#include "api.h"
#include "input_router.h"
#include "home_panel.h"
#include "macro_engine.h"
#include "feature_manager.h"
namespace d3dref9 {
static std::atomic<bool> g_stop{false}; static HANDLE g_thread=nullptr; static HINSTANCE g_inst=nullptr;
static DWORD WINAPI Worker(LPVOID){InputRouter::Instance().Install();HomePanel::Instance().Create(g_inst);MacroEngine::Instance().Start();auto start=Clock::now();bool startup=false;MSG msg{};while(!g_stop.load()){while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT){g_stop=true;break;}TranslateMessage(&msg);DispatchMessageW(&msg);}if(!startup&&std::chrono::duration_cast<std::chrono::seconds>(Clock::now()-start).count()>=150){startup=true;}SleepMs(5);}HomePanel::Instance().Destroy();InputRouter::Instance().Uninstall();return 0;}
void Initialize(HINSTANCE h){if(g_thread)return;g_inst=h;g_stop=false;g_thread=CreateThread(nullptr,0,Worker,nullptr,0,nullptr);}
void Shutdown(){if(!g_thread)return;g_stop=true;PostThreadMessageW(GetThreadId(g_thread),WM_QUIT,0,0);WaitForSingleObject(g_thread,3000);CloseHandle(g_thread);g_thread=nullptr;}
}
extern "C" __declspec(dllexport) void d3dref9_initialize(){HMODULE h=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,(LPCWSTR)&d3dref9_initialize,&h);d3dref9::Initialize(h);}
extern "C" __declspec(dllexport) void d3dref9_shutdown(){d3dref9::Shutdown();}
BOOL APIENTRY DllMain(HMODULE h,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(h);d3dref9::Initialize(h);}else if(reason==DLL_PROCESS_DETACH){d3dref9::Shutdown();}return TRUE;}


#include "api.h"
#include "input_router.h"
#include "home_panel.h"
#include "macro_engine.h"
#include "feature_manager.h"
#include "game_handlers.h"
#include "render_adapter.h"
namespace d3dref9 {
static std::atomic<bool> g_stop{false}; static HANDLE g_thread=nullptr; static HINSTANCE g_inst=nullptr;
static DWORD WINAPI Worker(LPVOID){InputRouter::Instance().Install();HomePanel::Instance().Create(g_inst);MacroEngine::Instance().Start();GameHandlers::Instance().Start();Features().ApplyStartupHandlers();
// 启动阶段不写入游戏处理器。大厅、切图和登录阶段可能尚未建立
// 目标对象；保持处理器关闭，避免在无效上下文中触发旧入口。
// 房间保持和摔不掉血在 Worker 启动后自动开启；两键优化在 150 秒后开启。
auto start=Clock::now();bool startup=false;
MSG msg{};while(!g_stop.load()){while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT){g_stop=true;break;}TranslateMessage(&msg);DispatchMessageW(&msg);}InputRouter::Instance().PollFallback();if(!startup&&std::chrono::duration_cast<std::chrono::seconds>(Clock::now()-start).count()>=150){Features().ApplyDelayedStartup();startup=true;}SleepMs(5);}GameHandlers::Instance().Stop();HomePanel::Instance().Destroy();InputRouter::Instance().Uninstall();return 0;}
bool Initialize(HINSTANCE h){
    if(g_thread) return true;
    if(h) g_inst=h;
    g_stop=false;
    g_thread=CreateThread(nullptr,0,Worker,nullptr,0,nullptr);
    return g_thread!=nullptr;
}
void Shutdown(){if(!g_thread)return;g_stop=true;PostThreadMessageW(GetThreadId(g_thread),WM_QUIT,0,0);WaitForSingleObject(g_thread,3000);CloseHandle(g_thread);g_thread=nullptr;}
}
extern "C" D3DREF9_API void d3dref9_initialize(){HMODULE h=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,(LPCWSTR)&d3dref9_initialize,&h);(void)d3dref9::Initialize(h);}
extern "C" D3DREF9_API void d3dref9_shutdown(){d3dref9::Shutdown();}
// Initialize() only creates the worker and never waits for it. Windows starts
// the new thread after outstanding DLL initialization has completed, so the
// worker performs hooks/module lookups outside the loader lock. Some 10.4
// launch paths load ordinal 1 but never call it; PROCESS_ATTACH must therefore
// schedule the same idempotent worker as the proven stable no-damage build.
BOOL APIENTRY DllMain(HMODULE h,DWORD reason,LPVOID reserved){
    if(reason==DLL_PROCESS_ATTACH){
        DisableThreadLibraryCalls(h);
        (void)d3dref9::Initialize(h);
    } else if(reason==DLL_PROCESS_DETACH){
        // During process teardown all threads are terminated by the loader;
        // waiting here would re-enter loader-locked code.  Explicit unloads
        // should call d3dref9_shutdown() first, which performs the orderly
        // worker/UI teardown and restores hooks.
        (void)reserved;
    }
    return TRUE;
}

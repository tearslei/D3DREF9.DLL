#include <windows.h>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstring>

using Direct3DCreate9_t = void* (WINAPI*)(UINT);
using CreateDevice_t = HRESULT (STDMETHODCALLTYPE*)(void*, UINT, int, HWND, DWORD, void*, void**);
static Direct3DCreate9_t g_realCreate9=nullptr;
static CreateDevice_t g_realCreateDevice=nullptr;
static BYTE g_saved[5]{}; static BYTE* g_create9=nullptr; static BYTE* g_trampoline=nullptr;
static CRITICAL_SECTION g_lock; static HANDLE g_log=INVALID_HANDLE_VALUE;

static void Log(const char*fmt,...){EnterCriticalSection(&g_lock);if(g_log==INVALID_HANDLE_VALUE)g_log=CreateFileA("C:\\Windows\\Temp\\d3d9_early_trace.log",FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(g_log!=INVALID_HANDLE_VALUE){char b[512];va_list ap;va_start(ap,fmt);int n=_vsnprintf_s(b,sizeof(b),_TRUNCATE,fmt,ap);va_end(ap);if(n>0){DWORD w;WriteFile(g_log,b,(DWORD)n,&w,nullptr);}}LeaveCriticalSection(&g_lock);}
static void* Method(void*obj,size_t i){return obj?((void**)*(void***)obj)[i]:nullptr;}

static HRESULT STDMETHODCALLTYPE TraceCreateDevice(void*self,UINT adapter,int type,HWND wnd,DWORD flags,void*pp,void**out){
 HRESULT hr=g_realCreateDevice?g_realCreateDevice(self,adapter,type,wnd,flags,pp,out):E_FAIL;
 if(SUCCEEDED(hr)&&out&&*out){void*dev=*out;Log("CreateDevice self=%p device=%p vtbl=%p Present17=%p EndScene42=%p DIP82=%p\\n",self,dev,*(void**)dev,Method(dev,17),Method(dev,42),Method(dev,82));}
 else Log("CreateDevice hr=0x%08lx\\n",(unsigned long)hr);
 return hr;
}
static void HookCreateDevice(void*d3d){
 if(!d3d||g_realCreateDevice)return;auto old=*(void***)d3d;if(!old)return;
 void**copy=(void**)VirtualAlloc(nullptr,17*sizeof(void*),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);if(!copy){Log("CreateDevice vtable alloc failed\\n");return;}
 memcpy(copy,old,17*sizeof(void*));g_realCreateDevice=(CreateDevice_t)copy[16];copy[16]=(void*)&TraceCreateDevice;*(void***)d3d=copy;Log("IDirect3D9=%p oldVtable=%p CreateDevice16=%p\\n",d3d,old,g_realCreateDevice);
}
static void* WINAPI TraceDirect3DCreate9(UINT sdk){
 void*d3d=g_realCreate9?g_realCreate9(sdk):nullptr;Log("Direct3DCreate9 sdk=%u result=%p\\n",sdk,d3d);HookCreateDevice(d3d);return d3d;
}
static bool InstallCreate9Hook(){
 HMODULE m=GetModuleHandleA("d3d9.dll");if(!m)return false;auto target=(BYTE*)GetProcAddress(m,"Direct3DCreate9");if(!target)return false;
 if(g_realCreate9)return true;
 // Do not copy a relative branch into the trampoline; wait for a safe direct prologue.
 if(target[0]==0xE9||target[0]==0xE8){Log("Direct3DCreate9 unsupported branch prologue %02X\\n",target[0]);return true;}
 BYTE*tr=(BYTE*)VirtualAlloc(nullptr,16,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);if(!tr)return false;
 memcpy(g_saved,target,5);memcpy(tr,g_saved,5);tr[5]=0xE9;*(int32_t*)(tr+6)=(int32_t)((target+5)-(tr+10));
 DWORD old=0;if(!VirtualProtect(target,5,PAGE_EXECUTE_READWRITE,&old)){VirtualFree(tr,0,MEM_RELEASE);return false;}
 target[0]=0xE9;*(int32_t*)(target+1)=(int32_t)((BYTE*)&TraceDirect3DCreate9-(target+5));FlushInstructionCache(GetCurrentProcess(),target,5);DWORD tmp;VirtualProtect(target,5,old,&tmp);
 g_create9=target;g_trampoline=tr;g_realCreate9=(Direct3DCreate9_t)tr;Log("hooked Direct3DCreate9=%p trampoline=%p\\n",target,tr);return true;
}
static DWORD WINAPI Init(LPVOID){for(int i=0;i<150&&!g_realCreate9;i++){InstallCreate9Hook();Sleep(100);}Log("d3d9 early trace ready pid=%lu hooked=%d\\n",(unsigned long)GetCurrentProcessId(),g_realCreate9?1:0);return 0;}
BOOL APIENTRY DllMain(HMODULE h,DWORD reason,LPVOID){
 if(reason==DLL_PROCESS_ATTACH){
  DisableThreadLibraryCalls(h);
  // Install synchronously.  The tracer is loaded by the D3DREF9 proxy just
  // before ordinal 1 is called; a worker thread could otherwise lose this
  // race and miss the first Direct3DCreate9/CreateDevice call.
  InitializeCriticalSection(&g_lock);
  bool hooked=InstallCreate9Hook();
  Log("d3d9 early trace ready pid=%lu hooked=%d\\n",(unsigned long)GetCurrentProcessId(),hooked?1:0);
  if (!hooked) {
    HANDLE t=CreateThread(nullptr,0,Init,nullptr,0,nullptr);
    if(t) CloseHandle(t);
  }
 }
 return TRUE;
}

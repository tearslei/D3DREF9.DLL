#include <windows.h>
#include <winnt.h>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstring>

using GP = FARPROC (WINAPI*)(HMODULE,LPCSTR);
using VP = BOOL (WINAPI*)(LPVOID,SIZE_T,DWORD,PDWORD);
using WPM = BOOL (WINAPI*)(HANDLE,LPVOID,LPCVOID,SIZE_T,SIZE_T*);
using MEM = void* (__cdecl*)(void*,const void*,size_t);
static GP realGP=nullptr; static VP realVP=nullptr; static WPM realWPM=nullptr; static MEM realMemcpy=nullptr,realMemmove=nullptr;
static CRITICAL_SECTION cs; static HANDLE logh=INVALID_HANDLE_VALUE;
static void logline(const char* fmt,...){EnterCriticalSection(&cs);if(logh==INVALID_HANDLE_VALUE)logh=CreateFileA("C:\\Windows\\Temp\\d3dref9_write_trace.log",FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(logh!=INVALID_HANDLE_VALUE){char b[512];va_list ap;va_start(ap,fmt);int n=_vsnprintf_s(b,sizeof(b),_TRUNCATE,fmt,ap);va_end(ap);if(n>0){DWORD w;WriteFile(logh,b,(DWORD)n,&w,nullptr);}}LeaveCriticalSection(&cs);}
static void hex(const BYTE*p,size_t n,char*o,size_t c){size_t k=0;for(size_t i=0;i<n&&k+3<c;i++)k+=(size_t)snprintf(o+k,c-k,"%02X",p[i]);o[k]=0;}
static BOOL WINAPI traceVP(LPVOID a,SIZE_T n,DWORD np,PDWORD op){logline("VP addr=%p size=%lu new=0x%lx\n",a,(unsigned long)n,(unsigned long)np);return realVP?realVP(a,n,np,op):FALSE;}
static BOOL WINAPI traceWPM(HANDLE h,LPVOID a,LPCVOID s,SIZE_T n,SIZE_T*w){BYTE b[32]={},x[32]={};size_t m=n<32?n:32;if(h==GetCurrentProcess()&&a&&m)ReadProcessMemory(h,a,b,m,nullptr);BOOL ok=realWPM?realWPM(h,a,s,n,w):FALSE;if(h==GetCurrentProcess()&&a&&m)ReadProcessMemory(h,a,x,m,nullptr);char hb[80],hx[80];hex(b,m,hb,80);hex(x,m,hx,80);logline("WPM addr=%p size=%lu ok=%d before=%s after=%s\n",a,(unsigned long)n,ok?1:0,hb,hx);return ok;}
static void* __cdecl traceMemcpy(void*d,const void*s,size_t n){void*r=realMemcpy?realMemcpy(d,s,n):nullptr;logline("memcpy dst=%p src=%p size=%lu\n",d,s,(unsigned long)n);return r;}
static void* __cdecl traceMemmove(void*d,const void*s,size_t n){void*r=realMemmove?realMemmove(d,s,n):nullptr;logline("memmove dst=%p src=%p size=%lu\n",d,s,(unsigned long)n);return r;}
static FARPROC WINAPI hookGP(HMODULE h,LPCSTR name){FARPROC p=realGP?realGP(h,name):nullptr;if(!name)return p;logline("GP %s => %p\n",name,p);if(!_stricmp(name,"VirtualProtect")){realVP=(VP)p;return (FARPROC)traceVP;}if(!_stricmp(name,"WriteProcessMemory")){realWPM=(WPM)p;return (FARPROC)traceWPM;}if(!_stricmp(name,"memcpy")){realMemcpy=(MEM)p;return (FARPROC)traceMemcpy;}if(!_stricmp(name,"memmove")){realMemmove=(MEM)p;return (FARPROC)traceMemmove;}return p;}
static void scan_patch(){
 HMODULE m=GetModuleHandleA("D3DREF9.DLL"); if(!m)return; BYTE*base=(BYTE*)m; auto*dos=(IMAGE_DOS_HEADER*)base; auto*nt=(IMAGE_NT_HEADERS32*)(base+dos->e_lfanew); DWORD sz=nt->OptionalHeader.SizeOfImage;
 FARPROC vp=GetProcAddress(GetModuleHandleA("kernel32.dll"),"VirtualProtect"); FARPROC wpm=GetProcAddress(GetModuleHandleA("kernel32.dll"),"WriteProcessMemory");
 FARPROC mm=GetProcAddress(GetModuleHandleA("ntdll.dll"),"RtlMoveMemory"); DWORD vals[3]={(DWORD)(uintptr_t)vp,(DWORD)(uintptr_t)wpm,(DWORD)(uintptr_t)mm}; DWORD reps[3]={(DWORD)(uintptr_t)&traceVP,(DWORD)(uintptr_t)&traceWPM,(DWORD)(uintptr_t)&traceMemmove};
 DWORD count[3]={}; for(DWORD off=0;off+4<sz;off+=4){DWORD v=*(DWORD*)(base+off);for(int i=0;i<3;i++)if(v==vals[i]&&v){DWORD old; if(VirtualProtect(base+off,4,PAGE_READWRITE,&old)){*(DWORD*)(base+off)=reps[i];VirtualProtect(base+off,4,old,&old);count[i]++;}}}
 logline("scan patched VP=%lu WPM=%lu RtlMove=%lu\n",count[0],count[1],count[2]);
}static bool patch(){HMODULE m=GetModuleHandleA("D3DREF9.DLL");if(!m)return false;BYTE*base=(BYTE*)m;auto*d=(IMAGE_DOS_HEADER*)base;if(d->e_magic!=IMAGE_DOS_SIGNATURE)return false;auto*n=(IMAGE_NT_HEADERS32*)(base+d->e_lfanew);auto dir=n->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];auto*im=(IMAGE_IMPORT_DESCRIPTOR*)(base+dir.VirtualAddress);for(;im->Name;im++){const char*dll=(const char*)(base+im->Name);if(_stricmp(dll,"KERNEL32.dll"))continue;auto*o=(IMAGE_THUNK_DATA32*)(base+(im->OriginalFirstThunk?im->OriginalFirstThunk:im->FirstThunk));auto*f=(IMAGE_THUNK_DATA32*)(base+im->FirstThunk);for(;o->u1.AddressOfData;o++,f++){if(o->u1.Ordinal&IMAGE_ORDINAL_FLAG32)continue;auto*ib=(IMAGE_IMPORT_BY_NAME*)(base+o->u1.AddressOfData);if(!strcmp((char*)ib->Name,"GetProcAddress")){DWORD old;VirtualProtect(&f->u1.Function,4,PAGE_READWRITE,&old);realGP=(GP)(uintptr_t)f->u1.Function;f->u1.Function=(DWORD)(uintptr_t)&hookGP;VirtualProtect(&f->u1.Function,4,old,&old);logline("IAT patched realGP=%p\n",realGP);return true;}}}return false;}
static DWORD WINAPI init(LPVOID){InitializeCriticalSection(&cs);for(int i=0;i<50&&!patch();i++)Sleep(100);scan_patch();logline("trace started pid=%lu\n",(unsigned long)GetCurrentProcessId());return 0;}
BOOL APIENTRY DllMain(HMODULE,DWORD r,LPVOID){if(r==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(GetModuleHandle(nullptr));CreateThread(nullptr,0,init,nullptr,0,nullptr);}return TRUE;}

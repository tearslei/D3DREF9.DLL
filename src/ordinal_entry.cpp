#include "api.h"

// Compatibility entry used by the CF 1.1.85.7 loader.  The reference
// D3DREF9.DLL exposes a single x86, no-argument ordinal-1 entry and returns
// with a plain RET.  Keeping this wrapper __cdecl and void preserves the
// caller's stack; initialization itself is idempotent in dllmain.cpp.
namespace d3dref9 { bool Initialize(HINSTANCE); }

// BOOL is returned in EAX; the reference entry's caller does not clean any
// arguments (plain RET), so this remains a zero-argument __cdecl ABI.
extern "C" BOOL __cdecl d3dref9_ordinal1() {
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                       reinterpret_cast<LPCWSTR>(&d3dref9_ordinal1), &self);
    return d3dref9::Initialize(self) ? TRUE : FALSE;
}

#include <windows.h>

// Temporary bootstrap proxy used only for the D3D9 creation trace.  The
// original ordinal-1 implementation stays byte-for-byte in
// D3DREF9.original.dll and every caller is tail-jumped to it.
extern "C" void* g_original_ordinal = nullptr;

static void WriteLog(const wchar_t* text) {
  wchar_t path[MAX_PATH]{};
  GetTempPathW(MAX_PATH, path);
  lstrcatW(path, L"d3dref9_early_proxy.log");
  HANDLE h = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                         nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h != INVALID_HANDLE_VALUE) {
    char utf8[512]{};
    int n = WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8, sizeof(utf8) - 1, nullptr, nullptr);
    if (n > 1) { DWORD written{}; WriteFile(h, utf8, static_cast<DWORD>(n - 1), &written, nullptr); }
    CloseHandle(h);
  }
}

static bool DirectoryOfSelf(HMODULE self, wchar_t out[MAX_PATH]) {
  if (!GetModuleFileNameW(self, out, MAX_PATH)) return false;
  wchar_t* slash = wcsrchr(out, L'\\');
  if (!slash) return false;
  slash[1] = L'\0';
  return true;
}

static bool LoadEarlyTracer(HMODULE self) {
  wchar_t dir[MAX_PATH]{};
  if (!DirectoryOfSelf(self, dir)) { WriteLog(L"proxy: self path failed\r\n"); return false; }
  lstrcatW(dir, L"d3d9_early_trace.dll");
  HMODULE tracer = LoadLibraryW(dir);
  WriteLog(tracer ? L"proxy: d3d9 early tracer loaded\r\n" : L"proxy: d3d9 early tracer failed\r\n");
  return tracer != nullptr;
}

// Keep this as a tail jump: ordinal 1 has no public prototype and the caller's
// stack/register ABI must be preserved exactly.
extern "C" __attribute__((naked)) void OrdinalOne() {
  __asm__ __volatile__(
    "movl _g_original_ordinal, %eax\n\t"
    "testl %eax, %eax\n\t"
    "jz 1f\n\t"
    "jmp *%eax\n\t"
    "1: ret\n\t");
}

BOOL APIENTRY DllMain(HMODULE self, DWORD reason, LPVOID) {
  if (reason != DLL_PROCESS_ATTACH) return TRUE;
  DisableThreadLibraryCalls(self);
  // Load the recorder before the original ordinal is ever called.  Loading it
  // from a worker was too late: the original ordinal can create D3D9 during
  // the first call, before that worker gets scheduled.
  LoadEarlyTracer(self);
  wchar_t original[MAX_PATH]{};
  if (DirectoryOfSelf(self, original)) {
    lstrcatW(original, L"D3DREF9.original.dll");
    HMODULE real = LoadLibraryW(original);
    if (real) g_original_ordinal = reinterpret_cast<void*>(GetProcAddress(real, reinterpret_cast<LPCSTR>(1)));
  }
  if (!g_original_ordinal) WriteLog(L"proxy: original ordinal 1 unavailable\r\n");
  else WriteLog(L"proxy: original ordinal 1 ready\r\n");
  return TRUE;
}

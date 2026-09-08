#include <windows.h>
#include <cstdio>

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) return 2;
    HMODULE module = LoadLibraryW(argv[1]);
    if (!module) {
        std::printf("LoadLibrary failed: %lu\n", GetLastError());
        return 3;
    }
    wchar_t loadedPath[MAX_PATH]{};
    GetModuleFileNameW(module, loadedPath, MAX_PATH);
    ::wprintf(L"loaded=%p path=%ls\n", static_cast<void*>(module), loadedPath);
    using Entry = BOOL(__cdecl*)();
    auto entry = reinterpret_cast<Entry>(GetProcAddress(module, MAKEINTRESOURCEA(1)));
    if (argc < 3 || _wcsicmp(argv[2], L"nocall") != 0)
        std::printf("ordinal1=%p result=%d\n", static_cast<void*>(entry), entry ? entry() : -1);
    std::fflush(stdout);
    Sleep(3000);
    // The production DLL is process-lifetime.  The probe exits without an
    // explicit FreeLibrary so its worker cannot race an unload.
    return 0;
}

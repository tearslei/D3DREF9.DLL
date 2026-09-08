#include "render_adapter.h"

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include "feature_manager.h"

namespace d3dref9 {
namespace {
// The 1.1.85.7 client keeps a small wrapper at this absolute crossfire.exe
// address; its first dword is the live IDirect3DDevice9 pointer.  This chain
// was read-only verified in the current match (wrapper 0x15DCC018 -> device
// 0x147666A0) and avoids the login-time D3DXCreateFontA/VTable hooks that
// previously caused ERS/startup failures.
constexpr uintptr_t kDeviceWrapperAbs = 0x011BF438u;
using FontFn = HRESULT (WINAPI*)(void*, int, unsigned, unsigned, unsigned, BOOL,
                                 unsigned char, unsigned, unsigned, unsigned,
                                 const char*, void**);
using GetTransformFn = HRESULT (STDMETHODCALLTYPE*)(void*, DWORD, float*);
using GetViewportFn = HRESULT (STDMETHODCALLTYPE*)(void*, void*);
using GetStreamSourceFn = HRESULT (STDMETHODCALLTYPE*)(void*, unsigned, void**, unsigned*, unsigned*);
using GetRenderStateFn = HRESULT (STDMETHODCALLTYPE*)(void*, unsigned, unsigned*);
using SetRenderStateFn = HRESULT (STDMETHODCALLTYPE*)(void*, unsigned, unsigned);

void Log(const char* text) {
    char path[MAX_PATH]{}; GetTempPathA(sizeof(path), path);
    std::strcat(path, "d3dref9_render.log");
    FILE* fp = nullptr; fopen_s(&fp, path, "a");
    if (fp) { std::fprintf(fp, "%llu %s\n", static_cast<unsigned long long>(GetTickCount64()), text); std::fclose(fp); }
}

bool Patch5(void* address, const std::array<uint8_t, 5>& bytes) {
    if (!address) return false;
    DWORD old = 0; if (!VirtualProtect(address, 5, PAGE_EXECUTE_READWRITE, &old)) return false;
    std::memcpy(address, bytes.data(), 5);
    FlushInstructionCache(GetCurrentProcess(), address, 5);
    DWORD ignored = 0; VirtualProtect(address, 5, old, &ignored); return true;
}

void* VMethod(void* device, size_t index) {
    if (!device) return nullptr;
    __try {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(device, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT ||
            (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return nullptr;
        auto vtable = *static_cast<void***>(device);
        if (!vtable || !VirtualQuery(vtable + index, &mbi, sizeof(mbi)) ||
            mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return nullptr;
        return vtable[index];
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

bool ReadPtr32(uintptr_t address, uintptr_t& out) {
    out = 0;
    __try {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!address || !VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(mbi)) ||
            mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
            return false;
        out = static_cast<uintptr_t>(*reinterpret_cast<volatile uint32_t*>(address));
        return out != 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        out = 0;
        return false;
    }
}

bool Executable(void* address) {
    if (!address) return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(address, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT)
        return false;
    const auto p = mbi.Protect & 0xFFu;
    return p == PAGE_EXECUTE || p == PAGE_EXECUTE_READ ||
           p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY;
}

} // namespace

RenderAdapter& RenderAdapter::Instance() { static RenderAdapter r; return r; }

bool RenderAdapter::Start() {
    // Passive device capture.  Never patch executable code or the COM VTable
    // in the production path: the old D3DXCreateFontA and VTable hooks ran
    // during login and were the source of ERS/startup failures.
    auto cf = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"crossfire.exe"));
    if (!cf) return false;
    const auto slot = cf + (kDeviceWrapperAbs - 0x00400000u);
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(reinterpret_cast<void*>(slot), &mbi, sizeof(mbi)) ||
        mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
        return false;
    uintptr_t wrapper = 0, device = 0;
    if (!ReadPtr32(slot, wrapper) || !ReadPtr32(wrapper, device)) return false;
    void* object = reinterpret_cast<void*>(device);
    const auto getTransform = VMethod(object, 45);
    const auto getViewport = VMethod(object, 48);
    if (!Executable(getTransform) || !Executable(getViewport)) return false;

    auto previous = device_.exchange(object, std::memory_order_acq_rel);
    if (previous != object) {
        uintptr_t vtable = 0;
        ReadPtr32(device, vtable);
        char buf[192]{};
        std::snprintf(buf, sizeof(buf),
                      "event=device_passive wrapper=0x%08X device=0x%08X vtbl=0x%08X get_transform=0x%08X get_viewport=0x%08X",
                      static_cast<unsigned>(wrapper), static_cast<unsigned>(device), static_cast<unsigned>(vtable),
                      static_cast<unsigned>(reinterpret_cast<uintptr_t>(getTransform)),
                      static_cast<unsigned>(reinterpret_cast<uintptr_t>(getViewport)));
        Log(buf);
    }
    return true;
}

void RenderAdapter::Stop() {
    std::lock_guard<std::mutex> lock(mu_);
    RemoveFontHook();
    auto device = device_.load(std::memory_order_acquire);
    if (device && deviceVtableCopy_ && *static_cast<void***>(device) == deviceVtableCopy_)
        *static_cast<void***>(device) = deviceVtableOriginal_;
    if (deviceVtableCopy_) VirtualFree(deviceVtableCopy_, 0, MEM_RELEASE);
    deviceVtableOriginal_ = nullptr; deviceVtableCopy_ = nullptr; drawOriginal_ = nullptr;
    device_.store(nullptr, std::memory_order_release);
}

bool RenderAdapter::InstallFontHook() {
    auto module = GetModuleHandleW(L"d3dx9_29.dll");
    if (!module) { Log("event=font_hook_wait module_missing"); return false; }
    auto target = reinterpret_cast<uint8_t*>(GetProcAddress(module, "D3DXCreateFontA"));
    if (!target) { Log("event=font_hook_skip proc_missing"); return false; }
    if (target[0] == 0xE9 || target[0] == 0xE8 || target[0] == 0xC3) { Log("event=font_hook_skip unsupported_prologue"); return false; }
    static constexpr std::array<uint8_t, 5> kExpected{{0x8B, 0xFF, 0x55, 0x8B, 0xEC}};
    std::array<uint8_t, 5> original{}; std::memcpy(original.data(), target, original.size());
    if (original != kExpected) {
        Log("event=font_hook_skip signature_mismatch");
        return false;
    }
    auto* trampoline = static_cast<uint8_t*>(VirtualAlloc(nullptr, 16, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!trampoline) return false;
    std::memcpy(trampoline, original.data(), original.size());
    trampoline[5] = 0xE9;
    *reinterpret_cast<int32_t*>(trampoline + 6) = static_cast<int32_t>((target + 5) - (trampoline + 10));
    std::array<uint8_t, 5> patch{{0xE9, 0, 0, 0, 0}};
    *reinterpret_cast<int32_t*>(patch.data() + 1) = static_cast<int32_t>(reinterpret_cast<uint8_t*>(&FontHook) - (target + 5));
    if (!Patch5(target, patch)) { VirtualFree(trampoline, 0, MEM_RELEASE); return false; }
    fontTarget_ = target; fontTrampoline_ = trampoline; fontOriginal_ = original; hookInstalled_ = true;
    Log("event=font_hook_installed");
    return true;
}

void RenderAdapter::RemoveFontHook() {
    if (!hookInstalled_) return;
    Patch5(fontTarget_, fontOriginal_);
    if (fontTrampoline_) VirtualFree(fontTrampoline_, 0, MEM_RELEASE);
    fontTarget_ = nullptr; fontTrampoline_ = nullptr; fontOriginal_ = {}; hookInstalled_ = false;
    Log("event=font_hook_removed");
}

HRESULT RenderAdapter::CallOriginalFont(void* device, int height, unsigned width,
                                        unsigned weight, unsigned mipLevels, BOOL italic,
                                        unsigned charSet, unsigned outputPrecision,
                                        unsigned quality, unsigned pitchAndFamily,
                                        const char* faceName, void** font) {
    auto fn = reinterpret_cast<FontFn>(Instance().fontTrampoline_);
    return fn ? fn(device, height, width, weight, mipLevels, italic, charSet,
                   outputPrecision, quality, pitchAndFamily, faceName, font) : E_FAIL;
}

HRESULT WINAPI RenderAdapter::FontHook(void* device, int height, unsigned width,
                                       unsigned weight, unsigned mipLevels, BOOL italic,
                                       unsigned charSet, unsigned outputPrecision,
                                       unsigned quality, unsigned pitchAndFamily,
                                       const char* faceName, void** font) {
    Instance().device_.store(device, std::memory_order_release);
    auto& self = Instance();
    if (device && !self.deviceVtableCopy_) {
        auto old = *static_cast<void***>(device);
        if (old) {
            auto copy = static_cast<void**>(VirtualAlloc(nullptr, 119 * sizeof(void*), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
            if (copy) {
                std::memcpy(copy, old, 119 * sizeof(void*));
                self.deviceVtableOriginal_ = old;
                self.drawOriginal_ = copy[82];
                copy[82] = reinterpret_cast<void*>(&DrawHook);
                *static_cast<void***>(device) = copy;
                self.deviceVtableCopy_ = copy;
            }
        }
    }
    char buf[160]{};
    std::snprintf(buf, sizeof(buf), "event=device_captured device=0x%08Ix vtbl=0x%08Ix present=0x%08Ix endscene=0x%08Ix dip=0x%08Ix",
                  reinterpret_cast<size_t>(device), reinterpret_cast<size_t>(device ? *static_cast<void***>(device) : nullptr),
                  reinterpret_cast<size_t>(VMethod(device, 17)), reinterpret_cast<size_t>(VMethod(device, 42)),
                  reinterpret_cast<size_t>(VMethod(device, 82)));
    Log(buf);
    return CallOriginalFont(device, height, width, weight, mipLevels, italic, charSet,
                            outputPrecision, quality, pitchAndFamily, faceName, font);
}

HRESULT STDMETHODCALLTYPE RenderAdapter::DrawHook(void* device, int type, int baseVertex,
                                                   unsigned minVertex, unsigned numVertex,
                                                   unsigned startIndex, unsigned primCount) {
    auto& self = Instance();
    const auto n = self.drawCount_.fetch_add(1, std::memory_order_relaxed) + 1;
    if (n == 1 || (n % 1000) == 0) {
        char buf[128]{};
        std::snprintf(buf, sizeof(buf), "event=draw_indexed_primitive count=%llu prim=%u vertices=%u",
                      static_cast<unsigned long long>(n), primCount, numVertex);
        Log(buf);
    }
    auto fn = reinterpret_cast<HRESULT (STDMETHODCALLTYPE*)(void*, int, int, unsigned, unsigned, unsigned, unsigned)>(self.drawOriginal_);
    if (!fn) return E_FAIL;

    // TCII 的人物模型 stride 为 36/40/44/70。开启人物透视时只修改本次
    // draw 的渲染状态，调用结束后完整恢复，避免污染游戏其它批次。
    unsigned stride = 0; void* vb = nullptr; unsigned offset = 0;
    auto getStream = reinterpret_cast<GetStreamSourceFn>(VMethod(device, 101));
    if (getStream) getStream(device, 0, &vb, &offset, &stride);
    const bool model = stride == 36 || stride == 40 || stride == 44 || stride == 70;
    const bool esp = Features().IsOn(Feature::PlayerEsp);
    const bool wall = Features().IsOn(Feature::BulletWall);
    auto getState = reinterpret_cast<GetRenderStateFn>(VMethod(device, 58));
    auto setState = reinterpret_cast<SetRenderStateFn>(VMethod(device, 57));
    unsigned oldZ = 1, oldLight = 1, oldBlend = 1, oldFill = 3, oldZFunc = 4, oldFog = 0;
    bool saved = false;
    if (getState && setState && (wall || (esp && model))) {
        getState(device, 7, &oldZ); getState(device, 137, &oldLight);
        getState(device, 27, &oldBlend); getState(device, 8, &oldFill);
        getState(device, 23, &oldZFunc); getState(device, 28, &oldFog);
        saved = true;
    }
    HRESULT hr = E_FAIL;
    // Source myDrawIndexedPrimitive_x first draws non-model batches with
    // depth disabled for wall-through, then restores the complete state.
    // Stride 32 is the map geometry path and is intentionally excluded.
    if (saved && wall && !model) {
        setState(device, 7, 0); setState(device, 27, 0);
        hr = fn(device, type, baseVertex, minVertex, numVertex, startIndex, primCount);
        setState(device, 7, oldZ); setState(device, 27, oldBlend);
    } else if (saved && esp && model) {
        // Chams/highlight pass (fog on, lighting/alpha off) followed by an
        // x-ray pass.  Both passes are reverted before returning to the game.
        setState(device, 28, 1); setState(device, 137, 0); setState(device, 27, 0);
        setState(device, 7, 0); setState(device, 23, 1);
        hr = fn(device, type, baseVertex, minVertex, numVertex, startIndex, primCount);
        setState(device, 7, oldZ); setState(device, 23, 4); setState(device, 8, oldFill);
        // A second depth-tested draw preserves the source's outline/chams
        // behaviour while leaving the host's render state untouched.
        const auto hr2 = fn(device, type, baseVertex, minVertex, numVertex, startIndex, primCount);
        if (FAILED(hr)) hr = hr2;
        setState(device, 137, oldLight); setState(device, 27, oldBlend);
        setState(device, 28, oldFog); setState(device, 23, oldZFunc);
    } else {
        hr = fn(device, type, baseVertex, minVertex, numVertex, startIndex, primCount);
    }
    if (vb) {
        auto release = reinterpret_cast<ULONG (STDMETHODCALLTYPE*)(void*)>(VMethod(device, 2));
        if (release) release(vb);
    }
    return hr;
}

bool RenderAdapter::ReadFrame(Matrix4& view, Matrix4& projection, Viewport& viewport) const {
    auto device = device_.load(std::memory_order_acquire);
    if (!device) return false;
    auto getTransform = reinterpret_cast<GetTransformFn>(VMethod(device, 45));
    auto getViewport = reinterpret_cast<GetViewportFn>(VMethod(device, 48));
    if (!getTransform || !getViewport) return false;
    struct RawViewport { DWORD x, y, width, height; float minZ, maxZ; } raw{};
    __try {
        if (FAILED(getTransform(device, 2, view.m))) return false;       // D3DTS_VIEW
        if (FAILED(getTransform(device, 3, projection.m))) return false; // D3DTS_PROJECTION
        if (FAILED(getViewport(device, &raw))) return false;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // A device reset can invalidate the old COM object between Start()
        // and this call.  Clear it so the next tick reacquires the wrapper.
        device_.store(nullptr, std::memory_order_release);
        return false;
    }
    for (const auto value : view.m) if (!std::isfinite(value)) return false;
    for (const auto value : projection.m) if (!std::isfinite(value)) return false;
    viewport.x = static_cast<float>(raw.x); viewport.y = static_cast<float>(raw.y);
    viewport.width = static_cast<float>(raw.width); viewport.height = static_cast<float>(raw.height);
    viewport.minZ = raw.minZ; viewport.maxZ = raw.maxZ;
    return viewport.width > 0.0f && viewport.height > 0.0f;
}

} // namespace d3dref9


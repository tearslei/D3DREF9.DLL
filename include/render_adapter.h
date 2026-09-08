#pragma once

#include "entity_adapter.h"

namespace d3dref9 {

// Read-only D3D9 bridge.  It captures the device handed to D3DXCreateFontA
// and reads the standard view/projection/viewport state through the device
// vtable.  No draw state is changed by this adapter.
class RenderAdapter {
public:
    static RenderAdapter& Instance();
    bool Start();
    void Stop();
    bool ReadFrame(Matrix4& view, Matrix4& projection, Viewport& viewport) const;
    void* Device() const { return device_.load(std::memory_order_acquire); }

private:
    RenderAdapter() = default;
    bool InstallFontHook();
    void RemoveFontHook();
    static HRESULT WINAPI FontHook(void* device, int height, unsigned width,
                                   unsigned weight, unsigned mipLevels, BOOL italic,
                                   unsigned charSet, unsigned outputPrecision,
                                   unsigned quality, unsigned pitchAndFamily,
                                   const char* faceName, void** font);
    static HRESULT STDMETHODCALLTYPE DrawHook(void* device, int type, int baseVertex,
                                              unsigned minVertex, unsigned numVertex,
                                              unsigned startIndex, unsigned primCount);
    static HRESULT CallOriginalFont(void* device, int height, unsigned width,
                                    unsigned weight, unsigned mipLevels, BOOL italic,
                                    unsigned charSet, unsigned outputPrecision,
                                    unsigned quality, unsigned pitchAndFamily,
                                    const char* faceName, void** font);

    mutable std::atomic<void*> device_{nullptr};
    void* fontTarget_{};
    void* fontTrampoline_{};
    std::array<uint8_t, 5> fontOriginal_{};
    bool hookInstalled_{false};
    uint64_t lastAttempt_{0};
    void** deviceVtableOriginal_{};
    void** deviceVtableCopy_{};
    void* drawOriginal_{};
    std::atomic<uint64_t> drawCount_{0};
    mutable std::mutex mu_;
};

} // namespace d3dref9

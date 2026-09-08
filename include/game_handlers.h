#pragma once
#include "common.h"
#include "feature_manager.h"
#include "entity_adapter.h"
#include "render_adapter.h"
#include "aim_controller.h"

namespace d3dref9 {

// Source-derived runtime adapter for the 1.1.85.7 client.
// All writes are guarded by module/range checks and are reversible.
class GameHandlers {
public:
    static GameHandlers& Instance();
    void Start();
    void Stop();
    bool Ready() const;
    // 只有实体/阵营/骨骼适配器确认有效目标后才允许瞄准或自动开枪。
    // 当前版本默认为 false，避免在无目标时发送 3/1 瞬狙序列。
    bool TargetingReady() const { return targetReady_.load(); }
    void SetTargetingReady(bool ready) { targetReady_ = ready; }
    EntityAdapter& Entities() { return entities_; }
    void Apply(Feature f, bool on);
    // 返回当前帧距离准心最近的有效敌方；可选返回所选骨骼部位。
    bool AcquireTarget(EntitySnapshot& out, AimBone* bone = nullptr,
                       bool instantSniper = false);
    bool AimTarget(const EntitySnapshot& target, AimBone bone);
    // Source-compatible ray visibility test. Returns true when the client
    // reports no blocking hit. Missing/invalid engine state returns false so
    // the obstacle filter is fail-closed.
    bool IsVisible(const Vec3& me, const Vec3& target) const;
    uintptr_t CurrentPlayer() const;

private:
    struct AimConfig {
        // TCII's original "微型" selector uses 1 + 16 as the screen
        // divisor.  A positive pixel radius overrides that source-compatible
        // rectangle when explicitly configured.
        uint32_t microRangeDivisor{16};
        float microRadiusPx{0.0f};
        float bodyPaddingPx{4.0f};
        uint32_t lockHoldMs{80};
        float switchMarginPx{3.0f};
        uint32_t autoFireIntervalMs{130};
        bool visibilityRequired{true};
        bool visibilityFailClosed{true};
        uint32_t instantRangeDivisor{8};
    };

    GameHandlers() = default;
    GameHandlers(const GameHandlers&) = delete;
    GameHandlers& operator=(const GameHandlers&) = delete;
    static DWORD WINAPI ThreadProc(LPVOID);
    void Run();
    void Tick();
    void Log(const char* event, Feature f, bool on, uintptr_t addr = 0, uint32_t value = 0);
    void SetStatic(Feature f, bool on);
    void TickDynamic(Feature f, bool on);
    bool WriteFeature32(Feature f, uintptr_t address, uint32_t value);
    void RestoreFeature(Feature f);
    void TickAimAndFire();
    void LogAimGate(uint32_t code);
    void LogEntitySnapshot();
    void LoadAimConfig();

    std::atomic<bool> stop_{false};
    std::atomic<bool> started_{false};
    HANDLE thread_{nullptr};
    std::array<bool, static_cast<size_t>(Feature::Count)> applied_{};
    std::atomic<bool> targetReady_{false};
    uint64_t lastRoomPulse_{0};
    uint64_t lastEntityLog_{0};
    uint64_t lastAutoFire_{0};
    uint64_t lastAimGateLog_{0};
    uint32_t lastAimGateCode_{0};
    uint64_t lastAimTargetLog_{0};
    uint32_t lastAimTargetSlot_{0};
    uint64_t lastAutoFireLog_{0};
    uint64_t lastRadioPulse_{0};
    uint64_t lastStaticRetry_{0};
    bool teleportDone_{false};
    AimConfig aimConfig_{};
    uint32_t lockedSlot_{0};
    uint64_t lockedSince_{0};
    float lockedScore_{0.0f};
    EntityAdapter entities_{};
    mutable std::mutex mu_;
    std::unordered_map<uintptr_t, uint32_t> originalValues_;
    std::array<std::vector<uintptr_t>, static_cast<size_t>(Feature::Count)> featureWrites_{};
};

} // namespace d3dref9

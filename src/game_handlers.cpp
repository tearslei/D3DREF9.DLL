#include "game_handlers.h"
#include "instant_sniper.h"
#include "input_router.h"
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cwchar>
#include <tlhelp32.h>

namespace d3dref9 {
namespace {
constexpr uintptr_t kImageBase = 0x00400000u;
// Offsets recovered from TCII2.0 source (TCII变态版本(加追敌和跳舞).e).
constexpr uintptr_t kPlayerPtr = 0x16B3FD0u;
constexpr uintptr_t kWeaponPtr = 0x1E70F30u;
constexpr uintptr_t kLobby = 0x171BC00u;
constexpr uintptr_t kCrossfireEspAbs = 0x011D536Cu;
constexpr uintptr_t kEscMenuAbs = 0x011BE744u;
constexpr uintptr_t kRadioStateRva = 0x1E75A6Cu;
constexpr uintptr_t kRadioBlockRva = 0x1718840u;
constexpr uintptr_t kViewRootRva = 0x16B3FD0u - 72u;
// TCII source: IsVisible_ecx = 0x11AEF38, IsVisible_call = 0x5EE560.
constexpr uintptr_t kIsVisibleEcxAbs = 0x011AEF38u;
constexpr uintptr_t kIsVisibleCallAbs = 0x005EE560u;

HMODULE Shell() { return GetModuleHandleW(L"cshell.dll"); }
HMODULE Cf() { return GetModuleHandleW(L"crossfire.exe"); }
uintptr_t S(uintptr_t off) { auto b = reinterpret_cast<uintptr_t>(Shell()); return b ? b + off : 0; }
uintptr_t CAbs(uintptr_t absolute) {
    auto b = reinterpret_cast<uintptr_t>(Cf());
    return b ? b + (absolute - kImageBase) : 0;
}
bool Committed(uintptr_t p, size_t n = sizeof(uint32_t)) {
    if (!p) return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(reinterpret_cast<void*>(p), &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT || (mbi.Protect & PAGE_NOACCESS)) return false;
    auto begin = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
    return p >= begin && p + n <= begin + mbi.RegionSize;
}
uint32_t Read32(uintptr_t p) { return Committed(p) ? *reinterpret_cast<volatile uint32_t*>(p) : 0; }
bool Write32(uintptr_t p, uint32_t v) {
    if (!Committed(p)) return false;
    DWORD old = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(p), sizeof(v), PAGE_READWRITE, &old)) return false;
    *reinterpret_cast<volatile uint32_t*>(p) = v;
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(p), sizeof(v));
    DWORD ignored = 0; VirtualProtect(reinterpret_cast<void*>(p), sizeof(v), old, &ignored);
    return true;
}
bool WriteFloat(uintptr_t p, float v) {
    uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(v), "float size");
    std::memcpy(&bits, &v, sizeof(bits));
    return Write32(p, bits);
}
void RotateDiagnostic(const char* name) {
    char path[MAX_PATH]{}; GetTempPathA(sizeof(path), path); std::strcat(path, name);
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) return;
    char bak[MAX_PATH]{}; std::snprintf(bak, sizeof(bak), "%s.prev", path);
    MoveFileExA(path, bak, MOVEFILE_REPLACE_EXISTING);
}
bool WriteZeros(uintptr_t p, size_t n) {
    if (!Committed(p, n)) return false;
    DWORD old = 0; if (!VirtualProtect(reinterpret_cast<void*>(p), n, PAGE_READWRITE, &old)) return false;
    SecureZeroMemory(reinterpret_cast<void*>(p), n);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(p), n);
    DWORD ignored = 0; VirtualProtect(reinterpret_cast<void*>(p), n, old, &ignored); return true;
}
void OptimizeHelperProcesses(bool enable) {
    // TCII 的“优化游戏进程”并不是对 DLL 自身打补丁，而是遍历其
    // 相关启动器/聊天/代理进程并调整调度优先级。保持同一进程列表，
    // 但不依赖外部命令或注册表。
    static const wchar_t* kNames[] = {
        L"QTalk.exe", L"QTWeb.exe", L"TASLogin.exe", L"CrossProxy.exe",
        L"TPHelper.exe", L"TclsQmFix.exe", L"TenioDL.exe", L"TQMCenter.exe",
        L"BackgroundDownloader.exe", L"TCLSHost.exe", L"CrossSSOHolder.exe"
    };
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32W pe{}; pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            bool match = false;
            for (const auto* n : kNames)
                if (_wcsicmp(pe.szExeFile, n) == 0) { match = true; break; }
            if (match) {
                HANDLE hp = OpenProcess(PROCESS_SET_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION,
                                        FALSE, pe.th32ProcessID);
                if (hp) {
                    SetPriorityClass(hp, enable ? HIGH_PRIORITY_CLASS : NORMAL_PRIORITY_CLASS);
                    SetProcessPriorityBoost(hp, enable ? FALSE : TRUE);
                    CloseHandle(hp);
                }
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
}
uintptr_t Player() { return Read32(S(kPlayerPtr)); }
bool Alive(uintptr_t p) { return p && Read32(p + 1320u) == 1u; }

float IniFloat(const std::wstring& path, const wchar_t* key, float fallback) {
    wchar_t value[64]{};
    const auto def = std::to_wstring(fallback);
    GetPrivateProfileStringW(L"aim", key, def.c_str(), value,
                             static_cast<DWORD>(std::size(value)), path.c_str());
    wchar_t* end = nullptr;
    const float parsed = static_cast<float>(std::wcstof(value, &end));
    return end != value && std::isfinite(parsed) ? parsed : fallback;
}

uint32_t IniUint(const std::wstring& path, const wchar_t* key, uint32_t fallback) {
    const auto value = GetPrivateProfileIntW(L"aim", key, static_cast<INT>(fallback), path.c_str());
    return value < 0 ? fallback : static_cast<uint32_t>(value);
}

bool IniBool(const std::wstring& path, const wchar_t* key, bool fallback) {
    wchar_t value[16]{};
    GetPrivateProfileStringW(L"aim", key, fallback ? L"true" : L"false", value,
                             static_cast<DWORD>(std::size(value)), path.c_str());
    return _wcsicmp(value, L"1") == 0 || _wcsicmp(value, L"true") == 0 ||
           _wcsicmp(value, L"yes") == 0 || _wcsicmp(value, L"on") == 0;
}

std::wstring ConfigPath() {
    HMODULE hm = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                       reinterpret_cast<LPCWSTR>(&GameHandlers::Instance), &hm);
    wchar_t module[MAX_PATH]{};
    if (!hm || !GetModuleFileNameW(hm, module, MAX_PATH)) return {};
    std::wstring path(module);
    const auto slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return {};
    path.resize(slash);
    path += L"\\config\\d3dref9自治.ini";
    return path;
}

struct AimWindow {
    float cx{};
    float cy{};
    float rx{};
    float ry{};
};

float DistanceToBody(const EntitySnapshot& e, const AimWindow& w, float padding) {
    if (!e.bodyBounds.valid) return 1.0e30f;
    const float left = e.bodyBounds.left - padding;
    const float top = e.bodyBounds.top - padding;
    const float right = e.bodyBounds.right + padding;
    const float bottom = e.bodyBounds.bottom + padding;
    const float dx = (w.cx < left) ? left - w.cx : (w.cx > right ? w.cx - right : 0.0f);
    const float dy = (w.cy < top) ? top - w.cy : (w.cy > bottom ? w.cy - bottom : 0.0f);
    return std::sqrt(dx * dx + dy * dy);
}

bool BodyIntersectsAimWindow(const EntitySnapshot& e, const AimWindow& w, float padding,
                             bool pixelCircle) {
    if (!e.bodyBounds.valid) return false;
    const float left = e.bodyBounds.left - padding;
    const float top = e.bodyBounds.top - padding;
    const float right = e.bodyBounds.right + padding;
    const float bottom = e.bodyBounds.bottom + padding;
    const float winLeft = w.cx - w.rx;
    const float winTop = w.cy - w.ry;
    const float winRight = w.cx + w.rx;
    const float winBottom = w.cy + w.ry;
    if (!pixelCircle)
        return right >= winLeft && left <= winRight &&
               bottom >= winTop && top <= winBottom;
    const float nearestX = (w.cx < left) ? left : (w.cx > right ? right : w.cx);
    const float nearestY = (w.cy < top) ? top : (w.cy > bottom ? bottom : w.cy);
    const float dx = nearestX - w.cx;
    const float dy = nearestY - w.cy;
    const float radius = (std::max)(w.rx, w.ry);
    return dx * dx + dy * dy <= radius * radius;
}

float WorldDistanceSq(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}
}

GameHandlers& GameHandlers::Instance() { static GameHandlers h; return h; }
bool GameHandlers::Ready() const { return Shell() != nullptr && Cf() != nullptr; }
uintptr_t GameHandlers::CurrentPlayer() const {
    // TCII's player-object slot is populated only after an actual match has
    // created the pawn.  players_root+0x30 is a transform object, not a pawn;
    // using it as a fallback corrupts unrelated transform fields.
    return Player();
}

void GameHandlers::Start() {
    if (started_.exchange(true)) return;
    // 每次进程建立独立诊断文件，避免旧版 CSV 表头与新字段混排。
    RotateDiagnostic("d3dref9_entities.csv");
    RotateDiagnostic("d3dref9_entity_hook.log");
    RotateDiagnostic("d3dref9_render.log");
    RotateDiagnostic("d3dref9_handlers.log");
    LoadAimConfig();
    lockedSlot_ = 0;
    lockedSince_ = 0;
    lockedScore_ = 0.0f;
    lastAimGateLog_ = 0;
    lastAimGateCode_ = 0;
    lastAimTargetLog_ = 0;
    lastAimTargetSlot_ = 0;
    lastAutoFireLog_ = 0;
    stop_ = false;
    thread_ = CreateThread(nullptr, 0, ThreadProc, this, 0, nullptr);
}
void GameHandlers::Stop() {
    if (!started_.exchange(false)) return;
    stop_ = true;
    if (thread_) { WaitForSingleObject(thread_, 2000); CloseHandle(thread_); thread_ = nullptr; }
    entities_.Shutdown();
    RenderAdapter::Instance().Stop();
    for (size_t i = 0; i < applied_.size(); ++i) if (applied_[i]) SetStatic(static_cast<Feature>(i), false);
}
DWORD WINAPI GameHandlers::ThreadProc(LPVOID p) { static_cast<GameHandlers*>(p)->Run(); return 0; }
void GameHandlers::Run() {
    // The adapter is intentionally initialized in the worker, after the host
    // has finished mapping cshell.dll.  This is read-only and safe in lobby or
    // in a match; missing pointers simply produce an empty snapshot.
    entities_.Initialize();
    uint64_t heartbeat = 0;
    while (!stop_) {
        RenderAdapter::Instance().Start();
        Tick();
        const auto now = GetTickCount64();
        if (now - heartbeat >= 1000) { heartbeat = now; Log("heartbeat", Feature::Count, true); }
        SleepMs(10);
    }
}
void GameHandlers::Log(const char* event, Feature f, bool on, uintptr_t addr, uint32_t value) {
    char path[MAX_PATH]{}; GetTempPathA(sizeof(path), path); std::strcat(path, "d3dref9_handlers.log");
    FILE* fp = nullptr; fopen_s(&fp, path, "a"); if (!fp) return;
    std::fprintf(fp, "%llu event=%s feature=%u on=%u addr=0x%08Ix value=0x%08X ready=%u\n",
        static_cast<unsigned long long>(GetTickCount64()), event, static_cast<unsigned>(f), on ? 1u : 0u,
        static_cast<size_t>(addr), value, Ready() ? 1u : 0u); std::fclose(fp);
}
void GameHandlers::Apply(Feature f, bool on) {
    std::lock_guard<std::mutex> l(mu_);
    const auto i = static_cast<size_t>(f); if (i >= applied_.size()) return;
    if (applied_[i] == on) return;
    // 处理器可能在 cshell/crossfire 尚未完成加载时收到启动或热键事件；
    // 不要提前标记为已应用，让 Tick() 在模块就绪后重试。
    if (!Ready()) { applied_[i] = !on; Log("deferred", f, on); return; }
    applied_[i] = on;
    SetStatic(f, on); Log("transition", f, on);
}
bool GameHandlers::WriteFeature32(Feature f, uintptr_t address, uint32_t value) {
    if (!address || !Committed(address)) return false;
    const auto key = static_cast<size_t>(f);
    if (key >= featureWrites_.size()) return false;
    if (!originalValues_.count(address)) {
        originalValues_.emplace(address, Read32(address));
        featureWrites_[key].push_back(address);
    }
    return Write32(address, value);
}
void GameHandlers::RestoreFeature(Feature f) {
    const auto key = static_cast<size_t>(f);
    if (key >= featureWrites_.size()) return;
    for (const auto address : featureWrites_[key]) {
        const auto it = originalValues_.find(address);
        if (it != originalValues_.end()) { Write32(address, it->second); originalValues_.erase(it); }
    }
    featureWrites_[key].clear();
}
void GameHandlers::Tick() {
    if (!Ready()) return;
    Matrix4 view{}, projection{}; Viewport viewport{};
    if (RenderAdapter::Instance().ReadFrame(view, projection, viewport))
        entities_.SetRenderFrame(view, projection, viewport);
    // Coordinate hook disabled until the live call contract is validated;
    // polling remains read-only and avoids patching executable game code.
    LogEntitySnapshot();
    // Static handlers can be toggled in the lobby before the player/weapon
    // objects exist.  Retry their guarded writes periodically so entering a
    // match activates the selected feature without requiring a second hotkey.
    const auto retryNow = GetTickCount64();
    if (retryNow - lastStaticRetry_ >= 250u) {
        lastStaticRetry_ = retryNow;
        constexpr Feature kRetry[] = {Feature::PlayerEsp, Feature::BulletWall,
            Feature::Headshot, Feature::ThirdPerson, Feature::Radio};
        for (const auto f : kRetry)
            if (Features().IsOn(f)) SetStatic(f, true);
    }
    for (size_t i = 0; i < applied_.size(); ++i) {
        Feature f = static_cast<Feature>(i); bool on = Features().IsOn(f);
        if (applied_[i] != on) { applied_[i] = on; SetStatic(f, on); Log("state", f, on); }
        if (on) TickDynamic(f, on);
    }
    TickAimAndFire();
}
void GameHandlers::LogAimGate(uint32_t code) {
    const auto now = GetTickCount64();
    if (code == lastAimGateCode_ && now - lastAimGateLog_ < 500u) return;
    lastAimGateCode_ = code;
    lastAimGateLog_ = now;
    // 1=no_entities, 2=no_frame, 3=local_unready,
    // 4=no_body_candidate, 5=visibility_blocked, 6=no_visible_point,
    // 7=ordinary gate closed (physical LMB is up).
    Log("aim_gate", Feature::AimAutoFire, false, 0, code);
}

void GameHandlers::LoadAimConfig() {
    const auto path = ConfigPath();
    if (path.empty()) return;
    aimConfig_.microRangeDivisor = (std::max<uint32_t>)(1u, IniUint(path, L"micro_range_divisor", 16u));
    aimConfig_.microRadiusPx = (std::max)(0.0f, IniFloat(path, L"micro_radius_px", 0.0f));
    aimConfig_.bodyPaddingPx = (std::max)(0.0f, IniFloat(path, L"body_padding_px", 4.0f));
    aimConfig_.lockHoldMs = IniUint(path, L"lock_hold_ms", 80u);
    aimConfig_.switchMarginPx = (std::max)(0.0f, IniFloat(path, L"switch_margin_px", 3.0f));
    aimConfig_.autoFireIntervalMs = (std::max<uint32_t>)(1u, IniUint(path, L"auto_fire_interval_ms", 130u));
    aimConfig_.visibilityRequired = IniBool(path, L"visibility_required", true);
    aimConfig_.visibilityFailClosed = IniBool(path, L"visibility_fail_closed", true);
    aimConfig_.instantRangeDivisor = (std::max<uint32_t>)(1u, IniUint(path, L"instant_range_divisor", 8u));
    Log("aim_config_loaded", Feature::AimAutoFire, true,
        static_cast<uintptr_t>(aimConfig_.microRangeDivisor),
        static_cast<uint32_t>(aimConfig_.microRadiusPx));
}

bool GameHandlers::AcquireTarget(EntitySnapshot& out, AimBone* bone, bool instantSniper) {
    out = {};
    const auto list = entities_.Snapshot();
    if (list.empty()) { LogAimGate(1); targetReady_ = false; return false; }
    Matrix4 view{}; Matrix4 projection{}; Viewport vp{};
    if (!RenderAdapter::Instance().ReadFrame(view, projection, vp) ||
        vp.width <= 0.0f || vp.height <= 0.0f) {
        LogAimGate(2);
        lockedSlot_ = 0;
        targetReady_ = false;
        return false;
    }
    const uint32_t rangeDivisor = instantSniper ? aimConfig_.instantRangeDivisor
                                                : aimConfig_.microRangeDivisor;
    const AimWindow window{
        vp.x + vp.width * 0.5f,
        vp.y + vp.height * 0.5f,
        aimConfig_.microRadiusPx > 0.0f
            ? aimConfig_.microRadiusPx
            : vp.width / static_cast<float>(1u + rangeDivisor),
        aimConfig_.microRadiusPx > 0.0f
            ? aimConfig_.microRadiusPx
            : vp.height / static_cast<float>(1u + rangeDivisor)
    };

    // The ray test is fail-closed.  A missing local pawn or an unavailable
    // engine intersection entry must never turn the obstacle filter into a
    // wallhack by accident.
    LocalSnapshot local{};
    const bool localOk = entities_.ReadLocal(local) && local.address;
    if (aimConfig_.visibilityRequired && !localOk && aimConfig_.visibilityFailClosed) {
        LogAimGate(3);
        lockedSlot_ = 0;
        targetReady_ = false;
        return false;
    }
    // The source implementation calls IsVisible only when the local pawn is
    // fully initialized.  Preserve that lifecycle rule even when a user
    // explicitly disables the fail-closed override; the toggle only controls
    // whether the visibility gate is required, not whether a null pawn may be
    // passed into the engine's ray routine.
    const bool canCheckVisibility = aimConfig_.visibilityRequired && localOk;

    struct Candidate {
        const EntitySnapshot* entity{};
        AimBone bone{AimBone::Neck};
        size_t boneIndex{};
        float score{1.0e30f};
        float worldDistanceSq{1.0e30f};
    };
    const std::array<AimBone, 5> bones{{AimBone::Head, AimBone::Neck,
                                         AimBone::Chest, AimBone::Waist,
                                         AimBone::Butt}};
    Candidate best{};
    Candidate locked{};
    uint32_t bodyCandidates = 0;
    uint32_t blockedPoints = 0;
    const auto now = GetTickCount64();
    for (const auto& candidate : list) {
        if (!candidate.enemy || !candidate.alive || !candidate.bodyBounds.valid)
            continue;

        // First apply the body-shape/window intersection.  This is the
        // source-compatible "微型" rectangle when micro_radius_px=0, or a
        // true pixel radius when that value is explicitly configured.  The
        // AABB is deliberately used as the pixel proxy: a rendered torso can
        // intersect the window between two sampled joints even when neither
        // joint center lands inside the tiny radius.
        if (!BodyIntersectsAimWindow(candidate, window, aimConfig_.bodyPaddingPx,
                                     aimConfig_.microRadiusPx > 0.0f))
            continue;
        ++bodyCandidates;

        Candidate current{};
        current.entity = &candidate;
        current.score = DistanceToBody(candidate, window, aimConfig_.bodyPaddingPx);
        current.worldDistanceSq = localOk ? WorldDistanceSq(local.position, candidate.position) : 0.0f;
        bool pointFound = false;
        float bestPointScore = FLT_MAX;
        for (size_t i = 0; i < bones.size(); ++i) {
            if (!candidate.boneValid[i] || !candidate.boneScreenValid[i]) continue;
            const auto& p = candidate.boneScreen[i];
            if (canCheckVisibility &&
                !IsVisible(local.position, candidate.bones[i])) {
                ++blockedPoints;
                continue;
            }
            const float dx = p.x - window.cx;
            const float dy = p.y - window.cy;
            const float pointScore = dx * dx + dy * dy;
            if (!pointFound || pointScore < bestPointScore) {
                current.bone = bones[i];
                current.boneIndex = i;
                bestPointScore = pointScore;
                pointFound = true;
            }
        }
        if (!pointFound) continue;
        // Prefer the nearest visible sampled joint for the actual aim write;
        // the body AABB intersection above is what grants pixel-level
        // qualification when the nearest joint itself is just outside the
        // radius.
        current.score = DistanceToBody(candidate, window, aimConfig_.bodyPaddingPx);

        const auto better = [](const Candidate& a, const Candidate& b) {
            if (!a.entity) return false;
            if (!b.entity) return true;
            if (std::fabs(a.score - b.score) > 0.01f) return a.score < b.score;
            return a.worldDistanceSq < b.worldDistanceSq;
        };
        if (better(current, best)) best = current;
        if (candidate.slot == lockedSlot_) locked = current;
    }
    if (!best.entity) {
        LogAimGate(blockedPoints ? 5u : (bodyCandidates ? 6u : 4u));
        lockedSlot_ = 0;
        targetReady_ = false;
        return false;
    }

    // Keep a valid target for a short hold window.  It prevents one-frame
    // swaps between two pixels that are both inside the tiny window, while a
    // clearly nearer target still wins immediately.
    Candidate chosen = best;
    const bool holdLock = locked.entity && lockedSlot_ != 0 &&
        (now - lockedSince_ <= aimConfig_.lockHoldMs) &&
        locked.score <= best.score + aimConfig_.switchMarginPx;
    if (holdLock) chosen = locked;
    if (chosen.entity->slot != lockedSlot_) {
        lockedSlot_ = chosen.entity->slot;
        lockedSince_ = now;
        lockedScore_ = chosen.score;
    } else {
        lockedScore_ = chosen.score;
    }
    out = *chosen.entity;
    if (chosen.entity->slot != lastAimTargetSlot_ || now - lastAimTargetLog_ >= 250u) {
        lastAimTargetSlot_ = chosen.entity->slot;
        lastAimTargetLog_ = now;
        Log("aim_target", Feature::AimAutoFire, true,
            static_cast<uintptr_t>(chosen.entity->address),
            static_cast<uint32_t>(chosen.entity->slot));
    }
    if (bone) *bone = chosen.bone;
    targetReady_ = true;
    return true;
}

bool GameHandlers::IsVisible(const Vec3& me, const Vec3& target) const {
    const auto ecxSlot = CAbs(kIsVisibleEcxAbs);
    const auto callAddr = CAbs(kIsVisibleCallAbs);
    if (!ecxSlot || !callAddr || !Committed(ecxSlot) || !Committed(callAddr)) return false;
    const auto thisPtr = Read32(ecxSlot);
    if (!thisPtr || !Committed(thisPtr)) return false;

    // IntersectQuery/IntersectInfo begin with the six/three floats consumed
    // by the engine routine.  Reserve the complete prefixes from the source
    // layout and pass pointers exactly as CALL_2_ecx does.
    struct Query { float v[6]{}; uint8_t tail[0x48]{}; } query{};
    struct Info { float impact[3]{}; uint8_t tail[0x34]{}; } info{};
    query.v[0] = me.x; query.v[1] = me.z; query.v[2] = me.y;
    query.v[3] = target.x; query.v[4] = target.z; query.v[5] = target.y;
    using Fn = int(__thiscall*)(void*, void*, void*);
    auto fn = reinterpret_cast<Fn>(callAddr);
    int result = 0;
    __try {
        result = fn(reinterpret_cast<void*>(static_cast<uintptr_t>(thisPtr)), &query, &info);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return result != 1;
}

bool GameHandlers::AimTarget(const EntitySnapshot& target, AimBone bone) {
    auto pl = CurrentPlayer();
    if (!pl || !target.alive) return false;
    size_t idx = 1;
    switch (bone) { case AimBone::Head: idx=0; break; case AimBone::Neck: idx=1; break;
    case AimBone::Chest: idx=2; break; case AimBone::Waist: idx=3; break; case AimBone::Butt: idx=4; break; }
    Vec3 p = target.boneValid[idx] ? target.bones[idx] : target.position;
    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) return false;
    LocalSnapshot local{}; if (!entities_.ReadLocal(local)) return false;
    // TCII 新型模式的鼠标角度位于人物对象 +3464/+3468（源码中的
    // 鼠标Y偏移_j）；旧版 +1328 仅在旧模式使用。当前客户端签名是
    // 1.1.85.7，新型路径优先写入 +3464/+3468。
    // EntityAdapter normalises the client's X/Z/Y memory layout to the
    // source D3D fields: x=X, y=depth(Y), z=vertical(Z).  VectorToRotation
    // stores radians at +3464/+3468; writing degrees here silently produced
    // values dozens of times too large and made the game ignore the aim.
    const float dx = p.x - local.position.x;
    const float depth = p.y - local.position.y;
    const float vertical = p.z - local.position.z;
    const float horizontal = std::sqrt(dx * dx + depth * depth);
    const float yaw = std::atan2(dx, depth);
    const float pitch = std::atan2(-vertical, horizontal);
    if (!std::isfinite(yaw) || !std::isfinite(pitch)) return false;
    // Source writes angle.Y (pitch) at 鼠标Y偏移_j and angle.X (yaw) at
    // 鼠标Y偏移_j+4.  Keep that ordering for the 1.1.85.7 new-mode object.
    bool ok = WriteFloat(pl + 3464u, pitch) && WriteFloat(pl + 3468u, yaw);
    if (ok) {
        uint32_t pitchBits = 0;
        std::memcpy(&pitchBits, &pitch, sizeof(pitchBits));
        Log("aim_write", Feature::AimAutoFire, true, pl + 3464u, pitchBits);
    }
    return ok;
}

void GameHandlers::TickAimAndFire() {
    if (!Features().IsOn(Feature::AimAutoFire) || InstantSniper::Instance().SniperMode() || InstantSniper::Instance().Busy()) {
        targetReady_ = false; return;
    }
    // Ordinary aim/fire is active only while the user physically holds LMB.
    // Use OS async state as the source of truth: injected clicks must never
    // satisfy this gate, and a missed hook-up edge must not latch auto-fire.
    // 仅使用低级鼠标钩子记录的非注入边沿；自动 LEFTDOWN/LEFTUP 不会
    // 改变物理保持状态，用户抬起左键后下一个工作 tick 立即停止。
    const bool physicalLmb = InputRouter::Instance().Physical(VK_LBUTTON);
    if (!physicalLmb) {
        targetReady_ = false;
        LogAimGate(7);
        return;
    }
    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) return;
    EntitySnapshot target{}; AimBone bone{};
    if (!AcquireTarget(target, &bone)) return;
    if (!AimTarget(target, bone)) return;
    // Alt+Z + physical LMB 普通模式为自动开枪；节流到 aim.auto_fire_interval_ms。
    const auto now = GetTickCount64();
    if (now - lastAutoFire_ >= aimConfig_.autoFireIntervalMs) {
        lastAutoFire_ = now;
        // TCII's original path uses the legacy mouse_event API.  On this
        // client it reaches the DirectInput mouse queue more reliably than a
        // pure SendInput pair.
        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
        const UINT sent = 1u; // one legacy click dispatched (down + up)
        if (now - lastAutoFireLog_ >= 250u) {
            lastAutoFireLog_ = now;
            Log("auto_fire_sent", Feature::AimAutoFire, true, 0, sent);
        }
    }
}

void GameHandlers::LogEntitySnapshot() {
    const auto now = GetTickCount64();
    if (now - lastEntityLog_ < 250) return;
    lastEntityLog_ = now;
    LocalSnapshot local{};
    const bool localOk = entities_.ReadLocal(local);
    const auto entities = entities_.Snapshot();
    char path[MAX_PATH]{};
    GetTempPathA(sizeof(path), path);
    // Keep the diagnostic filename ASCII so PowerShell and the game locale
    // resolve the same path (the previous UTF-8 narrow literal became a
    // mojibake filename under the system code page).
    std::strcat(path, "d3dref9_entities.csv");
    FILE* fp = nullptr;
    fopen_s(&fp, path, "a");
    if (!fp) return;
    static bool header = false;
    if (!header) {
        std::fprintf(fp, "tick,local_ok,local_addr,local_slot,mode,local_alive,spectating,coord_table,entity_slot,entity_addr,alive,enemy,zombie,x,y,z,pos_valid,bone_mask,screen_valid,screen_x,screen_y,screen_z,body_valid,body_left,body_top,body_right,body_bottom,bone_screen_mask\n");
        header = true;
    }
    if (entities.empty()) {
        std::fprintf(fp, "%llu,%u,0x%08Ix,%u,%u,%u,%u,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0\n",
                     static_cast<unsigned long long>(now), localOk ? 1u : 0u,
                     static_cast<size_t>(local.address), local.slot, local.mode,
                     local.alive ? 1u : 0u, local.spectating ? 1u : 0u);
    } else {
        for (const auto& e : entities) {
            unsigned mask = 0, screenMask = 0;
            for (size_t i = 0; i < e.boneValid.size(); ++i) if (e.boneValid[i]) mask |= 1u << i;
            for (size_t i = 0; i < e.boneScreenValid.size(); ++i) if (e.boneScreenValid[i]) screenMask |= 1u << i;
            std::fprintf(fp, "%llu,%u,0x%08Ix,%u,%u,%u,%u,0x%08Ix,%u,0x%08Ix,%u,%u,%u,%.3f,%.3f,%.3f,%u,%u,%u,%.3f,%.3f,%.3f,%u,%.3f,%.3f,%.3f,%.3f,%u\n",
                         static_cast<unsigned long long>(now), localOk ? 1u : 0u,
                         static_cast<size_t>(local.address), local.slot, local.mode,
                         local.alive ? 1u : 0u, local.spectating ? 1u : 0u,
                         static_cast<size_t>(entities_.CoordinateTable()),
                         e.slot, static_cast<size_t>(e.address), e.alive ? 1u : 0u,
                         e.enemy ? 1u : 0u, static_cast<unsigned>(e.zombieState),
                         e.position.x, e.position.y, e.position.z,
                         e.positionValid ? 1u : 0u, mask, e.screenValid ? 1u : 0u,
                         e.screen.x, e.screen.y, e.screen.z,
                         e.bodyBounds.valid ? 1u : 0u,
                         e.bodyBounds.left, e.bodyBounds.top,
                         e.bodyBounds.right, e.bodyBounds.bottom,
                         screenMask);
        }
    }
    std::fclose(fp);
}
void GameHandlers::SetStatic(Feature f, bool on) {
    if (!on) {
        RestoreFeature(f);
        if (f == Feature::OptimizeProcess) {
            SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
            SetProcessPriorityBoost(GetCurrentProcess(), TRUE);
        }
        // The source explicitly returns the camera mode to first person.
        // Do not retain a stale value captured while another third-person
        // writer was already active.
        if (f == Feature::ThirdPerson) {
            const auto view = Read32(S(kViewRootRva));
            if (view) Write32(view + 100u, 1u);
        }
        if (f == Feature::TeleportGround) teleportDone_ = false;
        return;
    }
    uintptr_t p = 0; uint32_t v = 0;
    switch (f) {
    case Feature::PlayerEsp:
        p = CAbs(kCrossfireEspAbs); if (!p) return;
        WriteFeature32(f, p, 0u); WriteFeature32(f, p - 8, 3u); WriteFeature32(f, p - 20, 0u); break;
    case Feature::ThirdPerson: { auto pl = CurrentPlayer(); auto view = Read32(S(kPlayerPtr - 72)); if (pl && view) WriteFeature32(f, view + 100, 3u); break; }
    case Feature::BulletWall: {
        // Source: *(CShell+weapon_rva-24) is the table root.  It is unrelated
        // to player-24 (which contains object metadata in this build).
        const auto w = Read32(S(kWeaponPtr) - 24u); if (!w) return;
        // TCII变态版(加追敌和跳舞): eight entries, stride 1428,
        // five penetration flags set to 1.
        for (uint32_t i = 0; i < 8; ++i)
            for (uint32_t off : {1392u, 1512u, 1644u, 1656u, 1668u})
                WriteFeature32(f, w + i * 1428u + off, 1u);
        break; }
    case Feature::Headshot: {
        auto wroot = Read32(S(kWeaponPtr) - 212u); if (!wroot) return;
        // TCII变态版(加追敌和跳舞) 开启刀枪爆头(): three angle
        // floats at +56/+60/+64 are raised from 8.0f to 180.0f.
        for (uint32_t off : {56u, 60u, 64u})
            WriteFeature32(f, wroot + off, 0x43340000u); // 180.0f
        break;
    }
    case Feature::OptimizeProcess: {
        SetPriorityClass(GetCurrentProcess(), on ? HIGH_PRIORITY_CLASS : NORMAL_PRIORITY_CLASS);
        SetProcessPriorityBoost(GetCurrentProcess(), on ? FALSE : TRUE);
        OptimizeHelperProcesses(on);
        break;
    }
    case Feature::Radio: {
        auto a = S(kRadioStateRva); auto b = S(kRadioBlockRva);
        if (a && b) { WriteFeature32(f, b, 0u); }
        break;
    }
    case Feature::TeleportGround:
        if (!on) teleportDone_ = false;
        break;
    default: break;
    }
}
void GameHandlers::TickDynamic(Feature f, bool) {
    auto pl = CurrentPlayer();
    // TCII keeps two player layouts.  The newer layout marks the live state
    // at +1320 and uses +1456 for the fall-damage immunity flag; the legacy
    // layout marks it at +1560 and uses +1692.  Handle this feature before the
    // common new-layout Alive() guard so F9+V also works on old maps/modes.
    if (f == Feature::FallNoDamage) {
        if (!pl) return;
        if (Read32(pl + 1320u) == 1u)
            WriteFeature32(f, pl + 1456u, 0xFFFFFFFFu);
        else if (Read32(pl + 1560u) == 1u)
            WriteFeature32(f, pl + 1692u, 0xFFFFFFFFu);
        return;
    }
    if (!pl || !Alive(pl)) return;
    switch (f) {
    case Feature::Radio: {
        const auto now = GetTickCount64();
        if (now - lastRadioPulse_ < 250u) break;
        lastRadioPulse_ = now;
        const auto a = S(kRadioStateRva), b = S(kRadioBlockRva);
        if (!a || !b || Read32(CAbs(kEscMenuAbs)) != 0u || Read32(S(0x1AE46C4u)) != 0u) break;
        WriteFeature32(f, b, 0u);
        WriteFeature32(f, a, static_cast<uint32_t>(now % 4u));
        break;
    }
    case Feature::NoRecoil: WriteFeature32(f, pl + 1376, 0xFFFFFFFFu); WriteFeature32(f, pl + 1384, 1u); break;
    case Feature::InstantReload: if (Read32(pl + 712) <= 1u && Read32(pl + 1600) == 5u) WriteFeature32(f, pl + 1600, 2u); break;
    case Feature::BunnyHop:
        WriteFeature32(f, pl + 2906u, 257u);
        WriteFeature32(f, pl + 1428u, 1u);
        break;
    case Feature::PlayerNoclip:
        WriteFeature32(f, pl + 3640u, 0xC0A00000u);
        WriteFeature32(f, pl + 3648u, 0xC0A00000u);
        break;
    case Feature::RoomStay: {
        auto now = GetTickCount64();
        if (Read32(S(kLobby)) == 13u && now - lastRoomPulse_ >= 20000u) {
            lastRoomPulse_ = now;
            HWND w = FindWindowW(L"CrossFire", L"穿越火线");
            if (w) { PostMessageW(w, WM_KEYDOWN, VK_F5, 0); PostMessageW(w, WM_KEYUP, VK_F5, 0); }
        }
        break;
    }
    case Feature::TeleportGround: {
        if (teleportDone_) break;
        // 仅使用当前实体坐标表中的第一个存活敌方位置；一次性写入，
        // 再由关闭处理恢复由游戏继续维护的位置。
        EntitySnapshot target{};
        if (!AcquireTarget(target, nullptr)) break;
        auto posRoot = Read32(S(kPlayerPtr) + 3444u);
        if (!posRoot) break;
        uint32_t tx{}, ty{}, tz{};
        std::memcpy(&tx, &target.position.x, sizeof(tx));
        std::memcpy(&ty, &target.position.y, sizeof(ty));
        std::memcpy(&tz, &target.position.z, sizeof(tz));
        if (WriteFeature32(f, posRoot + 224u, tx) &&
            WriteFeature32(f, posRoot + 228u, ty) &&
            WriteFeature32(f, posRoot + 232u, tz)) teleportDone_ = true;
        break;
    }
    default: break;
    }
}
}




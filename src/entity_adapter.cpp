#include "entity_adapter.h"

#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <algorithm>

namespace d3dref9 {
namespace {
constexpr uintptr_t kPlayersArrayRva = 0x166AD00u;
constexpr uintptr_t kGameModeRva = 0x166ACE0u;
constexpr uintptr_t kFovRva = 0x1E725B8u;
constexpr uintptr_t kObjectOffset = 0x210u;
// TCII source uses 本人ID=520 (0x208) as an index into the first-level
// player-pointer table.  It is not a field on the players-array pointer;
// dereference the array first, then read the slot pointer at +0x208.
constexpr uintptr_t kLocalIndexOffset = 0x208u;
// The local transform is reached through *(玩家数组 + 48).  The value at
// 玩家数组 + 0x208 is only the local slot pointer used by 取自己位置().
constexpr uintptr_t kLocalPersonOffset = 0x30u;
constexpr uintptr_t kSlotStride = 0xD80u;       // 3456
constexpr uintptr_t kSlotTableOffset = 0x14u;   // OBJECT + 20
constexpr uintptr_t kZombieStateOffset = 0x123BCu;
// TCII's 取敌人生存() returns TRUE when byte(entity + 516) == 0.
// The previous adapter inverted this field, causing every live target to be
// filtered out by AcquireTarget().
constexpr uintptr_t kEnemyAliveOffset = 0x204u; // WOD_读字节(slot + 516)
constexpr uintptr_t kViewRootOffset = 0x30u;
constexpr uintptr_t kLocalYawOffset = 0x44u;
constexpr uintptr_t kLocalPitchOffset = 0x48u;
constexpr uintptr_t kLocalPositionOffset = 0x6Cu;
constexpr uintptr_t kSpectatorOffset = 0x613Cu; // 24892
constexpr uintptr_t kCoordStride = 0x40u;
constexpr uintptr_t kCoordBase = 0x0Cu;
// Source mapping: head=5, neck=6, chest=4, waist=3, butt=2.
// Keep the array order identical to AimBone indexing used by the controller.
constexpr std::array<uint32_t, 5> kAimParts{{5u, 6u, 4u, 3u, 2u}};
constexpr uintptr_t kImageBase = 0x00400000u;
constexpr uintptr_t kCoordinateHookAbsolute = 0x0063567Fu;
constexpr std::array<uint8_t, 6> kCoordinateHookBytes{{0x89, 0x8D, 0x20, 0xFF, 0xFF, 0xFF}};

std::atomic<EntityAdapter*> g_activeAdapter{nullptr};
std::atomic<uint64_t> g_coordinateCallbacks{0};

HMODULE Shell() { return GetModuleHandleW(L"cshell.dll"); }

bool Committed(uintptr_t p, size_t n) {
    if (!p || n == 0 || p + n < p) return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(reinterpret_cast<const void*>(p), &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if ((mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) return false;
    const auto begin = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
    return p >= begin && p + n <= begin + mbi.RegionSize;
}

void HookLog(const char* fmt, ...) {
    char path[MAX_PATH]{};
    GetTempPathA(sizeof(path), path);
    std::strcat(path, "d3dref9_entity_hook.log");
    FILE* fp = nullptr;
    fopen_s(&fp, path, "a");
    if (!fp) return;
    va_list ap; va_start(ap, fmt); std::vfprintf(fp, fmt, ap); va_end(ap);
    std::fputc('\n', fp); std::fclose(fp);
}

bool WriteCode(uintptr_t address, const void* bytes, size_t size) {
    if (!Committed(address, size)) return false;
    DWORD old = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(address), size, PAGE_EXECUTE_READWRITE, &old)) return false;
    std::memcpy(reinterpret_cast<void*>(address), bytes, size);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(address), size);
    DWORD ignored = 0; VirtualProtect(reinterpret_cast<void*>(address), size, old, &ignored);
    return true;
}

void EmitMovEax(std::vector<uint8_t>& out, uintptr_t value) {
    out.push_back(0xB8);
    const auto v = static_cast<uint32_t>(value);
    for (unsigned i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
}

struct Vec4 { float x{}, y{}, z{}, w{}; };

Vec4 Transform(const Vec4& v, const Matrix4& m) {
    return {
        v.x * m.m[0] + v.y * m.m[4] + v.z * m.m[8] + v.w * m.m[12],
        v.x * m.m[1] + v.y * m.m[5] + v.z * m.m[9] + v.w * m.m[13],
        v.x * m.m[2] + v.y * m.m[6] + v.z * m.m[10] + v.w * m.m[14],
        v.x * m.m[3] + v.y * m.m[7] + v.z * m.m[11] + v.w * m.m[15]
    };
}
} // namespace

bool EntityAdapter::Initialize() {
    shell_ = Shell();
    coordinateTable_ = 0;
    pendingHookTable_ = 0;
    pendingHookObject_ = 0;
    g_activeAdapter.store(this, std::memory_order_release);
    if (!shell_) return false;
    // The source-compatible coordinate hook is installed lazily once the
    // client has mapped its game code.  The live 1.1.85.7 bytes at
    // crossfire+0x23567F match kCoordinateHookBytes; without this hook
    // coordinateTable_ remains zero and every bone read is empty.
    return true;
}

bool EntityAdapter::EnsureCoordinateHook() { return InstallCoordinateHook(); }

void EntityAdapter::SetRenderFrame(const Matrix4& view, const Matrix4& projection, const Viewport& viewport) {
    view_ = view;
    projection_ = projection;
    viewport_ = viewport;
    frameReady_ = viewport.width > 0.0f && viewport.height > 0.0f;
}

void EntityAdapter::Shutdown() {
    std::lock_guard<std::mutex> lock(hookMu_);
    RemoveCoordinateHook();
    if (g_activeAdapter.load(std::memory_order_acquire) == this)
        g_activeAdapter.store(nullptr, std::memory_order_release);
    coordinatePointer_.store(0, std::memory_order_release);
    coordinateTable_.store(0, std::memory_order_release);
    pendingHookTable_.store(0, std::memory_order_release);
    pendingHookObject_.store(0, std::memory_order_release);
    for (auto& p : coordinatePointers_) p.store(0, std::memory_order_release);
}

bool EntityAdapter::InstallCoordinateHook() {
    std::lock_guard<std::mutex> lock(hookMu_);
    if (hookInstalled_) return true;
    const auto cf = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"crossfire.exe"));
    if (!cf) { HookLog("event=skip reason=crossfire_missing"); return false; }
    const auto target = cf + (kCoordinateHookAbsolute - kImageBase);
    if (!Committed(target, kCoordinateHookBytes.size())) {
        HookLog("event=skip reason=target_uncommitted target=0x%08Ix", static_cast<size_t>(target));
        return false;
    }
    std::array<uint8_t, 6> current{};
    std::memcpy(current.data(), reinterpret_cast<const void*>(target), current.size());
    if (current != kCoordinateHookBytes) {
        HookLog("event=skip reason=signature_mismatch target=0x%08Ix bytes=%02X%02X%02X%02X%02X%02X",
                static_cast<size_t>(target), current[0], current[1], current[2], current[3], current[4], current[5]);
        return false;
    }
    auto* relay = static_cast<uint8_t*>(VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!relay) { HookLog("event=skip reason=virtual_alloc"); return false; }
    std::vector<uint8_t> code;
    code.reserve(32);
    code.push_back(0x60); // pushad
    code.push_back(0x50); // original EAX (second callback argument)
    code.push_back(0x51); // original ECX (first callback argument)
    EmitMovEax(code, reinterpret_cast<uintptr_t>(&EntityAdapter::CoordinateHookCallback));
    code.push_back(0xFF); code.push_back(0xD0); // call eax
    // CoordinateHookCallback is __cdecl, so it leaves its two 32-bit
    // arguments on the stack.  Discard them before POPAD; otherwise POPAD
    // would consume the callback arguments as saved registers and return to
    // the game with a corrupted ESP/register set.
    code.push_back(0x83); code.push_back(0xC4); code.push_back(0x08); // add esp, 8
    code.push_back(0x61); // popad
    code.insert(code.end(), current.begin(), current.end());
    EmitMovEax(code, target + current.size());
    code.push_back(0xFF); code.push_back(0xE0); // jmp eax
    std::memcpy(relay, code.data(), code.size());
    FlushInstructionCache(GetCurrentProcess(), relay, code.size());

    std::array<uint8_t, 6> patch{{0xE9, 0, 0, 0, 0, 0x90}};
    const auto rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(relay) - (target + 5));
    std::memcpy(patch.data() + 1, &rel, sizeof(rel));
    if (!WriteCode(target, patch.data(), patch.size())) {
        VirtualFree(relay, 0, MEM_RELEASE);
        HookLog("event=skip reason=patch_failed target=0x%08Ix", static_cast<size_t>(target));
        return false;
    }
    hookTarget_ = target;
    hookTrampoline_ = relay;
    hookOriginal_ = current;
    hookInstalled_ = true;
    HookLog("event=installed target=0x%08Ix relay=0x%08Ix", static_cast<size_t>(target), static_cast<size_t>(reinterpret_cast<uintptr_t>(relay)));
    return true;
}

void EntityAdapter::RemoveCoordinateHook() {
    if (!hookInstalled_) return;
    if (hookTarget_) WriteCode(hookTarget_, hookOriginal_.data(), hookOriginal_.size());
    if (hookTrampoline_) VirtualFree(hookTrampoline_, 0, MEM_RELEASE);
    HookLog("event=removed target=0x%08Ix", static_cast<size_t>(hookTarget_));
    hookTarget_ = 0;
    hookTrampoline_ = nullptr;
    hookOriginal_ = {};
    hookInstalled_ = false;
}

void __cdecl EntityAdapter::CoordinateHookCallback(uintptr_t table, uintptr_t object) {
    g_coordinateCallbacks.fetch_add(1, std::memory_order_relaxed);
    auto* adapter = g_activeAdapter.load(std::memory_order_acquire);
    // No VirtualQuery, file I/O, or slot loops on the game thread.
    if (adapter) {
        adapter->pendingHookTable_.store(table, std::memory_order_release);
        adapter->pendingHookObject_.store(object, std::memory_order_release);
    }
}

void EntityAdapter::RefreshCoordinateCapture() {
    const auto table = pendingHookTable_.exchange(0, std::memory_order_acq_rel);
    const auto object = pendingHookObject_.exchange(0, std::memory_order_acq_rel);
    if (table && object) CaptureCoordinatePointer(object, table);
}

void EntityAdapter::CaptureCoordinatePointer(uintptr_t object, uintptr_t pointer) {
    if (!object || object < 0x10000u || !pointer || pointer < 0x10000u) return;
    const auto root = EntityRoot();
    if (!root) return;
    uint32_t matchedSlot = 0;
    for (uint32_t slot = 1; slot <= 16; ++slot) {
        uint32_t candidate = 0;
        if (Read32(root + kSlotTableOffset + (slot - 1) * kSlotStride, candidate) && candidate == object) {
            matchedSlot = slot;
            break;
        }
    }
    if (!matchedSlot || !Committed(pointer, sizeof(uint32_t))) return;
    // Source: 数据指针 = 汇编取变量_整数型 (坐标指针[1]).  ECX is
    // therefore a pointer-to-table, not the table base itself.
    uint32_t table = 0;
    if (!Read32(pointer, table) || !table || !Committed(table, 16 * sizeof(uint32_t))) return;
    uint32_t first = 0;
    if (!Read32(table, first) || !first || !Committed(first, 4)) return;
    coordinatePointers_[matchedSlot].store(pointer, std::memory_order_release);
    coordinatePointer_.store(pointer, std::memory_order_release);
    // The source's 数据指针 is loaded from 坐标指针[1].  Do not replace the
    // active table with a callback for another slot.
    if (matchedSlot == 1) {
        const auto old = coordinateTable_.exchange(static_cast<uintptr_t>(table), std::memory_order_acq_rel);
        if (old != table)
            HookLog("event=capture slot=%u object=0x%08Ix pointer=0x%08Ix table=0x%08Ix", matchedSlot, static_cast<size_t>(object), static_cast<size_t>(pointer), static_cast<size_t>(table));
    }
}

bool EntityAdapter::ReadBytes(uintptr_t address, void* out, size_t size) {
    if (!out || !Committed(address, size)) return false;
    std::memcpy(out, reinterpret_cast<const void*>(address), size);
    return true;
}

bool EntityAdapter::Read32(uintptr_t address, uint32_t& out) {
    return ReadBytes(address, &out, sizeof(out));
}

bool EntityAdapter::Read8(uintptr_t address, uint8_t& out) {
    return ReadBytes(address, &out, sizeof(out));
}

bool EntityAdapter::ReadFloat(uintptr_t address, float& out) {
    return ReadBytes(address, &out, sizeof(out)) && std::isfinite(out);
}

uintptr_t EntityAdapter::PlayersRoot() const {
    if (!shell_) return 0;
    uint32_t p = 0;
    return Read32(reinterpret_cast<uintptr_t>(shell_) + kPlayersArrayRva, p) ? p : 0;
}

uintptr_t EntityAdapter::EntityRoot() const {
    const auto p = PlayersRoot();
    return p ? p + kObjectOffset : 0;
}

uint32_t EntityAdapter::Mode() const {
    uint32_t mode = 0;
    if (!shell_ || !Read32(reinterpret_cast<uintptr_t>(shell_) + kGameModeRva, mode)) return 0;
    return mode;
}

uint32_t EntityAdapter::DisplayCount(uint32_t mode) const {
    if (mode == 11 || mode == 24 || mode == 31) return 0;
    if (mode == 4 || mode == 27 || mode == 17 || mode == 29 || mode == 33 ||
        mode == 36 || mode == 41) return 16;
    if (mode == 14 || mode == 19 || mode == 5 || mode == 12 || mode == 18 || mode == 42) return 15;
    return 8;
}

uint32_t EntityAdapter::LocalSlot() const {
    const auto p = PlayersRoot();
    uint32_t local = 0;
    // The source stores a zero-based local index at playersRoot+0x208.
    // Zero is therefore a valid value and denotes slot 1; treating it as a
    // null pointer made every normal 8-player match lose its local/enemy
    // relationship and prevented target acquisition entirely.
    if (!p || !Read32(p + kLocalIndexOffset, local)) return 0;
    // Source: 取自己位置() = *( *(玩家数组) + 本人ID ) + 1.
    // The slot table is 1-based, while the source index is zero-based.
    return local < 16 ? local + 1u : 0u;
}

bool EntityAdapter::ReadLocal(LocalSnapshot& out) const {
    out = {};
    if (!shell_) return false;
    const auto p = PlayersRoot();
    uint32_t person = 0;
    // Source 取自己数据(): 临时人物=*(玩家数组), 人物一级=*(临时人物+48).
    // The prior implementation incorrectly treated +0x208 as the transform
    // pointer, which only works as a slot lookup and yields lobby garbage.
    if (!p || !Read32(p + kLocalPersonOffset, person) || !person) return false;
    out.address = person;
    out.slot = LocalSlot();
    out.mode = Mode();
    uint32_t alive = 0;
    const auto player = static_cast<uintptr_t>(Read32(reinterpret_cast<uintptr_t>(shell_) + 0x16B3FD0u, alive) ? alive : 0u);
    uint32_t playerAlive = 0;
    if (player) Read32(player + 1320u, playerAlive);
    // The global flag is valid during map transitions; once the pawn exists,
    // its +1320 field is the source-compatible live-state value.
    out.alive = player ? playerAlive == 1u : alive == 1u;
    uint32_t spectator = 0;
    out.spectating = player && Read32(player + kSpectatorOffset, spectator) && spectator != 0;
    // 人物一级 already is the transform block in TCII; its fields are
    // directly at +68/+72/+108/+112/+116.  Do not dereference +0x30 again.
    const bool anglesOk = ReadFloat(person + kLocalYawOffset, out.yaw) &&
                          ReadFloat(person + kLocalPitchOffset, out.pitch);
    // TCII's local transform uses the same X/Z/Y memory order as the
    // coordinate table: +0 = X, +4 = Z (depth), +8 = Y (vertical).
    const bool positionOk = ReadFloat(person + kLocalPositionOffset + 0, out.position.x) &&
                            ReadFloat(person + kLocalPositionOffset + 4, out.position.z) &&
                            ReadFloat(person + kLocalPositionOffset + 8, out.position.y);
    ReadFloat(reinterpret_cast<uintptr_t>(shell_) + kFovRva, out.fov);
    if (!anglesOk || !positionOk) return false;
    uint8_t zombie = 0;
    if (out.slot) {
        uint32_t ent = 0;
        if (Read32(EntityRoot() + kSlotTableOffset + (out.slot - 1) * kSlotStride, ent))
            Read8(static_cast<uintptr_t>(ent) + kZombieStateOffset, zombie);
    }
    out.zombieState = zombie;
    return true;
}

void EntityAdapter::SetCoordinateTable(uintptr_t table) {
    coordinateTable_ = table;
}

bool EntityAdapter::ReadPosition(uint32_t slot, uint32_t part, Vec3& out) const {
    out = {};
    if (slot == 0 || slot > 16) return false;

    // TCII source chain (取敌人坐标WW):
    // data = *(坐标指针[1]); xx = *(data + (slot-1)*4);
    // x/z/y = xx + 12 + 64*part at offsets 0/16/32.
    // The table is populated by the coordinate hook at 0x63567F.  Do not
    // infer a coordinate block from entity+0x2098; that chain is unrelated in
    // this client and produced zero/invalid aim points.
    const auto table = coordinateTable_.load(std::memory_order_acquire);
    if (!table) return false;
    uint32_t coordinate = 0;
    if (!Read32(table + (slot - 1u) * sizeof(uint32_t), coordinate) || !coordinate)
        return false;
    const auto base = static_cast<uintptr_t>(coordinate) + kCoordBase + kCoordStride * part;
    if (!ReadFloat(base + 0, out.x) || !ReadFloat(base + 16, out.z) || !ReadFloat(base + 32, out.y)) return false;
    if (out.x == 0.0f || out.x == -100000.0f) return false;
    return std::isfinite(out.x) && std::isfinite(out.y) && std::isfinite(out.z);
}

bool EntityAdapter::IsEnemy(uint32_t slot, uint32_t count, uint32_t localSlot,
                            uint8_t localZombie, uint8_t targetZombie) const {
    if (!localSlot || slot == localSlot) return false;
    if (count == 16) return true;
    if (count == 8) return localSlot <= 8 ? slot > 8 : slot <= 8;
    if (count == 15) {
        const auto norm = [](uint8_t v) { return (v == 1 || v == 0 || v == 777) ? v : uint8_t{1}; };
        const auto a = norm(localZombie), b = norm(targetZombie);
        return a != 777 && b != 777 && a != b;
    }
    return false;
}

std::vector<EntitySnapshot> EntityAdapter::Snapshot() const {
    std::vector<EntitySnapshot> result;
    const auto root = EntityRoot();
    if (!root) return result;
    const auto count = DisplayCount(Mode());
    if (count == 0) return result;
    const auto local = LocalSlot();
    uint8_t localZombie = 0;
    if (local) {
        uint32_t e = 0;
        if (Read32(root + kSlotTableOffset + (local - 1) * kSlotStride, e)) Read8(e + kZombieStateOffset, localZombie);
    }
    for (uint32_t slot = 1; slot <= 16; ++slot) {
        uint32_t entity = 0;
        if (!Read32(root + kSlotTableOffset + (slot - 1) * kSlotStride, entity) || !entity) continue;
        EntitySnapshot s{};
        s.slot = slot;
        s.address = entity;
        uint8_t alive = 0;
        s.alive = Read8(static_cast<uintptr_t>(entity) + kEnemyAliveOffset, alive) && alive == 0;
        Read8(static_cast<uintptr_t>(entity) + kZombieStateOffset, s.zombieState);
        // TCII's normal 8-player mode determines teams by slot halves; the
        // +0x400 byte is not stable across this client build, so do not let a
        // coincidental 0/1 value override the source-compatible fallback.
        s.enemy = IsEnemy(slot, count, local, localZombie, s.zombieState);
        // Read the complete coordinate block once per slot.  The previous
        // path called ReadFloat/VirtualQuery up to 18 times per slot and was
        // sampled every 5–10 ms, which consumed noticeable frame time.
        const auto table = coordinateTable_.load(std::memory_order_acquire);
        uint32_t coordinate = 0;
        std::array<uint8_t, 428> raw{};
        if (table && Read32(table + (slot - 1u) * sizeof(uint32_t), coordinate) &&
            coordinate && ReadBytes(static_cast<uintptr_t>(coordinate) + kCoordBase,
                                    raw.data(), raw.size())) {
            auto readPart = [&](uint32_t part, Vec3& out) {
                const size_t off = static_cast<size_t>(part) * kCoordStride;
                std::memcpy(&out.x, raw.data() + off + 0, sizeof(float));
                std::memcpy(&out.z, raw.data() + off + 16, sizeof(float));
                std::memcpy(&out.y, raw.data() + off + 32, sizeof(float));
                return std::isfinite(out.x) && std::isfinite(out.y) &&
                       std::isfinite(out.z) && out.x != 0.0f && out.x != -100000.0f;
            };
            s.positionValid = readPart(0, s.position);
            for (size_t i = 0; i < kAimParts.size(); ++i)
                s.boneValid[i] = readPart(kAimParts[i], s.bones[i]);
        }
        if (frameReady_) {
            Vec3 source = s.position;
            if (!s.positionValid) {
                for (const auto& bone : s.bones) {
                    if (bone.x != 0.0f || bone.y != 0.0f || bone.z != 0.0f) { source = bone; break; }
                }
            }
            // TCII stores coordinates as X/Z/Y; D3DX expects X/Y/Z.
            const Vec3 d3d{source.x, source.z, source.y};
            s.screenValid = WorldToScreen(d3d, view_, projection_, viewport_, s.screen);

            // Keep the body proxy separate from the root projection.  A root
            // point can be several pixels away from the rendered model (and
            // can even be hidden below the floor), while any one of the five
            // sampled bones may already be inside the user's micro window.
            float left = 1.0e30f, top = 1.0e30f;
            float right = -1.0e30f, bottom = -1.0e30f;
            bool haveBodyPoint = false;
            for (size_t i = 0; i < s.bones.size(); ++i) {
                if (!s.boneValid[i]) continue;
                const Vec3 boneD3d{s.bones[i].x, s.bones[i].z, s.bones[i].y};
                ScreenPoint projected{};
                if (!WorldToScreen(boneD3d, view_, projection_, viewport_, projected))
                    continue;
                s.boneScreen[i] = projected;
                s.boneScreenValid[i] = true;
                haveBodyPoint = true;
                left = (std::min)(left, projected.x);
                top = (std::min)(top, projected.y);
                right = (std::max)(right, projected.x);
                bottom = (std::max)(bottom, projected.y);
            }
            if (haveBodyPoint) {
                s.bodyBounds.left = left;
                s.bodyBounds.top = top;
                s.bodyBounds.right = right;
                s.bodyBounds.bottom = bottom;
                s.bodyBounds.valid = true;
            }
        }
        result.push_back(s);
    }
    return result;
}

bool EntityAdapter::WorldToScreen(const Vec3& world, const Matrix4& view,
                                  const Matrix4& projection, const Viewport& viewport,
                                  ScreenPoint& out) {
    const auto viewSpace = Transform({world.x, world.y, world.z, 1.0f}, view);
    const auto clip = Transform(viewSpace, projection);
    const float clipW = clip.w;
    if (!std::isfinite(clipW) || std::fabs(clipW) < 1e-6f) return false;
    const float nx = clip.x / clipW, ny = clip.y / clipW, nz = clip.z / clipW;
    if (!std::isfinite(nx) || !std::isfinite(ny) || !std::isfinite(nz)) return false;
    out.x = viewport.x + (1.0f + nx) * viewport.width * 0.5f;
    out.y = viewport.y + (1.0f - ny) * viewport.height * 0.5f;
    out.z = viewport.minZ + nz * (viewport.maxZ - viewport.minZ);
    return out.x >= viewport.x && out.x <= viewport.x + viewport.width &&
           out.y >= viewport.y && out.y <= viewport.y + viewport.height && out.z >= viewport.minZ && out.z <= viewport.maxZ;
}

bool EntityAdapter::SelectNearest(const std::vector<EntitySnapshot>& entities,
                                  float cx, float cy, EntitySnapshot& out) {
    bool found = false;
    float best = 0.0f;
    for (const auto& e : entities) {
        if (!e.enemy || !e.alive || !e.screenValid) continue;
        const float dx = e.screen.x - cx, dy = e.screen.y - cy;
        const float d = dx * dx + dy * dy;
        if (!found || d < best) { found = true; best = d; out = e; }
    }
    return found;
}

} // namespace d3dref9

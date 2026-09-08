#pragma once

#include "common.h"

namespace d3dref9 {

struct Vec3 {
    float x{};
    float y{};
    float z{};
};

struct Matrix4 {
    float m[16]{};
};

struct Viewport {
    float x{};
    float y{};
    float width{};
    float height{};
    float minZ{};
    float maxZ{1.0f};
};

struct ScreenPoint {
    float x{};
    float y{};
    float z{};
};

struct ScreenBounds {
    float left{};
    float top{};
    float right{};
    float bottom{};
    bool valid{};
};

struct EntitySnapshot {
    uint32_t slot{};
    uintptr_t address{};
    bool alive{};
    bool enemy{};
    uint8_t zombieState{};
    bool positionValid{};
    Vec3 position{};
    std::array<Vec3, 5> bones{};
    std::array<bool, 5> boneValid{};
    // Each bone is projected independently.  The five points are used as a
    // conservative screen-space body proxy by the aim selector; using only
    // the entity root made a target fail to qualify when a shoulder/hip was
    // the first visible pixel inside the magnetic window.
    std::array<ScreenPoint, 5> boneScreen{};
    std::array<bool, 5> boneScreenValid{};
    ScreenBounds bodyBounds{};
    bool screenValid{};
    ScreenPoint screen{};
};

struct LocalSnapshot {
    uintptr_t address{};
    uint32_t slot{};
    uint32_t mode{};
    bool alive{};
    bool spectating{};
    uint8_t zombieState{};
    Vec3 position{};
    float yaw{};
    float pitch{};
    float fov{};
};

// Adapter for the 1.1.85.7 x86 client.  Entity fields are read-only; the
// optional coordinate hook is the sole code patch and is installed only after
// its six-byte signature matches.  Until that hook captures a table,
// positionValid remains false while entity/team data can still be inspected.
class EntityAdapter {
public:
    bool Initialize();
    void Shutdown();
    bool Ready() const { return shell_ != nullptr; }
    bool ReadLocal(LocalSnapshot& out) const;
    std::vector<EntitySnapshot> Snapshot() const;

    void SetCoordinateTable(uintptr_t table);
    uintptr_t CoordinateTable() const { return coordinateTable_.load(); }
    bool EnsureCoordinateHook();
    void SetRenderFrame(const Matrix4& view, const Matrix4& projection, const Viewport& viewport);

    static bool WorldToScreen(const Vec3& world, const Matrix4& view,
                              const Matrix4& projection, const Viewport& viewport,
                              ScreenPoint& out);
    static bool SelectNearest(const std::vector<EntitySnapshot>& entities,
                              float cx, float cy, EntitySnapshot& out);

private:
    bool InstallCoordinateHook();
    void RemoveCoordinateHook();
    void CaptureCoordinatePointer(uintptr_t object, uintptr_t table);
    static void __cdecl CoordinateHookCallback(uintptr_t table, uintptr_t object);
    static bool ReadBytes(uintptr_t address, void* out, size_t size);
    static bool Read32(uintptr_t address, uint32_t& out);
    static bool Read8(uintptr_t address, uint8_t& out);
    static bool ReadFloat(uintptr_t address, float& out);
    uintptr_t PlayersRoot() const;
    uintptr_t EntityRoot() const;
    uint32_t Mode() const;
    uint32_t DisplayCount(uint32_t mode) const;
    uint32_t LocalSlot() const;
    bool ReadPosition(uint32_t slot, uint32_t part, Vec3& out) const;
    bool IsEnemy(uint32_t slot, uint32_t count, uint32_t localSlot,
                 uint8_t localZombie, uint8_t targetZombie) const;

    HMODULE shell_{};
    // ECX captured by the coordinate hook is a per-object pointer.  The
    // source stores it in 坐标指针[n] and dereferences it once to obtain the
    // shared table (数据指针).  Keep both values so diagnostics can prove the
    // pointer chain instead of treating ECX as the table itself.
    std::atomic<uintptr_t> coordinatePointer_{0};
    mutable std::atomic<uintptr_t> coordinateTable_{0};
    // TCII keeps one coordinate-pointer per player slot.  The shared data
    // pointer used by取敌人坐标_ing is specifically 坐标指针[1], so retaining
    // only the last callback would occasionally bind the table to another
    // player and return mismatched bones.  Keep all 16 pointers and publish
    // the slot-1 table as the active snapshot source.
    std::array<std::atomic<uintptr_t>, 21> coordinatePointers_{};
    Matrix4 view_{};
    Matrix4 projection_{};
    Viewport viewport_{};
    bool frameReady_{false};
    uintptr_t hookTarget_{};
    void* hookTrampoline_{};
    std::array<uint8_t, 6> hookOriginal_{};
    bool hookInstalled_{false};
    std::mutex hookMu_;
};

} // namespace d3dref9

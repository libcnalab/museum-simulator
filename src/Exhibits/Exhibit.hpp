#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Diagnostics/Log.hpp"
#include "Exhibits/Services.hpp"
#include "Interaction/ControlPanel.hpp"
#include "World/Catalogue.hpp"

struct Voice3D;


class RoomBuilder;
class InteractionSystem;
class MuseumWorld;

struct UpdateContext {
    float dt;                 // seconds, already clamped
    double time;              // museum clock in seconds
    Microsoft::Xna::Framework::Vector3 playerPosition;
    Microsoft::Xna::Framework::Vector3 playerForward;
    bool playerInRoom;        // the visitor is standing in this exhibit's room
    bool nearby;              // this room or a neighbour: worth simulating
};

/**
 * One room's exhibit. The base class carries no behaviour: each room overrides the
 * few hooks it needs. Exhibits own their dynamic objects (models, particles, render
 * targets); the static architecture they add goes through RoomBuilder.
 */
class Exhibit {
public:
    Exhibit(const RoomDef& room, Services& services) : room_(room), services_(services) {}
    virtual ~Exhibit();
    Exhibit(const Exhibit&) = delete;
    Exhibit& operator=(const Exhibit&) = delete;

    [[nodiscard]] const RoomDef& Room() const { return room_; }

    /** Adds pedestals, signs, colliders and lights. Called once while the world is built. */
    virtual void BuildStatic(RoomBuilder&) {}
    /** Furniture that is not part of the demonstration (benches, columns). Called after BuildStatic, before the control panels are stood up. */
    virtual void Dress(RoomBuilder&) {}
    /** Loads models, creates render targets and registers interactables. Called once after BuildStatic. */
    virtual void Load(InteractionSystem&, MuseumWorld&) {}
    virtual void Update(const UpdateContext&) {}
    /** Renders into render targets. Runs before the main scene, only when `nearby`. */
    virtual void RenderOffscreen(const DrawContext&) {}
    virtual void DrawOpaque(const DrawContext&) {}
    virtual void DrawTransparent(const DrawContext&) {}
    /** Called once per frame after the 3D scene, for overlays that need SpriteBatch. */
    virtual void DrawOverlay() {}
    virtual void OnEnter() {}
    virtual void OnLeave() {}

    // The museum calls these wrappers: they service the exhibit's control panels, then the virtual hooks.
    void DoBuildStatic(RoomBuilder& builder);
    void DoLoad(InteractionSystem& interaction, MuseumWorld& world);
    void DoRenderOffscreen(const DrawContext& ctx);
    void DoDrawOpaque(const DrawContext& ctx);

protected:
    /** Report an exception the exhibit caught but did not expect (see Museum::ReportExhibitError); the room carries on. */
    void ReportError(const std::string& what) const { Museum::ReportExhibitError(room_.key.c_str(), what.c_str()); }
    /** A looping positional voice that is released when the exhibit is destroyed. Null when audio is off. */
    Voice3D* CreateLoop(const std::string& sound, const Microsoft::Xna::Framework::Vector3& position, float volume, float radius);
    /** The wall the room's door is in, and the wall facing it (where wide screens go): walls of the room's local frame. */
    [[nodiscard]] Wall DoorWall() const { return room_.doors.empty() ? Wall::South : room_.LocalWall(room_.doors.front().wall); }
    [[nodiscard]] Wall FarWall() const { return Opposite(DoorWall()); }
    /** A local offset as a world offset: `ToWorld(0,0,0) + Off(x, y, z)` is `ToWorld(x, y, z)`. */
    [[nodiscard]] Microsoft::Xna::Framework::Vector3 Off(float x, float y, float z) const { return room_.Rotate(Microsoft::Xna::Framework::Vector3(x, y, z)); }
    [[nodiscard]] Microsoft::Xna::Framework::Vector3 Off(const Microsoft::Xna::Framework::Vector3& local) const { return room_.Rotate(local); }
    /** Centre and facing (yaw, 0 = +Z) of a screen hung on the far wall `height` metres up. */
    void FarScreenPose(float height, Microsoft::Xna::Framework::Vector3& centre, float& yaw) const;
    /** A point on the far wall: `along` metres to the right as seen from the door, `height` up; yaw faces into the room. */
    [[nodiscard]] Microsoft::Xna::Framework::Vector3 FarPoint(float along, float height, float& yaw) const;
    [[nodiscard]] Microsoft::Xna::Framework::Vector3 FarPoint0(float height) const { float y; return FarPoint(0.0f, height, y); }
    [[nodiscard]] float FarYaw() const { float y; (void)FarPoint(0.0f, 0.0f, y); return y; }
    /** Corner and edge vectors of an upright `w` x `h` quad on the far wall, its bottom edge `bottom` up, facing into the room. */
    void FarQuad(float along, float bottom, float w, float h, Microsoft::Xna::Framework::Vector3& origin, Microsoft::Xna::Framework::Vector3& u, Microsoft::Xna::Framework::Vector3& v) const;
    /** A control console standing in the room (local metres). Create it, and add its rows, in BuildStatic. */
    ControlPanel& AddPanel(const std::string& title, float x, float y, float z, float yaw, float width = 1.8f);
    [[nodiscard]] Services& S() { return services_; }
    [[nodiscard]] const Services& S() const { return services_; }
    [[nodiscard]] const RoomDef& R() const { return room_; }

private:
    const RoomDef& room_;
    Services& services_;
    std::vector<Voice3D*> voices_;
    std::vector<std::unique_ptr<ControlPanel>> panels_;
};

#pragma once

// Base for exhibits whose centrepiece is a live 2D screen on the far wall: a render target drawn with
// SpriteBatch (virtual DrawScreen), shown as a quad, with the usual control panel and information sign.
#include <memory>
#include <string>
#include <vector>

#include "Exhibits/Exhibit.hpp"
#include "Interaction/InteractionSystem.hpp"
#include "Exhibits/ExhibitKit.hpp"
#include "Rendering/LiveScreen.hpp"
#include "Rendering/QuadDraw.hpp"
#include "Rendering/RenderStates.hpp"
#include "Rendering/TextureLibrary.hpp"
#include "World/RoomBuilder.hpp"

class ScreenExhibit : public Exhibit {
public:
    ScreenExhibit(const RoomDef& room, Services& services, int pixelWidth = 960, int pixelHeight = 540, float widthMetres = 8.2f)
        : Exhibit(room, services), pixelWidth_(pixelWidth), pixelHeight_(pixelHeight), widthMetres_(widthMetres)
    {
    }

    void Load(InteractionSystem& interaction, MuseumWorld&) override
    {
        screen_ = std::make_unique<LiveScreen>(S(), pixelWidth_, pixelHeight_);
        // "Step up to the screen": most screens are readable only from a few metres, so pressing E on one glides the
        // visitor to the spot where it fills the view.
        Microsoft::Xna::Framework::Vector3 centre;
        float yaw;
        FarScreenPose(2.6f, centre, yaw);
        const Microsoft::Xna::Framework::Vector3 n(std::sin(yaw), 0, std::cos(yaw));
        const Microsoft::Xna::Framework::Vector3 right = Microsoft::Xna::Framework::Vector3::Normalize(Microsoft::Xna::Framework::Vector3::Cross(-n, Microsoft::Xna::Framework::Vector3::Up));
        const float halfW = widthMetres_ * 0.5f, halfH = widthMetres_ * static_cast<float>(pixelHeight_) / static_cast<float>(pixelWidth_) * 0.5f;
        Interactable ia;
        ia.bounds = Microsoft::Xna::Framework::BoundingBox(centre - right * halfW - Microsoft::Xna::Framework::Vector3(0, halfH, 0) - n * 0.3f, centre + right * halfW + Microsoft::Xna::Framework::Vector3(0, halfH, 0) + n * 0.3f);
        ia.bounds = Microsoft::Xna::Framework::BoundingBox(Microsoft::Xna::Framework::Vector3::Min(ia.bounds.Min, ia.bounds.Max), Microsoft::Xna::Framework::Vector3::Max(ia.bounds.Min, ia.bounds.Max));
        ia.maxDistance = 12.0f;
        const float viewDistance = widthMetres_ * 0.58f;
        const Microsoft::Xna::Framework::Vector3 eye(centre.X + n.X * viewDistance, R().centre.Y + 1.65f, centre.Z + n.Z * viewDistance);
        const float pitch = std::atan2(centre.Y - (R().centre.Y + 1.65f), viewDistance);
        ia.prompt = [] { return std::string("Step up to the screen"); };
        ia.activate = [this, eye, yaw, pitch] {
            if (S().focusView) S().focusView(eye, yaw, pitch);
        };
        interaction.Add(std::move(ia));
        OnScreenLoaded();
    }

    // A room built around a screen gets a pair of columns flanking it and a bench along the side wall facing the control panel.
    void Dress(RoomBuilder& b) override
    {
        using Microsoft::Xna::Framework::Vector3;
        const float hw = R().halfW, hd = R().halfD;
        float cx[2], cz[2];
        switch (FarWall()) {
            case Wall::North: for (int i = 0; i < 2; ++i) { cx[i] = (i == 0 ? -1.0f : 1.0f) * (hw - 0.9f); cz[i] = -hd + 0.9f; } break;
            case Wall::South: for (int i = 0; i < 2; ++i) { cx[i] = (i == 0 ? -1.0f : 1.0f) * (hw - 0.9f); cz[i] = hd - 0.9f; } break;
            case Wall::East: for (int i = 0; i < 2; ++i) { cx[i] = hw - 0.9f; cz[i] = (i == 0 ? -1.0f : 1.0f) * (hd - 0.9f); } break;
            default: for (int i = 0; i < 2; ++i) { cx[i] = -hw + 0.9f; cz[i] = (i == 0 ? -1.0f : 1.0f) * (hd - 0.9f); } break;
        }
        for (int i = 0; i < 2; ++i) b.AddColumn(cx[i], cz[i], 0.3f, "marble", Vector3(0.92f, 0.92f, 0.98f));
        // A bench against the east wall (the control panel is on the west wall), unless the screen is on that wall.
        if (FarWall() != Wall::East && DoorWall() != Wall::East) {
            b.AddBox(Vector3(hw - 1.0f, 0.0f, -1.7f), Vector3(hw - 0.45f, 0.1f, 1.7f), "stone", 1.0f, Vector3(0.7f, 0.7f, 0.74f));
            b.AddBox(Vector3(hw - 1.05f, 0.1f, -1.8f), Vector3(hw - 0.4f, 0.42f, 1.8f), "wood", 1.0f, Vector3(0.9f, 0.85f, 0.8f));
        }
    }

    void RenderOffscreen(const DrawContext&) override
    {
        if (!screen_) return;
        screen_->RenderCustom(Background(), [this](Microsoft::Xna::Framework::Graphics::SpriteBatch& sb, int, int) {
            try {
                DrawScreen(sb);
            } catch (...) {
                // A screen that threw between Begin and End must not leave the shared SpriteBatch begun: every later Begin
                // in the museum (signs, other screens) would fail. End it if it is begun, then report the original error.
                try { sb.End(); } catch (...) {}
                throw;
            }
        });
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
        if (!screen_ || !screen_->HasContent()) return;
        Microsoft::Xna::Framework::Vector3 centre;
        float yaw;
        FarScreenPose(2.6f, centre, yaw);
        using Microsoft::Xna::Framework::Vector3;
        const Vector3 n(std::sin(yaw), 0, std::cos(yaw));
        const Vector3 right = Vector3::Normalize(Vector3::Cross(-n, Vector3::Up)) * widthMetres_;
        const Vector3 up(0, widthMetres_ * static_cast<float>(pixelHeight_) / static_cast<float>(pixelWidth_), 0);
        RenderStates::SetOpaque3D(ctx.device);
        QuadDraw::Textured(ctx, screen_->Texture(), centre - right * 0.5f - up * 0.5f, right, up);
        DrawExtra(ctx);
    }

protected:
    /** Draws the screen's content; the SpriteBatch is not begun: call Begin/End as the demonstration needs. */
    virtual void DrawScreen(Microsoft::Xna::Framework::Graphics::SpriteBatch& sb) = 0;
    virtual void OnScreenLoaded() {}
    virtual void DrawExtra(const DrawContext&) {}
    [[nodiscard]] virtual Microsoft::Xna::Framework::Color Background() const { return Microsoft::Xna::Framework::Color(26, 32, 48, 255); }

    /** The standard information sign on the east wall. */
    void AddInfoSign(RoomBuilder& b, const std::string& title, const std::vector<std::string>& lines)
    {
        SignSpec what = Kit::Plate(title, lines);
        what.leftAlign = true;
        what.pixelWidth = 900;
        b.AddWallSign(Wall::East, 0.0f, 2.4f, 4.4f, 2.0f, what);
    }

    std::unique_ptr<LiveScreen> screen_;
    int pixelWidth_, pixelHeight_;
    float widthMetres_;
};

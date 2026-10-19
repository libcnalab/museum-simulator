#pragma once

#include <functional>
#include <memory>

#include "Exhibits/Services.hpp"
#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

/**
 * A small screen showing the scene from an arbitrary camera: a RenderTarget2D with a depth buffer,
 * drawn into with caller-supplied view/projection matrices, and a helper that shows it as a quad
 * in the room. The camera and projection rooms use it for their picture-in-picture views.
 */
class MiniView {
public:
    MiniView(Services& services, int width, int height);
    /** The same with a chosen depth buffer (None draws without depth testing) and an optional mip chain on the target. */
    MiniView(Services& services, int width, int height, Microsoft::Xna::Framework::Graphics::DepthFormat depth, bool mipMap);

    /**
     * Renders `draw` through the given camera into the target and restores the back buffer.
     * Must be called from RenderOffscreen (never between a SpriteBatch Begin/End).
     */
    void Render(const DrawContext& main, const Microsoft::Xna::Framework::Matrix& view, const Microsoft::Xna::Framework::Matrix& projection,
                const Microsoft::Xna::Framework::Vector3& eye, const std::function<void(const DrawContext&)>& draw,
                const Microsoft::Xna::Framework::Color& clear = Microsoft::Xna::Framework::Color(60, 90, 140, 255));

    [[nodiscard]] Microsoft::Xna::Framework::Graphics::Texture2D* Texture() { return target_.get(); }
    [[nodiscard]] Microsoft::Xna::Framework::Graphics::RenderTarget2D* Target() { return target_.get(); }
    [[nodiscard]] float Aspect() const { return static_cast<float>(width_) / static_cast<float>(height_); }
    [[nodiscard]] bool HasContent() const { return rendered_; }
    void MarkRendered() { rendered_ = true; }

    /** Draws the finished view as a quad facing `yaw` (0 faces +Z) centred at `centre` (world), `width` metres wide. */
    void DrawInRoom(const DrawContext& ctx, const Microsoft::Xna::Framework::Vector3& centre, float yaw, float width,
                    const Microsoft::Xna::Framework::Graphics::SamplerState* sampler = nullptr);

private:
    Services& s_;
    int width_, height_;
    bool rendered_ = false;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::RenderTarget2D> target_;
};

/**
 * A little diorama for the camera and projection rooms: a checkered ground and coloured solids
 * arranged at different depths, lit by the default three-light rig. `origin` is the centre of the ground.
 */
void DrawDiorama(const DrawContext& ctx, Services& services, const Microsoft::Xna::Framework::Vector3& origin, double time, float spread = 3.0f);

/** Positions of the diorama's solids (centre, radius), for tests like frustum containment. */
struct DioramaItem {
    Microsoft::Xna::Framework::Vector3 offset;
    float radius;
};
const DioramaItem* DioramaItems(int& count);

// GPU queries and anti-aliasing: OcclusionQuery (how many pixels of an object were visible?) and multisampled
// render targets (the same edges at 0, 2, 4 and 8 samples, magnified so the stair-steps can be counted).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "CNA/GraphicsCapability.hpp"
#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/OcclusionQuery.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColor.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/LitMesh.hpp"
#include "Rendering/MiniView.hpp"
#include "Rendering/RenderStates.hpp"
#include "Rendering/TextureLibrary.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

enum class Mode { Occlusion, Msaa };

class QueryLab final : public ScreenExhibit {
public:
    QueryLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 7.6f), mode_(room.params.Get("mode") == "msaa" ? Mode::Msaa : Mode::Occlusion) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel(Title(), -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        if (mode_ == Mode::Occlusion) {
            p.AddSlider("Wall position (slides across the sphere)", -3.0f, 3.0f, wallX_, [this](float v) { wallX_ = v; autoSlide_ = false; }, "%.2f");
            p.AddToggle("Slide the wall automatically", true, [this](bool v) { autoSlide_ = v; });
            p.AddSlider("Sphere distance", 2.0f, 8.0f, sphereZ_, [this](float v) { sphereZ_ = v; }, "%.1f");
            p.AddReadout("Visible pixels (PixelCount)", [this] { return supported_ ? std::to_string(pixels_) : std::string("not supported here"); });
            p.AddReadout("IsComplete", [this] { return std::string(complete_ ? "yes" : "not yet"); });
            p.AddReadout("Counts are exact", [this] { return std::string(precise_ ? "yes" : "no: visible or not only"); });
            p.AddReadout("Sphere drawn", [this] { return std::string(skipped_ ? "skipped last frame" : "drawn"); });
        } else {
            p.AddToggle("Rotate the shapes", true, [this](bool v) { rotate_ = v; });
            p.AddToggle("RasterizerState.MultiSampleAntiAlias", true, [this](bool v) { msaaRaster_ = v; });
            p.AddChoice("BlendState.MultiSampleMask", {"every sample (0xFFFFFFFF)", "sample 0 only (0x1)", "samples 0 and 2 (0x5)", "no sample (0)"}, 0, [this](int v) { sampleMask_ = v; });
            p.AddSlider("Zoom into the edges", 2.0f, 12.0f, zoom_, [this](float v) { zoom_ = v; }, "%.0f", 1.0f);
            p.AddReadout("MultiSampleAntiAliasing", [this] { return std::string(msaa_ ? "supported" : "NOT supported by this renderer"); });
            for (int i = 0; i < 4; ++i) p.AddReadout(std::string("Samples asked: ") + std::to_string(kCounts[i]), [this, i] { return targets_[static_cast<std::size_t>(i)] ? "got " + std::to_string(targets_[static_cast<std::size_t>(i)]->getMultiSampleCountProperty()) : std::string("-"); });
        }
        AddInfoSign(b, Title(), Explanation());
    }

    void OnScreenLoaded() override
    {
        GraphicsDevice& d = *S().device;
        view_ = std::make_unique<MiniView>(S(), 960, 540);
        fx_ = std::make_unique<BasicEffect>(d);
        fx_->EnableDefaultLighting();
        fx_->setTextureEnabledProperty(true);
        fx_->setTextureProperty(S().textures->White());
        flat_ = std::make_unique<BasicEffect>(d);
        flat_->setVertexColorEnabledProperty(true);
        flat_->setLightingEnabledProperty(false);
        flat_->setTextureEnabledProperty(false);
        if (mode_ == Mode::Occlusion) {
            supported_ = d.SupportsCapability(CNA::GraphicsCapability::OcclusionQuery);
            if (supported_) {
                // The capability says yes, so a refusal here contradicts it: report it, and show "not supported" as before.
                try { query_ = std::make_unique<OcclusionQuery>(d); } catch (const std::exception& e) { supported_ = false; Museum::Log("query", "OcclusionQuery: %s", e.what()); ReportError(std::string("OcclusionQuery despite the capability: ") + e.what()); }
            }
        } else {
            msaa_ = d.SupportsCapability(CNA::GraphicsCapability::MultiSampleAntiAliasing);
            for (int i = 0; i < 4; ++i) {
                try {
                    targets_[static_cast<std::size_t>(i)] = std::make_unique<RenderTarget2D>(d, 240, 135, false, SurfaceFormat::Color, DepthFormat::Depth24, kCounts[i], RenderTargetUsage::DiscardContents);
                    Museum::Log("query", "render target asked for %d samples, got %d", kCounts[i], targets_[static_cast<std::size_t>(i)]->getMultiSampleCountProperty());
                } catch (const std::exception& e) {
                    Museum::Log("query", "render target with %d samples: %s", kCounts[i], e.what());
                    // The sample count is a preference (XNA clamps it to what the device has), so the constructor should not refuse it.
                    ReportError("RenderTarget2D with " + std::to_string(kCounts[i]) + " samples: " + e.what());
                }
            }
        }
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
        time_ += ctx.dt;
        if (mode_ == Mode::Occlusion && autoSlide_) wallX_ = std::sin(time_ * 0.6f) * 2.6f;
    }

    void RenderOffscreen(const DrawContext& ctx) override
    {
        if (ctx.offscreen) {
            ScreenExhibit::RenderOffscreen(ctx);
            return;
        }
        if (mode_ == Mode::Occlusion) RenderOcclusion(ctx);
        else RenderMsaa(ctx);
        ScreenExhibit::RenderOffscreen(ctx);
    }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        Texture2D& white = *S().whiteTexture;
        sb.Begin(SpriteSortMode::Deferred, &BlendState::Opaque, &SamplerState::PointClamp, nullptr, nullptr);
        if (mode_ == Mode::Occlusion) {
            if (view_ && view_->HasContent()) sb.Draw(*view_->Texture(), Rectangle(0, 0, 960, 540), Color::White);
        } else {
            for (int i = 0; i < 4; ++i) {
                if (!targets_[static_cast<std::size_t>(i)]) continue;
                const int x = 12 + i * 234;
                sb.Draw(*targets_[static_cast<std::size_t>(i)], Rectangle(x, 54, 222, 125), Color::White);
                // The same few pixels from each target, magnified with point sampling: edge pixels become blocks.
                const int zw = std::max(8, static_cast<int>(222.0f / zoom_)), zh = std::max(8, static_cast<int>(125.0f / zoom_));
                sb.Draw(*targets_[static_cast<std::size_t>(i)], Rectangle(x, 200, 222, 300), Rectangle(112 - zw / 2, 50 - zh / 2, zw, zh), Color::White);
            }
        }
        sb.End();
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        const auto text = [&](const std::string& t, float x, float y, Color c, float s = 0.7f) { sb.DrawString(*S().font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), s, SpriteEffects::None, 0.0f); };
        sb.Draw(white, Rectangle(0, 0, 960, mode_ == Mode::Occlusion ? 40 : 46), Color(0, 0, 0, 170));
        text(Title(), 14, 6, Kit::kGold, 0.9f);
        if (mode_ == Mode::Occlusion) {
            text(supported_ ? "visible pixels: " + std::to_string(pixels_) + (precise_ ? "" : "  (this renderer reports only 0 or more than 0)") : "OcclusionQuery is not supported by this renderer", 380, 10, Kit::kText, 0.7f);
        } else {
            for (int i = 0; i < 4; ++i) text(kCounts[i] == 0 ? "no multisampling" : std::to_string(kCounts[i]) + " samples", 20.0f + static_cast<float>(i) * 234.0f, 60, Color(255, 255, 255, 255), 0.6f);
            if (!msaa_) text("This renderer does not support multisampling: all four targets look the same.", 14, 506, Kit::Rgba(255, 190, 150), 0.7f);
        }
        sb.End();
    }

private:
    static constexpr int kCounts[4] = {0, 2, 4, 8};

    [[nodiscard]] std::string Title() const { return mode_ == Mode::Occlusion ? "OcclusionQuery" : "Multisampled render targets"; }
    [[nodiscard]] std::vector<std::string> Explanation() const
    {
        if (mode_ == Mode::Occlusion) return {"An OcclusionQuery counts the pixels that pass the depth test between Begin and End. A game can draw a cheap", "stand-in first and skip the expensive object when the count is 0. The result arrives a few frames later (IsComplete)."};
        return {"RenderTarget2D takes a multisample count. Each pixel is shaded once but covered by several samples, so slanted edges", "get intermediate shades. Below, the same scene at 0, 2, 4 and 8 samples, and a magnified piece of each."};
    }

    void RenderOcclusion(const DrawContext& ctx)
    {
        if (!view_) return;
        const Vector3 c = R().centre;
        const Vector3 eye = c + Off(0, 1.4f, 7.5f);
        const Matrix view = Matrix::CreateLookAt(eye, c + Vector3(0, 1.2f, 0), Vector3::Up);
        const Matrix proj = Matrix::CreatePerspectiveFieldOfView(MathHelper::ToRadians(50.0f), view_->Aspect(), 0.1f, 60.0f);
        view_->Render(ctx, view, proj, eye, [this, &ctx, c](const DrawContext& sub) { DrawOcclusionScene(sub, c); }, Color(40, 50, 70, 255));
        (void)ctx;
    }
    void DrawOcclusionScene(const DrawContext& ctx, const Vector3& c)
    {
        GraphicsDevice& d = ctx.device;
        RenderStates::SetOpaque3D(d);
        d.setRasterizerStateProperty(RasterizerState::CullClockwise);
        fx_->setViewProperty(ctx.view.view);
        fx_->setProjectionProperty(ctx.view.projection);
        // The floor.
        fx_->setDiffuseColorProperty(Vector3(0.4f, 0.45f, 0.55f));
        fx_->setWorldProperty(Matrix::CreateScale(14.0f, 1.0f, 14.0f) * Matrix::CreateTranslation(c));
        EffectBank::Apply(*fx_);
        S().shapes->Plane().Draw(d);
        // The occluder: a tall wall between the camera and the sphere, drawn first so it fills the depth buffer.
        fx_->setDiffuseColorProperty(Vector3(0.85f, 0.45f, 0.3f));
        fx_->setWorldProperty(Matrix::CreateScale(1.8f, 2.8f, 0.2f) * Matrix::CreateTranslation(c + Off(wallX_, 1.4f, sphereZ_ - 3.2f)));
        EffectBank::Apply(*fx_);
        S().shapes->Cube().Draw(d);
        // The tested object.
        fx_->setDiffuseColorProperty(Vector3(0.35f, 0.7f, 0.95f));
        fx_->setWorldProperty(Matrix::CreateScale(1.6f) * Matrix::CreateTranslation(c + Off(0, 1.0f, sphereZ_ - 8.0f + 3.0f)));
        EffectBank::Apply(*fx_);
        if (supported_ && query_) {
            // Collect the previous result before reusing the query.
            if (inQuery_ && query_->getIsCompleteProperty()) {
                const int before = pixels_;
                pixels_ = query_->getPixelCountProperty();
                if ((before == 0) != (pixels_ == 0)) Museum::Log("query", "occlusion: %d visible pixels (precise: %s)", pixels_, query_->isPixelCountPreciseEXT() ? "yes" : "no");
                complete_ = true;
                precise_ = query_->isPixelCountPreciseEXT();
                inQuery_ = false;
            } else if (inQuery_) {
                complete_ = false;
            }
            if (!inQuery_) {
                query_->Begin();
                S().shapes->Sphere().Draw(d);
                query_->End();
                inQuery_ = true;
                skipped_ = false;
            } else {
                S().shapes->Sphere().Draw(d);   // the result is not back yet: keep drawing so the picture stays honest
            }
        } else {
            S().shapes->Sphere().Draw(d);
        }
    }

    void RenderMsaa(const DrawContext& ctx)
    {
        GraphicsDevice& d = ctx.device;
        const float a = rotate_ ? time_ * 0.4f : 0.3f;
        for (int i = 0; i < 4; ++i) {
            RenderTarget2D* t = targets_[static_cast<std::size_t>(i)].get();
            if (t == nullptr) continue;
            d.SetRenderTarget(t);
            d.Clear(ClearOptions::Target | ClearOptions::DepthBuffer, Color(24, 28, 44, 255), 1.0f, 0);
            RenderStates::SetOpaque3D(d);
            RasterizerState raster = RasterizerState::CullNone;
            raster.setMultiSampleAntiAliasProperty(msaaRaster_);   // off: the multisampled target is rasterised as if it had one sample
            d.setRasterizerStateProperty(raster);
            // Which samples of a multisampled target a draw may write: bit k of the mask is sample k.
            BlendState sampleBlend = BlendState::Opaque;
            static const int masks[] = {-1, 0x1, 0x5, 0x0};
            sampleBlend.setMultiSampleMaskProperty(masks[sampleMask_]);
            d.setBlendStateProperty(sampleBlend);
            flat_->setWorldProperty(Matrix::getIdentityProperty());
            flat_->setViewProperty(Matrix::getIdentityProperty());
            flat_->setProjectionProperty(Matrix::getIdentityProperty());
            std::vector<VertexPositionColor> v;
            const auto tri = [&](float cx, float cy, float r, float rot, Color col) {
                for (int k = 0; k < 3; ++k) {
                    const float ang = rot + static_cast<float>(k) * MathHelper::TwoPi / 3.0f;
                    v.emplace_back(Vector3(cx + std::cos(ang) * r * 0.5625f, cy + std::sin(ang) * r, 0.5f), col);
                }
            };
            tri(-0.45f, 0.0f, 0.7f, a, Color(255, 200, 80, 255));
            tri(0.45f, 0.0f, 0.5f, -a * 1.3f, Color(120, 220, 255, 255));
            // A fan of very thin slivers: the worst case for aliasing.
            for (int k = 0; k < 7; ++k) {
                const float ang = 0.2f + static_cast<float>(k) * 0.14f;
                v.emplace_back(Vector3(-0.95f, -0.9f, 0.5f), Color(255, 255, 255, 255));
                v.emplace_back(Vector3(-0.95f + std::cos(ang) * 1.9f, -0.9f + std::sin(ang) * 1.9f, 0.5f), Color(255, 255, 255, 255));
                v.emplace_back(Vector3(-0.95f + std::cos(ang + 0.012f) * 1.9f, -0.9f + std::sin(ang + 0.012f) * 1.9f, 0.5f), Color(255, 255, 255, 255));
            }
            EffectBank::Apply(*flat_);
            d.DrawUserPrimitives(PrimitiveType::TriangleList, v.data(), 0, static_cast<int>(v.size()) / 3);
        }
        d.SetRenderTarget(nullptr);
    }

    Mode mode_;
    std::unique_ptr<MiniView> view_;
    std::unique_ptr<BasicEffect> fx_, flat_;
    std::unique_ptr<OcclusionQuery> query_;
    std::unique_ptr<RenderTarget2D> targets_[4];
    float time_ = 0.0f, wallX_ = -2.0f, sphereZ_ = 5.0f, zoom_ = 5.0f;
    bool autoSlide_ = true, supported_ = true, inQuery_ = false, complete_ = false, precise_ = false, skipped_ = false, rotate_ = true, msaa_ = true, msaaRaster_ = true;
    int sampleMask_ = 0;
    int pixels_ = 0;
};

}  // namespace

std::unique_ptr<Exhibit> MakeQueryLab(const RoomDef& room, Services& services)
{
    return std::make_unique<QueryLab>(room, services);
}

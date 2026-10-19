// Render-state laboratory: RasterizerState, DepthStencilState and BlendState, one demonstration per room.
// Every verdict is visual: the same geometry is drawn under different states and the difference is on screen.
#include <algorithm>
#include <cmath>
#include <string>

#include "CNA/GraphicsCapability.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ExhibitKit.hpp"
#include "Exhibits/Math/MathKit.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/ClearOptions.hpp"
#include "Microsoft/Xna/Framework/Graphics/ColorWriteChannels.hpp"
#include "Microsoft/Xna/Framework/Graphics/CullMode.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/FillMode.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/StencilOperation.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColorTexture.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/LitMesh.hpp"
#include "Rendering/MiniView.hpp"
#include "Rendering/RenderStates.hpp"
#include "Rendering/TextureLibrary.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace MathKit;

namespace {

enum class Mode { Cull, Fill, DepthBias, Scissor, DepthFunc, DepthRead, StencilPortal, StencilOutline, StencilShadow, StencilCount, BlendGallery, ColorWrite, BlendFactor };

Mode ParseMode(const std::string& s)
{
    static const std::pair<const char*, Mode> table[] = {{"fill", Mode::Fill}, {"depthbias", Mode::DepthBias}, {"scissor", Mode::Scissor}, {"depthfunc", Mode::DepthFunc}, {"depthread", Mode::DepthRead},
                                                         {"portal", Mode::StencilPortal}, {"outline", Mode::StencilOutline}, {"shadow", Mode::StencilShadow}, {"stencilcount", Mode::StencilCount}, {"blendgallery", Mode::BlendGallery},
                                                         {"colorwrite", Mode::ColorWrite}, {"blendfactor", Mode::BlendFactor}};
    for (const auto& [name, mode] : table)
        if (s == name) return mode;
    return Mode::Cull;
}

class RenderStateLab final : public Exhibit {
public:
    RenderStateLab(const RoomDef& room, Services& services) : Exhibit(room, services), mode_(ParseMode(room.params.Get("mode"))), gizmos_(services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        BuildStagePlinth(b, 0.0f, 0.0f, 0.4f);
        ControlPanel& p = AddPanel(Title(), -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        switch (mode_) {
            case Mode::Cull:
                p.AddChoice("CullMode", {"CullNone", "CullClockwiseFace", "CullCounterClockwiseFace"}, 1, [this](int v) { cull_ = v; });
                p.AddSlider("Cube size", 0.8f, 6.0f, size_, [this](float v) { size_ = v; });
                p.AddHeading("Walk into the big cube to look at its inside");
                break;
            case Mode::Fill:
                p.AddChoice("FillMode", {"Solid", "WireFrame"}, 1, [this](int v) { wire_ = v == 1; });
                p.AddReadout("WireFrame capability", [this] { return std::string(S().device->SupportsCapability(CNA::GraphicsCapability::WireFrame) ? "yes: native polygon mode" : "NO on this renderer: Solid only"); });
                break;
            case Mode::DepthBias:
                p.AddSlider("DepthBias (x 1e-4)", -10.0f, 10.0f, bias_, [this](float v) { bias_ = v; }, "%.1f");
                p.AddSlider("SlopeScaleDepthBias", -4.0f, 4.0f, slope_, [this](float v) { slope_ = v; }, "%.1f");
                p.AddReadout("Renderer", [this] { return std::string(S().device->GetGraphicsRendererName()); });
                p.AddHeading("Two coplanar quads: bias decides which wins");
                break;
            case Mode::Scissor:
                p.AddToggle("ScissorTestEnable", true, [this](bool v) { scissor_ = v; });
                p.AddSlider("Scissor x", 0.0f, 400.0f, sx_, [this](float v) { sx_ = v; }, "%.0f", 1.0f);
                p.AddSlider("Scissor width", 60.0f, 560.0f, sw_, [this](float v) { sw_ = v; }, "%.0f", 1.0f);
                break;
            case Mode::DepthFunc:
                p.AddChoice("DepthBufferFunction", {"Less", "LessEqual", "Greater", "GreaterEqual", "Equal", "NotEqual", "Always", "Never"}, 0, [this](int v) { fn_ = v; });
                p.AddToggle("DepthBufferEnable", true, [this](bool v) { depthOn_ = v; });
                p.AddToggle("DepthBufferWriteEnable", true, [this](bool v) { depthWrite_ = v; });
                p.AddToggle("Draw the near cube first", false, [this](bool v) { nearFirst_ = v; });
                break;
            case Mode::DepthRead:
                p.AddToggle("DepthRead (no depth writes) for glass", true, [this](bool v) { depthWrite_ = !v; });
                p.AddReadout("DepthRead.DepthBufferWriteEnable", [] { return std::string(DepthStencilState::DepthRead.getDepthBufferWriteEnableProperty() ? "true" : "false"); });
                p.AddToggle("Sort the panes back to front", false, [this](bool v) { sorted_ = v; });
                break;
            case Mode::StencilPortal:
                p.AddToggle("StencilEnable", true, [this](bool v) { stencil_ = v; });
                p.AddSlider("Window size", 0.6f, 2.4f, size_, [this](float v) { size_ = v; });
                p.AddHeading("A window into another world, cut with the stencil buffer");
                break;
            case Mode::StencilOutline:
                p.AddToggle("StencilEnable (outline pass)", true, [this](bool v) { stencil_ = v; });
                p.AddSlider("Outline thickness", 1.02f, 1.3f, size_, [this](float v) { size_ = v; });
                break;
            case Mode::StencilShadow:
                p.AddToggle("Use stencil to avoid double blend", true, [this](bool v) { stencil_ = v; });
                p.AddSlider("Light height", 2.0f, 6.0f, size_, [this](float v) { size_ = v; });
                break;
            case Mode::StencilCount:
                p.AddChoice("Scene", {"Four overlapping discs", "Closed and open meshes (two-sided)"}, 0, [this](int v) { countScene_ = v; });
                p.AddChoice("Which operation runs", {"StencilPass (test passes)", "StencilFail (StencilFunction Never)", "StencilDepthBufferFail (depth test fails)"}, 0, [this](int v) { countFail_ = v; });
                p.AddChoice("StencilPass operation", {"Increment", "IncrementSaturation", "Decrement", "DecrementSaturation", "Invert", "Replace (reference 5)"}, 0, [this](int v) { countOp_ = v; });
                p.AddSlider("StencilWriteMask", 1.0f, 255.0f, static_cast<float>(writeMask_), [this](float v) { writeMask_ = static_cast<int>(v); }, "%.0f", 1.0f);
                p.AddSlider("Value the stencil is cleared to", 0.0f, 250.0f, static_cast<float>(clearValue_), [this](float v) { clearValue_ = static_cast<int>(v); }, "%.0f", 1.0f);
                p.AddReadout("What is shown", [this] { return countScene_ == 0 ? std::string("stencil = clear value + discs covering the pixel") : std::string("clockwise faces +1, counter-clockwise -1: open meshes leave a mark"); });
                break;
            case Mode::BlendGallery:
                p.AddSlider("Source opacity", 0.1f, 1.0f, alpha_, [this](float v) { alpha_ = v; });
                p.AddHeading("Sixteen BlendState configurations");
                break;
            case Mode::ColorWrite:
                p.AddToggle("Write Red", true, [this](bool v) { mask_[0] = v; });
                p.AddToggle("Write Green", true, [this](bool v) { mask_[1] = v; });
                p.AddToggle("Write Blue", true, [this](bool v) { mask_[2] = v; });
                p.AddToggle("Write Alpha", true, [this](bool v) { mask_[3] = v; });
                break;
            case Mode::BlendFactor:
                p.AddSlider("BlendFactor red", 0.0f, 1.0f, factor_[0], [this](float v) { factor_[0] = v; });
                p.AddSlider("BlendFactor green", 0.0f, 1.0f, factor_[1], [this](float v) { factor_[1] = v; });
                p.AddSlider("BlendFactor blue", 0.0f, 1.0f, factor_[2], [this](float v) { factor_[2] = v; });
                p.AddHeading("Source * BlendFactor + Dest * (1 - BlendFactor)");
                break;
        }
        SignSpec what = Kit::Plate(Title(), Explanation());
        what.leftAlign = true;
        what.pixelWidth = 900;
        b.AddWallSign(Wall::East, 0.0f, 2.4f, 4.4f, 2.0f, what);
    }

    void Load(InteractionSystem&, MuseumWorld&) override
    {
        fx_ = std::make_unique<BasicEffect>(*S().device);
        if (mode_ == Mode::Scissor) view_ = std::make_unique<MiniView>(S(), 640, 360);
    }

    void Update(const UpdateContext& ctx) override
    {
        if (ctx.nearby) time_ += ctx.dt;
    }

    void RenderOffscreen(const DrawContext& ctx) override
    {
        if (mode_ != Mode::Scissor || !view_) return;
        const Vector3 eye = R().ToWorld(0.0f, 3.0f, 5.5f);
        const Vector3 centre = R().ToWorld(0.0f, 0.5f, 0.0f);
        GraphicsDevice& d = ctx.device;
        view_->Render(ctx, Matrix::CreateLookAt(eye, centre, Vector3::Up), Matrix::CreatePerspectiveFieldOfView(MathHelper::ToRadians(55.0f), view_->Aspect(), 0.1f, 60.0f), eye,
                      [this, &d](const DrawContext& sub) {
                          static RasterizerState sc = [] { RasterizerState r = RasterizerState::CullClockwise; r.setScissorTestEnableProperty(true); return r; }();
                          d.setScissorRectangleProperty(scissor_ ? Rectangle(static_cast<int>(sx_), 60, std::min(static_cast<int>(sw_), 640 - static_cast<int>(sx_)), 240) : Rectangle(0, 0, 640, 360));
                          d.setRasterizerStateProperty(scissor_ ? sc : RasterizerState::CullClockwise);
                          DrawDiorama(sub, S(), R().ToWorld(0, 0.02f, 0), time_, 3.0f);
                          d.setScissorRectangleProperty(Rectangle(0, 0, 640, 360));
                      });
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
        if (!fx_) return;
        const Vector3 o = R().ToWorld(0.0f, 1.6f, 0.0f);
        switch (mode_) {
            case Mode::Cull: DrawCull(ctx, o); break;
            case Mode::Fill: DrawFill(ctx, o); break;
            case Mode::DepthBias: DrawBias(ctx); break;
            case Mode::Scissor: if (view_) view_->DrawInRoom(ctx, FarPoint0(2.3f), FarYaw(), 6.6f); break;
            case Mode::DepthFunc: DrawDepthFunc(ctx, o); break;
            case Mode::DepthRead: DrawDepthRead(ctx, o); break;
            case Mode::StencilPortal: DrawPortal(ctx, o); break;
            case Mode::StencilOutline: DrawOutline(ctx, o); break;
            case Mode::StencilShadow: DrawShadow(ctx); break;
            case Mode::StencilCount: DrawStencilCount(ctx, o); break;
            case Mode::BlendGallery: DrawBlendGallery(ctx, o); break;
            case Mode::ColorWrite: DrawColorWrite(ctx, o); break;
            case Mode::BlendFactor: DrawBlendFactor(ctx, o); break;
        }
        RenderStates::SetOpaque3D(ctx.device);
        gizmos_.Flush(ctx);
    }
    void DrawOverlay() override { gizmos_.DrawLabels(); }

private:
    [[nodiscard]] std::string Title() const
    {
        static const char* t[] = {"RasterizerState.CullMode", "RasterizerState.FillMode", "Depth bias and z-fighting", "Scissor rectangle", "DepthBufferFunction", "Depth read-only for glass", "Stencil: a window into another world", "Stencil: object outline", "Stencil: planar shadow", "Stencil: counting and masks", "BlendState gallery", "ColorWriteChannels", "BlendFactor"};
        return t[static_cast<int>(mode_)];
    }
    [[nodiscard]] std::vector<std::string> Explanation() const
    {
        switch (mode_) {
            case Mode::Cull: return {"CullMode removes triangles that face away (or towards) the camera: CullNone draws both sides, the", "other two remove clockwise or counter-clockwise triangles. Choose the opposite of the front side and the", "cube turns inside out; make it big enough to stand inside."};
            case Mode::Fill: return {"FillMode.WireFrame draws triangle edges only: handy for seeing tessellation. Only renderers with a polygon mode", "can do it: query GraphicsCapability.WireFrame first, because asking for WireFrame without it throws."};
            case Mode::DepthBias: return {"Two surfaces at the same depth fight for the pixel (z-fighting). DepthBias and SlopeScaleDepthBias push", "one slightly nearer so it wins cleanly: how decals sit on walls. CNA documents that the Vulkan renderer", "applies DepthBias unscaled, so realistic values may show no effect there."};
            case Mode::Scissor: return {"The scissor rectangle clips everything, not just sprites, when RasterizerState.ScissorTestEnable is on.", "Here a 3D scene is drawn into a render target with the scissor test narrowing the visible band."};
            case Mode::DepthFunc: return {"The depth test compares each new pixel's depth with the stored one. Less is the usual choice; Greater shows", "what is behind, Always ignores depth (painter's algorithm), Never draws nothing. Try the draw order too."};
            case Mode::DepthRead: return {"Translucent panes must not write depth, or a pane drawn first hides the ones behind it. DepthRead (test,", "no write) fixes that, ideally with the panes also sorted back to front."};
            case Mode::StencilPortal: return {"The stencil buffer is a per-pixel integer mask. First a window quad writes 1 into it (colour writes off);", "then a second world is drawn only where the stencil equals 1: a portal. Needs a depth-stencil back buffer."};
            case Mode::StencilOutline: return {"Draw the object writing 1 to the stencil; draw it again, slightly larger and flat coloured, only where the", "stencil is NOT 1: what remains is the outline. Switch the stencil off and the larger copy hides the object."};
            case Mode::StencilCount: return {"Each stencil operation changes a pixel's value as geometry covers it. Increment counts how many discs cover a pixel; the write", "mask limits which bits change; TwoSidedStencilMode counts clockwise faces up and counter-clockwise down, so closed meshes cancel out."};
            case Mode::StencilShadow: return {"A planar shadow is the object flattened onto the floor with Matrix.CreateShadow. Drawn with blending, any", "overlap gets darker twice; a stencil test that lets each floor pixel be shaded once avoids the double dark."};
            case Mode::BlendGallery: return {"Sixteen BlendStates over the same background: presets, premultiplied and straight alpha, additive,", "multiply, screen, subtract, min, max and custom source/destination factors. Each cell is labelled."};
            case Mode::ColorWrite: return {"ColorWriteChannels masks which colour channels a draw may change. Three overlapping quads are drawn", "with the mask you choose: remove red and every red contribution disappears while the others stay."};
            default: return {"With Blend.BlendFactor the blend mixes by a constant colour you set on the BlendState, independent of the", "texture's alpha: result = source * factor + destination * (1 - factor), per channel."};
        }
    }

    // helpers --------------------------------------------------------------------------------------
    BasicEffect& Lit(const DrawContext& ctx, const Matrix& world, const Vector3& colour)
    {
        BasicEffect& fx = *fx_;
        fx.setLightingEnabledProperty(true);
        fx.EnableDefaultLighting();
        fx.setTextureEnabledProperty(true);
        fx.setTextureProperty(S().textures->White());
        fx.setVertexColorEnabledProperty(false);
        fx.setPreferPerPixelLightingProperty(false);
        fx.setDiffuseColorProperty(colour);
        fx.setEmissiveColorProperty(Vector3(0, 0, 0));
        fx.setAlphaProperty(1.0f);
        fx.setFogEnabledProperty(false);
        fx.setPunctualLightEXT(PunctualLightEXT{});
        fx.setWorldProperty(world);
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);
        return fx;
    }
    void Flat(const DrawContext& ctx, const Matrix& world, const Color& c, const Vector3& a, const Vector3& u, const Vector3& v)
    {
        BasicEffect& fx = ctx.fx.Flat();
        fx.setWorldProperty(world);
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);
        EffectBank::Apply(fx);
        const VertexPositionColor q[6] = {{a, c}, {a + u, c}, {a + u + v, c}, {a, c}, {a + u + v, c}, {a + v, c}};
        ctx.device.DrawUserPrimitives(PrimitiveType::TriangleList, q, 0, 2);
    }
    void Solid(const DrawContext& ctx, const LitMesh& mesh, const Matrix& world, const Vector3& colour)
    {
        BasicEffect& fx = Lit(ctx, world, colour);
        EffectBank::Apply(fx);
        mesh.Draw(ctx.device);
    }

    void DrawCull(const DrawContext& ctx, const Vector3& o)
    {
        static const CullMode modes[3] = {CullMode::None, CullMode::CullClockwiseFace, CullMode::CullCounterClockwiseFace};
        RasterizerState r = RasterizerState::CullClockwise;
        r.setCullModeProperty(modes[cull_]);
        ctx.device.setRasterizerStateProperty(r);
        Solid(ctx, S().shapes->Cube(), Matrix::CreateScale(size_) * Matrix::CreateRotationY(time_ * 0.3f) * Matrix::CreateTranslation(o + Vector3(0, size_ > 2.0f ? 0.0f : 0.0f, 0)), Vector3(0.9f, 0.7f, 0.4f));
        gizmos_.Label(o + Vector3(0, size_ * 0.5f + 0.4f, 0), cull_ == 0 ? "both sides drawn" : (cull_ == 1 ? "outside visible (normal)" : "inside visible (inverted)"), Color(255, 230, 150, 255), 0.55f);
    }

    void DrawFill(const DrawContext& ctx, const Vector3& o)
    {
        RasterizerState r = RasterizerState::CullNone;
        const bool canWire = S().device->SupportsCapability(CNA::GraphicsCapability::WireFrame);   // the draw throws otherwise
        r.setFillModeProperty(wire_ && canWire ? FillMode::WireFrame : FillMode::Solid);
        if (wire_ && !canWire) gizmos_.Label(o + Vector3(0, 1.8f, 0), "WireFrame is not available on this renderer: drawing Solid", Color(255, 170, 140, 255), 0.55f);
        ctx.device.setRasterizerStateProperty(r);
        Solid(ctx, S().shapes->Sphere(), Matrix::CreateScale(2.0f) * Matrix::CreateRotationY(time_ * 0.4f) * Matrix::CreateTranslation(o + Off(-1.4f, 0, 0)), Vector3(0.5f, 0.8f, 1.0f));
        Solid(ctx, S().shapes->Cone(), Matrix::CreateScale(1.6f, 2.0f, 1.6f) * Matrix::CreateRotationY(time_ * 0.4f) * Matrix::CreateTranslation(o + Off(1.6f, -1.0f, 0)), Vector3(1.0f, 0.7f, 0.4f));
    }

    void DrawBias(const DrawContext& ctx)
    {
        // A grey floor quad and a coloured decal exactly on top of it, seen at a grazing angle from far away.
        GraphicsDevice& d = ctx.device;
        const Vector3 centre = R().ToWorld(0.0f, 0.05f, 0.0f);
        RasterizerState base = RasterizerState::CullNone;
        d.setRasterizerStateProperty(base);
        Flat(ctx, Matrix::CreateTranslation(centre), Color(170, 170, 180, 255), Vector3(-5, 0, -5), Vector3(10, 0, 0), Vector3(0, 0, 10));
        RasterizerState decal = RasterizerState::CullNone;
        decal.setDepthBiasProperty(bias_ * 1e-4f);
        decal.setSlopeScaleDepthBiasProperty(slope_);
        d.setRasterizerStateProperty(decal);
        for (int i = 0; i < 4; ++i)
            Flat(ctx, Matrix::CreateTranslation(centre), (i % 2) ? Color(240, 80, 70, 255) : Color(250, 210, 70, 255), Vector3(-4.5f + static_cast<float>(i) * 2.3f, 0, -3.0f), Vector3(2.1f, 0, 0), Vector3(0, 0, 6.0f));
        d.setRasterizerStateProperty(RasterizerState::CullClockwise);
    }

    void DrawDepthFunc(const DrawContext& ctx, const Vector3& o)
    {
        GraphicsDevice& d = ctx.device;
        static const CompareFunction fns[] = {CompareFunction::Less, CompareFunction::LessEqual, CompareFunction::Greater, CompareFunction::GreaterEqual, CompareFunction::Equal, CompareFunction::NotEqual, CompareFunction::Always, CompareFunction::Never};
        DepthStencilState s = DepthStencilState::Default;
        s.setDepthBufferEnableProperty(depthOn_);
        s.setDepthBufferWriteEnableProperty(depthWrite_);
        s.setDepthBufferFunctionProperty(fns[fn_]);
        d.setDepthStencilStateProperty(s);
        d.setRasterizerStateProperty(RasterizerState::CullClockwise);
        const Vector3 a = o + Off(-0.5f, 0, 0.8f), b = o + Off(0.5f, 0, -0.8f);   // a is nearer to a visitor in the south
        for (int i = 0; i < 2; ++i) {
            const bool nearCube = (i == 0) == nearFirst_;
            Solid(ctx, S().shapes->Cube(), Matrix::CreateScale(1.7f) * Matrix::CreateRotationY(0.5f) * Matrix::CreateTranslation(nearCube ? a : b), nearCube ? Vector3(0.95f, 0.35f, 0.3f) : Vector3(0.3f, 0.5f, 1.0f));
        }
        d.setDepthStencilStateProperty(DepthStencilState::Default);
        gizmos_.Label(a + Vector3(0, 1.2f, 0), "near (red)", Color(255, 150, 140, 255), 0.5f);
        gizmos_.Label(b + Vector3(0, 1.2f, 0), "far (blue)", Color(150, 180, 255, 255), 0.5f);
    }

    void DrawDepthRead(const DrawContext& ctx, const Vector3& o)
    {
        GraphicsDevice& d = ctx.device;
        // A solid post and three glass panes at different depths.
        Solid(ctx, S().shapes->Cylinder(), Matrix::CreateScale(0.5f, 2.6f, 0.5f) * Matrix::CreateTranslation(o + Off(0, -1.3f, -0.2f)), Vector3(0.8f, 0.6f, 0.4f));
        struct Pane { float z; Vector3 colour; };
        Pane panes[3] = {{1.2f, {0.9f, 0.3f, 0.3f}}, {0.2f, {0.3f, 0.9f, 0.4f}}, {-0.9f, {0.3f, 0.5f, 1.0f}}};   // farthest from a southern visitor is the lowest z
        if (sorted_) std::sort(std::begin(panes), std::end(panes), [](const Pane& a, const Pane& b) { return a.z < b.z; });
        else std::reverse(std::begin(panes), std::end(panes));
        d.setBlendStateProperty(BlendState::NonPremultiplied);
        // DepthRead is XNA's stock state for glass: depth test on, depth writes off. With writes left on the panes use a copy
        // of Default whose DepthBufferWriteEnable is set explicitly.
        if (depthWrite_) {
            DepthStencilState writing = DepthStencilState::Default;
            writing.setDepthBufferWriteEnableProperty(true);
            d.setDepthStencilStateProperty(writing);
        } else {
            d.setDepthStencilStateProperty(DepthStencilState::DepthRead);
        }
        d.setRasterizerStateProperty(RasterizerState::CullNone);
        for (const Pane& p : panes) {
            BasicEffect& fx = Lit(ctx, Matrix::CreateScale(2.4f, 2.4f, 1.0f) * Matrix::CreateTranslation(o + Off(0, 0, p.z)), p.colour);
            fx.setAlphaProperty(0.45f);
            EffectBank::Apply(fx);
            S().shapes->Panel().Draw(d);
        }
        d.setBlendStateProperty(BlendState::Opaque);
        d.setDepthStencilStateProperty(DepthStencilState::Default);
    }

    void DrawPortal(const DrawContext& ctx, const Vector3& o)
    {
        GraphicsDevice& d = ctx.device;
        d.Clear(ClearOptions::Stencil, Color(0, 0, 0, 0), 1.0f, 0);
        d.setRasterizerStateProperty(RasterizerState::CullNone);
        const Matrix frame = Matrix::CreateScale(size_ * 1.6f, size_, 1.0f) * Matrix::CreateTranslation(o);
        // 1. The window writes stencil = 1 and no colour.
        DepthStencilState write = DepthStencilState::Default;
        write.setStencilEnableProperty(stencil_);
        write.setStencilFunctionProperty(CompareFunction::Always);
        write.setStencilPassProperty(StencilOperation::Replace);
        write.setReferenceStencilProperty(1);
        d.setDepthStencilStateProperty(write);
        BlendState none = BlendState::Opaque;
        none.setColorWriteChannelsProperty(ColorWriteChannels::None);
        d.setBlendStateProperty(none);
        { BasicEffect& fx = Lit(ctx, frame, Vector3(1, 1, 1)); EffectBank::Apply(fx); S().shapes->Panel().Draw(d); }
        // 2. The other world, only where stencil == 1. Without the stencil it is simply drawn in front of the wall.
        DepthStencilState read = DepthStencilState::Default;
        read.setStencilEnableProperty(stencil_);
        read.setStencilFunctionProperty(CompareFunction::Equal);
        read.setStencilPassProperty(StencilOperation::Keep);
        read.setReferenceStencilProperty(1);
        read.setDepthBufferEnableProperty(false);
        d.setDepthStencilStateProperty(read);
        d.setBlendStateProperty(BlendState::Opaque);
        const Vector3 sky = Vector3(0.35f, 0.55f, 0.95f);
        { BasicEffect& fx = ctx.fx.Flat(); fx.setWorldProperty(Matrix::CreateTranslation(o + Off(0, 0, -0.5f))); fx.setViewProperty(ctx.view.view); fx.setProjectionProperty(ctx.view.projection); EffectBank::Apply(fx);
          const Color c(static_cast<std::uint8_t>(sky.X * 255), static_cast<std::uint8_t>(sky.Y * 255), static_cast<std::uint8_t>(sky.Z * 255), static_cast<std::uint8_t>(255));
          const VertexPositionColor q[6] = {{Vector3(-4, -3, 0), c}, {Vector3(4, -3, 0), c}, {Vector3(4, 3, 0), c}, {Vector3(-4, -3, 0), c}, {Vector3(4, 3, 0), c}, {Vector3(-4, 3, 0), c}};
          d.DrawUserPrimitives(PrimitiveType::TriangleList, q, 0, 2); }
        Solid(ctx, S().shapes->Sphere(), Matrix::CreateScale(1.2f) * Matrix::CreateTranslation(o + Off(std::sin(time_) * 0.8f, 0, -1.5f)), Vector3(1.0f, 0.8f, 0.3f));
        d.setDepthStencilStateProperty(DepthStencilState::Default);
        // The surrounding frame.
        d.setRasterizerStateProperty(RasterizerState::CullClockwise);
        gizmos_.Box(BoundingBox(o + Off(-size_ * 0.8f, -size_ * 0.5f, 0), o + Off(size_ * 0.8f, size_ * 0.5f, 0.01f)), Color(255, 230, 150, 255));
    }

    // A flat-colour quad in the plane z = 0 of `frame`, `w` x `h` metres, centred.
    void FlatRect(const DrawContext& ctx, const Matrix& frame, float cx, float cy, float w, float h, const Color& c)
    {
        BasicEffect& fx = ctx.fx.Flat();
        fx.setWorldProperty(frame);
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);
        EffectBank::Apply(fx);
        const float x0 = cx - w * 0.5f, x1 = cx + w * 0.5f, y0 = cy - h * 0.5f, y1 = cy + h * 0.5f;
        const VertexPositionColor q[6] = {{Vector3(x0, y0, 0), c}, {Vector3(x1, y0, 0), c}, {Vector3(x1, y1, 0), c}, {Vector3(x0, y0, 0), c}, {Vector3(x1, y1, 0), c}, {Vector3(x0, y1, 0), c}};
        ctx.device.DrawUserPrimitives(PrimitiveType::TriangleList, q, 0, 2);
    }
    void FlatDisc(const DrawContext& ctx, const Matrix& frame, float cx, float cy, float r, const Color& c)
    {
        BasicEffect& fx = ctx.fx.Flat();
        fx.setWorldProperty(frame);
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);
        EffectBank::Apply(fx);
        VertexPositionColor v[48 * 3];
        for (int i = 0; i < 48; ++i) {
            const float a0 = MathHelper::TwoPi * static_cast<float>(i) / 48.0f, a1 = MathHelper::TwoPi * static_cast<float>(i + 1) / 48.0f;
            v[i * 3 + 0] = {Vector3(cx, cy, 0), c};
            v[i * 3 + 1] = {Vector3(cx + std::cos(a0) * r, cy + std::sin(a0) * r, 0), c};
            v[i * 3 + 2] = {Vector3(cx + std::cos(a1) * r, cy + std::sin(a1) * r, 0), c};
        }
        ctx.device.DrawUserPrimitives(PrimitiveType::TriangleList, v, 0, 48);
    }

    void DrawStencilCount(const DrawContext& ctx, const Vector3& o)
    {
        GraphicsDevice& d = ctx.device;
        static const StencilOperation ops[] = {StencilOperation::Increment, StencilOperation::IncrementSaturation, StencilOperation::Decrement,
                                               StencilOperation::DecrementSaturation, StencilOperation::Invert, StencilOperation::Replace};
        const bool two = countScene_ == 1;
        const int start = two ? 128 : clearValue_;
        d.Clear(ClearOptions::Stencil, Color(0, 0, 0, 0), 1.0f, start);
        d.setRasterizerStateProperty(RasterizerState::CullNone);
        const Matrix frame = Matrix::CreateRotationY(R().frameYaw) * Matrix::CreateTranslation(o + Off(0, 0.2f, -1.0f));
        BlendState none = BlendState::Opaque;
        none.setColorWriteChannelsProperty(ColorWriteChannels::None);
        // Pass 1: the geometry only changes the stencil (no colour, no depth test).
        DepthStencilState count = DepthStencilState::Default;
        count.setDepthBufferEnableProperty(false);
        count.setStencilEnableProperty(true);
        count.setStencilFunctionProperty(CompareFunction::Always);
        count.setStencilPassProperty(two ? StencilOperation::Increment : ops[countOp_]);
        if (!two && countFail_ == 1) {   // the stencil test itself fails everywhere: StencilFail is the operation that runs
            count.setStencilFunctionProperty(CompareFunction::Never);
            count.setStencilPassProperty(StencilOperation::Keep);
            count.setStencilFailProperty(ops[countOp_]);
        } else if (!two && countFail_ == 2) {   // the stencil test passes but the depth test fails everywhere
            count.setDepthBufferEnableProperty(true);
            count.setDepthBufferFunctionProperty(CompareFunction::Never);
            count.setStencilPassProperty(StencilOperation::Keep);
            count.setStencilDepthBufferFailProperty(ops[countOp_]);
        }
        count.setReferenceStencilProperty(countOp_ == 5 && !two ? 5 : 1);
        count.setStencilWriteMaskProperty(two ? 255 : writeMask_);
        if (two) {
            count.setTwoSidedStencilModeProperty(true);
            count.setCounterClockwiseStencilFunctionProperty(CompareFunction::Always);
            count.setCounterClockwiseStencilPassProperty(StencilOperation::Decrement);
        }
        d.setDepthStencilStateProperty(count);
        d.setBlendStateProperty(none);
        if (!two) {
            const float cx[4] = {-1.5f, -0.5f, 0.45f, 1.4f}, cy[4] = {0.3f, -0.3f, 0.35f, -0.25f}, r[4] = {1.0f, 1.05f, 0.95f, 1.0f};
            for (int i = 0; i < 4; ++i) FlatDisc(ctx, frame, cx[i], cy[i], r[i], Color::White);
        } else {
            // The solids carry normals and texture coordinates, not vertex colours, so they go through the lit effect (the colour is masked in
            // this pass anyway); the flat effect reads a colour from every vertex and CNA now refuses a draw whose vertices lack one.
            BasicEffect& fx = ctx.fx.BeginLit();
            fx.setViewProperty(ctx.view.view);
            fx.setProjectionProperty(ctx.view.projection);
            const auto shape = [&](const LitMesh& m, const Matrix& w) { fx.setWorldProperty(w); EffectBank::Apply(fx); m.Draw(d); };
            shape(S().shapes->Cube(), Matrix::CreateScale(1.3f) * Matrix::CreateRotationY(time_ * 0.5f) * Matrix::CreateTranslation(o + Off(-1.9f, 0.0f, -1.0f)));
            shape(S().shapes->Sphere(), Matrix::CreateScale(1.6f) * Matrix::CreateTranslation(o + Off(0.0f, 0.0f, -1.0f)));
            shape(S().shapes->Panel(), Matrix::CreateScale(1.6f) * Matrix::CreateRotationY(R().frameYaw) * Matrix::CreateTranslation(o + Off(1.9f, 0.0f, -1.0f)));
        }
        // Pass 2: paint every pixel whose stencil value is `start + k` with a colour for k, using StencilFunction.Equal.
        d.setBlendStateProperty(BlendState::Opaque);
        d.setRasterizerStateProperty(RasterizerState::CullNone);
        for (int k = -4; k <= 6; ++k) {
            if (k == 0) continue;
            const int value = ((start + k) % 256 + 256) % 256;
            DepthStencilState read = DepthStencilState::Default;
            read.setDepthBufferEnableProperty(false);
            read.setStencilEnableProperty(true);
            read.setStencilFunctionProperty(CompareFunction::Equal);
            read.setStencilPassProperty(StencilOperation::Keep);
            read.setReferenceStencilProperty(value);
            read.setStencilMaskProperty(255);
            d.setDepthStencilStateProperty(read);
            static const Color up[7] = {Color(0, 0, 0, 255), Color(70, 200, 120, 255), Color(240, 220, 70, 255), Color(255, 150, 50, 255), Color(240, 70, 70, 255), Color(220, 80, 220, 255), Color(250, 250, 250, 255)};
            static const Color down[5] = {Color(0, 0, 0, 255), Color(255, 175, 80, 255), Color(120, 150, 255, 255), Color(60, 90, 200, 255), Color(30, 40, 120, 255)};
            const Color c = k > 0 ? up[k] : down[-k];
            FlatRect(ctx, frame, 0.0f, 0.0f, 8.0f, 4.0f, c);
        }
        d.setDepthStencilStateProperty(DepthStencilState::Default);
        d.setRasterizerStateProperty(RasterizerState::CullClockwise);
        gizmos_.Label(o + Off(0, 1.9f, -1.0f), two ? "closed meshes cancel to 0 (unmarked); the open panel does not" : "green 1 disc, yellow 2, orange 3, red 4 (the stencil value minus the cleared value)", Color(255, 230, 150, 255), 0.5f);
    }

    void DrawOutline(const DrawContext& ctx, const Vector3& o)
    {
        GraphicsDevice& d = ctx.device;
        d.Clear(ClearOptions::Stencil, Color(0, 0, 0, 0), 1.0f, 0);
        d.setRasterizerStateProperty(RasterizerState::CullClockwise);
        for (int i = 0; i < 2; ++i) {
            const Vector3 p = o + Off(-1.5f + static_cast<float>(i) * 3.0f, 0, 0);
            const Matrix m = Matrix::CreateRotationY(time_ * 0.6f + static_cast<float>(i)) * Matrix::CreateTranslation(p);
            DepthStencilState write = DepthStencilState::Default;
            write.setStencilEnableProperty(true);
            write.setStencilFunctionProperty(CompareFunction::Always);
            write.setStencilPassProperty(StencilOperation::Replace);
            write.setReferenceStencilProperty(1);
            d.setDepthStencilStateProperty(write);
            Solid(ctx, i == 0 ? S().shapes->Sphere() : S().shapes->Cube(), Matrix::CreateScale(1.7f) * m, Vector3(0.5f, 0.6f, 0.9f));
            DepthStencilState outline = DepthStencilState::Default;
            outline.setStencilEnableProperty(stencil_);
            outline.setStencilFunctionProperty(CompareFunction::NotEqual);
            outline.setStencilPassProperty(StencilOperation::Keep);
            outline.setReferenceStencilProperty(1);
            d.setDepthStencilStateProperty(outline);
            d.setRasterizerStateProperty(RasterizerState::CullCounterClockwise);   // draw the enlarged copy's back faces so only a rim shows
            BasicEffect& fx = Lit(ctx, Matrix::CreateScale(1.7f * size_) * m, Vector3(1.0f, 0.8f, 0.2f));
            fx.setLightingEnabledProperty(false);
            fx.setEmissiveColorProperty(Vector3(1.0f, 0.8f, 0.2f));
            fx.setDiffuseColorProperty(Vector3(0, 0, 0));
            EffectBank::Apply(fx);
            (i == 0 ? S().shapes->Sphere() : S().shapes->Cube()).Draw(d);
            d.setRasterizerStateProperty(RasterizerState::CullClockwise);
            d.setDepthStencilStateProperty(DepthStencilState::Default);
        }
    }

    void DrawShadow(const DrawContext& ctx)
    {
        GraphicsDevice& d = ctx.device;
        d.Clear(ClearOptions::Stencil, Color(0, 0, 0, 0), 1.0f, 0);
        d.setBlendStateProperty(BlendState::Opaque);
        const Vector3 floor = R().ToWorld(0, 0.04f, 0);
        d.setRasterizerStateProperty(RasterizerState::CullNone);
        Flat(ctx, Matrix::CreateTranslation(floor), Color(200, 200, 190, 255), Vector3(-4, 0, -4), Vector3(8, 0, 0), Vector3(0, 0, 8));
        d.setRasterizerStateProperty(RasterizerState::CullClockwise);
        const Vector3 lightPos = floor + Vector3(std::cos(time_ * 0.5f) * 1.5f, size_, std::sin(time_ * 0.5f) * 1.5f);
        const Plane ground(Vector3(0, 1, 0), -floor.Y);
        const Matrix shadow = Matrix::CreateShadow(Vector3::Normalize(lightPos - floor), ground);
        // Two overlapping occluders.
        const Matrix a = Matrix::CreateScale(1.3f) * Matrix::CreateTranslation(floor + Vector3(-0.4f, 0.8f, 0));
        const Matrix b = Matrix::CreateScale(1.0f) * Matrix::CreateTranslation(floor + Vector3(0.5f, 0.6f, 0.3f));
        Solid(ctx, S().shapes->Sphere(), a, Vector3(0.9f, 0.4f, 0.3f));
        Solid(ctx, S().shapes->Cube(), b, Vector3(0.3f, 0.6f, 0.95f));
        // Shadows: blended black. With the stencil test each floor pixel is darkened only once.
        d.setBlendStateProperty(BlendState::NonPremultiplied);
        DepthStencilState once = DepthStencilState::None;
        once.setStencilEnableProperty(stencil_);
        once.setStencilFunctionProperty(CompareFunction::Equal);
        once.setReferenceStencilProperty(0);
        once.setStencilPassProperty(StencilOperation::Increment);
        d.setDepthStencilStateProperty(once);
        d.setRasterizerStateProperty(RasterizerState::CullNone);
        for (const Matrix* m : {&a, &b}) {
            // The occluder's silhouette: the unit sphere/cube meshes are drawn through the shadow matrix with a dark colour.
            BasicEffect& dark = Lit(ctx, *m * shadow * Matrix::CreateTranslation(0, 0.01f, 0), Vector3(0, 0, 0));
            dark.setLightingEnabledProperty(false);
            dark.setEmissiveColorProperty(Vector3(0, 0, 0));
            dark.setDiffuseColorProperty(Vector3(0, 0, 0));
            dark.setAlphaProperty(0.55f);
            EffectBank::Apply(dark);
            (m == &a ? S().shapes->Sphere() : S().shapes->Cube()).Draw(d);
        }
        d.setBlendStateProperty(BlendState::Opaque);
        d.setDepthStencilStateProperty(DepthStencilState::Default);
        d.setRasterizerStateProperty(RasterizerState::CullClockwise);
        gizmos_.SolidSphere(ctx, lightPos, 0.12f, Vector3(1, 0.9f, 0.4f));
    }

    struct Cfg {
        const char* name;
        Blend src, dst;
        BlendFunction fn;
    };
    void DrawBlendGallery(const DrawContext& ctx, const Vector3& o)
    {
        GraphicsDevice& d = ctx.device;
        static const Cfg cfgs[16] = {
            {"One / Zero (opaque)", Blend::One, Blend::Zero, BlendFunction::Add},
            {"SrcAlpha / InvSrcAlpha", Blend::SourceAlpha, Blend::InverseSourceAlpha, BlendFunction::Add},
            {"One / InvSrcAlpha (premult.)", Blend::One, Blend::InverseSourceAlpha, BlendFunction::Add},
            {"SrcAlpha / One (additive)", Blend::SourceAlpha, Blend::One, BlendFunction::Add},
            {"One / One", Blend::One, Blend::One, BlendFunction::Add},
            {"DstColor / Zero (multiply)", Blend::DestinationColor, Blend::Zero, BlendFunction::Add},
            {"DstColor / SrcColor (2x)", Blend::DestinationColor, Blend::SourceColor, BlendFunction::Add},
            {"One / InvSrcColor (screen)", Blend::One, Blend::InverseSourceColor, BlendFunction::Add},
            {"SrcAlpha / One, RevSubtract", Blend::SourceAlpha, Blend::One, BlendFunction::ReverseSubtract},
            {"One / One, Subtract", Blend::One, Blend::One, BlendFunction::Subtract},
            {"One / One, Min", Blend::One, Blend::One, BlendFunction::Min},
            {"One / One, Max", Blend::One, Blend::One, BlendFunction::Max},
            {"InvDstColor / InvSrcColor", Blend::InverseDestinationColor, Blend::InverseSourceColor, BlendFunction::Add},
            {"Zero / SrcColor", Blend::Zero, Blend::SourceColor, BlendFunction::Add},
            {"SrcAlphaSat / One", Blend::SourceAlphaSaturation, Blend::One, BlendFunction::Add},
            {"DstAlpha / InvDstAlpha", Blend::DestinationAlpha, Blend::InverseDestinationAlpha, BlendFunction::Add},
        };
        const int a = static_cast<int>(alpha_ * 255.0f);
        for (int i = 0; i < 16; ++i) {
            const int col = i % 4, row = i / 4;
            const Vector3 c = o + Off(-3.6f + static_cast<float>(col) * 2.4f, 1.6f - static_cast<float>(row) * 1.3f, 0.0f);
            d.setBlendStateProperty(BlendState::Opaque);
            d.setDepthStencilStateProperty(DepthStencilState::None);
            d.setRasterizerStateProperty(RasterizerState::CullNone);
            // Background: a gradient texture; foreground: two overlapping translucent coloured squares.
            DrawTex(ctx, S().textures->Get("gradient"), c, 1.05f, Color::White, 0.0f);
            BlendState bs;
            bs.setColorSourceBlendProperty(cfgs[i].src);
            bs.setColorDestinationBlendProperty(cfgs[i].dst);
            bs.setColorBlendFunctionProperty(cfgs[i].fn);
            bs.setAlphaSourceBlendProperty(Blend::One);
            bs.setAlphaDestinationBlendProperty(Blend::Zero);
            d.setBlendStateProperty(bs);
            DrawTex(ctx, S().textures->White(), c + Off(-0.2f, 0.1f, 0.01f), 0.62f, Color(250, 120, 60, a), 0.0f);
            DrawTex(ctx, S().textures->White(), c + Off(0.2f, -0.1f, 0.02f), 0.62f, Color(60, 150, 250, a), 0.0f);
            gizmos_.Label(c + Vector3(0, -0.62f, 0), cfgs[i].name, Color(255, 230, 150, 255), 0.36f);
        }
        d.setBlendStateProperty(BlendState::Opaque);
        d.setDepthStencilStateProperty(DepthStencilState::Default);
    }
    void DrawTex(const DrawContext& ctx, Texture2D* tex, const Vector3& centre, float half, const Color& tint, float)
    {
        BasicEffect& fx = ctx.fx.World();
        fx.setTextureProperty(tex);
        fx.setTextureEnabledProperty(true);
        fx.setVertexColorEnabledProperty(true);
        fx.setDiffuseColorProperty(Vector3(1, 1, 1));
        // A turned room (entered from the north) is built with its frame rotated: the quad has to turn with it or it faces the wall.
        fx.setWorldProperty(Matrix::CreateRotationY(R().frameYaw) * Matrix::CreateTranslation(centre));
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);
        ctx.device.getSamplerStatesProperty()[0] = SamplerState::LinearClamp;
        EffectBank::Apply(fx);
        const VertexPositionColorTexture q[6] = {{Vector3(-half, -half, 0), tint, Vector2(0, 1)}, {Vector3(half, -half, 0), tint, Vector2(1, 1)}, {Vector3(half, half, 0), tint, Vector2(1, 0)},
                                                 {Vector3(-half, -half, 0), tint, Vector2(0, 1)}, {Vector3(half, half, 0), tint, Vector2(1, 0)}, {Vector3(-half, half, 0), tint, Vector2(0, 0)}};
        ctx.device.DrawUserPrimitives(PrimitiveType::TriangleList, q, 0, 2);
    }

    void DrawColorWrite(const DrawContext& ctx, const Vector3& o)
    {
        GraphicsDevice& d = ctx.device;
        BlendState bs = BlendState::Opaque;
        ColorWriteChannels ch = ColorWriteChannels::None;
        int bits = (mask_[0] ? 1 : 0) | (mask_[1] ? 2 : 0) | (mask_[2] ? 4 : 0) | (mask_[3] ? 8 : 0);
        ch = static_cast<ColorWriteChannels>(bits);
        bs.setColorWriteChannelsProperty(ch);
        d.setBlendStateProperty(BlendState::Opaque);
        DrawTex(ctx, S().textures->White(), o + Off(0, 0, -0.05f), 2.3f, Color(30, 30, 40, 255), 0.0f);
        d.setBlendStateProperty(bs);
        d.setDepthStencilStateProperty(DepthStencilState::None);
        DrawTex(ctx, S().textures->White(), o + Off(-0.7f, 0.5f, 0.01f), 1.3f, Color(255, 60, 60, 255), 0.0f);
        DrawTex(ctx, S().textures->White(), o + Off(0.7f, 0.5f, 0.02f), 1.3f, Color(60, 255, 60, 255), 0.0f);
        DrawTex(ctx, S().textures->White(), o + Off(0.0f, -0.7f, 0.03f), 1.3f, Color(60, 60, 255, 255), 0.0f);
        d.setBlendStateProperty(BlendState::Opaque);
        d.setDepthStencilStateProperty(DepthStencilState::Default);
    }

    void DrawBlendFactor(const DrawContext& ctx, const Vector3& o)
    {
        GraphicsDevice& d = ctx.device;
        d.setDepthStencilStateProperty(DepthStencilState::None);
        d.setBlendStateProperty(BlendState::Opaque);
        DrawTex(ctx, S().textures->Get("gradient"), o, 2.0f, Color::White, 0.0f);
        BlendState bs;
        bs.setColorSourceBlendProperty(Blend::BlendFactor);
        bs.setColorDestinationBlendProperty(Blend::InverseBlendFactor);
        bs.setAlphaSourceBlendProperty(Blend::One);
        bs.setAlphaDestinationBlendProperty(Blend::Zero);
        bs.setBlendFactorProperty(Color(static_cast<std::uint8_t>(factor_[0] * 255), static_cast<std::uint8_t>(factor_[1] * 255), static_cast<std::uint8_t>(factor_[2] * 255), static_cast<std::uint8_t>(255)));
        d.setBlendStateProperty(bs);
        DrawTex(ctx, S().textures->White(), o + Off(0, 0, 0.02f), 1.2f, Color(255, 230, 40, 255), 0.0f);
        d.setBlendStateProperty(BlendState::Opaque);
        d.setDepthStencilStateProperty(DepthStencilState::Default);
    }

    int countScene_ = 0, countOp_ = 0, countFail_ = 0, writeMask_ = 255, clearValue_ = 0;
    Mode mode_;
    Gizmos gizmos_;
    std::unique_ptr<BasicEffect> fx_;
    std::unique_ptr<MiniView> view_;
    float time_ = 0.0f, size_ = 1.6f, bias_ = 0.0f, slope_ = 0.0f, sx_ = 120.0f, sw_ = 300.0f, alpha_ = 0.7f;
    float factor_[3] = {0.6f, 0.3f, 0.9f};
    bool wire_ = true, scissor_ = true, depthOn_ = true, depthWrite_ = true, nearFirst_ = false, sorted_ = false, stencil_ = true;
    bool mask_[4] = {true, true, true, true};
    int cull_ = 1, fn_ = 0;
};

}  // namespace

std::unique_ptr<Exhibit> MakeRenderStateLab(const RoomDef& room, Services& services)
{
    return std::make_unique<RenderStateLab>(room, services);
}

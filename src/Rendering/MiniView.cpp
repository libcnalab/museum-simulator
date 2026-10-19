#include "Rendering/MiniView.hpp"

#include <cmath>

#include "Microsoft/Xna/Framework/Graphics/DepthFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/LitMesh.hpp"
#include "Rendering/QuadDraw.hpp"
#include "Rendering/RenderStates.hpp"
#include "Rendering/TextureLibrary.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

MiniView::MiniView(Services& services, int width, int height) : MiniView(services, width, height, DepthFormat::Depth24, false) {}

MiniView::MiniView(Services& services, int width, int height, DepthFormat depth, bool mipMap) : s_(services), width_(width), height_(height)
{
    target_ = std::make_unique<RenderTarget2D>(*s_.device, width, height, mipMap, SurfaceFormat::Color, depth, 0,
                                               RenderTargetUsage::PreserveContents);
}

void MiniView::Render(const DrawContext& main, const Matrix& view, const Matrix& projection, const Vector3& eye,
                      const std::function<void(const DrawContext&)>& draw, const Color& clear)
{
    GraphicsDevice& device = main.device;
    const SceneView sceneView(view, projection, eye);
    DrawContext sub{device, main.fx, sceneView, main.time, true};
    main.fx.SetCamera(view, projection);
    device.SetRenderTarget(target_.get());
    // A target made without a depth buffer may only clear its colour (XNA refuses to clear a depth buffer that is not there).
    if (target_->getDepthStencilFormatProperty() == DepthFormat::None) device.Clear(clear);
    else device.Clear(clear, 1.0f);
    RenderStates::SetOpaque3D(device);
    draw(sub);
    device.SetRenderTarget(nullptr);
    rendered_ = true;
}

void MiniView::DrawInRoom(const DrawContext& ctx, const Vector3& centre, float yaw, float width, const SamplerState* sampler)
{
    if (!rendered_) return;
    RenderStates::SetOpaque3D(ctx.device);
    if (sampler != nullptr) ctx.device.getSamplerStatesProperty()[0] = *sampler;
    const Vector3 n(std::sin(yaw), 0.0f, std::cos(yaw));
    const Vector3 right = Vector3::Normalize(Vector3::Cross(-n, Vector3::Up)) * width;
    const Vector3 up(0, width / Aspect(), 0);
    QuadDraw::Textured(ctx, target_.get(), centre - right * 0.5f - up * 0.5f, right, up);
}

namespace {
const DioramaItem kItems[] = {
    {{-1.6f, 0.5f, -0.4f}, 0.5f}, {{1.5f, 0.5f, -1.2f}, 0.5f}, {{0.2f, 0.5f, 1.4f}, 0.5f},   {{-0.4f, 0.5f, -2.6f}, 0.5f},
    {{2.2f, 0.5f, 1.8f}, 0.5f},   {{-2.4f, 0.5f, 2.4f}, 0.5f}, {{0.0f, 0.5f, -5.0f}, 0.5f},  {{-3.0f, 0.5f, -3.8f}, 0.5f},
};
}

const DioramaItem* DioramaItems(int& count)
{
    count = static_cast<int>(sizeof(kItems) / sizeof(kItems[0]));
    return kItems;
}

void DrawDiorama(const DrawContext& ctx, Services& s, const Vector3& origin, double time, float spread)
{
    GraphicsDevice& device = ctx.device;
    BasicEffect& fx = ctx.fx.BeginLit(s.textures->Get("checker"));
    fx.EnableDefaultLighting();
    fx.setPunctualLightEXT(PunctualLightEXT{});
    fx.setViewProperty(ctx.view.view);
    fx.setProjectionProperty(ctx.view.projection);
    fx.setDiffuseColorProperty(Vector3(0.8f, 0.85f, 0.9f));
    fx.setEmissiveColorProperty(Vector3(0, 0, 0));
    fx.setSpecularColorProperty(Vector3(0, 0, 0));
    fx.setWorldProperty(Matrix::CreateScale(spread * 2.6f, 1.0f, spread * 2.6f) * Matrix::CreateTranslation(origin));
    EffectBank::Apply(fx);
    s.shapes->Plane().Draw(device);

    fx.setTextureProperty(s.textures->White());
    const Vector3 colours[] = {{0.9f, 0.3f, 0.25f}, {0.3f, 0.5f, 0.95f}, {0.3f, 0.85f, 0.4f}, {0.95f, 0.8f, 0.3f},
                               {0.8f, 0.4f, 0.9f}, {0.3f, 0.85f, 0.85f}, {0.95f, 0.55f, 0.2f}, {0.85f, 0.85f, 0.9f}};
    int count = 0;
    const DioramaItem* items = DioramaItems(count);
    const float spin = static_cast<float>(time) * 0.6f;
    for (int i = 0; i < count; ++i) {
        fx.setDiffuseColorProperty(colours[i % 8]);
        Vector3 p = origin + items[i].offset * (spread / 3.0f);
        if (i % 4 >= 2) p.Y = origin.Y;   // cones and cylinders stand on their base
        const float size = items[i].radius * 2.0f * (spread / 3.0f);
        const Matrix place = Matrix::CreateTranslation(p);
        fx.setWorldProperty((i % 4 == 0 ? Matrix::CreateScale(size) * Matrix::CreateRotationY(spin) : Matrix::CreateScale(size)) * place);
        EffectBank::Apply(fx);
        switch (i % 4) {
            case 0: s.shapes->Cube().Draw(device); break;
            case 1: s.shapes->Sphere().Draw(device); break;
            case 2: s.shapes->Cone().Draw(device); break;
            default: s.shapes->Cylinder().Draw(device); break;
        }
    }
}

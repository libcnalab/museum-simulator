#include "Rendering/SignFactory.hpp"

#include <algorithm>

#include "CNA/RendererCapabilityProfile.hpp"
#include "Diagnostics/Log.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/DeviceLostException.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDeviceStatus.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteSortMode.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"
#include "Microsoft/Xna/Framework/Rectangle.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"
#include "Rendering/ImageUtil.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

SignFactory::SignFactory(GraphicsDevice& device, SpriteBatch& sprites, SpriteFont* font, SpriteFont* titleFont,
                         TextureLibrary& textures)
    : device_(device), sprites_(sprites), font_(font), titleFont_(titleFont), textures_(textures)
{
}

std::string SignFactory::CreateCustom(int w, int h, const Color& background,
                                      const std::function<void(SpriteBatch&, int, int)>& draw)
{
    const std::string name = "sign:" + std::to_string(counter_++);
#ifdef __EMSCRIPTEN__
    // A browser readback crosses the WebGL/Wasm boundary and can take more than a second for a room card.
    // Keep the rendered target itself: EasyGL generates its mip chain when the target is unbound, so this
    // is both the shortest path and the same filtered texture the room will sample afterwards.
    auto target = std::make_unique<RenderTarget2D>(device_, w, h, true, SurfaceFormat::Color, DepthFormat::None, 0,
                                                    RenderTargetUsage::PreserveContents);
    try {
        device_.SetRenderTarget(target.get());
        device_.Clear(background);
        const SamplerState sampler = SamplerState::LinearClamp;
        sprites_.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &sampler, nullptr, nullptr);
        draw(sprites_, w, h);
        sprites_.End();
        device_.SetRenderTarget(nullptr);
    } catch (const std::exception& e) {
        device_.SetRenderTarget(nullptr);
        Museum::Log("signs", "could not render sign: %s", e.what());
        Museum::ReportExhibitError("signs", (std::string("could not render sign: ") + e.what()).c_str());
    }
    textures_.Register(name, std::move(target));
#else
    std::vector<Color> pixels(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), background);
    try {
        // A lost device (a failed Reset, a lost Direct3D 9 device) refuses every draw with DeviceLostException until it is
        // reset, as XNA's does; the sign then keeps its background colour.
        if (device_.getGraphicsDeviceStatusProperty() != GraphicsDeviceStatus::Normal) throw DeviceLostException();
        RenderTarget2D target(device_, w, h, false, SurfaceFormat::Color, DepthFormat::None, 0,
                              RenderTargetUsage::PreserveContents);
        device_.SetRenderTarget(&target);
        device_.Clear(background);
        const SamplerState sampler = SamplerState::LinearClamp;
        sprites_.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &sampler, nullptr, nullptr);
        draw(sprites_, w, h);
        sprites_.End();
        device_.SetRenderTarget(nullptr);
        // A renderer that executes no rasterisation (HEADLESS) declares that it cannot read a render target back, and the
        // sign stays its background colour. A renderer that declares readback and then refuses it is an error below.
        if (device_.SupportsRendererFeatureEXT(CNA::RendererFeature::RenderTargetReadback)) {
            target.GetData(pixels.data(), static_cast<int>(pixels.size()));
        } else {
            static bool told = false;
            if (!told) Museum::Log("signs", "signs drawn without text: the renderer reports no RenderTargetReadback");
            told = true;
        }
    } catch (const DeviceLostException&) {
        static bool toldLost = false;
        if (!toldLost) Museum::Log("signs", "signs drawn without text: the graphics device is lost");
        toldLost = true;
    } catch (const std::exception& e) {
        device_.SetRenderTarget(nullptr);
        Museum::Log("signs", "could not render sign: %s", e.what());
        Museum::ReportExhibitError("signs", (std::string("could not render sign: ") + e.what()).c_str());
    }
    for (Color& c : pixels) c = Color(c.getRProperty(), c.getGProperty(), c.getBProperty(), static_cast<std::uint8_t>(255));
    textures_.Register(name, ImageUtil::CreateMipmapped(device_, pixels, w, h));
#endif
    if (scope_ != nullptr) scope_->push_back(name);
    return name;
}

std::string SignFactory::Create(const SignSpec& spec)
{
    const int w = spec.pixelWidth;
    const int h = spec.pixelHeight;
    return CreateCustom(w, h, spec.background, [&](SpriteBatch& sprites, int, int) {
        Texture2D* white = textures_.White();
        const int b = std::max(2, h / 40);
        sprites.Draw(*white, Rectangle(0, 0, w, b), spec.border);
        sprites.Draw(*white, Rectangle(0, h - b, w, b), spec.border);
        sprites.Draw(*white, Rectangle(0, 0, b, h), spec.border);
        sprites.Draw(*white, Rectangle(w - b, 0, b, h), spec.border);

        // Lay the lines out top to bottom, shrinking everything uniformly when they do not fit.
        float totalHeight = 0.0f;
        float widest = 0.0f;
        for (const SignLine& line : spec.lines) {
            SpriteFont* f = (line.large && titleFont_ != nullptr) ? titleFont_ : font_;
            if (f == nullptr) continue;
            totalHeight += static_cast<float>(f->getLineSpacingProperty());
            widest = std::max(widest, f->MeasureString(line.text).X);
        }
        const float margin = static_cast<float>(b) * 4.0f;
        const float fit = std::min({spec.textScale, (static_cast<float>(h) - margin * 2.0f) / std::max(totalHeight, 1.0f),
                                    (static_cast<float>(w) - margin * 2.0f) / std::max(widest, 1.0f)});
        float y = spec.leftAlign ? margin : (static_cast<float>(h) - totalHeight * fit) * 0.5f;
        for (const SignLine& line : spec.lines) {
            SpriteFont* f = (line.large && titleFont_ != nullptr) ? titleFont_ : font_;
            if (f == nullptr) continue;
            const Vector2 size = f->MeasureString(line.text);
            const float x = spec.leftAlign ? margin : (static_cast<float>(w) - size.X * fit) * 0.5f;
            sprites.DrawString(*f, line.text, Vector2(x, y), line.color, 0.0f, Vector2(0, 0), fit, SpriteEffects::None, 0.0f);
            y += static_cast<float>(f->getLineSpacingProperty()) * fit;
        }
    });
}

// Font build laboratory: XNA's FontDescriptionProcessor running in the game. A FontDescription (a typeface, a size, a style, the
// characters wanted) is turned into a SpriteFontContent by rasterising the TrueType file with FreeType and packing the glyphs into
// an atlas; the museum then builds a runtime SpriteFont from that atlas and draws with it, so every control changes real text.
// Without CNA's pipeline library (-DMUSEUM_PIPELINE=OFF) or without a FreeType in the build, the room says so.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/Content/PipelineSupport.hpp"
#include "Exhibits/ScreenExhibit.hpp"

#ifdef MUSEUM_PIPELINE
#include "CNA/Content/Pipeline/SpriteFontContentPipeline.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/ContentBuildLogger.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/ContentIdentity.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/ContentProcessorContext.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/FontDescriptionImporter.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Graphics/FontDescription.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Processors/FontProcessors.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Processors/SpriteFontContent.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Serialization/Compiler/ContentCompiler.hpp"
#include "System/NotSupportedException.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteFont.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#endif

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

#ifdef MUSEUM_PIPELINE
namespace Xna = Microsoft::Xna::Framework::Content::Pipeline;
namespace PipeGfx = Microsoft::Xna::Framework::Content::Pipeline::Graphics;
namespace Processors = Microsoft::Xna::Framework::Content::Pipeline::Processors;
namespace Compiler = Microsoft::Xna::Framework::Content::Pipeline::Serialization::Compiler;
using PipelineSupport::ProcessorContext;

#endif

class FontBuildLab final : public ScreenExhibit {
public:
    FontBuildLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 7.6f) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel("FontDescription", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
#ifdef MUSEUM_PIPELINE
        p.AddSlider("Size (points at 96 dpi)", 10.0f, 64.0f, size_, [this](float v) { size_ = v; Build(); }, "%.0f", 2.0f);
        p.AddChoice("Description from", {"these controls", "Museum.spritefont (FontDescriptionImporter)"}, 0, [this](int v) { fromFile_ = v == 1; Build(); });
        p.AddChoice("Style", {"Regular", "Bold (synthesised)", "Italic (synthesised)"}, 0, [this](int v) { style_ = v; Build(); });
        p.AddSlider("Spacing (extra pixels between glyphs)", -2.0f, 8.0f, spacing_, [this](float v) { spacing_ = v; Build(); }, "%.1f", 0.5f);
        p.AddToggle("UseKerning", true, [this](bool v) { kerning_ = v; Build(); });
        p.AddChoice("Characters", {"Basic Latin (32 to 126)", "Basic Latin and Latin-1", "Latin, Greek and Cyrillic", "Digits only"}, 0, [this](int v) { regions_ = v; Build(); });
        p.AddToggle("DefaultCharacter '?'", true, [this](bool v) { defaultChar_ = v; Build(); });
        p.AddChoice("Sample text", {"The quick brown fox jumps over the lazy dog", "Grüße aus Köln: 1234 € Ωμέγα Привет", "0123456789 +-*/"}, 0, [this](int v) { sample_ = v; DrawCheck(); });
        p.AddReadout("FontDescription", [this] { return imported_; });
        p.AddReadout("FontDescriptionProcessor", [this] { return status_; });
        p.AddReadout("Glyphs / atlas", [this] { return shape_; });
        p.AddReadout("ContentCompiler / XNB load", [this] { return compiled_; });
        p.AddReadout("LineSpacing", [this] { return lineSpacing_; });
        p.AddReadout("Build time", [this] { return buildTime_; });
#else
        p.AddReadout("Content pipeline", [] { return std::string("not built into this museum"); });
#endif
        AddInfoSign(b, "Building a font at run time",
                    {"A game normally compiles its fonts offline: a .spritefont names a typeface and the characters wanted, and XNA's FontDescriptionProcessor turns it into a glyph atlas in the .xnb.",
                     "CNA's pipeline does that work in process (FreeType rasterises Content/Fonts/DejaVuSans.ttf). Change a control and the processor runs again; the text on the screen is drawn with the font it just made."});
    }

    void OnScreenLoaded() override
    {
#ifdef MUSEUM_PIPELINE
        Build();
#endif
    }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        Texture2D& white = *S().whiteTexture;
        const auto rect = [&](float x, float y, float w, float h, Color c) { sb.Draw(white, Rectangle(static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h)), c); };
        const auto text = [&](const std::string& t, float x, float y, Color c, float s) { sb.DrawString(*S().font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), s, SpriteEffects::None, 0.0f); };
        text("The font the processor just built", 24, 8, Kit::kGold, 0.85f);
#ifdef MUSEUM_PIPELINE
        if (font_ != nullptr) {
            rect(24, 56, 912, 150, Color(16, 22, 34, 245));
            std::string line;
            try {
                static const char* const samples[] = {"The quick brown fox jumps over the lazy dog", "Gr\xC3\xBC\xC3\x9F" "e aus K\xC3\xB6ln: 1234 \xE2\x82\xAC \xCE\xA9\xCE\xBC\xCE\xAD\xCE\xB3\xCE\xB1 \xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82", "0123456789 +-*/"};
                line = samples[std::clamp(sample_, 0, 2)];
                const Vector2 size = font_->MeasureString(line);
                sb.DrawString(*font_, line, Vector2(36, 66), Color(255, 235, 170, 255));
                measured_ = "MeasureString: " + std::to_string(static_cast<int>(size.X)) + " x " + std::to_string(static_cast<int>(size.Y)) + " px";
                drawError_.clear();
            } catch (const std::exception& e) {
                drawError_ = std::string("DrawString refused: ") + std::string(e.what()).substr(0, 90);
            }
            if (!drawError_.empty()) text(drawError_, 36, 70, Kit::Rgba(255, 170, 140), 0.6f);
            text(measured_, 36, 176, Kit::kDim, 0.55f);
        } else {
            text(status_, 24, 90, Kit::Rgba(255, 190, 150), 0.7f);
        }
        // The atlas the processor produced, on a checkerboard so its transparent texels show.
        if (atlas_ != nullptr) {
            const float size = 300.0f, x = 620.0f, y = 220.0f;
            rect(x, y, size, size, Color(60, 60, 72, 255));
            for (int i = 0; i < 6; ++i)
                for (int j = 0; j < 6; ++j)
                    if ((i + j) % 2 == 0) rect(x + static_cast<float>(i) * size / 6.0f, y + static_cast<float>(j) * size / 6.0f, size / 6.0f, size / 6.0f, Color(96, 96, 108, 255));
            const float scale = size / static_cast<float>(std::max(atlas_->getWidthProperty(), atlas_->getHeightProperty()));
            sb.Draw(*atlas_, Rectangle(static_cast<int>(x), static_cast<int>(y), static_cast<int>(static_cast<float>(atlas_->getWidthProperty()) * scale), static_cast<int>(static_cast<float>(atlas_->getHeightProperty()) * scale)), Color::White);
            text("the atlas (" + shape_ + ")", x, y + size + 4.0f, Kit::kDim, 0.5f);
        }
        text("DejaVuSans.ttf -> FontDescriptionProcessor -> SpriteFontContent -> ContentCompiler -> XNB -> SpriteFontReader", 24, 232, Kit::kDim, 0.5f);
        text(status_, 24, 256, status_.rfind("built", 0) == 0 ? Kit::Rgba(150, 255, 190) : Kit::Rgba(255, 170, 140), 0.6f);
        text(warnings_, 24, 284, Kit::kDim, 0.5f);
#else
        text("This museum was built without CNA's content pipeline library (-DMUSEUM_PIPELINE=OFF).", 24, 90, Kit::Rgba(255, 190, 150), 0.7f);
        (void)rect;
#endif
        sb.End();
    }

private:
#ifdef MUSEUM_PIPELINE
    void DrawCheck() { drawError_.clear(); }

    void Build()
    {
        font_.reset();
        atlas_.reset();
        warnings_.clear();
        compiled_ = "-";
        try {
            if (!CNA::Content::Pipeline::IsFontRasterizationAvailable()) {
                status_ = "this CNA build has no font rasteriser (FreeType was not found when it was configured)";
                return;
            }
            const auto t0 = std::chrono::steady_clock::now();
            std::shared_ptr<PipeGfx::FontDescription> description;
            if (fromFile_) {
                // The description comes from an XNA .spritefont file; the controls above are ignored. The importer records the file as the
                // description's identity, which is where the processor looks for the typeface.
                PipelineSupport::ImporterContext importerContext;
                Xna::FontDescriptionImporter importer;
                description = importer.Import(S().contentRoot + "/Fonts/Museum.spritefont", importerContext);
                static const char* const styleNames[] = {"Regular", "Bold", "Italic"};
                imported_ = description->getFontNameProperty() + ", " + std::to_string(static_cast<int>(description->getSizeProperty())) + " pt, " + styleNames[std::clamp(static_cast<int>(description->getStyleProperty()), 0, 2)] +
                            ", spacing " + std::to_string(static_cast<int>(description->getSpacingProperty())) + ", kerning " + (description->getUseKerningProperty() ? "on" : "off") + ", " + std::to_string(description->getCharactersProperty().size()) + " characters";
            } else {
                imported_ = "built from the controls";
                description = std::make_shared<PipeGfx::FontDescription>("DejaVuSans", size_, spacing_,
                                                                         style_ == 1 ? PipeGfx::FontDescriptionStyle::Bold : (style_ == 2 ? PipeGfx::FontDescriptionStyle::Italic : PipeGfx::FontDescriptionStyle::Regular), kerning_);
                // The processor looks for DejaVuSans.ttf beside the description's own file.
                description->setIdentityProperty(Xna::ContentIdentity(S().contentRoot + "/Fonts/title.spritefont"));
            }
            if (!fromFile_) {
                std::vector<std::pair<char32_t, char32_t>> ranges;
                switch (regions_) {
                    case 0: ranges = {{32, 126}}; break;
                    case 1: ranges = {{32, 126}, {160, 255}, {0x20AC, 0x20AC}}; break;
                    case 2: ranges = {{32, 126}, {160, 255}, {0x20AC, 0x20AC}, {0x391, 0x3C9}, {0x410, 0x44F}}; break;
                    default: ranges = {{'0', '9'}, {' ', ' '}}; break;
                }
                for (const auto& [first, last] : ranges)
                    for (char32_t c = first; c <= last; ++c) description->getCharactersProperty().insert(static_cast<SharpRuntime::charcs>(c));
                if (defaultChar_) description->setDefaultCharacterProperty(static_cast<SharpRuntime::charcs>('?'));
            }
            ProcessorContext context;
            Processors::FontDescriptionProcessor processor;
            const std::shared_ptr<Processors::SpriteFontContent> output = processor.Process(description, context);
            const CNA::Content::Cnb::CnbSpriteFontData& data = output->Data();
            if (data.atlas.representations.empty() || data.atlas.representations[0].levels.empty()) throw std::runtime_error("the processor returned no atlas");
            const std::vector<std::uint8_t>& rgba = data.atlas.representations[0].levels[0];
            const int w = static_cast<int>(data.atlas.width), h = static_cast<int>(data.atlas.height);
            if (rgba.size() < static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4) throw std::runtime_error("the atlas is not 8 bits per channel RGBA");
            std::vector<Color> pixels(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
            for (std::size_t i = 0; i < pixels.size(); ++i) pixels[i] = Color(rgba[i * 4], rgba[i * 4 + 1], rgba[i * 4 + 2], rgba[i * 4 + 3]);
            atlas_ = std::make_unique<Texture2D>(*S().device, w, h);
            atlas_->SetData(pixels.data(), static_cast<int>(pixels.size()));
            Compiler::ContentCompiler compiler;
            Compiler::CompileOptions options;
            options.assetName = "runtime-font";
            options.targetProfile = GraphicsProfile::HiDef;
            const auto result = compiler.Compile<Processors::SpriteFontContent>(output, options);
            if (!manager_) manager_ = std::make_unique<PipelineSupport::MemoryContentManager>(&S().game->getServicesProperty());
            manager_->Unload();
            manager_->bytes = result.bytes;
            font_ = std::make_unique<SpriteFont>(manager_->Load<SpriteFont>("runtime-font"));
            compiled_ = std::to_string(result.bytes.size()) + " bytes; loaded by SpriteFontReader";
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            char buf[48];
            std::snprintf(buf, sizeof buf, "%.1f ms", ms);
            buildTime_ = buf;
            shape_ = std::to_string(data.characters.size()) + " glyphs, " + std::to_string(w) + " x " + std::to_string(h);
            lineSpacing_ = std::to_string(data.lineSpacing) + " px";
            status_ = std::string("built from DejaVuSans.ttf: ") + std::to_string(data.characters.size()) + " glyphs";
            if (!fromFile_ && style_ != 0) warnings_ = "the file holds only the regular face, so the style is synthesised from it";
        } catch (const std::exception& e) {
            status_ = std::string("could not build: ") + std::string(e.what()).substr(0, 110);
            buildTime_ = "-";
            shape_ = "-";
            lineSpacing_ = "-";
        }
    }

    std::unique_ptr<PipelineSupport::MemoryContentManager> manager_;
    std::unique_ptr<SpriteFont> font_;
    std::unique_ptr<Texture2D> atlas_;
    float size_ = 28.0f, spacing_ = 0.0f;
    int style_ = 0, regions_ = 0, sample_ = 0;
    bool kerning_ = true, defaultChar_ = true, fromFile_ = false;
    std::string drawError_, measured_ = "-", warnings_, imported_ = "-", compiled_ = "-";
#endif
    std::string status_ = "-", shape_ = "-", lineSpacing_ = "-", buildTime_ = "-";
};

}  // namespace

std::unique_ptr<Exhibit> MakeFontBuildLab(const RoomDef& room, Services& services)
{
    return std::make_unique<FontBuildLab>(room, services);
}

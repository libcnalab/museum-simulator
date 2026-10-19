// Compiler laboratory: the last step of XNA's content pipeline, ContentCompiler, writing an .xnb file. A picture is imported and processed
// (TextureProcessor), then compiled with the options a content project offers: target platform, graphics profile and compression. The
// museum shows the file's header bytes, how large it came out, and loads it straight back through a ContentManager that reads the bytes from
// memory (an OpenStream override), drawing the result. Without CNA's pipeline library (-DMUSEUM_PIPELINE=OFF) the room says so.
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "Diagnostics/Log.hpp"
#include "Exhibits/Content/PipelineSupport.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"

#ifdef MUSEUM_PIPELINE
#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Graphics/TextureContent.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Processors/TextureProcessor.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Serialization/Compiler/ContentCompiler.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/TextureImporter.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture3D.hpp"
#include "Microsoft/Xna/Framework/Graphics/TextureCube.hpp"
#include "System/IO/MemoryStream.hpp"
#endif

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

#ifdef MUSEUM_PIPELINE
namespace Xna = Microsoft::Xna::Framework::Content::Pipeline;
namespace PipeGfx = Microsoft::Xna::Framework::Content::Pipeline::Graphics;
namespace Compiler = Microsoft::Xna::Framework::Content::Pipeline::Serialization::Compiler;
namespace Processors = Microsoft::Xna::Framework::Content::Pipeline::Processors;
using PipelineSupport::ImporterContext;
using PipelineSupport::MemoryContentManager;
using PipelineSupport::ProcessorContext;

std::string Kb(std::size_t bytes)
{
    char buffer[40];
    std::snprintf(buffer, sizeof buffer, "%.1f KB (%zu bytes)", static_cast<double>(bytes) / 1024.0, bytes);
    return buffer;
}
#endif

class CompilerLab final : public ScreenExhibit {
public:
    CompilerLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 7.6f) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel("Writing an .xnb", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
#ifdef MUSEUM_PIPELINE
        p.AddChoice("Picture", {"coin.png", "brick.png"}, 0, [this](int v) { source_ = v; dirty_ = true; });
        p.AddChoice("TextureContent type", {"Texture2DContent", "TextureCubeContent", "Texture3DContent"}, 0, [this](int v) { shape_ = v; dirty_ = true; });
        p.AddToggle("TextureProcessor.GenerateMipmaps", false, [this](bool v) { mipmaps_ = v; dirty_ = true; });
        p.AddChoice("CompileOptions.compressContent", {"none", "LZX"}, 0, [this](int v) { compression_ = v; dirty_ = true; });
        p.AddChoice("CompileOptions.targetProfile", {"Reach", "HiDef"}, 0, [this](int v) { hidef_ = v == 1; dirty_ = true; });
        p.AddChoice("CompileOptions.targetPlatform", {"Windows", "WindowsPhone", "Xbox360"}, 0, [this](int v) { platform_ = v; dirty_ = true; });
        p.AddReadout("Processed texture", [this] { return processed_; });
        p.AddReadout("Compiled .xnb", [this] { return size_; });
        p.AddReadout("Root reader", [this] { return reader_; });
        p.AddReadout("ContentCompiler.GetTypeWriter", [this] { return writer_; });
        p.AddReadout("Header", [this] { return header_; });
        p.AddReadout("Loaded back", [this] { return loaded_; });
#else
        p.AddReadout("Content pipeline", [] { return std::string("not built into this museum"); });
#endif
        AddInfoSign(b, "From asset to .xnb",
                    {"The last stage of a content build is the compiler: it writes the processed object with the type writers into an .xnb file whose header says which platform and profile it is for and whether the body is compressed.",
                     "Here that runs in the game. The processed picture can be written as a Texture2D, repeated over six cube faces, or stacked into a four-slice volume. The bytes are handed to the matching runtime reader through a ContentManager; the 2D result is also drawn."});
    }

    ~CompilerLab() override
    {
#ifdef MUSEUM_PIPELINE
        loadedTexture_.reset();
        loadedCube_.reset();
        loadedVolume_.reset();
        manager_.reset();
#endif
    }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
#ifdef MUSEUM_PIPELINE
        if (dirty_) { dirty_ = false; Compile(); }
#endif
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        Text(sb, "TextureContent, compiled and loaded back", 24, 6, 0.8f, Kit::kGold);
#ifdef MUSEUM_PIPELINE
        if (loadedTexture_) sb.Draw(*loadedTexture_, Rectangle(34, 70, 256, 256), Color::White);
        Text(sb, "this picture was read out of the compiled bytes", 34, 336, 0.46f, Kit::kDim);
        float y = 70.0f;
        const auto line = [&](const std::string& s, Color c = Kit::kText, float scale = 0.5f) { Text(sb, s, 320, y, scale, c); y += 21.0f; };
        line("Header (first 32 bytes of the file)", Kit::kGold, 0.55f);
        for (std::size_t row = 0; row < 2; ++row) {
            std::string hex, ascii;
            for (std::size_t i = row * 16; i < row * 16 + 16 && i < bytes_.size(); ++i) {
                char one[8];
                std::snprintf(one, sizeof one, "%02X ", bytes_[i]);
                hex += one;
                ascii.push_back(bytes_[i] >= 32 && bytes_[i] < 127 ? static_cast<char>(bytes_[i]) : '.');
            }
            line(hex + "  " + ascii, Kit::Rgba(190, 235, 200), 0.5f);
        }
        line("bytes 0-2 'XNB', 3 target platform (w, m, x), 4 format version, 5 flags (0x01 HiDef, 0x40 LZ4, 0x80 LZX), 6-9 file size", Kit::kDim, 0.44f);
        y += 8.0f;
        line(size_, Kit::kText, 0.55f);
        line("flags " + flags_, Kit::kText, 0.5f);
        line("root reader " + reader_, Kit::kDim, 0.46f);
        line(loaded_, Kit::Rgba(170, 220, 255), 0.5f);
        if (!error_.empty()) line(error_, Kit::Rgba(255, 190, 150), 0.5f);
#else
        Text(sb, "CNA's content pipeline is not part of this build (-DMUSEUM_PIPELINE=OFF).", 34, 80, 0.6f, Kit::Rgba(255, 190, 150));
#endif
        sb.End();
    }

private:
    void Text(SpriteBatch& sb, const std::string& s, float x, float y, float scale, Color c)
    {
        sb.DrawString(*S().font, s.size() > 110 ? s.substr(0, 107) + "..." : s, Vector2(x, y), c, 0.0f, Vector2(0, 0), scale, SpriteEffects::None, 0.0f);
    }

#ifdef MUSEUM_PIPELINE
    void Compile()
    {
        error_.clear();
        loadedTexture_.reset();
        loadedCube_.reset();
        loadedVolume_.reset();
        try {
            ImporterContext importerContext;
            Xna::TextureImporter importer;
            const std::shared_ptr<PipeGfx::TextureContent> input = importer.Import(S().contentRoot + (source_ == 0 ? "/Textures/coin.png" : "/Textures/brick.png"), importerContext);
            Processors::TextureProcessor processor;
            processor.setGenerateMipmapsProperty(mipmaps_);
            ProcessorContext processorContext;
            const std::shared_ptr<PipeGfx::TextureContent> output = processor.Process(input, processorContext);
            const auto texture = std::dynamic_pointer_cast<PipeGfx::Texture2DContent>(output);
            if (!texture) throw std::runtime_error("the processor did not return a Texture2DContent");
            const auto& chain = *static_cast<const System::Collections::ObjectModel::Collection<std::shared_ptr<PipeGfx::MipmapChain>>&>(texture->getFacesProperty())[0];
            std::size_t raw = 0;
            for (int level = 0; level < chain.getCountProperty(); ++level) raw += chain[level]->GetPixelData().size();
            const std::size_t oneFaceRaw = raw;
            processed_ = std::to_string(chain[0]->getWidthProperty()) + " x " + std::to_string(chain[0]->getHeightProperty()) + ", " + std::to_string(chain.getCountProperty()) + " level(s), ";

            Compiler::ContentCompiler compiler;
            writer_ = compiler.GetTypeWriter(System::Type::From<std::int32_t>()) != nullptr ? "Int32 writer resolved" : "no Int32 writer";
            Compiler::CompileOptions options;
            options.targetPlatform = platform_ == 0 ? Xna::TargetPlatform::Windows : platform_ == 1 ? Xna::TargetPlatform::WindowsPhone : Xna::TargetPlatform::Xbox360;
            options.targetProfile = hidef_ ? GraphicsProfile::HiDef : GraphicsProfile::Reach;
            options.assetName = "compiled";
            options.compressContent = compression_ != 0;
            const auto result = [&] {
                if (shape_ == 0) {
                    processed_ += "one face, " + Kb(raw);
                    return compiler.Compile<PipeGfx::Texture2DContent>(texture, options);
                }
                if (shape_ == 1) {
                    auto cube = std::make_shared<PipeGfx::TextureCubeContent>();
                    for (int face = 0; face < 6; ++face)
                        for (int level = 0; level < chain.getCountProperty(); ++level)
                            std::as_const(cube->getFacesProperty())[face]->Add(chain[level]);
                    cube->Validate(options.targetProfile);
                    raw = oneFaceRaw * 6u;
                    processed_ += "six faces, " + Kb(raw);
                    return compiler.Compile<PipeGfx::TextureCubeContent>(cube, options);
                }
                auto volume = std::make_shared<PipeGfx::Texture3DContent>();
                constexpr int depth = 4;
                raw = 0;
                for (int slice = 0; slice < depth; ++slice) {
                    auto sliceChain = std::make_shared<PipeGfx::MipmapChain>();
                    for (int level = 0; level < chain.getCountProperty(); ++level) {
                        if (slice >= std::max(1, depth >> level)) break;
                        sliceChain->Add(chain[level]);
                        raw += chain[level]->GetPixelData().size();
                    }
                    volume->getFacesProperty().Add(sliceChain);
                }
                volume->Validate(options.targetProfile);
                processed_ += "four slices, " + Kb(raw);
                return compiler.Compile<PipeGfx::Texture3DContent>(volume, options);
            }();
            bytes_ = result.bytes;
            reader_ = result.rootReaderName.substr(0, result.rootReaderName.find(','));
            char ratio[64];
            std::snprintf(ratio, sizeof ratio, ", %.0f%% of the processed pixels", raw > 0 ? 100.0 * static_cast<double>(bytes_.size()) / static_cast<double>(raw) : 0.0);
            size_ = Kb(bytes_.size()) + ratio;
            if (bytes_.size() >= 10) {
                char flags[16];
                std::snprintf(flags, sizeof flags, "0x%02X", bytes_[5]);
                flags_ = std::string(flags) + (bytes_[5] & 0x01 ? " HiDef" : " Reach") + (bytes_[5] & 0x40 ? " LZ4" : "") + (bytes_[5] & 0x80 ? " LZX" : "");
                char head[80];
                std::snprintf(head, sizeof head, "'%c%c%c' platform '%c' v%d flags %s", bytes_[0], bytes_[1], bytes_[2], bytes_[3], bytes_[4], flags);
                header_ = head;
            }
            // Load it back: the manager hands the bytes to the same readers a file would reach.
            if (!manager_) manager_ = std::make_unique<MemoryContentManager>(&S().game->getServicesProperty());
            manager_->bytes = bytes_;
            manager_->Unload();
            if (shape_ == 0) {
                loadedTexture_ = std::make_unique<Texture2D>(manager_->ReadFresh<Texture2D>("compiled"));
                const Texture2D& t = *loadedTexture_;
                loaded_ = "loaded as Texture2D " + std::to_string(t.getWidthProperty()) + " x " + std::to_string(t.getHeightProperty()) + ", " + std::to_string(t.getLevelCountProperty()) + " level(s)";
            } else if (shape_ == 1) {
                loadedCube_ = std::make_unique<TextureCube>(manager_->ReadFresh<TextureCube>("compiled"));
                loaded_ = "loaded as TextureCube, size " + std::to_string(loadedCube_->getSizeProperty()) + ", " + std::to_string(loadedCube_->getLevelCountProperty()) + " level(s)";
            } else {
                loadedVolume_ = manager_->ReadFresh<std::shared_ptr<Texture3D>>("compiled");
                loaded_ = "loaded as Texture3D " + std::to_string(loadedVolume_->getWidthProperty()) + " x " + std::to_string(loadedVolume_->getHeightProperty()) + " x " + std::to_string(loadedVolume_->getDepthProperty()) + ", " + std::to_string(loadedVolume_->getLevelCountProperty()) + " level(s)";
            }
        } catch (const std::exception& e) {
            error_ = std::string("failed: ") + std::string(e.what()).substr(0, 80);
            loaded_ = "not loaded";
            Museum::Log("compiler", "%s", e.what());
        }
    }

    std::unique_ptr<MemoryContentManager> manager_;
    std::unique_ptr<Texture2D> loadedTexture_;
    std::unique_ptr<TextureCube> loadedCube_;
    std::shared_ptr<Texture3D> loadedVolume_;
    std::vector<std::uint8_t> bytes_;
    std::string processed_ = "-", size_ = "-", reader_ = "-", writer_ = "-", header_ = "-", flags_ = "-", loaded_ = "-", error_;
    int source_ = 0, shape_ = 0, compression_ = 0, platform_ = 0;
    bool mipmaps_ = false, hidef_ = false, dirty_ = true;
#endif
};

}  // namespace

std::unique_ptr<Exhibit> MakeCompilerLab(const RoomDef& room, Services& services)
{
    return std::make_unique<CompilerLab>(room, services);
}

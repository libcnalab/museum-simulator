// Pipeline Workshop, mode "material" (room wk-material): XNA's MaterialProcessor on a BasicMaterialContent the room builds, naming
// Content/Workshop/plaque.png (4x4: magenta, red at alpha 128, green, blue, then twelve (64,128,192,255)). MaterialProcessor builds the
// textures its material type uses through context.BuildAsset(..., "TextureProcessor", its own colour-key/format/mipmap/premultiply
// settings), so it needs a context that can build: CNA's XnaBridgeProcessorContext over a coordinator (WorkshopPipeline.hpp). Expected
// values come from XNA 4.0's MaterialProcessor (decompiled reference): the input is returned when its type stays, ChangeMaterialType makes
// a new material of the DefaultEffect's type carrying the name, opaque data and textures; only the type's own texture keys are built;
// an EffectMaterialContent without an effect is refused with InvalidContentException. The built texture is loaded back from the nested
// .xnb and its pixels compared exactly (TextureFormat is set to Color so nothing is block-compressed).
#ifdef MUSEUM_PIPELINE

#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "CNA/Content/Pipeline/XnaPipelineBridge.hpp"
#include "Exhibits/Content/PipelineSupport.hpp"
#include "Exhibits/Content/WorkshopChecks.hpp"
#include "Exhibits/Content/WorkshopPipeline.hpp"
#include "Exhibits/Services.hpp"
#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/ExternalReference.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Graphics/StockMaterials.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/InvalidContentException.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Processors/MaterialProcessor.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Processors/ProcessorEnums.hpp"
#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "System/ArgumentNullException.hpp"

using namespace Microsoft::Xna::Framework;

namespace {

namespace Xna = Microsoft::Xna::Framework::Content::Pipeline;
namespace PipeGfx = Microsoft::Xna::Framework::Content::Pipeline::Graphics;
namespace Processors = Microsoft::Xna::Framework::Content::Pipeline::Processors;
namespace Canon = CNA::Content::Pipeline;
using Workshop::Checks;
using Workshop::F;
using Workshop::Yes;
using TextureRef = std::shared_ptr<Xna::ExternalReference<PipeGfx::TextureContent>>;

struct Outcome {
    std::shared_ptr<PipeGfx::MaterialContent> material;
    std::vector<Canon::ContentAdditionalWriteOutput> nested;
    std::vector<Canon::ContentDependency> dependencies;
};

struct Bench {
    std::shared_ptr<Canon::ContentPipelineRegistry> registry = Workshop::Pipe::StockRegistry();
    Canon::ContentPipeline pipeline{registry};
    std::filesystem::path root, texture;
    Canon::ContentBuildEnvironment environment;

    // One processor run through its own context: XNA names a nested asset per build, so every run starts again at plaque_0.
    Outcome Run(Processors::MaterialProcessor& processor, const std::shared_ptr<PipeGfx::MaterialContent>& input) const
    {
        Canon::ContentSourceRootCapabilities roots;
        Canon::ContentDependencyCollector dependencies;
        Workshop::Pipe::RecordingLogger logger;
        const Canon::ContentProcessorParameters none;
        Canon::ContentProcessorContext canonical(root, texture, "material", "MaterialProcessor", none, roots, dependencies, logger, Canon::ContentOutputFormat::Xnb, environment, &pipeline);
        Canon::XnaBridgeProcessorContext context(canonical);
        Outcome out;
        out.material = processor.Process(input, context);
        out.nested = canonical.NestedOutputs();
        out.dependencies = dependencies.Dependencies();
        return out;
    }

    [[nodiscard]] std::string Built(const std::string& name) const { return (environment.outputDirectory / (name + ".xnb")).generic_string(); }
};

TextureRef Ref(const std::filesystem::path& path) { return std::make_shared<Xna::ExternalReference<PipeGfx::TextureContent>>(path.string()); }

std::shared_ptr<PipeGfx::BasicMaterialContent> Plaque(const Bench& bench)
{
    auto m = std::make_shared<PipeGfx::BasicMaterialContent>();
    m->setNameProperty("Plaque");
    m->setDiffuseColorProperty(Vector3(0.8f, 0.6f, 0.4f));
    m->setEmissiveColorProperty(Vector3(0.05f, 0.0f, 0.0f));
    m->setSpecularColorProperty(Vector3(0.1f, 0.2f, 0.3f));
    m->setSpecularPowerProperty(24.0f);
    m->setAlphaProperty(0.75f);
    m->setVertexColorEnabledProperty(false);
    m->setTextureProperty(Ref(bench.texture));
    m->getTexturesProperty().Add("NormalMap", Ref(bench.texture));   // a key BasicMaterialContent does not build
    return m;
}

std::string TextureName(const PipeGfx::MaterialContent& m, const std::string& key)
{
    TextureRef ref;
    return m.getTexturesProperty().TryGetValue(key, ref) && ref != nullptr ? ref->getFilenameProperty() : "(none)";
}

std::string Rgba(const Color& c)
{
    return "(" + std::to_string(c.getRProperty()) + "," + std::to_string(c.getGProperty()) + "," + std::to_string(c.getBProperty()) + "," + std::to_string(c.getAProperty()) + ")";
}

// The nested .xnb of the built texture, loaded back through a ContentManager.
struct LoadedTexture {
    int width = 0, height = 0, levels = 0;
    bool colourFormat = false;
    std::vector<Color> pixels;
};

LoadedTexture LoadBuilt(Services& s, const std::vector<std::uint8_t>& bytes)
{
    PipelineSupport::MemoryContentManager manager(&s.game->getServicesProperty());
    manager.bytes = bytes;
    const Graphics::Texture2D texture = manager.Load<Graphics::Texture2D>("plaque_0");
    LoadedTexture out;
    out.width = texture.getWidthProperty();
    out.height = texture.getHeightProperty();
    out.levels = texture.getLevelCountProperty();
    out.colourFormat = texture.getFormatProperty() == Graphics::SurfaceFormat::Color;
    out.pixels.resize(static_cast<std::size_t>(out.width * out.height));
    texture.GetData(out.pixels.data(), static_cast<int>(out.pixels.size()));
    return out;
}

}  // namespace

namespace Workshop {

void MaterialProcessorChecks(Checks& checks, Services& services)
{
    Bench bench;
    bench.root = Pipe::PrepareSourceRoot(services.contentRoot, {"plaque.png"});
    bench.texture = bench.root / "plaque.png";
    bench.environment.outputDirectory = Pipe::OutputRoot() / "bin";

    checks.Group("defaults", [&] {
        Processors::MaterialProcessor processor;
        checks.Add("MaterialProcessor defaults", "ColorKeyColor (255,0,255,255), ColorKeyEnabled, GenerateMipmaps, PremultiplyTextureAlpha true; ResizeTexturesToPowerOfTwo false; DxtCompressed; BasicEffect",
                   Rgba(processor.getColorKeyColorProperty()) + ", " + Yes(processor.getColorKeyEnabledProperty()) + ", " + Yes(processor.getGenerateMipmapsProperty()) + ", " + Yes(processor.getPremultiplyTextureAlphaProperty()) + ", " +
                       Yes(processor.getResizeTexturesToPowerOfTwoProperty()) + ", format " + std::to_string(static_cast<int>(processor.getTextureFormatProperty())) + ", effect " + std::to_string(static_cast<int>(processor.getDefaultEffectProperty())),
                   processor.getColorKeyColorProperty() == Color(255, 0, 255, 255) && processor.getColorKeyEnabledProperty() && processor.getGenerateMipmapsProperty() && processor.getPremultiplyTextureAlphaProperty() &&
                       !processor.getResizeTexturesToPowerOfTwoProperty() && processor.getTextureFormatProperty() == Processors::TextureProcessorOutputFormat::DxtCompressed &&
                       processor.getDefaultEffectProperty() == Processors::MaterialProcessorDefaultEffect::BasicEffect);
    });

    checks.Group("BasicEffect", [&] {
        Processors::MaterialProcessor processor;
        processor.setTextureFormatProperty(Processors::TextureProcessorOutputFormat::Color);
        const std::shared_ptr<PipeGfx::BasicMaterialContent> input = Plaque(bench);
        const Outcome out = bench.Run(processor, input);
        checks.Add("Process returns its input when the type stays", "the same BasicMaterialContent object", out.material == input ? "the same object" : "another object", out.material == input);
        const auto basic = std::dynamic_pointer_cast<PipeGfx::BasicMaterialContent>(out.material);
        checks.Add("the material's values survive", "Plaque; diffuse (0.8,0.6,0.4), emissive (0.05,0,0), specular (0.1,0.2,0.3) power 24, alpha 0.75, vertex colour off",
                   basic ? basic->getNameProperty() + ", alpha " + F(basic->getAlphaProperty().value_or(-1.0f)) + ", power " + F(basic->getSpecularPowerProperty().value_or(-1.0f)) : "not a BasicMaterialContent",
                   basic && basic->getNameProperty() == "Plaque" && basic->getDiffuseColorProperty() == Vector3(0.8f, 0.6f, 0.4f) && basic->getEmissiveColorProperty() == Vector3(0.05f, 0.0f, 0.0f) &&
                       basic->getSpecularColorProperty() == Vector3(0.1f, 0.2f, 0.3f) && basic->getSpecularPowerProperty() == 24.0f && basic->getAlphaProperty() == 0.75f && basic->getVertexColorEnabledProperty() == false);
        checks.Add("Textures[\"Texture\"] after processing", "the built asset: <output>/plaque_0.xnb (XNA names a nested build after its source, index 0)", TextureName(*out.material, "Texture"),
                   TextureName(*out.material, "Texture") == bench.Built("plaque_0"));
        checks.Add("Textures[\"NormalMap\"] (not a BasicMaterialContent texture)", "untouched: still the source path", TextureName(*out.material, "NormalMap"), TextureName(*out.material, "NormalMap") == bench.texture.string());
        std::string names;
        for (const auto& n : out.nested) names += (names.empty() ? "" : ", ") + n.logicalName;
        checks.Add("nested assets the processor built", "exactly one: plaque_0, an .xnb", std::to_string(out.nested.size()) + ": " + names,
                   out.nested.size() == 1 && out.nested[0].logicalName == "plaque_0" && out.nested[0].bytes.size() > 10 && out.nested[0].bytes[0] == 'X' && out.nested[0].bytes[1] == 'N' && out.nested[0].bytes[2] == 'B');
        bool dependsOnTexture = false;
        for (const auto& d : out.dependencies)
            if (d.kind == Canon::ContentDependencyKind::SourceFile && d.identity.size() >= 10 && d.identity.compare(d.identity.size() - 10, 10, "plaque.png") == 0) dependsOnTexture = true;
        checks.Add("the material depends on its texture's source", "a SourceFile dependency on plaque.png", dependsOnTexture ? "recorded" : "missing", dependsOnTexture);
        if (out.nested.size() != 1) return;
        const LoadedTexture t = LoadBuilt(services, out.nested[0].bytes);
        checks.Add("the built texture's shape", "4 x 4, SurfaceFormat.Color (TextureFormat = Color), 3 levels (GenerateMipmaps)",
                   std::to_string(t.width) + " x " + std::to_string(t.height) + ", Color " + Yes(t.colourFormat) + ", " + std::to_string(t.levels) + " levels", t.width == 4 && t.height == 4 && t.colourFormat && t.levels == 3);
        if (t.pixels.size() < 5) return;
        // Colour key: magenta becomes transparent black. Premultiplied alpha: (255,0,0,128) -> (128,0,0,128). Opaque pixels unchanged.
        const bool exact = t.pixels[0] == Color(0, 0, 0, 0) && t.pixels[1] == Color(128, 0, 0, 128) && t.pixels[2] == Color(0, 255, 0, 255) && t.pixels[3] == Color(0, 0, 255, 255) && t.pixels[4] == Color(64, 128, 192, 255);
        checks.Add("the built texture's pixels", "(0,0,0,0) (128,0,0,128) (0,255,0,255) (0,0,255,255), then (64,128,192,255)",
                   Rgba(t.pixels[0]) + " " + Rgba(t.pixels[1]) + " " + Rgba(t.pixels[2]) + " " + Rgba(t.pixels[3]) + " " + Rgba(t.pixels[4]), exact);
    });

    checks.Group("settings reach TextureProcessor", [&] {
        Processors::MaterialProcessor processor;
        processor.setTextureFormatProperty(Processors::TextureProcessorOutputFormat::Color);
        processor.setColorKeyEnabledProperty(false);
        processor.setPremultiplyTextureAlphaProperty(false);
        processor.setGenerateMipmapsProperty(false);
        const Outcome out = bench.Run(processor, Plaque(bench));
        if (out.nested.size() != 1) {
            checks.Add("ColorKeyEnabled, PremultiplyTextureAlpha, GenerateMipmaps off", "one nested texture", std::to_string(out.nested.size()) + " nested assets", false);
            return;
        }
        const LoadedTexture t = LoadBuilt(services, out.nested[0].bytes);
        checks.Add("ColorKeyEnabled, PremultiplyTextureAlpha, GenerateMipmaps off", "magenta kept (255,0,255,255), red kept (255,0,0,128), 1 level",
                   t.pixels.size() > 1 ? Rgba(t.pixels[0]) + " " + Rgba(t.pixels[1]) + ", " + std::to_string(t.levels) + " level(s)" : "no pixels",
                   t.pixels.size() > 1 && t.pixels[0] == Color(255, 0, 255, 255) && t.pixels[1] == Color(255, 0, 0, 128) && t.levels == 1);
    });

    // DefaultEffect: XNA's ChangeMaterialType<T> makes a new material of the stock type, copying Name, Identity, OpaqueData and Textures,
    // then builds the textures that type uses (Texture; plus Texture2 for dual texture, EnvironmentMap for the environment map).
    checks.Group("DefaultEffect", [&] {
        struct Case {
            Processors::MaterialProcessorDefaultEffect effect;
            const char* name;
            const char* extraKey;   // a second texture key the type builds, or null
        };
        const Case cases[] = {{Processors::MaterialProcessorDefaultEffect::SkinnedEffect, "SkinnedMaterialContent", nullptr},
                              {Processors::MaterialProcessorDefaultEffect::EnvironmentMapEffect, "EnvironmentMapMaterialContent", "EnvironmentMap"},
                              {Processors::MaterialProcessorDefaultEffect::DualTextureEffect, "DualTextureMaterialContent", "Texture2"},
                              {Processors::MaterialProcessorDefaultEffect::AlphaTestEffect, "AlphaTestMaterialContent", nullptr}};
        for (const Case& c : cases) {
            Processors::MaterialProcessor processor;
            processor.setTextureFormatProperty(Processors::TextureProcessorOutputFormat::Color);
            processor.setDefaultEffectProperty(c.effect);
            const std::shared_ptr<PipeGfx::BasicMaterialContent> input = Plaque(bench);
            if (c.extraKey != nullptr) input->getTexturesProperty().Add(c.extraKey, Ref(bench.texture));
            const Outcome out = bench.Run(processor, input);
            if (out.material == nullptr) {
                checks.Add(std::string("DefaultEffect ") + c.name, std::string("a new ") + c.name, "null", false);
                continue;
            }
            bool typeOk = false;
            Vector3 diffuse(-1, -1, -1);
            switch (c.effect) {
                case Processors::MaterialProcessorDefaultEffect::SkinnedEffect:
                    if (auto m = std::dynamic_pointer_cast<PipeGfx::SkinnedMaterialContent>(out.material)) typeOk = true, diffuse = m->getDiffuseColorProperty().value_or(diffuse);
                    break;
                case Processors::MaterialProcessorDefaultEffect::EnvironmentMapEffect:
                    if (auto m = std::dynamic_pointer_cast<PipeGfx::EnvironmentMapMaterialContent>(out.material)) typeOk = true, diffuse = m->getDiffuseColorProperty().value_or(diffuse);
                    break;
                case Processors::MaterialProcessorDefaultEffect::DualTextureEffect:
                    if (auto m = std::dynamic_pointer_cast<PipeGfx::DualTextureMaterialContent>(out.material)) typeOk = true, diffuse = m->getDiffuseColorProperty().value_or(diffuse);
                    break;
                default:
                    if (auto m = std::dynamic_pointer_cast<PipeGfx::AlphaTestMaterialContent>(out.material)) typeOk = true, diffuse = m->getDiffuseColorProperty().value_or(diffuse);
                    break;
            }
            const bool fresh = out.material != nullptr && out.material != input;
            const bool texturesOk = TextureName(*out.material, "Texture") == bench.Built("plaque_0") && TextureName(*out.material, "NormalMap") == bench.texture.string() &&
                                    (c.extraKey == nullptr || TextureName(*out.material, c.extraKey) == bench.Built("plaque_0"));
            // The same source built the same way twice is one asset under one name.
            checks.Add(std::string("DefaultEffect ") + c.name, std::string("a new ") + c.name + " named Plaque, diffuse copied, Texture" + (c.extraKey ? std::string(" and ") + c.extraKey : std::string()) + " -> plaque_0.xnb (one asset), NormalMap untouched",
                       std::string(typeOk ? "right type" : "wrong type") + (fresh ? ", new object" : ", same object") + ", " + (out.material ? out.material->getNameProperty() : "-") + ", Texture " +
                           std::filesystem::path(TextureName(*out.material, "Texture")).filename().string() + ", " + std::to_string(out.nested.size()) + " nested",
                       typeOk && fresh && out.material->getNameProperty() == "Plaque" && diffuse == Vector3(0.8f, 0.6f, 0.4f) && texturesOk && out.nested.size() == 1);
        }
    });

    checks.Group("refusals", [&] {
        Processors::MaterialProcessor processor;
        auto noEffect = std::make_shared<PipeGfx::EffectMaterialContent>();
        noEffect->setNameProperty("NoEffect");
        checks.ExpectThrow<Xna::InvalidContentException>("an EffectMaterialContent with no Effect", "InvalidContentException (XNA: MaterialProcessorEffectMaterialDoesntHaveEffect)", [&] { (void)bench.Run(processor, noEffect); });
        auto emptyName = std::make_shared<PipeGfx::EffectMaterialContent>();
        emptyName->setNameProperty("EmptyEffectName");
        emptyName->setEffectProperty(std::make_shared<Xna::ExternalReference<PipeGfx::EffectContent>>(std::string()));
        checks.ExpectThrow<Xna::InvalidContentException>("an EffectMaterialContent whose Effect has no file name", "InvalidContentException (XNA: ...ContentFilenameNullOrEmpty)", [&] { (void)bench.Run(processor, emptyName); });
        checks.ExpectThrow<System::ArgumentNullException>("Process(null)", "ArgumentNullException (XNA: input)", [&] { (void)bench.Run(processor, nullptr); });
    });
}

}  // namespace Workshop

#endif  // MUSEUM_PIPELINE

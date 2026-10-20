// Pipeline Workshop, mode "processor" (room wk-processor): a game's own importer and processors, written against XNA's
// ContentImporter<T> and ContentProcessor<TInput, TOutput>, registered with CNA's canonical registry through the public bridge
// (RegisterXnaImporter / RegisterXnaProcessor) and built by CNA's coordinator, so the processor receives CNA's real context
// (XnaBridgeProcessorContext) instead of the museum's own minimal one (PipelineSupport::ProcessorContext, which cannot build anything).
// Inside Process the room's PlaqueProcessor calls every context service and records what it saw:
//     AddDependency (a sidecar), AddOutputFile (a file it wrote beside its source), Convert to XNA's stock MaterialProcessor,
//     BuildAsset (a texture, twice the same way, and a label under an explicit asset name), BuildAndLoadAsset (a label processed by the
//     game's own LabelProcessor, and a texture), and the context's properties (output file name, directories, configuration, platform,
//     profile, processor parameters).
// The build result is then checked: dependencies, deployment files, the nested assets with XNA's generated names, the log, and the
// compiled .xnb loaded back through a ContentManager with the game's reader. Sources are copied into the build tree first.
// Finally XNA's PipelineComponentScanner lists the room's registered components (names, attributes, types, processor parameters).
#ifdef MUSEUM_PIPELINE

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "CNA/Content/Pipeline/XnaPipelineBridge.hpp"
#include "Exhibits/Content/PipelineSupport.hpp"
#include "Exhibits/Content/WorkshopChecks.hpp"
#include "Exhibits/Content/WorkshopPipeline.hpp"
#include "Exhibits/Services.hpp"
#include "Microsoft/Xna/Framework/Content/ContentReader.hpp"
#include "Microsoft/Xna/Framework/Content/ContentTypeReader.hpp"
#include "Microsoft/Xna/Framework/Content/ContentTypeReaderManager.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/ContentImporter.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/ContentImporterAttribute.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/ContentItem.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/ContentProcessor.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/ContentProcessorAttribute.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/ExternalReference.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Graphics/StockMaterials.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Graphics/TextureContent.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/PipelineComponentScanner.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/PipelineException.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/ProcessorParameter.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Processors/ProcessorEnums.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Serialization/Compiler/ContentCompiler.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Serialization/Compiler/ContentTypeWriter.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Serialization/Compiler/ContentWriter.hpp"
#include "Microsoft/Xna/Framework/Game.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/InvalidOperationException.hpp"

using namespace Microsoft::Xna::Framework;

namespace {

namespace Xna = Microsoft::Xna::Framework::Content::Pipeline;
namespace PipeGfx = Microsoft::Xna::Framework::Content::Pipeline::Graphics;
namespace Compiler = Microsoft::Xna::Framework::Content::Pipeline::Serialization::Compiler;
namespace Canon = CNA::Content::Pipeline;
namespace Content = Microsoft::Xna::Framework::Content;
using Workshop::Checks;
using Workshop::Yes;

// ---- the game's content types ---------------------------------------------------------------------------------------------------

class LabelSource final : public Xna::ContentItem {
public:
    static constexpr std::string_view XnaTypeName = "Museum.Workshop.LabelSource";
    std::string text;
    [[nodiscard]] const std::string& GetTypeName() const override
    {
        static const std::string name{XnaTypeName};
        return name;
    }
};

class PlaqueContent final : public Xna::ContentItem {
public:
    static constexpr std::string_view XnaTypeName = "Museum.Workshop.PlaqueContent";
    std::string text, captionLoaded;
    Xna::ExternalReference<PipeGfx::TextureContent> picture;
    Xna::ExternalReference<LabelSource> caption;
    [[nodiscard]] const std::string& GetTypeName() const override
    {
        static const std::string name{XnaTypeName};
        return name;
    }
};

// What PlaqueProcessor saw inside Process (processors are constructed per asset by the bridge, so the record is global).
struct Observed {
    bool ran = false;
    std::string outputFilename, outputDirectory, intermediateDirectory, configuration;
    Xna::TargetPlatform platform = Xna::TargetPlatform::Xbox360;
    Graphics::GraphicsProfile profile = Graphics::GraphicsProfile::Reach;
    std::int32_t edition = 0;
    bool editionInDictionary = false;
    bool convertSameObject = false;
    std::string convertedTexture, convertError;
    std::string picture, pictureAgain, caption, captionLoaded, buildError;
    std::string textureLoaded;
    std::string unknownProcessor, emptyProcessor;
    std::string outsideDependency, relativeDependency;
};
Observed observed;
std::string outsideDependencyPath;   // an existing file outside the build's source root (the original in Content/Workshop)

class LabelImporter final : public Xna::ContentImporter<LabelSource> {
public:
    std::shared_ptr<LabelSource> Import(const std::string& filename, Xna::ContentImporterContext& context) override
    {
        std::ifstream in(filename);
        auto label = std::make_shared<LabelSource>();
        std::getline(in, label->text);
        label->setIdentityProperty(Xna::ContentIdentity(filename, "LabelImporter"));
        label->setNameProperty(std::filesystem::path(filename).stem().string());
        context.getLoggerProperty().LogMessage("label of {0} characters", label->text.size());
        return label;
    }
};

class LabelProcessor final : public Xna::ContentProcessor<LabelSource, LabelSource> {
public:
    [[nodiscard]] std::string getPrefixProperty() const { return prefix_; }
    void setPrefixProperty(std::string value) { prefix_ = std::move(value); }
    static void DescribeParameters(Xna::ProcessorParameterBindings<LabelProcessor>& b) { b.Add<std::string>("Prefix", &LabelProcessor::getPrefixProperty, &LabelProcessor::setPrefixProperty); }

    std::shared_ptr<LabelSource> Process(const std::shared_ptr<LabelSource>& input, Xna::ContentProcessorContext&) override
    {
        auto out = std::make_shared<LabelSource>();
        out->setNameProperty(input->getNameProperty());
        out->text = prefix_;
        for (const char c : input->text) out->text.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        return out;
    }

private:
    std::string prefix_ = "> ";
};

std::string What(const std::exception& e) { return Checks::Short(std::string(typeid(e).name()) + ": " + e.what(), 160); }

class PlaqueProcessor final : public Xna::ContentProcessor<LabelSource, PlaqueContent> {
public:
    [[nodiscard]] std::int32_t getEditionProperty() const { return edition_; }
    void setEditionProperty(std::int32_t value) { edition_ = value; }
    [[nodiscard]] std::string getMaterialTextureProperty() const { return materialTexture_; }
    void setMaterialTextureProperty(std::string value) { materialTexture_ = std::move(value); }
    static void DescribeParameters(Xna::ProcessorParameterBindings<PlaqueProcessor>& b)
    {
        b.Add<std::int32_t>("Edition", &PlaqueProcessor::getEditionProperty, &PlaqueProcessor::setEditionProperty, "Edition", "Which edition of the plaque to build.");
        b.Add<std::string>("MaterialTexture", &PlaqueProcessor::getMaterialTextureProperty, &PlaqueProcessor::setMaterialTextureProperty);
    }

    std::shared_ptr<PlaqueContent> Process(const std::shared_ptr<LabelSource>& input, Xna::ContentProcessorContext& context) override
    {
        Observed& o = observed;
        o = Observed{};
        o.ran = true;
        o.outputFilename = context.getOutputFilenameProperty();
        o.outputDirectory = context.getOutputDirectoryProperty();
        o.intermediateDirectory = context.getIntermediateDirectoryProperty();
        o.configuration = context.getBuildConfigurationProperty();
        o.platform = context.getTargetPlatformProperty();
        o.profile = context.getTargetProfileProperty();
        o.edition = edition_;
        o.editionInDictionary = context.getParametersProperty().ContainsKey("Edition");
        const std::filesystem::path dir = std::filesystem::path(input->getIdentityProperty().getSourceFilenameProperty()).parent_path();

        context.AddDependency((dir / "plaque.notes").string());
        // XNA's BuildCoordinator records any absolute path (outside the root it keeps the absolute path) and refuses a relative one: its
        // GetRelativePath throws ArgumentException (FilenameNotAbsolute), rethrown as PipelineException.
        try {
            context.AddDependency(outsideDependencyPath);
            o.outsideDependency = "accepted";
        } catch (const std::exception& e) {
            o.outsideDependency = "thrown: " + What(e);
        }
        try {
            context.AddDependency("../escape.txt");
            o.relativeDependency = "accepted";
        } catch (const Xna::PipelineException& e) {
            o.relativeDependency = std::string("PipelineException: ") + Checks::Short(e.what(), 100);
        } catch (const std::exception& e) {
            o.relativeDependency = "other: " + What(e);
        }
        {
            std::ofstream legend(dir / "plaque.legend");
            legend << "legend written by PlaqueProcessor\n";
        }
        context.AddOutputFile((dir / "plaque.legend").string());

        // Convert: the material goes through XNA's stock MaterialProcessor, which builds its texture as a nested asset.
        auto material = std::make_shared<PipeGfx::BasicMaterialContent>();
        material->setNameProperty("PlaqueMaterial");
        material->setTextureProperty(std::make_shared<Xna::ExternalReference<PipeGfx::TextureContent>>((dir / materialTexture_).string()));
        try {
            Xna::OpaqueDataDictionary materialParameters;
            materialParameters.SetValue<Xna::Processors::TextureProcessorOutputFormat>("TextureFormat", Xna::Processors::TextureProcessorOutputFormat::Color);
            const std::shared_ptr<PipeGfx::MaterialContent> converted = context.Convert<PipeGfx::MaterialContent, PipeGfx::MaterialContent>(material, "MaterialProcessor", materialParameters);
            o.convertSameObject = converted == material;
            std::shared_ptr<Xna::ExternalReference<PipeGfx::TextureContent>> t;
            if (converted && converted->getTexturesProperty().TryGetValue("Texture", t) && t) o.convertedTexture = t->getFilenameProperty();
        } catch (const std::exception& e) {
            o.convertError = What(e);
        }
        // XNA's XnaContentProcessorContext.Convert: an empty name is ArgumentNullException("processorName"); an unknown one fails
        // ProcessorManager.ValidateProcessorTypes with InvalidOperationException (CantFindProcessor).
        try {
            (void)context.Convert<PipeGfx::MaterialContent, PipeGfx::MaterialContent>(material, "NoSuchProcessor");
            o.unknownProcessor = "no exception";
        } catch (const System::InvalidOperationException& e) {
            o.unknownProcessor = std::string("InvalidOperationException: ") + Checks::Short(e.what(), 100);
        } catch (const std::exception& e) {
            o.unknownProcessor = "other: " + What(e);
        }
        try {
            (void)context.Convert<PipeGfx::MaterialContent, PipeGfx::MaterialContent>(material, "");
            o.emptyProcessor = "no exception";
        } catch (const System::ArgumentNullException& e) {
            o.emptyProcessor = std::string("ArgumentNullException: ") + Checks::Short(e.what(), 100);
        } catch (const std::exception& e) {
            o.emptyProcessor = "other: " + What(e);
        }

        auto plaque = std::make_shared<PlaqueContent>();
        plaque->text = std::to_string(edition_) + ": " + input->text;
        try {
            const Xna::ExternalReference<PipeGfx::TextureContent> png((dir / "plaque.png").string());
            plaque->picture = context.BuildAsset<PipeGfx::TextureContent, PipeGfx::TextureContent>(png, "TextureProcessor");
            o.picture = plaque->picture.getFilenameProperty();
            o.pictureAgain = context.BuildAsset<PipeGfx::TextureContent, PipeGfx::TextureContent>(png, "TextureProcessor").getFilenameProperty();
            const Xna::ExternalReference<LabelSource> captionSource((dir / "caption.label").string());
            plaque->caption = context.BuildAsset<LabelSource, LabelSource>(captionSource, "LabelProcessor", {}, "", "Captions/first");
            o.caption = plaque->caption.getFilenameProperty();
            Xna::OpaqueDataDictionary star;
            star.SetValue<std::string>("Prefix", "* ");
            const std::shared_ptr<LabelSource> loaded = context.BuildAndLoadAsset<LabelSource, LabelSource>(captionSource, "LabelProcessor", star);
            plaque->captionLoaded = loaded ? loaded->text : std::string("(null)");
            o.captionLoaded = plaque->captionLoaded;
        } catch (const std::exception& e) {
            o.buildError = What(e);
        }
        try {
            const Xna::ExternalReference<PipeGfx::TextureContent> png((dir / "plaque.png").string());
            const std::shared_ptr<PipeGfx::TextureContent> texture = context.BuildAndLoadAsset<PipeGfx::TextureContent, PipeGfx::TextureContent>(png, "TextureProcessor");
            const auto t2 = std::dynamic_pointer_cast<PipeGfx::Texture2DContent>(texture);
            o.textureLoaded = t2 ? "Texture2DContent" : (texture ? "another TextureContent" : "null");
        } catch (const std::exception& e) {
            o.textureLoaded = "thrown: " + Checks::Short(std::string(typeid(e).name()) + ": " + e.what(), 400);
        }
        context.getLoggerProperty().LogImportantMessage("plaque edition {0} built", edition_);
        return plaque;
    }

private:
    std::int32_t edition_ = 1;
    std::string materialTexture_ = "plaque_material.png";
};

class LabelWriter final : public Compiler::ContentTypeWriter<LabelSource> {
public:
    [[nodiscard]] std::string GetRuntimeReader(Xna::TargetPlatform) const override { return "Museum.Workshop.LabelSourceReader, Museum"; }

protected:
    void Write(Compiler::ContentWriter& output, const std::shared_ptr<LabelSource>& value) override { output.Write(value->text); }
};

class PlaqueWriter final : public Compiler::ContentTypeWriter<PlaqueContent> {
public:
    [[nodiscard]] std::string GetRuntimeReader(Xna::TargetPlatform) const override { return "Museum.Workshop.PlaqueReader, Museum"; }

protected:
    void Write(Compiler::ContentWriter& output, const std::shared_ptr<PlaqueContent>& value) override
    {
        output.Write(value->text);
        output.WriteExternalReference(value->picture);
        output.WriteExternalReference(value->caption);
        output.Write(value->captionLoaded);
    }
};

// The game's runtime side: an external reference arrives as the referenced asset's name, relative to this asset's folder.
struct Plaque {
    std::string text, picture, caption, captionLoaded;
};

class PlaqueReader final : public Content::ContentTypeReader<Plaque> {
public:
    PlaqueReader() : ContentTypeReader("Museum.Workshop.Plaque") {}

protected:
    Plaque Read(Content::ContentReader& input, std::optional<Plaque>) override
    {
        Plaque p;
        p.text = input.ReadString();
        p.picture = input.ReadString();
        p.caption = input.ReadString();
        p.captionLoaded = input.ReadString();
        return p;
    }
};

bool EndsWith(const std::string& s, const std::string& tail) { return s.size() >= tail.size() && s.compare(s.size() - tail.size(), tail.size(), tail) == 0; }

}  // namespace

namespace Workshop {

void ProcessorContextChecks(Checks& checks, Services& services)
{
    checks.Info("the processor context used here", "-",
                "CNA's XnaBridgeProcessorContext over a ContentPipeline (public, CNA/Content/Pipeline/XnaPipelineBridge.hpp); the museum's own PipelineSupport::ProcessorContext records nothing and refuses BuildAsset/BuildAndLoadAsset");
    const std::filesystem::path root = Pipe::PrepareSourceRoot(services.contentRoot, {"plaque.label", "caption.label", "plaque.png", "plaque.notes"});
    std::filesystem::copy_file(root / "plaque.png", root / "plaque_material.png", std::filesystem::copy_options::overwrite_existing);
    outsideDependencyPath = std::filesystem::absolute(std::filesystem::path(services.contentRoot) / "Workshop" / "plaque.notes").string();

    auto registry = Pipe::StockRegistry();
    Xna::ContentImporterAttribute importerAttribute(".label");
    importerAttribute.setDefaultProcessorProperty("LabelProcessor");
    importerAttribute.setDisplayNameProperty("Label - Museum Workshop");
    Canon::RegisterXnaImporter<LabelImporter>(*registry, "LabelImporter", importerAttribute, "1", "Museum.Workshop");
    Xna::ContentProcessorAttribute processorAttribute;
    processorAttribute.setDisplayNameProperty("Plaque - Museum Workshop");
    Canon::RegisterXnaProcessor<PlaqueProcessor>(*registry, "PlaqueProcessor", processorAttribute, "1", "Museum.Workshop");
    Canon::RegisterXnaProcessor<LabelProcessor>(*registry, "LabelProcessor", Xna::ContentProcessorAttribute{}, "1", "Museum.Workshop");
    auto compiler = std::make_shared<Compiler::ContentCompiler>();
    compiler->AddTypeWriter<LabelWriter>();
    compiler->AddTypeWriter<PlaqueWriter>();
    Canon::RegisterXnaXnbOutput(*registry, compiler, {});   // container options: CNA::Internal::Xnb::XnbFileOptions, defaults, not named
    Canon::ContentPipeline pipeline(registry);

    Canon::ContentBuildRequest request;
    request.sourceRoot = root;
    request.source = "plaque.label";
    request.logicalName = "plaque";
    request.processor = "PlaqueProcessor";
    request.outputFormat = Canon::ContentOutputFormat::Xnb;
    request.parameters.Set("Edition", std::int64_t{3});
    request.environment.outputDirectory = Pipe::OutputRoot() / "bin";
    request.environment.intermediateDirectory = Pipe::OutputRoot() / "obj";
    request.environment.buildConfiguration = "Debug";
    request.environment.targetProfile = Graphics::GraphicsProfile::HiDef;
    Pipe::RecordingLogger logger;
    request.logger = &logger;
    const std::filesystem::path out = request.environment.outputDirectory;
    const auto built = [&](const std::string& name) { return (out / (name + ".xnb")).generic_string(); };

    // First the case where the material handed to Convert and the processor's own BuildAsset name the same picture, built with different
    // parameters (MaterialProcessor passes its colour-key and format settings, the processor none). XNA names nested builds per build:
    // the second distinct build of plaque.png is plaque_1, and the build succeeds.
    {
        Canon::ContentBuildRequest same = request;
        same.parameters.Set("MaterialTexture", std::string("plaque.png"));
        Pipe::RecordingLogger quiet;
        same.logger = &quiet;
        try {
            const Canon::ContentBuildResult r = pipeline.Build(same);
            std::string names;
            for (const auto& n : r.output.additionalOutputs) names += (names.empty() ? "" : ", ") + n.logicalName;
            checks.Add("Convert's material and BuildAsset name the same picture, built differently", "the build succeeds with plaque_0 (material) and plaque_1 (picture)", names,
                       observed.convertedTexture == built("plaque_0") && observed.picture == built("plaque_1"));
        } catch (const std::exception& e) {
            checks.Add("Convert's material and BuildAsset name the same picture, built differently", "the build succeeds with plaque_0 (material) and plaque_1 (picture)", Checks::Short(e.what(), 300), false);
        }
    }

    // Then the full scenario with the material naming its own copy of the picture (plaque_material.png).
    Canon::ContentBuildResult result;
    try {
        result = pipeline.Build(request);
    } catch (const std::exception& e) {
        checks.Add("ContentPipeline.Build(plaque.label, PlaqueProcessor)", "a built asset", Checks::Short(e.what(), 600), false);
        return;
    }
    checks.Add("the build selected the game's components", "LabelImporter -> PlaqueProcessor, .xnb output", result.importer.name + " -> " + result.processor.name + ", writer " + result.writer.name,
               result.importer.name == "LabelImporter" && result.processor.name == "PlaqueProcessor" && result.outputFormat == Canon::ContentOutputFormat::Xnb);
    const Observed& o = observed;
    checks.Add("PlaqueProcessor.Process ran with a context", "ran", o.ran ? "ran" : "did not run", o.ran);
    if (!o.ran) return;

    // The context's properties, as XNA defines them: OutputFilename is the output directory, the asset name and .xnb.
    checks.Add("ContentProcessorContext.OutputFilename", built("plaque"), o.outputFilename, o.outputFilename == built("plaque"));
    checks.Add("OutputDirectory and IntermediateDirectory", "the build's own: WorkshopOutput/bin and WorkshopOutput/obj",
               std::filesystem::path(o.outputDirectory).filename().string() + ", " + std::filesystem::path(o.intermediateDirectory).filename().string(),
               std::filesystem::path(o.outputDirectory) == out && std::filesystem::path(o.intermediateDirectory) == Pipe::OutputRoot() / "obj");
    checks.Add("BuildConfiguration, TargetPlatform, TargetProfile", "Debug, Windows, HiDef",
               o.configuration + ", " + std::to_string(static_cast<int>(o.platform)) + ", " + std::to_string(static_cast<int>(o.profile)),
               o.configuration == "Debug" && o.platform == Xna::TargetPlatform::Windows && o.profile == Graphics::GraphicsProfile::HiDef);
    checks.Add("processor parameter Edition = 3", "the property set before Process, and present in context.Parameters", std::to_string(o.edition) + ", in Parameters " + Yes(o.editionInDictionary),
               o.edition == 3 && o.editionInDictionary);

    // AddDependency and AddOutputFile.
    bool notes = false, primary = false, png = false, caption = false;
    for (const Canon::ContentDependency& d : result.dependencies) {
        if (d.kind == Canon::ContentDependencyKind::PrimarySource && EndsWith(d.identity, "plaque.label")) primary = true;
        if (d.kind == Canon::ContentDependencyKind::SourceFile && EndsWith(d.identity, "plaque.notes")) notes = true;
        if (d.kind == Canon::ContentDependencyKind::SourceFile && EndsWith(d.identity, "plaque.png")) png = true;
        if (d.kind == Canon::ContentDependencyKind::SourceFile && EndsWith(d.identity, "caption.label")) caption = true;
    }
    checks.Add("ContentProcessorContext.AddDependency(plaque.notes)", "a source-file dependency of the asset", notes ? "recorded" : "missing", notes);
    checks.Add("dependencies of the nested builds", "plaque.label primary; plaque.png and caption.label as source files", std::string(primary ? "primary " : "") + (png ? "png " : "") + (caption ? "caption" : ""), primary && png && caption);
    bool legend = false;
    for (const Canon::ContentDeploymentFile& f : result.deploymentFiles)
        if (f.outputPath == "plaque.legend") legend = true;
    checks.Add("ContentProcessorContext.AddOutputFile(plaque.legend)", "deployed beside the asset as plaque.legend", legend ? "a deployment file" : std::to_string(result.deploymentFiles.size()) + " deployment files, none plaque.legend", legend);

    // Convert, BuildAsset, BuildAndLoadAsset.
    checks.Add("Convert<MaterialContent, MaterialContent>(material, \"MaterialProcessor\")", "MaterialProcessor's result: the same object, its Texture now " + built("plaque_material_0"),
               o.convertError.empty() ? std::string(o.convertSameObject ? "same object, " : "another object, ") + o.convertedTexture : o.convertError,
               o.convertError.empty() && o.convertSameObject && o.convertedTexture == built("plaque_material_0"));
    checks.Add("Convert to an unknown processor", "InvalidOperationException (XNA: ProcessorManager.ValidateProcessorTypes, CantFindProcessor)", o.unknownProcessor, o.unknownProcessor.rfind("InvalidOperationException", 0) == 0);
    checks.Add("Convert with an empty processor name", "ArgumentNullException (XNA: processorName)", o.emptyProcessor, o.emptyProcessor.rfind("ArgumentNullException", 0) == 0);
    checks.Add("AddDependency of an absolute path outside the source root", "accepted (XNA's BuildCoordinator keeps an outside path absolute)", o.outsideDependency, o.outsideDependency == "accepted");
    checks.Add("AddDependency of a relative path", "PipelineException (XNA: GetRelativePath refuses a path that is not absolute)", o.relativeDependency, o.relativeDependency.rfind("PipelineException", 0) == 0);
    if (!o.buildError.empty()) checks.Add("BuildAsset / BuildAndLoadAsset", "no exception", o.buildError, false);
    checks.Add("BuildAsset<TextureContent, TextureContent>(plaque.png, \"TextureProcessor\")", built("plaque_0") + " (XNA's generated name: the source's name and index 0)", o.picture, o.picture == built("plaque_0"));
    checks.Add("the same BuildAsset again", "the same asset and name: " + built("plaque_0"), o.pictureAgain, o.pictureAgain == built("plaque_0"));
    checks.Add("BuildAsset(caption.label, \"LabelProcessor\", assetName \"Captions/first\")", built("Captions/first"), o.caption, o.caption == built("Captions/first"));
    checks.Add("BuildAndLoadAsset<LabelSource, LabelSource>(caption.label, \"LabelProcessor\", Prefix \"* \")", "the processed object: \"* CAPTION TEXT\"", o.captionLoaded, o.captionLoaded == "* CAPTION TEXT");
    checks.Add("BuildAndLoadAsset<TextureContent, TextureContent>(plaque.png, \"TextureProcessor\")", "a TextureContent (XNA: TextureProcessor's output is TextureContent, a Texture2DContent here)", o.textureLoaded,
               o.textureLoaded == "Texture2DContent");

    std::vector<std::string> names;
    for (const auto& n : result.output.additionalOutputs) names.push_back(n.logicalName);
    std::string list;
    for (const auto& n : names) list += (list.empty() ? "" : ", ") + n;
    const std::set<std::string> want{"plaque_material_0", "plaque_0", "Captions/first"};
    checks.Add("nested assets published with the plaque", "plaque_material_0, plaque_0, Captions/first: three, each once", list, names.size() == 3 && std::set<std::string>(names.begin(), names.end()) == want);
    std::string refs;
    for (const auto& r : result.runtimeReferences) refs += (refs.empty() ? "" : ", ") + r.logicalName;
    checks.Info("runtime references recorded", "-", refs.empty() ? "none" : refs);

    bool importerMessage = false, important = false;
    for (const Canon::ContentLogMessage& m : result.messages) {
        if (m.component == "LabelImporter" && m.text == "label of 17 characters") importerMessage = true;
        if (m.component == "PlaqueProcessor" && m.level == Canon::ContentLogLevel::Important && m.text == "plaque edition 3 built") important = true;
    }
    checks.Add("the components' log lines", "LabelImporter: \"label of 17 characters\" (LogMessage); PlaqueProcessor: \"plaque edition 3 built\" at Important (LogImportantMessage)",
               std::string(importerMessage ? "importer line" : "no importer line") + ", " + (important ? "important line" : "no important line") + " in " + std::to_string(result.messages.size()) + " messages",
               importerMessage && important);

    // The compiled plaque, read back by the game's reader.
    Content::ContentTypeReaderManager::AddTypeCreator("Museum.Workshop.PlaqueReader", [] { return std::make_unique<PlaqueReader>(); });
    try {
        PipelineSupport::MemoryContentManager manager(&services.game->getServicesProperty());
        manager.bytes.assign(result.output.bytes.begin(), result.output.bytes.end());
        const Plaque p = manager.Load<Plaque>("plaque");
        // XNA's ContentWriter.WriteExternalReference writes the referenced file relative to this asset's folder, without its extension.
        checks.Add("the compiled plaque, loaded back", "\"3: Pipeline Workshop\", references plaque_0 and Captions/first, loaded caption \"* CAPTION TEXT\"",
                   "\"" + p.text + "\", " + p.picture + ", " + p.caption + ", \"" + p.captionLoaded + "\"",
                   p.text == "3: Pipeline Workshop" && p.picture == "plaque_0" && p.caption == "Captions/first" && p.captionLoaded == "* CAPTION TEXT");
    } catch (const std::exception& e) {
        checks.Add("the compiled plaque, loaded back", "the plaque", What(e), false);
    }
    (void)Content::ContentTypeReaderManager::RemoveTypeCreatorEXT("Museum.Workshop.PlaqueReader");

    // The components the room registered, as XNA's PipelineComponentScanner lists them. CNA loads no assemblies: the names Update takes are
    // the catalogs the components were registered under ("Museum.Workshop" here, as the last argument of RegisterXnaImporter/Processor).
    // XNA's contract (PipelineComponentScanner.Update): true when the component set was (re)scanned, false when the same list is
    // already up to date; a list naming something unloadable records an error.
    checks.Group("PipelineComponentScanner", [&] {
        Xna::PipelineComponentScanner scanner(registry);
        const bool first = scanner.Update({"Museum.Workshop"});
        const std::vector<std::string> importers = scanner.getImporterNamesProperty();
        const std::vector<std::string> processors = scanner.getProcessorNamesProperty();
        const auto has = [](const std::vector<std::string>& scanned, const std::string& name) { return std::find(scanned.begin(), scanned.end(), name) != scanned.end(); };
        checks.Add("PipelineComponentScanner(registry).Update({\"Museum.Workshop\"})", "true; importer LabelImporter; processors LabelProcessor and PlaqueProcessor; no errors",
                   std::string(first ? "true" : "false") + "; importers " + std::to_string(importers.size()) + ", processors " + std::to_string(processors.size()) + ", errors " + std::to_string(scanner.getErrorsProperty().size()),
                   first && has(importers, "LabelImporter") && has(processors, "LabelProcessor") && has(processors, "PlaqueProcessor") && scanner.getErrorsProperty().empty());
        std::string all;
        for (const auto& n : importers) all += (all.empty() ? "" : " ") + n;
        all += " |";
        for (const auto& n : processors) all += " " + n;
        checks.Info("the scanned component names", "-", all);
        const auto importer = scanner.getImporterAttributesProperty().find("LabelImporter");
        const bool attribute = importer != scanner.getImporterAttributesProperty().end();
        const std::vector<std::string> extensions = attribute ? importer->second.getFileExtensionsProperty() : std::vector<std::string>{};
        checks.Add("ImporterAttributes[\"LabelImporter\"]", "extensions [.label], DefaultProcessor LabelProcessor, DisplayName \"Label - Museum Workshop\" (its ContentImporterAttribute)",
                   attribute ? (extensions.empty() ? std::string("no extension") : extensions[0]) + ", " + importer->second.getDefaultProcessorProperty() + ", \"" + importer->second.getDisplayNameProperty() + "\"" : "missing",
                   attribute && extensions == std::vector<std::string>{".label"} && importer->second.getDefaultProcessorProperty() == "LabelProcessor" && importer->second.getDisplayNameProperty() == "Label - Museum Workshop");
        const auto output = scanner.getImporterOutputTypesProperty().find("LabelImporter");
        const auto input = scanner.getProcessorInputTypesProperty().find("PlaqueProcessor");
        const auto produced = scanner.getProcessorOutputTypesProperty().find("PlaqueProcessor");
        const bool types = output != scanner.getImporterOutputTypesProperty().end() && input != scanner.getProcessorInputTypesProperty().end() && produced != scanner.getProcessorOutputTypesProperty().end();
        checks.Add("ImporterOutputTypes, ProcessorInputTypes, ProcessorOutputTypes", "LabelImporter -> Museum.Workshop.LabelSource; PlaqueProcessor: Museum.Workshop.LabelSource -> Museum.Workshop.PlaqueContent",
                   types ? output->second + "; " + input->second + " -> " + produced->second : "missing",
                   types && output->second == "Museum.Workshop.LabelSource" && input->second == "Museum.Workshop.LabelSource" && produced->second == "Museum.Workshop.PlaqueContent");
        const auto display = scanner.getProcessorAttributesProperty().find("PlaqueProcessor");
        checks.Add("ProcessorAttributes[\"PlaqueProcessor\"].DisplayName", "Plaque - Museum Workshop", display != scanner.getProcessorAttributesProperty().end() ? display->second.getDisplayNameProperty() : "missing",
                   display != scanner.getProcessorAttributesProperty().end() && display->second.getDisplayNameProperty() == "Plaque - Museum Workshop");
        const auto parameters = scanner.getProcessorParametersProperty().find("PlaqueProcessor");
        std::string edition = "missing";
        bool editionOk = false, materialOk = false;
        if (parameters != scanner.getProcessorParametersProperty().end())
            for (int i = 0; i < parameters->second.getCountProperty(); ++i) {
                const Xna::ProcessorParameter& p = parameters->second[i];
                if (p.getPropertyNameProperty() == "Edition") {
                    edition = p.getPropertyTypeProperty() + ", \"" + p.getDisplayNameProperty() + "\", \"" + p.getDescriptionProperty() + "\"";
                    editionOk = p.getPropertyTypeProperty() == "System.Int32" && p.getDisplayNameProperty() == "Edition" && p.getDescriptionProperty() == "Which edition of the plaque to build." && !p.getIsEnumProperty();
                }
                if (p.getPropertyNameProperty() == "MaterialTexture") materialOk = p.getPropertyTypeProperty() == "System.String";
            }
        checks.Add("ProcessorParameters[\"PlaqueProcessor\"]", "Edition: System.Int32, \"Edition\", \"Which edition of the plaque to build.\"; MaterialTexture: System.String", edition + (materialOk ? "; MaterialTexture System.String" : "; no MaterialTexture"),
                   editionOk && materialOk);
        const bool second = scanner.Update({"Museum.Workshop"});
        checks.Add("Update with the same list again", "false (XNA: AlreadyUpToDate)", second ? "true" : "false", !second);
        const bool third = scanner.Update({"Museum.Workshop", "No.Such.Catalog"});
        std::string errors;
        for (const auto& e : scanner.getErrorsProperty()) errors += (errors.empty() ? "" : " | ") + e;
        checks.Add("Update naming a catalog that does not exist", "true; one error naming No.Such.Catalog (XNA: an unloadable assembly is reported in Errors); LabelImporter still found",
                   std::string(third ? "true" : "false") + "; " + std::to_string(scanner.getErrorsProperty().size()) + " error(s): " + Checks::Short(errors, 120),
                   third && scanner.getErrorsProperty().size() == 1 && errors.find("No.Such.Catalog") != std::string::npos && scanner.getImporterAttributesProperty().count("LabelImporter") == 1);
        Xna::PipelineComponentScanner detached;
        const bool none = detached.Update({});
        checks.Add("a scanner with no registry: Update({})", "nothing found (CNA documents: Update finds nothing until a registry is attached)",
                   std::to_string(detached.getImporterNamesProperty().size()) + " importers, " + std::to_string(detached.getProcessorNamesProperty().size()) + " processors (Update answered " + (none ? "true" : "false") + ")",
                   detached.getImporterNamesProperty().empty() && detached.getProcessorNamesProperty().empty());
    });

    // Outside a coordinator there is nothing to build with: CNA's context says so instead of pretending.
    checks.Group("a context without a coordinator", [&] {
        Canon::ContentSourceRootCapabilities roots;
        Canon::ContentDependencyCollector dependencies;
        Pipe::RecordingLogger quiet;
        const Canon::ContentProcessorParameters none;
        Canon::ContentProcessorContext detached(root, root / "plaque.label", "plaque", "PlaqueProcessor", none, roots, dependencies, quiet, Canon::ContentOutputFormat::Xnb);
        Canon::XnaBridgeProcessorContext view(detached);
        auto material = std::make_shared<PipeGfx::BasicMaterialContent>();
        checks.ExpectThrow<Xna::PipelineException>("Convert through a context with no coordinator", "PipelineException (CNA documents: needs a running pipeline)",
                                                   [&] { (void)view.Convert<PipeGfx::MaterialContent, PipeGfx::MaterialContent>(material, "MaterialProcessor"); });
    });
}

}  // namespace Workshop

#endif  // MUSEUM_PIPELINE

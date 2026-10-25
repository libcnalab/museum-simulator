// Pipeline Workshop, mode "tasks" (room wk-tasks): the MSBuild-task half of XNA's content pipeline, as far as a game links it.
//   * TaskItem: MSBuild's ITaskItem contract (a path and case-insensitive metadata that keeps the spelling it was written with).
//   * ContentTask: a task of the game's own (ShelfTask) using the protected logging helpers; its messages reach an XNA
//     ContentBuildLogger and Execute answers false after an error, as an MSBuild task does.
//   * BuildXact: XNA 4.0's validation (BuildXact.Execute / BuildXactCoordinator.Build): a missing project, a phone target and a project
//     that would need Microsoft's XactBld3.exe are refused with false and an error; no project at all succeeds.
//   * XnaComponentNames: the one table translating a .contentproj's XNA component names into CNA's canonical ones.
//   * ContentCompilerOptions: the build-tool services a registry factory receives, and what their equality compares.
// BuildContent, CleanContent, GetLastOutputs and ContentProject are declared in cna_content_pipeline but defined on top of
// RunContentCompiler, which lives in cna_content_compiler; with that library linked (MUSEUM_CONTENT_COMPILER) they are shown in the
// room wk-contentproject (WorkshopContentProjectChecks.cpp), otherwise this room says they are not linked (docs/CNA_FINDINGS.md).
#ifdef MUSEUM_PIPELINE

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "CNA/Content/Pipeline/ContentCompiler.hpp"
#include "Exhibits/Content/WorkshopChecks.hpp"
#include "Exhibits/Content/WorkshopPipeline.hpp"
#include "Exhibits/Services.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/ContentBuildLogger.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/ContentIdentity.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Tasks/BuildXact.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Tasks/ContentTask.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Tasks/XnaComponentNames.hpp"

namespace {

namespace Xna = Microsoft::Xna::Framework::Content::Pipeline;
namespace Tasks = Microsoft::Xna::Framework::Content::Pipeline::Tasks;
namespace Canon = CNA::Content::Pipeline;
using Workshop::Checks;
using Workshop::Yes;

std::string Join(const std::vector<std::string>& items, const char* separator = ", ")
{
    std::string out;
    for (const std::string& s : items) out += (out.empty() ? "" : separator) + s;
    return out;
}

// An XNA ContentBuildLogger that keeps what it is told, by kind.
class KeepingLogger final : public Xna::ContentBuildLogger {
public:
    void LogImportantMessage(const std::string& message) override { important.push_back(message); }
    void LogMessage(const std::string& message) override { messages.push_back(message); }
    void LogWarning(const std::string&, const Xna::ContentIdentity&, const std::string& message) override { warnings.push_back(message); }
    std::vector<std::string> important, messages, warnings;
};

// A game's own task: it counts the items on a shelf, warns about an empty one and fails on a missing one.
class ShelfTask final : public Tasks::ContentTask {
public:
    std::vector<Tasks::TaskItem> shelf;
    [[nodiscard]] const std::string& GetTypeName() const override
    {
        static const std::string name = "Museum.Workshop.ShelfTask";
        return name;
    }
    bool Execute() override
    {
        bool ok = true;
        for (const Tasks::TaskItem& item : shelf) {
            if (!item.HasMetadata("Count")) {
                LogError("no count for " + item.getItemSpecProperty());
                ok = false;
            } else if (item.GetMetadata("count") == "0") {
                LogWarning(item.getItemSpecProperty() + " is empty");
            } else {
                LogMessage(item.getItemSpecProperty() + ": " + item.GetMetadata("COUNT"));
            }
        }
        return ok;
    }
};

bool Contains(const std::vector<std::string>& list, const std::string& value) { return std::find(list.begin(), list.end(), value) != list.end(); }

std::string Mapping(const Tasks::XnaComponentMapping& m)
{
    return "\"" + m.canonicalName + "\"" + (m.known ? ", known" : ", unknown") + (m.chooseByImportedType ? ", by imported type" : "") + (m.obsoleteMessage.empty() ? "" : ", obsolete") +
           (m.defaults.empty() ? "" : ", " + std::to_string(m.defaults.size()) + " defaults");
}

std::string Lookup(const std::vector<std::pair<std::string, std::string>>& pairs, const std::string& key)
{
    for (const auto& [k, v] : pairs)
        if (k == key) return v;
    return "(none)";
}

}  // namespace

namespace Workshop {

void BuildTaskChecks(Checks& checks, Services& services)
{
#ifdef MUSEUM_CONTENT_COMPILER
    checks.Info("BuildContent, CleanContent, GetLastOutputs, ContentProject", "-", "built and checked in the room wk-contentproject (cna_content_compiler is linked)");
#else
    checks.Info("BuildContent, CleanContent, GetLastOutputs, ContentProject", "-",
                "not run: they are defined over RunContentCompiler (cna_content_compiler), which this build does not link; see docs/CNA_FINDINGS.md");
#endif

    // ---- TaskItem -------------------------------------------------------------------------------------------------------------------
    checks.Group("TaskItem", [&] {
        Tasks::TaskItem item("Textures\\plaque.png");
        item.SetMetadata("Importer", "TextureImporter");
        item.SetMetadata("ProcessorParameters_ColorKeyColor", "255, 0, 255, 255");
        item.SetMetadata("Name", "");
        checks.Add("TaskItem(\"Textures\\\\plaque.png\").ItemSpec", "Textures\\plaque.png", item.getItemSpecProperty(), item.getItemSpecProperty() == "Textures\\plaque.png");
        checks.Add("GetMetadata is case-insensitive, as MSBuild's", "importer and IMPORTER both answer TextureImporter", item.GetMetadata("importer") + ", " + item.GetMetadata("IMPORTER"),
                   item.GetMetadata("importer") == "TextureImporter" && item.GetMetadata("IMPORTER") == "TextureImporter");
        checks.Add("absent metadata", "GetMetadata answers \"\" and HasMetadata false; an empty value is still present", "\"" + item.GetMetadata("Processor") + "\", " + Yes(item.HasMetadata("Processor")) + "; Name present " + Yes(item.HasMetadata("name")),
                   item.GetMetadata("Processor").empty() && !item.HasMetadata("Processor") && item.HasMetadata("name"));
        checks.Add("MetadataNames keeps the spelling written", "Importer, Name, ProcessorParameters_ColorKeyColor (name order)", Join(item.MetadataNames()),
                   item.MetadataNames() == std::vector<std::string>{"Importer", "Name", "ProcessorParameters_ColorKeyColor"});
        // TaskItem::operator== compares the path and every metadata entry (callable since CNA 48a5c75eb: it was "= default" over
        // System::Object, which has no operator==, and so implicitly deleted).
        Tasks::TaskItem copy = item;
        const bool equalBefore = copy == item;
        copy.SetMetadata("importer", "PngImporter");
        checks.Add("SetMetadata under another case replaces the value", "a copy equal before, different after, still one Importer entry (spelling kept: Importer)",
                   std::string(equalBefore ? "equal" : "different") + ", " + (copy == item ? "equal" : "different") + ", " + std::to_string(copy.MetadataNames().size()) + " names, Importer = " + copy.GetMetadata("Importer"),
                   equalBefore && !(copy == item) && copy.MetadataNames().size() == 3 && copy.GetMetadata("Importer") == "PngImporter");
        Tasks::TaskItem moved("elsewhere.png");
        for (const std::string& n : item.MetadataNames()) moved.SetMetadata(n, item.GetMetadata(n));
        checks.Add("TaskItem::operator==", "a copy equal, the same metadata on another path different",
                   std::string(Tasks::TaskItem(item) == item ? "copy equal" : "copy different") + ", " + (moved == item ? "other path equal" : "other path different"),
                   Tasks::TaskItem(item) == item && !(moved == item));
        copy.setItemSpecProperty("plaque.png");
        checks.Add("setItemSpecProperty, GetTypeName", "plaque.png; Microsoft.Build.Framework.ITaskItem's stand-in names itself", copy.getItemSpecProperty() + "; " + copy.GetTypeName(),
                   copy.getItemSpecProperty() == "plaque.png" && !copy.GetTypeName().empty());
    });

    // ---- a game's own ContentTask -------------------------------------------------------------------------------------------------
    checks.Group("ContentTask", [&] {
        ShelfTask task;
        KeepingLogger logger;
        task.SetLoggerEXT(&logger);
        Tasks::TaskItem fossils("fossils"), shells("shells"), coins("coins");
        fossils.SetMetadata("Count", "12");
        shells.SetMetadata("count", "0");
        task.shelf = {fossils, shells, coins};
        const bool ok = task.Execute();
        checks.Add("ShelfTask.Execute with one item missing its count", "false, with the error recorded (a task answers false, it does not throw)",
                   std::string(ok ? "true" : "false") + ", errors: " + Join(task.ErrorsEXT()), !ok && task.ErrorsEXT() == std::vector<std::string>{"no count for coins"});
        checks.Add("MessagesEXT, WarningsEXT", "\"fossils: 12\"; \"shells is empty\" (kept apart from the messages)", Join(task.MessagesEXT()) + "; " + Join(task.WarningsEXT()),
                   task.MessagesEXT() == std::vector<std::string>{"fossils: 12"} && task.WarningsEXT() == std::vector<std::string>{"shells is empty"});
        checks.Add("what reached the XNA ContentBuildLogger", "LogMessage \"fossils: 12\", LogWarning \"shells is empty\", the error as LogImportantMessage",
                   Join(logger.messages) + " | " + Join(logger.warnings) + " | " + Join(logger.important),
                   logger.messages == std::vector<std::string>{"fossils: 12"} && logger.warnings == std::vector<std::string>{"shells is empty"} && logger.important == std::vector<std::string>{"no count for coins"});
        ShelfTask silent;
        silent.SetLoggerEXT(nullptr);
        silent.shelf = {fossils};
        const bool silentOk = silent.Execute();
        checks.Add("a task with a null logger", "Execute true, the message still recorded", std::string(silentOk ? "true" : "false") + ", " + Join(silent.MessagesEXT()),
                   silentOk && silent.MessagesEXT() == std::vector<std::string>{"fossils: 12"} && silent.ErrorsEXT().empty());
    });

    // ---- BuildXact ---------------------------------------------------------------------------------------------------------------
    checks.Group("BuildXact", [&] {
        const std::filesystem::path root = Pipe::PrepareSourceRoot(services.contentRoot, {"workshop.xap", "plaque.label"});
        const std::filesystem::path out = Pipe::OutputRoot() / "xact";
        checks.Add("BuildXact::XnaTypeName", "Microsoft.Xna.Framework.Content.Pipeline.Tasks.BuildXact", std::string(Tasks::BuildXact::XnaTypeName), Tasks::BuildXact::XnaTypeName == "Microsoft.Xna.Framework.Content.Pipeline.Tasks.BuildXact");
        const auto task = [&](const std::string& project, const std::string& platform) {
            Tasks::BuildXact t;
            t.setRootDirectoryProperty(root.string());
            t.setOutputDirectoryProperty(out.string());
            t.setIntermediateDirectoryProperty((Pipe::OutputRoot() / "xact-obj").string());
            t.setTargetPlatformProperty(platform);
            t.setTargetProfileProperty("Reach");
            t.setXnaFrameworkVersionProperty("v4.0");
            t.setBuildConfigurationProperty("Release");
            t.setContentProjectGUIDProperty("{6F7A2D1C-0000-4000-8000-00000000A11E}");
            if (!project.empty()) t.setXactProjectsProperty({Tasks::TaskItem(project)});
            return t;
        };
        Tasks::BuildXact properties = task("workshop.xap", "Windows");
        properties.setRebuildAllProperty(true);
        properties.setLoggerRootDirectoryProperty(root.string());
        checks.Add("BuildXact properties", "each value as set; the three outputs empty before Execute",
                   properties.getTargetPlatformProperty() + ", " + properties.getTargetProfileProperty() + ", " + properties.getXnaFrameworkVersionProperty() + ", " + properties.getBuildConfigurationProperty() + ", RebuildAll " +
                       Yes(properties.getRebuildAllProperty()) + ", " + std::to_string(properties.getXactProjectsProperty().size()) + " project, outputs " + std::to_string(properties.getOutputXactFilesProperty().size() + properties.getRebuiltXactFilesProperty().size() + properties.getIntermediateFilesProperty().size()),
                   properties.getTargetPlatformProperty() == "Windows" && properties.getTargetProfileProperty() == "Reach" && properties.getXnaFrameworkVersionProperty() == "v4.0" && properties.getRebuildAllProperty() &&
                       properties.getXactProjectsProperty().size() == 1 && properties.getLoggerRootDirectoryProperty() == root.string() && properties.getOutputDirectoryProperty() == out.string() &&
                       properties.getContentProjectGUIDProperty() == "{6F7A2D1C-0000-4000-8000-00000000A11E}" && properties.getOutputXactFilesProperty().empty() && properties.getRebuiltXactFilesProperty().empty() &&
                       properties.getIntermediateFilesProperty().empty());

        Tasks::BuildXact none = task("", "Windows");
        checks.Add("Execute with no XACT projects", "true: nothing to build (XNA's loop over XactProjects runs zero times)", std::string(none.Execute() ? "true" : "false"), none.ErrorsEXT().empty());
        Tasks::BuildXact missing = task("missing.xap", "Windows");
        const bool missingOk = missing.Execute();
        checks.Add("Execute with a project that does not exist", "false, the error names missing.xap (XNA: XactProjectFileNotFound)", std::string(missingOk ? "true" : "false") + ": " + Checks::Short(Join(missing.ErrorsEXT()), 120),
                   !missingOk && Join(missing.ErrorsEXT()).find("missing.xap") != std::string::npos);
        Tasks::BuildXact label = task("plaque.label", "Windows");
        const bool labelOk = label.Execute();
        checks.Add("Execute with a file that is not an XACT project", "false, refused before any tool is looked for", std::string(labelOk ? "true" : "false") + ": " + Checks::Short(Join(label.ErrorsEXT()), 120),
                   !labelOk && !label.ErrorsEXT().empty() && Join(label.ErrorsEXT()).find("XactBld3") == std::string::npos);
        Tasks::BuildXact phone = task("workshop.xap", "WindowsPhone");
        const bool phoneOk = phone.Execute();
        checks.Add("Execute for TargetPlatform WindowsPhone", "false (XNA: BuildXactCoordinator.Build throws MobileNoXact)", std::string(phoneOk ? "true" : "false") + ": " + Checks::Short(Join(phone.ErrorsEXT()), 120), !phoneOk && !phone.ErrorsEXT().empty());
        const bool environment = std::getenv("CNA_XACTBLD") != nullptr;
        Tasks::BuildXact real = task("workshop.xap", "Windows");
        checks.Info("HasXactCompilerEXT (CNA_XACTBLD set: " + Yes(environment) + ")", "-", Yes(real.HasXactCompilerEXT()));
        if (real.HasXactCompilerEXT()) return;
        const bool realOk = real.Execute();
        checks.Add("Execute of a valid project without XactBld3.exe", "false, the error names XactBld3 (Microsoft's compiler is not something CNA ships); the outputs stay empty",
                   std::string(realOk ? "true" : "false") + ": " + Checks::Short(Join(real.ErrorsEXT()), 110),
                   !realOk && Join(real.ErrorsEXT()).find("XactBld3") != std::string::npos && real.getOutputXactFilesProperty().empty() && real.getRebuiltXactFilesProperty().empty());
    });

    // ---- XnaComponentNames -------------------------------------------------------------------------------------------------------
    checks.Group("XnaComponentNames", [&] {
        const Tasks::XnaComponentMapping texture = Tasks::MapXnaProcessorName("TextureProcessor");
        checks.Add("MapXnaProcessorName(\"TextureProcessor\")", "known, chosen by the imported type (one XNA processor over three content types), not obsolete; PremultiplyAlpha -> premultiplyAlpha, ColorKeyColor -> colorKey",
                   Mapping(texture) + "; " + Lookup(texture.parameterNames, "PremultiplyAlpha") + ", " + Lookup(texture.parameterNames, "ColorKeyColor"),
                   texture.known && texture.chooseByImportedType && texture.obsoleteMessage.empty() && Lookup(texture.parameterNames, "PremultiplyAlpha") == "premultiplyAlpha" && Lookup(texture.parameterNames, "ColorKeyColor") == "colorKey");
        const Tasks::XnaComponentMapping sprite = Tasks::MapXnaProcessorName("SpriteTextureProcessor");
        const Tasks::XnaComponentMapping model = Tasks::MapXnaProcessorName("ModelTextureProcessor");
        checks.Add("SpriteTextureProcessor and ModelTextureProcessor", "both known and obsolete, each sentence naming its processor and TextureProcessor; different defaults: generateMipmaps false / true",
                   Mapping(sprite) + ", mipmaps " + Lookup(sprite.defaults, "generateMipmaps") + " | " + Mapping(model) + ", mipmaps " + Lookup(model.defaults, "generateMipmaps"),
                   sprite.known && model.known && sprite.obsoleteMessage.find("SpriteTextureProcessor") != std::string::npos && model.obsoleteMessage.find("ModelTextureProcessor") != std::string::npos &&
                       sprite.obsoleteMessage.find("TextureProcessor (Texture") != std::string::npos && Lookup(sprite.defaults, "generateMipmaps") == "false" && Lookup(model.defaults, "generateMipmaps") == "true");
        const Tasks::XnaComponentMapping pass = Tasks::MapXnaProcessorName("PassThroughProcessor");
        const Tasks::XnaComponentMapping qualified = Tasks::MapXnaProcessorName("Microsoft.Xna.Framework.Content.Pipeline.Processors.ModelProcessor");
        const Tasks::XnaComponentMapping plain = Tasks::MapXnaProcessorName("ModelProcessor");
        checks.Add("PassThroughProcessor; ModelProcessor with and without its namespace", "known with no canonical name (the graph reaches it unnamed); the same mapping both ways",
                   Mapping(pass) + "; " + Mapping(qualified) + " = " + Mapping(plain), pass.known && pass.canonicalName.empty() && qualified.known && qualified.canonicalName == plain.canonicalName && !plain.canonicalName.empty());
        const Tasks::XnaComponentMapping unknown = Tasks::MapXnaProcessorName("PlaqueProcessor");
        checks.Add("MapXnaProcessorName(\"PlaqueProcessor\") (never an XNA component)", "unknown, no canonical name, a reason (XNA: Cannot find processor)", Mapping(unknown) + ": " + Checks::Short(unknown.reason, 90),
                   !unknown.known && unknown.canonicalName.empty() && !unknown.reason.empty());
        const Tasks::XnaComponentMapping song = Tasks::MapXnaImporterName("Mp3Importer", "SongProcessor");
        const Tasks::XnaComponentMapping effect = Tasks::MapXnaImporterName("Mp3Importer", "SoundEffectProcessor");
        checks.Add("MapXnaImporterName(\"Mp3Importer\", SongProcessor / SoundEffectProcessor)", "two different canonical importers: a streaming song source, then decoded PCM", Mapping(song) + " | " + Mapping(effect),
                   song.known && effect.known && !song.canonicalName.empty() && !effect.canonicalName.empty() && song.canonicalName != effect.canonicalName);
        const Tasks::XnaComponentMapping xml = Tasks::MapXnaImporterName("XmlImporter");
        checks.Add("MapXnaImporterName(\"XmlImporter\")", "known, no canonical name needed", Mapping(xml), xml.known && xml.canonicalName.empty());
        const std::vector<std::string> importers = Tasks::KnownXnaImporterNames();
        const std::vector<std::string> processors = Tasks::KnownXnaProcessorNames();
        checks.Add("KnownXnaImporterNames, KnownXnaProcessorNames", "sorted; TextureImporter, XImporter, FbxImporter, WavImporter; TextureProcessor, ModelProcessor, SoundEffectProcessor",
                   std::to_string(importers.size()) + " importers, " + std::to_string(processors.size()) + " processors" + (std::is_sorted(importers.begin(), importers.end()) && std::is_sorted(processors.begin(), processors.end()) ? ", sorted" : ", unsorted"),
                   std::is_sorted(importers.begin(), importers.end()) && std::is_sorted(processors.begin(), processors.end()) && Contains(importers, "TextureImporter") && Contains(importers, "XImporter") &&
                       Contains(importers, "FbxImporter") && Contains(importers, "WavImporter") && Contains(processors, "TextureProcessor") && Contains(processors, "ModelProcessor") && Contains(processors, "SoundEffectProcessor"));
    });

    // ---- ContentCompilerOptions ---------------------------------------------------------------------------------------------------
    checks.Group("ContentCompilerOptions", [&] {
        const Canon::ContentCompilerOptions defaults;
        checks.Add("ContentCompilerOptions defaults", "buildConfiguration Release; no effect compiler, launcher, XMA encoder, arguments or font directories",
                   defaults.buildConfiguration + "; " + (defaults.effectCompilerExecutable.empty() && defaults.effectCompilerLauncher.empty() && defaults.xmaEncoderExecutable.empty() && defaults.xmaEncoderLauncher.empty() && defaults.xmaEncoderArguments.empty() && defaults.fontDirectories.empty() ? "all empty" : "something set"),
                   defaults.buildConfiguration == "Release" && defaults.effectCompilerExecutable.empty() && defaults.effectCompilerLauncher.empty() && defaults.xmaEncoderExecutable.empty() && defaults.xmaEncoderLauncher.empty() &&
                       defaults.xmaEncoderArguments.empty() && defaults.fontDirectories.empty());
        // The container options (xnbContainer) are CNA::Internal::Xnb::XnbFileOptions, an internal type the museum does not name, so the
        // documented exclusion of the container from operator== is not exercised here.
        Canon::ContentCompilerOptions same = defaults;
        Canon::ContentCompilerOptions debug = defaults;
        debug.buildConfiguration = "Debug";
        Canon::ContentCompilerOptions fonts = defaults;
        fonts.fontDirectories = {services.contentRoot + "/Fonts"};
        Canon::ContentCompilerOptions xma = defaults;
        xma.xmaEncoderArguments = {"{input}", "{output}", "{quality}"};
        Canon::ContentCompilerOptions fxc = defaults;
        fxc.effectCompilerExecutable = "/opt/fxc/fxc.exe";
        fxc.effectCompilerLauncher = "wine";
        checks.Add("ContentCompilerOptions::operator== compares the build-tool services", "a copy equal; Debug, a font directory, XMA arguments or an effect compiler make them differ",
                   std::string(same == defaults ? "equal" : "different") + "; " + (debug == defaults ? "equal" : "different") + ", " + (fonts == defaults ? "equal" : "different") + ", " + (xma == defaults ? "equal" : "different") + ", " +
                       (fxc == defaults ? "equal" : "different"),
                   same == defaults && !(debug == defaults) && !(fonts == defaults) && !(xma == defaults) && !(fxc == defaults));
        // A registry factory as RunContentCompiler would call it: the effect compiler choice reaches it through the options.
        const Canon::ContentPipelineRegistryFactory factory = [](const Canon::ContentCompilerOptions& options) {
            auto registry = std::make_shared<Canon::ContentPipelineRegistry>();
            if (options.buildConfiguration == "Debug") registry->DocumentAbsentWriter(Canon::ContentOutputFormat::Cnb, "Museum.Workshop.Debug", "a debug build writes no .cnb for this type");
            return registry;
        };
        const auto made = factory(debug);
        checks.Add("a ContentPipelineRegistryFactory called with Debug options", "a fresh, mutable registry configured from the options",
                   std::string(made && !made->IsFrozen() ? "mutable" : "frozen or null") + ", " + (made ? Checks::Short(made->AbsentWriterReason(Canon::ContentOutputFormat::Cnb, "Museum.Workshop.Debug"), 60) : "-"),
                   made && !made->IsFrozen() && made->AbsentWriterReason(Canon::ContentOutputFormat::Cnb, "Museum.Workshop.Debug") == "a debug build writes no .cnb for this type");
    });
}

}  // namespace Workshop

#endif  // MUSEUM_PIPELINE

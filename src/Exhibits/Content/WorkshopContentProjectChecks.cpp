// Pipeline Workshop, mode "contentproject" (room wk-contentproject): an XNA content project built the way XNA's MSBuild targets build it,
// and CNA's own command-line coordinator embedded in the game. It needs cna_content_compiler (MUSEUM_CONTENT_COMPILER).
//   * ContentProject: Content/Workshop/workshop.contentproj read (properties, a decidable Condition, Compile and None items), the
//     reasons a project cannot be built, and the constructs the reader refuses.
//   * BuildContent (XNA 4.0 Tasks.BuildContent): the project's three assets built to .xnb; a second build with nothing changed rebuilds
//     nothing (XNA: RebuiltContentFiles empty); a changed source rebuilds only that asset; RebuildAll rebuilds all; a missing asset and a
//     pipeline assembly are refused while the rest still builds. The built files are loaded back.
//   * GetLastOutputs and CleanContent: what the last build left, and a clean that removes exactly the files the build owns.
//   * RunContentCompiler and RegisterBuiltInContentPipeline: the cna-content coordinator called with a registry factory (build, a
//     no-op rebuild, clean), its exit codes and refusals.
// Every source is copied into the build tree first; every output goes to WorkshopOutput/ next to the executable.
#if defined(MUSEUM_PIPELINE) && defined(MUSEUM_CONTENT_COMPILER)

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "CNA/Content/Pipeline/ContentBuildManifest.hpp"
#include "CNA/Content/Pipeline/ContentCompiler.hpp"
#include "Exhibits/Content/WorkshopChecks.hpp"
#include "Exhibits/Content/WorkshopPipeline.hpp"
#include "Exhibits/Services.hpp"
#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/InvalidContentException.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Tasks/BuildContent.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Tasks/CleanContent.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Tasks/ContentProject.hpp"
#include "Microsoft/Xna/Framework/Curve.hpp"
#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

using namespace Microsoft::Xna::Framework;

namespace {

namespace Fs = std::filesystem;
namespace Xna = Microsoft::Xna::Framework::Content::Pipeline;
namespace Tasks = Microsoft::Xna::Framework::Content::Pipeline::Tasks;
namespace Canon = CNA::Content::Pipeline;
using Microsoft::Xna::Framework::Content::ContentManager;
using Workshop::Checks;
using Workshop::F;
using Workshop::Yes;

std::string Join(const std::vector<std::string>& items, const char* separator = ", ")
{
    std::string out;
    for (const std::string& s : items) out += (out.empty() ? "" : separator) + s;
    return out;
}

// The file names (relative to the output directory, sorted) of a list of task items.
std::vector<std::string> Names(const std::vector<Tasks::TaskItem>& items, const Fs::path& out)
{
    std::vector<std::string> names;
    for (const Tasks::TaskItem& item : items) names.push_back(Fs::path(item.getItemSpecProperty()).lexically_relative(out).generic_string());
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> FilesIn(const Fs::path& dir)
{
    std::vector<std::string> names;
    std::error_code error;
    if (!Fs::exists(dir, error)) return names;
    for (const auto& entry : Fs::recursive_directory_iterator(dir))
        if (entry.is_regular_file()) names.push_back(entry.path().lexically_relative(dir).generic_string());
    std::sort(names.begin(), names.end());
    return names;
}

std::map<std::string, Fs::file_time_type> Times(const Fs::path& out, const std::vector<std::string>& names)
{
    std::map<std::string, Fs::file_time_type> times;
    for (const std::string& n : names) times[n] = Fs::last_write_time(out / n);
    return times;
}

bool Inside(const std::vector<Tasks::TaskItem>& items, const Fs::path& root)
{
    for (const Tasks::TaskItem& item : items) {
        const std::string rel = Fs::path(item.getItemSpecProperty()).lexically_relative(root).generic_string();
        if (rel.empty() || rel.rfind("..", 0) == 0) return false;
    }
    return true;
}

void WriteText(const Fs::path& path, const std::string& text)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
}

const std::vector<std::string> kBuilt{"plaque.xnb", "tone8.xnb", "vector.xnb"};

}  // namespace

namespace Workshop {

void ContentProjectChecks(Checks& checks, Services& services)
{
    const Fs::path base = Pipe::OutputRoot() / "project";
    Fs::remove_all(base);
    const Fs::path root = base / "source", out = base / "bin", obj = base / "obj";
    Fs::create_directories(root);
    for (const char* name : {"workshop.contentproj", "plaque.png", "tone8.wav", "vector.xml", "plaque.notes", "curve.cnj", "unknown.cnj"})
        Fs::copy_file(Fs::path(services.contentRoot) / "Workshop" / name, root / name, Fs::copy_options::overwrite_existing);
    const Fs::path titleRoot = Pipe::OutputRoot().parent_path();

    // ---- ContentProject ------------------------------------------------------------------------------------------------------------
    Tasks::ContentProject project;
    bool loaded = false;
    checks.Group("ContentProject", [&] {
        project = Tasks::ContentProject::Load((root / "workshop.contentproj").string());
        loaded = true;
        checks.Add("ContentProject::Load(workshop.contentproj): DirectoryEXT and properties", "the project's own folder; XnaPlatform Windows, XnaProfile HiDef, the GUID, Configuration Debug (from the decidable Condition)",
                   Fs::path(project.DirectoryEXT()).filename().string() + "; " + project.Property("XnaPlatform") + ", " + project.Property("XnaProfile") + ", " + project.Property("ProjectGuid") + ", " + project.Property("Configuration"),
                   Fs::equivalent(Fs::path(project.DirectoryEXT()), root) && project.Property("XnaPlatform") == "Windows" && project.Property("XnaProfile") == "HiDef" &&
                       project.Property("ProjectGuid") == "{6F7A2D1C-0000-4000-8000-00000000C0DE}" && project.Property("Configuration") == "Debug");
        std::vector<std::string> sources, copied;
        for (const auto& item : project.SourceAssets()) sources.push_back(item.include + "=" + item.Get("name") + "/" + item.Get("IMPORTER") + "/" + item.Get("processor"));
        for (const auto& item : project.CopiedFiles()) copied.push_back(item.kind + " " + item.include);
        checks.Add("Items, SourceAssets, CopiedFiles", "4 items; Compile plaque.png, tone8.wav, vector.xml with Name/Importer/Processor (metadata names case-insensitive); None plaque.notes",
                   std::to_string(project.Items().size()) + " items; " + Join(sources) + "; " + Join(copied),
                   project.Items().size() == 4 &&
                       sources == std::vector<std::string>{"plaque.png=plaque/TextureImporter/TextureProcessor", "tone8.wav=tone8/WavImporter/SoundEffectProcessor", "vector.xml=vector/XmlImporter/PassThroughProcessor"} &&
                       copied == std::vector<std::string>{"None plaque.notes"});
        const std::vector<std::string> unroutable = project.UnroutableEXT();
        checks.Add("UnroutableEXT() of a project of XNA's own components", "empty: every asset routes (TextureProcessor, SoundEffectProcessor, XmlImporter, PassThroughProcessor)",
                   unroutable.empty() ? "empty" : Checks::Short(Join(unroutable, " | "), 200), unroutable.empty());
        const Tasks::ContentProject custom = Tasks::ContentProject::Parse(
            R"(<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003"><ItemGroup>)"
            R"(<Compile Include="plaque.label"><Name>plaque</Name><Importer>LabelImporter</Importer><Processor>PlaqueProcessor</Processor></Compile>)"
            R"(</ItemGroup></Project>)",
            "custom.contentproj");
        const std::vector<std::string> reasons = custom.UnroutableEXT();
        checks.Add("UnroutableEXT() of a project naming a game's own importer and processor", "two reasons, one naming LabelImporter, one naming PlaqueProcessor, both found at once",
                   std::to_string(reasons.size()) + ": " + Checks::Short(Join(reasons, " | "), 160),
                   reasons.size() == 2 && Join(reasons).find("LabelImporter") != std::string::npos && Join(reasons).find("PlaqueProcessor") != std::string::npos);
        checks.ExpectThrow<Xna::InvalidContentException>("ContentProject::Parse of text that is not XML", "InvalidContentException", [] { (void)Tasks::ContentProject::Parse("not a project", "broken.contentproj"); });
        checks.ExpectThrow<Xna::InvalidContentException>("an ItemGroup whose Condition the reader cannot decide", "InvalidContentException (refused by name rather than guessed at)", [] {
            (void)Tasks::ContentProject::Parse(R"x(<Project><ItemGroup Condition="Exists('plaque.png')"><Compile Include="plaque.png" /></ItemGroup></Project>)x", "conditional.contentproj");
        });
        checks.ExpectThrow<Xna::InvalidContentException>("ContentProject::Load of a missing file", "InvalidContentException", [&] { (void)Tasks::ContentProject::Load((root / "missing.contentproj").string()); });
    });
    if (!loaded) return;

    // ---- BuildContent ------------------------------------------------------------------------------------------------------------
    checks.Add("BuildContent::CancelEventNameFormat", "XNA's own: Local\\Microsoft.Xna.GameStudio.ContentPipeline.CancelBuildEvent+{0}", Tasks::BuildContent::CancelEventNameFormat,
               Tasks::BuildContent::CancelEventNameFormat == "Local\\Microsoft.Xna.GameStudio.ContentPipeline.CancelBuildEvent+{0}");
    std::map<std::string, Fs::file_time_type> firstTimes;
    checks.Group("BuildContent: first build", [&] {
        Tasks::BuildContent task = project.ToBuildContentEXT(out.string(), obj.string());
        checks.Add("ToBuildContentEXT(out, obj)", "RootDirectory the project's folder, OutputDirectory/IntermediateDirectory as given, Windows, HiDef, Debug, the GUID, 3 source assets, not compressed",
                   task.getTargetPlatformProperty() + ", " + task.getTargetProfileProperty() + ", " + task.getBuildConfigurationProperty() + ", " + std::to_string(task.getSourceAssetsProperty().size()) + " assets, compress " + Yes(task.getCompressContentProperty()),
                   Fs::equivalent(Fs::path(task.getRootDirectoryProperty()), root) && task.getOutputDirectoryProperty() == out.string() && task.getIntermediateDirectoryProperty() == obj.string() && task.getTargetPlatformProperty() == "Windows" &&
                       task.getTargetProfileProperty() == "HiDef" && task.getBuildConfigurationProperty() == "Debug" && task.getContentProjectGUIDProperty() == "{6F7A2D1C-0000-4000-8000-00000000C0DE}" &&
                       task.getSourceAssetsProperty().size() == 3 && !task.getCompressContentProperty());
        checks.Add("the outputs before Execute", "OutputContentFiles, RebuiltContentFiles, IntermediateFiles empty",
                   std::to_string(task.getOutputContentFilesProperty().size() + task.getRebuiltContentFilesProperty().size() + task.getIntermediateFilesProperty().size()) + " items",
                   task.getOutputContentFilesProperty().empty() && task.getRebuiltContentFilesProperty().empty() && task.getIntermediateFilesProperty().empty());
        const bool ok = task.Execute();
        const std::vector<std::string> outputs = Names(task.getOutputContentFilesProperty(), out), rebuilt = Names(task.getRebuiltContentFilesProperty(), out);
        checks.Add("BuildContent.Execute: the first build", "true; OutputContentFiles = RebuiltContentFiles = plaque.xnb, tone8.xnb, vector.xnb",
                   std::string(ok ? "true" : "false: " + Checks::Short(Join(task.ErrorsEXT()), 120)) + "; outputs " + Join(outputs) + "; rebuilt " + Join(rebuilt), ok && outputs == kBuilt && rebuilt == kBuilt);
        bool metadata = !task.getOutputContentFilesProperty().empty();
        for (const Tasks::TaskItem& item : task.getOutputContentFilesProperty())
            metadata = metadata && Fs::path(item.getItemSpecProperty()).stem().string() == item.GetMetadata("Name") && !item.GetMetadata("SourceAsset").empty();
        checks.Add("every output item carries Name and SourceAsset metadata, and lies under the output directory in the build tree", "yes", Yes(metadata && Inside(task.getOutputContentFilesProperty(), out) && Inside(task.getOutputContentFilesProperty(), titleRoot)),
                   metadata && Inside(task.getOutputContentFilesProperty(), out) && Inside(task.getOutputContentFilesProperty(), titleRoot));
        bool configuration = false;
        for (const Tasks::TaskItem& item : task.getIntermediateFilesProperty())
            if (Fs::path(item.getItemSpecProperty()).parent_path() == obj) configuration = true;
        checks.Add("IntermediateFiles", "includes the build configuration written to the intermediate directory (not into the source tree)", Join(Names(task.getIntermediateFilesProperty(), base)), configuration);
        std::vector<std::string> sourceFiles = FilesIn(root);
        checks.Add("the source folder after the build", "unchanged: the 7 copied files and nothing else", Join(sourceFiles), sourceFiles.size() == 7);
        firstTimes = Times(out, kBuilt);

        ContentManager manager(&services.game->getServicesProperty(), out.string());
        const Graphics::Texture2D texture = manager.Load<Graphics::Texture2D>("plaque");
        const Vector3 v = manager.Load<Vector3>("vector");
        checks.Add("the built plaque.xnb and vector.xnb, loaded", "a 4x4 texture; (1.5, -2, 3.25)",
                   std::to_string(texture.getWidthProperty()) + "x" + std::to_string(texture.getHeightProperty()) + "; (" + F(v.X) + ", " + F(v.Y) + ", " + F(v.Z) + ")",
                   texture.getWidthProperty() == 4 && texture.getHeightProperty() == 4 && v == Vector3(1.5f, -2.0f, 3.25f));
    });
    if (firstTimes.empty()) return;

    checks.Group("BuildContent: incremental builds", [&] {
        Tasks::BuildContent again = project.ToBuildContentEXT(out.string(), obj.string());
        const bool ok = again.Execute();
        const std::map<std::string, Fs::file_time_type> times = Times(out, kBuilt);
        checks.Add("a second build with nothing changed", "true; OutputContentFiles the same three; RebuiltContentFiles empty (XNA: a null build); no output file rewritten",
                   std::string(ok ? "true" : "false") + "; outputs " + Join(Names(again.getOutputContentFilesProperty(), out)) + "; rebuilt [" + Join(Names(again.getRebuiltContentFilesProperty(), out)) + "]; " +
                       (times == firstTimes ? "files untouched" : "files rewritten"),
                   ok && Names(again.getOutputContentFilesProperty(), out) == kBuilt && again.getRebuiltContentFilesProperty().empty() && times == firstTimes);

        WriteText(root / "vector.xml", "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<XnaContent>\n  <Asset Type=\"Microsoft.Xna.Framework.Vector3\">4 5 6</Asset>\n</XnaContent>\n");
        Tasks::BuildContent changed = project.ToBuildContentEXT(out.string(), obj.string());
        const bool changedOk = changed.Execute();
        const std::map<std::string, Fs::file_time_type> after = Times(out, kBuilt);
        checks.Add("vector.xml changed, then built", "true; RebuiltContentFiles = vector.xnb only; plaque.xnb and tone8.xnb not rewritten",
                   std::string(changedOk ? "true" : "false") + "; rebuilt [" + Join(Names(changed.getRebuiltContentFilesProperty(), out)) + "]; plaque " + (after.at("plaque.xnb") == firstTimes.at("plaque.xnb") ? "untouched" : "rewritten") + ", tone8 " +
                       (after.at("tone8.xnb") == firstTimes.at("tone8.xnb") ? "untouched" : "rewritten"),
                   changedOk && Names(changed.getRebuiltContentFilesProperty(), out) == std::vector<std::string>{"vector.xnb"} && after.at("plaque.xnb") == firstTimes.at("plaque.xnb") && after.at("tone8.xnb") == firstTimes.at("tone8.xnb"));
        ContentManager manager(&services.game->getServicesProperty(), out.string());
        const Vector3 v = manager.Load<Vector3>("vector");
        checks.Add("the rebuilt vector.xnb, loaded", "(4, 5, 6)", "(" + F(v.X) + ", " + F(v.Y) + ", " + F(v.Z) + ")", v == Vector3(4.0f, 5.0f, 6.0f));

        Tasks::BuildContent all = project.ToBuildContentEXT(out.string(), obj.string());
        all.setRebuildAllProperty(true);
        const bool allOk = all.Execute();
        checks.Add("RebuildAll", "true; all three rebuilt", std::string(allOk ? "true" : "false") + "; rebuilt " + Join(Names(all.getRebuiltContentFilesProperty(), out)),
                   allOk && all.getRebuildAllProperty() && Names(all.getRebuiltContentFilesProperty(), out) == kBuilt);

        Tasks::GetLastOutputs last;
        last.setOutputDirectoryEXT(out.string());
        last.setIntermediateDirectoryProperty(obj.string());
        const bool lastOk = last.Execute();
        checks.Add("GetLastOutputs (the manifest the build left)", "true; plaque.xnb, tone8.xnb, vector.xnb", std::string(lastOk ? "true" : "false") + "; " + Join(Names(last.getOutputContentFilesProperty(), out)),
                   lastOk && Names(last.getOutputContentFilesProperty(), out) == kBuilt);
        Tasks::GetLastOutputs unbuilt;
        unbuilt.setIntermediateDirectoryProperty((base / "never-built").string());
        const bool unbuiltOk = unbuilt.Execute();
        checks.Add("GetLastOutputs where nothing was built", "true with an empty list (no previous build is not a failure)", std::string(unbuiltOk ? "true" : "false") + ", " + std::to_string(unbuilt.getOutputContentFilesProperty().size()) + " items",
                   unbuiltOk && unbuilt.getOutputContentFilesProperty().empty());
    });

    checks.Group("BuildContent: refusals", [&] {
        Tasks::BuildContent missing = project.ToBuildContentEXT(out.string(), obj.string());
        std::vector<Tasks::TaskItem> assets = missing.getSourceAssetsProperty();
        Tasks::TaskItem ghost("missing.png");
        ghost.SetMetadata("Importer", "TextureImporter");
        ghost.SetMetadata("Processor", "TextureProcessor");
        assets.push_back(ghost);
        missing.setSourceAssetsProperty(assets);
        const bool missingOk = missing.Execute();
        checks.Add("a project naming a source that does not exist", "false, the error names missing.png; the other three are still reported built",
                   std::string(missingOk ? "true" : "false") + ": " + Checks::Short(Join(missing.ErrorsEXT(), " | "), 120) + "; outputs " + Join(Names(missing.getOutputContentFilesProperty(), out)),
                   !missingOk && Join(missing.ErrorsEXT()).find("missing.png") != std::string::npos && Names(missing.getOutputContentFilesProperty(), out) == kBuilt);
        Tasks::BuildContent assemblies = project.ToBuildContentEXT(out.string(), obj.string());
        assemblies.setPipelineAssembliesProperty({Tasks::TaskItem("Museum.CustomPipeline.dll")});
        const bool assembliesOk = assemblies.Execute();
        checks.Add("PipelineAssemblies named", "false with a message saying where a custom component is registered instead (C++ loads no assemblies)",
                   std::string(assembliesOk ? "true" : "false") + ": " + Checks::Short(Join(assemblies.ErrorsEXT(), " | "), 120), !assembliesOk && Join(assemblies.ErrorsEXT()).find("RegisterXnaImporter") != std::string::npos);
        Tasks::BuildContent empty;
        const bool emptyOk = empty.Execute();
        checks.Add("BuildContent with no RootDirectory or OutputDirectory", "false, with an error", std::string(emptyOk ? "true" : "false") + ": " + Checks::Short(Join(empty.ErrorsEXT()), 100), !emptyOk && !empty.ErrorsEXT().empty());
    });

    // ---- CleanContent --------------------------------------------------------------------------------------------------------------
    checks.Group("CleanContent", [&] {
        WriteText(out / "visitor-notes.txt", "not the pipeline's\n");
        const std::vector<std::string> before = FilesIn(out);
        Tasks::CleanContent clean;
        clean.setOutputDirectoryProperty(out.string());
        clean.setIntermediateDirectoryProperty(obj.string());
        clean.setRootDirectoryProperty(root.string());
        clean.setTargetPlatformProperty("Windows");
        clean.setTargetProfileProperty("HiDef");
        clean.setBuildConfigurationProperty("Debug");
        clean.setContentProjectGUIDProperty("{6F7A2D1C-0000-4000-8000-00000000C0DE}");
        checks.Add("CleanContent properties and XnaTypeName", "as set; Microsoft.Xna.Framework.Content.Pipeline.Tasks.CleanContent",
                   clean.getTargetPlatformProperty() + ", " + clean.getTargetProfileProperty() + ", " + clean.getBuildConfigurationProperty() + "; " + std::string(Tasks::CleanContent::XnaTypeName),
                   clean.getOutputDirectoryProperty() == out.string() && clean.getIntermediateDirectoryProperty() == obj.string() && clean.getRootDirectoryProperty() == root.string() && clean.getTargetPlatformProperty() == "Windows" &&
                       clean.getTargetProfileProperty() == "HiDef" && clean.getBuildConfigurationProperty() == "Debug" && clean.getContentProjectGUIDProperty() == "{6F7A2D1C-0000-4000-8000-00000000C0DE}" &&
                       Tasks::CleanContent::XnaTypeName == "Microsoft.Xna.Framework.Content.Pipeline.Tasks.CleanContent");
        const bool ok = clean.Execute();
        const std::vector<std::string> left = FilesIn(out);
        std::vector<std::string> removed;
        std::set_difference(before.begin(), before.end(), left.begin(), left.end(), std::back_inserter(removed));
        const bool removedBuilt = std::includes(removed.begin(), removed.end(), kBuilt.begin(), kBuilt.end());
        bool onlyOwned = true;
        for (const std::string& r : removed)
            if (r != "plaque.xnb" && r != "tone8.xnb" && r != "vector.xnb" && r != Canon::ContentBuildManifestFileName && r != ".cna-content.lock") onlyOwned = false;
        checks.Add("CleanContent.Execute: what it removes", "true; plaque.xnb, tone8.xnb, vector.xnb and the build's manifest removed, nothing that is not the pipeline's",
                   std::string(ok ? "true" : "false") + "; removed " + Join(removed), ok && removedBuilt && onlyOwned);
        // XNA's CleanContent deletes the outputs and its own cache file (BuildItemCollection.DeleteStaleOutputFiles, SafeDeleteFile of
        // CacheFilename): the output directory is left with only what the project did not build.
        checks.Add("CleanContent.Execute: what it leaves", "visitor-notes.txt only (no file of the pipeline's own left behind)", Join(left), left == std::vector<std::string>{"visitor-notes.txt"});
        checks.Add("the build configuration in the intermediate directory", "removed by the clean", Fs::exists(obj / "cna-buildcontent.json") ? "still there" : "removed", !Fs::exists(obj / "cna-buildcontent.json"));
        Tasks::CleanContent twice;
        twice.setOutputDirectoryProperty(out.string());
        const bool twiceOk = twice.Execute();
        const std::vector<std::string> afterTwice = FilesIn(out);
        checks.Add("CleanContent again", "true, visitor-notes.txt still kept", std::string(twiceOk ? "true" : "false") + "; left " + Join(afterTwice),
                   twiceOk && std::find(afterTwice.begin(), afterTwice.end(), "visitor-notes.txt") != afterTwice.end());
        Tasks::CleanContent absent;
        absent.setOutputDirectoryProperty((base / "never-built").string());
        checks.Add("CleanContent of a directory that does not exist", "true: nothing to clean is not a failure", absent.Execute() ? "true" : "false", absent.ErrorsEXT().empty());
    });

    // ---- RunContentCompiler and RegisterBuiltInContentPipeline ---------------------------------------------------------------------
    checks.Group("RunContentCompiler", [&] {
        const Fs::path cnjRoot = base / "cnj", cnjOut = base / "cnj-bin";
        Fs::create_directories(cnjRoot);
        Fs::copy_file(root / "curve.cnj", cnjRoot / "curve.cnj", Fs::copy_options::overwrite_existing);
        int calls = 0;
        Canon::ContentCompilerOptions seen;
        const Canon::ContentPipelineRegistryFactory factory = [&](const Canon::ContentCompilerOptions& options) {
            ++calls;
            seen = options;
            auto registry = std::make_shared<Canon::ContentPipelineRegistry>();
            Canon::RegisterBuiltInContentPipeline(*registry, options);
            return registry;
        };
        {
            Canon::ContentPipelineRegistry registry;
            Canon::RegisterBuiltInContentPipeline(registry, {});
            std::string have;
            for (const char* ext : {".cnj", ".png", ".wav", ".mp3", ".wmv", ".gltf", ".fxb", ".fx", ".xnb", ".xml", ".x", ".fbx", ".spritefont"})
                if (registry.HasImporterForSource(std::string("a") + ext)) have += std::string(have.empty() ? "" : " ") + ext;
            checks.Add("RegisterBuiltInContentPipeline(registry, {})", "a source route for every built-in extension, .fx included without an effect compiler",
                       have, have == ".cnj .png .wav .mp3 .wmv .gltf .fxb .fx .xnb .xml .x .fbx .spritefont");
        }
        const int first = Canon::RunContentCompiler({"build", cnjRoot, "-o", cnjOut, "--build-configuration", "Debug", "--quiet"}, factory);
        checks.Add("RunContentCompiler(build curve.cnj, factory)", "0; the factory called once with buildConfiguration Debug; curve.cnb and the manifest written",
                   std::to_string(first) + "; " + std::to_string(calls) + " call(s), " + seen.buildConfiguration + "; " + Join(FilesIn(cnjOut)),
                   first == 0 && calls == 1 && seen.buildConfiguration == "Debug" && Fs::exists(cnjOut / "curve.cnb") && Fs::exists(cnjOut / Canon::ContentBuildManifestFileName));
        if (first != 0) return;
        const Fs::file_time_type stamp = Fs::last_write_time(cnjOut / "curve.cnb");
        const int second = Canon::RunContentCompiler({"build", cnjRoot, "-o", cnjOut, "--build-configuration", "Debug", "--quiet"}, factory);
        checks.Add("the same command again", "0, the factory called again (no registry is shared between calls), curve.cnb not rewritten",
                   std::to_string(second) + ", " + std::to_string(calls) + " calls, " + (Fs::last_write_time(cnjOut / "curve.cnb") == stamp ? "untouched" : "rewritten"),
                   second == 0 && calls == 2 && Fs::last_write_time(cnjOut / "curve.cnb") == stamp);
        {
            ContentManager manager(&services.game->getServicesProperty(), cnjOut.string());
            const Curve curve = manager.Load<Curve>("curve");
            checks.Add("the coordinator's curve.cnb, loaded", "Evaluate(2) = 7.5", F(curve.Evaluate(2.0f)), curve.Evaluate(2.0f) == 7.5f);
        }
        std::ifstream manifestFile(cnjOut / Canon::ContentBuildManifestFileName, std::ios::binary);
        const Canon::ContentBuildManifest manifest = Canon::ContentBuildManifest::Parse(std::string(std::istreambuf_iterator<char>(manifestFile), std::istreambuf_iterator<char>()));
        checks.Add("the coordinator's manifest", "one record, curve, whose output digest is ContentFileSha256(curve.cnb)",
                   std::to_string(manifest.Entries().size()) + " record(s)", manifest.Entries().size() == 1 && manifest.Find("curve") && !manifest.Find("curve")->outputs.empty() &&
                                                                         manifest.Find("curve")->outputs.front().sha256 == Canon::ContentFileSha256(cnjOut / "curve.cnb"));
        const int clean = Canon::RunContentCompiler({"clean", cnjOut, "--quiet"}, factory);
        checks.Add("RunContentCompiler(clean)", "0; curve.cnb removed", std::to_string(clean) + "; left " + Join(FilesIn(cnjOut)), clean == 0 && !Fs::exists(cnjOut / "curve.cnb"));

        Fs::copy_file(root / "unknown.cnj", cnjRoot / "unknown.cnj", Fs::copy_options::overwrite_existing);
        const int failing = Canon::RunContentCompiler({"build", cnjRoot, "-o", cnjOut, "--quiet"}, factory);
        checks.Add("a source tree with a .cnj the compiler refuses", "1 (a build failed)", std::to_string(failing), failing == 1);
        const int syntax = Canon::RunContentCompiler({"build", "--no-such-option"}, factory);
        checks.Add("invalid command-line syntax", "2", std::to_string(syntax), syntax == 2);
        checks.ExpectThrow<std::invalid_argument>("RunContentCompiler with a null registry", "std::invalid_argument",
                                                  [&] { (void)Canon::RunContentCompiler({"build", cnjRoot, "-o", cnjOut, "--quiet"}, std::shared_ptr<Canon::ContentPipelineRegistry>{}); });
        checks.ExpectThrow<std::invalid_argument>("RunContentCompiler with an empty factory", "std::invalid_argument",
                                                  [&] { (void)Canon::RunContentCompiler({"build", cnjRoot, "-o", cnjOut, "--quiet"}, Canon::ContentPipelineRegistryFactory{}); });
        auto prebuilt = std::make_shared<Canon::ContentPipelineRegistry>();
        Canon::RegisterBuiltInContentPipeline(*prebuilt, {});
        const int refused = Canon::RunContentCompiler({"build", cnjRoot, "-o", cnjOut, "--fx-compiler", "/opt/fxc/fxc.exe", "--quiet"}, prebuilt);
        checks.Add("a pre-built registry with --fx-compiler", "refused (non-zero): the option cannot reach a registry built before the command line", std::to_string(refused), refused != 0);
    });
}

}  // namespace Workshop

#endif  // MUSEUM_PIPELINE && MUSEUM_CONTENT_COMPILER

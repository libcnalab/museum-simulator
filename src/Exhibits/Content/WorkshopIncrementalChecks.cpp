// Pipeline Workshop, mode "incremental" (room wk-incremental): what an incremental CNA build decides with, through the public API the
// coordinator itself uses (RunContentCompiler lives in the cna-content tool library, which the museum does not link):
//   * a per-asset build configuration (.cna-content.json) parsed, queried, refused when malformed, and applied to a build;
//   * manifest records made from two real builds (curve.cnj, texture.cnj with its plaque.png sidecar), their fingerprints, and the
//     manifest serialized and parsed back;
//   * the incremental decision: unchanged sources keep every fingerprint; a changed sidecar moves only the texture's, and its
//     fingerprint state says which domain moved; a changed parameter moves the parameter domain; a content-build dependency needs the
//     dependency's own fingerprint;
//   * a failing dependency: the sidecar deleted, refused by the fingerprint and by the build;
//   * CompileCnjToCnb (absorbed sidecars, refused types), the headless source imports (image with a colour key, WAV in its two forms),
//     and opt-in .cnb chunk compression with its two size guards.
// Sources are copied into the build tree first and restored afterwards; nothing is written into the museum's source tree.
#ifdef MUSEUM_PIPELINE

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include "CNA/Content/Cnb/CnbChunkCompression.hpp"
#include "CNA/Content/Cnb/CnbDocument.hpp"
#include "CNA/Content/Cnb/CnbFormat.hpp"
#include "CNA/Content/Cnb/CnbSourceImport.hpp"
#include "CNA/Content/Cnb/CnjToCnb.hpp"
#include "CNA/Content/Pipeline/CnjContentPipeline.hpp"
#include "CNA/Content/Pipeline/ContentBuildConfiguration.hpp"
#include "CNA/Content/Pipeline/ContentBuildManifest.hpp"
#include "CNA/Content/Pipeline/ContentPipeline.hpp"
#include "CNA/Content/Pipeline/Texture2DContentPipeline.hpp"
#include "CNA/Content/Pipeline/TextureCompressionPipeline.hpp"
#include "Exhibits/Content/WorkshopChecks.hpp"
#include "Exhibits/Content/WorkshopPipeline.hpp"
#include "Exhibits/Services.hpp"
#include "Microsoft/Xna/Framework/Content/ContentLoadException.hpp"
#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/Curve.hpp"
#include "Microsoft/Xna/Framework/Game.hpp"

using namespace Microsoft::Xna::Framework;

namespace {

namespace Canon = CNA::Content::Pipeline;
namespace Cnb = CNA::Content::Cnb;
using Microsoft::Xna::Framework::Content::ContentLoadException;
using Microsoft::Xna::Framework::Content::ContentManager;
using Workshop::Checks;
using Workshop::F;
using Workshop::Yes;

std::vector<std::uint8_t> ReadBytes(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void WriteBytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

std::string Head(const std::string& digest) { return digest.size() > 12 ? digest.substr(0, 12) + "..." : (digest.empty() ? "(empty)" : digest); }

bool Hex64(const std::string& s)
{
    if (s.size() != 64) return false;
    for (const char c : s)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}

// The configuration document the room parses: an asset whose route is spelled out and whose logical name is moved, an asset with its
// own container and typed parameters, a project-wide container and one external source root.
const char* const kConfiguration = R"({"format":"CNA.ContentPipeline.Config","version":1,"outputFormat":"cnb",
"sourceRoots":{"shared":"../Shared"},
"assets":{
  "tone.mp3":{"outputFormat":"xnb","parameters":{"name":{"type":"string","value":"Workshop tone"},"durationMs":{"type":"u64","value":"500"}}},
  "curve.cnj":{"logicalName":"Curves/workshop","importer":"CNA.CnjImporter","processor":"CNA.CurveProcessor","writer":"CNA.CurveContentWriter"}
}})";

std::string Replace(std::string text, const std::string& from, const std::string& to)
{
    const std::size_t at = text.find(from);
    if (at != std::string::npos) text.replace(at, from.size(), to);
    return text;
}

// The same manifest claiming the previous format version: the digit after the first "version" key, 9 -> 8.
// The same manifest one version older than CNA's current ContentBuildManifestVersion (whatever that is).
std::string OlderVersion(std::string text)
{
    const std::size_t key = text.find("\"version\"");
    if (key == std::string::npos) return text;
    const std::size_t first = text.find_first_of("0123456789", key);
    if (first == std::string::npos) return text;
    const std::size_t last = text.find_first_not_of("0123456789", first);
    text.replace(first, last - first, std::to_string(Canon::ContentBuildManifestVersion - 1u));
    return text;
}

}  // namespace

namespace Workshop {

void IncrementalBuildChecks(Checks& checks, Services& services)
{
    const std::filesystem::path root = Pipe::PrepareSourceRoot(services.contentRoot, {"curve.cnj", "texture.cnj", "plaque.png", "unknown.cnj", "tone8.wav", "loop16.wav", "deep24.wav"});
    const std::filesystem::path out = Pipe::OutputRoot() / "incremental";
    std::filesystem::remove_all(out);
    std::filesystem::create_directories(out);
    const std::vector<std::uint8_t> originalPng = ReadBytes(root / "plaque.png");

    auto registry = std::make_shared<Canon::ContentPipelineRegistry>();
    Canon::RegisterTexture2DContentPipeline(*registry, Canon::MakeBlockCompressionTextureEncoder());
    Canon::RegisterCnjContentPipeline(*registry);
    const Canon::ContentPipeline pipeline(registry);
    const auto request = [&](const std::string& source, const std::string& logicalName) {
        Canon::ContentBuildRequest r;
        r.sourceRoot = root;
        r.source = source;
        r.logicalName = logicalName;
        r.environment.outputDirectory = out;
        return r;
    };
    ContentManager manager(&services.game->getServicesProperty(), out.string());

    // ---- the per-asset build configuration -----------------------------------------------------------------------------------------
    checks.Group("ContentBuildConfiguration", [&] {
        checks.Add("ContentBuildConfigurationVersion, ContentBuildConfigurationFileName", "1, \".cna-content.json\"",
                   std::to_string(Canon::ContentBuildConfigurationVersion) + ", \"" + Canon::ContentBuildConfigurationFileName + "\"",
                   Canon::ContentBuildConfigurationVersion == 1u && std::string(Canon::ContentBuildConfigurationFileName) == ".cna-content.json");
        const Canon::ContentBuildConfiguration config = Canon::ContentBuildConfiguration::Parse(kConfiguration, root / Canon::ContentBuildConfigurationFileName);
        std::string order;
        for (const auto& [source, entry] : config.Entries()) order += (order.empty() ? "" : ", ") + source;
        checks.Add("ContentBuildConfiguration::Parse: Entries() in source-path order", "curve.cnj, tone.mp3", order, order == "curve.cnj, tone.mp3");
        const Canon::ContentAssetBuildConfiguration* curve = config.Find("curve.cnj");
        checks.Add("Find(\"curve.cnj\")", "logical name Curves/workshop, CNA.CnjImporter, CNA.CurveProcessor, CNA.CurveContentWriter, no container of its own",
                   curve ? curve->logicalName + ", " + curve->importer + ", " + curve->processor + ", " + curve->writer + (curve->outputFormat ? ", own container" : ", no container of its own") : "null",
                   curve && curve->logicalName == "Curves/workshop" && curve->importer == "CNA.CnjImporter" && curve->processor == "CNA.CurveProcessor" && curve->writer == "CNA.CurveContentWriter" && !curve->outputFormat);
        const Canon::ContentAssetBuildConfiguration* tone = config.Find("tone.mp3");
        const Canon::ContentProcessorParameterValue* name = tone ? tone->parameters.Find("name") : nullptr;
        const Canon::ContentProcessorParameterValue* duration = tone ? tone->parameters.Find("durationMs") : nullptr;
        const bool typed = name && std::holds_alternative<std::string>(*name) && std::get<std::string>(*name) == "Workshop tone" && duration && std::holds_alternative<std::uint64_t>(*duration) &&
                           std::get<std::uint64_t>(*duration) == 500u;
        checks.Add("Find(\"tone.mp3\"): its own container and typed parameters", "outputFormat xnb; name = string \"Workshop tone\", durationMs = u64 500 (numbers are written as strings)",
                   tone ? std::string(tone->outputFormat ? Canon::ContentOutputFormatName(*tone->outputFormat) : "none") + "; parameters " + (typed ? "typed as written" : "not as written") : "null",
                   tone && tone->outputFormat == Canon::ContentOutputFormat::Xnb && typed);
        const std::filesystem::path* shared = config.SourceRoots().Find("shared");
        checks.Add("OutputFormat(), SourceRoots(), Find(an unconfigured source)", "cnb; alias shared = ../Shared as authored; null",
                   std::string(config.OutputFormat() ? Canon::ContentOutputFormatName(*config.OutputFormat()) : "none") + "; " + (shared ? "shared = " + shared->generic_string() : "no alias") + "; " + (config.Find("texture.cnj") ? "an entry" : "null"),
                   config.OutputFormat() == Canon::ContentOutputFormat::Cnb && shared && shared->generic_string() == "../Shared" && config.Find("texture.cnj") == nullptr);

        // The configuration applied to a build, as the coordinator applies it.
        Canon::ContentBuildRequest configured = request("curve.cnj", curve->logicalName);
        configured.importer = curve->importer;
        configured.processor = curve->processor;
        configured.writer = curve->writer;
        const Canon::ContentBuildResult built = pipeline.Build(configured);
        WriteBytes(out / (built.logicalName + ".cnb"), built.output.bytes);
        const Curve loaded = manager.Load<Curve>("Curves/workshop");
        checks.Add("the configured curve, built and loaded as Curves/workshop", "logical name Curves/workshop; Evaluate(2) = 7.5", built.logicalName + "; " + F(loaded.Evaluate(2.0f)),
                   built.logicalName == "Curves/workshop" && loaded.Evaluate(2.0f) == 7.5f);

        checks.ExpectThrow<std::runtime_error>("Parse: an unknown root field", "std::runtime_error (unknown fields are refused)", [] { (void)Canon::ContentBuildConfiguration::Parse(Replace(kConfiguration, "\"version\":1", "\"version\":1,\"colour\":\"red\"")); });
        checks.ExpectThrow<std::runtime_error>("Parse: an asset path leaving the root", "std::runtime_error (unsafe paths are refused)", [] { (void)Canon::ContentBuildConfiguration::Parse(Replace(kConfiguration, "\"curve.cnj\"", "\"../escape.cnj\"")); });
        checks.ExpectThrow<std::runtime_error>("Parse: a parameter whose value does not fit its type", "std::runtime_error (invalid typed parameter value)", [] { (void)Canon::ContentBuildConfiguration::Parse(Replace(kConfiguration, "\"value\":\"500\"", "\"value\":\"five hundred\"")); });
        checks.ExpectThrow<std::runtime_error>("Parse: version 2", "std::runtime_error (only version 1 is understood)", [] { (void)Canon::ContentBuildConfiguration::Parse(Replace(kConfiguration, "\"version\":1", "\"version\":2")); });
    });

    // ---- manifest records from two real builds ------------------------------------------------------------------------------------
    Canon::ContentBuildManifestEntry curveEntry, textureEntry;
    const auto entryFor = [&](const std::string& source, const std::string& logicalName) {
        const Canon::ContentBuildResult r = pipeline.Build(request(source, logicalName));
        const std::filesystem::path published = out / (logicalName + ".cnb");
        WriteBytes(published, r.output.bytes);
        Canon::ContentBuildManifestEntry e = Canon::MakeContentBuildManifestEntry(r, root, out, published);
        Canon::RefreshContentBuildDirectFingerprint(e, root);
        Canon::RefreshContentBuildEffectiveFingerprint(e);
        return e;
    };
    Canon::ContentBuildManifest manifest;
    checks.Group("manifest records", [&] {
        checks.Add("ContentSha256(\"abc\")", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad (FIPS 180-2)", Canon::ContentSha256({'a', 'b', 'c'}),
                   Canon::ContentSha256({'a', 'b', 'c'}) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
        curveEntry = entryFor("curve.cnj", "curve");
        textureEntry = entryFor("texture.cnj", "texture");
        const Canon::ContentBuildManifestOutput* o = textureEntry.outputs.empty() ? nullptr : &textureEntry.outputs.front();
        checks.Add("MakeContentBuildManifestEntry(texture build)", "node texture, source texture.cnj, one output texture.cnb whose sha256 = ContentFileSha256 of the published file",
                   o ? textureEntry.nodeId + ", " + textureEntry.source + ", " + o->path + ", " + Head(o->sha256) : "no output",
                   o && textureEntry.nodeId == "texture" && textureEntry.source == "texture.cnj" && o->logicalName == "texture" && o->path == "texture.cnb" && o->sha256 == Canon::ContentFileSha256(out / "texture.cnb"));
        bool primary = false, sidecar = false;
        for (const Canon::ContentDependency& d : textureEntry.dependencies) {
            if (d.kind == Canon::ContentDependencyKind::PrimarySource && d.identity == "texture.cnj") primary = true;
            if (d.kind == Canon::ContentDependencyKind::SourceFile && d.identity == "plaque.png") sidecar = true;
        }
        checks.Add("the texture record's dependencies", "texture.cnj (primary source), plaque.png (source file), root-relative", std::string(primary ? "primary " : "") + (sidecar ? "sidecar" : ""), primary && sidecar);
        checks.Add("direct and effective fingerprints", "64 lowercase hex digits each, and different for the two assets",
                   Head(curveEntry.fingerprint) + " / " + Head(textureEntry.fingerprint),
                   Hex64(curveEntry.directFingerprint) && Hex64(curveEntry.fingerprint) && Hex64(textureEntry.fingerprint) && curveEntry.fingerprint != textureEntry.fingerprint);
        manifest.Set(textureEntry);
        manifest.Set(curveEntry);
        const std::string text = manifest.Serialize();
        // The version is CNA's to raise; what must hold is that the file written carries it.
        const std::string carried = "\"version\":" + std::to_string(Canon::ContentBuildManifestVersion) + ",";   // the serializer writes compact JSON
        checks.Add("ContentBuildManifestVersion, ContentBuildManifestFileName", "the version the serialized manifest carries, \".cna-content-manifest.json\"",
                   std::to_string(Canon::ContentBuildManifestVersion) + (text.find(carried) != std::string::npos ? " (carried)" : " (not in the text)") + ", \"" + Canon::ContentBuildManifestFileName + "\"",
                   text.find(carried) != std::string::npos && std::string(Canon::ContentBuildManifestFileName) == ".cna-content-manifest.json");
        WriteBytes(out / Canon::ContentBuildManifestFileName, std::vector<std::uint8_t>(text.begin(), text.end()));
        const Canon::ContentBuildManifest again = Canon::ContentBuildManifest::Parse(text);
        checks.Add("Serialize, then Parse", "the same two records, the same text again, one trailing newline",
                   std::to_string(again.Entries().size()) + " records" + (again.Entries() == manifest.Entries() ? ", equal" : ", different") + (again.Serialize() == text ? ", same text" : ", other text"),
                   again.Entries() == manifest.Entries() && again.Serialize() == text && !text.empty() && text.back() == '\n' && text[text.size() > 1 ? text.size() - 2 : 0] != '\n');
        checks.Add("Find(\"texture\"), Find(\"nothing\")", "the texture record; null", std::string(again.Find("texture") ? "record" : "null") + "; " + (again.Find("nothing") ? "record" : "null"),
                   again.Find("texture") && *again.Find("texture") == textureEntry && again.Find("nothing") == nullptr);
        checks.ExpectThrow<std::runtime_error>("ContentBuildManifest::Parse of a manifest one version older", "std::runtime_error (incompatible)", [&] { (void)Canon::ContentBuildManifest::Parse(OlderVersion(text)); });
        checks.ExpectThrow<std::invalid_argument>("ContentBuildManifest::Set of a record without a node id", "std::invalid_argument", [&] {
            Canon::ContentBuildManifestEntry nameless = curveEntry;
            nameless.nodeId.clear();
            Canon::ContentBuildManifest scratch;
            scratch.Set(nameless);
        });
        Canon::ContentBuildManifest cleared = again;
        cleared.Clear();
        checks.Add("Clear()", "no records", std::to_string(cleared.Entries().size()) + " records", cleared.Entries().empty());
    });
    if (textureEntry.fingerprint.empty()) return;

    // ---- the incremental decision ---------------------------------------------------------------------------------------------------
    checks.Group("incremental decision", [&] {
        checks.Add("nothing changed: ComputeContentBuildDirectFingerprint", "both direct fingerprints as recorded (both assets would be skipped)",
                   std::string(Canon::ComputeContentBuildDirectFingerprint(curveEntry, root) == curveEntry.directFingerprint ? "curve same" : "curve moved") + ", " +
                       (Canon::ComputeContentBuildDirectFingerprint(textureEntry, root) == textureEntry.directFingerprint ? "texture same" : "texture moved"),
                   Canon::ComputeContentBuildDirectFingerprint(curveEntry, root) == curveEntry.directFingerprint && Canon::ComputeContentBuildDirectFingerprint(textureEntry, root) == textureEntry.directFingerprint);
        checks.Add("ComputeContentBuildFingerprint of the unchanged texture", "the recorded effective fingerprint", Head(Canon::ComputeContentBuildFingerprint(textureEntry, root)),
                   Canon::ComputeContentBuildFingerprint(textureEntry, root) == textureEntry.fingerprint);

        // The sidecar changes: another picture under the same name.
        std::filesystem::copy_file(std::filesystem::path(services.contentRoot) / "Pipeline" / "picture.png", root / "plaque.png", std::filesystem::copy_options::overwrite_existing);
        Canon::ContentBuildManifestEntry moved = textureEntry;
        Canon::RefreshContentBuildDirectFingerprint(moved, root);
        Canon::RefreshContentBuildEffectiveFingerprint(moved);
        const Canon::ContentBuildFingerprintState& a = textureEntry.fingerprintState;
        const Canon::ContentBuildFingerprintState& b = moved.fingerprintState;
        checks.Add("plaque.png replaced: the texture's fingerprint moves, the curve's does not", "texture moved, curve same",
                   std::string(moved.fingerprint != textureEntry.fingerprint ? "texture moved" : "texture same") + ", " +
                       (Canon::ComputeContentBuildDirectFingerprint(curveEntry, root) == curveEntry.directFingerprint ? "curve same" : "curve moved"),
                   moved.fingerprint != textureEntry.fingerprint && Canon::ComputeContentBuildDirectFingerprint(curveEntry, root) == curveEntry.directFingerprint);
        checks.Add("which fingerprint domain moved", "sourceDependencyBytes only: primarySourceBytes, sourceDependencySet, processorParameters, writerSchemas unchanged",
                   std::string("primary ") + (a.primarySourceBytes == b.primarySourceBytes ? "same" : "moved") + ", dependency set " + (a.sourceDependencySet == b.sourceDependencySet ? "same" : "moved") + ", dependency bytes " +
                       (a.sourceDependencyBytes == b.sourceDependencyBytes ? "same" : "moved") + ", parameters " + (a.processorParameters == b.processorParameters ? "same" : "moved") + ", writers " + (a.writerSchemas == b.writerSchemas ? "same" : "moved"),
                   a.primarySourceBytes == b.primarySourceBytes && a.sourceDependencySet == b.sourceDependencySet && a.sourceDependencyBytes != b.sourceDependencyBytes && a.processorParameters == b.processorParameters &&
                       a.writerSchemas == b.writerSchemas);
        const Canon::ContentBuildResult rebuilt = pipeline.Build(request("texture.cnj", "texture"));
        checks.Add("the rebuilt texture's output digest", "different from the recorded one (the new picture is inside)", Head(Canon::ContentSha256(rebuilt.output.bytes)),
                   !textureEntry.outputs.empty() && Canon::ContentSha256(rebuilt.output.bytes) != textureEntry.outputs.front().sha256);
        std::filesystem::remove(root / "plaque.png");
        WriteBytes(root / "plaque.png", originalPng);
        checks.Add("plaque.png restored", "the recorded direct fingerprint again", Canon::ComputeContentBuildDirectFingerprint(textureEntry, root) == textureEntry.directFingerprint ? "same as recorded" : "different",
                   Canon::ComputeContentBuildDirectFingerprint(textureEntry, root) == textureEntry.directFingerprint);

        // A processor parameter changes.
        Canon::ContentBuildManifestEntry parameterised = curveEntry;
        parameterised.parameters.Set("premultiplyAlpha", false);
        Canon::RefreshContentBuildDirectFingerprint(parameterised, root);
        checks.Add("a parameter added to the curve's record", "processorParameters moves, primarySourceBytes does not, the direct fingerprint moves",
                   std::string("parameters ") + (parameterised.fingerprintState.processorParameters != curveEntry.fingerprintState.processorParameters ? "moved" : "same") + ", primary " +
                       (parameterised.fingerprintState.primarySourceBytes == curveEntry.fingerprintState.primarySourceBytes ? "same" : "moved"),
                   parameterised.fingerprintState.processorParameters != curveEntry.fingerprintState.processorParameters && parameterised.fingerprintState.primarySourceBytes == curveEntry.fingerprintState.primarySourceBytes &&
                       parameterised.directFingerprint != curveEntry.directFingerprint);

        // A content-build dependency: the effective fingerprint needs the dependency's own.
        Canon::ContentBuildManifestEntry dependent = textureEntry;
        dependent.dependencies.push_back({Canon::ContentDependencyKind::ContentBuild, "curve", ""});
        Canon::RefreshContentBuildDirectFingerprint(dependent, root);
        checks.ExpectThrow<std::runtime_error>("ComputeContentBuildEffectiveFingerprint without the dependency's fingerprint", "std::runtime_error (absence is an error, never an unsafe skip)",
                                               [&] { (void)Canon::ComputeContentBuildEffectiveFingerprint(dependent); });
        const std::string withCurve = Canon::ComputeContentBuildEffectiveFingerprint(dependent, {{"curve", curveEntry.fingerprint}});
        const std::string withOther = Canon::ComputeContentBuildEffectiveFingerprint(dependent, {{"curve", textureEntry.fingerprint}});
        checks.Add("the dependency's fingerprint changes", "the dependent's effective fingerprint changes; the direct one does not depend on it", Head(withCurve) + " -> " + Head(withOther),
                   Hex64(withCurve) && withCurve != withOther && dependent.fingerprintState.contentDependencySet != textureEntry.fingerprintState.contentDependencySet);
    });

    // ---- a failing dependency ------------------------------------------------------------------------------------------------------
    checks.Group("a failing dependency", [&] {
        std::filesystem::remove(root / "plaque.png");
        try {
            (void)Canon::ComputeContentBuildDirectFingerprint(textureEntry, root);
            checks.Add("plaque.png deleted: the texture's fingerprint", "std::runtime_error naming plaque.png", "computed", false);
        } catch (const std::runtime_error& e) {
            checks.Add("plaque.png deleted: the texture's fingerprint", "std::runtime_error naming plaque.png", Checks::Short(e.what(), 160), std::string(e.what()).find("plaque.png") != std::string::npos);
        }
        try {
            (void)pipeline.Build(request("texture.cnj", "texture"));
            checks.Add("plaque.png deleted: building texture.cnj", "ContentPipelineError for texture.cnj naming plaque.png", "built", false);
        } catch (const Canon::ContentPipelineError& e) {
            checks.Add("plaque.png deleted: building texture.cnj", "ContentPipelineError at the Import stage for texture.cnj, from CNA.CnjImporter",
                       std::string(Canon::ContentPipelineStageName(e.Stage())) + " stage, " + e.Source().filename().string() + ", " + e.Component(),
                       e.Stage() == Canon::ContentPipelineStage::Import && e.Source().filename() == "texture.cnj" && e.Component() == "CNA.CnjImporter");
            checks.Info("the error's text", "-", Checks::Short(e.what(), 200));
        }
        checks.Add("the curve does not depend on it", "its direct fingerprint as recorded", Canon::ComputeContentBuildDirectFingerprint(curveEntry, root) == curveEntry.directFingerprint ? "same" : "moved",
                   Canon::ComputeContentBuildDirectFingerprint(curveEntry, root) == curveEntry.directFingerprint);
        WriteBytes(root / "plaque.png", originalPng);
    });

    // ---- CompileCnjToCnb ------------------------------------------------------------------------------------------------------------
    checks.Group("CompileCnjToCnb", [&] {
        const Cnb::CnjToCnbResult curve = Cnb::CompileCnjToCnb((root / "curve.cnj").string());
        const Cnb::CnbDocument doc = Cnb::CnbDocument::Parse(curve.bytes, "curve.cnb");
        checks.Add("CompileCnjToCnb(curve.cnj)", "a Curve: the type name and id the file's header and CMET chunk carry, absorbed [curve.cnj], no external references, CMET name curve",
                   curve.assetTypeName + " " + std::to_string(curve.assetTypeId) + ", absorbed " + std::to_string(curve.absorbedFiles.size()) + (curve.absorbedFiles.empty() ? "" : " (" + curve.absorbedFiles[0] + ")") + ", " +
                       std::to_string(curve.externalReferences.size()) + " references, name " + doc.Metadata().contentName,
                   curve.assetTypeName == doc.Metadata().assetTypeName && curve.assetTypeName.find("Curve") != std::string::npos && curve.assetTypeId == doc.AssetTypeId() && curve.absorbedFiles == std::vector<std::string>{"curve.cnj"} && curve.externalReferences.empty() && doc.Metadata().contentName == "curve");
        WriteBytes(out / "compiled" / "curve.cnb", curve.bytes);
        const Curve loaded = manager.Load<Curve>("compiled/curve");
        checks.Add("the compiled curve, loaded", "Evaluate(1) 2.5, Evaluate(2) 7.5", F(loaded.Evaluate(1.0f)) + ", " + F(loaded.Evaluate(2.0f)), loaded.Evaluate(1.0f) == 2.5f && loaded.Evaluate(2.0f) == 7.5f);
        const Cnb::CnjToCnbResult named = Cnb::CompileCnjToCnb((root / "curve.cnj").string(), {}, "Named/curve");
        checks.Add("CompileCnjToCnb(curve.cnj, contentName \"Named/curve\")", "CMET content name Named/curve", Cnb::CnbDocument::Parse(named.bytes, "named.cnb").Metadata().contentName,
                   Cnb::CnbDocument::Parse(named.bytes, "named.cnb").Metadata().contentName == "Named/curve");
        const Cnb::CnjToCnbResult texture = Cnb::CompileCnjToCnb((root / "texture.cnj").string());
        std::string absorbed;
        for (const std::string& f : texture.absorbedFiles) absorbed += (absorbed.empty() ? "" : ", ") + f;
        checks.Add("CompileCnjToCnb(texture.cnj)", "a Texture2D, absorbed texture.cnj, plaque.png (the sidecar no longer needs to ship)", texture.assetTypeName + ", absorbed " + absorbed,
                   texture.assetTypeName.find("Texture2D") != std::string::npos && texture.absorbedFiles == std::vector<std::string>{"texture.cnj", "plaque.png"});
        try {
            (void)Cnb::CompileCnjToCnb((root / "unknown.cnj").string());
            checks.Add("CompileCnjToCnb(unknown.cnj, type \"Effect\")", "ContentLoadException naming the type", "compiled", false);
        } catch (const ContentLoadException& e) {
            checks.Add("CompileCnjToCnb(unknown.cnj, type \"Effect\")", "ContentLoadException naming the type", Checks::Short(e.what(), 140), std::string(e.what()).find("Effect") != std::string::npos);
        }
    });

    // ---- headless source imports -------------------------------------------------------------------------------------------------
    checks.Group("CnbSourceImport", [&] {
        Cnb::CnbImageImportOptions keyed;
        keyed.colorKey = std::array<std::uint8_t, 3>{255, 0, 255};
        const Cnb::CnbTextureData plain = Cnb::ImportImageAsCnbTexture2D(root / "plaque.png");
        const Cnb::CnbTextureData key = Cnb::ImportImageAsCnbTexture2D((root / "plaque.png").string(), keyed);
        const auto pixel = [](const Cnb::CnbTextureData& t, std::size_t i) {
            if (t.representations.empty() || t.representations[0].levels.empty() || t.representations[0].levels[0].size() < (i + 1) * 4) return std::string("-");
            const std::vector<std::uint8_t>& l = t.representations[0].levels[0];
            return "(" + std::to_string(l[i * 4]) + "," + std::to_string(l[i * 4 + 1]) + "," + std::to_string(l[i * 4 + 2]) + "," + std::to_string(l[i * 4 + 3]) + ")";
        };
        checks.Add("ImportImageAsCnbTexture2D(plaque.png)", "4x4, 1 level, Rgba8; pixels as written: (255,0,255,255), (255,0,0,128)",
                   std::to_string(plain.width) + "x" + std::to_string(plain.height) + ", " + std::to_string(plain.mipCount) + " level, " + (plain.representations.size() == 1 && plain.representations[0].format == Cnb::CnbTextureFormat::Rgba8 ? "Rgba8" : "other") + "; " +
                       pixel(plain, 0) + ", " + pixel(plain, 1),
                   plain.width == 4 && plain.height == 4 && plain.mipCount == 1 && plain.representations.size() == 1 && plain.representations[0].format == Cnb::CnbTextureFormat::Rgba8 && pixel(plain, 0) == "(255,0,255,255)" &&
                       pixel(plain, 1) == "(255,0,0,128)");
        checks.Add("... with colorKey magenta", "the magenta pixel keeps its RGB with alpha 0; the others untouched", pixel(key, 0) + ", " + pixel(key, 1), pixel(key, 0) == "(255,0,255,0)" && pixel(key, 1) == "(255,0,0,128)");

        const std::vector<std::uint8_t> tone = ReadBytes(root / "tone8.wav");
        const CNA::Content::Import::ImportedSound imported = Cnb::DecodeWavAsImportedSound(tone, "tone8.wav");
        checks.Add("DecodeWavAsImportedSound(tone8.wav)", "8-bit unsigned kept, 8000 Hz, 1 channel, 80 frames, no loop", std::string(CNA::Content::Import::ImportedPcmEncodingName(imported.encoding)) + ", " + std::to_string(imported.sampleRate) + " Hz, " +
                                                                                           std::to_string(imported.channels) + " ch, " + std::to_string(imported.frameCount) + " frames, loop " + std::to_string(imported.loopLength),
                   imported.encoding == CNA::Content::Import::ImportedPcmEncoding::Unsigned8 && imported.sampleRate == 8000u && imported.channels == 1u && imported.frameCount == 80u && imported.loopLength == 0u);
        const Cnb::CnbSoundEffectData authored = Cnb::ProcessImportedSoundEffect(imported);
        const Cnb::CnbSoundEffectData whole = Cnb::ProcessImportedSoundEffect(imported, Cnb::SoundEffectLoopPolicy::WholeSoundWhenUnset);
        checks.Add("ProcessImportedSoundEffect: AsAuthored, then WholeSoundWhenUnset", "loop 0 + 0, then 0 + 80 (XNA's processor gives an unlooped source the whole sound)",
                   std::to_string(authored.loopStart) + " + " + std::to_string(authored.loopLength) + ", then " + std::to_string(whole.loopStart) + " + " + std::to_string(whole.loopLength),
                   authored.loopLength == 0u && whole.loopStart == 0u && whole.loopLength == 80u);
        // CnbSourceImport.hpp (CNA acc2ee55b): 8-bit PCM is stored as Pcm8 unchanged; 24-bit, 32-bit and float narrow to Pcm16
        // with round-to-nearest and saturation, because no CNB schema stores those widths.
        const Cnb::CnbSoundEffectData kept = Cnb::DecodeWavAsCnbSoundEffect(tone, "tone8.wav");
        const auto u8 = [&](std::size_t i) { return kept.samples.size() > i ? static_cast<int>(kept.samples[i]) : -1; };
        checks.Add("DecodeWavAsCnbSoundEffect(tone8.wav): 8-bit stored as Pcm8", "Pcm8, 80 frames; the bytes unchanged: 128, 138, 218",
                   std::string(kept.format == Cnb::CnbAudioFormat::Pcm8 ? "Pcm8" : (kept.format == Cnb::CnbAudioFormat::Pcm16 ? "Pcm16" : "other")) + ", " + std::to_string(kept.frameCount) + " frames; " + std::to_string(u8(0)) + ", " + std::to_string(u8(1)) + ", " + std::to_string(u8(9)),
                   kept.format == Cnb::CnbAudioFormat::Pcm8 && kept.frameCount == 80u && u8(0) == 128 && u8(1) == 138 && u8(9) == 218);
        const Cnb::CnbSoundEffectData loop = Cnb::ImportWavAsCnbSoundEffect(root / "loop16.wav");
        checks.Add("ImportWavAsCnbSoundEffect(loop16.wav)", "Pcm16 stereo, 40 frames, loop 10 + 20 from the smpl chunk", std::string(loop.format == Cnb::CnbAudioFormat::Pcm16 ? "Pcm16 " : "other ") + std::to_string(loop.channels) + " ch, " +
                                                                                                                               std::to_string(loop.frameCount) + " frames, loop " + std::to_string(loop.loopStart) + " + " + std::to_string(loop.loopLength),
                   loop.format == Cnb::CnbAudioFormat::Pcm16 && loop.channels == 2u && loop.frameCount == 40u && loop.loopStart == 10u && loop.loopLength == 20u);
        try {
            const CNA::Content::Import::ImportedSound deep = Cnb::ImportWavAsImportedSound((root / "deep24.wav").string());
            checks.Info("ImportWavAsImportedSound(deep24.wav)", "-", std::string("accepted as ") + CNA::Content::Import::ImportedPcmEncodingName(deep.encoding) + " (the source-oriented record has a 24-bit encoding)");
        } catch (const ContentLoadException& e) {
            checks.Info("ImportWavAsImportedSound(deep24.wav)", "-", "refused: " + Checks::Short(e.what(), 140));
        }
        try {
            // deep24.wav holds 0, 1000, 2000 ... 9000: divided by 256 and rounded (not truncated), 1000 is 4 and 6000 is 23.
            const Cnb::CnbSoundEffectData deep = Cnb::DecodeWavAsCnbSoundEffect(ReadBytes(root / "deep24.wav"), "deep24.wav");
            const auto s16 = [&](std::size_t i) { return deep.samples.size() >= (i + 1) * 2 ? static_cast<int>(static_cast<std::int16_t>(deep.samples[i * 2] | deep.samples[i * 2 + 1] << 8)) : -99999; };
            checks.Add("DecodeWavAsCnbSoundEffect(deep24.wav, 24-bit PCM): narrowed to Pcm16", "Pcm16, 10 frames; 0, 4, 23, 35 (rounded to nearest)",
                       std::string(deep.format == Cnb::CnbAudioFormat::Pcm16 ? "Pcm16" : "another format") + ", " + std::to_string(deep.frameCount) + " frames; " + std::to_string(s16(0)) + ", " +
                           std::to_string(s16(1)) + ", " + std::to_string(s16(6)) + ", " + std::to_string(s16(9)),
                       deep.format == Cnb::CnbAudioFormat::Pcm16 && deep.frameCount == 10u && s16(0) == 0 && s16(1) == 4 && s16(6) == 23 && s16(9) == 35);
        } catch (const ContentLoadException& e) {
            checks.Add("DecodeWavAsCnbSoundEffect(deep24.wav, 24-bit PCM): narrowed to Pcm16", "Pcm16, 10 frames; 0, 4, 23, 35 (rounded to nearest)", "refused: " + Checks::Short(e.what(), 140), false);
        }
    });

    // ---- .cnb chunk compression -------------------------------------------------------------------------------------------------
    checks.Group("CnbChunkCompression", [&] {
        std::vector<std::uint8_t> raw(4096);
        for (std::size_t i = 0; i < raw.size(); ++i) raw[i] = static_cast<std::uint8_t>((i / 64) % 7);
        checks.Add("CnbCompressionToString", "None, Lz4, Zstd, Deflate; \"unknown codec 9\"",
                   Cnb::CnbCompressionToString(Cnb::CnbCompression::None) + ", " + Cnb::CnbCompressionToString(Cnb::CnbCompression::Lz4) + ", " + Cnb::CnbCompressionToString(Cnb::CnbCompression::Zstd) + ", " +
                       Cnb::CnbCompressionToString(Cnb::CnbCompression::Deflate) + "; \"" + Cnb::CnbCompressionToString(static_cast<Cnb::CnbCompression>(9u)) + "\"",
                   Cnb::CnbCompressionToString(static_cast<Cnb::CnbCompression>(9u)) == "unknown codec 9");
        checks.Add("IsCnbCompressionSupported(None)", "true (always)", Yes(Cnb::IsCnbCompressionSupported(Cnb::CnbCompression::None)), Cnb::IsCnbCompressionSupported(Cnb::CnbCompression::None));
        checks.Add("CompressCnbChunk(raw, None)", "a copy of the 4096 bytes", Cnb::CompressCnbChunk(raw, Cnb::CnbCompression::None) == raw ? "an identical copy" : "different", Cnb::CompressCnbChunk(raw, Cnb::CnbCompression::None) == raw);
        checks.ExpectThrow<ContentLoadException>("CompressCnbChunk(raw, Lz4)", "ContentLoadException (identifier assigned, no implementation in this build)", [&] { (void)Cnb::CompressCnbChunk(raw, Cnb::CnbCompression::Lz4); });
        const bool zstd = Cnb::IsCnbCompressionSupported(Cnb::CnbCompression::Zstd);
        checks.Info("IsCnbCompressionSupported(Zstd)", "-", zstd ? "yes (CNA_CNB_ZSTD found libzstd)" : "no: this build has no Zstandard");
        if (!zstd) return;
        const std::vector<std::uint8_t> packed = Cnb::CompressCnbChunk(raw, Cnb::CnbCompression::Zstd);
        const std::vector<std::uint8_t> back = Cnb::DecompressCnbChunk(packed, Cnb::CnbCompression::Zstd, raw.size(), 1u << 20, "wk-incremental chunk");
        checks.Add("Zstd: CompressCnbChunk, then DecompressCnbChunk", "smaller than 4096 bytes, and the same 4096 bytes back", std::to_string(packed.size()) + " bytes, " + (back == raw ? "identical" : "different"), packed.size() < raw.size() && back == raw);
        checks.ExpectThrow<ContentLoadException>("DecompressCnbChunk with uncompressedSize 4095", "ContentLoadException (the stream must expand to exactly the size the table says)",
                                                 [&] { (void)Cnb::DecompressCnbChunk(packed, Cnb::CnbCompression::Zstd, raw.size() - 1, 1u << 20, "wk-incremental chunk"); });
        checks.ExpectThrow<ContentLoadException>("DecompressCnbChunk with a 1024-byte ceiling", "ContentLoadException (checked against the ceiling before anything is allocated)",
                                                 [&] { (void)Cnb::DecompressCnbChunk(packed, Cnb::CnbCompression::Zstd, raw.size(), 1024u, "wk-incremental chunk"); });
    });
}

}  // namespace Workshop

#endif  // MUSEUM_PIPELINE

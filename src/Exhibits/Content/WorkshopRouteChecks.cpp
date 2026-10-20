// Pipeline Workshop, mode "routes" (room wk-routes): every source route CNA's own build engine ships, registered explicitly into one
// ContentPipelineRegistry (the registrations RegisterBuiltInContentPipeline would make, which lives in the cna-content tool library the
// museum does not link) and run through ContentPipeline::Build on a tiny source copied into the build tree:
//     .cnj Curve and Texture2D (RegisterCnjContentPipeline), .wav (RegisterSoundEffectContentPipeline), .mp3 as a Song
//     (RegisterSongContentPipeline with the build-time duration probe), .wmv as a Video (RegisterVideoContentPipeline with the frame
//     probe), .gltf (RegisterModelContentPipeline), .fxb (RegisterCompiledEffectContentPipeline), .xnb (RegisterXnbContentPipeline) and
//     XNA's intermediate .xml (RegisterXnaXmlSourceContentPipeline).
// Each built file is written to WorkshopOutput/routes next to the executable and loaded back through an ordinary ContentManager (the
// public load route), or, where a load needs a device the scenario does not have (a sound), decoded from XNA's documented .xnb layout.
// The expectations are the sources' own values and the routes' header documentation; the refusals CNA documents are provoked.
#ifdef MUSEUM_PIPELINE

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "CNA/Content/Cnb/CnbDocument.hpp"
#include "CNA/Content/Cnb/CnbSourceImport.hpp"
#include "CNA/Content/Pipeline/BuildTimeMediaDecoder.hpp"
#include "CNA/Content/Pipeline/CnjContentPipeline.hpp"
#include "CNA/Content/Pipeline/ContentPipeline.hpp"
#include "CNA/Content/Pipeline/EffectContentPipeline.hpp"
#include "CNA/Content/Pipeline/ModelContentPipeline.hpp"
#include "CNA/Content/Pipeline/ProcessedSoundEffect.hpp"
#include "CNA/Content/Pipeline/SongContentPipeline.hpp"
#include "CNA/Content/Pipeline/SoundEffectContentPipeline.hpp"
#include "CNA/Content/Pipeline/Texture2DContentPipeline.hpp"
#include "CNA/Content/Pipeline/TextureCompressionPipeline.hpp"
#include "CNA/Content/Pipeline/VideoContentPipeline.hpp"
#include "CNA/Content/Pipeline/XnaXmlSourceContentPipeline.hpp"
#include "CNA/Content/Pipeline/XnbContentPipeline.hpp"
#include "CNA/Content/Pipeline/XnbOutputContentPipeline.hpp"
#include "Exhibits/Content/WorkshopChecks.hpp"
#include "Exhibits/Content/WorkshopPipeline.hpp"
#include "Exhibits/Services.hpp"
#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Content/ContentLoadException.hpp"
#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Serialization/Compiler/ContentCompiler.hpp"
#include "Microsoft/Xna/Framework/Curve.hpp"
#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/Graphics/Model.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelMesh.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelMeshPart.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Media/Song.hpp"
#include "Microsoft/Xna/Framework/Media/Video/Video.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

using namespace Microsoft::Xna::Framework;

namespace {

namespace Canon = CNA::Content::Pipeline;
namespace BuildMedia = CNA::Content::Pipeline::BuildTimeMedia;
namespace Cnb = CNA::Content::Cnb;
namespace Compiler = Microsoft::Xna::Framework::Content::Pipeline::Serialization::Compiler;
using Microsoft::Xna::Framework::Content::ContentManager;
using Workshop::Checks;
using Workshop::F;
using Workshop::Yes;

std::string Id(const Canon::ContentComponentIdentity& id) { return id.name + "/" + id.version; }

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

std::uint32_t Le32(const std::vector<std::uint8_t>& b, std::size_t at)
{
    return at + 4 <= b.size() ? static_cast<std::uint32_t>(b[at]) | static_cast<std::uint32_t>(b[at + 1]) << 8 | static_cast<std::uint32_t>(b[at + 2]) << 16 | static_cast<std::uint32_t>(b[at + 3]) << 24 : 0u;
}

std::uint16_t Le16(const std::vector<std::uint8_t>& b, std::size_t at)
{
    return at + 2 <= b.size() ? static_cast<std::uint16_t>(b[at] | b[at + 1] << 8) : std::uint16_t{0};
}

std::string Rgba(const Color& c)
{
    return "(" + std::to_string(c.getRProperty()) + "," + std::to_string(c.getGProperty()) + "," + std::to_string(c.getBProperty()) + "," + std::to_string(c.getAProperty()) + ")";
}

// One route bench: the registry, the source folder in the build tree, and where the built files go.
struct Bench {
    std::filesystem::path root, out;
    std::shared_ptr<Canon::ContentPipelineRegistry> registry;
    std::unique_ptr<Canon::ContentPipeline> pipeline;

    [[nodiscard]] Canon::ContentBuildRequest Request(const std::string& source, const std::string& logicalName, Canon::ContentOutputFormat format = Canon::ContentOutputFormat::Cnb) const
    {
        Canon::ContentBuildRequest request;
        request.sourceRoot = root;
        request.source = source;
        request.logicalName = logicalName;
        request.outputFormat = format;
        request.environment.outputDirectory = out;
        request.environment.intermediateDirectory = Workshop::Pipe::OutputRoot() / "routes-obj";
        return request;
    }

    // Publishes what a build produced the way the coordinator does: the primary file, every additional output, every deployment file.
    void Publish(const Canon::ContentBuildResult& r) const
    {
        const char* ext = Canon::ContentOutputFormatExtension(r.outputFormat);
        WriteBytes(out / (r.logicalName + ext), r.output.bytes);
        for (const Canon::ContentAdditionalWriteOutput& extra : r.output.additionalOutputs) WriteBytes(out / (extra.logicalName + ext), extra.bytes);
        for (const Canon::ContentDeploymentFile& file : r.deploymentFiles) {
            std::filesystem::create_directories((out / file.outputPath).parent_path());
            std::filesystem::copy_file(file.source, out / file.outputPath, std::filesystem::copy_options::overwrite_existing);
        }
    }
};

std::vector<Color> Pixels(const Graphics::Texture2D& texture)
{
    std::vector<Color> pixels(static_cast<std::size_t>(texture.getWidthProperty() * texture.getHeightProperty()));
    texture.GetData(pixels.data(), static_cast<int>(pixels.size()));
    return pixels;
}

// XNA's SoundEffectReader payload, read from the end of an uncompressed .xnb: Int32 format size, WAVEFORMATEX, Int32 data size, the
// samples, Int32 loop start, Int32 loop length, Int32 duration in milliseconds.
struct XnbSound {
    bool found = false;
    std::uint16_t formatTag = 0, channels = 0, blockAlign = 0, bitsPerSample = 0;
    std::uint32_t sampleRate = 0, dataSize = 0, loopStart = 0, loopLength = 0, durationMs = 0;
};

XnbSound ParseXnbSound(const std::vector<std::uint8_t>& b, std::uint32_t expectedDataSize)
{
    XnbSound s;
    if (b.size() < 12 + expectedDataSize + 4 + 22) return s;
    const std::size_t tail = b.size() - 12;
    const std::size_t dataSizeAt = tail - expectedDataSize - 4;
    if (Le32(b, dataSizeAt) != expectedDataSize) return s;
    for (const std::uint32_t formatSize : {18u, 16u}) {
        const std::size_t formatAt = dataSizeAt - formatSize;
        if (Le32(b, formatAt - 4) != formatSize) continue;
        s.found = true;
        s.formatTag = Le16(b, formatAt);
        s.channels = Le16(b, formatAt + 2);
        s.sampleRate = Le32(b, formatAt + 4);
        s.blockAlign = Le16(b, formatAt + 12);
        s.bitsPerSample = Le16(b, formatAt + 14);
        s.dataSize = expectedDataSize;
        s.loopStart = Le32(b, tail);
        s.loopLength = Le32(b, tail + 4);
        s.durationMs = Le32(b, tail + 8);
        break;
    }
    return s;
}

}  // namespace

namespace Workshop {

void BuildRouteChecks(Checks& checks, Services& services)
{
    Bench bench;
    bench.root = Pipe::PrepareSourceRoot(services.contentRoot, {"curve.cnj", "texture.cnj", "plaque.png", "tone8.wav", "loop16.wav", "tone.mp3", "clip.wmv", "uvsets.gltf", "vector.xml"});
    std::filesystem::copy_file(std::filesystem::path(services.contentRoot) / "Effects" / "blur2tap.fxb", bench.root / "blur2tap.fxb", std::filesystem::copy_options::overwrite_existing);
    bench.out = Pipe::OutputRoot() / "routes";
    std::filesystem::remove_all(bench.out);
    std::filesystem::create_directories(bench.out);

    // The registry: every route explicitly, as a user-built compiler would register them.
    const bool media = BuildMedia::IsAvailable();
    checks.Info("build-time media decoder", "-", media ? "available: " + BuildMedia::Identity() : "unavailable: " + BuildMedia::UnavailableReason());
    bench.registry = std::make_shared<Canon::ContentPipelineRegistry>();
    auto compiler = std::make_shared<Compiler::ContentCompiler>();
    checks.Group("registering the routes", [&] {
        Canon::RegisterTexture2DContentPipeline(*bench.registry, Canon::MakeBlockCompressionTextureEncoder());
        Canon::RegisterCnjContentPipeline(*bench.registry);
        Canon::RegisterSoundEffectContentPipeline(*bench.registry, BuildMedia::MakeCompressedSoundDecoder());
        Canon::RegisterSongContentPipeline(*bench.registry, BuildMedia::MakeSongDurationProbe());
        Canon::RegisterVideoContentPipeline(*bench.registry, BuildMedia::MakeVideoMetadataProbe());
        Canon::RegisterModelContentPipeline(*bench.registry);
        Canon::RegisterCompiledEffectContentPipeline(*bench.registry);
        Canon::RegisterXnbContentPipeline(*bench.registry);
        Canon::RegisterXnbOutputContentPipeline(*bench.registry, {});
        Canon::RegisterXnaXmlSourceContentPipeline(*bench.registry, compiler, {});   // container options: CNA::Internal::Xnb::XnbFileOptions defaults
        std::string have;
        for (const char* ext : {".cnj", ".wav", ".mp3", ".wmv", ".gltf", ".fxb", ".xnb", ".xml"})
            if (bench.registry->HasImporterForSource(std::string("x") + ext)) have += std::string(have.empty() ? "" : " ") + ext;
        checks.Add("every route registered", "importers for .cnj .wav .mp3 .wmv .gltf .fxb .xnb .xml", have, have == ".cnj .wav .mp3 .wmv .gltf .fxb .xnb .xml");
    });
    bench.pipeline = std::make_unique<Canon::ContentPipeline>(bench.registry);
    ContentManager manager(&services.game->getServicesProperty(), bench.out.string());

    // ---- .cnj: a Curve through CNA's own document route, loaded back by ContentManager ------------------------------------------------
    checks.Group(".cnj Curve", [&] {
        const Canon::ContentBuildResult r = bench.pipeline->Build(bench.Request("curve.cnj", "curve"));
        bench.Publish(r);
        checks.Add("curve.cnj: the CNJ route's components", "CNA.CnjImporter/1 -> CNA.CurveProcessor/1 -> CNA.CurveContentWriter/1 (CnjContentPipeline.hpp)",
                   Id(r.importer) + " -> " + Id(r.processor) + " -> " + Id(r.writer),
                   Id(r.importer) == "CNA.CnjImporter/1" && Id(r.processor) == "CNA.CurveProcessor/1" && Id(r.writer) == "CNA.CurveContentWriter/1");
        const Curve curve = manager.Load<Curve>("curve");
        checks.Add("ContentManager.Load<Curve>(\"curve\") of the built .cnb", "2 keys; Evaluate(-1) 2.5, (1) 2.5 (Step), (2) 7.5, (3) 7.5",
                   std::to_string(curve.getKeysProperty().getCountProperty()) + " keys; " + F(curve.Evaluate(-1.0f)) + ", " + F(curve.Evaluate(1.0f)) + ", " + F(curve.Evaluate(2.0f)) + ", " + F(curve.Evaluate(3.0f)),
                   curve.getKeysProperty().getCountProperty() == 2 && curve.Evaluate(-1.0f) == 2.5f && curve.Evaluate(1.0f) == 2.5f && curve.Evaluate(2.0f) == 7.5f && curve.Evaluate(3.0f) == 7.5f);
    });

    // ---- .cnj: a Texture2D document converging on the texture route ---------------------------------------------------------------------
    std::vector<Color> direct;
    checks.Group(".cnj Texture2D", [&] {
        const Canon::ContentBuildResult r = bench.pipeline->Build(bench.Request("texture.cnj", "texture"));
        bench.Publish(r);
        bool sidecar = false;
        for (const Canon::ContentDependency& d : r.dependencies)
            if (d.kind == Canon::ContentDependencyKind::SourceFile && d.identity.find("plaque.png") != std::string::npos) sidecar = true;
        checks.Add("texture.cnj: CNJ importer, then the Texture2D route; plaque.png a source-file dependency", "CNA.CnjImporter, a texture processor, plaque.png recorded",
                   Id(r.importer) + " -> " + Id(r.processor) + ", plaque.png " + (sidecar ? "recorded" : "missing"), r.importer.name == "CNA.CnjImporter" && sidecar);
        const Graphics::Texture2D texture = manager.Load<Graphics::Texture2D>("texture");
        direct = Pixels(texture);
        const bool size = texture.getWidthProperty() == 4 && texture.getHeightProperty() == 4;
        checks.Add("ContentManager.Load<Texture2D>(\"texture\")", "4x4; opaque pixels as in plaque.png: magenta kept (no colour key asked), green, blue, (64,128,192,255)",
                   std::to_string(texture.getWidthProperty()) + "x" + std::to_string(texture.getHeightProperty()) + (size ? ": " + Rgba(direct[0]) + " " + Rgba(direct[2]) + " " + Rgba(direct[3]) + " " + Rgba(direct[15]) : std::string()),
                   size && direct[0] == Color(255, 0, 255, 255) && direct[2] == Color(0, 255, 0, 255) && direct[3] == Color(0, 0, 255, 255) && direct[15] == Color(64, 128, 192, 255));
        if (size) checks.Info("the half-transparent red pixel (255,0,0,128) after the texture route", "-", Rgba(direct[1]));
    });

    // ---- .wav: 8-bit PCM, kept 8-bit by SoundEffectProcessor in both containers ----------------------------------------------------------
    checks.Group(".wav SoundEffect", [&] {
        const Canon::ContentBuildResult cnb = bench.pipeline->Build(bench.Request("tone8.wav", "tone8"));
        checks.Add("tone8.wav: WavImporter -> SoundEffectProcessor -> SoundEffectContentWriter", "the three components of SoundEffectContentPipeline.hpp",
                   Id(cnb.importer) + " -> " + Id(cnb.processor) + " -> " + Id(cnb.writer),
                   cnb.importer == Canon::WavImporter{}.Identity() && cnb.processor == Canon::SoundEffectProcessor{}.Identity() && cnb.writer == Canon::SoundEffectContentWriter{}.Identity());
        const Cnb::CnbDocument doc = Cnb::CnbDocument::Parse(cnb.output.bytes, "tone8.cnb");
        checks.Add("the .cnb of an 8-bit source", "a SoundEffect document of schema 2 or later (CnbAudioFormat::Pcm8 needs schema 2)",
                   cnb.output.assetTypeName + ", type id " + std::to_string(doc.AssetTypeId()) + ", schema " + std::to_string(doc.AssetSchemaVersion()),
                   doc.AssetTypeId() == cnb.output.assetTypeId && doc.AssetSchemaVersion() >= 2u);
        const Canon::ContentBuildResult xnb = bench.pipeline->Build(bench.Request("tone8.wav", "tone8", Canon::ContentOutputFormat::Xnb));
        const XnbSound s = ParseXnbSound(xnb.output.bytes, 80u);
        checks.Add("the .xnb of an 8-bit source (XNA's SoundEffectProcessor keeps 8-bit PCM)", "PCM tag 1, mono, 8000 Hz, block 1, 8 bits, 80 data bytes",
                   s.found ? "tag " + std::to_string(s.formatTag) + ", " + std::to_string(s.channels) + " ch, " + std::to_string(s.sampleRate) + " Hz, block " + std::to_string(s.blockAlign) + ", " + std::to_string(s.bitsPerSample) + " bits, " + std::to_string(s.dataSize) + " bytes"
                           : "no SoundEffect payload of 80 bytes found",
                   s.found && s.formatTag == 1 && s.channels == 1 && s.sampleRate == 8000 && s.blockAlign == 1 && s.bitsPerSample == 8);
        checks.Add("loop and duration in the .xnb", "loop 0 + 80 (a source without a loop gets the whole sound), 10 ms", s.found ? "loop " + std::to_string(s.loopStart) + " + " + std::to_string(s.loopLength) + ", " + std::to_string(s.durationMs) + " ms" : "-",
                   s.found && s.loopStart == 0 && s.loopLength == 80 && s.durationMs == 10);
        checks.Add("rootReaderName of the .xnb", "Microsoft.Xna.Framework.Content.SoundEffectReader", xnb.output.rootReaderName, xnb.output.rootReaderName.rfind("Microsoft.Xna.Framework.Content.SoundEffectReader", 0) == 0);
    });

    // ---- the processed sound itself: ProcessedSoundEffect, MakeProcessedSoundEffect, ToCnbSoundEffect ------------------------------------
    checks.Group("ProcessedSoundEffect", [&] {
        checks.Add("ProcessedPcmSampleBytes / BitsPerSample / EncodingName", "Unsigned8: 1, 8, \"8-bit unsigned PCM\"; Signed16LittleEndian: 2, 16, \"16-bit PCM\"",
                   std::to_string(Canon::ProcessedPcmSampleBytes(Canon::ProcessedPcmEncoding::Unsigned8)) + ", " + std::to_string(Canon::ProcessedPcmBitsPerSample(Canon::ProcessedPcmEncoding::Unsigned8)) + ", \"" +
                       Canon::ProcessedPcmEncodingName(Canon::ProcessedPcmEncoding::Unsigned8) + "\"; " + std::to_string(Canon::ProcessedPcmSampleBytes(Canon::ProcessedPcmEncoding::Signed16LittleEndian)) + ", " +
                       std::to_string(Canon::ProcessedPcmBitsPerSample(Canon::ProcessedPcmEncoding::Signed16LittleEndian)) + ", \"" + Canon::ProcessedPcmEncodingName(Canon::ProcessedPcmEncoding::Signed16LittleEndian) + "\"",
                   Canon::ProcessedPcmSampleBytes(Canon::ProcessedPcmEncoding::Unsigned8) == 1u && Canon::ProcessedPcmBitsPerSample(Canon::ProcessedPcmEncoding::Unsigned8) == 8u &&
                       std::string(Canon::ProcessedPcmEncodingName(Canon::ProcessedPcmEncoding::Unsigned8)) == "8-bit unsigned PCM" && Canon::ProcessedPcmSampleBytes(Canon::ProcessedPcmEncoding::Signed16LittleEndian) == 2u &&
                       Canon::ProcessedPcmBitsPerSample(Canon::ProcessedPcmEncoding::Signed16LittleEndian) == 16u && std::string(Canon::ProcessedPcmEncodingName(Canon::ProcessedPcmEncoding::Signed16LittleEndian)) == "16-bit PCM");
        const Canon::ProcessedSoundEffect tone = Canon::MakeProcessedSoundEffect(Cnb::ImportWavAsImportedSound(bench.root / "tone8.wav"));
        checks.Add("MakeProcessedSoundEffect(tone8.wav)", "Unsigned8, 8000 Hz, 1 channel, 80 frames, loop 0 + 80, 80 sample bytes, frame 1 byte",
                   std::string(Canon::ProcessedPcmEncodingName(tone.encoding)) + ", " + std::to_string(tone.sampleRate) + " Hz, " + std::to_string(tone.channels) + " ch, " + std::to_string(tone.frameCount) + " frames, loop " +
                       std::to_string(tone.loopStart) + " + " + std::to_string(tone.loopLength) + ", " + std::to_string(tone.samples.size()) + " bytes, frame " + std::to_string(Canon::ProcessedFrameBytes(tone)),
                   tone.encoding == Canon::ProcessedPcmEncoding::Unsigned8 && tone.sampleRate == 8000u && tone.channels == 1u && tone.frameCount == 80u && tone.loopStart == 0u && tone.loopLength == 80u && tone.samples.size() == 80u &&
                       Canon::ProcessedFrameBytes(tone) == 1u);
        const Canon::ProcessedSoundEffect loop = Canon::MakeProcessedSoundEffect(Cnb::ImportWavAsImportedSound(bench.root / "loop16.wav"));
        checks.Add("MakeProcessedSoundEffect(loop16.wav) keeps the smpl loop", "Signed16LittleEndian, 2 channels, 40 frames, loop 10 + 20 (smpl frames 10..29, end inclusive), frame 4 bytes",
                   std::string(Canon::ProcessedPcmEncodingName(loop.encoding)) + ", " + std::to_string(loop.channels) + " ch, " + std::to_string(loop.frameCount) + " frames, loop " + std::to_string(loop.loopStart) + " + " +
                       std::to_string(loop.loopLength) + ", frame " + std::to_string(Canon::ProcessedFrameBytes(loop)),
                   loop.encoding == Canon::ProcessedPcmEncoding::Signed16LittleEndian && loop.channels == 2u && loop.frameCount == 40u && loop.loopStart == 10u && loop.loopLength == 20u && Canon::ProcessedFrameBytes(loop) == 4u);
        const Cnb::CnbSoundEffectData cnb = Canon::ToCnbSoundEffect(tone);
        checks.Add("ToCnbSoundEffect(tone8) keeps the width", "Pcm8, 8000 Hz, 1 channel, 80 frames, loop 0 + 80, the same 80 sample bytes",
                   std::string(cnb.format == Cnb::CnbAudioFormat::Pcm8 ? "Pcm8" : "another format") + ", " + std::to_string(cnb.sampleRate) + " Hz, " + std::to_string(cnb.channels) + " ch, " + std::to_string(cnb.frameCount) + " frames, loop " +
                       std::to_string(cnb.loopStart) + " + " + std::to_string(cnb.loopLength) + (cnb.samples == tone.samples ? ", same bytes" : ", different bytes"),
                   cnb.format == Cnb::CnbAudioFormat::Pcm8 && cnb.sampleRate == 8000u && cnb.channels == 1u && cnb.frameCount == 80u && cnb.loopStart == 0u && cnb.loopLength == 80u && cnb.samples == tone.samples);
        CNA::Content::Import::ImportedSound wide;
        wide.encoding = CNA::Content::Import::ImportedPcmEncoding::Signed24LittleEndian;
        wide.sampleRate = 8000;
        wide.channels = 1;
        wide.frameCount = 2;
        wide.samples.assign(6, 0);
        checks.ExpectThrow<Microsoft::Xna::Framework::Content::ContentLoadException>("MakeProcessedSoundEffect of 24-bit PCM", "ContentLoadException (the genuine importer refuses every width but 8 and 16)",
                                                                                  [&] { (void)Canon::MakeProcessedSoundEffect(wide); });
    });

    // ---- .mp3 as a streaming Song: the build carries the probed duration and deploys the media file ------------------------------------
    checks.Group(".mp3 Song", [&] {
        if (!media) {
            checks.Info(".mp3 Song route", "a build-time media decoder", "unavailable: the duration probe cannot run");
            return;
        }
        Canon::ContentBuildRequest request = bench.Request("tone.mp3", "tone");
        request.parameters.Set(Canon::SongNameParameter, std::string("Workshop tone"));
        const Canon::ContentBuildResult r = bench.pipeline->Build(request);
        bench.Publish(r);
        std::string deployed;
        for (const Canon::ContentDeploymentFile& f : r.deploymentFiles) deployed += (deployed.empty() ? "" : ", ") + f.outputPath;
        checks.Add("tone.mp3: SongImporter -> SongProcessor -> SongContentWriter, the media deployed beside it", "the Song components; deployment file tone.mp3",
                   Id(r.importer) + " -> " + Id(r.processor) + " -> " + Id(r.writer) + "; deployed " + (deployed.empty() ? "nothing" : deployed),
                   r.importer.name == Canon::SongImporter{}.Identity().name && r.processor == Canon::SongProcessor{}.Identity() && r.writer == Canon::SongContentWriter{}.Identity() && deployed == "tone.mp3");
        const std::optional<std::uint32_t> probed = BuildMedia::MakeSongDurationProbe()(bench.root / "tone.mp3");
        checks.Info("BuildTimeMedia::ProbeAudioDurationTicks(tone.mp3)", "-", std::to_string(BuildMedia::ProbeAudioDurationTicks((bench.root / "tone.mp3").string())) + " ticks; the probe answers " + (probed ? std::to_string(*probed) + " ms" : "nothing"));
        const Microsoft::Xna::Framework::Media::Song song = manager.Load<Microsoft::Xna::Framework::Media::Song>("tone");
        const long long ms = static_cast<long long>(song.getDurationProperty().getTicksProperty() / 10000);
        checks.Add("ContentManager.Load<Song>(\"tone\")", "Name \"Workshop tone\" (the name parameter), Duration = the build-time probe's answer" + (probed ? " (" + std::to_string(*probed) + " ms)" : std::string()),
                   "\"" + song.getNameProperty() + "\", " + std::to_string(ms) + " ms", song.getNameProperty() == "Workshop tone" && probed && ms == static_cast<long long>(*probed));
    });

    // ---- .wmv as a Video: frame metadata read by the build-time probe ---------------------------------------------------------------
    checks.Group(".wmv Video", [&] {
        Canon::VideoProcessor plain;   // a processor that has no probe behind it requires the frame metadata as parameters
        checks.ExpectThrow<std::exception>("VideoProcessor() without a probe: ValidateParameters({})", "refused: width, height and framesPerSecond are required (VideoContentPipeline.hpp)",
                                           [&] { plain.ValidateParameters(Canon::ContentProcessorParameters{}); });
        Canon::ContentProcessorParameters given;
        given.Set(Canon::VideoWidthParameter, std::uint64_t{32});
        given.Set(Canon::VideoHeightParameter, std::uint64_t{24});
        given.Set(Canon::VideoFramesPerSecondParameter, 10.0);
        bool accepted = true;
        try {
            plain.ValidateParameters(given);
        } catch (const std::exception&) {
            accepted = false;
        }
        checks.Add("VideoProcessor() with width, height and framesPerSecond given", "accepted", accepted ? "accepted" : "refused", accepted);
        if (!media) {
            checks.Info(".wmv Video route", "a build-time media decoder", "unavailable: the frame probe cannot run");
            return;
        }
        const Canon::ContentBuildResult r = bench.pipeline->Build(bench.Request("clip.wmv", "clip"));
        bench.Publish(r);
        checks.Add("clip.wmv: VideoImporter -> VideoProcessor -> VideoContentWriter", "the Video components of VideoContentPipeline.hpp",
                   Id(r.importer) + " -> " + Id(r.processor) + " -> " + Id(r.writer), r.importer.name == Canon::VideoImporter{}.Identity().name && r.writer == Canon::VideoContentWriter{}.Identity());
        const Microsoft::Xna::Framework::Media::Video video = manager.Load<Microsoft::Xna::Framework::Media::Video>("clip");
        const long long ms = static_cast<long long>(video.getDurationProperty().getTicksProperty() / 10000);
        checks.Add("ContentManager.Load<Video>(\"clip\")", "32x24 at 10 frames/s, 1000 ms, soundtrack Music (0): the clip's own metadata through the probe",
                   std::to_string(video.getWidthProperty()) + "x" + std::to_string(video.getHeightProperty()) + " at " + F(video.getFramesPerSecondProperty()) + " frames/s, " + std::to_string(ms) + " ms, soundtrack " +
                       std::to_string(static_cast<int>(video.getVideoSoundtrackTypeProperty())),
                   video.getWidthProperty() == 32 && video.getHeightProperty() == 24 && video.getFramesPerSecondProperty() == 10.0f && ms == 1000 && static_cast<int>(video.getVideoSoundtrackTypeProperty()) == 0);
    });

    // ---- .gltf through the Model route ---------------------------------------------------------------------------------------------------
    checks.Group(".gltf Model", [&] {
        const Canon::ContentBuildResult r = bench.pipeline->Build(bench.Request("uvsets.gltf", "uvsets"));
        bench.Publish(r);
        checks.Add("uvsets.gltf: GltfImporter -> ModelProcessor -> ModelContentWriter", "the Model components of ModelContentPipeline.hpp",
                   Id(r.importer) + " -> " + Id(r.processor) + " -> " + Id(r.writer),
                   r.importer == Canon::GltfImporter{}.Identity() && r.processor == Canon::ModelProcessor{}.Identity() && r.writer == Canon::ModelContentWriter{}.Identity());
        std::string references;
        for (const Canon::RuntimeContentReference& ref : r.runtimeReferences) references += (references.empty() ? "" : ", ") + ref.logicalName;
        checks.Info("uvsets.gltf (images embedded as data URIs): what the default build publishes", "-",
                    std::to_string(1 + r.output.additionalOutputs.size()) + " output(s), " + std::to_string(r.deploymentFiles.size()) + " deployment file(s); runtime references: " + (references.empty() ? "none" : references));
        try {
            (void)manager.Load<Graphics::Model>("uvsets");
            checks.Add("ContentManager.Load<Model>(\"uvsets\") of the default build", "loads: the build succeeded, so what it published is a loadable model", "loaded", true);
        } catch (const std::exception& e) {
            checks.Add("ContentManager.Load<Model>(\"uvsets\") of the default build", "loads: the build succeeded, so what it published is a loadable model", Checks::Short(e.what(), 160), false);
        }
        // The same model with generated child assets: the embedded images become their own .cnb outputs.
        Canon::ContentBuildRequest children = bench.Request("uvsets.gltf", "uvsets_children");
        children.parameters.Set(Canon::ModelGenerateChildAssetsParameter, true);
        const Canon::ContentBuildResult withChildren = bench.pipeline->Build(children);
        bench.Publish(withChildren);
        std::string extra;
        for (const Canon::ContentAdditionalWriteOutput& o : withChildren.output.additionalOutputs) extra += (extra.empty() ? "" : ", ") + o.logicalName;
        checks.Add("generateChildAssets = true", "the two embedded images published as child outputs beside the model", std::to_string(withChildren.output.additionalOutputs.size()) + ": " + extra,
                   withChildren.output.additionalOutputs.size() == 2);
        Graphics::Model model = manager.Load<Graphics::Model>("uvsets_children");
        int meshes = model.getMeshesProperty().getCountProperty(), parts = 0, vertices = 0, primitives = 0;
        for (Graphics::ModelMesh* mesh : model.getMeshesProperty())
            for (Graphics::ModelMeshPart* part : mesh->getMeshPartsProperty()) {
                ++parts;
                vertices += part->getNumVerticesProperty();
                primitives += part->getPrimitiveCountProperty();
            }
        checks.Add("ContentManager.Load<Model>(\"uvsets_children\")", "the quad of uvsets.gltf: 1 mesh, 1 part, 4 vertices, 2 triangles",
                   std::to_string(meshes) + " mesh, " + std::to_string(parts) + " part, " + std::to_string(vertices) + " vertices, " + std::to_string(primitives) + " triangles", meshes == 1 && parts == 1 && vertices == 4 && primitives == 2);
        Canon::ModelProcessor processor;
        Canon::ContentProcessorParameters wrong;
        wrong.Set(Canon::ModelGenerateChildAssetsParameter, std::string("yes"));
        checks.ExpectThrow<std::exception>("ModelProcessor.ValidateParameters(generateChildAssets = \"yes\")", "refused: the generated-child policy is a boolean (ModelContentPipeline.hpp)", [&] { processor.ValidateParameters(wrong); });
    });

    // ---- .fxb: a genuine XNA-compiled effect, carried byte for byte into .xnb, refused for .cnb ----------------------------------------
    checks.Group(".fxb compiled effect", [&] {
        const std::vector<std::uint8_t> fxb = ReadBytes(bench.root / "blur2tap.fxb");
        std::vector<std::uint8_t> bare(32, 0), garbage(32, 0x41);
        const std::uint32_t token = 0xFEFF0901u;
        std::memcpy(bare.data(), &token, 4);
        checks.Add("IsCompiledEffectBinary", "true for XNA's wrapped blur2tap.fxb (0xBCF00BCF) and a bare 0xFEFF0901 token; false for text",
                   std::string(Canon::IsCompiledEffectBinary(fxb) ? "true" : "false") + ", " + (Canon::IsCompiledEffectBinary(bare) ? "true" : "false") + ", " + (Canon::IsCompiledEffectBinary(garbage) ? "true" : "false"),
                   Canon::IsCompiledEffectBinary(fxb) && Canon::IsCompiledEffectBinary(bare) && !Canon::IsCompiledEffectBinary(garbage));
        const Canon::ContentBuildResult r = bench.pipeline->Build(bench.Request("blur2tap.fxb", "blur2tap", Canon::ContentOutputFormat::Xnb));
        const std::vector<std::uint8_t>& b = r.output.bytes;
        const bool tail = b.size() >= fxb.size() + 4 && Le32(b, b.size() - fxb.size() - 4) == fxb.size() && std::equal(fxb.begin(), fxb.end(), b.end() - static_cast<std::ptrdiff_t>(fxb.size()));
        checks.Add("blur2tap.fxb -> .xnb: CompiledEffectImporter -> CompiledEffectProcessor -> EffectReader payload", "the 720 bytes unchanged after their Int32 length, root reader EffectReader",
                   Id(r.importer) + " -> " + Id(r.processor) + ", " + r.output.rootReaderName + ", " + (tail ? "bytecode intact" : "bytecode not found"),
                   r.importer == Canon::CompiledEffectImporter{}.Identity() && r.processor == Canon::CompiledEffectProcessor{}.Identity() && tail && r.output.rootReaderName.rfind("Microsoft.Xna.Framework.Content.EffectReader", 0) == 0);
        const std::string reason = Canon::CompiledEffectHasNoCnbSchemaReason();
        try {
            (void)bench.pipeline->Build(bench.Request("blur2tap.fxb", "blur2tap"));
            checks.Add("blur2tap.fxb -> .cnb", "refused with CompiledEffectHasNoCnbSchemaReason()", "built", false);
        } catch (const Canon::ContentPipelineError& e) {
            const bool says = std::string(e.what()).find(reason) != std::string::npos;
            checks.Add("blur2tap.fxb -> .cnb", "ContentPipelineError carrying CompiledEffectHasNoCnbSchemaReason() (\"" + Checks::Short(reason, 60) + "\")",
                       std::string(Canon::ContentPipelineStageName(e.Stage())) + ": " + Checks::Short(e.what(), 120), says);
        }
    });

    // ---- .xnb in: XNB compatibility import of a texture this room built as .xnb, rebuilt as .cnb -----------------------------------------
    checks.Group(".xnb import", [&] {
        const Canon::ContentBuildResult asXnb = bench.pipeline->Build(bench.Request("texture.cnj", "texture", Canon::ContentOutputFormat::Xnb));
        WriteBytes(bench.root / "imported.xnb", asXnb.output.bytes);
        const Canon::ContentBuildResult r = bench.pipeline->Build(bench.Request("imported.xnb", "imported"));
        bench.Publish(r);
        const Graphics::Texture2D texture = manager.Load<Graphics::Texture2D>("imported");
        const std::vector<Color> pixels = Pixels(texture);
        checks.Add("texture .xnb -> XnbImporter -> .cnb -> Load<Texture2D>", "XnbImporter; the same 4x4 pixels as the .cnb built straight from texture.cnj",
                   Id(r.importer) + ", " + std::to_string(texture.getWidthProperty()) + "x" + std::to_string(texture.getHeightProperty()) + (pixels == direct ? ", identical pixels" : ", different pixels"),
                   r.importer == Canon::XnbImporter{}.Identity() && !direct.empty() && pixels == direct);
        Canon::XnbVideoProcessor video;
        checks.Add("XnbVideoProcessor types", "ImportedXnbVideoType -> ProcessedVideoType", video.InputType() + " -> " + video.OutputType(),
                   video.InputType() == Canon::ImportedXnbVideoType && video.OutputType() == Canon::ProcessedVideoType);
    });

    // ---- .xml: XNA's intermediate XML, written as .xnb, refused as .cnb ---------------------------------------------------------------
    checks.Group(".xml intermediate", [&] {
        const Canon::ContentBuildResult r = bench.pipeline->Build(bench.Request("vector.xml", "vector", Canon::ContentOutputFormat::Xnb));
        bench.Publish(r);
        const Vector3 v = manager.Load<Vector3>("vector");
        checks.Add("vector.xml -> XmlImporter -> .xnb -> Load<Vector3>", "importer XmlImporter; (1.5, -2, 3.25) exactly",
                   Id(r.importer) + "; (" + F(v.X) + ", " + F(v.Y) + ", " + F(v.Z) + ")", r.importer.name == "XmlImporter" && v == Vector3(1.5f, -2.0f, 3.25f));
        try {
            (void)bench.pipeline->Build(bench.Request("vector.xml", "vector"));
            checks.Add("vector.xml -> .cnb", "refused by writer resolution, naming the type (XnaXmlSourceContentPipeline.hpp)", "built", false);
        } catch (const Canon::ContentPipelineError& e) {
            checks.Add("vector.xml -> .cnb", "refused by writer resolution, naming the type (XnaXmlSourceContentPipeline.hpp)", std::string(Canon::ContentPipelineStageName(e.Stage())) + ": " + Checks::Short(e.what(), 140),
                       std::string(e.what()).find("Vector3") != std::string::npos);
        }
    });
}

}  // namespace Workshop

#endif  // MUSEUM_PIPELINE

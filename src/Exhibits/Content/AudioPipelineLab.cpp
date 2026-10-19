// Audio pipeline laboratory: the XNA content pipeline's WavImporter and SoundEffectProcessor (and SongProcessor) running in the game. A WAV
// from Content/Audio is imported into AudioContent, the processor converts it for a chosen quality (the sample rate and, for the lower
// qualities, the codec), and the museum shows both formats, both sizes and both waveforms, and plays either through a SoundEffect (the
// processed one is wrapped back into a WAV container first, so whatever codec the processor chose is what CNA decodes).
// Without CNA's pipeline library (-DMUSEUM_PIPELINE=OFF) the room says so.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "Diagnostics/Log.hpp"
#include "Exhibits/Content/PipelineSupport.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"

#ifdef MUSEUM_PIPELINE
#include "Microsoft/Xna/Framework/Audio/SoundEffect.hpp"
#include "Microsoft/Xna/Framework/Audio/SoundEffectInstance.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Audio/AudioContent.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Mp3Importer.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Processors/AudioProcessors.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Serialization/Compiler/ContentCompiler.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/WavImporter.hpp"
#endif

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

#ifdef MUSEUM_PIPELINE
namespace Xna = Microsoft::Xna::Framework::Content::Pipeline;
namespace PipeAudio = Microsoft::Xna::Framework::Content::Pipeline::Audio;
namespace Processors = Microsoft::Xna::Framework::Content::Pipeline::Processors;
namespace Compiler = Microsoft::Xna::Framework::Content::Pipeline::Serialization::Compiler;
using PipelineSupport::ImporterContext;
using PipelineSupport::MemoryContentManager;
using PipelineSupport::ProcessorContext;

const char* const kFiles[] = {"ambience", "arpeggio", "chirp", "drums", "tone", "step"};

std::string FormatText(const PipeAudio::AudioFormat& f)
{
    const std::string tag = f.getFormatProperty() == 1 ? "PCM" : f.getFormatProperty() == 2 ? "ADPCM" : f.getFormatProperty() == 3 ? "float" : "format tag " + std::to_string(f.getFormatProperty());
    return std::to_string(f.getSampleRateProperty()) + " Hz, " + std::to_string(f.getChannelCountProperty()) + " ch, " + std::to_string(f.getBitsPerSampleProperty()) + " bit, " + tag;
}

std::string Kb(std::size_t bytes)
{
    char buffer[40];
    std::snprintf(buffer, sizeof buffer, "%.1f KB", static_cast<double>(bytes) / 1024.0);
    return buffer;
}

void Put32(std::string& s, std::uint32_t v) { for (int i = 0; i < 4; ++i) s.push_back(static_cast<char>((v >> (8 * i)) & 0xFF)); }

// The processed sound as a WAV file in memory: RIFF header, the format the processor chose (its native WAVEFORMATEX bytes), the data.
std::string WavFile(const PipeAudio::AudioFormat& format, const std::vector<SharpRuntime::bytecs>& data)
{
    const std::vector<SharpRuntime::bytecs>& fmt = format.getNativeWaveFormatProperty();
    std::string out = "RIFF";
    Put32(out, static_cast<std::uint32_t>(4 + 8 + fmt.size() + (fmt.size() & 1) + 8 + data.size() + (data.size() & 1)));
    out += "WAVEfmt ";
    Put32(out, static_cast<std::uint32_t>(fmt.size()));
    out.append(reinterpret_cast<const char*>(fmt.data()), fmt.size());
    if (fmt.size() & 1) out.push_back('\0');
    out += "data";
    Put32(out, static_cast<std::uint32_t>(data.size()));
    out.append(reinterpret_cast<const char*>(data.data()), data.size());
    if (data.size() & 1) out.push_back('\0');
    return out;
}

// Peaks per column of the first channel when the data is 16-bit PCM, otherwise empty.
std::vector<float> Peaks(const PipeAudio::AudioFormat& format, const std::vector<SharpRuntime::bytecs>& data, int columns)
{
    std::vector<float> peaks;
    if (format.getFormatProperty() != 1 || format.getBitsPerSampleProperty() != 16 || data.size() < 4) return peaks;
    const std::size_t frames = data.size() / (2u * static_cast<std::size_t>(format.getChannelCountProperty()));
    const std::size_t stride = static_cast<std::size_t>(format.getChannelCountProperty());
    for (int c = 0; c < columns; ++c) {
        const std::size_t a = frames * static_cast<std::size_t>(c) / static_cast<std::size_t>(columns), b = std::max(a + 1, frames * static_cast<std::size_t>(c + 1) / static_cast<std::size_t>(columns));
        float peak = 0.0f;
        for (std::size_t i = a; i < b && i < frames; ++i) {
            std::int16_t v;
            std::memcpy(&v, data.data() + i * stride * 2, 2);
            peak = std::max(peak, std::abs(static_cast<float>(v)) / 32768.0f);
        }
        peaks.push_back(peak);
    }
    return peaks;
}
#endif

class AudioPipelineLab final : public ScreenExhibit {
public:
    AudioPipelineLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 7.6f) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel("Processing audio", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
#ifdef MUSEUM_PIPELINE
        p.AddChoice("Audio importer", {"WavImporter (.wav)", "Mp3Importer (arpeggio.mp3)"}, 0, [this](int v) { mp3_ = v == 1; dirty_ = true; });
        p.AddChoice("WAV file (WavImporter)", {kFiles[0], kFiles[1], kFiles[2], kFiles[3], kFiles[4], kFiles[5]}, 1, [this](int v) { file_ = v; dirty_ = true; });
        p.AddChoice("SoundEffectProcessor.Quality", {"Low", "Medium", "Best"}, 2, [this](int v) { quality_ = v; dirty_ = true; });
        p.AddButton("Play the imported sound", [this] { Play(false); });
        p.AddButton("Play the processed sound", [this] { Play(true); });
        p.AddButton("ContentCompiler: write an .xnb, load it back, play", [this] { CompileAndLoad(); });
        p.AddReadout("Importer attribute", [this] { return importer_; });
        p.AddReadout("Imported", [this] { return imported_; });
        p.AddReadout("Processed", [this] { return processed_; });
        p.AddReadout("Size", [this] { return size_; });
        p.AddReadout("Duration / loop", [this] { return duration_; });
        p.AddReadout("SongProcessor", [this] { return song_; });
        p.AddReadout("Compiled .xnb", [this] { return compiled_; });
        p.AddReadout("Playback", [this] { return playback_; });
#else
        p.AddReadout("Content pipeline", [] { return std::string("not built into this museum"); });
#endif
        AddInfoSign(b, "Importing and processing a sound",
                    {"WavImporter and Mp3Importer decode a source file into AudioContent: a format, the sample data, a duration and loop points. Their attributes declare the accepted extension and default processor. SoundEffectProcessor converts it for the target: lower qualities resample to a lower rate and",
                     "may switch to a compressed codec. Here both versions are shown and can be heard; SongProcessor, for music files, keeps only the file name and duration because songs stream from disk."});
    }

    ~AudioPipelineLab() override { StopAll(); }
    void OnLeave() override { StopAll(); }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
#ifdef MUSEUM_PIPELINE
        if (dirty_) { dirty_ = false; Process(); }
#endif
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        Texture2D& white = *S().whiteTexture;
        Text(sb, "A sound, imported and processed", 24, 6, 0.8f, Kit::kGold);
#ifdef MUSEUM_PIPELINE
        const auto wave = [&](const std::vector<float>& peaks, float y, const char* title, const std::string& detail, Color c) {
            Text(sb, title, 34, y, 0.55f, Kit::kGold);
            Text(sb, detail, 200, y + 2, 0.46f, Kit::kDim);
            const float mid = y + 78.0f;
            sb.Draw(white, Rectangle(34, static_cast<int>(mid) - 70, 890, 140), Kit::Rgba(18, 24, 38));
            if (peaks.empty()) { Text(sb, "(the waveform is drawn for 16-bit PCM only)", 330, mid - 8, 0.5f, Kit::kDim); return; }
            for (std::size_t i = 0; i < peaks.size(); ++i) {
                const int h = std::max(1, static_cast<int>(peaks[i] * 66.0f));
                sb.Draw(white, Rectangle(36 + static_cast<int>(i) * 2, static_cast<int>(mid) - h, 2, h * 2), c);
            }
        };
        wave(importedPeaks_, 44.0f, "Imported", imported_, Kit::Rgba(120, 200, 255));
        wave(processedPeaks_, 270.0f, "Processed", processed_, Kit::Rgba(255, 190, 90));
        if (!error_.empty()) Text(sb, error_, 34, 500, 0.5f, Kit::Rgba(255, 190, 150));
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
    void Process()
    {
        error_.clear();
        StopAll();
        importedWav_.clear();
        processedWav_.clear();
        processedContent_.reset();
        songContent_.reset();
        compiled_ = "-";
        try {
            ImporterContext importerContext;
            std::shared_ptr<PipeAudio::AudioContent> audio;
            Xna::ContentImporterAttribute attribute = Xna::WavImporter::Attribute();
            if (mp3_) {
                Xna::Mp3Importer importer;
                audio = importer.Import(S().contentRoot + "/Audio/arpeggio.mp3", importerContext);
                attribute = Xna::Mp3Importer::Attribute();
            } else {
                Xna::WavImporter importer;
                audio = importer.Import(S().contentRoot + "/Audio/" + kFiles[file_] + ".wav", importerContext);
            }
            importer_ = attribute.getFileExtensionsProperty()[0] + ", \"" + attribute.getDisplayNameProperty() +
                        "\", default " + attribute.getDefaultProcessorProperty();
            imported_ = FormatText(*audio->getFormatProperty());
            const std::vector<SharpRuntime::bytecs> importedData = audio->getDataProperty();
            importedPeaks_ = Peaks(*audio->getFormatProperty(), importedData, 445);
            importedWav_ = WavFile(*audio->getFormatProperty(), importedData);

            Processors::SoundEffectProcessor processor;
            processor.setQualityProperty(quality_ == 0 ? PipeAudio::ConversionQuality::Low : quality_ == 1 ? PipeAudio::ConversionQuality::Medium : PipeAudio::ConversionQuality::Best);
            ProcessorContext context;
            const std::shared_ptr<Processors::SoundEffectContent> sound = processor.Process(audio, context);
            processedContent_ = sound;
            processed_ = FormatText(*sound->Format());
            processedPeaks_ = Peaks(*sound->Format(), sound->Data(), 445);
            processedWav_ = WavFile(*sound->Format(), sound->Data());
            char buffer[128];
            std::snprintf(buffer, sizeof buffer, "%s -> %s (%.0f%%)", Kb(importedData.size()).c_str(), Kb(sound->Data().size()).c_str(),
                          importedData.empty() ? 0.0 : 100.0 * static_cast<double>(sound->Data().size()) / static_cast<double>(importedData.size()));
            size_ = buffer;
            std::snprintf(buffer, sizeof buffer, "%.2f s, loop %d + %d samples", sound->Duration().getTotalSecondsProperty(), static_cast<int>(sound->LoopStart()), static_cast<int>(sound->LoopLength()));
            duration_ = buffer;
            Processors::SongProcessor songProcessor;
            songProcessor.setQualityProperty(quality_ == 0 ? PipeAudio::ConversionQuality::Low : quality_ == 1 ? PipeAudio::ConversionQuality::Medium : PipeAudio::ConversionQuality::Best);
            const std::shared_ptr<Processors::SongContent> song = songProcessor.Process(audio, context);
            songContent_ = song;
            std::snprintf(buffer, sizeof buffer, "file name \"%s\", %.2f s", std::filesystem::path(song->FileName()).filename().string().c_str(), song->Duration().getTotalSecondsProperty());
            song_ = buffer;
        } catch (const std::exception& e) {
            error_ = std::string("failed: ") + std::string(e.what()).substr(0, 90);
            Museum::Log("audiopipeline", "%s", e.what());
        }
    }

    void Play(bool processed)
    {
        StopAll();
        const std::string& wav = processed ? processedWav_ : importedWav_;
        if (wav.empty()) { playback_ = "nothing imported"; return; }
        try {
            std::istringstream stream(wav, std::ios::binary);
            effect_.reset(Microsoft::Xna::Framework::Audio::SoundEffect::FromStream(stream));
            instance_ = std::make_unique<Microsoft::Xna::Framework::Audio::SoundEffectInstance>(effect_->CreateInstance());
            instance_->Play();
            playback_ = std::string(processed ? "processed" : "imported") + " sound playing (" + Kb(wav.size()) + " as a WAV)";
        } catch (const std::exception& e) {
            playback_ = std::string("refused: ") + std::string(e.what()).substr(0, 60);
        }
    }

    // The processed sound compiled by ContentCompiler into an .xnb (and the song next to it), loaded back through a ContentManager that reads the
    // bytes from memory, and played: the whole pipeline in one button.
    void CompileAndLoad()
    {
        StopAll();
        if (!processedContent_) { compiled_ = "nothing processed"; return; }
        try {
            Compiler::ContentCompiler compiler;
            Compiler::CompileOptions options;
            options.assetName = "compiled";
            const auto sound = compiler.Compile<Processors::SoundEffectContent>(processedContent_, options);
            const auto song = compiler.Compile<Processors::SongContent>(songContent_, options);
            if (!manager_) manager_ = std::make_unique<MemoryContentManager>(&S().game->getServicesProperty());
            manager_->bytes = sound.bytes;
            manager_->Unload();
            effect_ = std::make_unique<Microsoft::Xna::Framework::Audio::SoundEffect>(manager_->Load<Microsoft::Xna::Framework::Audio::SoundEffect>("compiled"));
            instance_ = std::make_unique<Microsoft::Xna::Framework::Audio::SoundEffectInstance>(effect_->CreateInstance());
            instance_->Play();
            char buffer[160];
            std::snprintf(buffer, sizeof buffer, "sound %s, song %zu bytes (a name and a duration)", Kb(sound.bytes.size()).c_str(), song.bytes.size());
            compiled_ = buffer;
            playback_ = "the sound loaded from the compiled bytes is playing";
        } catch (const std::exception& e) {
            compiled_ = std::string("failed: ") + std::string(e.what()).substr(0, 70);
        }
    }

    void StopAll()
    {
        try {
            if (instance_) instance_->Stop();
        } catch (const std::exception&) {}
        instance_.reset();
        effect_.reset();
    }

    std::unique_ptr<MemoryContentManager> manager_;
    std::shared_ptr<Processors::SoundEffectContent> processedContent_;
    std::shared_ptr<Processors::SongContent> songContent_;
    std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffect> effect_;
    std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffectInstance> instance_;
    std::string importedWav_, processedWav_;
    std::vector<float> importedPeaks_, processedPeaks_;
    std::string importer_ = "-", imported_ = "-", processed_ = "-", size_ = "-", duration_ = "-", song_ = "-", playback_ = "-", compiled_ = "-", error_;
    int file_ = 1, quality_ = 2;
    bool mp3_ = false, dirty_ = true;
#endif
};

}  // namespace

std::unique_ptr<Exhibit> MakeAudioPipelineLab(const RoomDef& room, Services& services)
{
    return std::make_unique<AudioPipelineLab>(room, services);
}

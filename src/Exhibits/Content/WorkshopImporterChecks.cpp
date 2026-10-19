// Pipeline Workshop, mode "importers" (room wk-importers): the importers, processors and probes no other room runs, on tiny files that
// tools/gen_workshop.py makes with ffmpeg (Content/Workshop: a 0.5 s 440 Hz tone as MP3 and WMA, a 1 s 32x24 WMV with and
// without a sound track). Whether this build can decode compressed media at all (BuildTimeMedia::IsAvailable) and whether an XMA
// encoder is attached are reported as information: an unavailable codec is a refusal to record, never a pass. Lossy codecs cannot be
// compared sample for sample, so decoded audio is checked for what survives coding exactly: the channel count, the rate, and the tone's
// frequency (counted from zero crossings, 1 % tolerance); durations are allowed one codec frame of padding.
#ifdef MUSEUM_PIPELINE

#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "CNA/Content/Pipeline/BuildTimeMediaDecoder.hpp"
#include "CNA/Content/Pipeline/XmaEncoderService.hpp"
#include "Exhibits/Content/PipelineSupport.hpp"
#include "Exhibits/Content/WorkshopChecks.hpp"
#include "Exhibits/Services.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Audio/AudioContent.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/ContentItem.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Processors/PassThroughProcessor.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/WmaImporter.hpp"
#include "Microsoft/Xna/Framework/TitleLocation.hpp"
#include "System/IO/FileNotFoundException.hpp"

namespace {

namespace Xna = Microsoft::Xna::Framework::Content::Pipeline;
namespace Media = CNA::Content::Pipeline::BuildTimeMedia;
namespace Canon = CNA::Content::Pipeline;
using Workshop::Checks;
using Workshop::F;
using Workshop::Yes;

class Parcel final : public Xna::ContentItem {
public:
    static constexpr std::string_view XnaTypeName = "Museum.Workshop.Parcel";
    int weight = 0;
    [[nodiscard]] const std::string& GetTypeName() const override
    {
        static const std::string name{XnaTypeName};
        return name;
    }
};

// Rising zero crossings of the first channel in the middle half of the samples (clear of codec delay and padding), per second.
double Frequency(const std::vector<std::uint8_t>& pcm, int channels, int rate)
{
    const std::size_t frames = pcm.size() / 2 / static_cast<std::size_t>(channels);
    if (frames < 100 || rate <= 0) return 0.0;
    const auto sample = [&](std::size_t i) {
        std::int16_t s = 0;
        std::memcpy(&s, pcm.data() + i * 2 * static_cast<std::size_t>(channels), 2);
        return static_cast<int>(s);
    };
    const std::size_t from = frames / 4, to = frames * 3 / 4;
    std::size_t first = 0, last = 0;
    int crossings = 0;
    for (std::size_t i = from + 1; i < to; ++i)
        if (sample(i - 1) < 0 && sample(i) >= 0) {
            if (crossings == 0) first = i;
            last = i;
            ++crossings;
        }
    return crossings < 2 ? 0.0 : static_cast<double>(crossings - 1) * rate / static_cast<double>(last - first);
}

double Seconds(std::int64_t ticks) { return static_cast<double>(ticks) / 1e7; }

std::string DescribeAudio(const Media::DecodedAudio& a)
{
    return std::to_string(a.channels) + " ch, " + std::to_string(a.sampleRate) + " Hz, " + std::to_string(a.bitsPerSample) + " bit, " + F(Seconds(a.durationTicks), "%.3f") + " s, " + F(Frequency(a.pcm, a.channels, a.sampleRate), "%.1f") + " Hz";
}

bool Near(double got, double want, double tolerance) { return std::abs(got - want) <= tolerance; }

}  // namespace

namespace Workshop {

void ImporterChecks(Checks& checks, Services& services)
{
    const std::string dir = services.contentRoot + "/Workshop/";

    // ---- PassThroughProcessor: XNA returns its input argument, the very object --------------------------------------------------------
    checks.Group("PassThroughProcessor", [&] {
        Xna::Processors::PassThroughProcessor processor;
        PipelineSupport::ProcessorContext context;
        auto parcel = std::make_shared<Parcel>();
        parcel->weight = 7;
        const Xna::ContentObject out = processor.Process(Xna::Box<Parcel>(parcel), context);
        const std::shared_ptr<Parcel> back = Xna::Unbox<Parcel>(out);
        checks.Add("PassThroughProcessor.Process(an object)", "the same object back (XNA: return input)", back == parcel ? "the same object" : (back ? "a different object" : "null"), back == parcel && back->weight == 7);
        const Xna::ContentObject text = processor.Process(Xna::Box<std::string>(std::string("unchanged")), context);
        checks.Add("PassThroughProcessor.Process(a string)", "\"unchanged\", typed System.String", "\"" + Xna::Unbox<std::string>(text) + "\", " + text.StableType(), Xna::Unbox<std::string>(text) == "unchanged" && text.StableType() == "System.String");
        checks.Add("PassThroughProcessor input and output types", "System.Object -> System.Object (ContentProcessor<object, object>)", processor.getInputTypeNameProperty() + " -> " + processor.getOutputTypeNameProperty(),
                   processor.getInputTypeNameProperty() == "System.Object" && processor.getOutputTypeNameProperty() == "System.Object");
    });

    // ---- the build-time media decoder ------------------------------------------------------------------------------------------------
    const bool decoder = Media::IsAvailable();
    checks.Info("BuildTimeMedia::IsAvailable", "-", decoder ? "available: " + Media::Identity() : "unavailable: " + Media::UnavailableReason());
    checks.Group("WmaImporter", [&] {
        const Xna::ContentImporterAttribute attribute = Xna::WmaImporter::Attribute();
        const auto& extensions = attribute.getFileExtensionsProperty();
        checks.Add("WmaImporter.Attribute", "extension .wma, default processor SongProcessor", (extensions.empty() ? std::string("no extension") : extensions[0]) + ", " + attribute.getDefaultProcessorProperty(),
                   extensions.size() == 1 && extensions[0] == ".wma" && attribute.getDefaultProcessorProperty() == "SongProcessor");
        PipelineSupport::ImporterContext context;
        checks.ExpectThrow<System::IO::FileNotFoundException>("WmaImporter.Import of a missing file", "FileNotFoundException (XNA checks FileInfo.Exists first)",
                                                              [&] { (void)Xna::WmaImporter().Import(dir + "absent.wma", context); });
        if (!decoder) {
            checks.ExpectThrow<std::exception>("WmaImporter.Import without a decoder", "a refusal naming the missing decoder", [&] { (void)Xna::WmaImporter().Import(dir + "tone.wma", context); });
            return;
        }
        const std::shared_ptr<Xna::Audio::AudioContent> audio = Xna::WmaImporter().Import(dir + "tone.wma", context);
        const auto& format = *audio->getFormatProperty();
        checks.Add("WmaImporter.Import(tone.wma): file type and channels", "AudioFileType.Wma, 1 channel (the channel count survives decoding)",
                   std::to_string(static_cast<int>(audio->getFileTypeProperty())) + ", " + std::to_string(format.getChannelCountProperty()) + " ch",
                   audio->getFileTypeProperty() == Xna::Audio::AudioFileType::Wma && format.getChannelCountProperty() == 1);
        // XNA reports the decoder's PCM, not the source format, on the measured MP3 route (44100 Hz, 16 bit, tag 1); its WMA route could
        // not be measured (CNA docs/xna-content-pipeline-media.md sections 2 and 3), so the reported format is information only.
        checks.Info("WmaImporter.Import(tone.wma): reported format", "-",
                    "tag " + std::to_string(format.getFormatProperty()) + ", " + std::to_string(format.getSampleRateProperty()) + " Hz, " + std::to_string(format.getBitsPerSampleProperty()) + " bit (source: WMA 2, 22050 Hz)");
        // AudioContent.Duration is the length of the audio data in whole milliseconds. tone.wma's ASF header declares 0.500 s, but the
        // audio it actually decodes to is 10240 samples at 22050 Hz = 464.4 ms (ffmpeg's own decoder gives the same count: its WMA encoder
        // drops the last partial frame). Which of the two XNA's WMA route would report could not be measured (CNA's media notes, section
        // 3), so the check is that Duration agrees with the PCM the importer holds, and the declared length is information.
        const long long ms = static_cast<long long>(audio->getDurationProperty().getTicksProperty() / 10000);
        const double pcmSeconds = format.getSampleRateProperty() > 0 ? static_cast<double>(audio->getDataProperty().size()) / (2.0 * format.getChannelCountProperty() * format.getSampleRateProperty()) : 0.0;
        checks.Add("WmaImporter.Import(tone.wma): Duration", "the length of the PCM it holds, 464 ms (10240 samples at 22050 Hz)", std::to_string(ms) + " ms; the PCM lasts " + F(pcmSeconds, "%.4f") + " s",
                   ms == static_cast<long long>(pcmSeconds * 1000.0) && ms == 464);
        checks.Info("tone.wma: declared length against decodable length", "-", F(Seconds(Media::ProbeAudioDurationTicks(dir + "tone.wma")), "%.3f") + " s declared by the container, 0.464 s of decodable audio");
    });

    if (decoder) {
        checks.Group("DecodeAudio", [&] {
            const Media::DecodedAudio mp3 = Media::DecodeAudio(dir + "tone.mp3", 0, Media::AudioSourceFormat::Mpeg);
            const double f = Frequency(mp3.pcm, mp3.channels, mp3.sampleRate);
            checks.Add("DecodeAudio(tone.mp3, keep rate, Mpeg)", "1 ch, 22050 Hz, 16 bit, 440 Hz +-1 %, 0.5 s + at most one 1152-sample frame of padding each side", DescribeAudio(mp3),
                       mp3.channels == 1 && mp3.sampleRate == 22050 && mp3.bitsPerSample == 16 && Near(f, 440.0, 4.4) && Seconds(mp3.durationTicks) >= 0.5 - 1e-3 && Seconds(mp3.durationTicks) <= 0.5 + 2 * 1152.0 / 22050.0);
            const Media::DecodedAudio resampled = Media::DecodeAudio(dir + "tone.mp3", 44100);
            const double f2 = Frequency(resampled.pcm, resampled.channels, resampled.sampleRate);
            checks.Add("DecodeAudio(tone.mp3, 44100)", "1 ch, 44100 Hz, still 440 Hz +-1 %, twice the samples", DescribeAudio(resampled),
                       resampled.channels == 1 && resampled.sampleRate == 44100 && Near(f2, 440.0, 4.4) && Near(static_cast<double>(resampled.pcm.size()), 2.0 * static_cast<double>(mp3.pcm.size()), 64.0));
            const Media::DecodedAudio wma = Media::DecodeAudio(dir + "tone.wma", 0, Media::AudioSourceFormat::WindowsMedia);
            checks.Add("DecodeAudio(tone.wma, keep rate, WindowsMedia)", "1 ch, 22050 Hz, 440 Hz +-1 %", DescribeAudio(wma),
                       wma.channels == 1 && wma.sampleRate == 22050 && Near(Frequency(wma.pcm, wma.channels, wma.sampleRate), 440.0, 4.4));
            // Resampling changes the number of samples, never the length of the sound: twice the rate, twice the samples.
            const Media::DecodedAudio wma44 = Media::DecodeAudio(dir + "tone.wma", 44100, Media::AudioSourceFormat::WindowsMedia);
            const double s22 = static_cast<double>(wma.pcm.size()) / 2.0 / 22050.0, s44 = static_cast<double>(wma44.pcm.size()) / 2.0 / 44100.0;
            checks.Add("DecodeAudio(tone.wma, 44100): the sound keeps its length", "the PCM lasts as long as at 22050 Hz (within 1 ms)", F(s44, "%.4f") + " s at 44100 vs " + F(s22, "%.4f") + " s at 22050",
                       wma44.sampleRate == 44100 && Near(s44, s22, 0.001));
            const double m22 = static_cast<double>(mp3.pcm.size()) / 2.0 / 22050.0, m44 = static_cast<double>(resampled.pcm.size()) / 2.0 / 44100.0;
            checks.Add("DecodeAudio(tone.mp3, 44100): the sound keeps its length", "the PCM lasts as long as at 22050 Hz (within 1 ms)", F(m44, "%.4f") + " s at 44100 vs " + F(m22, "%.4f") + " s at 22050", Near(m44, m22, 0.001));
            checks.ExpectThrow<std::runtime_error>("DecodeAudio(tone.wma, required Mpeg)", "std::runtime_error: not of the required format (documented)",
                                                   [&] { (void)Media::DecodeAudio(dir + "tone.wma", 0, Media::AudioSourceFormat::Mpeg); });
            checks.ExpectThrow<std::runtime_error>("DecodeAudio(clip.wmv)", "std::runtime_error: no audio stream (documented)", [&] { (void)Media::DecodeAudio(dir + "clip.wmv", 0); });
            checks.ExpectThrow<std::runtime_error>("DecodeAudio of a missing file", "std::runtime_error (documented)", [&] { (void)Media::DecodeAudio(dir + "absent.mp3", 0); });
        });
        checks.Group("ProbeVideo", [&] {
            const Media::ProbedVideo plain = Media::ProbeVideo(dir + "clip.wmv");
            checks.Add("ProbeVideo(clip.wmv)", "32 x 24, 10 frames/s, 1.0 s, no audio",
                       std::to_string(plain.width) + " x " + std::to_string(plain.height) + ", " + F(plain.framesPerSecond) + " frames/s, " + F(Seconds(plain.durationTicks), "%.3f") + " s, audio " + Yes(plain.hasAudio),
                       plain.width == 32 && plain.height == 24 && plain.framesPerSecond == 10.0f && Near(Seconds(plain.durationTicks), 1.0, 0.1) && !plain.hasAudio);
            const Media::ProbedVideo sound = Media::ProbeVideo(dir + "clip_audio.wmv");
            checks.Add("ProbeVideo(clip_audio.wmv)", "32 x 24, 10 frames/s, has audio", std::to_string(sound.width) + " x " + std::to_string(sound.height) + ", audio " + Yes(sound.hasAudio),
                       sound.width == 32 && sound.height == 24 && sound.hasAudio);
            checks.ExpectThrow<std::runtime_error>("ProbeVideo(tone.wma)", "std::runtime_error: no video stream (documented)", [&] { (void)Media::ProbeVideo(dir + "tone.wma"); });
            const double wmaSeconds = Seconds(Media::ProbeAudioDurationTicks(dir + "tone.wma"));
            checks.Add("ProbeAudioDurationTicks(tone.wma)", "0.5 s within one WMA frame", F(wmaSeconds, "%.4f") + " s", Near(wmaSeconds, 0.5, 0.093));
        });
        checks.Group("EncodeWindowsMediaAudio", [&] {
            // 0.25 s of a 1 kHz tone at 44100 Hz, encoded to WMA in the build tree and decoded again.
            std::vector<std::uint8_t> pcm;
            const int rate = 44100;
            for (int i = 0; i < rate / 4; ++i) {
                const auto s = static_cast<std::int16_t>(std::lround(12000.0 * std::sin(2.0 * 3.14159265358979323846 * 1000.0 * i / rate)));
                pcm.push_back(static_cast<std::uint8_t>(s & 0xFF));
                pcm.push_back(static_cast<std::uint8_t>((s >> 8) & 0xFF));
            }
            const std::filesystem::path out = std::filesystem::path(Microsoft::Xna::Framework::TitleLocation::Path()) / "WorkshopOutput" / "encoded.wma";
            std::filesystem::create_directories(out.parent_path());
            Media::EncodeWindowsMediaAudio(pcm, 1, rate, 64000, out.string());
            const Media::DecodedAudio back = Media::DecodeAudio(out.string(), 0, Media::AudioSourceFormat::WindowsMedia);
            checks.Add("EncodeWindowsMediaAudio, then DecodeAudio", "a WMA file: 1 ch, 44100 Hz, 1000 Hz +-1 %", DescribeAudio(back),
                       back.channels == 1 && back.sampleRate == 44100 && Near(Frequency(back.pcm, back.channels, back.sampleRate), 1000.0, 10.0));
        });
    }

    // ---- the XMA encoder service -----------------------------------------------------------------------------------------------------
    checks.Group("XmaEncoderService", [&] {
        checks.Add("XmaEncodeQualityName", "low, medium, best (documented spellings)",
                   std::string(Canon::XmaEncodeQualityName(Canon::XmaEncodeQuality::Low)) + ", " + Canon::XmaEncodeQualityName(Canon::XmaEncodeQuality::Medium) + ", " + Canon::XmaEncodeQualityName(Canon::XmaEncodeQuality::Best),
                   std::string(Canon::XmaEncodeQualityName(Canon::XmaEncodeQuality::Low)) == "low" && std::string(Canon::XmaEncodeQualityName(Canon::XmaEncodeQuality::Medium)) == "medium" &&
                       std::string(Canon::XmaEncodeQualityName(Canon::XmaEncodeQuality::Best)) == "best");
        const auto none = Canon::MakeUnavailableXmaEncoder();
        Canon::XmaEncodeRequest request;
        request.pcm.assign(4410 * 2, 0);
        request.sampleRate = 44100;
        const Canon::XmaEncodeResult refused = none->Encode(request);
        const std::string reason = none->UnavailableReason();
        checks.Add("MakeUnavailableXmaEncoder", "never available; reason begins \"XMA ENCODER EXTERNALLY UNAVAILABLE\"; Encode fails with no data",
                   "available " + Yes(none->Available()) + ", \"" + Checks::Short(reason, 60) + "\", encode " + (refused.succeeded ? "succeeded" : "failed") + ", " + std::to_string(refused.data.size()) + " bytes",
                   !none->Available() && reason.rfind(Canon::XmaEncoderUnavailableSentence, 0) == 0 && !refused.succeeded && refused.data.empty());
        const auto attached = Canon::BuildXmaEncoder();
        checks.Info("BuildXmaEncoder (this process)", "-", attached->Available() ? "available: " + attached->Identity().ToString() : "unavailable: " + Checks::Short(attached->UnavailableReason(), 120));
        const auto external = Canon::MakeExternalXmaEncoder();
        checks.Info("MakeExternalXmaEncoder() with no executable named", "-", external->Available() ? "found one: " + external->Identity().ToString() : "unavailable: " + Checks::Short(external->UnavailableReason(), 120));
    });
}

}  // namespace Workshop

#endif  // MUSEUM_PIPELINE

// Pipeline Workshop, mode "cnbloader" (room wk-cnbloader): CNA's native compiled container, .cnb, extended with a game-defined asset
// type through its one public route. The room mints the type's identifier (CnbAssetTypeIdFromName, FNV-1a-32 | 0x80000000, recomputed
// here), writes a file with CnbWriter and CnbByteWriter into the build tree (next to the executable, never into the source tree),
// registers a loader with CnbLoaderRegistry::Register that decodes it with CnbDocument and CnbByteReader, and loads it through an
// ordinary ContentManager. The documented refusals are checked, the registration is removed and re-added, and it is removed for good
// when the visitor leaves the room. The XML half: a ContentTypeSerializer of the game's own registered with the IntermediateSerializer,
// writing a compact text form of one member, round-tripped exactly.
#ifdef MUSEUM_PIPELINE

#include <any>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "CNA/Content/Cnb/CnbByteReader.hpp"
#include "CNA/Content/Cnb/CnbByteWriter.hpp"
#include "CNA/Content/Cnb/CnbCrc32c.hpp"
#include "CNA/Content/Cnb/CnbDocument.hpp"
#include "CNA/Content/Cnb/CnbFormat.hpp"
#include "CNA/Content/Cnb/CnbLoaderRegistry.hpp"
#include "CNA/Content/Cnb/CnbWriter.hpp"
#include "Exhibits/Content/WorkshopChecks.hpp"
#include "Exhibits/Services.hpp"
#include "Microsoft/Xna/Framework/Content/ContentLoadException.hpp"
#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/Content/ContentSerializerAttribute.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/ContentItem.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/InvalidContentException.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Serialization/Intermediate/ContentTypeSerializer.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Serialization/Intermediate/IntermediateReader.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Serialization/Intermediate/IntermediateSerializer.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Serialization/Intermediate/IntermediateWriter.hpp"
#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/TitleLocation.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"
#include "System/Xml/XmlReader.hpp"
#include "System/Xml/XmlWriter.hpp"
#include "System/Xml/XmlWriterSettings.hpp"

using namespace Microsoft::Xna::Framework;

namespace {

namespace Cnb = CNA::Content::Cnb;
namespace Xna = Microsoft::Xna::Framework::Content::Pipeline;
namespace Intermediate = Microsoft::Xna::Framework::Content::Pipeline::Serialization::Intermediate;
using CNA::Content::CnbLoaderRegistry;
using Microsoft::Xna::Framework::Content::ContentLoadException;
using Microsoft::Xna::Framework::Content::ContentManager;
using Microsoft::Xna::Framework::Content::ContentSerializerAttribute;
using Workshop::F;
using Workshop::Checks;

constexpr const char* kFossilType = "Museum.Workshop.Fossil";
constexpr Cnb::CnbChunkId kFossilChunk = Cnb::MakeChunkId('F', 'O', 'S', 'L');

// The game's runtime type.
struct Fossil {
    std::string name;
    std::int32_t era = 0;
    float length = 0.0f;
    double ageYears = 0.0;
    std::uint64_t specimenId = 0;
    std::vector<Vector3> outline;
};

std::uint32_t Fnv1a32(const std::string& text)
{
    std::uint32_t h = 2166136261u;
    for (const unsigned char c : text) {
        h ^= c;
        h *= 16777619u;
    }
    return h;
}

Fossil Sample()
{
    Fossil f;
    f.name = "Archaeopteryx lithographica";
    f.era = -150;
    f.length = 0.505f;
    f.ageYears = 1.505e8;
    f.specimenId = 0x0123456789ABCDEFull;
    f.outline = {Vector3(0.0f, 0.1f, 0.2f), Vector3(-1.5f, 2.25f, 1.0f / 3.0f), Vector3(1e-9f, -0.0f, 1e9f)};
    return f;
}

std::vector<std::uint8_t> WriteFossil(const Fossil& f, std::uint32_t typeId, const std::string& metadataName)
{
    Cnb::CnbWriter writer(typeId, 1u);
    writer.SetMetadata(metadataName, "fossil");
    Cnb::CnbByteWriter payload;
    payload.WriteString(f.name);
    payload.WriteI32(f.era);
    payload.WriteF32(f.length);
    payload.WriteF64(f.ageYears);
    payload.WriteU64(f.specimenId);
    payload.WriteU32(static_cast<std::uint32_t>(f.outline.size()));
    for (const Vector3& p : f.outline) {
        payload.WriteF32(p.X);
        payload.WriteF32(p.Y);
        payload.WriteF32(p.Z);
    }
    writer.AddChunk(kFossilChunk, payload.Take(), Cnb::CnbChunkFlags::Mandatory, 4u);
    return writer.Build();
}

std::any LoadFossil(const Cnb::CnbDocument& document, ContentManager&, const std::string&)
{
    document.RequireAsset(Cnb::CnbAssetTypeIdFromName(kFossilType), 1u);
    Cnb::CnbByteReader r = document.OpenChunk(document.RequireSingle(kFossilChunk));
    Fossil f;
    f.name = r.ReadString();
    f.era = r.ReadI32();
    f.length = r.ReadF32();
    f.ageYears = r.ReadF64();
    f.specimenId = r.ReadU64();
    const std::uint32_t count = r.ReadCount(12u, "outline points");
    for (std::uint32_t i = 0; i < count; ++i) {
        const float x = r.ReadF32(), y = r.ReadF32(), z = r.ReadF32();
        f.outline.emplace_back(x, y, z);
    }
    r.RequireExhausted();
    return std::any(std::move(f));
}

bool Same(const Fossil& a, const Fossil& b)
{
    if (a.name != b.name || a.era != b.era || a.length != b.length || a.ageYears != b.ageYears || a.specimenId != b.specimenId || a.outline.size() != b.outline.size()) return false;
    for (std::size_t i = 0; i < a.outline.size(); ++i)
        if (!(a.outline[i] == b.outline[i])) return false;
    return true;
}

std::filesystem::path OutputFolder() { return std::filesystem::path(TitleLocation::Path()) / "WorkshopOutput"; }

void WriteFile(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!out) throw std::runtime_error("cannot write " + path.string());
}

Fossil LoadThroughManager(Services& s, const std::string& asset)
{
    ContentManager manager(&s.game->getServicesProperty(), OutputFolder().string());
    return manager.Load<Fossil>(asset);
}

// ---- the XML half: a custom ContentTypeSerializer ---------------------------------------------------------------------------------

class SeismogramContent final : public Xna::ContentItem {
public:
    static constexpr std::string_view XnaTypeName = "Museum.Workshop.Seismogram";
    std::string station;
    std::vector<std::pair<float, float>> trace;   // (seconds, amplitude)
    [[nodiscard]] const std::string& GetTypeName() const override
    {
        static const std::string name{XnaTypeName};
        return name;
    }
};

std::string TraceText(const std::vector<std::pair<float, float>>& trace)
{
    std::string out;
    for (const auto& [t, a] : trace) {
        out += (out.empty() ? "" : " ") + F(t, "%.9g") + ":" + F(a, "%.9g");
    }
    return out;
}

int serializeCalls = 0, deserializeCalls = 0;

// The trace is written as one compact text element instead of a list of pairs: the reason a game writes its own serializer.
class SeismogramSerializer final : public Intermediate::ContentTypeSerializer<SeismogramContent> {
protected:
    void Serialize(Intermediate::IntermediateWriter& output, const std::shared_ptr<SeismogramContent>& value, const ContentSerializerAttribute&) override
    {
        ++serializeCalls;
        ContentSerializerAttribute station, trace;
        station.setElementNameProperty("Station");
        trace.setElementNameProperty("Trace");
        output.WriteObject<std::string>(value->station, station);
        output.WriteObject<std::string>(TraceText(value->trace), trace);
    }

    [[nodiscard]] std::shared_ptr<SeismogramContent> Deserialize(Intermediate::IntermediateReader& input, const ContentSerializerAttribute&, std::shared_ptr<SeismogramContent> existing) override
    {
        ++deserializeCalls;
        if (!existing) existing = std::make_shared<SeismogramContent>();
        ContentSerializerAttribute station, trace;
        station.setElementNameProperty("Station");
        trace.setElementNameProperty("Trace");
        existing->station = input.ReadObject<std::string>(station);
        std::istringstream text(input.ReadObject<std::string>(trace));
        std::string pair;
        existing->trace.clear();
        while (text >> pair) {
            const std::size_t colon = pair.find(':');
            existing->trace.emplace_back(std::stof(pair.substr(0, colon)), std::stof(pair.substr(colon + 1)));
        }
        return existing;
    }
};

}  // namespace

namespace Workshop {

void CnbLoaderLeave()
{
    const bool removed = CnbLoaderRegistry::Remove(Cnb::CnbAssetTypeIdFromName(kFossilType));
    Museum::Log("workshop", "left wk-cnbloader: the Museum.Workshop.Fossil .cnb loader %s", removed ? "was removed" : "was not registered");
}

void CnbLoaderChecks(Checks& checks, Services& services)
{
    const std::uint32_t id = Cnb::CnbAssetTypeIdFromName(kFossilType);
    const std::uint32_t expectedId = Fnv1a32(kFossilType) | 0x80000000u;
    checks.Add("CnbAssetTypeIdFromName(\"Museum.Workshop.Fossil\")", "FNV-1a-32 of the name | 0x80000000 = " + std::to_string(expectedId), std::to_string(id), id == expectedId && Cnb::IsCustomAssetTypeId(id));

    const Fossil sample = Sample();
    std::vector<std::uint8_t> bytes;
    checks.Group("CnbWriter", [&] {
        bytes = WriteFossil(sample, id, kFossilType);
        const bool magic = bytes.size() >= 64 && bytes[0] == 0x43 && bytes[1] == 0x4E && bytes[2] == 0x42 && bytes[3] == 0x1A;
        checks.Add("CnbWriter.Build: container magic", "43 4E 42 1A ('CNB' + 0x1A), at least the 64-byte header", magic ? "present, " + std::to_string(bytes.size()) + " bytes" : "missing",
                   magic && Cnb::CnbDocument::HasMagic(bytes));
        const Cnb::CnbDocument doc = Cnb::CnbDocument::Parse(bytes, "fossil.cnb");
        checks.Add("CnbDocument.Parse: identity", "asset type " + std::to_string(id) + ", schema 1, CMET name Museum.Workshop.Fossil, content name fossil",
                   std::to_string(doc.AssetTypeId()) + ", schema " + std::to_string(doc.AssetSchemaVersion()) + ", " + doc.Metadata().assetTypeName + ", " + doc.Metadata().contentName,
                   doc.AssetTypeId() == id && doc.AssetSchemaVersion() == 1u && doc.Metadata().present && doc.Metadata().assetTypeName == kFossilType && doc.Metadata().contentName == "fossil");
        const auto chunk = doc.FindSingle(kFossilChunk);
        checks.Add("the FOSL chunk", "one mandatory chunk of 4+27+4+4+8+8+4+36 = 95 bytes", chunk ? Cnb::ChunkIdToString(doc.ChunkAt(*chunk).type) + ", " + std::to_string(doc.ChunkData(*chunk).size()) + " bytes, mandatory " + Yes(doc.ChunkAt(*chunk).IsMandatory()) : "absent",
                   chunk.has_value() && doc.ChunkData(*chunk).size() == 95u && doc.ChunkAt(*chunk).IsMandatory());
        WriteFile(OutputFolder() / "fossil.cnb", bytes);
    });
    if (bytes.empty()) return;

    (void)CnbLoaderRegistry::Remove(id);
    checks.Group("CnbLoaderRegistry.Register", [&] {
        CnbLoaderRegistry::Register(id, kFossilType, &LoadFossil);
        checks.Add("IsRegistered / RegisteredTypeName", "true, Museum.Workshop.Fossil", Yes(CnbLoaderRegistry::IsRegistered(id)) + ", " + CnbLoaderRegistry::RegisteredTypeName(id),
                   CnbLoaderRegistry::IsRegistered(id) && CnbLoaderRegistry::RegisteredTypeName(id) == kFossilType);
        CnbLoaderRegistry::Register(id, kFossilType, &LoadFossil);
        checks.Add("registering the same type again", "accepted, no effect (documented)", "accepted", CnbLoaderRegistry::IsRegistered(id));
    });
    checks.Group("ContentManager.Load<Fossil>", [&] {
        const Fossil got = LoadThroughManager(services, "fossil");
        checks.Add("ContentManager.Load<Fossil>(\"fossil\") from fossil.cnb", "every field as written: name, era -150, 0.505f, 1.505e8, 0x0123456789ABCDEF, 3 points bit for bit",
                   "\"" + got.name + "\", " + std::to_string(got.era) + ", " + F(got.length, "%.9g") + ", " + F(got.ageYears) + ", " + std::to_string(got.outline.size()) + " points", Same(got, sample));
    });

    // The refusals CnbLoaderRegistry documents.
    checks.ExpectThrow<std::invalid_argument>("Register under a built-in identifier (Texture2D)", "std::invalid_argument: not a custom identifier",
                                              [&] { CnbLoaderRegistry::Register(Cnb::CnbAssetTypeId::Texture2D, "Microsoft.Xna.Framework.Graphics.Texture2D", &LoadFossil); });
    checks.ExpectThrow<std::invalid_argument>("Register a name that does not hash to the identifier", "std::invalid_argument",
                                              [&] { CnbLoaderRegistry::Register(Cnb::CnbAssetTypeIdFromName("Museum.Workshop.Pebble"), "Museum.Workshop.Rock", &LoadFossil); });
    checks.ExpectThrow<std::invalid_argument>("Register an empty loader", "std::invalid_argument",
                                              [&] { CnbLoaderRegistry::Register(Cnb::CnbAssetTypeIdFromName("Museum.Workshop.Pebble"), "Museum.Workshop.Pebble", {}); });
    checks.Group("files the loader must refuse", [&] {
        checks.ExpectThrow<ContentLoadException>("CnbWriter.Build with a CMET name that does not hash to the identifier", "ContentLoadException (documented: the writer never produces a file its reader refuses)",
                                                 [&] { (void)WriteFossil(sample, id, "Museum.Workshop.Impostor"); });
        // So the impostor is made by hand: a well-formed Impostor file whose header then claims the Fossil identifier, with the header
        // checksum (CRC-32C of bytes 0-43, docs/cnb-format.md section 3) recomputed so that only the name check can catch it.
        std::vector<std::uint8_t> impostor = WriteFossil(sample, Cnb::CnbAssetTypeIdFromName("Museum.Workshop.Impostor"), "Museum.Workshop.Impostor");
        for (int i = 0; i < 4; ++i) impostor[12 + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(id >> (8 * i));
        const std::uint32_t crc = Cnb::Crc32c(std::span<const std::uint8_t>(impostor.data(), 44));
        for (int i = 0; i < 4; ++i) impostor[44 + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(crc >> (8 * i));
        WriteFile(OutputFolder() / "impostor.cnb", impostor);
        checks.ExpectThrow<ContentLoadException>("a file with the Fossil identifier but another CMET type name", "ContentLoadException (ResolveForDocument: the name must equal the registered one)",
                                                 [&] { (void)LoadThroughManager(services, "impostor"); });
        std::vector<std::uint8_t> damaged = bytes;
        damaged[damaged.size() - 7] ^= 0x5Au;   // inside the FOSL payload: its CRC-32C no longer matches
        WriteFile(OutputFolder() / "damaged.cnb", damaged);
        checks.ExpectThrow<ContentLoadException>("a file with one payload byte changed", "ContentLoadException (the chunk checksum disagrees)", [&] { (void)LoadThroughManager(services, "damaged"); });
    });
    checks.Group("CnbLoaderRegistry.Remove", [&] {
        const bool first = CnbLoaderRegistry::Remove(id), second = CnbLoaderRegistry::Remove(id);
        checks.Add("Remove twice", "true, then false", Yes(first) + ", " + Yes(second), first && !second);
        checks.ExpectThrow<ContentLoadException>("Load after Remove", "ContentLoadException: no loader for the file's type", [&] { (void)LoadThroughManager(services, "fossil"); });
        // Registered again for the rest of the visit; the room removes it when the visitor leaves (OnLeave).
        CnbLoaderRegistry::Register(id, kFossilType, &LoadFossil);
        checks.Add("registered again for the visit", "IsRegistered true; removed again on leaving the room", Yes(CnbLoaderRegistry::IsRegistered(id)), CnbLoaderRegistry::IsRegistered(id));
    });

    checks.Group("ContentTypeSerializer", [&] {
        // The registry is process-wide and has no removal (XNA finds serializers by reflection and never forgets one either), so the
        // serializer is registered once per process.
        static const bool registered = [] {
            (void)Intermediate::IntermediateSerializer::AddTypeSerializer<SeismogramSerializer>();
            return true;
        }();
        (void)registered;
        Intermediate::ContentTypeSerializerBase* found = Intermediate::IntermediateSerializer::FindTypeSerializer(std::string(SeismogramContent::XnaTypeName));
        checks.Add("AddTypeSerializer, then FindTypeSerializer(\"Museum.Workshop.Seismogram\")", "the room's SeismogramSerializer",
                   dynamic_cast<SeismogramSerializer*>(found) != nullptr ? "SeismogramSerializer" : (found != nullptr ? "another serializer" : "null"), dynamic_cast<SeismogramSerializer*>(found) != nullptr);
        checks.Info("removing a ContentTypeSerializer", "-", "the public API has AddTypeSerializer but no removal; XNA has neither (serializers are found by reflection)");

        auto value = std::make_shared<SeismogramContent>();
        value->station = "Lyon";
        value->trace = {{0.0f, 1.5f}, {0.25f, -2.0f}, {0.5f, 0.125f}, {1.0f, 1.0f / 3.0f}};
        System::Xml::XmlWriterSettings settings;
        settings.Indent = true;
        settings.NewLineChars = "\n";
        std::unique_ptr<System::Xml::XmlWriter> writer(System::Xml::XmlWriter::CreateToString(settings));
        const int serializedBefore = serializeCalls, deserializedBefore = deserializeCalls;
        Intermediate::IntermediateSerializer::Serialize<SeismogramContent>(*writer, value, std::string());
        const std::string xml = writer->ToString();
        const std::string expectedTrace = "<Trace>0:1.5 0.25:-2 0.5:0.125 1:0.333333343</Trace>";
        // XNA's IntermediateWriter declares each namespace once on the root (xmlns:Workshop="Museum.Workshop", the prefix being the
        // namespace's last segment) and spells the type through it.
        const bool shape = xml.find("<XnaContent xmlns:Workshop=\"Museum.Workshop\"") != std::string::npos && xml.find("<Asset Type=\"Workshop:Seismogram\">") != std::string::npos &&
                           xml.find("<Station>Lyon</Station>") != std::string::npos && xml.find(expectedTrace) != std::string::npos && xml.find("<Station>") < xml.find("<Trace>");
        checks.Add("IntermediateSerializer.Serialize through the custom serializer", "<XnaContent xmlns:Workshop=\"Museum.Workshop\"><Asset Type=\"Workshop:Seismogram\"><Station>Lyon</Station>" + expectedTrace + ", Serialize called once",
                   (shape ? "that document" : Checks::Short(xml, 200)) + ", " + std::to_string(serializeCalls - serializedBefore) + " call(s)", shape && serializeCalls - serializedBefore == 1);
        std::unique_ptr<System::Xml::XmlReader> reader(System::Xml::XmlReader::CreateFromString(xml));
        const std::shared_ptr<SeismogramContent> back = Intermediate::IntermediateSerializer::Deserialize<SeismogramContent>(*reader, std::string());
        checks.Add("IntermediateSerializer.Deserialize back", "station Lyon, the four (time, amplitude) pairs bit for bit, Deserialize called once",
                   back ? back->station + ", " + std::to_string(back->trace.size()) + " pairs, " + std::to_string(deserializeCalls - deserializedBefore) + " call(s)" : "null",
                   back != nullptr && back->station == value->station && back->trace == value->trace && deserializeCalls - deserializedBefore == 1);
        std::string missing = xml;
        const std::size_t at = missing.find(expectedTrace);
        missing.erase(at, expectedTrace.size());
        checks.ExpectThrow<Xna::InvalidContentException>("a document without the Trace element", "InvalidContentException (XNA IntermediateReader: ElementNotFound)", [&] {
            std::unique_ptr<System::Xml::XmlReader> r(System::Xml::XmlReader::CreateFromString(missing));
            (void)Intermediate::IntermediateSerializer::Deserialize<SeismogramContent>(*r, std::string());
        });
        checks.ExpectThrow<Xna::InvalidContentException>("a document that is not XnaContent", "InvalidContentException (XNA IntermediateSerializer: NotIntermediateXml)", [&] {
            std::unique_ptr<System::Xml::XmlReader> r(System::Xml::XmlReader::CreateFromString("<Seismogram><Station>Lyon</Station></Seismogram>"));
            (void)Intermediate::IntermediateSerializer::Deserialize<SeismogramContent>(*r, std::string());
        });
    });
}

}  // namespace Workshop

#endif  // MUSEUM_PIPELINE

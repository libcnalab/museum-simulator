// Pipeline Workshop, mode "typewriter" (room wk-typewriter): a game's own content type written by its own ContentTypeWriter<T>,
// compiled by ContentCompiler into .xnb bytes in memory, and read back by matching ContentTypeReader<T> classes registered with
// ContentTypeReaderManager, through a ContentManager whose OpenStream serves those bytes. Every field is compared exactly; one
// label is written as a shared resource referenced twice, one is a derived type reached through its base writer, one is null.
// Then the loader is fed bytes the XNA contract says it must refuse with ContentLoadException (ContentReader.PrepareStream and
// ReadAsset in XNA 4.0: bad magic, a size field larger than the stream, a body cut short, an unknown reader, a type version that
// disagrees).
// Last, a game type that has no ContentTypeWriter at all: XNA writes it with its implicit ReflectiveWriter and reads it with
// ReflectiveReader<T> (Content/Workshop/reflective.xnb, written byte for byte by tools/gen_workshop.py). CNA has no reflection, so the
// game declares the field list once with ReflectiveTypeReaderBuilder and the file is loaded through an ordinary ContentManager.
#ifdef MUSEUM_PIPELINE

#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "Exhibits/Content/PipelineSupport.hpp"
#include "Exhibits/Content/WorkshopChecks.hpp"
#include "Exhibits/Services.hpp"
#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Content/ContentLoadException.hpp"
#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/Content/ContentReader.hpp"
#include "Microsoft/Xna/Framework/Content/ContentTypeReader.hpp"
#include "Microsoft/Xna/Framework/Content/ContentTypeReaderManager.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/ContentItem.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Serialization/Compiler/ContentCompiler.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Serialization/Compiler/ContentTypeWriter.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Serialization/Compiler/ContentTypeWriterAttribute.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Serialization/Compiler/ContentWriter.hpp"
#include "Microsoft/Xna/Framework/Content/ReflectiveTypeReader.hpp"
#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Quaternion.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"
#include "Microsoft/Xna/Framework/Vector4.hpp"
#include "System/TimeSpan.hpp"

using namespace Microsoft::Xna::Framework;

namespace {

namespace Xna = Microsoft::Xna::Framework::Content::Pipeline;
namespace Compiler = Microsoft::Xna::Framework::Content::Pipeline::Serialization::Compiler;
namespace Content = Microsoft::Xna::Framework::Content;

// ---- build time: the content types and their writers ------------------------------------------------------------------------------

class LabelContent : public Xna::ContentItem {
public:
    static constexpr std::string_view XnaTypeName = "Museum.Workshop.LabelContent";
    std::string text;
    Color colour{0, 0, 0, 255};
    [[nodiscard]] const std::string& GetTypeName() const override
    {
        static const std::string name{XnaTypeName};
        return name;
    }
};

class FramedLabelContent final : public LabelContent {
public:
    static constexpr std::string_view XnaTypeName = "Museum.Workshop.FramedLabelContent";
    std::int32_t frameWidth = 0;
    [[nodiscard]] const std::string& GetTypeName() const override
    {
        static const std::string name{XnaTypeName};
        return name;
    }
};

class SpecimenContent final : public Xna::ContentItem {
public:
    static constexpr std::string_view XnaTypeName = "Museum.Workshop.SpecimenContent";
    std::string title;
    std::int32_t catalogueNumber = 0;
    float mass = 0.0f;
    double age = 0.0;
    bool fragile = false;
    std::int64_t accession = 0;
    std::uint64_t barcode = 0;
    std::uint8_t shelf = 0;
    std::int32_t drawer = 0;   // written with Write7BitEncodedInt
    Vector2 footprint;
    Vector3 position;
    Vector4 glow;
    Color tint;
    Quaternion orientation;
    Matrix placement;
    std::vector<std::string> tags;
    std::shared_ptr<LabelContent> primary;   // a FramedLabelContent, written through the LabelContent writer family
    std::shared_ptr<LabelContent> missing;   // null
    std::shared_ptr<LabelContent> sharedA;   // one object referenced twice: one shared resource
    std::shared_ptr<LabelContent> sharedB;
    std::shared_ptr<LabelContent> other;     // a second shared resource
    [[nodiscard]] const std::string& GetTypeName() const override
    {
        static const std::string name{XnaTypeName};
        return name;
    }
};

class LabelWriter final : public Compiler::ContentTypeWriter<LabelContent> {
public:
    [[nodiscard]] std::string GetRuntimeReader(Xna::TargetPlatform) const override { return "Museum.Workshop.LabelReader, Museum"; }

protected:
    void Write(Compiler::ContentWriter& output, const std::shared_ptr<LabelContent>& value) override
    {
        output.Write(value->text);
        output.Write(value->colour);
    }
};

class FramedLabelWriter final : public Compiler::ContentTypeWriter<FramedLabelContent> {
public:
    [[nodiscard]] std::string GetRuntimeReader(Xna::TargetPlatform) const override { return "Museum.Workshop.FramedLabelReader, Museum"; }

protected:
    void Write(Compiler::ContentWriter& output, const std::shared_ptr<FramedLabelContent>& value) override
    {
        output.Write(value->text);
        output.Write(value->colour);
        output.Write(value->frameWidth);
    }
};

class SpecimenWriter final : public Compiler::ContentTypeWriter<SpecimenContent> {
public:
    [[nodiscard]] std::string GetRuntimeReader(Xna::TargetPlatform) const override { return "Museum.Workshop.SpecimenReader, Museum"; }
    [[nodiscard]] std::int32_t getTypeVersionProperty() const override { return 2; }

protected:
    void Write(Compiler::ContentWriter& output, const std::shared_ptr<SpecimenContent>& v) override
    {
        output.Write(v->title);
        output.Write(v->catalogueNumber);
        output.Write(v->mass);
        output.Write(v->age);
        output.Write(v->fragile);
        output.Write(static_cast<SharpRuntime::longcs>(v->accession));
        output.Write(v->barcode);
        output.Write(static_cast<SharpRuntime::bytecs>(v->shelf));
        output.Write7BitEncodedInt(v->drawer);
        output.Write(v->footprint);
        output.Write(v->position);
        output.Write(v->glow);
        output.Write(v->tint);
        output.Write(v->orientation);
        output.Write(v->placement);
        output.Write(static_cast<std::int32_t>(v->tags.size()));
        for (const std::string& t : v->tags) output.Write(t);
        output.WriteObject<LabelContent>(v->primary);
        output.WriteObject<LabelContent>(v->missing);
        output.WriteSharedResource<LabelContent>(v->sharedA);
        output.WriteSharedResource<LabelContent>(v->sharedB);
        output.WriteSharedResource<LabelContent>(v->other);
    }
};

// A second, tiny type for the two Int32 object forms: WriteObject<Int32> (type id, then the value) and WriteRawObject<Int32> (the value
// alone, the reader found by type). Kept apart from the specimen so that a problem with either cannot hide the rest of the round trip.
class TallyContent final : public Xna::ContentItem {
public:
    static constexpr std::string_view XnaTypeName = "Museum.Workshop.TallyContent";
    std::int32_t boxed = 0, raw = 0;
    [[nodiscard]] const std::string& GetTypeName() const override
    {
        static const std::string name{XnaTypeName};
        return name;
    }
};

class TallyWriter final : public Compiler::ContentTypeWriter<TallyContent> {
public:
    [[nodiscard]] std::string GetRuntimeReader(Xna::TargetPlatform) const override { return "Museum.Workshop.TallyReader, Museum"; }

protected:
    void Write(Compiler::ContentWriter& output, const std::shared_ptr<TallyContent>& value) override
    {
        output.WriteObject<std::int32_t>(value->boxed);
        output.WriteRawObject<std::int32_t>(value->raw);
    }
};

// ---- run time: the game's types and their readers ---------------------------------------------------------------------------------

struct Label {
    virtual ~Label() = default;
    std::string text;
    Color colour;
};

struct FramedLabel final : Label {
    std::int32_t frameWidth = 0;
};

struct Specimen {
    std::string title;
    std::int32_t catalogueNumber = 0;
    float mass = 0.0f;
    double age = 0.0;
    bool fragile = false;
    std::int64_t accession = 0;
    std::uint64_t barcode = 0;
    std::uint8_t shelf = 0;
    std::int32_t drawer = 0;
    Vector2 footprint;
    Vector3 position;
    Vector4 glow;
    Color tint;
    Quaternion orientation;
    Matrix placement;
    std::vector<std::string> tags;
    std::shared_ptr<Label> primary, missing, sharedA, sharedB, other;
    std::string assetName;
};

struct Tally {
    std::int32_t boxed = 0, raw = 0;
};

class TallyReader final : public Content::ContentTypeReader<std::shared_ptr<Tally>> {
public:
    TallyReader() : ContentTypeReader("Museum.Workshop.Tally") {}

protected:
    std::shared_ptr<Tally> Read(Content::ContentReader& input, std::optional<std::shared_ptr<Tally>>) override
    {
        auto t = std::make_shared<Tally>();
        t->boxed = input.ReadObject<std::int32_t>();
        t->raw = input.ReadRawObject<std::int32_t>();
        return t;
    }
};

class LabelReader final : public Content::ContentTypeReader<std::shared_ptr<Label>> {
public:
    LabelReader() : ContentTypeReader("Museum.Workshop.Label") {}

protected:
    std::shared_ptr<Label> Read(Content::ContentReader& input, std::optional<std::shared_ptr<Label>>) override
    {
        auto l = std::make_shared<Label>();
        l->text = input.ReadString();
        l->colour = input.ReadColor();
        return l;
    }
};

class FramedLabelReader final : public Content::ContentTypeReader<std::shared_ptr<Label>> {
public:
    FramedLabelReader() : ContentTypeReader("Museum.Workshop.FramedLabel") {}

protected:
    std::shared_ptr<Label> Read(Content::ContentReader& input, std::optional<std::shared_ptr<Label>>) override
    {
        auto l = std::make_shared<FramedLabel>();
        l->text = input.ReadString();
        l->colour = input.ReadColor();
        l->frameWidth = input.ReadInt32();
        return l;
    }
};

template <int Version>
class SpecimenReaderT final : public Content::ContentTypeReader<std::shared_ptr<Specimen>> {
public:
    SpecimenReaderT() : ContentTypeReader("Museum.Workshop.Specimen") {}
    [[nodiscard]] int getTypeVersionProperty() const override { return Version; }

protected:
    std::shared_ptr<Specimen> Read(Content::ContentReader& input, std::optional<std::shared_ptr<Specimen>>) override
    {
        auto s = std::make_shared<Specimen>();
        s->title = input.ReadString();
        s->catalogueNumber = input.ReadInt32();
        s->mass = input.ReadSingle();
        s->age = input.ReadDouble();
        s->fragile = input.ReadBoolean();
        s->accession = input.ReadInt64();
        s->barcode = input.ReadUInt64();
        s->shelf = static_cast<std::uint8_t>(input.ReadByte());
        s->drawer = input.Read7BitEncodedInt();
        s->footprint = input.ReadVector2();
        s->position = input.ReadVector3();
        s->glow = input.ReadVector4();
        s->tint = input.ReadColor();
        s->orientation = input.ReadQuaternion();
        s->placement = input.ReadMatrix();
        const std::int32_t count = input.ReadInt32();
        for (std::int32_t i = 0; i < count && i < 64; ++i) s->tags.push_back(input.ReadString());
        s->primary = input.ReadObject<std::shared_ptr<Label>>();
        s->missing = input.ReadObject<std::shared_ptr<Label>>();
        // XNA calls a shared resource's fixup after the root object, once the shared section has been read.
        input.ReadSharedResource<std::shared_ptr<Label>>([s](const std::shared_ptr<Label>& l) { s->sharedA = l; });
        input.ReadSharedResource<std::shared_ptr<Label>>([s](const std::shared_ptr<Label>& l) { s->sharedB = l; });
        input.ReadSharedResource<std::shared_ptr<Label>>([s](const std::shared_ptr<Label>& l) { s->other = l; });
        s->assetName = input.getAssetNameProperty();
        return s;
    }
};

constexpr const char* kSpecimenReader = "Museum.Workshop.SpecimenReader";
constexpr const char* kLabelReader = "Museum.Workshop.LabelReader";
constexpr const char* kFramedReader = "Museum.Workshop.FramedLabelReader";
constexpr const char* kTallyReader = "Museum.Workshop.TallyReader";

void RegisterReaders(int specimenVersion)
{
    using Manager = Content::ContentTypeReaderManager;
    for (const char* name : {kSpecimenReader, kLabelReader, kFramedReader, kTallyReader}) (void)Manager::RemoveTypeCreatorEXT(name);
    if (specimenVersion == 2)
        Manager::AddTypeCreator(kSpecimenReader, [] { return std::make_unique<SpecimenReaderT<2>>(); });
    else
        Manager::AddTypeCreator(kSpecimenReader, [] { return std::make_unique<SpecimenReaderT<1>>(); });
    Manager::AddTypeCreator(kLabelReader, [] { return std::make_unique<LabelReader>(); });
    Manager::AddTypeCreator(kFramedReader, [] { return std::make_unique<FramedLabelReader>(); });
    Manager::AddTypeCreator(kTallyReader, [] { return std::make_unique<TallyReader>(); });
}

void RemoveReaders()
{
    for (const char* name : {kSpecimenReader, kLabelReader, kFramedReader, kTallyReader}) (void)Content::ContentTypeReaderManager::RemoveTypeCreatorEXT(name);
}

std::shared_ptr<SpecimenContent> MakeSpecimen()
{
    auto s = std::make_shared<SpecimenContent>();
    s->title = "Ammonite, Jurassic \xC3\xA9tage";   // UTF-8 survives the 7-bit length prefix
    s->catalogueNumber = -20481;
    s->mass = 0.1f;
    s->age = 1.85e8;
    s->fragile = true;
    s->accession = -9007199254740993LL;
    s->barcode = 18446744073709551557ULL;
    s->shelf = 200;
    s->drawer = 300;   // two bytes in 7-bit form
    s->footprint = Vector2(12.5f, -0.25f);
    s->position = Vector3(1.0f / 3.0f, -2.0f, 1e-7f);
    s->glow = Vector4(0.0f, 0.5f, 1.0f, 3.4028235e38f);
    s->tint = Color(10, 20, 30, 40);
    s->orientation = Quaternion::CreateFromYawPitchRoll(0.3f, -0.2f, 0.1f);
    s->placement = Matrix::CreateRotationY(0.7f) * Matrix::CreateTranslation(4.0f, 5.0f, -6.0f);
    s->tags = {"spiral", "", "fossil"};
    auto framed = std::make_shared<FramedLabelContent>();
    framed->text = "Hall 3";
    framed->colour = Color(1, 2, 3, 4);
    framed->frameWidth = 7;
    s->primary = framed;
    auto shared = std::make_shared<LabelContent>();
    shared->text = "Donated";
    shared->colour = Color(250, 240, 230, 255);
    s->sharedA = shared;
    s->sharedB = shared;
    auto other = std::make_shared<LabelContent>();
    other->text = "On loan";
    other->colour = Color(5, 6, 7, 8);
    s->other = other;
    return s;
}

bool SameLabel(const std::shared_ptr<Label>& got, const std::shared_ptr<LabelContent>& want)
{
    return got != nullptr && want != nullptr && got->text == want->text && got->colour == want->colour;
}

std::uint32_t Le32(const std::vector<std::uint8_t>& b, std::size_t at)
{
    return at + 4 <= b.size() ? static_cast<std::uint32_t>(b[at]) | static_cast<std::uint32_t>(b[at + 1]) << 8 | static_cast<std::uint32_t>(b[at + 2]) << 16 | static_cast<std::uint32_t>(b[at + 3]) << 24 : 0u;
}

std::shared_ptr<Specimen> Load(Services& s, const std::vector<std::uint8_t>& bytes, const std::string& name)
{
    PipelineSupport::MemoryContentManager manager(&s.game->getServicesProperty());
    manager.bytes = bytes;
    return manager.Load<std::shared_ptr<Specimen>>(name);
}

// ---- a game type without a writer of its own, read reflectively -----------------------------------------------------------------

// The game's C# types (see tools/gen_workshop.py): Wing, SettingsBase { int Revision }, Curator { string Name; int Years },
// ExhibitSettings : SettingsBase { Title, Visitors, Scale, Position, Open, Wing, Opening, Tint, Curator, Deputy }.
enum class Wing : std::int32_t { North, South, East, West };
struct SettingsBase {
    std::int32_t revision = 0;
};
struct Curator {
    std::string name;
    std::int32_t years = 0;
};
struct ExhibitSettings : SettingsBase {
    std::string title;
    std::int32_t visitors = 0;
    float scale = 0.0f;
    Vector3 position;
    bool open = false;
    std::string openedBy;   // filled by the Custom step, not a member of the C# type
    Wing wing = Wing::North;
    System::TimeSpan opening;
    Color tint{0, 0, 0, 0};
    std::shared_ptr<Curator> curator, deputy;
};

constexpr const char* kExhibitType = "Museum.Workshop.ExhibitSettings";
constexpr const char* kBaseType = "Museum.Workshop.SettingsBase";
constexpr const char* kCuratorType = "Museum.Workshop.Curator";
constexpr const char* kWingType = "Museum.Workshop.Wing";

std::vector<std::string> ReflectiveReaderNames()
{
    return {Content::CanonicalReflectiveReaderNameEXT(kExhibitType), Content::CanonicalReflectiveReaderNameEXT(kBaseType), Content::CanonicalReflectiveReaderNameEXT(kCuratorType),
            Content::EnumTypeReader<Wing>::CanonicalReaderName(kWingType)};
}

void RegisterReflective()
{
    Content::ReflectiveTypeReaderBuilder<SettingsBase> base(kBaseType);
    base.Field(&SettingsBase::revision);
    base.Register();
    Content::ReflectiveTypeReaderBuilder<Curator>(kCuratorType).Field(&Curator::name).Field(&Curator::years).RegisterShared();
    Content::ReflectiveTypeReaderBuilder<ExhibitSettings>(kExhibitType)
        .Base(base)
        .Field(&ExhibitSettings::title)
        .Field(&ExhibitSettings::visitors)
        .Field(&ExhibitSettings::scale)
        .Field(&ExhibitSettings::position)
        .Custom([](ExhibitSettings& s, Content::ContentReader& input) {
            s.open = input.ReadBoolean();
            s.openedBy = "the Custom step";
        })
        .EnumField(&ExhibitSettings::wing, kWingType)
        .Field(&ExhibitSettings::opening)
        .Field(&ExhibitSettings::tint)
        .Field(&ExhibitSettings::curator)
        .Field(&ExhibitSettings::deputy)
        .Register();
}

void RemoveReflective()
{
    for (const std::string& readerName : ReflectiveReaderNames()) (void)Content::ContentTypeReaderManager::RemoveTypeCreatorEXT(readerName);
}

void ReflectiveChecks(Workshop::Checks& checks, Services& services)
{
    using Workshop::Checks;
    using Workshop::F;
    checks.Add("CanonicalReflectiveReaderNameEXT(\"Museum.Workshop.ExhibitSettings\")", "Microsoft.Xna.Framework.Content.ReflectiveReader`1[[Museum.Workshop.ExhibitSettings]], the same as ReflectiveTypeReader<T>::CanonicalReaderName",
               Content::CanonicalReflectiveReaderNameEXT(kExhibitType),
               Content::CanonicalReflectiveReaderNameEXT(kExhibitType) == "Microsoft.Xna.Framework.Content.ReflectiveReader`1[[Museum.Workshop.ExhibitSettings]]" &&
                   Content::ReflectiveTypeReader<ExhibitSettings>::CanonicalReaderName(kExhibitType) == Content::CanonicalReflectiveReaderNameEXT(kExhibitType));
    RemoveReflective();
    RegisterReflective();
    std::string registered;
    for (const std::string& name : ReflectiveReaderNames()) registered += Content::ContentTypeReaderManager::IsRegistered(name) ? "+" : "-";
    checks.Add("ReflectiveTypeReaderBuilder: Register, RegisterShared, and the EnumField's reader", "the four readers the file's table names are registered (++++)", registered, registered == "++++");
    checks.Group("ContentManager.Load<ExhibitSettings>(\"Workshop/reflective\")", [&] {
        Content::ContentManager manager(&services.game->getServicesProperty(), services.contentRoot);
        const ExhibitSettings s = manager.Load<ExhibitSettings>("Workshop/reflective");
        checks.Add("reflective.xnb: the base type's member first, then the fields", "Revision 7; \"Hall of Pipelines\", 1234, 0.75, (1.5, -2, 3.25)",
                   "Revision " + std::to_string(s.revision) + "; \"" + s.title + "\", " + std::to_string(s.visitors) + ", " + F(s.scale) + ", (" + F(s.position.X) + ", " + F(s.position.Y) + ", " + F(s.position.Z) + ")",
                   s.revision == 7 && s.title == "Hall of Pipelines" && s.visitors == 1234 && s.scale == 0.75f && s.position == Vector3(1.5f, -2.0f, 3.25f));
        checks.Add("Boolean through Custom, enum, TimeSpan, Color", "Open true (read by the Custom step), Wing East (2), Opening 9:30:00, Tint (10,20,30,255)",
                   std::string("Open ") + (s.open ? "true" : "false") + " (" + s.openedBy + "), Wing " + std::to_string(static_cast<int>(s.wing)) + ", Opening " + F(s.opening.getTotalHoursProperty()) + " h, Tint (" +
                       std::to_string(s.tint.getRProperty()) + "," + std::to_string(s.tint.getGProperty()) + "," + std::to_string(s.tint.getBProperty()) + "," + std::to_string(s.tint.getAProperty()) + ")",
                   s.open && s.openedBy == "the Custom step" && s.wing == Wing::East && s.opening.getTicksProperty() == 342000000000LL && s.tint == Color(10, 20, 30, 255));
        checks.Add("a nested reference-type member and a null one", "Curator {\"Ada\", 12} through ReflectiveSharedTypeReader; Deputy null (index 0)",
                   s.curator ? "Curator {\"" + s.curator->name + "\", " + std::to_string(s.curator->years) + "}" + (s.deputy ? ", Deputy set" : ", Deputy null") : "Curator null",
                   s.curator && s.curator->name == "Ada" && s.curator->years == 12 && s.deputy == nullptr);
    });
    checks.ExpectThrow<std::logic_error>("Register() of a declaration with a SharedResourceField", "std::logic_error (the deferred fixup needs RegisterShared's stable object)",
                                         [] { Content::ReflectiveTypeReaderBuilder<ExhibitSettings>("Museum.Workshop.SharedSettings").SharedResourceField(&ExhibitSettings::deputy).Register(); });
    RemoveReflective();
    checks.ExpectThrow<Content::ContentLoadException>("reflective.xnb with its readers removed", "ContentLoadException (XNA: the type table must resolve in full, TypeReaderNotFound)", [&] {
        Content::ContentManager manager(&services.game->getServicesProperty(), services.contentRoot);
        (void)manager.Load<ExhibitSettings>("Workshop/reflective");
    });
}

}  // namespace

namespace Workshop {

void TypeWriterChecks(Checks& checks, Services& services)
{
    Compiler::ContentCompiler compiler;
    compiler.AddTypeWriter<LabelWriter>(Compiler::ContentTypeWriterAttribute{});
    compiler.AddTypeWriter<FramedLabelWriter>();
    compiler.AddTypeWriter<SpecimenWriter>();
    compiler.AddTypeWriter<TallyWriter>();
    const std::shared_ptr<SpecimenContent> source = MakeSpecimen();

    checks.Group("ContentCompiler.GetTypeWriter", [&] {
        const auto writer = compiler.GetTypeWriter(System::Type::From<SpecimenContent>());
        const bool found = writer != nullptr;
        checks.Add("ContentCompiler.GetTypeWriter(typeof(SpecimenContent))", "the registered SpecimenWriter: reader \"Museum.Workshop.SpecimenReader, Museum\", TypeVersion 2",
                   found ? "reader \"" + writer->GetRuntimeReader(Xna::TargetPlatform::Windows) + "\", TypeVersion " + std::to_string(writer->getTypeVersionProperty()) : "null",
                   found && writer->GetRuntimeReader(Xna::TargetPlatform::Windows) == "Museum.Workshop.SpecimenReader, Museum" && writer->getTypeVersionProperty() == 2 &&
                       writer->getTargetTypeProperty() == System::Type::From<SpecimenContent>());
    });

    std::vector<std::uint8_t> bytes;
    checks.Group("ContentCompiler.Compile", [&] {
        Compiler::CompileOptions options;
        options.assetName = "specimen";
        // The result type is CNA::Internal::Xnb::XnbAssetWriteResult: only its public fields (bytes, rootReaderName) are read.
        const auto result = compiler.Compile<SpecimenContent>(source, options);
        bytes = result.bytes;
        const bool magic = bytes.size() > 10 && bytes[0] == 'X' && bytes[1] == 'N' && bytes[2] == 'B';
        checks.Add("compiled header", "'XNB', platform 'w', version 5, flags 0x00 (Reach, uncompressed), size field = file length",
                   magic ? std::string("'XNB' '") + static_cast<char>(bytes[3]) + "' v" + std::to_string(bytes[4]) + " flags " + std::to_string(bytes[5]) + ", size field " + std::to_string(Le32(bytes, 6)) + " of " + std::to_string(bytes.size()) : "no XNB magic",
                   magic && bytes[3] == 'w' && bytes[4] == 5 && bytes[5] == 0 && Le32(bytes, 6) == bytes.size());
        checks.Add("root reader of the compiled asset", "Museum.Workshop.SpecimenReader (from SpecimenWriter.GetRuntimeReader)", result.rootReaderName,
                   result.rootReaderName.rfind("Museum.Workshop.SpecimenReader", 0) == 0);
        const std::string text(bytes.begin(), bytes.end());
        const bool listsAll = text.find("Museum.Workshop.SpecimenReader") != std::string::npos && text.find("Museum.Workshop.LabelReader") != std::string::npos && text.find("Museum.Workshop.FramedLabelReader") != std::string::npos;
        checks.Add("type manifest names every reader the object needs", "Specimen, Label and FramedLabel readers in the file", listsAll ? "all three present" : "missing", listsAll);
    });
    if (bytes.empty()) return;

    RegisterReaders(2);
    checks.Group("load back", [&] {
        const std::shared_ptr<Specimen> got = Load(services, bytes, "specimen");
        checks.Add("ContentManager.Load<Specimen> through the registered readers", "an object", got != nullptr ? "loaded" : "null", got != nullptr);
        if (got == nullptr) return;
        const SpecimenContent& w = *source;
        checks.Add("String, Int32, Single, Double, Boolean", "\"" + w.title + "\", -20481, 0.1f, 1.85e8, true",
                   "\"" + got->title + "\", " + std::to_string(got->catalogueNumber) + ", " + F(got->mass, "%.9g") + ", " + F(got->age) + ", " + Yes(got->fragile),
                   got->title == w.title && got->catalogueNumber == w.catalogueNumber && got->mass == w.mass && got->age == w.age && got->fragile == w.fragile);
        checks.Add("Int64, UInt64, Byte, 7-bit encoded Int32", "-9007199254740993, 18446744073709551557, 200, 300",
                   std::to_string(got->accession) + ", " + std::to_string(got->barcode) + ", " + std::to_string(got->shelf) + ", " + std::to_string(got->drawer),
                   got->accession == w.accession && got->barcode == w.barcode && got->shelf == w.shelf && got->drawer == w.drawer);
        checks.Add("Vector2, Vector3, Vector4 bit for bit", "(12.5, -0.25), (1/3, -2, 1e-7), (0, 0.5, 1, float max)",
                   "(" + F(got->footprint.X) + ", " + F(got->footprint.Y) + "), (" + F(got->position.X, "%.9g") + ", " + F(got->position.Y) + ", " + F(got->position.Z) + "), w " + F(got->glow.W),
                   got->footprint == w.footprint && got->position == w.position && got->glow == w.glow);
        checks.Add("Color, Quaternion, Matrix bit for bit", "Color(10,20,30,40), the yaw-pitch-roll quaternion, rotation*translation",
                   "colour " + std::to_string(got->tint.getRProperty()) + "," + std::to_string(got->tint.getGProperty()) + "," + std::to_string(got->tint.getBProperty()) + "," + std::to_string(got->tint.getAProperty()) + ", M41 " + F(got->placement.M41),
                   got->tint == w.tint && got->orientation == w.orientation && got->placement == w.placement);
        checks.Add("a list of strings, the empty one included", "spiral | (empty) | fossil",
                   std::to_string(got->tags.size()) + " tags" + (got->tags.size() == 3 ? ": " + got->tags[0] + " | " + got->tags[1] + " | " + got->tags[2] : ""), got->tags == w.tags);
        const auto framed = std::dynamic_pointer_cast<FramedLabel>(got->primary);
        checks.Add("WriteObject<LabelContent> of a FramedLabelContent", "read by the FramedLabel reader: \"Hall 3\", frame 7",
                   framed != nullptr ? "FramedLabel \"" + framed->text + "\", frame " + std::to_string(framed->frameWidth) : (got->primary != nullptr ? "a plain Label" : "null"),
                   framed != nullptr && SameLabel(got->primary, w.primary) && framed->frameWidth == 7);
        checks.Add("WriteObject of a null reference", "ReadObject returns null", got->missing == nullptr ? "null" : "an object", got->missing == nullptr);
        checks.Add("one shared resource referenced twice", "both references are the same object after the fixups",
                   got->sharedA != nullptr && got->sharedA == got->sharedB ? "same object" : (got->sharedA == nullptr ? "fixup not called" : "two objects"),
                   got->sharedA != nullptr && got->sharedA == got->sharedB && SameLabel(got->sharedA, w.sharedA));
        checks.Add("a second shared resource", "a different object, \"On loan\"", got->other != nullptr ? "\"" + got->other->text + "\"" + (got->other == got->sharedA ? " (same as the first!)" : "") : "null",
                   got->other != nullptr && got->other != got->sharedA && SameLabel(got->other, w.other));
        checks.Add("ContentReader.AssetName inside the reader", "specimen", got->assetName, got->assetName == "specimen");
    });

    checks.Group("compressed", [&] {
        Compiler::CompileOptions options;
        options.assetName = "specimen";
        options.compressContent = true;
        const auto result = compiler.Compile<SpecimenContent>(source, options);
        const std::vector<std::uint8_t>& packed = result.bytes;
        const bool lzx = packed.size() > 14 && (packed[5] & 0x80) != 0;
        checks.Add("CompileOptions.compressContent", "flags bit 0x80 (LZX, XNA 4.0's only compression), size field = file length",
                   packed.size() > 10 ? "flags " + std::to_string(packed[5]) + ", size field " + std::to_string(Le32(packed, 6)) + " of " + std::to_string(packed.size()) : "too short",
                   lzx && Le32(packed, 6) == packed.size());
        const std::shared_ptr<Specimen> got = Load(services, packed, "specimen");
        checks.Add("the compressed file loads to the same object", "title, matrix and shared label equal",
                   got != nullptr ? "\"" + got->title + "\"" : "null",
                   got != nullptr && got->title == source->title && got->placement == source->placement && got->sharedA != nullptr && got->sharedA == got->sharedB);
    });

    checks.Group("Int32 objects", [&] {
        auto tally = std::make_shared<TallyContent>();
        tally->boxed = -42;
        tally->raw = 123456789;
        Compiler::CompileOptions options;
        options.assetName = "tally";
        const auto result = compiler.Compile<TallyContent>(tally, options);
        const std::string text(result.bytes.begin(), result.bytes.end());
        // XNA's ContentWriter.WriteRawObject<T> puts T's writer into the type table (GetTypeWriter(typeof(T), out index)), so the file's
        // manifest names Int32Reader and the runtime ContentTypeReaderManager can find it by type.
        checks.Add("the manifest lists Int32Reader", "Microsoft.Xna.Framework.Content.Int32Reader in the file", text.find("Int32Reader") != std::string::npos ? "listed" : "not listed",
                   text.find("Int32Reader") != std::string::npos);
        PipelineSupport::MemoryContentManager manager(&services.game->getServicesProperty());
        manager.bytes = result.bytes;
        try {
            const std::shared_ptr<Tally> got = manager.Load<std::shared_ptr<Tally>>("tally");
            checks.Add("WriteObject<Int32> then WriteRawObject<Int32>, read back", "-42, 123456789", got ? std::to_string(got->boxed) + ", " + std::to_string(got->raw) : "null",
                       got != nullptr && got->boxed == -42 && got->raw == 123456789);
        } catch (const std::exception& e) {
            checks.Add("WriteObject<Int32> then WriteRawObject<Int32>, read back", "-42, 123456789 (XNA: ReadRawObject<T> finds T's reader through ContentTypeReaderManager.GetTypeReader(typeof(T)))",
                       Checks::Short(std::string("thrown: ") + e.what()), false);
        }
    });

    // The refusals. XNA 4.0 (ContentReader.PrepareStream / ReadAsset) turns each of these into ContentLoadException.
    using Content::ContentLoadException;
    checks.ExpectThrow<ContentLoadException>("bad magic ('YNB')", "ContentLoadException (XNA: BadXnbMagic)", [&] {
        std::vector<std::uint8_t> broken = bytes;
        broken[0] = 'Y';
        (void)Load(services, broken, "specimen");
    });
    checks.ExpectThrow<ContentLoadException>("file cut in half, header kept", "ContentLoadException (XNA: size field larger than the stream, BadXnbSize)", [&] {
        std::vector<std::uint8_t> broken(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(bytes.size() / 2));
        (void)Load(services, broken, "specimen");
    });
    checks.ExpectThrow<ContentLoadException>("body cut short, size field patched to match", "ContentLoadException (XNA: the reader's EndOfStream is an IOException, BadXnb)", [&] {
        std::vector<std::uint8_t> broken(bytes.begin(), bytes.end() - 40);
        const std::uint32_t size = static_cast<std::uint32_t>(broken.size());
        for (int i = 0; i < 4; ++i) broken[6 + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(size >> (8 * i));
        (void)Load(services, broken, "specimen");
    });
    {   // Version 4 is XNA 3.x's container. XNA 4.0 refuses it (BadXnbVersion); CNA reads it on purpose, with the legacy
        // SurfaceFormat numbering (docs/xnb-interoperability.md, "Version").
        std::string got = "loaded";
        try {
            std::vector<std::uint8_t> legacy = bytes;
            legacy[4] = 4;
            (void)Load(services, legacy, "specimen");
        } catch (const std::exception& e) {
            got = Checks::Short(std::string("thrown: ") + e.what(), 140);
        }
        checks.Add("XNB version byte 4 (XNA 3.x's container)", "loaded (CNA's documented legacy read; XNA 4.0: BadXnbVersion)", got, got == "loaded");
    }
    RegisterReaders(1);
    checks.ExpectThrow<ContentLoadException>("reader TypeVersion 1, file written with 2", "ContentLoadException (XNA: ReadTypeManifest, BadXnbTypeVersion)", [&] { (void)Load(services, bytes, "specimen"); });
    RemoveReaders();
    checks.ExpectThrow<ContentLoadException>("no reader registered for the file's reader name", "ContentLoadException (XNA: TypeReaderNotFound)", [&] { (void)Load(services, bytes, "specimen"); });
    checks.Add("ContentTypeReaderManager after RemoveTypeCreatorEXT", "the three readers are no longer registered",
               Content::ContentTypeReaderManager::IsRegistered(kSpecimenReader) ? "still registered" : "removed", !Content::ContentTypeReaderManager::IsRegistered(kSpecimenReader) && !Content::ContentTypeReaderManager::IsRegistered(kLabelReader));

    checks.Group("ReflectiveTypeReader", [&] { ReflectiveChecks(checks, services); });
}

}  // namespace Workshop

#endif  // MUSEUM_PIPELINE

// The Pipeline Workshop: content-pipeline operations whose results can be checked exactly, run in the game on
// data the room builds itself. Each check logs one line
//     [museum][check] <room> <name>: PASS|FAIL|INFO (expected ..., got ...)
// like the renderer-validation rooms, so a run's log is the witness of the result. One class, one mode per room; the modes
// other than meshhelper live in their own files (Workshop*Checks.cpp) behind WorkshopChecks.hpp.
//
//   meshhelper   MeshHelper on a cube built with MeshBuilder: merging duplicate positions and vertices, normals,
//                tangent frames, cache optimisation, winding order, TransformScene, and skeleton search/flattening.
//   processor    a game's own importer and processor run through CNA's real processor context (XnaBridgeProcessorContext over a
//                coordinator): AddDependency, AddOutputFile, Convert to a stock processor, BuildAsset and BuildAndLoadAsset.
//   typewriter   a game type with its own ContentTypeWriter and ContentTypeReader: compiled to .xnb bytes, loaded back through a
//                ContentManager, compared field by field; shared resources, nested objects, and corrupted streams refused.
//   material     MaterialProcessor on a BasicMaterialContent naming a texture: the texture is built by TextureProcessor.
//   importers    the importers and probes no other room shows: WmaImporter, PassThroughProcessor, build-time media decoding.
//   cnbloader    a game-defined .cnb asset type: CnbWriter/CnbByteWriter, CnbLoaderRegistry::Register, ContentManager.Load, and a
//                custom ContentTypeSerializer for the IntermediateSerializer.
//   gltf         CNA's glTF import on small files written for the room: UV sets, texture transforms, specular/ior, sparse
//                accessors, morph targets, and a refused required extension.
//   xml          System::Xml::Serialization::XmlSerializer round trips of XNA values through CNA's adapters.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <exception>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "Diagnostics/Log.hpp"
#include "Exhibits/Content/WorkshopChecks.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#ifdef MUSEUM_PIPELINE
#include "Exhibits/Content/PipelineSupport.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Graphics/MeshBuilder.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Graphics/MeshHelper.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Graphics/NodeContent.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Graphics/VertexChannel.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Graphics/VertexChannelNames.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Graphics/VertexContent.hpp"
#endif

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

enum class Mode { MeshHelper, Processor, TypeWriter, Material, Importers, CnbLoader, Gltf, Xml };

Mode ParseMode(const std::string& name)
{
    static const std::pair<const char*, Mode> modes[] = {{"meshhelper", Mode::MeshHelper}, {"processor", Mode::Processor}, {"typewriter", Mode::TypeWriter}, {"material", Mode::Material},
                                                         {"importers", Mode::Importers},   {"cnbloader", Mode::CnbLoader}, {"gltf", Mode::Gltf},             {"xml", Mode::Xml}};
    for (const auto& [key, mode] : modes)
        if (name == key) return mode;
    return Mode::MeshHelper;
}

struct ModeText {
    const char* title;
    const char* sign;
    std::vector<std::string> lines;
};

const ModeText& TextFor(Mode mode)
{
    static const ModeText texts[] = {
        {"MeshHelper, checked", "MeshHelper",
         {"XNA's MeshHelper works on imported meshes before ModelProcessor: it merges duplicate positions and vertices, computes normals and tangent",
          "frames, reorders triangles for the vertex cache, flips winding, moves a whole scene and finds a skeleton. Each result here is checked exactly."}},
        {"A processor context, checked", "ContentProcessorContext",
         {"A processor talks to the build through its context: it records dependencies and extra output files, converts objects with other processors",
          "and builds other assets. Here a game's own importer and processor run through CNA's real context over a coordinator, and every call is checked."}},
        {"ContentTypeWriter and ContentTypeReader, checked", "A game's own content type",
         {"A game type reaches an .xnb through its own ContentTypeWriter and comes back through a matching ContentTypeReader. The room compiles one,",
          "loads it through a ContentManager, compares every field, shares one object between two references, and feeds the loader broken bytes."}},
        {"MaterialProcessor, checked", "MaterialProcessor",
         {"MaterialProcessor builds the textures a material names (through TextureProcessor, with its own colour-key and premultiply settings) and",
          "can turn a BasicMaterialContent into another stock material type. The room runs it on a material it builds and checks the result."}},
        {"Importers and codecs, checked", "Importers not shown elsewhere",
         {"WmaImporter, PassThroughProcessor and CNA's build-time media decoder, run on tiny files generated for the room. A codec that this",
          "machine does not have is reported as unavailable: it is never counted as a pass."}},
        {"A custom .cnb asset type, checked", "CnbLoaderRegistry",
         {"CNA's own compiled container, .cnb, has one extension route for a game's types: write the file with CnbWriter, register a loader under",
          "the type's identifier, and ContentManager.Load does the rest. The room also registers a ContentTypeSerializer for the XML side."}},
        {"glTF import, checked", "CNA's glTF importer",
         {"Small glTF files written for this room (JSON with base64 buffers) are loaded through ContentManager: two UV sets, texture transforms,",
          "specular and IOR, sparse accessors and morph targets are compared with the values written, and a file requiring an unknown extension is refused."}},
        {"XmlSerializer, checked", "XmlSerializer and XNA values",
         {"A game saves settings with System.Xml.Serialization.XmlSerializer. CNA states the member lists of XNA's value types once (XmlSerializationEXT),",
          "so a Vector3, a Matrix or a Keys value round-trips exactly. The room serializes a settings record and compares every value it reads back."}},
    };
    return texts[static_cast<int>(mode)];
}

#ifdef MUSEUM_PIPELINE
namespace Xna = Microsoft::Xna::Framework::Content::Pipeline;
namespace PipeGfx = Microsoft::Xna::Framework::Content::Pipeline::Graphics;

using Triangle = std::array<int, 3>;

// A triangle as the same three position indices whichever corner it starts at (winding kept).
Triangle Canonical(Triangle t)
{
    const auto lowest = std::min_element(t.begin(), t.end()) - t.begin();
    std::rotate(t.begin(), t.begin() + lowest, t.end());
    return t;
}

std::multiset<Triangle> Triangles(const PipeGfx::GeometryContent& g, bool reverse = false)
{
    std::multiset<Triangle> out;
    const auto& indices = g.getIndicesProperty();
    const auto& positionIndices = g.getVerticesProperty().getPositionIndicesProperty();
    for (int i = 0; i + 2 < indices.getCountProperty(); i += 3) {
        Triangle t{static_cast<int>(positionIndices.At(static_cast<int>(indices[i]))), static_cast<int>(positionIndices.At(static_cast<int>(indices[i + 1]))),
                   static_cast<int>(positionIndices.At(static_cast<int>(indices[i + 2])))};
        if (reverse) std::swap(t[1], t[2]);
        out.insert(Canonical(t));
    }
    return out;
}

std::string Vec(const Vector3& v)
{
    char buf[64];
    std::snprintf(buf, sizeof buf, "(%.3f, %.3f, %.3f)", static_cast<double>(v.X), static_cast<double>(v.Y), static_cast<double>(v.Z));
    return buf;
}
#endif

class PipelineChecksLab final : public ScreenExhibit {
public:
    PipelineChecksLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services), mode_(ParseMode(room.params.Get("mode"))) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel("Pipeline checks", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        p.AddButton("Run the checks again", [this] { Run(); });
        p.AddReadout("Passed / failed / info", [this] { return Summary(); });
        const ModeText& text = TextFor(mode_);
        AddInfoSign(b, text.sign, text.lines);
    }

    void OnScreenLoaded() override { Run(); }

    void OnLeave() override
    {
#ifdef MUSEUM_PIPELINE
        // The .cnb loader registry is process-wide: the room's game type must not outlive the visit.
        if (mode_ == Mode::CnbLoader) Workshop::CnbLoaderLeave();
#endif
    }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        const auto text = [&](const std::string& t, float x, float y, Color c, float s = 0.6f) { sb.DrawString(*S().font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), s, SpriteEffects::None, 0.0f); };
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        text(TextFor(mode_).title, 24, 12, Kit::kGold, 1.0f);
        // Up to 13 rows at full size; longer lists shrink their rows, and past 26 rows they continue in a second column.
        const std::vector<Workshop::Check>& list = checks_.List();
        const std::size_t count = list.size();
        const bool twoColumns = count > 26;
        const std::size_t perColumn = twoColumns ? (count + 1) / 2 : count;
        const float row = perColumn == 0 ? 34.0f : std::min(34.0f, 460.0f / static_cast<float>(perColumn));
        const float scale = std::min(1.0f, row / 34.0f);
        for (std::size_t i = 0; i < count; ++i) {
            const Workshop::Check& c = list[i];
            const float x = twoColumns && i >= perColumn ? 488.0f : 24.0f;
            const float y = 64.0f + row * static_cast<float>(twoColumns && i >= perColumn ? i - perColumn : i);
            const Color colour = c.informational ? Kit::kDim : (c.pass ? Kit::Rgba(120, 230, 140) : Kit::Rgba(255, 120, 110));
            const std::size_t nameChars = twoColumns ? 34 : 44;
            text(std::string(c.informational ? "info " : (c.pass ? "PASS " : "FAIL ")) + c.name.substr(0, nameChars), x, y, colour, 0.58f * scale);
            if (!twoColumns) text(c.got.substr(0, 70), 520, y, Kit::kText, 0.5f * scale);
        }
        sb.End();
    }

private:
    [[nodiscard]] std::string Summary() const
    {
        int pass = 0, fail = 0, info = 0;
        for (const Workshop::Check& c : checks_.List()) (c.informational ? info : (c.pass ? pass : fail))++;
        return std::to_string(pass) + " / " + std::to_string(fail) + " / " + std::to_string(info);
    }

    void Add(const std::string& name, const std::string& expected, const std::string& got, bool pass) { checks_.Add(name, expected, got, pass); }

    void Run()
    {
        checks_ = Workshop::Checks(R().key);
        try {
            switch (mode_) {
                case Mode::Xml: Workshop::XmlSerializerChecks(checks_, S()); return;
#ifdef MUSEUM_PIPELINE
                case Mode::MeshHelper: MeshHelperChecks(); return;
                case Mode::Processor: Workshop::ProcessorContextChecks(checks_, S()); return;
                case Mode::TypeWriter: Workshop::TypeWriterChecks(checks_, S()); return;
                case Mode::Material: Workshop::MaterialProcessorChecks(checks_, S()); return;
                case Mode::Importers: Workshop::ImporterChecks(checks_, S()); return;
                case Mode::CnbLoader: Workshop::CnbLoaderChecks(checks_, S()); return;
                case Mode::Gltf: Workshop::GltfImportChecks(checks_, S()); return;
#else
                default: checks_.Info("content pipeline", "MUSEUM_PIPELINE=ON", "not compiled in"); return;
#endif
            }
        } catch (const std::exception& e) {
            Add("the check run itself", "no exception", Workshop::Checks::Short(e.what()), false);
        }
    }

#ifdef MUSEUM_PIPELINE
    // A unit cube built face by face: every corner is its own position and vertex, as an exporter that does not share
    // corners writes it. 6 faces x 2 triangles x 3 corners = 36 positions and vertices; 8 distinct positions.
    static std::shared_ptr<PipeGfx::MeshContent> Cube()
    {
        const std::shared_ptr<PipeGfx::MeshBuilder> builder = PipeGfx::MeshBuilder::StartMesh("Cube");
        builder->setMergeDuplicatePositionsProperty(false);
        const int uvs = builder->CreateVertexChannel<Vector2>(PipeGfx::VertexChannelNames::TextureCoordinate(0));
        const Vector3 n[6] = {Vector3::UnitX, -Vector3::UnitX, Vector3::UnitY, -Vector3::UnitY, Vector3::UnitZ, -Vector3::UnitZ};
        for (const Vector3& normal : n) {
            // Two axes spanning the face, chosen so that (u x v) points along the outward normal.
            const Vector3 u = std::abs(normal.Y) > 0.5f ? Vector3::UnitX : Vector3::Cross(Vector3::UnitY, normal);
            const Vector3 v = Vector3::Cross(normal, u);
            const Vector3 c[4] = {normal - u - v, normal + u - v, normal + u + v, normal - u + v};
            const Vector2 t[4] = {Vector2(0, 1), Vector2(1, 1), Vector2(1, 0), Vector2(0, 0)};
            const int order[6] = {0, 2, 1, 0, 3, 2};   // clockwise seen from outside, XNA's front face
            for (const int k : order) {
                builder->SetVertexChannelData(uvs, Xna::Box<Vector2>(t[k]));
                builder->AddTriangleVertex(builder->CreatePosition(c[k] * 0.5f));
            }
        }
        return builder->FinishMesh();
    }

    void MeshHelperChecks()
    {
        const std::shared_ptr<PipeGfx::MeshContent> mesh = Cube();
        const std::shared_ptr<PipeGfx::GeometryContent> geometry = mesh->getGeometryProperty()[0];
        PipeGfx::GeometryContent& g = *geometry;
        const int before = mesh->getPositionsProperty().getCountProperty();
        const std::multiset<Triangle> original = Triangles(g);

        PipeGfx::MeshHelper::MergeDuplicatePositions(mesh, 0.001f);
        const int merged = mesh->getPositionsProperty().getCountProperty();
        Add("MergeDuplicatePositions(mesh, 0.001)", "36 -> 8 positions", std::to_string(before) + " -> " + std::to_string(merged), before == 36 && merged == 8);

        const int verticesBefore = g.getVerticesProperty().getVertexCountProperty();
        PipeGfx::MeshHelper::MergeDuplicateVertices(mesh);
        const int verticesAfter = g.getVerticesProperty().getVertexCountProperty();
        Add("MergeDuplicateVertices(mesh)", "36 -> 24 vertices (4 per face), 36 indices", std::to_string(verticesBefore) + " -> " + std::to_string(verticesAfter) + ", " + std::to_string(g.getIndicesProperty().getCountProperty()) + " indices",
            verticesBefore == 36 && verticesAfter == 24 && g.getIndicesProperty().getCountProperty() == 36);
        const std::multiset<Triangle> mergedTriangles = Triangles(g);
        Add("the triangles survive both merges", "the same 12 triangles", std::to_string(mergedTriangles.size()) + " triangles", mergedTriangles.size() == 12);

        // XNA's contract (MeshHelper.CalculateNormals): every triangle adds its normalised face normal, cross(c - b, b - a),
        // to each of its corners' positions, and the sums are normalised. A cube face split into two triangles therefore
        // counts twice at the corners its diagonal touches. The expected normals are computed here the same way, from
        // the merged geometry, before CNA runs.
        const auto& positions = mesh->getPositionsProperty();
        const auto& positionIndices = g.getVerticesProperty().getPositionIndicesProperty();
        const auto& triangleIndices = g.getIndicesProperty();
        std::vector<Vector3> expectedNormals(static_cast<std::size_t>(positions.getCountProperty()), Vector3::Zero);
        for (int i = 0; i + 2 < triangleIndices.getCountProperty(); i += 3) {
            const int pa = positionIndices.At(static_cast<int>(triangleIndices[i])), pb = positionIndices.At(static_cast<int>(triangleIndices[i + 1])),
                      pc = positionIndices.At(static_cast<int>(triangleIndices[i + 2]));
            const Vector3 a = positions[pa], b = positions[pb], c = positions[pc];
            const Vector3 face = Vector3::Normalize(Vector3::Cross(c - b, b - a));
            for (const int k : {pa, pb, pc}) expectedNormals[static_cast<std::size_t>(k)] += face;
        }
        for (Vector3& n : expectedNormals) n = Vector3::Normalize(n);
        PipeGfx::MeshHelper::CalculateNormals(mesh, true);
        const auto& channels = g.getVerticesProperty().getChannelsProperty();
        const std::string normalName = PipeGfx::VertexChannelNames::Normal(0);
        const bool hasNormals = channels.Contains(normalName);
        float worst = 0.0f;
        std::string corner = "-";
        if (hasNormals) {
            const auto normals = channels.Get<Vector3>(normalName);
            for (int i = 0; i < normals->getCountProperty(); ++i) {
                const int position = positionIndices.At(i);
                worst = std::max(worst, Vector3::Distance(normals->At(i), expectedNormals[static_cast<std::size_t>(position)]));
                const Vector3 p = positions[position];
                if (p.X > 0 && p.Y > 0 && p.Z > 0) corner = Vec(normals->At(i));
            }
        }
        Add("CalculateNormals(mesh, true) on shared positions", "XNA's per-triangle sum at each position, error < 0.0001", hasNormals ? "+++ corner " + corner + ", worst error " + std::to_string(worst) : "no normal channel",
            hasNormals && worst < 0.0001f);

        PipeGfx::MeshHelper::CalculateTangentFrames(mesh, PipeGfx::VertexChannelNames::TextureCoordinate(0), PipeGfx::VertexChannelNames::Tangent(0), PipeGfx::VertexChannelNames::Binormal(0));
        const bool hasTangents = channels.Contains(PipeGfx::VertexChannelNames::Tangent(0)) && channels.Contains(PipeGfx::VertexChannelNames::Binormal(0));
        float worstDot = 0.0f;
        if (hasTangents && hasNormals) {
            const auto normals = channels.Get<Vector3>(normalName);
            const auto tangents = channels.Get<Vector3>(PipeGfx::VertexChannelNames::Tangent(0));
            for (int i = 0; i < tangents->getCountProperty(); ++i) worstDot = std::max(worstDot, std::abs(Vector3::Dot(tangents->At(i), normals->At(i))));
        }
        Add("CalculateTangentFrames: tangent and binormal channels", "both written, one per vertex", hasTangents ? "both present" : "missing", hasTangents);
        Add("tangents are perpendicular to the normals", "|tangent . normal| < 0.001", "worst " + std::to_string(worstDot), hasTangents && worstDot < 0.001f);

        const std::multiset<Triangle> beforeCache = Triangles(g);
        PipeGfx::MeshHelper::OptimizeForCache(mesh);
        Add("OptimizeForCache keeps every triangle", "the same 12 triangles, same winding", Triangles(g) == beforeCache ? "identical set" : "changed", Triangles(g) == beforeCache && g.getIndicesProperty().getCountProperty() == 36);

        const std::multiset<Triangle> reversed = Triangles(g, true);
        PipeGfx::MeshHelper::SwapWindingOrder(mesh);
        Add("SwapWindingOrder reverses every triangle", "each triangle's corners in the opposite order", Triangles(g) == reversed ? "all reversed" : "not all reversed", Triangles(g) == reversed);
        (void)original;

        // TransformScene moves the whole scene: wherever the transform lands (node transforms or positions), a vertex's
        // place in the world must move by exactly the translation.
        const auto scene = std::make_shared<PipeGfx::NodeContent>();
        scene->setNameProperty("Scene");
        scene->getChildrenProperty().Add(mesh);
        const Vector3 cornerBefore = Vector3::Transform(mesh->getPositionsProperty()[0], mesh->getAbsoluteTransformProperty());
        PipeGfx::MeshHelper::TransformScene(scene, Matrix::CreateTranslation(0.0f, 2.0f, 0.0f));
        const Vector3 cornerAfter = Vector3::Transform(mesh->getPositionsProperty()[0], mesh->getAbsoluteTransformProperty());
        const Vector3 moved = cornerAfter - cornerBefore;
        Add("TransformScene(scene, translation 0,2,0)", "every vertex 2 m higher in the world", Vec(moved), Vector3::Distance(moved, Vector3(0, 2, 0)) < 1e-5f);

        // A skeleton next to the mesh: Hips -> Spine -> Head, Hips -> Leg.
        auto bone = [](const char* name) {
            auto b = std::make_shared<PipeGfx::BoneContent>();
            b->setNameProperty(name);
            return b;
        };
        const auto hips = bone("Hips"), spine = bone("Spine"), head = bone("Head"), leg = bone("Leg");
        spine->getChildrenProperty().Add(head);
        hips->getChildrenProperty().Add(spine);
        hips->getChildrenProperty().Add(leg);
        scene->getChildrenProperty().Add(hips);
        const std::shared_ptr<PipeGfx::BoneContent> root = PipeGfx::MeshHelper::FindSkeleton(scene);
        Add("FindSkeleton(scene)", "Hips", root != nullptr ? root->getNameProperty() : "null", root != nullptr && root->getNameProperty() == "Hips");
        std::string order;
        if (root != nullptr)
            for (const auto& b : PipeGfx::MeshHelper::FlattenSkeleton(root)) order += (order.empty() ? "" : " ") + b->getNameProperty();
        Add("FlattenSkeleton(Hips)", "Hips Spine Head Leg (depth first)", order.empty() ? "-" : order, order == "Hips Spine Head Leg");
    }
#endif

    Mode mode_;
    Workshop::Checks checks_{""};
};

}  // namespace

std::unique_ptr<Exhibit> MakePipelineChecksLab(const RoomDef& room, Services& services)
{
    return std::make_unique<PipelineChecksLab>(room, services);
}

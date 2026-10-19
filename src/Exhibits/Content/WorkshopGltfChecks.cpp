// Pipeline Workshop, mode "gltf" (room wk-gltf): CNA's glTF import, reached the way a game reaches it (ContentManager.Load<Model> of a
// .gltf), on files tools/gen_workshop.py writes into Content/Workshop with every checked number in the script itself:
//   uvsets      TEXCOORD_0 and TEXCOORD_1 must reach the vertex buffer unchanged (two TextureCoordinate elements, usage index 0 and 1);
//               KHR_texture_transform on the base-colour map and on the normal map (whose transform overrides texCoord to 1), an
//               occlusion map on texCoord 1, KHR_materials_ior and KHR_materials_specular must reach the PbrEffect unchanged.
//   sparse      a POSITION accessor with sparse overrides on a bufferView, and a TEXCOORD_0 accessor that is sparse with no bufferView
//               (glTF 2.0 section 3.6.2.3: zeros, then the overrides).
//   morph       two morph targets (one dense, one sparse with no bufferView) and the mesh's default weights.
//   required    an extension nobody implements listed in extensionsRequired: refused, by name (glTF 2.0 section 3.12).
//   used        the same extension in extensionsUsed only: the file loads and the import report names what it ignored.
//   instancing  EXT_mesh_gpu_instancing used (not required): the file must load; what CNA does with the instances is reported only.
// Vertices are matched to the authored ones by their (unique) positions, so a re-indexing importer is not penalised.
#ifdef MUSEUM_PIPELINE

#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Exhibits/Content/WorkshopChecks.hpp"
#include "Exhibits/Services.hpp"
#include "Microsoft/Xna/Framework/Content/ContentLoadException.hpp"
#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/Graphics/GltfImportReportEXT.hpp"
#include "Microsoft/Xna/Framework/Graphics/Model.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelMesh.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelMeshPart.hpp"
#include "Microsoft/Xna/Framework/Graphics/MorphTargetEXT.hpp"
#include "Microsoft/Xna/Framework/Graphics/PbrEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/TextureTransformEXT.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexDeclaration.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexElement.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

using Workshop::Checks;
using Workshop::F;
using Workshop::Yes;

// The values tools/gen_workshop.py writes.
const std::array<Vector3, 4> kQuad = {Vector3(-1, -1, 0), Vector3(1, -1, 0), Vector3(1, 1, 0), Vector3(-1, 1, 0)};
const std::array<Vector2, 4> kUv0 = {Vector2(0, 1), Vector2(1, 1), Vector2(1, 0), Vector2(0, 0)};
const std::array<Vector2, 4> kUv1 = {Vector2(0.125f, 0.875f), Vector2(0.625f, 0.875f), Vector2(0.625f, 0.375f), Vector2(0.125f, 0.375f)};
const std::array<Vector3, 4> kSparsePositions = {Vector3(-1, -1, 0), Vector3(2.0f, -1.5f, 0.25f), Vector3(1, 1, 0), Vector3(-1.25f, 2.0f, -0.5f)};
const std::array<Vector2, 4> kSparseUv = {Vector2(0, 0), Vector2(0, 0), Vector2(0.5f, 0.75f), Vector2(0, 0)};
const std::array<Vector3, 4> kMorphDense = {Vector3(0, 0, 0.5f), Vector3(0, 0, 0.25f), Vector3(0, 0, -0.25f), Vector3(0, 0, -0.5f)};
const std::array<Vector3, 4> kMorphSparse = {Vector3(0, 0, 0), Vector3(0, 0, 0), Vector3(0, 1, 1), Vector3(0, 0, 0)};
constexpr const char* kImaginary = "EXT_museum_imaginary_feature";

struct Vertex {
    Vector3 position;
    std::optional<Vector2> uv0, uv1;
};

struct Layout {
    int stride = 0, position = -1, uv0 = -1, uv1 = -1;
    std::string elements;
};

Layout LayoutOf(const VertexDeclaration& d)
{
    Layout l;
    l.stride = d.getVertexStrideProperty();
    for (const VertexElement& e : d.GetVertexElements()) {
        const auto usage = e.getVertexElementUsageProperty();
        if (usage == VertexElementUsage::Position && e.getUsageIndexProperty() == 0) l.position = e.getOffsetProperty();
        if (usage == VertexElementUsage::TextureCoordinate && e.getUsageIndexProperty() == 0) l.uv0 = e.getOffsetProperty();
        if (usage == VertexElementUsage::TextureCoordinate && e.getUsageIndexProperty() == 1) l.uv1 = e.getOffsetProperty();
        l.elements += (l.elements.empty() ? "" : " ") + e.ToString();
    }
    return l;
}

std::vector<Vertex> Decode(const std::vector<std::uint8_t>& bytes, const Layout& l, int first, int count)
{
    std::vector<Vertex> out;
    for (int v = 0; v < count; ++v) {
        const std::size_t base = static_cast<std::size_t>(first + v) * static_cast<std::size_t>(l.stride);
        if (l.position < 0 || base + static_cast<std::size_t>(l.stride) > bytes.size()) break;
        Vertex x;
        std::memcpy(&x.position, bytes.data() + base + static_cast<std::size_t>(l.position), 12);
        if (l.uv0 >= 0) {
            Vector2 t;
            std::memcpy(&t, bytes.data() + base + static_cast<std::size_t>(l.uv0), 8);
            x.uv0 = t;
        }
        if (l.uv1 >= 0) {
            Vector2 t;
            std::memcpy(&t, bytes.data() + base + static_cast<std::size_t>(l.uv1), 8);
            x.uv1 = t;
        }
        out.push_back(x);
    }
    return out;
}

// The vertices of a part, read back through VertexBuffer.GetData (XNA's generic GetData<T>, here T = float over the whole stride).
std::vector<Vertex> PartVertices(ModelMeshPart& part, Layout& layout)
{
    VertexBuffer& vb = *part.getVertexBufferProperty();
    layout = LayoutOf(vb.getVertexDeclarationProperty());
    std::vector<float> floats(static_cast<std::size_t>(vb.getVertexCountProperty()) * static_cast<std::size_t>(layout.stride) / 4u);
    vb.GetData(floats.data(), static_cast<int>(floats.size()));
    std::vector<std::uint8_t> bytes(floats.size() * 4u);
    std::memcpy(bytes.data(), floats.data(), bytes.size());
    return Decode(bytes, layout, part.getVertexOffsetProperty(), part.getNumVerticesProperty());
}

int Authored(const std::array<Vector3, 4>& positions, const Vector3& p)
{
    for (int i = 0; i < 4; ++i)
        if (positions[static_cast<std::size_t>(i)] == p) return i;
    return -1;
}

std::string V2(const Vector2& v) { return "(" + F(v.X) + ", " + F(v.Y) + ")"; }

ModelMeshPart* FirstPart(Model& model)
{
    for (ModelMesh* mesh : model.getMeshesProperty())
        if (mesh->getMeshPartsProperty().getCountProperty() > 0) return mesh->getMeshPartsProperty()[0];
    return nullptr;
}

bool Mentions(const GltfImportDiagnosticEXT& d, const std::string& name)
{
    if (d.Code.find(name) != std::string::npos || d.Subject.find(name) != std::string::npos || d.Message.find(name) != std::string::npos) return true;
    for (const std::string& detail : d.Details)
        if (detail.find(name) != std::string::npos) return true;
    return false;
}

const char* KindName(GltfImportDiagnosticKindEXT k)
{
    switch (k) {
        case GltfImportDiagnosticKindEXT::Information: return "Information";
        case GltfImportDiagnosticKindEXT::GeneratedData: return "GeneratedData";
        case GltfImportDiagnosticKindEXT::InvalidSourceData: return "InvalidSourceData";
        case GltfImportDiagnosticKindEXT::Approximation: return "Approximation";
        case GltfImportDiagnosticKindEXT::DroppedData: return "DroppedData";
        case GltfImportDiagnosticKindEXT::UnsupportedFeature: return "UnsupportedFeature";
    }
    return "?";
}

}  // namespace

namespace Workshop {

void GltfImportChecks(Checks& checks, Services& services)
{
    Microsoft::Xna::Framework::Content::ContentManager manager(&services.game->getServicesProperty(), services.contentRoot);

    checks.Group("uvsets", [&] {
        Model model = manager.Load<Model>("Workshop/uvsets");
        ModelMeshPart* part = FirstPart(model);
        checks.Add("Load<Model>(\"Workshop/uvsets\")", "one mesh part", part != nullptr ? "loaded" : "no part", part != nullptr);
        if (part == nullptr) return;
        Layout layout;
        const std::vector<Vertex> vertices = PartVertices(*part, layout);
        checks.Info("uvsets vertex declaration", "-", "stride " + std::to_string(layout.stride) + ": " + Checks::Short(layout.elements, 160));
        int matched = 0, uv0ok = 0, uv1ok = 0;
        std::string first;
        for (const Vertex& v : vertices) {
            const int i = Authored(kQuad, v.position);
            if (i < 0) continue;
            ++matched;
            if (v.uv0 && *v.uv0 == kUv0[static_cast<std::size_t>(i)]) ++uv0ok;
            if (v.uv1 && *v.uv1 == kUv1[static_cast<std::size_t>(i)]) ++uv1ok;
            if (i == 1) first = "vertex (1,-1,0): uv0 " + (v.uv0 ? V2(*v.uv0) : std::string("none")) + ", uv1 " + (v.uv1 ? V2(*v.uv1) : std::string("none"));
        }
        checks.Add("TEXCOORD_0 in the vertex buffer", "TextureCoordinate usage 0 at every vertex, values as written (4 of 4)", std::to_string(uv0ok) + " of " + std::to_string(matched) + " matched vertices", matched == 4 && uv0ok == 4);
        checks.Add("TEXCOORD_1 in the vertex buffer", "TextureCoordinate usage 1, values as written: (0.625, 0.875) at (1,-1,0)", first.empty() ? std::to_string(uv1ok) + " of " + std::to_string(matched) : first,
                   matched == 4 && uv1ok == 4);
        PbrEffect* pbr = dynamic_cast<PbrEffect*>(part->getEffectProperty());
        checks.Add("the material's effect", "PbrEffect (metallic-roughness material)", pbr != nullptr ? "PbrEffect" : "another effect", pbr != nullptr);
        if (pbr == nullptr) return;
        const std::array<int, 5>& sets = pbr->getTextureCoordinateSetsEXTProperty();
        checks.Add("texture coordinate set per map", "base 0, normal 1 (KHR_texture_transform texCoord override), metallic-roughness 0, emissive 0, occlusion 1",
                   std::to_string(sets[0]) + " " + std::to_string(sets[1]) + " " + std::to_string(sets[2]) + " " + std::to_string(sets[3]) + " " + std::to_string(sets[4]),
                   sets[0] == 0 && sets[1] == 1 && sets[2] == 0 && sets[3] == 0 && sets[4] == 1);
        const auto& transforms = pbr->getTextureTransformsEXTProperty();
        const TextureTransformEXT base = transforms[0], normal = transforms[1], occlusion = transforms[4];
        checks.Add("KHR_texture_transform on the base-colour map", "offset (0.25, 0.5), scale (2, 3), rotation 0.5", "offset " + V2(base.Offset) + ", scale " + V2(base.Scale) + ", rotation " + F(base.Rotation),
                   base.Offset == Vector2(0.25f, 0.5f) && base.Scale == Vector2(2.0f, 3.0f) && base.Rotation == 0.5f);
        checks.Add("KHR_texture_transform on the normal map", "offset (0.1, 0.2), scale (0.5, 0.5), rotation 0", "offset " + V2(normal.Offset) + ", scale " + V2(normal.Scale) + ", rotation " + F(normal.Rotation),
                   normal.Offset == Vector2(0.1f, 0.2f) && normal.Scale == Vector2(0.5f, 0.5f) && normal.Rotation == 0.0f);
        checks.Add("a map without a transform", "the identity (occlusion)", "offset " + V2(occlusion.Offset) + ", scale " + V2(occlusion.Scale), occlusion == TextureTransformEXT{});
        checks.Add("metallicFactor, roughnessFactor", "0.25, 0.75", F(pbr->getMetallicFactorProperty()) + ", " + F(pbr->getRoughnessFactorProperty()),
                   pbr->getMetallicFactorProperty() == 0.25f && pbr->getRoughnessFactorProperty() == 0.75f);
        checks.Add("KHR_materials_ior", "ior 1.33", F(pbr->getIorEXTProperty()), pbr->getIorEXTProperty() == 1.33f);
        const Vector3 sc = pbr->getSpecularColorFactorEXTProperty();
        checks.Add("KHR_materials_specular", "specularFactor 0.7, specularColorFactor (0.9, 0.8, 0.7)", F(pbr->getSpecularFactorEXTProperty()) + ", (" + F(sc.X) + ", " + F(sc.Y) + ", " + F(sc.Z) + ")",
                   pbr->getSpecularFactorEXTProperty() == 0.7f && sc == Vector3(0.9f, 0.8f, 0.7f));
    });

    checks.Group("sparse", [&] {
        Model model = manager.Load<Model>("Workshop/sparse");
        ModelMeshPart* part = FirstPart(model);
        if (part == nullptr) {
            checks.Add("Load<Model>(\"Workshop/sparse\")", "one mesh part", "no part", false);
            return;
        }
        Layout layout;
        const std::vector<Vertex> vertices = PartVertices(*part, layout);
        int matched = 0, uvOk = 0;
        std::string positions;
        for (const Vertex& v : vertices) {
            const int i = Authored(kSparsePositions, v.position);
            positions += (positions.empty() ? "" : " ") + std::string("(") + F(v.position.X) + "," + F(v.position.Y) + "," + F(v.position.Z) + ")";
            if (i < 0) continue;
            ++matched;
            if (v.uv0 && *v.uv0 == kSparseUv[static_cast<std::size_t>(i)]) ++uvOk;
        }
        checks.Add("sparse POSITION over a bufferView", "vertices 1 and 3 replaced: (2,-1.5,0.25) and (-1.25,2,-0.5); 0 and 2 from the bufferView", Checks::Short(positions, 120), matched == 4 && vertices.size() == 4);
        checks.Add("sparse TEXCOORD_0 with no bufferView", "zeros except vertex 2: (0.5, 0.75)", std::to_string(uvOk) + " of " + std::to_string(matched) + " as written" + (layout.uv0 < 0 ? " (no texture coordinate element)" : ""), matched == 4 && uvOk == 4);
    });

    checks.Group("morph", [&] {
        Model model = manager.Load<Model>("Workshop/morph");
        ModelMeshPart* part = FirstPart(model);
        auto* morph = part != nullptr ? dynamic_cast<MorphTargetDataEXT*>(part->getTagProperty()) : nullptr;
        checks.Add("MorphTargetDataEXT on the part", "present, two targets", morph != nullptr ? std::to_string(morph->PositionDeltas.size()) + " targets" : "none", morph != nullptr && morph->PositionDeltas.size() == 2);
        if (morph == nullptr || morph->PositionDeltas.size() != 2) return;
        Layout layout = LayoutOf(part->getVertexBufferProperty()->getVertexDeclarationProperty());
        layout.stride = morph->Stride;
        const int count = morph->Stride > 0 ? static_cast<int>(morph->BaseVertexBytes.size()) / morph->Stride : 0;
        const std::vector<Vertex> base = Decode(morph->BaseVertexBytes, layout, 0, count);
        int matched = 0, dense = 0, sparse = 0;
        for (std::size_t v = 0; v < base.size(); ++v) {
            const int i = Authored(kQuad, base[v].position);
            if (i < 0 || v >= morph->PositionDeltas[0].size() || v >= morph->PositionDeltas[1].size()) continue;
            ++matched;
            if (morph->PositionDeltas[0][v] == kMorphDense[static_cast<std::size_t>(i)]) ++dense;
            if (morph->PositionDeltas[1][v] == kMorphSparse[static_cast<std::size_t>(i)]) ++sparse;
        }
        checks.Add("target 0 (dense) position deltas", "z +0.5, +0.25, -0.25, -0.5 at the four corners", std::to_string(dense) + " of " + std::to_string(matched), matched == 4 && dense == 4);
        checks.Add("target 1 (sparse, no bufferView) position deltas", "(0,1,1) at vertex 2, zero elsewhere", std::to_string(sparse) + " of " + std::to_string(matched), matched == 4 && sparse == 4);
        const std::vector<float>& w = morph->Weights;
        checks.Add("the mesh's default weights", "0.25, 0.5", w.size() == 2 ? F(w[0]) + ", " + F(w[1]) : std::to_string(w.size()) + " weights", w.size() == 2 && w[0] == 0.25f && w[1] == 0.5f);
    });

    checks.Group("extensions", [&] {
        checks.ExpectThrow<Microsoft::Xna::Framework::Content::ContentLoadException>("extensionsRequired: [\"EXT_museum_imaginary_feature\"]", "ContentLoadException naming the extension (glTF 2.0 section 3.12)",
                                                                                     [&] { (void)manager.Load<Model>("Workshop/required"); });
        try {
            (void)manager.Load<Model>("Workshop/required");
        } catch (const std::exception& e) {
            const bool named = std::string(e.what()).find(kImaginary) != std::string::npos;
            checks.Add("the refusal names the extension", "the message contains EXT_museum_imaginary_feature", named ? "named" : Checks::Short(e.what(), 120), named);
        }
        Model used = manager.Load<Model>("Workshop/used");
        const GltfImportReportEXT& report = used.getGltfImportReportEXTProperty();
        const GltfImportDiagnosticEXT* hit = nullptr;
        for (const GltfImportDiagnosticEXT& d : report.Diagnostics)
            if (Mentions(d, kImaginary)) hit = &d;
        checks.Add("extensionsUsed only: the file loads and the report names it", "a diagnostic of kind UnsupportedFeature mentioning EXT_museum_imaginary_feature",
                   hit != nullptr ? std::string(KindName(hit->Kind)) + " \"" + hit->Code + "\"" : std::to_string(report.Diagnostics.size()) + " diagnostics, none naming it",
                   hit != nullptr && hit->Kind == GltfImportDiagnosticKindEXT::UnsupportedFeature);
    });

    checks.Group("instancing", [&] {
        // An optional extension a loader does not implement must not stop the load (glTF 2.0 section 3.12); the room only reports what CNA
        // did with the three instances. Drawing one model several times from the application is not EXT_mesh_gpu_instancing.
        Model model = manager.Load<Model>("Workshop/instancing");
        const GltfImportReportEXT& r = model.getGltfImportReportEXTProperty();
        checks.Add("EXT_mesh_gpu_instancing used, not required", "the file loads", "loaded: " + std::to_string(model.getMeshesProperty().getCountProperty()) + " mesh(es)", true);
        std::string said;
        for (const GltfImportDiagnosticEXT& d : r.Diagnostics)
            if (Mentions(d, "EXT_mesh_gpu_instancing")) said = std::string(KindName(d.Kind)) + " \"" + d.Code + "\"";
        checks.Info("what CNA made of the three instances", "-", "MeshInstanceCount " + std::to_string(r.MeshInstanceCount) + ", NodeCount " + std::to_string(r.NodeCount) + (said.empty() ? ", no diagnostic names the extension" : ", report: " + said));
    });
}

}  // namespace Workshop

#endif  // MUSEUM_PIPELINE

// Model import laboratory: the XNA content pipeline's XImporter (DirectX text .x) and FbxImporter (ASCII FBX 6.1 or modern binary FBX 7.5) reading real files in
// the game. A cottage is written in the first two formats and a two-material quad exercises the binary record stream; each importer returns a NodeContent tree (frames, meshes,
// materials), ModelProcessor turns that into a ModelContent, and the museum draws every part with the colour of its material and the
// transform of its frame. Seeing the two files give the same model is the point: they are different formats of one scene.
// Without CNA's pipeline library (-DMUSEUM_PIPELINE=OFF) the room says so.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "Diagnostics/Log.hpp"
#include "Exhibits/Content/PipelineSupport.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ExhibitKit.hpp"
#include "Exhibits/Math/MathKit.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/RenderStates.hpp"

#ifdef MUSEUM_PIPELINE
#include "Microsoft/Xna/Framework/Content/Pipeline/ContentImporterAttribute.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/ModelImporters.hpp"
#endif

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace MathKit;

namespace {

#ifdef MUSEUM_PIPELINE
namespace Xna = Microsoft::Xna::Framework::Content::Pipeline;
namespace Processors = Microsoft::Xna::Framework::Content::Pipeline::Processors;
using PipelineSupport::ImporterContext;
using PipelineSupport::ProcessorContext;

std::string Join(const std::vector<std::string>& v)
{
    std::string text;
    for (const std::string& s : v) text += (text.empty() ? "" : ", ") + s;
    return text;
}

std::string AttributeText(const Xna::ContentImporterAttribute& a)
{
    return Join(a.getFileExtensionsProperty()) + ", \"" + a.getDisplayNameProperty() + "\", default processor " + a.getDefaultProcessorProperty();
}
#endif

class ModelImportLab final : public Exhibit {
public:
    ModelImportLab(const RoomDef& room, Services& services) : Exhibit(room, services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel("Importing a model file", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
#ifdef MUSEUM_PIPELINE
        p.AddChoice("File", {"cottage.x (XImporter)", "cottage.fbx (FbxImporter, ASCII 6.1)", "binary-modern.fbx (FbxImporter, binary 7.5)"}, 0, [this](int v) { file_ = v; Build(); });
        p.AddSlider("ModelProcessor.Scale", 0.5f, 2.0f, scale_, [this](float v) { scale_ = v; Build(); }, "%.2f", 0.05f);
        p.AddToggle("ModelProcessor.SwapWindingOrder", false, [this](bool v) { swap_ = v; Build(); });
        p.AddToggle("Spin", true, [this](bool v) { spin_ = v; });
        p.AddReadout("Importer", [this] { return attribute_; });
        p.AddReadout("Nodes / meshes / batches", [this] { return nodes_; });
        p.AddReadout("Import warnings", [this] { return warnings_; });
        p.AddReadout("Bones / meshes / parts", [this] { return shape_; });
        p.AddReadout("Triangles / Convert calls", [this] { return triangles_; });
        p.AddReadout("Result", [this] { return status_; });
        for (int i = 0; i < 4; ++i) p.AddReadout("Node " + std::to_string(i + 1), [this, i] { return i < static_cast<int>(tree_.size()) ? tree_[static_cast<std::size_t>(i)] : std::string("-"); });
#else
        p.AddReadout("Content pipeline", [] { return std::string("not built into this museum"); });
#endif
        MathKit::BuildStagePlinth(b, 0.0f, 0.0f, 0.4f);
        SignSpec what = Kit::Plate("Importing a 3D Model File",
                                   {"An importer reads a file format and returns a tree of nodes: frames, meshes, materials. XNA shipped XImporter (the DirectX .x text and binary format) and FbxImporter; here both read",
                                    "a cottage written as .x and ASCII FBX. The third choice is a modern binary FBX 7.5 with compressed arrays: XNA's old FBX SDK refused it, while CNA reads it. ModelProcessor builds and draws every result."});
        what.leftAlign = true;
        what.pixelWidth = 900;
        b.AddWallSign(Wall::East, 0.0f, 2.4f, 4.4f, 2.0f, what);
    }

    void OnEnter() override
    {
#ifdef MUSEUM_PIPELINE
        if (!built_) Build();
#endif
    }

#ifdef MUSEUM_PIPELINE
    void Update(const UpdateContext& ctx) override
    {
        if (ctx.nearby && spin_) time_ += ctx.dt;
    }
#endif

    void DrawOpaque(const DrawContext& ctx) override
    {
#ifdef MUSEUM_PIPELINE
        if (!built_) Build();
        if (parts_.empty()) return;
        GraphicsDevice& d = ctx.device;
        RenderStates::SetOpaque3D(d);
        d.setRasterizerStateProperty(RasterizerState::CullNone);
        // The cottage has no texture coordinates, so every vertex has the same one and the sampler sees zero derivatives. With a mip-mapped
        // filter (the default LinearWrap) Mesa's software GL turns that into a NaN level of detail and draws black; a filter without a mip
        // term has nothing to choose, so the room draws with LinearClamp.
        d.getSamplerStatesProperty()[0] = SamplerState::LinearClamp;
        const Matrix place = Matrix::CreateRotationY(time_ * 0.5f) * Matrix::CreateTranslation(R().ToWorld(0.0f, 0.5f, 0.0f));
        for (const PipelineSupport::ProcessedPart& part : parts_) {
            if (part.vertices.empty() || part.indices.empty()) continue;
            BasicEffect& fx = ctx.fx.BeginLit();
            fx.EnableDefaultLighting();
            fx.setPunctualLightEXT(PunctualLightEXT{});
            fx.setViewProperty(ctx.view.view);
            fx.setProjectionProperty(ctx.view.projection);
            fx.setWorldProperty(part.world * place);
            fx.setDiffuseColorProperty(part.diffuse);
            fx.setEmissiveColorProperty(Vector3(0, 0, 0));
            fx.setSpecularColorProperty(Vector3(0.2f, 0.2f, 0.2f));
            EffectBank::Apply(fx);
            d.DrawUserIndexedPrimitives(PrimitiveType::TriangleList, part.vertices.data(), 0, static_cast<int>(part.vertices.size()), part.indices.data(), 0, static_cast<int>(part.indices.size()) / 3);
        }
        d.getSamplerStatesProperty()[0] = SamplerState::LinearWrap;
        RenderStates::SetOpaque3D(d);
#endif
    }

private:
#ifdef MUSEUM_PIPELINE
    void Build()
    {
        built_ = true;
        parts_.clear();
        tree_.clear();
        try {
            ImporterContext importerContext;
            const std::string path = S().contentRoot + (file_ == 0 ? "/Models/Import/cottage.x" :
                                                        file_ == 1 ? "/Models/Import/cottage.fbx" :
                                                                     "/Models/Import/binary-modern.fbx");
            std::shared_ptr<Xna::Graphics::NodeContent> root;
            if (file_ == 0) {
                Xna::XImporter importer;
                attribute_ = AttributeText(Xna::XImporter::Attribute());
                root = importer.Import(path, importerContext);
            } else {
                Xna::FbxImporter importer;
                attribute_ = AttributeText(Xna::FbxImporter::Attribute());
                root = importer.Import(path, importerContext);
            }
            int meshes = 0, geometries = 0, positions = 0;
            PipelineSupport::DescribeTree(root, 0, tree_, meshes, geometries, positions);
            nodes_ = std::to_string(tree_.size()) + " / " + std::to_string(meshes) + " / " + std::to_string(geometries) + " (" + std::to_string(positions) + " positions)";
            warnings_ = importerContext.warnings() == 0 ? std::string("none") : std::to_string(importerContext.warnings()) + ": " + importerContext.lastWarning().substr(0, 40);

            Processors::ModelProcessor processor;
            processor.setScaleProperty(scale_);
            processor.setSwapWindingOrderProperty(swap_);
            ProcessorContext context;
            const std::shared_ptr<Processors::ModelContent> model = processor.Process(root, context);
            parts_ = PipelineSupport::ReadAllParts(*model);
            int triangles = 0;
            for (const auto& part : parts_) triangles += static_cast<int>(part.indices.size()) / 3;
            int parts = 0;
            for (const auto& m : model->getMeshesProperty()) parts += static_cast<int>(m->getMeshPartsProperty().size());
            shape_ = std::to_string(model->getBonesProperty().size()) + " / " + std::to_string(model->getMeshesProperty().size()) + " / " + std::to_string(parts);
            triangles_ = std::to_string(triangles) + " / " + std::to_string(context.conversions);
            status_ = "imported, processed and drawn";
        } catch (const std::exception& e) {
            parts_.clear();
            status_ = std::string("could not import: ") + std::string(e.what()).substr(0, 70);
            Museum::Log("modelimport", "%s", e.what());
        }
    }

    std::vector<PipelineSupport::ProcessedPart> parts_;
    std::vector<std::string> tree_;
    std::string attribute_ = "-", nodes_ = "-", warnings_ = "-", shape_ = "-", triangles_ = "-", status_ = "-";
    float scale_ = 1.0f;
    int file_ = 0;
    bool swap_ = false, spin_ = true, built_ = false;
#endif
    float time_ = 0.0f;
};

}  // namespace

std::unique_ptr<Exhibit> MakeModelImportLab(const RoomDef& room, Services& services)
{
    return std::make_unique<ModelImportLab>(room, services);
}

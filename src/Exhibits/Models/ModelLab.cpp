// Model laboratory: CNA's glTF import and Model API on show. Rooms edit the effects of a *private* copy of a model
// (loaded through the room's own ContentManager), so the shared models used by other rooms are never touched.
#include <algorithm>
#include <cmath>
#include <exception>
#include <string>

#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ExhibitKit.hpp"
#include "Exhibits/Math/MathKit.hpp"
#include "Microsoft/Xna/Framework/BoundingFrustum.hpp"
#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/Graphics/AnimationPlayer.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/GltfImportReportEXT.hpp"
#include "Microsoft/Xna/Framework/Graphics/Model.hpp"
#include "Microsoft/Xna/Framework/Graphics/MorphTargetEXT.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelBone.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelMesh.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelMeshPart.hpp"
#include "Microsoft/Xna/Framework/Graphics/AlphaModeEXT.hpp"
#include "Microsoft/Xna/Framework/Graphics/IEffectMatrices.hpp"
#include "Microsoft/Xna/Framework/Graphics/PbrEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/TextureAddressMode.hpp"
#include "Microsoft/Xna/Framework/Graphics/TextureFilter.hpp"
#include "Microsoft/Xna/Framework/Graphics/TextureTransformEXT.hpp"
#include "Rendering/TextureLibrary.hpp"
#include "Microsoft/Xna/Framework/Graphics/SkinnedPbrEffect.hpp"
#include "Exhibits/Models/ProceduralIbl.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/RenderStates.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace MathKit;

namespace {

enum class Mode { Info, Pbr, Rigid, Skinned, Instances, Bounds, Effects, Report, Ibl, Morph, Variants, Material, Wrap };

Mode ParseMode(const std::string& s)
{
    static const std::pair<const char*, Mode> table[] = {{"pbr", Mode::Pbr}, {"rigid", Mode::Rigid}, {"skinned", Mode::Skinned}, {"instances", Mode::Instances},
                                                         {"bounds", Mode::Bounds}, {"effects", Mode::Effects}, {"report", Mode::Report}, {"ibl", Mode::Ibl}, {"morph", Mode::Morph}, {"variants", Mode::Variants}, {"material", Mode::Material}, {"wrap", Mode::Wrap}};
    for (const auto& [name, mode] : table)
        if (s == name) return mode;
    return Mode::Info;
}

const char* kNames[] = {"cube", "sphere", "torus", "cone", "star", "knot", "vase", "robot", "tentacle"};

class ModelLab final : public Exhibit {
public:
    ~ModelLab() override { RestoreOriginalEffects(); }
    ModelLab(const RoomDef& room, Services& services) : Exhibit(room, services), mode_(ParseMode(room.params.Get("mode"))), gizmos_(services)
    {
        name_ = room.params.Get("model", mode_ == Mode::Rigid ? "robot" : (mode_ == Mode::Skinned ? "tentacle" : (mode_ == Mode::Morph ? "morph" : (mode_ == Mode::Variants ? "variants" : (mode_ == Mode::Wrap ? "wrapquads" : "knot")))));
    }

    void BuildStatic(RoomBuilder& b) override
    {
        BuildStagePlinth(b, 0.0f, 0.0f, 0.4f);
        ControlPanel& p = AddPanel(Title(), -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        switch (mode_) {
            case Mode::Info:
                p.AddChoice("Model", {"cube", "sphere", "torus", "cone", "star", "knot", "vase", "robot", "tentacle"}, 5, [this](int v) { name_ = kNames[v]; Reload(); });
                p.AddSlider("Spin", 0.0f, 2.0f, spin_, [this](float v) { spin_ = v; });
                p.AddReadout("Meshes / parts / bones", [this] { return Counts(); });
                p.AddReadout("Triangles", [this] { return Triangles(); });
                p.AddReadout("Root bone / its children", [this] { return RootBone(); });
                p.AddReadout("CopyBoneTransformsTo", [this] { return BoneCopy(); });
                p.AddButton("CopyBoneTransformsFrom: move a bone", [this] { BoneRoundTrip(); });
                p.AddReadout("Moved, then restored", [this] { return roundTrip_; });
                p.AddReadout("Part effect class", [this] { return EffectName(); });
                p.AddReadout("Part topology / base-colour sampler (CNA ext.)", [this] { return PartState(); });
                p.AddReadout("BoundingSphere (CNA ext.)", [this] { return Bounds(); });
                p.AddReadout("Skins / animation payload", [this] { return Payload(); });
                break;
            case Mode::Pbr:
                p.AddSlider("MetallicFactor", 0.0f, 1.0f, metal_, [this](float v) { metal_ = v; });
                p.AddSlider("RoughnessFactor", 0.04f, 1.0f, rough_, [this](float v) { rough_ = v; });
                p.AddSlider("Base colour hue", 0.0f, 1.0f, hue_, [this](float v) { hue_ = v; });
                p.AddSlider("EmissiveFactor", 0.0f, 1.0f, emissive_, [this](float v) { emissive_ = v; });
                p.AddHeading("glTF models arrive with PbrEffect (CNA extension)");
                break;
            case Mode::Rigid:
                p.AddToggle("Play the glTF clip", true, [this](bool v) { play_ = v; });
                p.AddSlider("Speed", 0.0f, 3.0f, speed_, [this](float v) { speed_ = v; });
                p.AddSlider("Time (scrub)", 0.0f, 4.0f, clock_, [this](float v) { clock_ = v; play_ = false; }, "%.2f s");
                p.AddReadout("Bones", [this] { return model_ ? std::to_string(model_->getBonesProperty().getCountProperty()) : std::string("-"); });
                p.AddReadout("Clip", [this] { return clip_ != nullptr ? std::string("wave, ") + Fmt(clipLength_) + " s" : std::string("none found"); });
                break;
            case Mode::Skinned:
                p.AddToggle("AnimationPlayer running", true, [this](bool v) { play_ = v; });
                p.AddSlider("Speed", 0.0f, 3.0f, speed_, [this](float v) { speed_ = v; });
                p.AddToggle("Show the skeleton", true, [this](bool v) { skeleton_ = v; });
                p.AddButton("ApplyBindPoseBoneTransformsEXT: the rest pose", [this] { BindPose(); });
                p.AddReadout("Bind pose applied to", [this] { return bindPoseText_; });
                p.AddReadout("Joints", [this] { return skin_ ? std::to_string(skin_->BoneCount) : std::string("-"); });
                p.AddReadout("Clips", [this] { return skin_ ? std::to_string(skin_->AnimationClips.size()) : std::string("-"); });
                break;
            case Mode::Instances:
                p.AddSlider("Instances", 1.0f, 400.0f, static_cast<float>(count_), [this](float v) { count_ = static_cast<int>(v); }, "%.0f", 1.0f);
                p.AddToggle("Cull with BoundingFrustum", true, [this](bool v) { cull_ = v; });
                p.AddReadout("Drawn this frame", [this] { return std::to_string(drawn_) + " of " + std::to_string(count_); });
                p.AddReadout("Frame rate", [this] { return Fmt(S().stats->fps) + " fps"; });
                break;
            case Mode::Bounds:
                p.AddSlider("Scale", 0.5f, 2.0f, scale_, [this](float v) { scale_ = v; });
                p.AddToggle("Model sphere", true, [this](bool v) { showModel_ = v; });
                p.AddToggle("Per-mesh spheres", true, [this](bool v) { showMeshes_ = v; });
                p.AddReadout("ModelMesh.BoundingSphere", [this] { return std::string("one per mesh, in model space"); });
                break;
            case Mode::Effects:
                p.AddChoice("Effect on every part", {"Original (PbrEffect)", "BasicEffect, lit", "BasicEffect, unlit"}, 0, [this](int v) { effectChoice_ = v; ApplyEffects(); });
                p.AddReadout("ModelMeshPart.Effect", [this] { return EffectName(); });
                break;
            case Mode::Wrap:
                p.AddToggle("Apply each part's sampler state before drawing", true, [this](bool v) { applySamplers_ = v; });
                p.AddReadout("Left quad: part sampler (slot 0)", [this] { return SamplerOf(0); });
                p.AddReadout("Middle quad: part sampler (slot 0)", [this] { return SamplerOf(1); });
                p.AddReadout("Right quad: part sampler (slot 0)", [this] { return SamplerOf(2); });
                p.AddReadout("What happens if it is not applied", [] { return std::string("every quad draws with the device's LinearWrap"); });
                break;
            case Mode::Material:
                p.AddChoice("Base colour texture", {"brick", "coin (has alpha)", "checker"}, 0, [this](int v) { texture_ = v; });
                p.AddToggle("Normal map (generated in code)", true, [this](bool v) { normalOn_ = v; });
                p.AddToggle("Emissive map (circuit)", false, [this](bool v) { emissiveOn_ = v; });
                p.AddSlider("Texture offset U", -1.0f, 1.0f, uvOffset_[0], [this](float v) { uvOffset_[0] = v; });
                p.AddSlider("Texture offset V", -1.0f, 1.0f, uvOffset_[1], [this](float v) { uvOffset_[1] = v; });
                p.AddSlider("Texture scale", 0.25f, 4.0f, uvScale_, [this](float v) { uvScale_ = v; });
                p.AddSlider("Texture rotation (degrees)", 0.0f, 360.0f, uvRotation_, [this](float v) { uvRotation_ = v; }, "%.0f", 1.0f);
                p.AddChoice("Alpha mode (CNAEXT)", {"Opaque", "Mask", "Blend"}, 0, [this](int v) { alphaMode_ = v; });
                p.AddSlider("Alpha cutoff (Mask)", 0.05f, 0.95f, cutoff_, [this](float v) { cutoff_ = v; });
                break;
            case Mode::Morph:
                p.AddToggle("Play the glTF weight track", true, [this](bool v) { play_ = v; });
                p.AddSlider("Weight of target 0: squash", 0.0f, 1.0f, morphWeights_[0], [this](float v) { morphWeights_[0] = v; play_ = false; morphDirty_ = true; });
                p.AddSlider("Weight of target 1: spikes", 0.0f, 1.0f, morphWeights_[1], [this](float v) { morphWeights_[1] = v; play_ = false; morphDirty_ = true; });
                p.AddReadout("Morph targets", [this] { return morph_ != nullptr ? std::to_string(morph_->PositionDeltas.size()) + " (positions and normals)" : std::string("none found"); });
                p.AddReadout("Weight track", [this] { return morph_ != nullptr ? std::to_string(morph_->WeightTrack.Keys.size()) + " keys" : std::string("-"); });
                break;
            case Mode::Variants:
                p.AddChoice("Material variant", {"gold", "red plastic", "blue rubber", "brushed steel"}, 0, [this](int v) { variant_ = v; variantDirty_ = true; });
                p.AddSlider("Spin", 0.0f, 2.0f, spin_, [this](float v) { spin_ = v; });
                p.AddReadout("Variants in the file", [this] { return model_ ? std::to_string(model_->getMaterialVariantNamesEXTProperty().size()) : std::string("-"); });
                p.AddReadout("Active variant index", [this] { return model_ ? std::to_string(model_->getMaterialVariantEXTProperty()) : std::string("-"); });
                break;
            case Mode::Ibl:
                p.AddChoice("Model", {"sphere", "torus", "knot", "vase"}, 0, [this](int v) { static const char* n[] = {"sphere", "torus", "knot", "vase"}; name_ = n[v]; Reload(); });
                p.AddChoice("Environment", {"Sky", "Sunset", "Studio"}, 0, [this](int v) { env_ = v; iblDirty_ = true; });
                p.AddToggle("Image-based lighting", true, [this](bool v) { iblOn_ = v; });
                p.AddSlider("Intensity", 0.0f, 2.0f, iblIntensity_, [this](float v) { iblIntensity_ = v; });
                p.AddSlider("MetallicFactor", 0.0f, 1.0f, metal_, [this](float v) { metal_ = v; });
                p.AddSlider("RoughnessFactor", 0.04f, 1.0f, rough_, [this](float v) { rough_ = v; });
                p.AddReadout("Environment maps", [this] { return std::string("irradiance 16, specular 64 x 5 mips, BRDF 32 x 32"); });
                p.AddReadout("Renderer uses the environment", [this] { return std::string(S().device->SupportsImageBasedLightingEXT() ? "yes (SupportsImageBasedLightingEXT)" : "no: the light is carried but not shaded"); });
                break;
            case Mode::Report:
                p.AddChoice("Model", {"cube", "sphere", "torus", "cone", "star", "knot", "vase", "robot", "tentacle"}, 7, [this](int v) { name_ = kNames[v]; Reload(); });
                p.AddHeading("GltfImportReportEXT (CNA extension)");
                p.AddReadout("Nodes / meshes / primitives", [this] { return ReportLine(0); });
                p.AddReadout("Skins / animations / clips", [this] { return ReportLine(1); });
                p.AddReadout("Diagnostics", [this] { return ReportLine(2); });
                p.AddReadout("Anything lost on import?", [this] { return ReportLine(3); });
                break;
        }
        p.AddReadout("PbrEffect on this renderer", [this] { return refusal_.empty() ? std::string("drawn") : std::string("refused: BasicEffect used"); });
        SignSpec what = Kit::Plate(Title(), Explanation());
        what.leftAlign = true;
        what.pixelWidth = 900;
        b.AddWallSign(Wall::East, 0.0f, 2.4f, 4.4f, 2.0f, what);
    }

    void Load(InteractionSystem&, MuseumWorld&) override
    {
        content_ = std::make_unique<Content::ContentManager>(&S().game->getServicesProperty(), S().contentRoot);
        Reload();
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
        time_ += ctx.dt;
        spin_t_ += ctx.dt * spin_;
        if (mode_ == Mode::Rigid && model_ && clip_ != nullptr) {
            if (play_) clock_ = std::fmod(clock_ + ctx.dt * speed_, std::max(0.1f, clipLength_));
            ApplyClipToBonesEXT(*model_, *clip_, System::TimeSpan::FromSeconds(clock_));
        }
        if (mode_ == Mode::Morph && morph_ != nullptr && morphPart_ != nullptr) {
            if (play_) {
                clock_ = std::fmod(clock_ + ctx.dt, 4.0f);
                morphWeights_ = EvaluateMorphWeightsEXT(morph_->WeightTrack, static_cast<double>(clock_));
                morphWeights_.resize(2, 0.0f);
                morphDirty_ = true;
            }
            if (morphDirty_) {
                try { SetMorphWeightsEXT(*morphPart_, morphWeights_); } catch (const std::exception& e) {
                    Museum::Log("models", "morph: %s", e.what());
                    static bool reported = false;   // per-frame path while the animation plays: report the first failure only
                    if (!reported) { reported = true; ReportError(std::string("SetMorphWeightsEXT: ") + e.what()); }
                }
                morphDirty_ = false;
            }
        }
        if (mode_ == Mode::Variants && model_ && variantDirty_) {
            try {
                model_->setMaterialVariantEXTProperty(variant_);
                for (ModelMesh* mesh : model_->getMeshesProperty())
                    for (int i = 0; i < mesh->getMeshPartsProperty().getCountProperty(); ++i)
                        if (auto* pbr = dynamic_cast<PbrEffect*>(mesh->getMeshPartsProperty()[i]->getEffectProperty())) {
                            pbr->EnableDefaultLighting();
                            if (!ibl_.irradiance) ibl_ = ProceduralIbl::Build(*S().device, 0);   // metals need an environment to reflect
                            pbr->setImageBasedLightEXT(ibl_.Light(0.8f));
                        }
            } catch (const std::exception& e) {
                Museum::Log("models", "variant: %s", e.what());
                ReportError(std::string("material variant: ") + e.what());
            }
            variantDirty_ = false;
        }
        if (mode_ == Mode::Skinned && player_ && play_) player_->Update(System::TimeSpan::FromSeconds(ctx.dt * speed_), true, true);
        if (mode_ == Mode::Skinned && player_ && model_) {
            for (ModelMesh* mesh : model_->getMeshesProperty())
                for (int i = 0; i < mesh->getMeshPartsProperty().getCountProperty(); ++i)
                    if (auto* sk = dynamic_cast<SkinnedPbrEffect*>(mesh->getMeshPartsProperty()[i]->getEffectProperty())) sk->SetBoneTransforms(player_->GetSkinTransforms());
        }
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
        if (!model_) return;
        GraphicsDevice& d = ctx.device;
        RenderStates::SetOpaque3D(d);
        const Vector3 o = R().ToWorld(0.0f, 0.4f, 0.0f);
        switch (mode_) {
            case Mode::Instances: DrawInstances(ctx, o); break;
            case Mode::Bounds: DrawBounds(ctx, o); break;
            case Mode::Report: break;
            case Mode::Wrap: DrawWrap(ctx, o); break;
            default: {
                const float scale = FitScale();
                const Matrix world = Matrix::CreateScale(scale) * (mode_ == Mode::Variants ? Matrix::CreateRotationX(MathHelper::ToRadians(62.0f)) : Matrix::getIdentityProperty()) * Matrix::CreateRotationY(mode_ == Mode::Rigid || mode_ == Mode::Skinned ? spin_t_ * 0.4f : spin_t_) * Matrix::CreateTranslation(o + Vector3(0, mode_ == Mode::Skinned || mode_ == Mode::Rigid ? 0.0f : 1.3f, 0));
                if (mode_ == Mode::Pbr) ConfigurePbr();
                if (mode_ == Mode::Ibl) ConfigureIbl(ctx);
                if (mode_ == Mode::Material) ConfigureMaterial(ctx);
                DrawModel(ctx, world);
                if (mode_ == Mode::Rigid) DrawBones(ctx, world);
                if (mode_ == Mode::Skinned && skeleton_ && player_) DrawSkeleton(ctx, world);
            }
        }
        RenderStates::SetOpaque3D(d);
        gizmos_.Flush(ctx);
    }
    void DrawOverlay() override { gizmos_.DrawLabels(); }

private:
    [[nodiscard]] std::string Title() const
    {
        static const char* t[] = {"Loading a glTF model", "PbrEffect: metallic and roughness", "Rigid animation from a glTF clip", "Skinned animation", "Many model instances", "Model and mesh bounds", "Swapping a model's effects", "The glTF import report", "Image-based lighting", "Morph targets", "Material variants", "PBR material maps", "Sampler states from a glTF file"};
        return t[static_cast<int>(mode_)];
    }
    [[nodiscard]] std::vector<std::string> Explanation() const
    {
        switch (mode_) {
            case Mode::Wrap: return {"A glTF texture carries a sampler: filter and wrap mode. CNA keeps it on each ModelMeshPart (SamplerStates, CNA extension) and leaves the device alone,", "so the application applies it before the draw. The three quads share one picture and UVs from 0 to 2: repeat tiles it, clamp smears its edge, mirror flips alternate tiles."};
            case Mode::Info: return {"ContentManager.Load<Model> reads a glTF file into a Model: meshes made of parts, a bone tree, and for each part", "an Effect. Pick a model to see what the importer produced. Materials arrive as PbrEffect (a CNA extension)."};
            case Mode::Pbr: return {"glTF materials are metallic-roughness: MetallicFactor 1 turns a surface into mirror-like metal that takes its", "colour from reflections, RoughnessFactor blurs the highlight. The model's own PbrEffects are edited on a private copy."};
            case Mode::Rigid: return {"A glTF animation that moves ordinary nodes arrives as a clip in the model's Tag (ModelAnimationsEXT).", "ApplyClipToBonesEXT poses the bones at a time; Model.Draw then uses the absolute bone transforms. Axes mark the joints."};
            case Mode::Skinned: return {"A skinned mesh bends with a skeleton. CNA imports the joints into SkinningData and a clip; AnimationPlayer", "turns time into skin matrices, which go to the model's SkinnedPbrEffect with SetBoneTransforms every frame."};
            case Mode::Instances: return {"Drawing a model many times is one Model.Draw per copy. BoundingFrustum.Contains on the model's bounding", "sphere skips copies you cannot see: stand in the room and look away to watch the drawn count drop."};
            case Mode::Bounds: return {"Every Model has a merged BoundingSphere (CNA extension) and every ModelMesh its own: the standard inputs for", "culling and picking. Spheres are in model space, so they are transformed by the world matrix to draw."};
            case Mode::Effects: return {"A ModelMeshPart's Effect can be replaced: here every part is switched between its PbrEffect and a", "BasicEffect (lit or flat). The geometry never changes; only how it is shaded does."};
            case Mode::Material: return {"PbrEffect takes a base colour map, a normal map, an emissive map and (CNA extension) a texture transform that", "offsets, scales and rotates the UVs of every map, plus an alpha mode: Opaque, Mask (cut-out) or Blend. The sphere shows each at once."};
            case Mode::Morph: return {"A glTF mesh can carry morph targets: per-vertex position and normal deltas blended by weights. CNA keeps them in the", "mesh part's Tag (MorphTargetDataEXT); SetMorphWeightsEXT re-uploads the blended vertices, EvaluateMorphWeightsEXT reads the file's animation."};
            case Mode::Variants: return {"KHR_materials_variants lets one glTF mesh carry several looks. CNA exposes their names and the active index on the", "Model (CNA extension); switching swaps the effects of the mesh parts: no geometry or file is reloaded."};
            case Mode::Ibl: return {"ImageBasedLightEXT gives PbrEffect a whole environment instead of a few lights: an irradiance cube for diffuse", "light, a prefiltered specular cube whose mip is the roughness, and a BRDF table. All three are generated in code here."};
            default: return {"CNA reports what the glTF importer did: counts of nodes, skins and clips, plus a diagnostic for anything", "approximated, generated or dropped. Nothing is lost silently: pick a model and read its report."};
        }
    }

    // The model's parts belong to the content manager's cached asset, so a stand-in effect handed to a part (ApplyEffects) must be
    // taken back before this room's own BasicEffects are destroyed, or the next load of the same asset finds dangling effects.
    void RestoreOriginalEffects()
    {
        if (model_ != nullptr && !originals_.empty()) {
            std::size_t k = 0;
            for (ModelMesh* mesh : model_->getMeshesProperty())
                for (int i = 0; i < mesh->getMeshPartsProperty().getCountProperty(); ++i, ++k)
                    if (k < originals_.size()) mesh->getMeshPartsProperty()[i]->setEffectProperty(originals_[k]);
        }
        overrides_.clear();
    }

    void Reload()
    {
        RestoreOriginalEffects();
        model_.reset();
        clip_ = nullptr;
        skin_ = nullptr;
        player_.reset();
        try {
            model_ = std::make_unique<Model>(content_->Load<Model>("Models/" + name_));
        } catch (const std::exception& e) {
            Museum::Log("models", "cannot load %s: %s", name_.c_str(), e.what());
            ReportError("cannot load Models/" + name_ + ": " + e.what());
            return;
        }
        if (auto* anim = dynamic_cast<ModelAnimationsEXT*>(model_->getTagProperty())) {
            const auto it = anim->Clips.find("wave");
            if (it != anim->Clips.end()) {
                clip_ = &it->second;
                clipLength_ = static_cast<float>(clip_->Duration.getTotalSecondsProperty());
            } else if (!anim->Clips.empty()) {
                clip_ = &anim->Clips.begin()->second;
                clipLength_ = static_cast<float>(clip_->Duration.getTotalSecondsProperty());
            }
        }
        if (auto* sd = dynamic_cast<SkinningData*>(model_->getTagProperty())) {
            skin_ = sd;
            if (!sd->AnimationClips.empty()) {
                player_ = std::make_unique<AnimationPlayer>(*sd);
                player_->StartClip(sd->AnimationClips.begin()->second);
            }
        }
        // Everything is drawn with lighting: PbrEffect and SkinnedPbrEffect default to no lights.
        for (ModelMesh* mesh : model_->getMeshesProperty())
            for (int i = 0; i < mesh->getMeshPartsProperty().getCountProperty(); ++i) {
                Effect* fx = mesh->getMeshPartsProperty()[i]->getEffectProperty();
                if (auto* pbr = dynamic_cast<PbrEffect*>(fx)) pbr->EnableDefaultLighting();
                if (auto* sk = dynamic_cast<SkinnedPbrEffect*>(fx)) sk->EnableDefaultLighting();
                if (auto* basic = dynamic_cast<BasicEffect*>(fx)) basic->EnableDefaultLighting();
            }
        morph_ = nullptr;
        morphPart_ = nullptr;
        for (ModelMesh* mesh : model_->getMeshesProperty())
            for (int i = 0; i < mesh->getMeshPartsProperty().getCountProperty(); ++i)
                if (auto* md = dynamic_cast<MorphTargetDataEXT*>(mesh->getMeshPartsProperty()[i]->getTagProperty())) {
                    morph_ = md;
                    morphPart_ = mesh->getMeshPartsProperty()[i];
                }
        variantDirty_ = true;
        originals_.clear();
        for (ModelMesh* mesh : model_->getMeshesProperty())
            for (int i = 0; i < mesh->getMeshPartsProperty().getCountProperty(); ++i) originals_.push_back(mesh->getMeshPartsProperty()[i]->getEffectProperty());
        overrides_.clear();
        if (mode_ == Mode::Effects || !refusal_.empty()) ApplyEffects();
    }

    [[nodiscard]] float FitScale() const
    {
        const auto sphere = model_->getBoundingSphereEXTProperty();
        const float radius = sphere ? std::max(0.1f, sphere->Radius) : 0.5f;
        return (mode_ == Mode::Rigid || mode_ == Mode::Skinned ? 1.6f : 1.5f) / radius * (mode_ == Mode::Rigid ? 0.55f : 0.8f);
    }

    void DrawModel(const DrawContext& ctx, const Matrix& world)
    {
        ctx.device.setRasterizerStateProperty(RasterizerState::CullClockwise);
        DrawGuarded(world, ctx);
    }

    // Keep the room usable on any reduced backend that refuses one of the imported model's effects:
    // swap every part to a lit BasicEffect (what Effects mode does by hand) and report the boundary.
    void DrawGuarded(const Matrix& world, const DrawContext& ctx)
    {
        try {
            model_->Draw(world, ctx.view.view, ctx.view.projection);
        } catch (const std::exception& e) {
            if (refusal_.empty()) Museum::Log("models", "%s: drawing refused (%s); using BasicEffect", name_.c_str(), std::string(e.what()).substr(0, 90).c_str());
            refusal_ = e.what();
            ApplyEffects();
            model_->Draw(world, ctx.view.view, ctx.view.projection);
        }
    }

    void ConfigurePbr()
    {
        for (ModelMesh* mesh : model_->getMeshesProperty())
            for (int i = 0; i < mesh->getMeshPartsProperty().getCountProperty(); ++i)
                if (auto* pbr = dynamic_cast<PbrEffect*>(mesh->getMeshPartsProperty()[i]->getEffectProperty())) {
                    const float h = hue_ - std::floor(hue_);
                    pbr->setDiffuseColorProperty(Vector3(0.5f + 0.5f * std::cos(h * MathHelper::TwoPi), 0.5f + 0.5f * std::cos((h - 0.333f) * MathHelper::TwoPi), 0.5f + 0.5f * std::cos((h - 0.667f) * MathHelper::TwoPi)));
                    pbr->setMetallicFactorProperty(metal_);
                    pbr->setRoughnessFactorProperty(rough_);
                    pbr->setEmissiveFactorProperty(Vector3(emissive_, emissive_ * 0.6f, emissive_ * 0.2f));
                }
    }

    void ConfigureMaterial(const DrawContext& ctx)
    {
        if (!normalMap_) {
            // A normal map from the brick texture's brightness: slopes of the height field become tilted normals.
            const int n = 128;
            std::vector<Color> px(static_cast<std::size_t>(n * n));
            const auto height = [&](int x, int y) { return std::sin(static_cast<float>(x) * 0.45f) * std::sin(static_cast<float>(y) * 0.45f) + (((x / 16) + (y / 16)) % 2 ? 0.0f : 0.6f); };
            for (int y = 0; y < n; ++y)
                for (int x = 0; x < n; ++x) {
                    const float dx = height((x + 1) % n, y) - height((x + n - 1) % n, y), dy = height(x, (y + 1) % n) - height(x, (y + n - 1) % n);
                    Vector3 nrm = Vector3::Normalize(Vector3(-dx * 0.5f, -dy * 0.5f, 1.0f));
                    px[static_cast<std::size_t>(y * n + x)] = Color(static_cast<int>((nrm.X * 0.5f + 0.5f) * 255.0f), static_cast<int>((nrm.Y * 0.5f + 0.5f) * 255.0f), static_cast<int>((nrm.Z * 0.5f + 0.5f) * 255.0f), 255);
                }
            normalMap_ = std::make_unique<Texture2D>(ctx.device, n, n);
            normalMap_->SetData(px.data(), static_cast<int>(px.size()));
        }
        static const char* names[] = {"brick", "coin", "checker"};
        TextureTransformEXT t;
        t.Offset = Vector2(uvOffset_[0], uvOffset_[1]);
        t.Scale = Vector2(uvScale_, uvScale_);
        t.Rotation = MathHelper::ToRadians(uvRotation_);
        const AlphaModeEXT mode = alphaMode_ == 0 ? AlphaModeEXT::Opaque : (alphaMode_ == 1 ? AlphaModeEXT::Mask : AlphaModeEXT::Blend);
        for (ModelMesh* mesh : model_->getMeshesProperty())
            for (int i = 0; i < mesh->getMeshPartsProperty().getCountProperty(); ++i)
                if (auto* pbr = dynamic_cast<PbrEffect*>(mesh->getMeshPartsProperty()[i]->getEffectProperty())) {
                    pbr->setDiffuseColorProperty(Vector3(1, 1, 1));
                    pbr->setTextureProperty(S().textures->Get(names[texture_]));
                    pbr->setNormalMapProperty(normalOn_ ? normalMap_.get() : nullptr);
                    pbr->setEmissiveMapProperty(emissiveOn_ ? S().textures->Get("circuit") : nullptr);
                    pbr->setEmissiveFactorProperty(emissiveOn_ ? Vector3(1, 1, 1) : Vector3(0, 0, 0));
                    pbr->setMetallicFactorProperty(0.0f);
                    pbr->setRoughnessFactorProperty(0.6f);
                    for (int slot = 0; slot < 5; ++slot) pbr->setTextureTransformEXTProperty(slot, t);
                    pbr->setAlphaModeEXTProperty(mode);
                    pbr->setAlphaCutoffEXTProperty(cutoff_);
                    pbr->setDoubleSidedEXTProperty(alphaMode_ != 0);
                    pbr->EnableDefaultLighting();
                }
        if (alphaMode_ == 2) RenderStates::SetTransparent3D(ctx.device, BlendState::NonPremultiplied);
    }

    void ConfigureIbl(const DrawContext& ctx)
    {
        if (iblDirty_ || !ibl_.irradiance) {
            ibl_ = ProceduralIbl::Build(ctx.device, env_);
            iblDirty_ = false;
        }
        const ImageBasedLightEXT light = iblOn_ ? ibl_.Light(iblIntensity_) : ImageBasedLightEXT{};
        for (ModelMesh* mesh : model_->getMeshesProperty())
            for (int i = 0; i < mesh->getMeshPartsProperty().getCountProperty(); ++i)
                if (auto* pbr = dynamic_cast<PbrEffect*>(mesh->getMeshPartsProperty()[i]->getEffectProperty())) {
                    pbr->setDiffuseColorProperty(Vector3(0.9f, 0.7f, 0.5f));
                    pbr->setMetallicFactorProperty(metal_);
                    pbr->setRoughnessFactorProperty(rough_);
                    pbr->setImageBasedLightEXT(light);
                    if (iblOn_) {
                        // The environment is the light: switch the stock three-light rig off so it is easy to see.
                        for (DirectionalLight* l : {&pbr->getDirectionalLight0Property(), &pbr->getDirectionalLight1Property(), &pbr->getDirectionalLight2Property()}) l->setEnabledProperty(false);
                        pbr->setAmbientLightColorProperty(Vector3(0, 0, 0));
                    } else {
                        pbr->EnableDefaultLighting();
                    }
                }
    }

    void DrawBones(const DrawContext& ctx, const Matrix& world)
    {
        (void)ctx;
        std::vector<Matrix> abs(static_cast<std::size_t>(model_->getBonesProperty().getCountProperty()));
        model_->CopyAbsoluteBoneTransformsTo(abs);
        for (std::size_t i = 0; i < abs.size(); ++i) {
            gizmos_.Axes(abs[i] * world, 0.22f);
            gizmos_.Label(Vector3::Transform(Vector3::Zero, abs[i] * world) + Vector3(0, 0.12f, 0), model_->getBonesProperty()[static_cast<int>(i)]->getNameProperty(), Color(180, 230, 255, 255), 0.4f);
        }
    }
    void DrawSkeleton(const DrawContext&, const Matrix& world)
    {
        const std::vector<Matrix>& w = player_->GetWorldTransforms();
        for (std::size_t i = 0; i < w.size(); ++i) {
            const Vector3 p = Vector3::Transform(Vector3::Zero, w[i] * world);
            if (i > 0) gizmos_.Line(Vector3::Transform(Vector3::Zero, w[static_cast<std::size_t>(skin_->SkeletonHierarchy[i] >= 0 ? skin_->SkeletonHierarchy[i] : 0)] * world), p, Color(255, 230, 90, 255));
            gizmos_.Box(BoundingBox(p - Vector3(0.04f, 0.04f, 0.04f), p + Vector3(0.04f, 0.04f, 0.04f)), Color(255, 120, 90, 255));
        }
    }

    void DrawInstances(const DrawContext& ctx, const Vector3& o)
    {
        const auto sphere = model_->getBoundingSphereEXTProperty();
        const float radius = sphere ? sphere->Radius : 0.5f;
        const BoundingFrustum frustum(ctx.view.view * ctx.view.projection);
        const int side = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(count_))));
        drawn_ = 0;
        ctx.device.setRasterizerStateProperty(RasterizerState::CullClockwise);
        for (int i = 0; i < count_; ++i) {
            const float x = (static_cast<float>(i % side) - static_cast<float>(side) * 0.5f) * 1.2f, z = (static_cast<float>(i / side) - static_cast<float>(side) * 0.5f) * 1.2f;
            const Vector3 p = o + Off(x, 0.5f, z);
            const float scale = 0.5f / radius;
            if (cull_ && frustum.Contains(BoundingSphere(p, radius * scale)) == ContainmentType::Disjoint) continue;
            DrawGuarded(Matrix::CreateScale(scale) * Matrix::CreateRotationY(spin_t_ * 0.5f + static_cast<float>(i)) * Matrix::CreateTranslation(p), ctx);
            ++drawn_;
        }
    }

    void DrawBounds(const DrawContext& ctx, const Vector3& o)
    {
        const Matrix world = Matrix::CreateScale(scale_ * FitScale()) * Matrix::CreateRotationY(spin_t_) * Matrix::CreateTranslation(o + Vector3(0, 1.3f, 0));
        DrawModel(ctx, world);
        std::vector<Matrix> abs(static_cast<std::size_t>(model_->getBonesProperty().getCountProperty()));
        model_->CopyAbsoluteBoneTransformsTo(abs);
        if (showModel_)
            if (const auto sphere = model_->getBoundingSphereEXTProperty()) gizmos_.Sphere(sphere->Transform(world), Color(255, 220, 90, 255), 40);
        if (showMeshes_)
            for (ModelMesh* mesh : model_->getMeshesProperty()) {
                const ModelBone* parent = mesh->getParentBoneProperty();
                const Matrix bone = parent != nullptr ? abs[static_cast<std::size_t>(parent->getIndexProperty())] : Matrix::getIdentityProperty();
                gizmos_.Sphere(mesh->getBoundingSphereProperty().Transform(bone * world), Color(110, 220, 255, 255), 28);
            }
    }

    void ApplyEffects()
    {
        if (!model_) return;
        std::size_t k = 0;
        // Once the renderer has refused PbrEffect parts, "original" means the lit BasicEffect stand-in.
        const int choice = effectChoice_ == 0 && !refusal_.empty() ? 1 : effectChoice_;
        overrides_.clear();
        for (ModelMesh* mesh : model_->getMeshesProperty())
            for (int i = 0; i < mesh->getMeshPartsProperty().getCountProperty(); ++i, ++k) {
                ModelMeshPart* part = mesh->getMeshPartsProperty()[i];
                if (choice == 0) {
                    part->setEffectProperty(originals_[k]);
                } else {
                    auto basic = std::make_unique<BasicEffect>(*S().device);
                    Vector3 colour(0.8f, 0.6f, 0.3f);
                    if (auto* pbr = dynamic_cast<PbrEffect*>(originals_[k])) colour = pbr->getDiffuseColorProperty();
                    basic->setDiffuseColorProperty(colour);
                    basic->setTextureEnabledProperty(false);
                    if (choice == 1) basic->EnableDefaultLighting();
                    else {
                        basic->setLightingEnabledProperty(false);
                        basic->setEmissiveColorProperty(colour);
                        basic->setDiffuseColorProperty(Vector3(0, 0, 0));
                    }
                    part->setEffectProperty(basic.get());
                    overrides_.push_back(std::move(basic));
                }
            }
    }

    [[nodiscard]] std::string Counts() const
    {
        if (!model_) return "-";
        int parts = 0;
        for (ModelMesh* m : model_->getMeshesProperty()) parts += m->getMeshPartsProperty().getCountProperty();
        return std::to_string(model_->getMeshesProperty().getCountProperty()) + " / " + std::to_string(parts) + " / " + std::to_string(model_->getBonesProperty().getCountProperty());
    }
    // The bone tree: Model.Root, ModelBone.Children, and the whole palette copied out in one call.
    [[nodiscard]] std::string RootBone() const
    {
        if (!model_ || model_->getRootProperty() == nullptr) return "-";
        const ModelBone* root = model_->getRootProperty();
        return "\"" + root->getNameProperty() + "\" / " + std::to_string(root->getChildrenProperty().getCountProperty()) + " child(ren)";
    }
    [[nodiscard]] std::string BoneCopy() const
    {
        if (!model_) return "-";
        std::vector<Matrix> bones(static_cast<std::size_t>(model_->getBonesProperty().getCountProperty()));
        model_->CopyBoneTransformsTo(bones);
        return std::to_string(bones.size()) + " matrices copied";
    }
    // Copies the bones out, moves the first bone's transform up a metre, writes the set back with CopyBoneTransformsFrom, reads it again
    // to see that the model took it, then writes the original back so the exhibit is left as it was.
    void BoneRoundTrip()
    {
        if (!model_ || model_->getBonesProperty().getCountProperty() == 0) { roundTrip_ = "no model"; return; }
        std::vector<Matrix> original(static_cast<std::size_t>(model_->getBonesProperty().getCountProperty()));
        model_->CopyBoneTransformsTo(original);
        std::vector<Matrix> changed = original;
        changed[0] = changed[0] * Matrix::CreateTranslation(0.0f, 1.0f, 0.0f);
        model_->CopyBoneTransformsFrom(changed);
        std::vector<Matrix> after(original.size());
        model_->CopyBoneTransformsTo(after);
        const float moved = after[0].M42 - original[0].M42;
        model_->CopyBoneTransformsFrom(original);
        std::vector<Matrix> restored(original.size());
        model_->CopyBoneTransformsTo(restored);
        roundTrip_ = Fmt(moved) + " m, then " + Fmt(restored[0].M42 - original[0].M42) + " m";
    }
    [[nodiscard]] std::string Triangles() const
    {
        if (!model_) return "-";
        int tri = 0;
        for (ModelMesh* m : model_->getMeshesProperty())
            for (int i = 0; i < m->getMeshPartsProperty().getCountProperty(); ++i) tri += m->getMeshPartsProperty()[i]->getPrimitiveCountProperty();
        return std::to_string(tri);
    }
    // What the importer recorded on the first part besides its effect: the primitive topology the index buffer holds and the sampler
    // state the glTF asked for on the base colour slot. CNA stores both on the part; the application decides whether to apply them.
    [[nodiscard]] std::string PartState() const
    {
        if (!model_ || model_->getMeshesProperty().getCountProperty() == 0 || model_->getMeshesProperty()[0]->getMeshPartsProperty().getCountProperty() == 0) return "-";
        const ModelMeshPart* part = model_->getMeshesProperty()[0]->getMeshPartsProperty()[0];
        static const char* const topology[] = {"TriangleList", "TriangleStrip", "LineList", "LineStrip", "PointList"};
        const int type = static_cast<int>(part->getPrimitiveTypeEXTProperty());
        const SamplerState& sampler = part->getSamplerStatesEXTProperty()[0];
        const auto address = [](TextureAddressMode m) { return m == TextureAddressMode::Wrap ? "Wrap" : m == TextureAddressMode::Clamp ? "Clamp" : "Mirror"; };
        const auto filter = [](TextureFilter f) { return f == TextureFilter::Point ? "Point" : f == TextureFilter::Linear ? "Linear" : f == TextureFilter::Anisotropic ? "Anisotropic" : "mixed"; };
        return std::string(type >= 0 && type < 5 ? topology[type] : "other") + " / " + filter(sampler.getFilterProperty()) + ", " + address(sampler.getAddressUProperty()) + " x " + address(sampler.getAddressVProperty());
    }
    // The quads sit side by side in the model's own bone tree; each mesh is drawn on its own so the sampler state of its part can be
    // put on the device first, which is how an application honours what the file asked for.
    void DrawWrap(const DrawContext& ctx, const Vector3& o)
    {
        GraphicsDevice& d = ctx.device;
        d.setRasterizerStateProperty(RasterizerState::CullNone);
        const Matrix world = Matrix::CreateScale(2.4f) * Matrix::CreateRotationY(R().frameYaw) * Matrix::CreateTranslation(o + Off(0, 1.35f, 0));
        std::vector<Matrix> bones(static_cast<std::size_t>(model_->getBonesProperty().getCountProperty()));
        model_->CopyAbsoluteBoneTransformsTo(bones);
        for (ModelMesh* mesh : model_->getMeshesProperty()) {
            const ModelBone* parent = mesh->getParentBoneProperty();
            const Matrix bone = parent != nullptr ? bones[static_cast<std::size_t>(parent->getIndexProperty())] : Matrix::getIdentityProperty();
            for (int i = 0; i < mesh->getMeshPartsProperty().getCountProperty(); ++i) {
                ModelMeshPart* part = mesh->getMeshPartsProperty()[i];
                if (auto* matrices = dynamic_cast<IEffectMatrices*>(part->getEffectProperty())) {
                    matrices->setWorldProperty(bone * world);
                    matrices->setViewProperty(ctx.view.view);
                    matrices->setProjectionProperty(ctx.view.projection);
                }
                if (auto* pbr = dynamic_cast<PbrEffect*>(part->getEffectProperty())) {
                    pbr->setAmbientLightColorProperty(Vector3(0.9f, 0.9f, 0.9f));
                    pbr->setEmissiveFactorProperty(Vector3::Zero);
                }
                d.getSamplerStatesProperty()[0] = applySamplers_ ? part->getSamplerStatesEXTProperty()[0] : SamplerState::LinearWrap;
            }
            try {
                mesh->Draw();
            } catch (const std::exception& e) {
                if (refusal_.empty()) Museum::Log("models", "wrap quads: drawing refused (%s)", std::string(e.what()).substr(0, 90).c_str());
                refusal_ = e.what();
            }
        }
        d.getSamplerStatesProperty()[0] = SamplerState::LinearWrap;
        gizmos_.Label(R().ToWorld(-2.9f, 0.1f, 0.0f), "REPEAT", Color(255, 230, 150, 255), 0.55f);
        gizmos_.Label(R().ToWorld(0.0f, 0.1f, 0.0f), "CLAMP_TO_EDGE", Color(255, 230, 150, 255), 0.55f);
        gizmos_.Label(R().ToWorld(2.9f, 0.1f, 0.0f), "MIRRORED_REPEAT", Color(255, 230, 150, 255), 0.55f);
    }
    [[nodiscard]] std::string SamplerOf(int mesh) const
    {
        if (!model_ || mesh >= model_->getMeshesProperty().getCountProperty() || model_->getMeshesProperty()[mesh]->getMeshPartsProperty().getCountProperty() == 0) return "-";
        const SamplerState& sampler = model_->getMeshesProperty()[mesh]->getMeshPartsProperty()[0]->getSamplerStatesEXTProperty()[0];
        const auto address = [](TextureAddressMode m) { return m == TextureAddressMode::Wrap ? "Wrap" : m == TextureAddressMode::Clamp ? "Clamp" : "Mirror"; };
        return std::string(sampler.getFilterProperty() == TextureFilter::Linear ? "Linear" : "other filter") + ", " + address(sampler.getAddressUProperty()) + " x " + address(sampler.getAddressVProperty());
    }
    [[nodiscard]] std::string EffectName() const
    {
        if (!model_ || model_->getMeshesProperty().getCountProperty() == 0 || model_->getMeshesProperty()[0]->getMeshPartsProperty().getCountProperty() == 0) return "-";
        Effect* fx = model_->getMeshesProperty()[0]->getMeshPartsProperty()[0]->getEffectProperty();
        std::string n = fx != nullptr ? fx->GetTypeName() : "none";
        const std::size_t dot = n.rfind('.');
        return dot == std::string::npos ? n : n.substr(dot + 1);
    }
    [[nodiscard]] std::string Bounds() const
    {
        if (!model_) return "-";
        const auto sphere = model_->getBoundingSphereEXTProperty();
        return sphere ? "radius " + Fmt(sphere->Radius) + " m" : "none";
    }
    [[nodiscard]] std::string Payload() const
    {
        if (!model_) return "-";
        std::string t = std::to_string(model_->getSkinsEXTProperty().size()) + " / ";
        if (dynamic_cast<SkinningData*>(model_->getTagProperty())) return t + "SkinningData";
        if (dynamic_cast<ModelAnimationsEXT*>(model_->getTagProperty())) return t + "ModelAnimationsEXT";
        return t + "none";
    }
    [[nodiscard]] std::string ReportLine(int line) const
    {
        if (!model_) return "-";
        const GltfImportReportEXT& r = model_->getGltfImportReportEXTProperty();
        switch (line) {
            case 0: return std::to_string(r.NodeCount) + " / " + std::to_string(r.DistinctMeshCount) + " / " + std::to_string(r.PrimitiveCount);
            case 1: return std::to_string(r.SkinCount) + " / " + std::to_string(r.AnimationCount) + " / " + std::to_string(r.ClipCount);
            case 2: return std::to_string(r.Diagnostics.size()) + (r.Diagnostics.empty() ? "" : ": " + r.Diagnostics.front().Code);
            default: return r.AnythingLost() ? "YES: see diagnostics" : "no";
        }
    }

    Mode mode_;
    std::unique_ptr<Texture2D> normalMap_;
    int texture_ = 0, alphaMode_ = 0;
    bool normalOn_ = true, emissiveOn_ = false;
    float uvOffset_[2] = {0.0f, 0.0f}, uvScale_ = 1.0f, uvRotation_ = 0.0f, cutoff_ = 0.5f;
    MorphTargetDataEXT* morph_ = nullptr;
    ModelMeshPart* morphPart_ = nullptr;
    std::vector<float> morphWeights_ = {0.0f, 0.0f};
    bool morphDirty_ = true, variantDirty_ = true;
    int variant_ = 0;
    ProceduralIbl ibl_;
    int env_ = 0;
    bool iblOn_ = true, iblDirty_ = true;
    float iblIntensity_ = 1.0f;
    Gizmos gizmos_;
    std::string name_;
    std::string refusal_;   // why the renderer refused to draw the model's PbrEffect parts; empty while they draw
    std::unique_ptr<Content::ContentManager> content_;
    std::unique_ptr<Model> model_;
    const AnimationClip* clip_ = nullptr;
    SkinningData* skin_ = nullptr;
    // Stops the animation and poses every skinned effect in the bind pose (what the skeleton looks like with no clip playing).
    void BindPose()
    {
        if (!model_ || !skin_) return;
        try {
            const std::size_t n = ApplyBindPoseBoneTransformsEXT(*model_, *skin_);
            play_ = false;
            bindPoseText_ = std::to_string(n) + " skinned effect(s); the animation is paused (toggle it to resume)";
        } catch (const std::exception& e) {
            bindPoseText_ = std::string(e.what()).substr(0, 60);
        }
    }
    std::string bindPoseText_ = "-", roundTrip_ = "press the button";
    std::unique_ptr<AnimationPlayer> player_;
    std::vector<Effect*> originals_;
    std::vector<std::unique_ptr<BasicEffect>> overrides_;
    float time_ = 0.0f, spin_t_ = 0.0f, spin_ = 0.5f, clock_ = 0.0f, clipLength_ = 4.0f, speed_ = 1.0f, scale_ = 1.0f;
    float metal_ = 0.8f, rough_ = 0.3f, hue_ = 0.1f, emissive_ = 0.0f;
    bool play_ = true, applySamplers_ = true, skeleton_ = true, cull_ = true, showModel_ = true, showMeshes_ = true;
    int count_ = 120, drawn_ = 0, effectChoice_ = 0;
};

}  // namespace

std::unique_ptr<Exhibit> MakeModelLab(const RoomDef& room, Services& services)
{
    return std::make_unique<ModelLab>(room, services);
}

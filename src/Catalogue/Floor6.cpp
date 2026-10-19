// Floor 6: models, animation and particles.
#include "Catalogue/CatalogueBuilder.hpp"

namespace {

WingSpec ParticleWing()
{
    WingSpec w;
    w.name = "Particle Complex";
    w.tagline = "Eleven effects from one emitter: every control changes the effect live";
    w.theme = "corridor-warm";
    w.accent = {1.0f, 0.55f, 0.25f};
    w.spineSide = Wall::West;
    const auto pa = [](const char* key, const char* title, const char* subtitle, const char* preset, bool outdoor = false) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "particle-lab";
        r.params = {{"preset", preset}};
        r.width = 13;
        r.depth = 13;
        r.height = 5.5f;
        r.theme = "cave";
        r.apis = {"DynamicVertexBuffer", "SetDataOptions.Discard", "IndexBuffer", "BlendState.Additive", "SpriteBatch"};
        (void)outdoor;
        return r;
    };
    w.rooms = {
        pa("pt-fire", "Fire", "Additive flames rising from a cone emitter, with a crackling loop", "fire"),
        pa("pt-smoke", "Smoke", "Large alpha-blended puffs growing and fading as they climb", "smoke"),
        pa("pt-sparks", "Sparks", "A fast fountain of sparks that bounce on the floor", "sparks"),
        pa("pt-snow", "Snow", "Thousands of flakes drifting down with a gentle sway", "snow"),
        pa("pt-rain", "Rain", "Fast thin streaks with gravity and a floor kill plane", "rain"),
        pa("pt-fountain", "Fountain", "An arc of water drops that splash back to the pool, with a water loop", "fountain"),
        pa("pt-dust", "Dust Motes", "A few slow specks drifting in a beam: sub-pixel size and alpha", "dust"),
        pa("pt-fireworks", "Fireworks", "Rockets that explode into sparks when their life ends", "fireworks"),
        pa("pt-bubbles", "Bubbles", "Rising translucent spheres that grow as they go", "bubbles"),
        pa("pt-leaves", "Falling Leaves", "Wide, slow, spinning leaves swaying on the way down", "leaves"),
        pa("pt-magic", "Magic Spiral", "Additive sparkles pulled into a rising spiral by a swirl force", "magic"),
    };
    return w;
}

WingSpec ModelWing()
{
    WingSpec w;
    w.name = "Model Museum";
    w.tagline = "glTF import, PbrEffect, rigid and skinned animation, instances, bounds, effects";
    w.theme = "wood-gallery";
    w.accent = {0.90f, 0.72f, 0.40f};
    w.spineSide = Wall::East;
    const auto mo = [](const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis, const char* model = "") {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "model-lab";
        r.params = {{"mode", mode}};
        if (model[0] != 0) r.params.items.emplace_back("model", model);
        r.width = 13;
        r.depth = 13;
        r.height = 5.2f;
        r.theme = "wood-gallery";
        r.apis = std::move(apis);
        r.origin = ApiOrigin::CnaExtension;
        return r;
    };
    w.rooms = {
        mo("model-load", "Loading Models", "ContentManager.Load<Model>: meshes, parts, bones, effects and bounds of nine glTF files", "info", {"ContentManager.Load<Model>", "Model", "ModelMesh", "ModelMeshPart", "Model.BoundingSphereEXT", "Model.Root", "ModelBone.Children", "Model.CopyBoneTransformsTo", "Model.CopyBoneTransformsFrom", "ModelMeshPart.PrimitiveTypeEXT", "ModelMeshPart.SamplerStatesEXT"}),
        mo("model-pbr", "Metallic and Rough", "PbrEffect factors edited live on a private copy of a model", "pbr", {"PbrEffect", "PbrEffect.MetallicFactor", "PbrEffect.RoughnessFactor", "PbrEffect.EmissiveFactor"}),
        mo("model-rigid", "Rigid Animation", "A glTF node animation applied with ApplyClipToBonesEXT on a robot arm", "rigid", {"ModelAnimationsEXT", "ApplyClipToBonesEXT", "Model.CopyAbsoluteBoneTransformsTo", "ModelBone"}),
        mo("model-skinned", "Skinned Animation", "A tentacle bending through AnimationPlayer and SkinnedPbrEffect", "skinned", {"ApplyBindPoseBoneTransformsEXT", "SkinningData", "AnimationPlayer", "SkinnedPbrEffect.SetBoneTransforms"}),
        mo("model-instances", "Many Instances", "Up to 400 copies of one model, with BoundingFrustum culling on its sphere", "instances", {"Model.Draw", "BoundingFrustum.Contains", "Model.BoundingSphereEXT"}),
        mo("model-bounds", "Bounding Volumes", "The merged sphere of a model and the sphere of every mesh", "bounds", {"ModelMesh.BoundingSphere", "Model.BoundingSphereEXT", "BoundingSphere.Transform"}, "robot"),
        mo("model-effects", "Swapping Effects", "Replacing every ModelMeshPart's effect with BasicEffect, lit or flat", "effects", {"ModelMeshPart.Effect", "BasicEffect", "PbrEffect"}),
        mo("model-report", "Import Report", "GltfImportReportEXT: counts and diagnostics for each model the importer read", "report", {"GltfImportReportEXT", "GltfImportDiagnosticEXT"}),
        mo("model-material", "PBR Material Maps", "Base colour, normal and emissive maps, a texture transform and the three alpha modes on one PbrEffect", "material", {"PbrEffect.NormalMap", "PbrEffect.EmissiveMap", "TextureTransformEXT", "AlphaModeEXT", "PbrEffect.AlphaCutoffEXT"}, "sphere"),
        [&] {
            RoomSpec r = mo("model-sampler", "Sampler States from a glTF File", "Three quads with UVs from 0 to 2: the glTF sampler (repeat, clamp, mirror) travels on each ModelMeshPart and the application puts it on the device", "wrap",
                            {"ModelMeshPart.SamplerStatesEXT", "ModelMesh.Draw", "Model.CopyAbsoluteBoneTransformsTo", "GraphicsDevice.SamplerStates", "SamplerState", "TextureAddressMode", "PbrEffect.Texture"});
            r.origin = ApiOrigin::CnaExtension;
            return r;
        }(),
        mo("model-morph", "Morph Targets", "Blend shapes from a glTF file: two targets, per-vertex deltas and a weight animation", "morph", {"MorphTargetDataEXT", "SetMorphWeightsEXT", "EvaluateMorphWeightsEXT", "ModelMeshPart.Tag"}, "morph"),
        mo("model-variants", "Material Variants", "KHR_materials_variants: one mesh, four looks switched at run time", "variants", {"Model.MaterialVariantEXT", "Model.MaterialVariantNamesEXT", "PbrEffect"}, "variants"),
        mo("model-ibl", "Image-Based Lighting", "ImageBasedLightEXT: an irradiance cube, a prefiltered specular cube and a BRDF table lighting a PBR model", "ibl", {"ImageBasedLightEXT", "PbrEffect.ImageBasedLightEXT", "TextureCube", "PbrEffect.MetallicFactor", "PbrEffect.RoughnessFactor", "GraphicsDevice.SupportsImageBasedLightingEXT"}, "sphere"),
        [&] {
            RoomSpec r = mo("model-skinfx", "SkinnedEffect", "XNA's skinning effect: a tube bent by four bone matrices, with one, two or four blend weights per vertex", "",
                            {"SkinnedEffect", "SkinnedEffect.SetBoneTransforms", "SkinnedEffect.WeightsPerVertex", "SkinnedEffect.setPunctualLightEXT", "PunctualLightEXT", "VertexPositionNormalTextureSkinned", "VertexBuffer", "IndexBuffer"});
            r.exhibit = "skinfx-lab";
            r.origin = ApiOrigin::Xna;
            return r;
        }(),
        [&] {
            RoomSpec r = mo("model-skinext", "Your Own Skinned Model", "SkinnedModelEXT: a skeleton, parts and keyframed clips built in code; ComputeBoneTransformsEXT, AttachPartEXT and RemovePartEXT", "",
                            {"SkinnedModelEXT", "SkinnedModelEXT.ComputeBoneTransformsEXT", "SkinnedModelEXT.AttachPartEXT", "SkinnedModelEXT.RemovePartEXT", "SkinnedModelEXT.AddPartEXT", "AnimationClipEXT", "BoneTrackEXT", "KeyframeEXT",
                             "SkinnedEffect.SetBoneTransforms", "VertexPositionNormalTextureSkinned"});
            r.exhibit = "skinext-lab";
            r.origin = ApiOrigin::CnaExtension;
            return r;
        }(),
        [&] {
            RoomSpec r = mo("model-cameras", "Cameras in a glTF File", "Model.CamerasEXT: three cameras a scene file places, a perspective, an orthographic and an infinite one, drawn through and as frusta", "",
                            {"Model.CamerasEXT", "ModelCameraEXT", "ModelCameraEXT.Projection", "ModelCameraEXT.WorldTransform", "ModelCameraEXT.IsPerspective", "ModelCameraEXT.HasInfiniteFarPlane", "BoundingFrustum", "Matrix.Invert"});
            r.exhibit = "modelcameras-lab";
            r.origin = ApiOrigin::CnaExtension;
            return r;
        }(),
    };
    return w;
}

WingSpec AnimationWing()
{
    WingSpec w;
    w.name = "Animation Complex";
    w.tagline = "Procedural animation: motion built from maths and matrix hierarchies";
    w.theme = "machine";
    w.accent = {0.55f, 0.85f, 0.75f};
    w.spineSide = Wall::West;
    const auto an = [](const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "anim-lab";
        r.params = {{"mode", mode}};
        r.width = 13;
        r.depth = 13;
        r.height = 5.5f;
        r.theme = "machine";
        r.apis = std::move(apis);
        return r;
    };
    w.rooms = {
        an("an-pendulum", "Pendulum Wave", "Eight pendulums whose periods step evenly, from a formula or by integration", "pendulum", {"MathHelper", "GameTime", "Vector3"}),
        an("an-clock", "A Clock", "Hour, minute and second hands as nested rotations driven by the time of day", "clock", {"Matrix.CreateRotationZ", "Matrix.Multiply"}),
        an("an-gears", "Meshing Gears", "Gear ratios as rotation matrices, with an adjustable tooth count", "gears", {"Matrix.CreateRotationY", "Matrix.Multiply", "MeshBuilder"}),
        an("an-conveyor", "Conveyor", "Scrolling texture coordinates, crates riding the belt and a swept gate counter", "conveyor", {"VertexPositionColorTexture", "SamplerState.LinearWrap", "BoundingBox"}),
        an("an-doors", "Proximity Doors", "Doors that open as you approach, with Lerp against SmoothStep easing", "doors", {"MathHelper.Lerp", "MathHelper.SmoothStep", "Vector3.Distance"}),
        an("an-kinetic", "Kinetic Sculpture", "Sixty rods whose phase depends on position: waves, twists and Lissajous figures", "kinetic", {"Matrix.CreateRotationZ", "Matrix.Multiply"}),
        an("an-gyroscope", "Gyroscope", "Three nested rings: child transforms composed with their parents", "gyroscope", {"Matrix.CreateRotationX", "Matrix.CreateRotationY", "Matrix.CreateRotationZ"}),
        an("an-walker", "A Walking Figure", "Forward kinematics: hips, thighs and shins swinging without any stored animation", "walker", {"Matrix.Multiply", "Matrix.CreateRotationZ", "MathHelper.WrapAngle"}),
    };
    return w;
}

WingSpec SimulationWing()
{
    WingSpec w;
    w.name = "Simulation Lab";
    w.tagline = "Flocking, collisions, numerical integration and inverse kinematics";
    w.theme = "machine";
    w.accent = {0.75f, 0.60f, 0.95f};
    w.spineSide = Wall::East;
    const auto sim = [](const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "sim-lab";
        r.params = {{"mode", mode}};
        r.width = 13;
        r.depth = 13;
        r.height = 5.5f;
        r.theme = "machine";
        r.apis = std::move(apis);
        return r;
    };
    w.rooms = {
        sim("sim-flock", "Flocking", "Separation, alignment and cohesion: seventy boids steering with Vector3 maths, scattering when you walk up", "flock",
            {"Vector3", "Vector3.Normalize", "Vector3.LengthSquared", "Matrix.CreateScale", "Matrix.CreateTranslation"}),
        sim("sim-balls", "Bouncing Balls", "Gravity, restitution and impulse collisions between spheres found with BoundingSphere.Intersects", "balls",
            {"BoundingSphere.Intersects", "Vector3.Dot", "BoundingBox"}),
        sim("sim-integrators", "Numerical Integrators", "Explicit Euler, semi-implicit Euler, Verlet and RK4 stepping the same spring: which keeps its energy?", "integrators",
            {"MathHelper", "Vector3", "GameTime"}),
        sim("sim-ik", "Inverse Kinematics", "A two-bone arm reaching for a target with the law of cosines, a pole direction and a reach limit", "ik",
            {"Vector3.Cross", "Vector3.Dot", "MathHelper.ToRadians", "MathHelper.ToDegrees"}),
    };
    return w;
}

}  // namespace

FloorSpec MakeFloor6()
{
    FloorSpec f;
    f.index = 6;
    f.name = "Models, Animation and Particles";
    f.tagline = "glTF models and their effects, procedural and skeletal animation, particle systems";
    f.theme = "concourse";
    f.accent = {1.0f, 0.55f, 0.25f};
    f.wings.push_back(ModelWing());
    f.wings.push_back(AnimationWing());
    f.wings.push_back(ParticleWing());
    f.wings.push_back(SimulationWing());
    return f;
}

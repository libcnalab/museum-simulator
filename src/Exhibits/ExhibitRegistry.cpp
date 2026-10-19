#include "Exhibits/ExhibitRegistry.hpp"

#include "Exhibits/ExhibitFactories.hpp"

namespace {

struct Entry {
    const char* name;
    std::unique_ptr<Exhibit> (*make)(const RoomDef&, Services&);
};

constexpr Entry kEntries[] = {
    {"lobby", MakeLobby},
    {"hub", MakeHub},
    {"spine", MakeSpine},
    {"gallery", MakeGallery},
    {"elevator", MakeElevator},
    {"stairs", MakeStairs},
    {"info", MakeInfoHall},
    {"materials", MakeMaterialsRoom},
    {"lighting", MakeLightingRoom},
    {"renderstates", MakeRenderStateRoom},
    {"rendertarget", MakeRenderTargetRoom},
    {"particles", MakeParticleRoom},
    {"models", MakeModelGallery},
    {"animation", MakeAnimationRoom},
    {"sky", MakeSkyRoom},
    {"audio", MakeAudioRoom},
    {"vectors", MakeVectorLab},
    {"transforms", MakeTransformLab},
    {"quaternions", MakeQuaternionLab},
    {"curves", MakeCurveLab},
    {"collision", MakeCollisionLab},
    {"cameras", MakeCameraLab},
    {"applab", MakeAppLab},
    {"sprites", MakeSpriteLab},
    {"text", MakeTextLab},
    {"geometry", MakeGeometryLab},
    {"textures", MakeTextureLab},
    {"shaders", MakeShaderLab},
    {"effects", MakeEffectLab},
    {"audio-lab", MakeAudioLab},
    {"input-lab", MakeInputLab},
    {"content-lab", MakeContentLab},
    {"cnaext-lab", MakeCnaExtLab},
    {"platform-lab", MakePlatformLab},
    {"players-lab", MakePlayersLab},
    {"net-lab", MakeNetLab},
    {"avatar-lab", MakeAvatarLab},
    {"pipeline-lab", MakePipelineLab},
    {"converter-lab", MakeConverterLab},
    {"video-lab", MakeVideoLab},
    {"processor-lab", MakeProcessorLab},
    {"fxb-lab", MakeCompiledFxLab},
    {"shader-errors", MakeShaderErrorLab},
    {"skinext-lab", MakeSkinExtLab},
    {"modelcameras-lab", MakeModelCamerasLab},
    {"online-lab", MakeOnlineLab},
    {"exception-lab", MakeExceptionLab},
    {"log-lab", MakeLogLab},
    {"color-gallery", MakeColorGalleryLab},
    {"fontbuild-lab", MakeFontBuildLab},
    {"meshbuild-lab", MakeMeshBuildLab},
    {"volumeshader-lab", MakeVolumeShaderLab},
    {"inspector-lab", MakeInspectorLab},
    {"phonesensor-lab", MakePhoneSensorLab},
    {"diagnostics-lab", MakeDiagnosticsLab},
    {"modelimport-lab", MakeModelImportLab},
    {"compiler-lab", MakeCompilerLab},
    {"audiopipeline-lab", MakeAudioPipelineLab},
    {"vertextypes-lab", MakeVertexTypesLab},
    {"perf-lab", MakePerfLab},
    {"validation-lab", MakeValidationLab},
    {"roof-lab", MakeRoofLab},
    {"views-lab", MakeViewsLab},
    {"query-lab", MakeQueryLab},
    {"services-lab", MakeServicesLab},
    {"shadow-lab", MakeShadowLab},
    {"device-lab", MakeDeviceLab},
    {"notification-lab", MakeNotificationLab},
    {"medialibrary-lab", MakeMediaLibraryLab},
    {"pipeline-checks", MakePipelineChecksLab},
    {"math-checks", MakeMathChecksLab},
    {"capi-lab", MakeCApiLab},
    {"history-lab", MakeHistoryLab},
    {"struct-lab", MakeStructLab},
    {"anim-lab", MakeAnimLab},
    {"model-lab", MakeModelLab},
    {"skinfx-lab", MakeSkinFxLab},
    {"reflect-lab", MakeReflectLab},
    {"terrain-lab", MakeTerrainLab},
    {"sim-lab", MakeSimLab},
    {"particle-lab", MakeParticleLab},
    {"targets", MakeTargetLab},
    {"renderstate-lab", MakeRenderStateLab},
    {"lighting-lab", MakeLightingLab},
};

}  // namespace

std::unique_ptr<Exhibit> CreateExhibit(const RoomDef& room, Services& services)
{
    for (const Entry& e : kEntries)
        if (room.exhibit == e.name) return e.make(room, services);
    return nullptr;
}

bool IsKnownExhibit(const std::string& name)
{
    if (name.empty()) return true;   // an empty themed room
    for (const Entry& e : kEntries)
        if (name == e.name) return true;
    return false;
}

std::vector<std::string> KnownExhibits()
{
    std::vector<std::string> names;
    for (const Entry& e : kEntries) names.emplace_back(e.name);
    return names;
}

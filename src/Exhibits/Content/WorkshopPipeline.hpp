#pragma once

// What the processor and material workshop rooms share: a canonical pipeline registry holding CNA's own stock components (the image
// importer and texture processor, XNA's MaterialProcessor under its XNA name, the .xnb writers), a build log that keeps every message,
// and a source folder in the build tree. A processor context that can build other assets needs a coordinator (CNA's ContentPipeline)
// behind it; that is the public route XnaBridgeProcessorContext takes. Building is in memory: nothing is published, the output
// directory only names the nested assets.
#ifdef MUSEUM_PIPELINE

#include <filesystem>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

#include "CNA/Content/Pipeline/ContentPipeline.hpp"
#include "CNA/Content/Pipeline/Texture2DContentPipeline.hpp"
#include "CNA/Content/Pipeline/TextureCompressionPipeline.hpp"
#include "CNA/Content/Pipeline/XnaModelSourceContentPipeline.hpp"
#include "CNA/Content/Pipeline/XnbOutputContentPipeline.hpp"
#include "Microsoft/Xna/Framework/TitleLocation.hpp"

namespace Workshop::Pipe {

namespace Canon = CNA::Content::Pipeline;

class RecordingLogger final : public Canon::ContentBuildLogger {
public:
    void Log(const Canon::ContentLogMessage& message) override { messages.push_back(message); }
    std::vector<Canon::ContentLogMessage> messages;
};

// Registers CNA's stock texture route, XNA's MaterialProcessor (and the "TextureProcessor" name it builds textures with), and the .xnb
// writers. A caller adds its own components before handing the registry to a ContentPipeline, which freezes it.
inline std::shared_ptr<Canon::ContentPipelineRegistry> StockRegistry()
{
    auto registry = std::make_shared<Canon::ContentPipelineRegistry>();
    Canon::RegisterTexture2DContentPipeline(*registry, Canon::MakeBlockCompressionTextureEncoder());
    Canon::RegisterXnaModelSourceContentPipeline(*registry);
    // The container options are CNA::Internal::Xnb::XnbFileOptions; the defaults (Windows, version 5, Reach, uncompressed) are passed
    // without naming the type.
    Canon::RegisterXnbOutputContentPipeline(*registry, {});
    return registry;
}

inline std::filesystem::path OutputRoot() { return std::filesystem::path(Microsoft::Xna::Framework::TitleLocation::Path()) / "WorkshopOutput"; }

// Copies the named files of Content/Workshop into a source folder in the build tree, so that a processor may also write files beside its
// source (AddOutputFile names such a file) without ever touching the museum's own source tree.
inline std::filesystem::path PrepareSourceRoot(const std::string& contentRoot, std::initializer_list<const char*> files)
{
    const std::filesystem::path root = OutputRoot() / "source";
    std::filesystem::create_directories(root);
    for (const char* name : files) std::filesystem::copy_file(std::filesystem::path(contentRoot) / "Workshop" / name, root / name, std::filesystem::copy_options::overwrite_existing);
    return std::filesystem::canonical(root);
}

}  // namespace Workshop::Pipe

#endif  // MUSEUM_PIPELINE

#include "Rendering/ModelLibrary.hpp"

#include <exception>

#include "Diagnostics/Log.hpp"

using namespace Microsoft::Xna::Framework::Graphics;

Model* ModelLibrary::Get(const std::string& name)
{
    auto it = models_.find(name);
    if (it != models_.end()) return it->second.get();
    std::unique_ptr<Model> model;
    try {
        model = std::make_unique<Model>(content_.Load<Model>("Models/" + name));
    } catch (const std::exception& e) {
        Museum::Log("models", "missing or unreadable model '%s': %s", name.c_str(), e.what());
        Museum::ReportExhibitError("models", ("missing or unreadable model '" + name + "': " + e.what()).c_str());
        ++failed_;
    }
    Model* raw = model.get();
    models_[name] = std::move(model);
    return raw;
}

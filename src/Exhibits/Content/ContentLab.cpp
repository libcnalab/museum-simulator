// Content laboratory: what ContentManager does with assets. Loading each asset type with timings, the per-manager
// cache and Unload, loose image formats, load failures that are caught and reported, the CNA content compiler's
// .cnb and .xnb outputs against loose files, TitleContainer and StorageContainer, and resource disposal.
// Nothing here is faked: every figure is measured when you press the button.
#include <algorithm>
#include <any>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <exception>
#include <fstream>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"
#include "Microsoft/Xna/Framework/Audio/SoundEffect.hpp"
#include "Microsoft/Xna/Framework/BoundingSphere.hpp"
#include "Microsoft/Xna/Framework/Content/ContentLoadException.hpp"
#include "Microsoft/Xna/Framework/Content/ContentReader.hpp"
#include "Microsoft/Xna/Framework/Content/ContentTypeReader.hpp"
#include "Microsoft/Xna/Framework/Content/ContentTypeReaderManager.hpp"
#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/Content/LooseFileContentTypeReader.hpp"
#include "Microsoft/Xna/Framework/Content/ResourceContentManager.hpp"
#include "Microsoft/Xna/Framework/Graphics/Model.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Quaternion.hpp"
#include "Microsoft/Xna/Framework/Storage/StorageContainer.hpp"
#include "Microsoft/Xna/Framework/Storage/StorageDevice.hpp"
#include "Microsoft/Xna/Framework/TitleContainer.hpp"
#include "System/IO/FileStream.hpp"
#include "System/IO/Stream.hpp"
#include "System/Resources/ResourceManager.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Audio;
using namespace Microsoft::Xna::Framework::Content;
using namespace Microsoft::Xna::Framework::Graphics;
namespace fs = std::filesystem;

namespace {

enum class Mode { Load, Cache, Formats, Failure, Pipeline, Storage, Lifetime, Custom, Resource, Reader };

Mode ParseMode(const std::string& s)
{
    static const std::pair<const char*, Mode> table[] = {{"cache", Mode::Cache},       {"formats", Mode::Formats}, {"failure", Mode::Failure}, {"pipeline", Mode::Pipeline},
                                                         {"storage", Mode::Storage}, {"lifetime", Mode::Lifetime}, {"custom", Mode::Custom}, {"resource", Mode::Resource}, {"reader", Mode::Reader}};
    for (const auto& [name, mode] : table)
        if (s == name) return mode;
    return Mode::Load;
}

using Clock = std::chrono::steady_clock;

double MillisSince(Clock::time_point t0)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

std::string Fmt(double v, const char* f = "%.2f")
{
    char buf[48];
    std::snprintf(buf, sizeof buf, f, v);
    return buf;
}

std::string Kb(std::uintmax_t bytes)
{
    return Fmt(static_cast<double>(bytes) / 1024.0, "%.1f") + " KB";
}

std::string Describe(const std::string& what)
{
    return what.size() > 96 ? what.substr(0, 93) + "..." : what;
}

/** An application type loaded through ContentManager: the content system is extensible, nothing here is built in. */
struct Recipe {
    std::string name;
    int minutes = 0;
    std::vector<std::string> steps;
};

/** A reader for loose ".recipe" files, registered with ContentManager::RegisterTypeReader<Recipe>. */
class RecipeFileReader final : public LooseFileContentTypeReader<Recipe> {
public:
    [[nodiscard]] std::vector<std::string> GetExtensions() const override { return {".recipe"}; }
    Recipe Read(const std::string& path, ContentManager&) override
    {
        std::ifstream in(path);
        if (!in) throw ContentLoadException("cannot open " + path);
        Recipe r;
        std::string line;
        while (std::getline(in, line)) {
            const std::size_t colon = line.find(':');
            if (colon == std::string::npos) continue;
            const std::string key = line.substr(0, colon), value = line.size() > colon + 2 ? line.substr(colon + 2) : std::string();
            if (key == "name") r.name = value;
            else if (key == "minutes") r.minutes = std::atoi(value.c_str());
            else if (key == "step") r.steps.push_back(value);
        }
        if (r.name.empty()) throw ContentLoadException("a .recipe file needs a name: line (" + path + ")");
        return r;
    }
};

/** A second type, loaded from a .cnj document instead: one type can have a reader or named .cnj loaders, not both. */
struct Dish : Recipe {};

/** Minimal extraction of the fields this room's .cnj document uses (a real project would use a JSON library). */
Dish RecipeFromCnj(const std::string& json, ContentManager&)
{
    Dish r;
    const auto text = [&](const std::string& key) {
        const std::size_t at = json.find("\"" + key + "\"");
        if (at == std::string::npos) return std::string();
        const std::size_t open = json.find('"', json.find(':', at) + 1), close = json.find('"', open + 1);
        return open == std::string::npos || close == std::string::npos ? std::string() : json.substr(open + 1, close - open - 1);
    };
    r.name = text("name");
    if (const std::size_t at = json.find("\"minutes\""); at != std::string::npos) r.minutes = std::atoi(json.c_str() + json.find(':', at) + 1);
    if (const std::size_t at = json.find("\"steps\""); at != std::string::npos) {
        const std::size_t end = json.find(']', at);
        for (std::size_t p = json.find('"', json.find('[', at)); p != std::string::npos && p < end; p = json.find('"', p + 1)) {
            const std::size_t close = json.find('"', p + 1);
            r.steps.push_back(json.substr(p + 1, close - p - 1));
            p = close;
        }
    }
    return r;
}

/** An application type stored in an .xnb file, and the ContentTypeReader that turns the file's bytes back into it. */
struct Menu {
    std::string name;
    int cents = 0;
    float spice = 0.0f;
    std::vector<std::string> items;
    Color accent;          // ReadColor
    Vector2 plate;         // ReadVector2: the plate's size in centimetres
    Vector4 nutrition;     // ReadVector4: protein, fat, carbohydrate, fibre in grams
    Vector3 offset;        // ReadVector3
    Quaternion rotation;   // ReadQuaternion
    Matrix transform;      // ReadMatrix
    BoundingSphere bounds; // ReadBoundingSphere
    std::string assetName; // ContentReader.AssetName: the name the asset was loaded under
    bool hasManager = false;
    int version = 0;
    char platform = '?';
};

class MenuReader final : public ContentTypeReader<Menu> {
public:
    MenuReader() : ContentTypeReader<Menu>("Museum.MenuReader") {}

protected:
    Menu Read(ContentReader& input, std::optional<Menu>) override
    {
        Menu m;
        m.name = input.ReadString();
        m.cents = input.ReadInt32();
        m.spice = input.ReadSingle();
        const int count = input.ReadInt32();
        for (int i = 0; i < count && i < 64; ++i) m.items.push_back(input.ReadString());
        m.accent = input.ReadColor();
        m.plate = input.ReadVector2();
        m.nutrition = input.ReadVector4();
        m.offset = input.ReadVector3();
        m.rotation = input.ReadQuaternion();
        m.transform = input.ReadMatrix();
        m.bounds = input.ReadBoundingSphere();
        m.assetName = input.getAssetNameProperty();
        m.hasManager = input.getContentManagerProperty() != nullptr;
        m.version = input.getVersionProperty();
        m.platform = input.getPlatformProperty();
        return m;
    }
};

/** One line of a result table; textures are kept so the screen can show what was loaded. */
struct Row {
    std::string name;
    std::string detail;
    double ms = 0.0;
    int status = 0;   // 0 ok, 1 note, 2 failure
    std::unique_ptr<Texture2D> picture;
};

class ContentLab final : public ScreenExhibit {
public:
    ContentLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 7.6f), mode_(ParseMode(room.params.Get("mode"))) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel(Title(), -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        switch (mode_) {
            case Mode::Load:
                p.AddButton("Load a Texture2D (png)", [this] { LoadTexture("Textures/brick"); });
                p.AddButton("Load a SoundEffect (wav)", [this] { LoadSound("Audio/chirp"); });
                p.AddButton("Load a Model (gltf)", [this] { LoadModel("Models/torus"); });
                p.AddButton("Load all of them again", [this] { rows_.clear(); LoadTexture("Textures/brick"); LoadTexture("Textures/marble"); LoadSound("Audio/chirp"); LoadModel("Models/torus"); LoadModel("Models/robot"); });
                p.AddButton("Clear", [this] { rows_.clear(); });
                p.AddReadout("ContentManager.RootDirectory", [this] { return manager_ != nullptr ? manager_->getRootDirectoryProperty() : std::string("-"); });
                p.AddReadout("ContentManager.ServiceProvider", [this] { return manager_ != nullptr && manager_->getServiceProviderProperty() != nullptr ? std::string("present") : std::string("null"); });
                break;
            case Mode::Cache:
                p.AddButton("Load brick (this manager)", [this] { CacheLoad(false); });
                p.AddButton("Load brick (a new manager)", [this] { CacheLoad(true); });
                p.AddButton("Unload() this manager", [this] { CacheUnload(); });
                p.AddReadout("Loads so far", [this] { return std::to_string(loads_); });
                // GraphicsResource.IsDisposed on every copy the room still holds: Unload() disposes what the manager cached.
                p.AddReadout("Held copies not IsDisposed", [this] { return std::to_string(Alive()) + " of " + std::to_string(handles_.size()); });
                p.AddReadout("Bar length", [] { return std::string("milliseconds"); });
                break;
            case Mode::Formats:
                p.AddButton("Load every format", [this] { LoadFormats(); });
                p.AddReadout("Reference", [] { return std::string("sample.png (lossless)"); });
                break;
            case Mode::Failure:
                p.AddButton("Ask for an asset that is missing", [this] { Fail(0); });
                p.AddButton("Load a PNG as a SoundEffect", [this] { Fail(1); });
                p.AddButton("Load a truncated PNG", [this] { Fail(2); });
                p.AddButton("Load an empty name", [this] { Fail(3); });
                p.AddButton("Load a text file as a Texture2D", [this] { Fail(4); });
                p.AddReadout("Caught so far", [this] { return std::to_string(rows_.size()); });
                break;
            case Mode::Pipeline:
                p.AddButton("Measure (5 cold loads each)", [this] { MeasurePipeline(); });
                p.AddReadout("Compiled content", [this] { return compiledPresent_ ? std::string("found next to the executable") : std::string("not found: build with cna-content"); });
                break;
            case Mode::Storage:
                p.AddButton("Open the storage container", [this] { OpenStorage(); });
                p.AddButton("Write a save file", [this] { WriteSave(); });
                p.AddButton("Read it back", [this] { ReadSave(); });
                p.AddButton("Delete the save file", [this] { DeleteSave(); });
                p.AddButton("CreateDirectory(\"slots/one\")", [this] { DirectoryOp(0); });
                p.AddButton("Write a file inside it", [this] { DirectoryOp(1); });
                p.AddButton("GetFileNames / GetDirectoryNames", [this] { DirectoryOp(2); });
                p.AddButton("DeleteDirectory(\"slots\")", [this] { DirectoryOp(3); });
                p.AddButton("Read a title file (OpenStream)", [this] { ReadTitleFile(); });
                p.AddButton("StorageDevice.DeleteContainer(\"MuseumLab\")", [this] { DeleteWholeContainer(); });
                p.AddReadout("Container", [this] { return container_ ? container_->getDisplayNameProperty() : std::string("not open"); });
                p.AddReadout("StorageDevice.IsConnected", [this] { return device_ ? (device_->getIsConnectedProperty() ? std::string("true") : std::string("false")) : std::string("open the device first"); });
                p.AddReadout("StorageDevice.DeviceChanged events", [this] { return std::to_string(storageChanged_) + " since this room loaded"; });
                break;
            case Mode::Custom:
                p.AddButton("Load Custom/soup (a .recipe file)", [this] { CustomLoad(0); });
                p.AddButton("Load Custom/stew (a .cnj document)", [this] { CustomLoad(1); });
                p.AddButton("Load Custom/mystery (.cnj, unknown type)", [this] { CustomLoad(2); });
                p.AddButton("Load a .recipe without registering a reader", [this] { CustomLoad(3); });
                p.AddReadout("Registered (per manager)", [this] { return std::string("Recipe: a .recipe reader; Dish: a \"Recipe\" .cnj loader"); });
                break;
            case Mode::Reader:
                p.AddButton("Load Custom/menu (reader not registered yet)", [this] { ReaderLoad(false); });
                p.AddButton("Register MenuReader, then load Custom/menu", [this] { ReaderLoad(true); });
                p.AddButton("Remove the reader again", [this] { ReaderRemove(); });
                p.AddReadout("MenuReader registered / GetTypeReader", [this] {
                    const bool registered = ContentTypeReaderManager::IsRegistered("Museum.MenuReader");
                    const bool resolved = ContentTypeReaderManager{}.GetTypeReader(System::Type::From<Menu>()) != nullptr;
                    return std::string(registered ? "yes" : "no") + " / " + (resolved ? "Menu resolved" : "not associated yet");
                });
                p.AddReadout("The file records", [] { return std::string("reader \"Museum.MenuReader\", version 0"); });
                break;
            case Mode::Resource:
                p.AddButton("Load Texture2D \"picture\" from memory", [this] { ResourceLoad(0); });
                p.AddButton("Load SoundEffect \"sound\" from memory", [this] { ResourceLoad(1); });
                p.AddButton("Load Model \"shape\" from memory", [this] { ResourceLoad(2); });
                p.AddButton("Ask for an asset the family does not hold", [this] { ResourceLoad(3); });
                p.AddButton("A manager built without a resource family", [this] { ResourceLoad(4); });
                p.AddReadout("Resource family", [this] { return resourceSummary_; });
                p.AddReadout("Lookups served by ResourceManager", [this] { return std::to_string(resourceServed_); });
                break;
            case Mode::Lifetime:
                p.AddButton("Create 64 textures", [this] { Create(64); });
                p.AddButton("Dispose every second one", [this] { DisposeHalf(); });
                p.AddButton("Dispose them all", [this] { DisposeAll(); });
                p.AddButton("Content: Load then Unload()", [this] { UnloadDemo(); });
                p.AddReadout("Device events: ResourceCreated / ResourceDestroyed", [this] { return std::to_string(deviceCreated_) + " / " + std::to_string(deviceDestroyed_); });
                p.AddReadout("Last event Resource / Name / Tag", [this] { return lastResourceEvent_; });
                p.AddReadout("Created / disposed / alive", [this] { return std::to_string(created_) + " / " + std::to_string(disposedEvents_) + " / " + std::to_string(Alive()); });
                p.AddReadout("Texture memory alive", [this] { return Kb(static_cast<std::uintmax_t>(Alive()) * kTexDim * kTexDim * 4); });
                break;
        }
        AddInfoSign(b, Title(), Explanation());
    }

    void OnScreenLoaded() override
    {
        if (mode_ == Mode::Lifetime) {
            createdToken_ = S().device->ResourceCreated.Add([this](System::Object*, const ResourceCreatedEventArgs& e) {
                ++deviceCreated_;
                lastResourceEvent_ = std::string(e.getResourceProperty() != nullptr ? "non-null" : "null") + " / - / -";
            });
            destroyedToken_ = S().device->ResourceDestroyed.Add([this](System::Object*, const ResourceDestroyedEventArgs& e) {
                ++deviceDestroyed_;
                lastResourceEvent_ = "- / " + (e.getNameProperty().empty() ? std::string("(empty)") : e.getNameProperty()) + " / " + (e.getTagProperty() != nullptr ? "non-null" : "null");
            });
            subscribed_ = true;
        }
        if (mode_ == Mode::Storage)
            storageChangedToken_ = Storage::StorageDevice::DeviceChanged.Add([this](System::Object*, const System::EventArgs&) { ++storageChanged_; });
        manager_ = NewManager(S().contentRoot);
        compiledPresent_ = fs::exists(fs::path(S().contentRoot) / "Compiled" / "cnb");
        if (mode_ == Mode::Resource) BuildResources();
        if (mode_ == Mode::Load) {
            LoadTexture("Textures/brick");
            LoadSound("Audio/chirp");
            LoadModel("Models/torus");
        } else if (mode_ == Mode::Formats) {
            LoadFormats();
        }
    }

    void OnLeave() override { ReleaseAll(); }
    ~ContentLab() override { ReleaseAll(); }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        const SamplerState& smp = SamplerState::LinearClamp;
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &smp, nullptr, nullptr);
        Texture2D& white = *S().whiteTexture;
        const auto rect = [&](float x, float y, float w, float h, Color c) { sb.Draw(white, Rectangle(static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h)), c); };
        const auto text = [&](const std::string& t, float x, float y, Color c, float s = 0.7f) { sb.DrawString(*S().font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), s, SpriteEffects::None, 0.0f); };
        text(Title(), 24, 12, Kit::kGold, 1.0f);
        const Color ok = Kit::Rgba(120, 230, 150), warn = Kit::Rgba(255, 200, 90), bad = Kit::Rgba(255, 130, 120);
        if (mode_ == Mode::Lifetime) {
            for (std::size_t i = 0; i < tex_.size(); ++i) {
                const int cx = static_cast<int>(i % 16), cy = static_cast<int>(i / 16);
                const bool dead = tex_[i].dead;
                rect(30.0f + static_cast<float>(cx) * 56.0f, 80.0f + static_cast<float>(cy) * 56.0f, 50, 50, dead ? Kit::Rgba(140, 60, 60) : Kit::Rgba(60, 160, 100));
            }
            text("green: a live Texture2D    red: IsDisposed is true (its Disposing event ran)", 30, 330, Kit::kDim, 0.65f);
            text(lifetimeNote_, 30, 370, Kit::kText, 0.7f);
        } else if (mode_ == Mode::Cache) {
            for (std::size_t i = 0; i < rows_.size() && i < 12; ++i) {
                const Row& r = rows_[rows_.size() - 1 - i];
                const float y = 66.0f + static_cast<float>(i) * 36.0f;
                const float w = static_cast<float>(std::min(600.0, r.ms * 40.0 + 4.0));
                rect(300, y, w, 26, r.status == 1 ? warn : ok);
                text(r.name, 24, y + 2, Kit::kText, 0.65f);
                text(Fmt(r.ms) + " ms  " + r.detail, 308, y + 2, Kit::Rgba(20, 20, 20), 0.6f);
            }
            if (rows_.empty()) text("Press 'Load brick' a few times, then Unload and load again.", 24, 100, Kit::kDim, 0.8f);
            text("Every texture returned stays alive here: Unload() only empties the manager's cache.", 24, 500, Kit::kDim, 0.65f);
        } else {
            float y = 64.0f;
            if (rows_.empty()) text(EmptyHint(), 24, 100, Kit::kDim, 0.8f);
            for (const Row& r : rows_) {
                const bool hasPic = r.picture != nullptr;
                const float rowH = rows_.size() > 7 ? (hasPic ? 60.0f : 40.0f) : (hasPic ? 68.0f : 44.0f);   // eight rows still fit
                if (y + rowH > 530.0f) break;
                rect(20, y, 920, rowH - 6, Kit::Rgba(24, 30, 46));
                rect(20, y, 6, rowH - 6, r.status == 0 ? ok : (r.status == 1 ? warn : bad));
                float x = 36.0f;
                if (hasPic) {
                    sb.Draw(*r.picture, Rectangle(34, static_cast<int>(y) + 2, rows_.size() > 7 ? 80 : 88, rows_.size() > 7 ? 50 : 58), Color::White);
                    x = 134.0f;
                }
                text(r.name, x, y + 4, Kit::kText, 0.75f);
                text(Describe(r.detail), x, y + 4 + 26, Kit::kDim, 0.6f);
                if (r.ms > 0.0) text(Fmt(r.ms) + " ms", 830, y + 8, r.status == 2 ? bad : ok, 0.75f);
                y += rowH;
            }
        }
        sb.End();
    }

private:
    static constexpr int kTexDim = 64;
    struct Tracked {
        std::unique_ptr<Texture2D> texture;
        bool dead = false;
    };

    [[nodiscard]] std::string Title() const
    {
        static const char* t[] = {"Loading assets", "The ContentManager cache", "Loose image formats", "When loading fails", "The content compiler", "TitleContainer and storage", "Resource lifetime", "Teaching ContentManager a new type", "Content from memory", "Content readers"};
        return t[static_cast<int>(mode_)];
    }
    [[nodiscard]] std::string EmptyHint() const
    {
        switch (mode_) {
            case Mode::Failure: return "Each button asks for something that cannot load; the exception is caught and shown here.";
            case Mode::Pipeline: return "Press 'Measure': each asset is loaded from loose files, .cnb and .xnb with a fresh ContentManager.";
            case Mode::Storage: return "Open the storage container first, then write, read and delete a save file.";
            default: return "Press a button on the control panel.";
        }
    }
    [[nodiscard]] std::vector<std::string> Explanation() const
    {
        switch (mode_) {
            case Mode::Load: return {"ContentManager.Load<T>(name) turns an asset into a Texture2D, SoundEffect, Model...: here straight from loose", "PNG, WAV and glTF files with no build step. The time is measured with a clock around the call."};
            case Mode::Cache: return {"Each ContentManager caches what it loaded: the second Load of a name returns the shared asset in microseconds.", "Unload() empties that manager's cache, so the next Load decodes again; copies you hold stay valid, other managers are separate."};
            case Mode::Formats: return {"With no content build, ContentManager decodes loose image files: PNG, JPEG, BMP, TGA, GIF and QOI. The error", "column compares each decoded picture with the PNG: only JPEG and the 256-colour GIF differ, as they should. TIFF is not supported."};
            case Mode::Failure: return {"A missing, mistyped or corrupt asset raises ContentLoadException: a game catches it and carries on. The museum", "does exactly that, shows the message here and keeps running (nothing is crashed on purpose)."};
            case Mode::Pipeline: return {"cna-content (CNA's content compiler) built the same three sources as .cnb (CNA's format) and .xnb (XNA's) when this", "museum was built. Both load through the same Load<T> call. Sizes are files on disk; times are measured live."};
            case Mode::Storage: return {"TitleContainer.OpenStream reads files shipped with the game; StorageDevice and StorageContainer keep saved data", "per user. CNA stores it under a per-user folder (CNAEXT: SetAppNameEXT and GetStorageRootEXT)."};
            case Mode::Reader: return {"An .xnb file lists the type readers it needs by name. A game that has its own content type writes a ContentTypeReader for it,", "registers it under that name (ContentTypeReaderManager::AddTypeCreator) and Load<T> reads the file's bytes with it."};
            case Mode::Resource: return {"ResourceContentManager serves assets from a resource family instead of the content folder: a game can embed its", "compiled .xnb files in the executable. Here three .xnb files are held in memory behind a System.Resources.ResourceManager."};
            case Mode::Custom: return {"ContentManager is extensible: RegisterTypeReader teaches it a loose-file format for your own type, and", "RegisterCnjLoader (CNA extension) a named .cnj document type. Load<Recipe> then works like Load<Texture2D>, failures included."};
            default: return {"GraphicsResource objects are disposed explicitly with Dispose(); IsDisposed and the Disposing event let you track", "their lifetime. A texture you copy out of ContentManager outlives Unload(), so dispose it yourself when done."};
        }
    }

    // The compiled .xnb files as byte strings, as if they were linked into the program; the ResourceManager serves them by name.
    void BuildResources()
    {
        resources_.clear();
        resourceBytes_.clear();
        std::uintmax_t bytes = 0;
        for (const char* name : {"picture", "sound", "shape"}) {
            std::ifstream in(fs::path(S().contentRoot) / "Compiled" / "xnb" / (std::string(name) + ".xnb"), std::ios::binary);
            if (!in) continue;
            std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            bytes += data.size();
            resourceBytes_[name] = std::vector<std::uint8_t>(data.begin(), data.end());
            resources_[name] = std::move(data);
        }
        resourceManager_ = std::make_unique<System::Resources::ResourceManager>(
            "Museum.Resources", [](std::string_view, std::string_view, std::string_view) -> std::optional<std::string> { return std::nullopt; },
            [this](std::string_view, std::string_view culture, std::string_view name) -> std::optional<std::any> {
                if (!culture.empty()) return std::nullopt;
                const auto it = resourceBytes_.find(std::string(name));
                if (it == resourceBytes_.end()) return std::nullopt;
                ++resourceServed_;
                return std::any(it->second);   // a binary resource: the compiled .xnb file
            });
        resourceSummary_ = resources_.empty() ? std::string("no compiled .xnb found: build with cna-content") : std::to_string(resources_.size()) + " assets, " + Kb(bytes) + " in memory";
    }

    void ResourceLoad(int what)
    {
        if (what == 4) {
            Attempt("ResourceContentManager(services).Load<Texture2D>(\"picture\")", [&](Row&) {
                ResourceContentManager bare(&S().game->getServicesProperty());
                auto t = std::make_unique<Texture2D>(bare.Load<Texture2D>("picture"));
                return std::string("loaded?!");
            }, true);
            return;
        }
        if (!resourceManager_) return;
        const std::string asset = what == 0 ? "picture" : (what == 1 ? "sound" : (what == 2 ? "shape" : "unlisted"));
        auto m = std::make_unique<ResourceContentManager>(&S().game->getServicesProperty(), resourceManager_.get());
        if (what == 0) {
            Attempt("ResourceContentManager.Load<Texture2D>(\"picture\")", [&](Row& r) {
                auto tex = std::make_unique<Texture2D>(m->Load<Texture2D>(asset));
                const std::string d = std::to_string(tex->getWidthProperty()) + " x " + std::to_string(tex->getHeightProperty()) + " pixels, from " + Kb(resources_[asset].size()) + " held in memory";
                r.picture = std::move(tex);
                return d;
            });
        } else if (what == 1) {
            Attempt("ResourceContentManager.Load<SoundEffect>(\"sound\")", [&](Row&) {
                const SoundEffect s = m->Load<SoundEffect>(asset);
                return "duration " + Fmt(s.getDurationProperty().getTotalSecondsProperty()) + " s, from " + Kb(resources_[asset].size());
            });
        } else if (what == 2) {
            Attempt("ResourceContentManager.Load<Model>(\"shape\")", [&](Row&) {
                const Model model = m->Load<Model>(asset);
                return std::to_string(model.getMeshesProperty().getCountProperty()) + " mesh(es), from " + Kb(resources_[asset].size());
            });
        } else {
            Attempt("ResourceContentManager.Load<Texture2D>(\"unlisted\")", [&](Row&) {
                auto t = std::make_unique<Texture2D>(m->Load<Texture2D>(asset));
                return std::string("loaded?!");
            }, true);
        }
        keep_.push_back(std::move(m));
        if (rows_.size() > 6) rows_.erase(rows_.begin());
    }

    std::unique_ptr<ContentManager> NewManager(const std::string& root)
    {
        return std::make_unique<ContentManager>(&S().game->getServicesProperty(), root);
    }

    void ReleaseAll()
    {
        if (subscribed_) {
            subscribed_ = false;
            try {
                S().device->ResourceCreated.Remove(createdToken_);
                S().device->ResourceDestroyed.Remove(destroyedToken_);
            } catch (...) {
            }
        }
        if (storageChangedToken_ != 0) {
            try { Storage::StorageDevice::DeviceChanged.Remove(storageChangedToken_); } catch (...) {}
            storageChangedToken_ = 0;
        }
        if (mode_ == Mode::Reader) (void)ContentTypeReaderManager::RemoveTypeCreatorEXT("Museum.MenuReader");   // the registry is process-wide
        rows_.clear();
        handles_.clear();
        tex_.clear();
        keep_.clear();
        container_.reset();
        manager_.reset();
        other_.reset();
    }

    template <typename F>
    void Attempt(const std::string& name, F&& load, bool expectFailure = false)
    {
        Row row;
        row.name = name;
        const auto t0 = Clock::now();
        try {
            row.detail = load(row);
            row.ms = MillisSince(t0);
            row.status = expectFailure ? 1 : 0;
        } catch (const std::exception& e) {
            row.ms = MillisSince(t0);
            row.status = expectFailure ? 1 : 2;
            row.detail = std::string(typeid(e).name()).find("ContentLoadException") != std::string::npos ? std::string("ContentLoadException: ") + e.what() : std::string("exception: ") + e.what();
            Museum::Log("content", "%s: %s", name.c_str(), e.what());
        }
        rows_.push_back(std::move(row));
    }

    // --- loading -----------------------------------------------------------------------------------
    void LoadTexture(const std::string& asset)
    {
        auto m = NewManager(S().contentRoot);
        Attempt("Load<Texture2D>(\"" + asset + "\")", [&](Row& r) {
            auto tex = std::make_unique<Texture2D>(m->Load<Texture2D>(asset));
            const std::string d = std::to_string(tex->getWidthProperty()) + " x " + std::to_string(tex->getHeightProperty()) + ", " + std::to_string(tex->getLevelCountProperty()) + " mip level(s)";
            r.picture = std::move(tex);
            keep_.push_back(std::move(m));
            return d;
        });
        if (rows_.size() > 6) rows_.erase(rows_.begin());
    }
    void LoadSound(const std::string& asset)
    {
        auto m = NewManager(S().contentRoot);
        Attempt("Load<SoundEffect>(\"" + asset + "\")", [&](Row&) {
            const SoundEffect s = m->Load<SoundEffect>(asset);
            return "duration " + Fmt(s.getDurationProperty().getTotalSecondsProperty()) + " s";
        });
        if (rows_.size() > 6) rows_.erase(rows_.begin());
    }
    void LoadModel(const std::string& asset)
    {
        auto m = NewManager(S().contentRoot);
        Attempt("Load<Model>(\"" + asset + "\")", [&](Row&) {
            const Model model = m->Load<Model>(asset);
            return std::to_string(model.getMeshesProperty().getCountProperty()) + " mesh(es), " + std::to_string(model.getBonesProperty().getCountProperty()) + " bone(s)";
        });
        if (rows_.size() > 6) rows_.erase(rows_.begin());
    }

    // --- cache -------------------------------------------------------------------------------------
    void CacheLoad(bool freshManager)
    {
        ContentManager* m = manager_.get();
        if (freshManager) {
            other_ = NewManager(S().contentRoot);
            m = other_.get();
        }
        Row row;
        row.name = freshManager ? "new manager" : "this manager";
        const auto t0 = Clock::now();
        try {
            handles_.push_back(std::make_unique<Texture2D>(m->Load<Texture2D>("Textures/brick")));
            row.ms = MillisSince(t0);
            ++loads_;
            row.detail = freshManager ? "(its own cache: a full load)" : (firstLoadDone_ ? "(cached)" : "(first load)");
            row.status = !freshManager && firstLoadDone_ ? 0 : 1;
            if (!freshManager) firstLoadDone_ = true;
        } catch (const std::exception& e) {
            row.detail = e.what();
            row.status = 2;
        }
        rows_.push_back(std::move(row));
        if (rows_.size() > 24) rows_.erase(rows_.begin());
    }
    void CacheUnload()
    {
        if (manager_) manager_->Unload();
        firstLoadDone_ = false;
        Row row;
        row.name = "Unload()";
        row.detail = "the cache is empty: the next Load decodes the file again";
        row.status = 1;
        rows_.push_back(std::move(row));
    }
    [[nodiscard]] int Alive() const
    {
        int n = 0;
        if (mode_ == Mode::Lifetime) {
            for (const Tracked& t : tex_) n += t.dead ? 0 : 1;
            return n;
        }
        for (const auto& h : handles_) n += h->getIsDisposedProperty() ? 0 : 1;
        return n;
    }

    // --- formats -----------------------------------------------------------------------------------
    void LoadFormats()
    {
        rows_.clear();
        std::vector<Color> reference;
        int refW = 0, refH = 0;
        for (const char* ext : {"png", "jpg", "bmp", "tga", "gif", "tif", "qoi"}) {
            const std::string asset = std::string("Formats/sample.") + ext;
            auto m = NewManager(S().contentRoot);
            const bool unsupported = std::string(ext) == "tif";   // CNA's decoder is dependency-free and has no TIFF
            Attempt(asset + (unsupported ? "   (TIFF: not supported by CNA's decoder)" : ""), [&](Row& r) {
                auto tex = std::make_unique<Texture2D>(m->Load<Texture2D>(asset));
                std::vector<Color> px(static_cast<std::size_t>(tex->getWidthProperty()) * static_cast<std::size_t>(tex->getHeightProperty()));
                tex->GetData(px.data(), static_cast<int>(px.size()));
                std::string note = std::to_string(tex->getWidthProperty()) + " x " + std::to_string(tex->getHeightProperty()) + ", file " + Kb(fs::file_size(fs::path(S().contentRoot) / asset));
                if (reference.empty()) {
                    reference = px;
                    refW = tex->getWidthProperty();
                    refH = tex->getHeightProperty();
                } else if (px.size() == reference.size() && tex->getWidthProperty() == refW && tex->getHeightProperty() == refH) {
                    double sum = 0;
                    for (std::size_t i = 0; i < px.size(); ++i)
                        sum += (std::abs(px[i].getRProperty() - reference[i].getRProperty()) + std::abs(px[i].getGProperty() - reference[i].getGProperty()) + std::abs(px[i].getBProperty() - reference[i].getBProperty())) / 3.0;
                    note += ", mean error vs PNG " + Fmt(sum / static_cast<double>(px.size())) + " / 255";
                } else {
                    note += ", different size from the PNG";
                }
                r.picture = std::move(tex);
                keep_.push_back(std::move(m));
                return note;
            }, unsupported);
        }
        // A DDS file keeps a compressed texture and its whole mip chain: ContentManager does not load it, the CNAEXT stream reader does.
        Attempt("Formats/sample.dds via Texture2D.DDSFromStreamEXT (DXT1, a mip chain)", [&](Row& r) {
            System::IO::FileStream stream((fs::path(S().contentRoot) / "Formats/sample.dds").string(), System::IO::FileMode::Open, System::IO::FileAccess::Read);
            auto tex = std::make_unique<Texture2D>(Texture2D::DDSFromStreamEXT(*S().device, stream));
            const SurfaceFormat f = tex->getFormatProperty();
            std::string note = std::to_string(tex->getWidthProperty()) + " x " + std::to_string(tex->getHeightProperty()) + ", " + (f == SurfaceFormat::Dxt1 ? "Dxt1" : f == SurfaceFormat::Color ? "Color (decompressed)" : "format " + std::to_string(static_cast<int>(f))) +
                               ", " + std::to_string(tex->getLevelCountProperty()) + " mip levels, file " + Kb(fs::file_size(fs::path(S().contentRoot) / "Formats/sample.dds"));
            r.picture = std::move(tex);
            return note;
        });
    }

    // --- failures ----------------------------------------------------------------------------------
    void Fail(int which)
    {
        auto m = NewManager(S().contentRoot);
        switch (which) {
            case 0: Attempt("Load<Texture2D>(\"Textures/does-not-exist\")", [&](Row&) { (void)m->Load<Texture2D>("Textures/does-not-exist"); return std::string("unexpectedly loaded"); }, true); break;
            case 1: Attempt("Load<SoundEffect>(\"Textures/brick\")", [&](Row&) { (void)m->Load<SoundEffect>("Textures/brick"); return std::string("unexpectedly loaded"); }, true); break;
            case 2: Attempt("Load<Texture2D>(\"Formats/broken\")  (half a PNG)", [&](Row&) { (void)m->Load<Texture2D>("Formats/broken"); return std::string("unexpectedly loaded"); }, true); break;
            case 3: Attempt("Load<Texture2D>(\"\")", [&](Row&) { (void)m->Load<Texture2D>(""); return std::string("unexpectedly loaded"); }, true); break;
            default: Attempt("Load<Texture2D>(\"Formats/notes\")  (a .txt file)", [&](Row&) { (void)m->Load<Texture2D>("Formats/notes"); return std::string("unexpectedly loaded"); }, true); break;
        }
        if (rows_.size() > 8) rows_.erase(rows_.begin());
    }

    // --- pipeline ----------------------------------------------------------------------------------
    void MeasurePipeline()
    {
        rows_.clear();
        const fs::path root(S().contentRoot);
        struct Asset { const char* name; const char* ext; int kind; };
        const Asset assets[] = {{"picture", "png", 0}, {"sound", "wav", 1}, {"shape", "gltf", 2}};
        const char* labels[3] = {"loose", "cnb", "xnb"};
        const std::string roots[3] = {(root / "Pipeline").string(), (root / "Compiled" / "cnb").string(), (root / "Compiled" / "xnb").string()};
        const char* exts[3] = {nullptr, "cnb", "xnb"};
        for (const Asset& a : assets) {
            for (int f = 0; f < 3; ++f) {
                const fs::path file = f == 0 ? fs::path(roots[0]) / (std::string(a.name) + "." + a.ext) : fs::path(roots[f]) / (std::string(a.name) + "." + exts[f]);
                Row row;
                row.name = std::string(a.name) + "  (" + labels[f] + ")";
                if (!fs::exists(file)) {
                    row.detail = "missing: " + file.filename().string();
                    row.status = 2;
                    rows_.push_back(std::move(row));
                    continue;
                }
                const auto size = fs::file_size(file);
                double total = 0.0;
                int done = 0;
                std::string error;
                for (int i = 0; i < 5; ++i) {
                    try {
                        auto m = NewManager(roots[f]);
                        const auto t0 = Clock::now();
                        if (a.kind == 0) {
                            const Texture2D t = m->Load<Texture2D>(a.name);
                            (void)t;
                        } else if (a.kind == 1) {
                            const SoundEffect s = m->Load<SoundEffect>(a.name);
                            (void)s;
                        } else {
                            const Model md = m->Load<Model>(a.name);
                            (void)md;
                        }
                        total += MillisSince(t0);
                        ++done;
                    } catch (const std::exception& e) {
                        error = e.what();
                        break;
                    }
                }
                row.ms = done > 0 ? total / done : 0.0;
                row.detail = error.empty() ? "file " + Kb(size) + ", mean of " + std::to_string(done) + " cold loads" : "file " + Kb(size) + ", failed: " + error;
                row.status = error.empty() ? (f == 0 ? 1 : 0) : 2;
                rows_.push_back(std::move(row));
            }
        }
        try {
            ContentManager m(&S().game->getServicesProperty(), roots[0]);
            const auto& manifest = m.GetContentManifest();
            Row row;
            row.name = "GetContentManifest() on Pipeline/";
            row.detail = std::to_string(manifest.size()) + " asset(s) found under the root";
            row.status = 1;
            rows_.push_back(std::move(row));
        } catch (const std::exception& e) {
            Museum::Log("content", "manifest: %s", e.what());
        }
    }

    // --- storage -----------------------------------------------------------------------------------
    void OpenStorage()
    {
        rows_.clear();
        Row row;
        row.name = "StorageDevice";
        try {
            Storage::StorageDevice::SetAppNameEXT("cna-3d-museum");
            std::unique_ptr<System::IAsyncResult> sel = Storage::StorageDevice::BeginShowSelector(nullptr, nullptr);
            device_ = Storage::StorageDevice::EndShowSelector(sel.get());
            std::unique_ptr<System::IAsyncResult> open = device_->BeginOpenContainer("MuseumLab", nullptr, nullptr);
            container_ = device_->EndOpenContainer(open.get());
            row.detail = "root " + Storage::StorageDevice::GetStorageRootEXT() + "  connected " + (device_->getIsConnectedProperty() ? "yes" : "no") + "  free " + Fmt(static_cast<double>(device_->getFreeSpaceProperty()) / 1.0e9, "%.1f") + " of " + Fmt(static_cast<double>(device_->getTotalSpaceProperty()) / 1.0e9, "%.1f") + " GB";
            row.status = 0;
        } catch (const std::exception& e) {
            row.detail = std::string("storage unavailable: ") + e.what();
            row.status = 2;
        }
        rows_.push_back(std::move(row));
    }
    void WriteSave()
    {
        Row row;
        row.name = "CreateFile(\"lab.sav\")";
        if (!container_) { row.detail = "open the container first"; row.status = 2; rows_.push_back(std::move(row)); return; }
        try {
            ++saveCounter_;
            const std::string body = "museum-lab save\nwrites=" + std::to_string(saveCounter_) + "\n";
            auto stream = container_->CreateFile("lab.sav");
            std::vector<SharpRuntime::bytecs> bytes(body.begin(), body.end());
            stream->Write(bytes.data(), 0, static_cast<int>(bytes.size()));
            stream->Close();
            row.detail = "wrote " + std::to_string(body.size()) + " bytes";
        } catch (const std::exception& e) {
            row.detail = e.what();
            row.status = 2;
        }
        rows_.push_back(std::move(row));
    }
    void ReadSave()
    {
        Row row;
        row.name = "OpenFile(\"lab.sav\", Open)";
        if (!container_) { row.detail = "open the container first"; row.status = 2; rows_.push_back(std::move(row)); return; }
        try {
            if (!container_->FileExists("lab.sav")) {
                row.detail = "FileExists is false: nothing saved yet";
                row.status = 1;
            } else {
                auto stream = container_->OpenFile("lab.sav", System::IO::FileMode::Open);
                std::vector<SharpRuntime::bytecs> bytes(static_cast<std::size_t>(stream->getLengthProperty()));
                const int n = stream->Read(bytes.data(), 0, static_cast<int>(bytes.size()));
                stream->Close();
                std::string s(bytes.begin(), bytes.begin() + n);
                for (char& c : s) if (c == '\n') c = ' ';
                row.detail = "read " + std::to_string(n) + " bytes: " + s;
            }
        } catch (const std::exception& e) {
            row.detail = e.what();
            row.status = 2;
        }
        rows_.push_back(std::move(row));
    }
    // The container is a small file system of its own: directories, listings with a search pattern, and deletion.
    void DirectoryOp(int op)
    {
        Row row;
        static const char* names[] = {"CreateDirectory(\"slots/one\")", "CreateFile(\"slots/one/a.sav\")", "GetFileNames / GetDirectoryNames", "DeleteDirectory(\"slots\")"};
        row.name = names[op];
        if (!container_) { row.detail = "open the container first"; row.status = 2; rows_.push_back(std::move(row)); return; }
        try {
            switch (op) {
                case 0:
                    container_->CreateDirectory("slots/one");
                    row.detail = std::string("DirectoryExists: ") + (container_->DirectoryExists("slots/one") ? "true" : "false");
                    break;
                case 1: {
                    if (!container_->DirectoryExists("slots/one")) { row.detail = "create the directory first"; row.status = 1; break; }
                    auto stream = container_->CreateFile("slots/one/a.sav");
                    const std::string body = "slot one";
                    std::vector<SharpRuntime::bytecs> bytes(body.begin(), body.end());
                    stream->Write(bytes.data(), 0, static_cast<int>(bytes.size()));
                    stream->Close();
                    row.detail = "wrote " + std::to_string(body.size()) + " bytes";
                    break;
                }
                case 2: {
                    std::string text;
                    for (const std::string& n : container_->GetDirectoryNames()) text += "dir " + n + "  ";
                    for (const std::string& n : container_->GetFileNames()) text += "file " + n + "  ";
                    for (const std::string& n : container_->GetFileNames("*.sav")) text += "*.sav: " + n + "  ";
                    text += std::string("FileExists(slots/one/a.sav): ") + (container_->FileExists("slots/one/a.sav") ? "true" : "false");
                    row.detail = text.empty() ? "(empty)" : text;
                    break;
                }
                default:
                    if (container_->DirectoryExists("slots")) {
                        // A directory must be empty before it can go (as System.IO.Directory.Delete without recursion): the first try says so, then it is emptied from the bottom up.
                        std::string said;
                        try {
                            container_->DeleteDirectory("slots");
                        } catch (const std::exception& e) {
                            said = "refused while it has contents; ";
                        }
                        if (container_->FileExists("slots/one/a.sav")) container_->DeleteFile("slots/one/a.sav");
                        if (container_->DirectoryExists("slots/one")) container_->DeleteDirectory("slots/one");
                        if (container_->DirectoryExists("slots")) container_->DeleteDirectory("slots");
                        row.detail = said + "emptied from the bottom up; DirectoryExists now " + (container_->DirectoryExists("slots") ? "true" : "false");
                    } else {
                        row.detail = "nothing to delete";
                        row.status = 1;
                    }
            }
        } catch (const std::exception& e) {
            row.detail = e.what();
            row.status = 2;
        }
        rows_.push_back(std::move(row));
    }
    // Removes the whole container (every file and folder in it) through the device; the open handle is dropped first.
    void DeleteWholeContainer()
    {
        Row row;
        row.name = "DeleteContainer(\"MuseumLab\")";
        try {
            if (!device_) { row.detail = "open the container first"; row.status = 1; rows_.push_back(std::move(row)); return; }
            container_.reset();
            device_->DeleteContainer("MuseumLab");
            row.detail = "deleted with everything in it; open the container again to start over";
        } catch (const std::exception& e) {
            row.detail = e.what();
            row.status = 2;
        }
        rows_.push_back(std::move(row));
    }
    void DeleteSave()
    {
        Row row;
        row.name = "DeleteFile(\"lab.sav\")";
        try {
            if (container_ && container_->FileExists("lab.sav")) {
                container_->DeleteFile("lab.sav");
                row.detail = "deleted";
            } else {
                row.detail = "nothing to delete";
                row.status = 1;
            }
        } catch (const std::exception& e) {
            row.detail = e.what();
            row.status = 2;
        }
        rows_.push_back(std::move(row));
    }
    void ReadTitleFile()
    {
        Row row;
        row.name = "TitleContainer.OpenStream(\"" + std::string("Content/Formats/notes.txt") + "\")";
        try {
            auto stream = TitleContainer::OpenStream("Content/Formats/notes.txt");
            std::vector<SharpRuntime::bytecs> bytes(static_cast<std::size_t>(stream->getLengthProperty()));
            const int n = stream->Read(bytes.data(), 0, static_cast<int>(bytes.size()));
            stream->Close();
            std::string s(bytes.begin(), bytes.begin() + n);
            for (char& c : s) if (c == '\n') c = ' ';
            row.detail = s;
        } catch (const std::exception& e) {
            row.detail = e.what();
            row.status = 2;
        }
        rows_.push_back(std::move(row));
        if (rows_.size() > 8) rows_.erase(rows_.begin());
    }

    // --- custom types ------------------------------------------------------------------------------
    void CustomLoad(int which)
    {
        auto m = NewManager(S().contentRoot);
        const char* assets[] = {"Custom/soup", "Custom/stew", "Custom/mystery", "Custom/soup"};
        Attempt(std::string("Load<") + (which == 1 || which == 2 ? "Dish" : "Recipe") + ">(\"" + assets[which] + "\")", [&](Row&) {
            if (which != 3) {
                m->RegisterTypeReader<Recipe>(std::make_unique<RecipeFileReader>());
                m->RegisterCnjLoader<Dish>("Recipe", &RecipeFromCnj);
            }
            const Recipe r = which == 1 || which == 2 ? static_cast<Recipe>(m->Load<Dish>(assets[which])) : m->Load<Recipe>(assets[which]);
            std::string steps;
            for (const std::string& st : r.steps) steps += st + "; ";
            return r.name + ", " + std::to_string(r.minutes) + " min: " + steps;
        }, which >= 2);
        if (rows_.size() > 6) rows_.erase(rows_.begin());
    }

    // --- content readers -----------------------------------------------------------------------------
    void ReaderLoad(bool registerFirst)
    {
        if (registerFirst && !ContentTypeReaderManager::IsRegistered("Museum.MenuReader"))
            ContentTypeReaderManager::AddTypeCreator("Museum.MenuReader", [] { return std::make_unique<MenuReader>(); });
        auto m = NewManager(S().contentRoot);
        Attempt("Load<Menu>(\"Custom/menu\")", [&](Row&) {
            const Menu menu = m->Load<Menu>("Custom/menu");
            std::string items;
            for (const std::string& it : menu.items) items += it + ", ";
            return "\"" + menu.name + "\": " + std::to_string(menu.cents / 100) + "." + (menu.cents % 100 < 10 ? "0" : "") + std::to_string(menu.cents % 100) + ", spice " + Fmt(menu.spice) + ", " + items;
        }, !registerFirst && !ContentTypeReaderManager::IsRegistered("Museum.MenuReader"));
        if (ContentTypeReaderManager::IsRegistered("Museum.MenuReader")) {
            // The rest of what the reader pulled out of the same file, through the other ContentReader calls.
            Attempt("ReadColor, ReadVector2/3/4, Quaternion", [&](Row&) {
                const Menu menu = m->Load<Menu>("Custom/menu");
                return "colour " + std::to_string(menu.accent.getRProperty()) + "," + std::to_string(menu.accent.getGProperty()) + "," + std::to_string(menu.accent.getBProperty()) + "  Vector2 " + Fmt(menu.plate.X, "%.0f") + "x" + Fmt(menu.plate.Y, "%.0f") + "  Vector4 " + Fmt(menu.nutrition.X, "%.0f") + "/" +
                       Fmt(menu.nutrition.Y, "%.0f") + "/" + Fmt(menu.nutrition.Z, "%.0f") + "/" + Fmt(menu.nutrition.W, "%.0f") + "  Vector3 " + Fmt(menu.offset.X, "%.0f") + "," + Fmt(menu.offset.Y, "%.0f") + "," + Fmt(menu.offset.Z, "%.0f") + "  Quaternion.W " + Fmt(menu.rotation.W);
            });
            Attempt("ReadMatrix, ReadBoundingSphere, reader context", [&](Row&) {
                const Menu menu = m->Load<Menu>("Custom/menu");
                return "translation " + Fmt(menu.transform.M41, "%.0f") + "," + Fmt(menu.transform.M42, "%.0f") + "," + Fmt(menu.transform.M43, "%.0f") + "  sphere r " + Fmt(menu.bounds.Radius) + "  asset " + menu.assetName + "  manager " + (menu.hasManager ? "yes" : "no") + "  XNB v" + std::to_string(menu.version) + " platform '" + std::string(1, menu.platform) + "'";
            });
        }
        if (rows_.size() > 6) rows_.erase(rows_.begin());
    }
    void ReaderRemove()
    {
        const bool removed = ContentTypeReaderManager::RemoveTypeCreatorEXT("Museum.MenuReader");
        Row row;
        row.name = "RemoveTypeCreatorEXT(\"Museum.MenuReader\")";
        row.detail = removed ? "removed: the next load fails again" : "it was not registered";
        rows_.push_back(std::move(row));
        if (rows_.size() > 6) rows_.erase(rows_.begin());
    }

    // --- lifetime ----------------------------------------------------------------------------------
    void Create(int n)
    {
        for (int i = 0; i < n && tex_.size() < 128; ++i) {
            Tracked t;
            t.texture = std::make_unique<Texture2D>(*S().device, kTexDim, kTexDim);
            const std::size_t index = tex_.size();
            t.texture->setNameProperty("lifetime texture " + std::to_string(index));
            t.texture->Disposing += [this, index](System::Object*, const System::EventArgs&) {
                if (index < tex_.size()) tex_[index].dead = true;
                ++disposedEvents_;
            };
            tex_.push_back(std::move(t));
            ++created_;
        }
        lifetimeNote_ = "created " + std::to_string(tex_.size()) + " textures of " + std::to_string(kTexDim) + " x " + std::to_string(kTexDim);
    }
    void DisposeHalf()
    {
        for (std::size_t i = 0; i < tex_.size(); i += 2)
            if (!tex_[i].dead) tex_[i].texture->Dispose();
        lifetimeNote_ = "Dispose() on every second texture";
    }
    void DisposeAll()
    {
        for (Tracked& t : tex_)
            if (!t.dead) t.texture->Dispose();
        lifetimeNote_ = "Dispose() on all of them: memory is returned immediately";
    }
    void UnloadDemo()
    {
        try {
            ContentManager m(&S().game->getServicesProperty(), S().contentRoot);
            Texture2D t = m.Load<Texture2D>("Textures/brick");
            const bool before = t.getIsDisposedProperty();
            m.Unload();
            lifetimeNote_ = std::string("Load then Unload(): IsDisposed was ") + (before ? "true" : "false") + " before and " + (t.getIsDisposedProperty() ? "true" : "false") + " after: the copy you hold survives the manager's cache";
        } catch (const std::exception& e) {
            lifetimeNote_ = std::string("failed: ") + e.what();
        }
    }

    Mode mode_;
    std::size_t createdToken_ = 0, destroyedToken_ = 0;
    System::EventHandler<System::EventArgs>::Token storageChangedToken_ = 0;
    bool subscribed_ = false;
    int deviceCreated_ = 0, deviceDestroyed_ = 0, storageChanged_ = 0;
    std::unique_ptr<ContentManager> manager_, other_;
    std::unique_ptr<System::Resources::ResourceManager> resourceManager_;
    std::map<std::string, std::string> resources_;
    std::map<std::string, std::vector<std::uint8_t>> resourceBytes_;
    std::string resourceSummary_ = "-";
    int resourceServed_ = 0;
    std::vector<std::unique_ptr<ContentManager>> keep_;   // managers whose textures are on screen
    std::vector<Row> rows_;
    std::vector<std::unique_ptr<Texture2D>> handles_;
    std::vector<Tracked> tex_;
    std::unique_ptr<Storage::StorageDevice> device_;
    std::unique_ptr<Storage::StorageContainer> container_;
    int loads_ = 0, created_ = 0, disposedEvents_ = 0, saveCounter_ = 0;
    bool firstLoadDone_ = false, compiledPresent_ = false;
    std::string lifetimeNote_ = "press 'Create 64 textures'";
    std::string lastResourceEvent_ = "- / - / -";
};

}  // namespace

std::unique_ptr<Exhibit> MakeContentLab(const RoomDef& room, Services& services)
{
    return std::make_unique<ContentLab>(room, services);
}

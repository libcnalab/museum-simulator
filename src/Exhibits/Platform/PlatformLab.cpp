// Platform laboratory: what CNA reports about the window, the display adapter, the active graphics renderer and
// the platform. These are queries, not simulations: the tables show this machine and this renderer, and change
// when the museum is started with another CNA_GRAPHICS_RENDERER.
#include <algorithm>
#include <cstdio>
#include <exception>
#include <string>
#include <vector>

#include "CNA/GraphicsRendererMaturity.hpp"
#include "CNA/GraphicsRendererType.hpp"
#include "CNA/Platform/IPlatform.hpp"
#include "CNA/Platform/NativeWindowHandle.hpp"
#include "CNA/Platform/NativeWindowSystem.hpp"
#include "CNA/Platform/PlatformCapabilities.hpp"
#include "CNA/RendererCapabilityProfile.hpp"
#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"
#ifdef MUSEUM_PHONE
#include "Microsoft/Phone/Shell/PhoneApplicationService.hpp"
#endif
#include "Microsoft/Xna/Framework/DisplayOrientation.hpp"
#include "Microsoft/Xna/Framework/GameWindow.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"
#include "Microsoft/Xna/Framework/Graphics/DisplayMode.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsAdapter.hpp"
#include "Microsoft/Xna/Framework/Graphics/PresentationParameters.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

#ifdef MUSEUM_PHONE
// What PhoneApplicationService.State stores: any System::Object, here one that just names itself.
struct StateEntry final : System::Object {
    [[nodiscard]] const std::string& GetTypeName() const override
    {
        static const std::string name = "Museum.StateEntry";
        return name;
    }
};
#endif

enum class Mode { Window, Adapter, Renderer, Platform };

Mode ParseMode(const std::string& s)
{
    static const std::pair<const char*, Mode> table[] = {{"adapter", Mode::Adapter}, {"renderer", Mode::Renderer}, {"platform", Mode::Platform}};
    for (const auto& [name, mode] : table)
        if (s == name) return mode;
    return Mode::Window;
}

std::string Hex(long long v)
{
    char buf[24];
    std::snprintf(buf, sizeof buf, "%04llX", static_cast<unsigned long long>(v));
    return buf;
}

std::string Num(double v, const char* f = "%.0f")
{
    char buf[48];
    std::snprintf(buf, sizeof buf, f, v);
    return buf;
}

const char* FeatureName(CNA::RendererFeature f)
{
    using F = CNA::RendererFeature;
    switch (f) {
        case F::ThreeDimensionalPipeline: return "3D pipeline";
        case F::DepthStencilBuffer: return "depth/stencil buffer";
        case F::MultiSampleAntiAliasing: return "multisample AA";
        case F::MultipleRenderTargets: return "multiple render targets";
        case F::AnisotropicFiltering: return "anisotropic filtering";
        case F::WireFrameRasterization: return "wireframe fill";
        case F::OcclusionQueries: return "occlusion queries";
        case F::ShaderEffects: return "shader effects";
        case F::ShaderEffectSourceExecution: return "shader source execution";
        case F::Texture3DStorage: return "3D texture storage";
        case F::MultiStreamVertexInput: return "multi-stream vertices";
        case F::InstancedDrawing: return "instanced drawing";
        case F::StencilBuffer: return "stencil buffer";
        case F::AdditiveBlending: return "additive blending";
        case F::CompiledXnaEffects: return "compiled XNA effects";
        case F::Float32RenderTargets: return "float32 render targets";
        case F::Float16RenderTargets: return "float16 render targets";
        case F::Float16TextureLinearFiltering: return "float16 linear filter";
        case F::ShadowSampling: return "shadow sampling";
        case F::ImageBasedLighting: return "image-based lighting";
        case F::ShaderDialectGlslDesktop: return "GLSL desktop";
        case F::ShaderDialectGlslEs: return "GLSL ES";
        case F::ShaderDialectGlslVulkan: return "GLSL (Vulkan)";
        case F::ShaderDialectHlsl: return "HLSL";
        case F::ShaderDialectMsl: return "MSL";
        case F::ShaderDialectWgsl: return "WGSL";
        case F::Texture3DSampling: return "3D texture sampling";
        case F::BaseInstanceDrawing: return "base-instance drawing";
        default: return "?";
    }
}

class PlatformLab final : public ScreenExhibit {
public:
    PlatformLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 7.6f), mode_(ParseMode(room.params.Get("mode"))) {}

    ~PlatformLab() override { RestoreGlobals(); }

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel(Title(), -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        switch (mode_) {
            case Mode::Window:
                p.AddChoice("Window title", {"CNA 3D Museum", "Hello from GameWindow", "Title set by the Window room"}, 0, [this](int v) { SetTitle(v); });
                p.AddToggle("AllowUserResizing", S().game->getWindowProperty().getAllowUserResizingProperty(), [this](bool v) { SetResizable(v); });
                p.AddReadout("ClientSizeChanged events", [this] { return std::to_string(resizeEvents_); });
                p.AddReadout("Orientation / events", [this] { return OrientationText(); });
                p.AddReadout("Activated / Deactivated", [this] { return std::to_string(activated_) + " / " + std::to_string(deactivated_); });
                p.AddReadout("Files dropped on the window", [this] { return std::to_string(drops_); });
#ifdef MUSEUM_PHONE
                p.AddReadout("Shell: Launch/Act/Deact/Close", [this] { return std::to_string(shell_[0]) + " / " + std::to_string(shell_[1]) + " / " + std::to_string(shell_[2]) + " / " + std::to_string(shell_[3]); });
                p.AddButton("PhoneApplicationService.State: add an entry", [this] {
                    auto& state = Microsoft::Phone::Shell::PhoneApplicationService::getCurrentProperty().getStateProperty();
                    state.Add("visit " + std::to_string(state.getCountProperty() + 1), std::make_shared<StateEntry>());
                });
                p.AddReadout("PhoneApplicationService.State entries", [] { return std::to_string(Microsoft::Phone::Shell::PhoneApplicationService::getCurrentProperty().getStateProperty().getCountProperty()); });
#endif
                break;
            case Mode::Adapter:
                p.AddReadout("Adapter", [this] { return adapterName_; });
                p.AddReadout("Display modes", [this] { return std::to_string(modes_.size()); });
                p.AddReadout("Profile", [this] { return profileName_; });
                p.AddChoice("UseNullDevice / UseReferenceDevice (asked, never kept)", {"neither", "UseReferenceDevice", "UseNullDevice", "both"}, 0, [this](int v) { deviceFlags_ = v; Museum::Log("platform", "device flags %d: type %s, required renderer %s", v, DeviceTypeText(false).c_str(), DeviceTypeText(true).c_str()); });
                p.AddReadout("GetCurrentDeviceTypeEXT", [this] { return DeviceTypeText(false); });
                p.AddReadout("GetRequiredRendererEXT", [this] { return DeviceTypeText(true); });
                p.AddChoice("Is this shape widescreen? (IsWideScreenAspectRatioEXT)", {"4:3", "5:4", "16:10", "16:9", "21:9"}, 3, [this](int v) { aspectIndex_ = v; });
                p.AddReadout("IsWideScreenAspectRatioEXT", [this] {
                    static const float ratios[] = {4.0f / 3.0f, 5.0f / 4.0f, 16.0f / 10.0f, 16.0f / 9.0f, 21.0f / 9.0f};
                    const float r = ratios[std::clamp(aspectIndex_, 0, 4)];
                    return std::string(GraphicsAdapter::IsWideScreenAspectRatioEXT(r) ? "widescreen" : "not widescreen");
                });
                break;
            case Mode::Renderer:
                p.AddReadout("Active renderer", [this] { return std::string(S().device->GetGraphicsRendererName()); });
                p.AddReadout("Maturity", [this] { return maturity_; });
                p.AddChoice("Table", {"Features", "Limits and formats", "Device queries", "Report text"}, 0, [this](int v) { page_ = v; });
                p.AddSlider("Report text: scroll (GetRendererCapabilityReportEXT)", 0.0f, 1.0f, 0.0f, [this](float v) { reportScroll_ = v; }, "%.2f");
                break;
            case Mode::Platform:
                p.AddReadout("Platform", [this] { return platformName_; });
                p.AddReadout("Capabilities", [this] { return std::to_string(supported_) + " of " + std::to_string(total_) + " supported"; });
                p.AddReadout("Clock (GetPlatformEXT)", [this] { return clock_; });
                p.AddReadout("Subsystems up", [this] { return subsystems_; });
                p.AddReadout("Device interfaces", [this] { return devices_; });
                break;
        }
        AddInfoSign(b, Title(), Explanation());
    }

    void OnScreenLoaded() override
    {
        // Marked subscribed before the first Add: if a later Add throws, RestoreGlobals still removes the handlers
        // already added (each captures this room), instead of leaving them on the game after the room is gone.
        subscribed_ = true;
        try {
            Game& g = *S().game;
            originalTitle_ = g.getWindowProperty().getTitleProperty();
            originalResizable_ = g.getWindowProperty().getAllowUserResizingProperty();
            resizeToken_ = g.getWindowProperty().ClientSizeChanged.Add([this](System::Object*, const System::EventArgs&) { ++resizeEvents_; });
            orientationToken_ = g.getWindowProperty().OrientationChanged.Add([this](System::Object*, const System::EventArgs&) { ++orientationEvents_; });
            // Logged too, so a run with a window manager (tools/window_check.sh) can see focus moving to another window and back.
            activeToken_ = g.Activated.Add([this](System::Object*, const System::EventArgs&) { ++activated_; Museum::Log("window", "Game.Activated (%d)", activated_); });
            inactiveToken_ = g.Deactivated.Add([this](System::Object*, const System::EventArgs&) { ++deactivated_; Museum::Log("window", "Game.Deactivated (%d)", deactivated_); });
            dropToken_ = g.getWindowProperty().FileDropEXT.Add([this](System::Object*, const FileDropEventArgsEXT& e) {
                ++drops_;
                std::string names;
                for (const auto& file : e.getFilesProperty()) names += (names.empty() ? "" : ", ") + file;
                Museum::Log("window", "FileDropEXT: %zu file(s): %s", e.getFilesProperty().size(), names.c_str());
            });
#ifdef MUSEUM_PHONE
            // Microsoft.Phone.Shell maps the game's window events onto a phone application's lifetime: subscribe first, then attach, so
            // Launching (raised by the attach itself) is counted.
            auto& shell = Microsoft::Phone::Shell::PhoneApplicationService::getCurrentProperty();
            shellTokens_[0] = shell.Launching.Add([this](System::Object*, const Microsoft::Phone::Shell::LaunchingEventArgs&) { ++shell_[0]; });
            shellTokens_[1] = shell.Activated.Add([this](System::Object*, const Microsoft::Phone::Shell::ActivatedEventArgs&) { ++shell_[1]; });
            shellTokens_[2] = shell.Deactivated.Add([this](System::Object*, const Microsoft::Phone::Shell::DeactivatedEventArgs&) { ++shell_[2]; });
            shellTokens_[3] = shell.Closing.Add([this](System::Object*, const Microsoft::Phone::Shell::ClosingEventArgs&) { ++shell_[3]; });
            shell.AttachEXT(g);
#endif
        } catch (const std::exception& e) {
            ReportError(std::string("subscribing to the window and game events: ") + e.what());
        }
        Refresh();
    }

    void Update(const UpdateContext& ctx) override
    {
        if (!ctx.nearby) return;
        refresh_ -= ctx.dt;
        if (refresh_ <= 0.0f) {
            refresh_ = 0.5f;
            Refresh();
        }
    }

    void OnLeave() override { RestoreGlobals(); }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        Texture2D& white = *S().whiteTexture;
        const auto rect = [&](float x, float y, float w, float h, Color c) { sb.Draw(white, Rectangle(static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h)), c); };
        const auto text = [&](const std::string& t, float x, float y, Color c, float s = 0.7f) { sb.DrawString(*S().font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), s, SpriteEffects::None, 0.0f); };
        text(Title(), 24, 12, Kit::kGold, 1.0f);
        switch (mode_) {
            case Mode::Window:
            case Mode::Adapter: {
                float y = 66.0f;
                for (const std::string& l : lines_) {
                    text(l, 30, y, Kit::kText, 0.65f);
                    y += 27.0f;
                }
                break;
            }
            case Mode::Renderer: DrawRenderer(rect, text); break;
            case Mode::Platform: {
                int i = 0;
                for (const auto& [name, on] : caps_) {
                    const float x = 30.0f + static_cast<float>(i % 3) * 305.0f, y = 66.0f + static_cast<float>(i / 3) * 40.0f;
                    rect(x, y, 14, 14, on ? Kit::Rgba(110, 230, 150) : Kit::Rgba(120, 70, 70));
                    text(name, x + 24, y - 6, on ? Kit::kText : Kit::kDim, 0.6f);
                    ++i;
                }
                break;
            }
        }
        sb.End();
    }

private:
    [[nodiscard]] std::string Title() const
    {
        static const char* t[] = {"The game window", "Display adapter and back buffer", "The active renderer", "Platform capabilities"};
        return t[static_cast<int>(mode_)];
    }
    [[nodiscard]] std::vector<std::string> Explanation() const
    {
        switch (mode_) {
            case Mode::Window: return {"GameWindow is the operating-system window: title, client area, resizing, and (CNAEXT) borderless mode and file", "drops. The counters show real events; try resizing the window with its border, or dragging a file onto it."};
            case Mode::Adapter: return {"GraphicsAdapter describes the display hardware and its DisplayModes; PresentationParameters describe the", "back buffer the museum draws into. All of it is read, never faked, and differs per machine and renderer."};
            case Mode::Renderer: return {"CNA's RendererCapabilityProfile tells a game what the active renderer can do (CNAEXT). A game asks once and", "picks its path: that is how this museum decides to skip wireframe or ShaderEffect source where unsupported."};
            default: return {"CNA's PlatformCapabilities say what the host can do: clipboard, IME, gamepads, haptics, camera, tray, file dialogs.", "Desktop SDL3 supports most; a terminal or browser platform supports fewer. Query once, then adapt."};
        }
    }

    void RestoreGlobals()
    {
        if (!subscribed_) return;
        subscribed_ = false;
        try {
            Game& g = *S().game;
            g.getWindowProperty().ClientSizeChanged.Remove(resizeToken_);
            g.getWindowProperty().OrientationChanged.Remove(orientationToken_);
            g.Activated.Remove(activeToken_);
            g.Deactivated.Remove(inactiveToken_);
            g.getWindowProperty().FileDropEXT.Remove(dropToken_);
#ifdef MUSEUM_PHONE
            auto& shell = Microsoft::Phone::Shell::PhoneApplicationService::getCurrentProperty();
            shell.DetachEXT();
            shell.Launching.Remove(shellTokens_[0]);
            shell.Activated.Remove(shellTokens_[1]);
            shell.Deactivated.Remove(shellTokens_[2]);
            shell.Closing.Remove(shellTokens_[3]);
#endif
            g.getWindowProperty().setTitleProperty(originalTitle_);
            g.getWindowProperty().setAllowUserResizingProperty(originalResizable_);
        } catch (const std::exception&) {}
    }

    void SetTitle(int v)
    {
        static const char* titles[] = {"CNA 3D Museum", "Hello from GameWindow", "Title set by the Window room"};
        try { S().game->getWindowProperty().setTitleProperty(v == 0 ? originalTitle_ : titles[v]); } catch (const std::exception&) {}
    }
    void SetResizable(bool v)
    {
        try { S().game->getWindowProperty().setAllowUserResizingProperty(v); } catch (const std::exception&) {}
    }

    // The operating system's own handle for the window (CNAEXT), the thing a program passes to a native API. Which fields are filled
    // depends on the windowing system: an X11 window is an integer id, a Win32 window a pointer.
    // The orientation CNA derives from the surface's shape (width over height) and the orientations the game declared it supports.
    [[nodiscard]] std::string OrientationText() const
    {
        try {
            const DisplayOrientation current = S().game->getWindowProperty().getCurrentOrientationProperty();
            const DisplayOrientation supported = S().graphics != nullptr ? S().graphics->getSupportedOrientationsProperty() : DisplayOrientation::Default;
            const auto name = [](DisplayOrientation o) {
                switch (o) {
                    case DisplayOrientation::LandscapeLeft: return "LandscapeLeft";
                    case DisplayOrientation::LandscapeRight: return "LandscapeRight";
                    case DisplayOrientation::Portrait: return "Portrait";
                    default: return "Default";
                }
            };
            return std::string(name(current)) + " (supported mask " + std::to_string(static_cast<int>(supported)) + ") / " + std::to_string(orientationEvents_);
        } catch (const std::exception& e) {
            return std::string("unavailable: ") + std::string(e.what()).substr(0, 30);
        }
    }

    [[nodiscard]] static std::string NativeWindowLine(const GameWindow& w)
    {
        try {
            const CNA::Platform::NativeWindowHandle h = w.GetNativeWindowHandleEXT();
            char buf[96];
            std::snprintf(buf, sizeof buf, "GetNativeWindowHandleEXT: %s", CNA::Platform::ToString(h.system).c_str());
            std::string line = buf;
            CNA::Platform::X11NativeWindow x;
            CNA::Platform::WaylandNativeWindow wl;
            CNA::Platform::Win32NativeWindow win;
            if (CNA::Platform::TryGetX11(h, x)) {
                std::snprintf(buf, sizeof buf, ", Display* %s, Window id 0x%llx", x.display != nullptr ? "set" : "null", static_cast<unsigned long long>(x.window));
                line += buf;
            } else if (CNA::Platform::TryGetWayland(h, wl)) {
                line += std::string(", wl_display ") + (wl.display != nullptr ? "set" : "null") + ", wl_surface " + (wl.surface != nullptr ? "set" : "null");
            } else if (CNA::Platform::TryGetWin32(h, win)) {
                line += std::string(", HWND ") + (win.hwnd != nullptr ? "set" : "null");
            } else {
                line += ", no window to hand to a native API";
            }
            return line;
        } catch (const std::exception& e) {
            return std::string("GetNativeWindowHandleEXT: ") + std::string(e.what()).substr(0, 60);
        }
    }

    // What the two static device flags would select. They apply to devices created later, so they are set only for the question and
    // put back at once: the museum's own device keeps what it was created with.
    [[nodiscard]] std::string DeviceTypeText(bool renderer) const
    {
        const bool oldNull = GraphicsAdapter::getUseNullDeviceProperty(), oldReference = GraphicsAdapter::getUseReferenceDeviceProperty();
        std::string text;
        try {
            GraphicsAdapter::setUseReferenceDeviceProperty(deviceFlags_ == 1 || deviceFlags_ == 3);
            GraphicsAdapter::setUseNullDeviceProperty(deviceFlags_ == 2 || deviceFlags_ == 3);
            if (renderer) {
                const auto required = GraphicsAdapter::GetRequiredRendererEXT();
                text = required.has_value() ? std::string(CNA::getGraphicsRendererName(*required)) + " renderer" : "none: the build's own renderer";
            } else {
                switch (GraphicsAdapter::GetCurrentDeviceTypeEXT()) {
                    case GraphicsDeviceTypeEXT::Hardware: text = "Hardware"; break;
                    case GraphicsDeviceTypeEXT::Reference: text = "Reference"; break;
                    default: text = "Null (it wins over Reference)"; break;
                }
            }
        } catch (const std::exception& e) {
            text = std::string("failed: ") + std::string(e.what()).substr(0, 40);
        }
        GraphicsAdapter::setUseNullDeviceProperty(oldNull);
        GraphicsAdapter::setUseReferenceDeviceProperty(oldReference);
        return text;
    }

    void Refresh()
    {
        lines_.clear();
        try {
            GraphicsDevice& d = *S().device;
            if (mode_ == Mode::Window) {
                GameWindow& w = S().game->getWindowProperty();
                const Rectangle cb = w.getClientBoundsProperty();
                lines_.push_back("Title: " + w.getTitleProperty());
                lines_.push_back("ClientBounds: " + std::to_string(cb.X) + ", " + std::to_string(cb.Y) + "  size " + std::to_string(cb.Width) + " x " + std::to_string(cb.Height));
                lines_.push_back(std::string("AllowUserResizing: ") + (w.getAllowUserResizingProperty() ? "true" : "false"));
                lines_.push_back(std::string("IsBorderless (CNAEXT): ") + (w.getIsBorderlessEXTProperty() ? "true" : "false"));
                lines_.push_back("ScreenDeviceName: " + w.getScreenDeviceNameProperty());
                lines_.push_back(NativeWindowLine(w));
                lines_.push_back(std::string("Game.IsActive: ") + (S().game->getIsActiveProperty() ? "true" : "false") + "    IsMouseVisible: " + (S().game->getIsMouseVisibleProperty() ? "true" : "false"));
                lines_.push_back(std::string("Game.IsFixedTimeStep: ") + (S().game->getIsFixedTimeStepProperty() ? "true" : "false") + "    TargetElapsedTime: " + Num(S().game->getTargetElapsedTimeProperty().getTotalMillisecondsProperty(), "%.2f") + " ms");
            } else if (mode_ == Mode::Adapter) {
                GraphicsAdapter& a = d.getAdapterProperty();
                adapterName_ = a.getDescriptionProperty();
                const auto& pp = d.getPresentationParametersProperty();
                const DisplayMode cur = a.getCurrentDisplayModeProperty();
                modes_.clear();
                for (const DisplayMode& m : a.getSupportedDisplayModesProperty()) modes_.push_back(m);
                static const char* profiles[] = {"Reach", "HiDef"};
                profileName_ = profiles[std::min(static_cast<int>(d.getGraphicsProfileProperty()), 1)];
                lines_.push_back("Adapter: " + a.getDescriptionProperty() + "    device " + a.getDeviceNameProperty() + (a.getIsDefaultAdapterProperty() ? "  (default)" : ""));
                lines_.push_back("Current display mode: " + std::to_string(cur.getWidthProperty()) + " x " + std::to_string(cur.getHeightProperty()) + "   aspect " + Num(static_cast<double>(cur.getAspectRatioProperty()), "%.3f") + (a.getIsWideScreenProperty() ? "  (widescreen)" : ""));
                lines_.push_back("Supported display modes: " + std::to_string(modes_.size()));
                std::string first;
                for (std::size_t i = 0; i < modes_.size() && i < 8; ++i) first += std::to_string(modes_[i].getWidthProperty()) + "x" + std::to_string(modes_[i].getHeightProperty()) + "  ";
                lines_.push_back("   " + first + (modes_.size() > 8 ? "..." : ""));
                lines_.push_back("GraphicsProfile: " + profileName_);
                lines_.push_back("Back buffer: " + std::to_string(pp.getBackBufferWidthProperty()) + " x " + std::to_string(pp.getBackBufferHeightProperty()) + ", " + std::to_string(pp.getMultiSampleCountProperty()) + "x multisample");
                lines_.push_back(std::string("Full screen: ") + (pp.getIsFullScreenProperty() ? "yes" : "no") + "    present interval " + std::to_string(static_cast<int>(pp.getPresentationIntervalProperty())) + "    depth format " + std::to_string(static_cast<int>(pp.getDepthStencilFormatProperty())));
                lines_.push_back("Adapters: " + std::to_string(GraphicsAdapter::getAdaptersProperty().size()) + "    vendor 0x" + Hex(a.getVendorIdProperty()) + "  device 0x" + Hex(a.getDeviceIdProperty()) + "  revision " + std::to_string(a.getRevisionProperty()) + "    wide screen: " + (a.getIsWideScreenProperty() ? "yes" : "no"));
                {
                    SurfaceFormat sf = SurfaceFormat::Color;
                    DepthFormat df = DepthFormat::None;
                    int ms = 0;
                    const bool reach = a.IsProfileSupported(GraphicsProfile::Reach), hidef = a.IsProfileSupported(GraphicsProfile::HiDef);
                    const bool rt = a.QueryRenderTargetFormat(GraphicsProfile::HiDef, SurfaceFormat::Color, DepthFormat::Depth24Stencil8, 4, sf, df, ms);
                    lines_.push_back(std::string("IsProfileSupported: Reach ") + (reach ? "yes" : "no") + ", HiDef " + (hidef ? "yes" : "no") +
                                     "    QueryRenderTargetFormat(Color, Depth24Stencil8, 4x): " + (rt ? "as asked" : "adjusted") + " -> format " + std::to_string(static_cast<int>(sf)) +
                                     ", depth " + std::to_string(static_cast<int>(df)) + ", " + std::to_string(ms) + " samples");
                    const bool bb = a.QueryBackBufferFormat(GraphicsProfile::HiDef, SurfaceFormat::Color, DepthFormat::Depth24, 0, sf, df, ms);
                    lines_.push_back(std::string("QueryBackBufferFormat(Color, Depth24): ") + (bb ? "ok" : "not available") + "    PresentationParameters.BackBufferFormat " + std::to_string(static_cast<int>(pp.getBackBufferFormatProperty())) +
                                     ", DisplayOrientation " + std::to_string(static_cast<int>(pp.getDisplayOrientationProperty())) + ", headless " + (pp.getHeadlessEXTProperty() ? "yes" : "no"));
                }
                const Viewport vp = d.getViewportProperty();
                lines_.push_back("Viewport: " + std::to_string(vp.getWidthProperty()) + " x " + std::to_string(vp.getHeightProperty()) + "   title-safe area " + std::to_string(vp.getTitleSafeAreaProperty().Width) + " x " + std::to_string(vp.getTitleSafeAreaProperty().Height));
            } else if (mode_ == Mode::Renderer) {
                const CNA::GraphicsRendererType type = d.GetGraphicsRendererType();
                maturity_ = CNA::toStringView(CNA::getGraphicsRendererMaturity(type));
            } else if (mode_ == Mode::Platform) {
                const CNA::Platform::PlatformCapabilities& c = S().game->GetPlatformCapabilitiesEXT();
                CNA::Platform::IPlatform& host = S().game->GetPlatformEXT();
                platformName_ = host.GetName() + " platform, " + std::string(S().device->GetGraphicsRendererName()) + " renderer";
                char clock[96];
                std::snprintf(clock, sizeof clock, "%llu Hz, up %llu ms", static_cast<unsigned long long>(host.GetPerformanceFrequency()), static_cast<unsigned long long>(host.GetTicksMilliseconds()));
                clock_ = clock;
                using CNA::Platform::PlatformSubsystem;
                const std::pair<const char*, PlatformSubsystem> subs[] = {{"Video", PlatformSubsystem::Video}, {"Audio", PlatformSubsystem::Audio}, {"Gamepad", PlatformSubsystem::Gamepad}, {"Haptic", PlatformSubsystem::Haptic}, {"Sensor", PlatformSubsystem::Sensor}};
                subsystems_.clear();
                for (const auto& [n, id] : subs)
                    if (host.IsSubsystemInitialized(id)) subsystems_ += (subsystems_.empty() ? "" : ", ") + std::string(n);
                if (subsystems_.empty()) subsystems_ = "none";
                // Each device interface is a pointer that is null where the platform has none.
                const std::pair<const char*, bool> ifaces[] = {{"keyboard", host.GetKeyboard() != nullptr}, {"mouse", host.GetMouse() != nullptr}, {"gamepad", host.GetGamepad() != nullptr}, {"joystick", host.GetJoystick() != nullptr},
                                                                {"text", host.GetTextInput() != nullptr}, {"sensors", host.GetSensors() != nullptr}, {"haptics", host.GetHaptics() != nullptr}, {"clipboard", host.GetClipboard() != nullptr},
                                                                {"displays", host.GetDisplays() != nullptr}, {"dialogs", host.GetDialogs() != nullptr}};
                int up = 0;
                std::string missing;
                for (const auto& [n, on] : ifaces) {
                    up += on ? 1 : 0;
                    if (!on) missing += (missing.empty() ? " (without " : ", ") + std::string(n);
                }
                devices_ = std::to_string(up) + " of 10" + (missing.empty() ? "" : missing + ")");
                Museum::Log("platform", "%s; clock %s; subsystems %s; interfaces %s", platformName_.c_str(), clock_.c_str(), subsystems_.c_str(), devices_.c_str());
                caps_.clear();
                const std::pair<const char*, bool> all[] = {
                    {"multiple windows", c.multipleWindows}, {"high DPI", c.highDpi}, {"multiple displays", c.multipleDisplays}, {"borderless fullscreen", c.borderlessFullscreen},
                    {"native window handle", c.nativeWindowHandle}, {"surface presentation", c.surfacePresentation}, {"OpenGL context", c.openGlContext}, {"clipboard", c.clipboard},
                    {"text input", c.textInput}, {"IME", c.ime}, {"exact keyboard state", c.exactKeyboardState}, {"pixel-accurate mouse", c.pixelAccurateMouse},
                    {"relative mouse", c.relativeMouse}, {"cursor shapes", c.cursorShapes}, {"global pointer", c.globalPointer}, {"input device enumeration", c.inputDeviceEnumeration},
                    {"gamepad", c.gamepad}, {"joystick", c.joystick}, {"gamepad rumble", c.gamepadRumble}, {"gamepad sensors", c.gamepadSensors},
                    {"haptics", c.haptics}, {"sensors", c.sensors}, {"power info", c.powerInfo}, {"message box", c.messageBox},
                    {"native file dialog", c.nativeFileDialog}, {"system tray", c.tray}, {"camera", c.camera}, {"managed entry point", c.managedEntrypoint},
                    {"drag and drop", c.dragAndDrop}, {"primary selection", c.primarySelection}, {"clipboard data", c.clipboardData}};
                supported_ = 0;
                for (const auto& [n, on] : all) {
                    caps_.emplace_back(n, on);
                    supported_ += on ? 1 : 0;
                }
                total_ = static_cast<int>(caps_.size());
            }
        } catch (const std::exception& e) {
            lines_.push_back(std::string("query failed: ") + e.what());
        }
    }

    template <typename Rect, typename Text>
    void DrawRenderer(Rect& rect, Text& text)
    {
        GraphicsDevice& d = *S().device;
        text(std::string("renderer ") + std::string(d.GetGraphicsRendererName()) + "   maturity " + maturity_, 24, 50, Kit::kText, 0.7f);
        try {
            const CNA::RendererCapabilityProfile& prof = d.GetRendererCapabilityProfileEXT();
            if (page_ == 0) {
                using F = CNA::RendererFeature;
                const F all[] = {F::ThreeDimensionalPipeline, F::DepthStencilBuffer, F::StencilBuffer, F::MultiSampleAntiAliasing, F::MultipleRenderTargets, F::AnisotropicFiltering,
                                 F::WireFrameRasterization, F::OcclusionQueries, F::ShaderEffects, F::ShaderEffectSourceExecution, F::CompiledXnaEffects, F::Texture3DStorage,
                                 F::Texture3DSampling, F::MultiStreamVertexInput, F::InstancedDrawing, F::BaseInstanceDrawing, F::AdditiveBlending, F::Float32RenderTargets,
                                 F::Float16RenderTargets, F::Float16TextureLinearFiltering, F::ShadowSampling, F::ImageBasedLighting, F::ShaderDialectGlslDesktop, F::ShaderDialectGlslEs,
                                 F::ShaderDialectGlslVulkan, F::ShaderDialectHlsl, F::ShaderDialectMsl, F::ShaderDialectWgsl};
                int i = 0;
                for (const F f : all) {
                    const auto& info = prof.GetFeature(f);
                    const float x = 24.0f + static_cast<float>(i % 2) * 470.0f, y = 80.0f + static_cast<float>(i / 2) * 31.0f;
                    Color c = Kit::Rgba(150, 150, 150);
                    const char* word = "unknown";
                    switch (info.support) {
                        case CNA::RendererFeatureSupport::Supported: c = Kit::Rgba(110, 230, 150); word = "yes"; break;
                        case CNA::RendererFeatureSupport::Restricted: c = Kit::Rgba(255, 210, 100); word = "restricted"; break;
                        case CNA::RendererFeatureSupport::Unsupported: c = Kit::Rgba(235, 120, 110); word = "no"; break;
                        default: break;
                    }
                    rect(x, y + 4, 12, 12, c);
                    text(std::string(FeatureName(f)) + ": " + word, x + 20, y - 2, Kit::kText, 0.58f);
                    ++i;
                }
            } else if (page_ == 3) {
                // CNA's own English report of the renderer, as one string a bug report can carry.
                const std::string_view report = d.GetRendererCapabilityReportEXT();
                std::vector<std::string> rows;
                std::string row;
                for (const char c : report) {
                    if (c == '\n') { rows.push_back(row); row.clear(); } else row.push_back(c);
                }
                if (!row.empty()) rows.push_back(row);
                const int visible = 16, total = static_cast<int>(rows.size());
                const int first = std::clamp(static_cast<int>(reportScroll_ * static_cast<float>(std::max(0, total - visible)) + 0.5f), 0, std::max(0, total - visible));
                float y = 80.0f;
                for (int i = first; i < std::min(total, first + visible); ++i) {
                    text(rows[static_cast<std::size_t>(i)].substr(0, 120), 24, y, Kit::kText, 0.5f);
                    y += 26.0f;
                }
                text(std::to_string(report.size()) + " bytes, " + std::to_string(total) + " lines, showing " + std::to_string(first + 1) + " to " + std::to_string(std::min(total, first + visible)), 24, 500, Kit::kDim, 0.55f);
            } else if (page_ == 2) {
                // Questions a game asks the device itself, as opposed to the capability profile.
                const auto yes = [](bool v) { return v ? "yes" : "no"; };
                const CNA::DisplayColorSpace spaces[] = {CNA::DisplayColorSpace::Srgb, CNA::DisplayColorSpace::Scrgb, CNA::DisplayColorSpace::Hdr10};
                const char* spaceNames[] = {"sRGB", "scRGB", "HDR10"};
                std::vector<std::string> rows;
                rows.push_back(std::string("GetDisplayColorSpaceEXT: ") + spaceNames[std::min(static_cast<int>(d.GetDisplayColorSpaceEXT()), 2)]);
                for (int i = 0; i < 3; ++i) rows.push_back(std::string("SupportsDisplayColorSpaceEXT(") + spaceNames[i] + "): " + yes(d.SupportsDisplayColorSpaceEXT(spaces[i])));
                rows.push_back(std::string("SetDisplayColorSpaceEXT(HDR10) accepted: ") + yes(d.SetDisplayColorSpaceEXT(CNA::DisplayColorSpace::Hdr10)) + "   (an honest no where there is no HDR swap chain)");
                rows.push_back(std::string("SupportsShadowSamplingEXT: ") + yes(d.SupportsShadowSamplingEXT()) + "      SupportsImageBasedLightingEXT: " + yes(d.SupportsImageBasedLightingEXT()));
                rows.push_back(std::string("ExecutesShaderEffectSourceEXT: ") + yes(d.ExecutesShaderEffectSourceEXT()));
                int lw = 0, lh = 0;
                const bool logical = d.GetLogicalSizeEXT(lw, lh);
                rows.push_back(std::string("GetLogicalSizeEXT: ") + (logical ? std::to_string(lw) + " x " + std::to_string(lh) : std::string("no virtual resolution set")));
                rows.push_back("GetTrackedResourceCount: " + std::to_string(d.GetTrackedResourceCount()) + " live GPU resources on this device");
                float y = 80.0f;
                for (const std::string& r : rows) {
                    text(r, 24, y, Kit::kText, 0.62f);
                    y += 34.0f;
                }
            } else {
                const std::pair<CNA::RendererLimit, const char*> limits[] = {{CNA::RendererLimit::MaxTextureDimension, "max texture dimension"}, {CNA::RendererLimit::MaxVertexStreams, "max vertex streams"},
                                                                             {CNA::RendererLimit::MaxColorAttachments, "max colour attachments"}, {CNA::RendererLimit::MaxSampledTexturesPerShaderStage, "sampled textures per stage"},
                                                                             {CNA::RendererLimit::MaxVertexInputAttributes, "vertex attributes"}, {CNA::RendererLimit::MaxUniformBufferBytes, "uniform buffer bytes"}};
                float y = 80.0f;
                for (const auto& [lim, name] : limits) {
                    const auto v = prof.GetLimit(lim);
                    text(std::string(name) + ": " + (v.known ? std::to_string(v.value) : "unknown"), 24, y, Kit::kText, 0.6f);
                    y += 26.0f;
                }
                static const char* fmtNames[] = {"Color", "Bgr565", "Bgra5551", "Bgra4444", "Dxt1", "Dxt3", "Dxt5", "NormByte2", "NormByte4", "Rgba1010102", "Rg32", "Rgba64", "Alpha8", "Single", "Vector2", "Vector4", "HalfSingle", "HalfVector2", "HalfVector4", "HdrBlend"};
                text("surface format       sampled  filter  target  blend", 440, 80, Kit::kDim, 0.55f);
                for (int i = 0; i < 20; ++i) {
                    const auto s = prof.GetSurfaceFormatSupport(static_cast<std::uint32_t>(i));
                    const float yy = 104.0f + static_cast<float>(i) * 21.0f;
                    text(fmtNames[i], 440, yy, Kit::kText, 0.5f);
                    const CNA::RendererFormatUsage usages[] = {CNA::RendererFormatUsage::Sampled, CNA::RendererFormatUsage::Filterable, CNA::RendererFormatUsage::RenderTarget, CNA::RendererFormatUsage::Blendable};
                    for (int u = 0; u < 4; ++u) {
                        const bool known = s.IsKnown(usages[u]);
                        const bool on = s.Supports(usages[u]);
                        rect(590.0f + static_cast<float>(u) * 80.0f, yy + 4, 12, 12, !known ? Kit::Rgba(110, 110, 110) : (on ? Kit::Rgba(110, 230, 150) : Kit::Rgba(235, 120, 110)));
                    }
                }
                text("green yes, red no, gray not reported", 24, 260, Kit::kDim, 0.55f);
                // CNA's own statement of what this renderer cannot do (RendererCapabilityProfile.GetAdditionalLimitationsText), wrapped to the left column.
                text("CNA's own note on this profile:", 24, 292, Kit::kText, 0.55f);
                std::string words(prof.GetAdditionalLimitationsText()), line;
                float ly = 316.0f;
                std::size_t pos = 0;
                while (pos < words.size() && ly < 520.0f) {
                    const std::size_t end = words.find(' ', pos);
                    const std::string word = words.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
                    pos = end == std::string::npos ? words.size() : end + 1;
                    if (line.size() + word.size() + 1 > 62) {
                        text(line, 24, ly, Kit::kDim, 0.46f);
                        ly += 19.0f;
                        line.clear();
                    }
                    line += (line.empty() ? "" : " ") + word;
                }
                if (!line.empty() && ly < 520.0f) text(line, 24, ly, Kit::kDim, 0.46f);
                if (words.empty()) text("(none stated)", 24, 316, Kit::kDim, 0.46f);
            }
        } catch (const std::exception& e) {
            text(std::string("capability profile unavailable: ") + e.what(), 24, 100, Kit::Rgba(255, 190, 150), 0.7f);
        }
    }

    Mode mode_;
    float refresh_ = 0.0f;
    std::vector<std::string> lines_;
    int aspectIndex_ = 3;
    std::string adapterName_ = "-", profileName_ = "-", maturity_ = "-", platformName_ = "-", clock_ = "-", subsystems_ = "-", devices_ = "-", originalTitle_;
    std::vector<DisplayMode> modes_;
    std::vector<std::pair<std::string, bool>> caps_;
    int supported_ = 0, total_ = 0, page_ = 0, deviceFlags_ = 0;
    float reportScroll_ = 0.0f;
    int orientationEvents_ = 0, resizeEvents_ = 0, activated_ = 0, deactivated_ = 0, drops_ = 0;
    bool subscribed_ = false;
    int shell_[4] = {0, 0, 0, 0};   // Phone shell events (MUSEUM_PHONE)
    std::size_t shellTokens_[4] = {0, 0, 0, 0};
    bool originalResizable_ = false;
    std::size_t orientationToken_ = 0, resizeToken_ = 0, activeToken_ = 0, inactiveToken_ = 0, dropToken_ = 0;
};

}  // namespace

std::unique_ptr<Exhibit> MakePlatformLab(const RoomDef& room, Services& services)
{
    return std::make_unique<PlatformLab>(room, services);
}

// glTF cameras: Model.CamerasEXT lists the cameras a glTF scene places (a projection already built from the file's own
// parameters, and the node's world transform); the view is the inverse of that transform. The museum's own stage model carries
// three cameras: a perspective one, an orthographic one and a perspective one with no far plane. Each is drawn through
// its camera into a small view, and its frustum is drawn over the stage on the plinth.
#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ExhibitKit.hpp"
#include "Exhibits/Math/MathKit.hpp"
#include "Exhibits/ModelUtil.hpp"
#include "Microsoft/Xna/Framework/BoundingFrustum.hpp"
#include "Microsoft/Xna/Framework/Graphics/Model.hpp"
#include "Rendering/EffectBank.hpp"
#include "Rendering/Gizmos.hpp"
#include "Rendering/MiniView.hpp"
#include "Rendering/ModelLibrary.hpp"
#include "Rendering/RenderStates.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace MathKit;

namespace {

class ModelCamerasLab final : public Exhibit {
public:
    ModelCamerasLab(const RoomDef& room, Services& services) : Exhibit(room, services), gizmos_(services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        plinth_ = BuildStagePlinth(b, 0.0f, 0.0f, 0.9f);
        ControlPanel& p = AddPanel("Model.CamerasEXT", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        p.AddChoice("Highlight a camera", {"all", "Front (perspective)", "Top (orthographic)", "Corner (infinite far plane)"}, 0, [this](int v) { highlight_ = v - 1; });
        p.AddToggle("Show the frusta", true, [this](bool v) { frusta_ = v; });
        p.AddSlider("Turn the stage", 0.0f, 6.28f, spin_, [this](float v) { spin_ = v; }, "%.2f rad");
        p.AddReadout("CamerasEXT.size()", [this] { return std::to_string(cameras_.size()); });
        p.AddReadout("Camera 1", [this] { return Describe(0); });
        p.AddReadout("Camera 2", [this] { return Describe(1); });
        p.AddReadout("Camera 3", [this] { return Describe(2); });
        SignSpec what = Kit::Plate("Cameras in a glTF file (CNAEXT)",
                                   {"XNA's Model has no cameras; a glTF scene can place several. CNA imports each as a ModelCameraEXT: a projection built from the file's own",
                                    "parameters (an orthographic one, or a perspective one with no far plane, which becomes an infinite projection) and the node's world transform,",
                                    "whose inverse is the view. The three small views are rendered through those cameras; the frusta on the stage are the same matrices."});
        what.leftAlign = true;
        what.pixelWidth = 900;
        b.AddWallSign(Wall::East, 0.0f, 2.4f, 4.4f, 2.0f, what);
    }

    void Load(InteractionSystem&, MuseumWorld&) override
    {
        try {
            model_ = S().models->Get("stage");
            if (model_ != nullptr) cameras_ = model_->getCamerasEXTProperty();
            for (int i = 0; i < 3; ++i) views_[static_cast<std::size_t>(i)] = std::make_unique<MiniView>(S(), 480, 270);
        } catch (const std::exception& e) {
            Museum::Log("cameras", "%s", e.what());
        }
    }

    void RenderOffscreen(const DrawContext& ctx) override
    {
        if (model_ == nullptr) return;
        try {
            for (std::size_t i = 0; i < cameras_.size() && i < 3; ++i) {
                Matrix view, proj;
                Vector3 eye;
                CameraMatrices(i, view, proj, eye);
                views_[i]->Render(ctx, view, proj, eye, [&](const DrawContext& c) { DrawStage(c, Matrix::getIdentityProperty()); });
            }
        } catch (const std::exception& e) {
            if (!reported_) Museum::Log("cameras", "render: %s", e.what());
            reported_ = true;
        }
    }

    void DrawOpaque(const DrawContext& ctx) override
    {
        if (model_ == nullptr) return;
        // The stage itself, small, on the plinth.
        const Matrix toPlinth = Matrix::CreateScale(kScale) * Matrix::CreateTranslation(plinth_ + Vector3(0.0f, 0.1f, 0.0f));
        DrawStage(ctx, Matrix::CreateRotationY(spin_) * toPlinth);
        if (frusta_) {
            static const Color colours[] = {Color(255, 120, 100, 255), Color(120, 220, 255, 255), Color(255, 220, 90, 255)};
            for (std::size_t i = 0; i < cameras_.size() && i < 3; ++i) {
                if (highlight_ >= 0 && static_cast<std::size_t>(highlight_) != i) continue;
                Matrix view, proj;
                Vector3 eye;
                CameraMatrices(i, view, proj, eye);
                // The frustum is drawn cut at 14 m (an infinite far plane has no far corners), from the camera's own field of view or box.
                const Matrix drawProj = cameras_[i].IsPerspective ? Matrix::CreatePerspectiveFieldOfView(2.0f * std::atan(1.0f / proj.M22), proj.M22 / proj.M11, 0.3f, 14.0f)
                                                                   : Matrix::CreateOrthographic(2.0f / proj.M11, 2.0f / proj.M22, 0.1f, 14.0f);
                // The cameras live in the stage's own space; the stage on the plinth is turned and scaled, so its frustum is brought back through that.
                const Matrix modelToWorld = Matrix::CreateRotationY(spin_) * toPlinth;
                gizmos_.Frustum(BoundingFrustum(Matrix::Invert(modelToWorld) * view * drawProj), colours[i]);
            }
            gizmos_.Flush(ctx);
        }
        for (std::size_t i = 0; i < cameras_.size() && i < 3; ++i) {
            const float along = (static_cast<float>(i) - 1.0f) * 3.5f;
            float yaw;
            const Vector3 c = FarPoint(along, 2.3f, yaw);
            views_[i]->DrawInRoom(ctx, c, yaw, 3.2f);
        }
    }

private:
    static constexpr float kScale = 0.17f;

    void CameraMatrices(std::size_t i, Matrix& view, Matrix& proj, Vector3& eye) const
    {
        const ModelCameraEXT& c = cameras_[i];
        view = Matrix::Invert(c.WorldTransform);
        proj = c.Projection;
        eye = c.WorldTransform.getTranslationProperty();
    }

    [[nodiscard]] std::string Describe(std::size_t i) const
    {
        if (i >= cameras_.size()) return "-";
        const ModelCameraEXT& c = cameras_[i];
        char buf[128];
        std::snprintf(buf, sizeof buf, "%s: %s%s", c.Name.c_str(), c.IsPerspective ? "perspective" : "orthographic", c.HasInfiniteFarPlane ? ", infinite" : "");
        return buf;
    }

    void DrawStage(const DrawContext& ctx, const Matrix& world)
    {
        BasicEffect& fx = ctx.fx.BeginLit(S().textures->White());
        fx.EnableDefaultLighting();
        fx.setViewProperty(ctx.view.view);
        fx.setProjectionProperty(ctx.view.projection);
        fx.setSpecularColorProperty(Vector3(0, 0, 0));
        RenderStates::SetOpaque3D(ctx.device);
        ModelUtil::DrawWithEffect(ctx.device, *model_, fx, world);
    }

    Gizmos gizmos_;
    Model* model_ = nullptr;
    std::vector<ModelCameraEXT> cameras_;
    std::array<std::unique_ptr<MiniView>, 3> views_;
    Vector3 plinth_{0, 0, 0};
    int highlight_ = -1;
    bool frusta_ = true, reported_ = false;
    float spin_ = 0.0f;
};

}  // namespace

std::unique_ptr<Exhibit> MakeModelCamerasLab(const RoomDef& room, Services& services)
{
    return std::make_unique<ModelCamerasLab>(room, services);
}

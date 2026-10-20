// Pipeline Workshop, mode "animation" (room wk-animation): animation as an importer delivers it, and the collections a vertex channel holds.
//   * XImporter on arm.x (tools/gen_workshop.py): frames Root > Arm > Hand, AnimTicksPerSecond 10, one AnimationSet "Wave" with
//     position keys on Arm and matrix keys on Hand. With no skeleton in the file each animation lands on the frame it names, as
//     NodeContent.Animations; every keyframe's time and transform is compared with the file.
//   * AnimationChannel's contract from XNA 4.0 (AnimationChannel.Add: List.BinarySearch, then past equal times; membership by
//     reference), AnimationKeyframe.CompareTo, and the refusals.
//   * BoneWeight, BoneWeightCollection.NormalizeWeights (sort descending, drop weights <= 0 and beyond maxWeights, scale by 1/sum, the
//     expectations computed the same way in float), IndexCollection.AddRange and PositionCollection.
#ifdef MUSEUM_PIPELINE

#include <memory>
#include <string>
#include <vector>

#include "Exhibits/Content/PipelineSupport.hpp"
#include "Exhibits/Content/WorkshopChecks.hpp"
#include "Exhibits/Services.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Graphics/AnimationContent.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Graphics/NodeContent.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Graphics/VertexCollections.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/InvalidContentException.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/ModelImporters.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/TimeSpan.hpp"

using namespace Microsoft::Xna::Framework;

namespace {

namespace Xna = Microsoft::Xna::Framework::Content::Pipeline;
namespace PipeGfx = Microsoft::Xna::Framework::Content::Pipeline::Graphics;
using Workshop::Checks;
using Workshop::F;
using Workshop::Yes;

std::shared_ptr<PipeGfx::NodeContent> FindNode(const std::shared_ptr<PipeGfx::NodeContent>& node, const std::string& name)
{
    if (node == nullptr) return nullptr;
    if (node->getNameProperty() == name) return node;
    for (const auto& child : node->getChildrenProperty())
        if (auto found = FindNode(child, name)) return found;
    return nullptr;
}

std::string Seconds(System::TimeSpan t) { return F(static_cast<double>(t.getTicksProperty()) / 1e7) + " s"; }

std::string Translation(const Matrix& m) { return "(" + F(m.M41) + ", " + F(m.M42) + ", " + F(m.M43) + ")"; }

std::string Times(const PipeGfx::AnimationChannel& channel)
{
    std::string out;
    for (const auto& k : channel) out += (out.empty() ? "" : " ") + F(static_cast<double>(k->getTimeProperty().getTicksProperty()) / 1e7);
    return out;
}

std::string Weights(const PipeGfx::BoneWeightCollection& c)
{
    std::string out;
    for (int i = 0; i < c.getCountProperty(); ++i) out += (out.empty() ? "" : ", ") + c.getItem(i).getBoneNameProperty() + " " + F(c.getItem(i).getWeightProperty(), "%.7g");
    return out;
}

std::shared_ptr<PipeGfx::AnimationKeyframe> Key(double seconds, const Matrix& transform)
{
    return std::make_shared<PipeGfx::AnimationKeyframe>(System::TimeSpan(static_cast<long long>(seconds * 1e7)), transform);
}

}  // namespace

namespace Workshop {

void AnimationChecks(Checks& checks, Services& services)
{
    // ---- XImporter: AnimationContent on the frames of arm.x -----------------------------------------------------------------------
    checks.Group("XImporter animation", [&] {
        PipelineSupport::ImporterContext context;
        Xna::XImporter importer;
        const std::shared_ptr<PipeGfx::NodeContent> root = importer.Import(services.contentRoot + "/Workshop/arm.x", context);
        const std::shared_ptr<PipeGfx::NodeContent> arm = FindNode(root, "Arm");
        const std::shared_ptr<PipeGfx::NodeContent> hand = FindNode(root, "Hand");
        checks.Add("XImporter.Import(arm.x): the frame tree", "Root > Arm > Hand", root ? root->getNameProperty() + (arm ? " > Arm" : "") + (hand ? " > Hand" : "") : "null", root && arm && hand && hand->getParentProperty() == arm.get());
        if (!arm || !hand) return;
        std::shared_ptr<PipeGfx::AnimationContent> wave;
        const bool onArm = arm->getAnimationsProperty().TryGetValue("Wave", wave) && wave != nullptr;
        checks.Add("NodeContent.Animations of Arm", "one AnimationContent \"Wave\" (no skeleton: the animation lands on the frame it names); Root carries none",
                   std::to_string(arm->getAnimationsProperty().getCountProperty()) + " on Arm" + (onArm ? " (\"" + wave->getNameProperty() + "\")" : "") + ", " + std::to_string(root->getAnimationsProperty().getCountProperty()) + " on Root",
                   onArm && arm->getAnimationsProperty().getCountProperty() == 1 && wave->getNameProperty() == "Wave" && root->getAnimationsProperty().getCountProperty() == 0);
        if (!onArm) return;
        checks.Add("AnimationContent.Duration", "1 s (the last key, tick 10 at 10 ticks per second)", Seconds(wave->getDurationProperty()), wave->getDurationProperty().getTicksProperty() == 10000000);
        std::shared_ptr<PipeGfx::AnimationChannel> channel;
        const bool hasChannel = wave->getChannelsProperty().TryGetValue("Arm", channel) && channel != nullptr;
        checks.Add("AnimationContent.Channels", "one channel, keyed by the bone it moves: Arm", std::to_string(wave->getChannelsProperty().getCountProperty()) + " channel(s)" + (hasChannel ? ", Arm" : ""),
                   hasChannel && wave->getChannelsProperty().getCountProperty() == 1);
        if (!hasChannel) return;
        const bool keys = channel->getCountProperty() == 3;
        checks.Add("the Arm channel's keyframes: times", "3 keyframes at 0, 0.5, 1 s", std::to_string(channel->getCountProperty()) + " at " + Times(*channel),
                   keys && (*channel)[0]->getTimeProperty().getTicksProperty() == 0 && (*channel)[1]->getTimeProperty().getTicksProperty() == 5000000 && (*channel)[2]->getTimeProperty().getTicksProperty() == 10000000);
        if (keys) {
            const bool exact = (*channel)[0]->getTransformProperty() == Matrix::CreateTranslation(0, 0, 0) && (*channel)[1]->getTransformProperty() == Matrix::CreateTranslation(1, 0, 0) &&
                               (*channel)[2]->getTransformProperty() == Matrix::CreateTranslation(2, 0, 0);
            checks.Add("the Arm channel's keyframes: transforms", "translations (0,0,0), (1,0,0), (2,0,0), nothing else in the matrices",
                       Translation((*channel)[0]->getTransformProperty()) + ", " + Translation((*channel)[1]->getTransformProperty()) + ", " + Translation((*channel)[2]->getTransformProperty()) + (exact ? ", exact" : ", not pure translations"), exact);
        }
        std::shared_ptr<PipeGfx::AnimationContent> lift;
        std::shared_ptr<PipeGfx::AnimationChannel> handChannel;
        const bool onHand = hand->getAnimationsProperty().TryGetValue("Wave", lift) && lift && lift->getChannelsProperty().TryGetValue("Hand", handChannel) && handChannel;
        checks.Add("the Hand animation (matrix keys)", "\"Wave\" on Hand, channel Hand: identity at 0 s, translation (0, 0.25, 0) at 1 s; duration 1 s",
                   onHand ? std::to_string(handChannel->getCountProperty()) + " keys at " + Times(*handChannel) + (handChannel->getCountProperty() == 2 ? ", last " + Translation((*handChannel)[1]->getTransformProperty()) : "") + ", duration " + Seconds(lift->getDurationProperty()) : "missing",
                   onHand && handChannel->getCountProperty() == 2 && (*handChannel)[0]->getTransformProperty() == Matrix::getIdentityProperty() && (*handChannel)[1]->getTransformProperty() == Matrix::CreateTranslation(0, 0.25f, 0) &&
                       lift->getDurationProperty().getTicksProperty() == 10000000);
        checks.Add("ToString of AnimationContent, the two dictionaries, a channel and a keyframe", "the full XNA type names, as XNA's object.ToString",
                   wave->ToString() + " | " + channel->ToString(),
                   wave->ToString() == "Microsoft.Xna.Framework.Content.Pipeline.Graphics.AnimationContent" && wave->getChannelsProperty().ToString() == "Microsoft.Xna.Framework.Content.Pipeline.Graphics.AnimationChannelDictionary" &&
                       arm->getAnimationsProperty().ToString() == "Microsoft.Xna.Framework.Content.Pipeline.Graphics.AnimationContentDictionary" && channel->ToString() == "Microsoft.Xna.Framework.Content.Pipeline.Graphics.AnimationChannel" &&
                       (*channel)[0]->ToString() == "Microsoft.Xna.Framework.Content.Pipeline.Graphics.AnimationKeyframe");
    });

    // ---- AnimationChannel and AnimationKeyframe ----------------------------------------------------------------------------------
    checks.Group("AnimationChannel", [&] {
        PipeGfx::AnimationChannel channel;
        const auto k2 = Key(2, Matrix::CreateTranslation(2, 0, 0)), k0 = Key(0, Matrix::getIdentityProperty()), k1 = Key(1, Matrix::CreateTranslation(1, 0, 0)), k1b = Key(1, Matrix::CreateTranslation(0, 1, 0));
        const int i2 = channel.Add(k2), i0 = channel.Add(k0), i1 = channel.Add(k1), i1b = channel.Add(k1b);
        checks.Add("Add(2 s), Add(0 s), Add(1 s), Add(another 1 s)", "indices 0, 0, 1, 2: sorted by time, a key at an occupied time goes after it",
                   std::to_string(i2) + ", " + std::to_string(i0) + ", " + std::to_string(i1) + ", " + std::to_string(i1b) + "; times " + Times(channel),
                   i2 == 0 && i0 == 0 && i1 == 1 && i1b == 2 && channel[0] == k0 && channel[1] == k1 && channel[2] == k1b && channel[3] == k2);
        const auto twin = Key(1, Matrix::CreateTranslation(1, 0, 0));
        checks.Add("Contains and IndexOf are by reference", "Contains(k1) true, Contains(an equal twin) false; IndexOf(k1b) 2, IndexOf(twin) -1",
                   Yes(channel.Contains(k1)) + ", " + Yes(channel.Contains(twin)) + "; " + std::to_string(channel.IndexOf(k1b)) + ", " + std::to_string(channel.IndexOf(twin)),
                   channel.Contains(k1) && !channel.Contains(twin) && channel.IndexOf(k1b) == 2 && channel.IndexOf(twin) == -1);
        checks.Add("AnimationKeyframe.CompareTo is by time only", "-1, 1, 0 (0 s vs 2 s, 2 s vs 0 s, the two 1 s keys)",
                   std::to_string(k0->CompareTo(*k2)) + ", " + std::to_string(k2->CompareTo(*k0)) + ", " + std::to_string(k1->CompareTo(*k1b)), k0->CompareTo(*k2) == -1 && k2->CompareTo(*k0) == 1 && k1->CompareTo(*k1b) == 0);
        const bool removed = channel.Remove(k1), again = channel.Remove(k1);
        channel.RemoveAt(0);
        checks.Add("Remove(k1) twice, then RemoveAt(0)", "true, false; 2 keys left: 1 s (k1b) and 2 s", Yes(removed) + ", " + Yes(again) + "; " + std::to_string(channel.getCountProperty()) + " keys at " + Times(channel),
                   removed && !again && channel.getCountProperty() == 2 && channel[0] == k1b && channel[1] == k2);
        k1b->setTransformProperty(Matrix::CreateScale(3.0f));
        checks.Add("AnimationKeyframe.Transform set after adding", "the channel's keyframe sees it (same object)", "M11 " + F(channel[0]->getTransformProperty().M11), channel[0]->getTransformProperty().M11 == 3.0f);
        checks.ExpectThrow<System::ArgumentOutOfRangeException>("AnimationChannel.RemoveAt(10)", "ArgumentOutOfRangeException (XNA: List.RemoveAt)", [&] { channel.RemoveAt(10); });
        checks.ExpectThrow<System::ArgumentOutOfRangeException>("AnimationChannel[5]", "ArgumentOutOfRangeException (XNA: List indexer)", [&] { (void)channel[5]; });
        checks.ExpectThrow<System::ArgumentNullException>("AnimationChannel.Add(null)", "ArgumentNullException (XNA: \"item\")", [&] { (void)channel.Add(nullptr); });
        channel.Clear();
        checks.Add("Clear()", "0 keys", std::to_string(channel.getCountProperty()) + " keys", channel.getCountProperty() == 0);

        PipeGfx::AnimationContent content;
        auto walk = std::make_shared<PipeGfx::AnimationChannel>();
        (void)walk->Add(Key(0.25, Matrix::getIdentityProperty()));
        content.getChannelsProperty().Add("Leg", walk);
        content.setDurationProperty(System::TimeSpan(2500000));
        PipeGfx::AnimationContentDictionary clips;
        clips.Add("Walk", std::make_shared<PipeGfx::AnimationContent>(content));
        checks.Add("an AnimationContent built in code, in an AnimationContentDictionary", "Walk: duration 0.25 s, channel Leg with one key",
                   std::to_string(clips.getCountProperty()) + " clip; duration " + Seconds(clips["Walk"]->getDurationProperty()) + ", channels " + std::to_string(clips["Walk"]->getChannelsProperty().getCountProperty()),
                   clips.ContainsKey("Walk") && clips["Walk"]->getDurationProperty().getTicksProperty() == 2500000 && clips["Walk"]->getChannelsProperty().ContainsKey("Leg"));
    });

    // ---- the vertex collections ---------------------------------------------------------------------------------------------------
    checks.Group("VertexCollections", [&] {
        const PipeGfx::BoneWeight hip("Hip", 0.5f), unset;
        checks.Add("BoneWeight(\"Hip\", 0.5) and default(BoneWeight)", "Hip 0.5; an empty name and 0", hip.getBoneNameProperty() + " " + F(hip.getWeightProperty()) + "; \"" + unset.getBoneNameProperty() + "\" " + F(unset.getWeightProperty()),
                   hip.getBoneNameProperty() == "Hip" && hip.getWeightProperty() == 0.5f && unset.getBoneNameProperty().empty() && unset.getWeightProperty() == 0.0f);
        checks.ExpectThrow<System::ArgumentNullException>("BoneWeight(\"\", 0.5)", "ArgumentNullException (XNA: boneName)", [] { (void)PipeGfx::BoneWeight("", 0.5f); });
        checks.ExpectThrow<System::ArgumentOutOfRangeException>("BoneWeight(\"Hip\", 1.5)", "ArgumentOutOfRangeException (XNA: weight outside [0, 1])", [] { (void)PipeGfx::BoneWeight("Hip", 1.5f); });
        checks.Add("BoneWeight equality and ToString", "equal by name and weight; the full type name", std::string(hip == PipeGfx::BoneWeight("Hip", 0.5f) ? "equal" : "different") + ", " + (hip != PipeGfx::BoneWeight("Hip", 0.25f) ? "differs" : "same") + "; " + hip.ToString(),
                   hip == PipeGfx::BoneWeight("Hip", 0.5f) && hip != PipeGfx::BoneWeight("Hip", 0.25f) && hip.ToString() == "Microsoft.Xna.Framework.Content.Pipeline.Graphics.BoneWeight");

        const auto fill = [](PipeGfx::BoneWeightCollection& c) {
            c.Add(PipeGfx::BoneWeight("A", 0.2f));
            c.Add(PipeGfx::BoneWeight("B", 0.5f));
            c.Add(PipeGfx::BoneWeight("C", 0.0f));
            c.Add(PipeGfx::BoneWeight("D", 0.1f));
        };
        PipeGfx::BoneWeightCollection all;
        fill(all);
        all.NormalizeWeights();
        // XNA: sum in the sorted order in float, scale = 1 / sum, each weight * scale.
        const float sum3 = 0.5f + 0.2f + 0.1f, scale3 = 1.0f / sum3;
        checks.Add("NormalizeWeights() of A .2, B .5, C 0, D .1", "B, A, D (largest first, C dropped as <= 0), each times 1 / 0.8f",
                   Weights(all),
                   all.getCountProperty() == 3 && all.getItem(0).getBoneNameProperty() == "B" && all.getItem(1).getBoneNameProperty() == "A" && all.getItem(2).getBoneNameProperty() == "D" &&
                       all.getItem(0).getWeightProperty() == 0.5f * scale3 && all.getItem(1).getWeightProperty() == 0.2f * scale3 && all.getItem(2).getWeightProperty() == 0.1f * scale3);
        PipeGfx::BoneWeightCollection two;
        fill(two);
        two.NormalizeWeights(2);
        const float scale2 = 1.0f / (0.5f + 0.2f);
        checks.Add("NormalizeWeights(2) of the same", "B, A only, each times 1 / 0.7f", Weights(two),
                   two.getCountProperty() == 2 && two.getItem(0).getBoneNameProperty() == "B" && two.getItem(1).getBoneNameProperty() == "A" && two.getItem(0).getWeightProperty() == 0.5f * scale2 &&
                       two.getItem(1).getWeightProperty() == 0.2f * scale2);
        checks.ExpectThrow<System::ArgumentOutOfRangeException>("NormalizeWeights(0)", "ArgumentOutOfRangeException (XNA: maxWeights <= 0)", [&] {
            PipeGfx::BoneWeightCollection c;
            fill(c);
            c.NormalizeWeights(0);
        });
        checks.ExpectThrow<Xna::InvalidContentException>("NormalizeWeights() of weights that sum to zero", "InvalidContentException (XNA: CantNormalizeWeights)", [] {
            PipeGfx::BoneWeightCollection c;
            c.Add(PipeGfx::BoneWeight("Z", 0.0f));
            c.NormalizeWeights();
        });

        PipeGfx::IndexCollection indices;
        indices.Add(7);
        indices.AddRange({0, 1, 2, 2, 3, 0});
        std::string list;
        for (int i = 0; i < indices.getCountProperty(); ++i) list += (list.empty() ? "" : " ") + std::to_string(indices.getItem(i));
        checks.Add("IndexCollection.Add(7), AddRange({0,1,2,2,3,0})", "7 0 1 2 2 3 0 in order; ToString the full type name", list + "; " + indices.ToString(),
                   list == "7 0 1 2 2 3 0" && indices.ToString() == "Microsoft.Xna.Framework.Content.Pipeline.Graphics.IndexCollection");
        PipeGfx::PositionCollection positions;
        positions.Add(Vector3(1, 2, 3));
        positions.Add(Vector3(-1, 0, 0.5f));
        checks.Add("PositionCollection", "2 positions as added; ToString the full type name; BoneWeightCollection's too",
                   std::to_string(positions.getCountProperty()) + ", (" + F(positions.getItem(1).X) + ", " + F(positions.getItem(1).Y) + ", " + F(positions.getItem(1).Z) + "); " + positions.ToString(),
                   positions.getCountProperty() == 2 && positions.getItem(0) == Vector3(1, 2, 3) && positions.getItem(1) == Vector3(-1, 0, 0.5f) && positions.ToString() == "Microsoft.Xna.Framework.Content.Pipeline.Graphics.PositionCollection" &&
                       all.ToString() == "Microsoft.Xna.Framework.Content.Pipeline.Graphics.BoneWeightCollection");
    });
}

}  // namespace Workshop

#endif  // MUSEUM_PIPELINE

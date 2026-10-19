// Pipeline Workshop, mode "xml" (room wk-xml): System::Xml::Serialization::XmlSerializer round trips of XNA values. .NET's
// XmlSerializer reflects over public fields; C++ cannot, so CNA states the member lists of the math types once
// (Microsoft/Xna/Framework/XmlSerializationEXT.hpp) and the Keys names once (Input/XmlSerializationEXT.hpp). A game's own settings
// record then needs only its own member list. Every value must come back bit for bit: .NET writes floats with the round-trip
// format, infinity as INF, and an enum as its member name. Color and Rectangle are serializable in XNA (their public R/G/B/A/
// PackedValue properties and X/Y/Width/Height fields); whether CNA states their member lists is checked at compile time.
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "Exhibits/Content/WorkshopChecks.hpp"
#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Input/Keys.hpp"
#include "Microsoft/Xna/Framework/Input/XmlSerializationEXT.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Quaternion.hpp"
#include "Microsoft/Xna/Framework/Rectangle.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"
#include "Microsoft/Xna/Framework/Vector4.hpp"
#include "Microsoft/Xna/Framework/XmlSerializationEXT.hpp"
#include "System/Xml/Serialization/XmlSerializer.hpp"

using namespace Microsoft::Xna::Framework;
using Microsoft::Xna::Framework::Input::Keys;

namespace {

// A game's own save record, the way an XNA game writes one: public fields named as the elements.
struct WorkshopSettings {
    std::string PlayerName;
    std::int32_t Volume = 0;
    bool Fullscreen = false;
    Vector2 Cursor;
    Vector3 SpawnPoint;
    Vector4 Glow;
    Quaternion Facing;
    Matrix Camera;
    Keys JumpKey = Keys::None;
    Keys PauseKey = Keys::None;
    SHARP_XML_SERIALIZABLE(WorkshopSettings, "WorkshopSettings", SHARP_XML_M(WorkshopSettings, PlayerName), SHARP_XML_M(WorkshopSettings, Volume), SHARP_XML_M(WorkshopSettings, Fullscreen),
                           SHARP_XML_M(WorkshopSettings, Cursor), SHARP_XML_M(WorkshopSettings, SpawnPoint), SHARP_XML_M(WorkshopSettings, Glow), SHARP_XML_M(WorkshopSettings, Facing),
                           SHARP_XML_M(WorkshopSettings, Camera), SHARP_XML_M(WorkshopSettings, JumpKey), SHARP_XML_M(WorkshopSettings, PauseKey))
};

// The same question XmlSerializer<T> asks (a root name and a member list found by argument-dependent lookup), asked without
// instantiating the serializer, so a missing adapter is a reported result instead of a compile error.
template <typename T>
constexpr bool HasXmlMembers = requires(const T* value) {
    { SharpXmlRootName(value) };
    { SharpXmlMembers(value) };
};

bool SameBits(float a, float b)
{
    std::uint32_t x = 0, y = 0;
    std::memcpy(&x, &a, 4);
    std::memcpy(&y, &b, 4);
    return x == y;
}

bool SameBits(const Vector4& a, const Vector4& b) { return SameBits(a.X, b.X) && SameBits(a.Y, b.Y) && SameBits(a.Z, b.Z) && SameBits(a.W, b.W); }

bool SameBits(const Matrix& a, const Matrix& b)
{
    const float* p = &a.M11;
    const float* q = &b.M11;
    for (int i = 0; i < 16; ++i)
        if (!SameBits(p[i], q[i])) return false;
    return true;
}

std::string Between(const std::string& xml, const std::string& tag)
{
    const std::string open = "<" + tag + ">", close = "</" + tag + ">";
    const std::size_t a = xml.find(open);
    if (a == std::string::npos) return "(no <" + tag + ">)";
    const std::size_t b = xml.find(close, a);
    return b == std::string::npos ? "(unclosed)" : xml.substr(a + open.size(), b - a - open.size());
}

}  // namespace

namespace Workshop {

void XmlSerializerChecks(Checks& checks, Services&)
{
    using System::Xml::Serialization::XmlSerializationOptions;
    using System::Xml::Serialization::XmlSerializer;
    XmlSerializationOptions bare;
    bare.OmitXmlDeclaration = true;

    checks.Group("Vector3 as a document", [&] {
        const XmlSerializer<Vector3> serializer;
        const std::string xml = serializer.Serialize(Vector3(1.0f, 2.5f, -3.0f), bare);
        // .NET writes the root element named after the type, the fields in declaration order, each float in its shortest round-trip form.
        const bool shape = xml.rfind("<Vector3", 0) == 0 && xml.find("<X>1</X><Y>2.5</Y><Z>-3</Z></Vector3>") != std::string::npos;
        checks.Add("XmlSerializer<Vector3>.Serialize(1, 2.5, -3)", "<Vector3 ...><X>1</X><Y>2.5</Y><Z>-3</Z></Vector3>", Checks::Short(xml, 160), shape);
        checks.Add("XmlSerializer<Vector3>.RootElementName", "Vector3", XmlSerializer<Vector3>::RootElementName(), XmlSerializer<Vector3>::RootElementName() == "Vector3");
    });

    checks.Group("value round trips", [&] {
        const float hard[] = {0.1f, 1.0f / 3.0f, 2.0f / 3.0f, 1e-7f, 123456.789f, -7.25f, 3.4028235e38f, 1.17549435e-38f};
        {
            const XmlSerializer<Vector2> s;
            const Vector2 v(hard[0], hard[1]);
            const Vector2 back = s.Deserialize(s.Serialize(v));
            checks.Add("Vector2 round trip", "bit for bit (0.1, 1/3)", "(" + F(back.X, "%.9g") + ", " + F(back.Y, "%.9g") + ")", SameBits(back.X, v.X) && SameBits(back.Y, v.Y));
        }
        {
            const XmlSerializer<Vector3> s;
            const Vector3 v(hard[2], hard[3], hard[4]);
            const Vector3 back = s.Deserialize(s.Serialize(v));
            checks.Add("Vector3 round trip", "bit for bit (2/3, 1e-7, 123456.789)", "(" + F(back.X, "%.9g") + ", " + F(back.Y, "%.9g") + ", " + F(back.Z, "%.9g") + ")",
                       SameBits(back.X, v.X) && SameBits(back.Y, v.Y) && SameBits(back.Z, v.Z));
        }
        {
            const XmlSerializer<Vector4> s;
            const Vector4 v(hard[5], hard[6], hard[7], 0.0f);
            const Vector4 back = s.Deserialize(s.Serialize(v));
            checks.Add("Vector4 round trip", "bit for bit (-7.25, float max, smallest normal, 0)", "(" + F(back.X, "%.9g") + ", " + F(back.Y, "%.9g") + ", " + F(back.Z, "%.9g") + ", " + F(back.W) + ")", SameBits(back, v));
        }
        {
            const XmlSerializer<Vector4> s;
            const Vector4 v(INFINITY, -INFINITY, NAN, 1.0f);
            const std::string xml = s.Serialize(v);
            const Vector4 back = s.Deserialize(xml);
            // XmlConvert.ToString(float): positive infinity is "INF", negative "-INF", NaN "NaN" (XML Schema's lexical forms).
            const std::string spelled = Between(xml, "X") + " " + Between(xml, "Y") + " " + Between(xml, "Z");
            checks.Add("infinities and NaN", "written INF -INF NaN, read back as the same values", spelled + (std::isnan(back.Z) ? ", NaN back" : ", not NaN back"),
                       spelled == "INF -INF NaN" && std::isinf(back.X) && back.X > 0 && std::isinf(back.Y) && back.Y < 0 && std::isnan(back.Z) && back.W == 1.0f);
        }
        {
            const XmlSerializer<Quaternion> s;
            const Quaternion q = Quaternion::CreateFromYawPitchRoll(0.3f, -1.1f, 2.9f);
            const std::string xml = s.Serialize(q, bare);
            const Quaternion back = s.Deserialize(xml);
            const bool order = xml.find("<X>") < xml.find("<Y>") && xml.find("<Y>") < xml.find("<Z>") && xml.find("<Z>") < xml.find("<W>");
            checks.Add("Quaternion round trip", "bit for bit, elements X Y Z W in that order", order ? "order X Y Z W" : "other order",
                       order && SameBits(Vector4(back.X, back.Y, back.Z, back.W), Vector4(q.X, q.Y, q.Z, q.W)));
        }
        {
            const XmlSerializer<Matrix> s;
            const Matrix m = Matrix::CreatePerspectiveFieldOfView(0.9f, 16.0f / 9.0f, 0.1f, 1000.0f) * Matrix::CreateRotationZ(0.2f);
            const std::string xml = s.Serialize(m, bare);
            const Matrix back = s.Deserialize(xml);
            bool order = true;
            std::size_t at = 0;
            for (const char* e : {"<M11>", "<M12>", "<M13>", "<M14>", "<M21>", "<M22>", "<M23>", "<M24>", "<M31>", "<M32>", "<M33>", "<M34>", "<M41>", "<M42>", "<M43>", "<M44>"}) {
                const std::size_t next = xml.find(e);
                order = order && next != std::string::npos && next >= at;
                at = next;
            }
            checks.Add("Matrix round trip", "bit for bit, M11 ... M44 row by row", order ? "16 elements in order" : "elements missing or out of order", order && SameBits(back, m));
        }
    });

    checks.Group("Keys", [&] {
        WorkshopSettings probe;
        probe.JumpKey = Keys::Space;
        probe.PauseKey = Keys::OemTilde;
        const XmlSerializer<WorkshopSettings> s;
        const std::string xml = s.Serialize(probe, bare);
        // .NET writes an enum member by name, never by number.
        checks.Add("Keys written by name", "<JumpKey>Space</JumpKey>, <PauseKey>OemTilde</PauseKey>", Between(xml, "JumpKey") + ", " + Between(xml, "PauseKey"),
                   Between(xml, "JumpKey") == "Space" && Between(xml, "PauseKey") == "OemTilde");
        const WorkshopSettings back = s.Deserialize(xml);
        checks.Add("Keys read back", "Space, OemTilde", std::to_string(static_cast<int>(back.JumpKey)) + ", " + std::to_string(static_cast<int>(back.PauseKey)), back.JumpKey == Keys::Space && back.PauseKey == Keys::OemTilde);
        std::string bad = xml;
        const std::size_t at = bad.find("Space");
        bad.replace(at, 5, "Spacebar");
        checks.ExpectThrow<std::exception>("an unknown Keys name", "refused (.NET: InvalidOperationException, 'Instance validation error')", [&] { (void)s.Deserialize(bad); });
    });

    checks.Group("a settings record", [&] {
        WorkshopSettings save;
        save.PlayerName = "Ada & <Grace> \"Lovelace\"";
        save.Volume = -3;
        save.Fullscreen = true;
        save.Cursor = Vector2(640.5f, 359.25f);
        save.SpawnPoint = Vector3(0.1f, -0.2f, 0.3f);
        save.Glow = Vector4(1.0f, 0.5f, 0.25f, 0.125f);
        save.Facing = Quaternion::CreateFromAxisAngle(Vector3::Normalize(Vector3(1, 2, 3)), 0.77f);
        save.Camera = Matrix::CreateLookAt(Vector3(3, 4, 5), Vector3::Zero, Vector3::Up);
        save.JumpKey = Keys::LeftControl;
        save.PauseKey = Keys::F12;
        const XmlSerializer<WorkshopSettings> s;
        XmlSerializationOptions pretty;
        pretty.Indent = true;
        const std::string xml = s.Serialize(save, pretty);
        const WorkshopSettings back = s.Deserialize(xml);
        checks.Add("string with markup characters", "escaped on write, identical on read", "\"" + back.PlayerName + "\"", back.PlayerName == save.PlayerName && xml.find("&lt;Grace&gt;") != std::string::npos);
        checks.Add("Int32 and Boolean", "-3, true (written \"true\")", std::to_string(back.Volume) + ", " + Yes(back.Fullscreen) + ", text " + Between(xml, "Fullscreen"),
                   back.Volume == -3 && back.Fullscreen && Between(xml, "Fullscreen") == "true");
        checks.Add("nested Vector2/3/4, Quaternion, Matrix", "all bit for bit",
                   std::string(SameBits(back.Camera, save.Camera) ? "matrix equal" : "matrix differs") + ", spawn " + F(back.SpawnPoint.X, "%.9g"),
                   SameBits(back.Cursor.X, save.Cursor.X) && SameBits(back.Cursor.Y, save.Cursor.Y) && back.SpawnPoint == save.SpawnPoint && SameBits(back.Glow, save.Glow) && back.Facing == save.Facing &&
                       SameBits(back.Camera, save.Camera));
        checks.Add("nested Keys", "LeftControl, F12", Between(xml, "JumpKey") + ", " + Between(xml, "PauseKey"), back.JumpKey == Keys::LeftControl && back.PauseKey == Keys::F12);
        checks.ExpectThrow<std::exception>("a document whose root is another type", "refused (.NET: InvalidOperationException, '<Vector3> was not expected')",
                                           [&] { (void)s.Deserialize("<Vector3><X>1</X><Y>2</Y><Z>3</Z></Vector3>"); });
    });

    // Color and Rectangle are XNA value types that .NET's XmlSerializer handles by reflection (Color: its read/write R, G, B, A and
    // PackedValue properties; Rectangle: its X, Y, Width, Height fields). CNA states member lists for the vectors, Quaternion and Matrix;
    // whether it states them for these two decides whether a game's save record holding one compiles at all.
    checks.Add("an XmlSerializer member list for Color", "stated (XNA: XmlSerializer serializes Color's R, G, B, A, PackedValue)", HasXmlMembers<Color> ? "stated" : "none: XmlSerializer<Color> does not compile",
               HasXmlMembers<Color>);
    checks.Add("an XmlSerializer member list for Rectangle", "stated (XNA: XmlSerializer serializes Rectangle's X, Y, Width, Height)",
               HasXmlMembers<Rectangle> ? "stated" : "none: XmlSerializer<Rectangle> does not compile", HasXmlMembers<Rectangle>);
}

}  // namespace Workshop

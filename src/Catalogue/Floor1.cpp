// Floor 1: Fundamentals. The application model, the maths a game is built on, geometry tests and cameras.
#include "Catalogue/CatalogueBuilder.hpp"

namespace {

WingSpec MathWing()
{
    WingSpec w;
    w.name = "Vector Mathematics";
    w.tagline = "Vector3: arithmetic, products, reflection and interpolation";
    w.theme = "corridor-cool";
    w.accent = {0.45f, 0.78f, 0.98f};
    w.spineSide = Wall::East;
    const auto vec = [](const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "vectors";
        r.params = {{"mode", mode}};
        r.width = 13;
        r.depth = 13;
        r.height = 5.0f;
        r.theme = "white-cube";
        r.apis = std::move(apis);
        return r;
    };
    w.rooms = {
        vec("vec-add", "Adding Vectors", "Vector3.Add and Subtract: the parallelogram rule, with sliders for both vectors", "add", {"Vector3.Add", "Vector3.Subtract"}),
        vec("vec-dot", "The Dot Product", "Vector3.Dot measures how much two vectors agree; its sign and size follow the angle", "dot", {"Vector3.Dot"}),
        vec("vec-cross", "The Cross Product", "Vector3.Cross builds a perpendicular vector whose length is the spanned area", "cross", {"Vector3.Cross", "Vector3.Length"}),
        vec("vec-project", "Projection and Rejection", "Splitting a vector into a part along another and a part across it", "project", {"Vector3.Dot", "Vector3.Multiply"}),
        vec("vec-reflect", "Reflection", "Vector3.Reflect bounces a vector off a surface with a given normal", "reflect", {"Vector3.Reflect", "Vector3.Normalize", "Plane"}),
        vec("vec-lerp", "Interpolation", "Vector3.Lerp, SmoothStep and CatmullRom between two points", "lerp", {"Vector3.Lerp", "Vector3.SmoothStep", "Vector3.CatmullRom"}),
        vec("vec-normalize", "Length and Normalize", "Vector3.Length, Normalize and Distance on the unit sphere", "normalize", {"Vector3.Length", "Vector3.Normalize", "Vector3.Distance"}),
    };
    return w;
}


WingSpec ValueTypeWing()
{
    WingSpec w;
    w.name = "Value Types";
    w.tagline = "Packed vectors, Rectangle and Point, Color: the small structures everything else is built from";
    w.theme = "corridor-warm";
    w.accent = {0.55f, 0.85f, 0.65f};
    w.spineSide = Wall::East;
    const auto vt = [](const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "struct-lab";
        r.params = {{"mode", mode}};
        r.width = 13;
        r.depth = 13;
        r.height = 5.0f;
        r.theme = "white-cube";
        r.apis = std::move(apis);
        return r;
    };
    w.rooms = {
        vt("vt-packed", "Packed Vector Formats", "Byte4, NormalizedShort4, HalfVector4, Rgba1010102, Bgr565... what each stores and what it loses", "packed",
           {"IPackedVector", "IPackedVectorT.PackedValue", "Byte4", "NormalizedByte4", "NormalizedShort4", "Short4", "HalfVector4", "Rgba1010102", "Rgba64", "Bgra4444", "Bgra5551", "Bgr565", "Rg32", "Short2", "NormalizedShort2", "NormalizedByte2", "HalfVector2"}),
        vt("vt-rectangle", "Rectangle and Point", "Intersects, Intersect, Union, Contains and Inflate computed live on two rectangles", "rect", {"Rectangle", "Rectangle.Intersects", "Rectangle.Intersect", "Rectangle.Union", "Rectangle.Contains", "Rectangle.Inflate", "Rectangle.Bottom", "Rectangle.Location", "Rectangle.IsEmpty", "Point"}),
        vt("vt-color", "Colour", "Color.Lerp, Multiply, FromNonPremultiplied and the packed 32-bit value", "color", {"Color", "Color.Lerp", "Color.Multiply", "Color.FromNonPremultiplied", "Color.PackedValue", "Color.ToVector3", "Color.PackFromVector4"}),
        []{
            RoomSpec r;
            r.key = "vt-colors";
            r.title = "The 141 Named Colours";
            r.subtitle = "Color.Transparent and Color.AliceBlue to Color.YellowGreen, in declaration order, by hue or by brightness";
            r.exhibit = "color-gallery";
            r.width = 13;
            r.depth = 13;
            r.height = 5.0f;
            r.theme = "white-cube";
            r.apis = {"Color", "Color.Transparent", "Color.AliceBlue", "Color.CornflowerBlue", "Color.Gold", "Color.Red", "Color.Green", "Color.Blue", "Color.White", "Color.Black", "Color.YellowGreen", "Color.R", "Color.G", "Color.B", "Color.A", "Color.PackedValue", "Color.ToString"};
            return r;
        }(),
        []{
            RoomSpec r;
            r.key = "vt-converters";
            r.title = "Type Converters";
            r.subtitle = "Vector3, Color, BoundingBox, Ray, Matrix and more as text in four cultures: what ConvertToString writes and ConvertFromString refuses";
            r.exhibit = "converter-lab";
            r.width = 13;
            r.depth = 13;
            r.height = 5.0f;
            r.theme = "white-cube";
            r.apis = {"Vector3Converter", "Vector2Converter", "Vector4Converter", "QuaternionConverter", "ColorConverter", "RectangleConverter", "PointConverter", "BoundingBoxConverter", "BoundingSphereConverter", "PlaneConverter", "RayConverter", "MatrixConverter", "MathTypeConverter", "MathTypeConverter.CanConvertFrom", "MathTypeConverter.GetProperties", "Vector3Converter.ConvertTo", "Vector3Converter.ConvertFrom"};
            return r;
        }(),
    };
    return w;
}

WingSpec TransformWing()
{
    WingSpec w;
    w.name = "Matrices and Transforms";
    w.tagline = "World, view and projection: how a point reaches the screen";
    w.theme = "corridor-green";
    w.accent = {0.55f, 0.90f, 0.55f};
    w.spineSide = Wall::West;
    const auto tr = [](const char* key, const char* title, const char* subtitle, const char* mode, std::vector<std::string> apis) {
        RoomSpec r;
        r.key = key;
        r.title = title;
        r.subtitle = subtitle;
        r.exhibit = "transforms";
        r.params = {{"mode", mode}};
        r.width = 13;
        r.depth = 13;
        r.theme = "white-cube";
        r.apis = std::move(apis);
        return r;
    };
    w.rooms = {
        tr("mat-order", "Transform Order", "Matrix.CreateScale, CreateRotationY and CreateTranslation multiplied in two orders", "order", {"Matrix.CreateScale", "Matrix.CreateRotationY", "Matrix.CreateTranslation", "Matrix.Multiply"}),
        tr("mat-hierarchy", "Parent and Child", "Local and world matrices, Matrix.Decompose and transform hierarchies", "hierarchy", {"Matrix.Multiply", "Matrix.Decompose", "Matrix.Translation"}),
        tr("mat-view", "The View Matrix", "Matrix.CreateLookAt: where the camera is, what it looks at, and what it sees", "lookat", {"Matrix.CreateLookAt", "Matrix.Invert", "BoundingFrustum"}),
        tr("mat-perspective", "Perspective Projection", "CreatePerspectiveFieldOfView with live field of view, aspect and clip planes", "perspective", {"Matrix.CreatePerspectiveFieldOfView", "BoundingFrustum.Contains"}),
        tr("mat-ortho", "Orthographic Projection", "CreateOrthographic beside perspective: no vanishing point, constant size", "ortho", {"Matrix.CreateOrthographic", "Matrix.CreatePerspectiveFieldOfView"}),
        tr("mat-screen", "World to Screen", "World, view, clip and screen coordinates of a point, with Viewport.Project and Unproject", "project", {"Viewport.Project", "Viewport.Unproject", "Vector4.Transform", "Matrix.Multiply"}),
        tr("mat-offcenter", "Off-Centre Projections", "CreatePerspective, CreatePerspectiveOffCenter and CreateOrthographicOffCenter: asymmetric frusta and the classic pixel-space matrix", "offcenter",
           {"Matrix.CreatePerspective", "Matrix.CreatePerspectiveOffCenter", "Matrix.CreateOrthographicOffCenter"}),
        tr("mat-billboard", "Billboards", "Matrix.CreateBillboard and CreateConstrainedBillboard: sprites that turn to face a camera, fully or about an axis", "billboard",
           {"Matrix.CreateBillboard", "Matrix.CreateConstrainedBillboard"}),
        tr("mat-reflection", "Reflection Matrices", "Matrix.CreateReflection mirrors a world across a Plane that Plane.Transform moves, and the triangle winding flips", "reflection",
           {"Matrix.CreateReflection", "Plane.Transform", "Matrix.Determinant"}),
        tr("mat-normals", "Normals and the Inverse Transpose", "Why normals need Transpose(Invert(world)) under non-uniform scale: red arrows lean, green ones are perpendicular", "normals",
           {"Matrix.Invert", "Matrix.Transpose", "Vector3.TransformNormal"}),
    };
    return w;
}

/** One lab room: same shape for every maths room, so the data stays a table. */
RoomSpec Lab(const char* key, const char* title, const char* subtitle, const char* exhibit, const char* mode, std::vector<std::string> apis)
{
    RoomSpec r;
    r.key = key;
    r.title = title;
    r.subtitle = subtitle;
    r.exhibit = exhibit;
    r.params = {{"mode", mode}};
    r.width = 13;
    r.depth = 13;
    r.theme = "white-cube";
    r.apis = std::move(apis);
    return r;
}

WingSpec RotationWing()
{
    WingSpec w;
    w.name = "Rotation and Interpolation";
    w.tagline = "Quaternions, Euler angles, easing functions and curves";
    w.theme = "corridor-warm";
    w.accent = {0.98f, 0.70f, 0.35f};
    w.spineSide = Wall::East;
    w.rooms = {
        Lab("quat-euler", "Euler Angles and Gimbal Lock", "Yaw, pitch and roll as a quaternion and as three matrices, and where they stop agreeing", "quaternions", "euler",
            {"Quaternion.CreateFromYawPitchRoll", "Matrix.CreateFromQuaternion", "Matrix.CreateRotationX"}),
        Lab("quat-slerp", "Slerp and Lerp", "Quaternion.Slerp turns at constant speed; Lerp then Normalize does not", "quaternions", "slerp",
            {"Quaternion.Slerp", "Quaternion.Lerp", "Quaternion.Normalize", "Quaternion.Dot"}),
        Lab("quat-axis", "Axis and Angle", "Quaternion.CreateFromAxisAngle, Concatenate and Inverse", "quaternions", "axisangle",
            {"Quaternion.CreateFromAxisAngle", "Quaternion.Concatenate", "Quaternion.Inverse", "Quaternion.Conjugate"}),
        Lab("curve-easing", "Easing Functions", "MathHelper.Lerp, SmoothStep, Hermite and CatmullRom graphed side by side", "curves", "easing",
            {"MathHelper.Lerp", "MathHelper.SmoothStep", "MathHelper.Hermite", "MathHelper.CatmullRom"}),
        Lab("curve-spline", "Splines", "Vector3.CatmullRom through four control points with a rider", "curves", "spline", {"Vector3.CatmullRom"}),
        Lab("curve-class", "The Curve Class", "Curve, CurveKey, tangents, continuity and loop types", "curves", "curve", {"Curve", "CurveKey", "CurveLoopType", "CurveTangent", "CurveContinuity", "CurveKeyCollection", "CurveKey.TangentIn", "CurveKey.TangentOut", "Curve.IsConstant", "Curve.PreLoop", "Curve.PostLoop", "Curve.ComputeTangents", "Curve.ComputeTangent"}),
        Lab("curve-barycentric", "Barycentric Coordinates", "Vector3.Barycentric weights inside a triangle", "curves", "barycentric", {"Vector3.Barycentric"}),
    };
    return w;
}

WingSpec CollisionWing()
{
    WingSpec w;
    w.name = "Collision and Geometry";
    w.tagline = "BoundingBox, BoundingSphere, BoundingFrustum, Plane and Ray";
    w.theme = "corridor-dark";
    w.accent = {0.95f, 0.45f, 0.45f};
    w.spineSide = Wall::West;
    w.rooms = {
        Lab("col-box-box", "Box vs Box", "BoundingBox.Intersects and Contains between two axis-aligned boxes", "collision", "box-box", {"BoundingBox.Intersects", "BoundingBox.Contains", "ContainmentType"}),
        Lab("col-sphere-box", "Sphere vs Box", "BoundingSphere.Intersects and Contains against a box", "collision", "sphere-box", {"BoundingSphere.Intersects", "BoundingSphere.Contains"}),
        Lab("col-ray-box", "Ray vs Box", "Ray.Intersects(BoundingBox): the distance to the entry point", "collision", "ray-box", {"Ray.Intersects", "BoundingBox"}),
        Lab("col-ray-sphere", "Ray vs Sphere", "Ray.Intersects(BoundingSphere) with a movable aim", "collision", "ray-sphere", {"Ray.Intersects", "BoundingSphere"}),
        Lab("col-ray-plane", "Ray vs Plane", "Ray.Intersects(Plane): crossing distance, parallel and backward rays", "collision", "ray-plane", {"Ray.Intersects", "Plane"}),
        Lab("col-plane-box", "Which Side of the Plane", "Plane.Intersects(BoundingBox) classifies five boxes as Front, Back or Intersecting", "collision", "plane-box", {"Plane.Intersects", "Plane.DotCoordinate", "Plane.DotNormal", "PlaneIntersectionType"}),
        Lab("col-merge", "Fitting and Merging Bounds", "CreateFromPoints for boxes and spheres, CreateMerged and the sphere around a box", "collision", "merge", {"BoundingBox.CreateFromPoints", "BoundingSphere.CreateFromPoints", "BoundingSphere.CreateMerged", "BoundingSphere.CreateFromBoundingBox", "BoundingBox.CreateFromSphere"}),
        Lab("col-frustum", "Frustum Culling", "BoundingFrustum.Contains: fully inside, partly inside or culled, with the frustum's corners and its own sphere", "collision", "frustum", {"BoundingFrustum.Contains", "BoundingFrustum", "BoundingFrustum.GetCorners", "BoundingSphere.CreateFromFrustum", "Matrix.CreatePerspectiveFieldOfView"}),
        Lab("col-pick", "Picking", "Your view ray tested with Ray.Intersects against boxes and spheres", "collision", "pick", {"Ray.Intersects", "BoundingBox", "BoundingSphere"}),
        // Appended last so no room before it moves: every by-reference overload checked against its value twin.
        Lab("math-checks", "Math, Checked", "Every by-reference overload of the math types against its value twin, bit for bit, and XNA's answers at the edges", "math-checks", "",
            {"Vector2.Add", "Vector2.Barycentric", "Vector2.CatmullRom", "Vector2.Clamp", "Vector2.Distance", "Vector2.DistanceSquared", "Vector2.Divide", "Vector2.Dot", "Vector2.Hermite", "Vector2.Lerp", "Vector2.Max", "Vector2.Min", "Vector2.Multiply", "Vector2.Negate", "Vector2.Normalize", "Vector2.Reflect", "Vector2.SmoothStep", "Vector2.Subtract", "Vector2.Transform", "Vector2.TransformNormal", "Vector3.Add", "Vector3.Barycentric", "Vector3.CatmullRom", "Vector3.Clamp", "Vector3.Cross", "Vector3.Distance", "Vector3.DistanceSquared", "Vector3.Divide", "Vector3.Dot", "Vector3.Hermite", "Vector3.Lerp", "Vector3.Max", "Vector3.Min", "Vector3.Multiply", "Vector3.Negate", "Vector3.Normalize", "Vector3.Reflect", "Vector3.SmoothStep", "Vector3.Subtract", "Vector3.Transform", "Vector3.TransformNormal", "Vector4.Add", "Vector4.Barycentric", "Vector4.CatmullRom", "Vector4.Clamp", "Vector4.Distance", "Vector4.DistanceSquared", "Vector4.Divide", "Vector4.Dot", "Vector4.Hermite", "Vector4.Lerp", "Vector4.Max", "Vector4.Min", "Vector4.Multiply", "Vector4.Negate", "Vector4.Normalize", "Vector4.SmoothStep", "Vector4.Subtract", "Vector4.Transform", "Matrix.Add", "Matrix.CreateBillboard", "Matrix.CreateConstrainedBillboard", "Matrix.CreateFromAxisAngle", "Matrix.CreateFromQuaternion", "Matrix.CreateFromYawPitchRoll", "Matrix.CreateLookAt", "Matrix.CreateOrthographic", "Matrix.CreateOrthographicOffCenter", "Matrix.CreatePerspective", "Matrix.CreatePerspectiveFieldOfView", "Matrix.CreatePerspectiveOffCenter", "Matrix.CreateRotationX", "Matrix.CreateRotationY", "Matrix.CreateRotationZ", "Matrix.CreateScale", "Matrix.CreateShadow", "Matrix.CreateTranslation", "Matrix.CreateReflection", "Matrix.CreateWorld", "Matrix.Divide", "Matrix.Invert", "Matrix.Lerp", "Matrix.Multiply", "Matrix.Negate", "Matrix.Subtract", "Matrix.Transpose", "Matrix.Transform", "Quaternion.Add", "Quaternion.Concatenate", "Quaternion.Conjugate", "Quaternion.CreateFromAxisAngle", "Quaternion.CreateFromRotationMatrix", "Quaternion.CreateFromYawPitchRoll", "Quaternion.Divide", "Quaternion.Dot", "Quaternion.Inverse", "Quaternion.Lerp", "Quaternion.Slerp", "Quaternion.Subtract", "Quaternion.Multiply", "Quaternion.Negate", "Quaternion.Normalize", "BoundingBox.Contains", "BoundingBox.Intersects", "BoundingBox.CreateFromSphere", "BoundingBox.CreateMerged", "BoundingFrustum.Contains", "BoundingFrustum.Intersects", "BoundingSphere.Transform", "BoundingSphere.Contains", "BoundingSphere.CreateFromBoundingBox", "BoundingSphere.CreateMerged", "BoundingSphere.Intersects", "Plane.Dot", "Plane.DotCoordinate", "Plane.DotNormal", "Plane.Intersects", "Plane.Normalize", "Plane.Transform", "Ray.Intersects", "Rectangle.Contains", "Rectangle.Intersects", "Rectangle.Intersect", "Rectangle.Union", "MathHelper.Clamp", "MathHelper.Lerp", "MathHelper.WrapAngle", "ContainmentType", "PlaneIntersectionType"}),
    };
    return w;
}

WingSpec CameraWing()
{
    WingSpec w;
    w.name = "Cameras";
    w.tagline = "Orbit, chase, top-down, security, split-screen and mirror cameras";
    w.theme = "corridor-cool";
    w.accent = {0.75f, 0.60f, 0.98f};
    w.spineSide = Wall::East;
    w.rooms = {
        Lab("cam-orbit", "Orbit Camera", "A camera circling a target: CreateLookAt from distance, elevation and angle", "cameras", "orbit", {"Matrix.CreateLookAt", "RenderTarget2D", "BoundingFrustum"}),
        Lab("cam-chase", "Chase Camera", "A spring-smoothed third-person camera following a moving car", "cameras", "chase", {"Matrix.CreateLookAt", "Vector3.Lerp", "GameTime"}),
        Lab("cam-topdown", "Top-Down and Orthographic", "A map camera with a switch between orthographic and perspective", "cameras", "topdown", {"Matrix.CreateOrthographic", "Matrix.CreateLookAt"}),
        Lab("cam-security", "Security Camera", "A fixed camera that pans and zooms, with its frustum drawn in the scene", "cameras", "security", {"Matrix.CreatePerspectiveFieldOfView", "BoundingFrustum"}),
        Lab("cam-fov", "Field of View", "Two cameras, one position, two fields of view side by side", "cameras", "fov", {"Matrix.CreatePerspectiveFieldOfView", "RenderTarget2D"}),
        Lab("cam-clip", "Near and Far Planes", "Clip planes, depth precision and z-fighting with a large far/near ratio", "cameras", "clip", {"Matrix.CreatePerspectiveFieldOfView", "DepthFormat", "DepthStencilState"}),
        Lab("cam-transition", "Camera Transitions", "Moving between viewpoints with Vector3.SmoothStep and Quaternion.Slerp", "cameras", "transition", {"Vector3.SmoothStep", "Quaternion.Slerp", "Matrix.CreateWorld"}),
        Lab("cam-split", "Split Screen", "GraphicsDevice.Viewport used to draw two cameras into one render target", "cameras", "split", {"Viewport", "GraphicsDevice.Viewport", "RenderTarget2D"}),
        Lab("cam-mirror", "Rear-View Mirror", "A camera at your eyes looking backwards on a mirrored screen", "cameras", "mirror", {"RenderTarget2D", "Matrix.CreateLookAt"}),
    };
    return w;
}


RoomSpec ServicesRoom()
{
    RoomSpec r = Lab("app-services", "Game Services", "GameServiceContainer: finding shared objects by type, with and without a registration", "applab", "components",
                     {"GameServiceContainer", "GameServiceContainer.AddService", "GameServiceContainer.GetService", "GameServiceContainer.RemoveService", "Game.Services", "IGraphicsDeviceService"});
    r.exhibit = "services-lab";
    r.params = {};
    return r;
}

WingSpec AppWing()
{
    WingSpec w;
    w.name = "Application Model";
    w.tagline = "Game, GameTime, the device, the back buffer, content and components";
    w.theme = "corridor-cool";
    w.accent = {0.40f, 0.75f, 0.95f};
    w.spineSide = Wall::West;
    w.rooms = {
        Lab("app-lifecycle", "Game Lifecycle", "Initialize, LoadContent, Update, Draw and UnloadContent with live counters and a fixed/variable step switch", "applab", "lifecycle",
            {"Game", "GameTime", "Game.IsFixedTimeStep", "Game.TargetElapsedTime", "Game.LaunchParameters", "LaunchParameters"}),
        Lab("app-gametime", "GameTime and Frame Rate", "Time-based versus frame-based movement, with an artificial slowdown and a frame-time graph", "applab", "gametime",
            {"GameTime.ElapsedGameTime", "GameTime.TotalGameTime", "Game.ResetElapsedTime", "Game.SuppressDraw", "Game.InactiveSleepTime"}),
        Lab("app-device", "Device and Renderer Report", "The live GraphicsDevice, adapter, GraphicsCapability list and renderer capability profile", "applab", "device",
            {"GraphicsDevice", "GraphicsAdapter", "GraphicsCapability", "RendererCapabilityProfile", "PresentationParameters"}),
        Lab("app-viewport", "Back Buffer, Viewport and Fullscreen", "GraphicsDeviceManager.ApplyChanges: resolution, fullscreen and vertical sync at run time", "applab", "viewport",
            {"GraphicsDeviceManager", "PresentationParameters", "PresentInterval", "GraphicsDeviceStatus", "GraphicsDeviceManager.DeviceReset", "GraphicsDeviceManager.DeviceResetting", "GraphicsDevice.DeviceLost", "GraphicsDevice.Present", "GraphicsDevice.VertexTextures", "GraphicsDeviceManager.PreparingDeviceSettings", "Viewport", "Viewport.MinDepth", "Viewport.MaxDepth", "Viewport.Project", "GameWindow.ClientBounds"}),
        Lab("app-content", "Content Lifetime", "ContentManager.Load, caching, Unload and ContentLoadException with your own manager", "applab", "content",
            {"ContentManager", "ContentManager.Load", "ContentManager.Unload", "ContentLoadException", "Texture2D"}),
        Lab("app-components", "Game Components", "GameComponent and DrawableGameComponent with UpdateOrder, DrawOrder, Enabled and Visible", "applab", "components",
            {"GameComponent", "DrawableGameComponent", "Game.Components", "GameComponentCollection.ComponentAdded", "GameComponentCollection.ComponentRemoved", "GameComponentCollectionEventArgs", "GameComponentCollectionEventArgs.GameComponent", "IGameComponent", "IGameComponent.Initialize", "IUpdateable", "IDrawable"}),
        ServicesRoom(),
    };
    return w;
}

}  // namespace

FloorSpec MakeFloor1()
{
    FloorSpec f;
    f.index = 1;
    f.name = "Fundamentals";
    f.tagline = "The application model, the maths of games, geometry tests and cameras";
    f.theme = "concourse";
    f.accent = {0.40f, 0.75f, 0.95f};
    f.wings.push_back(AppWing());
    f.wings.push_back(MathWing());
    f.wings.push_back(TransformWing());
    f.wings.push_back(RotationWing());
    f.wings.push_back(CollisionWing());
    f.wings.push_back(CameraWing());
    f.wings.push_back(ValueTypeWing());
    return f;
}

# Museum map

The museum is **data**: floors, wings and rooms are described in `src/Catalogue/FloorN.cpp`, laid out automatically by
`src/World/Layout.cpp` and built lazily around the visitor (`src/World/MuseumWorld.cpp`). This page is generated from that
catalogue by `tools/gen_docs.sh`; the plan below (hand-written) explains the shape every floor shares.

## Shape of a floor

```
                       wing galleries (one per wing, rooms on both sides)
   west wings  <--------- concourse (spine, runs north) --------->  east wings
                              |
 adjacent-floor stairs -- floor hall (hub) -- elevator car (10 buttons, one per floor)
```

- Every floor has a **hall** (the hub you arrive in from the elevator), a **concourse** that runs north from it,
  and **wings**: a gallery corridor off the concourse with exhibit rooms on both sides.
- Room numbers read `floor.wing.room`, for example `4.2.07`. They are printed on the door-side plaque of every room,
  on the lintel signs of the gallery, on the floor map and in the directory (`M` or `Tab`).
- Each floor has its own accent colour (floor numerals, signs, map highlight).
- Walkable switchback stairs connect every adjacent pair of floors; elevators connect all ten directly. The directory
  can also teleport to any room on a floor you have already visited (floor 0 is always open).

## What each floor is for

| Floor | Name | Subject |
|---|---|---|
| 0 | Lobby and Introductory Tour | the entrance hall and the first eleven showcase rooms |
| 1 | Fundamentals | application model, vectors, matrices, rotations and curves, collision, cameras |
| 2 | Two-Dimensional Rendering | SpriteBatch, blending and sorting, sprite techniques, SpriteFont and text |
| 3 | Three-Dimensional Fundamentals | vertex and index buffers, primitive types, dynamic geometry, instancing; textures |
| 4 | Effects and Lighting | BasicEffect in depth, the lighting lab, CNA ShaderEffect shaders |
| 5 | Render State and Render Targets | Rasterizer/DepthStencil/Blend states, render targets, mirrors, MRT |
| 6 | Models, Animation and Particles | glTF models, skeletal and rigid animation, particle systems |
| 7 | Audio and Input | SoundEffect, 3D audio, synthesis, music; keyboard, mouse, pad, text input |
| 8 | Content, Platform and CNA | content pipeline, CNA extensions (DebugDraw, CRT, ASCII...), platform queries |
| 9 | Performance and Validation | stress labs, renderer self-checks, reference scenes, the observation deck |

## The rooms

<!-- BEGIN MAP -->
10 floors, 36 wings, 295 exhibit rooms (7 open to the sky), 370 walkable cells in all.

### Floor 0: Lobby and Introductory Tour

Start here: a first look at what CNA can do

**First Steps** - What CNA is, models, textures, lighting and the sky

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 0.1.01 | `info` | CNA Information Hall | XNA 4.0 | GraphicsDevice |
| 0.1.02 | `models` | Model Gallery | XNA 4.0 | Model, ModelMesh, ContentManager, Matrix |
| 0.1.03 | `materials` | Materials & Textures | XNA 4.0 | Texture2D, SamplerState, SamplerStateCollection |
| 0.1.04 | `lighting` | Lighting Room | XNA 4.0 | BasicEffect, DirectionalLight, BasicEffect.PreferPerPixelLighting |
| 0.1.05 | `sky` | Sky Courtyard | XNA 4.0 | BlendState, DepthStencilState |
| 0.1.06 | `grounds-sculpture` | Sculpture Garden | XNA 4.0 | BasicEffect.World, BasicEffect.EmissiveColor, Matrix.CreateRotationY, Matrix.CreateScale |
| 0.1.07 | `grounds-fountain` | Fountain Plaza | XNA 4.0 | DynamicVertexBuffer, BlendState.NonPremultiplied, Texture2D, SoundEffectInstance |

**Showcases** - Render state, render targets, particles, animation and audio

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 0.2.01 | `renderstates` | Render State Room | XNA 4.0 | DepthStencilState, RasterizerState, BlendState |
| 0.2.02 | `rendertarget` | Render Target Room | XNA 4.0 | RenderTarget2D, GraphicsDevice.SetRenderTarget |
| 0.2.03 | `particles` | Particle Room | XNA 4.0 | DynamicVertexBuffer, IndexBuffer, BlendState |
| 0.2.04 | `animation` | Animation Room | XNA 4.0 | Matrix |
| 0.2.05 | `audio` | Audio Room | XNA 4.0 | SoundEffect, SoundEffectInstance, AudioEmitter, AudioListener |

### Floor 1: Fundamentals

The application model, the maths of games, geometry tests and cameras

**Application Model** - Game, GameTime, the device, the back buffer, content and components

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 1.1.01 | `app-lifecycle` | Game Lifecycle | XNA 4.0 | Game, GameTime, Game.IsFixedTimeStep, Game.TargetElapsedTime, Game.LaunchParameters, LaunchParameters |
| 1.1.02 | `app-gametime` | GameTime and Frame Rate | XNA 4.0 | GameTime.ElapsedGameTime, GameTime.TotalGameTime, Game.ResetElapsedTime, Game.SuppressDraw, Game.InactiveSleepTime |
| 1.1.03 | `app-device` | Device and Renderer Report | XNA 4.0 | GraphicsDevice, GraphicsAdapter, GraphicsCapability, RendererCapabilityProfile, PresentationParameters |
| 1.1.04 | `app-viewport` | Back Buffer, Viewport and Fullscreen | XNA 4.0 | GraphicsDeviceManager, PresentationParameters, PresentInterval, GraphicsDeviceStatus, GraphicsDeviceManager.DeviceReset, GraphicsDeviceManager.DeviceResetting, GraphicsDevice.DeviceLost, GraphicsDevice.Present, GraphicsDevice.VertexTextures, GraphicsDeviceManager.PreparingDeviceSettings, Viewport, Viewport.MinDepth, Viewport.MaxDepth, Viewport.Project, GameWindow.ClientBounds |
| 1.1.05 | `app-content` | Content Lifetime | XNA 4.0 | ContentManager, ContentManager.Load, ContentManager.Unload, ContentLoadException, Texture2D |
| 1.1.06 | `app-components` | Game Components | XNA 4.0 | GameComponent, DrawableGameComponent, Game.Components, GameComponentCollection.ComponentAdded, GameComponentCollection.ComponentRemoved, GameComponentCollectionEventArgs, GameComponentCollectionEventArgs.GameComponent, IGameComponent, IGameComponent.Initialize, IUpdateable, IDrawable |
| 1.1.07 | `app-services` | Game Services | XNA 4.0 | GameServiceContainer, GameServiceContainer.AddService, GameServiceContainer.GetService, GameServiceContainer.RemoveService, Game.Services, IGraphicsDeviceService |

**Vector Mathematics** - Vector3: arithmetic, products, reflection and interpolation

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 1.2.01 | `vec-add` | Adding Vectors | XNA 4.0 | Vector3.Add, Vector3.Subtract |
| 1.2.02 | `vec-dot` | The Dot Product | XNA 4.0 | Vector3.Dot |
| 1.2.03 | `vec-cross` | The Cross Product | XNA 4.0 | Vector3.Cross, Vector3.Length |
| 1.2.04 | `vec-project` | Projection and Rejection | XNA 4.0 | Vector3.Dot, Vector3.Multiply |
| 1.2.05 | `vec-reflect` | Reflection | XNA 4.0 | Vector3.Reflect, Vector3.Normalize, Plane |
| 1.2.06 | `vec-lerp` | Interpolation | XNA 4.0 | Vector3.Lerp, Vector3.SmoothStep, Vector3.CatmullRom |
| 1.2.07 | `vec-normalize` | Length and Normalize | XNA 4.0 | Vector3.Length, Vector3.Normalize, Vector3.Distance |

**Matrices and Transforms** - World, view and projection: how a point reaches the screen

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 1.3.01 | `mat-order` | Transform Order | XNA 4.0 | Matrix.CreateScale, Matrix.CreateRotationY, Matrix.CreateTranslation, Matrix.Multiply |
| 1.3.02 | `mat-hierarchy` | Parent and Child | XNA 4.0 | Matrix.Multiply, Matrix.Decompose, Matrix.Translation |
| 1.3.03 | `mat-view` | The View Matrix | XNA 4.0 | Matrix.CreateLookAt, Matrix.Invert, BoundingFrustum |
| 1.3.04 | `mat-perspective` | Perspective Projection | XNA 4.0 | Matrix.CreatePerspectiveFieldOfView, BoundingFrustum.Contains |
| 1.3.05 | `mat-ortho` | Orthographic Projection | XNA 4.0 | Matrix.CreateOrthographic, Matrix.CreatePerspectiveFieldOfView |
| 1.3.06 | `mat-screen` | World to Screen | XNA 4.0 | Viewport.Project, Viewport.Unproject, Vector4.Transform, Matrix.Multiply |
| 1.3.07 | `mat-offcenter` | Off-Centre Projections | XNA 4.0 | Matrix.CreatePerspective, Matrix.CreatePerspectiveOffCenter, Matrix.CreateOrthographicOffCenter |
| 1.3.08 | `mat-billboard` | Billboards | XNA 4.0 | Matrix.CreateBillboard, Matrix.CreateConstrainedBillboard |
| 1.3.09 | `mat-reflection` | Reflection Matrices | XNA 4.0 | Matrix.CreateReflection, Plane.Transform, Matrix.Determinant |
| 1.3.10 | `mat-normals` | Normals and the Inverse Transpose | XNA 4.0 | Matrix.Invert, Matrix.Transpose, Vector3.TransformNormal |

**Rotation and Interpolation** - Quaternions, Euler angles, easing functions and curves

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 1.4.01 | `quat-euler` | Euler Angles and Gimbal Lock | XNA 4.0 | Quaternion.CreateFromYawPitchRoll, Matrix.CreateFromQuaternion, Matrix.CreateRotationX |
| 1.4.02 | `quat-slerp` | Slerp and Lerp | XNA 4.0 | Quaternion.Slerp, Quaternion.Lerp, Quaternion.Normalize, Quaternion.Dot |
| 1.4.03 | `quat-axis` | Axis and Angle | XNA 4.0 | Quaternion.CreateFromAxisAngle, Quaternion.Concatenate, Quaternion.Inverse, Quaternion.Conjugate |
| 1.4.04 | `curve-easing` | Easing Functions | XNA 4.0 | MathHelper.Lerp, MathHelper.SmoothStep, MathHelper.Hermite, MathHelper.CatmullRom |
| 1.4.05 | `curve-spline` | Splines | XNA 4.0 | Vector3.CatmullRom |
| 1.4.06 | `curve-class` | The Curve Class | XNA 4.0 | Curve, CurveKey, CurveLoopType, CurveTangent, CurveContinuity, CurveKeyCollection, CurveKey.TangentIn, CurveKey.TangentOut, Curve.IsConstant, Curve.PreLoop, Curve.PostLoop, Curve.ComputeTangents, Curve.ComputeTangent |
| 1.4.07 | `curve-barycentric` | Barycentric Coordinates | XNA 4.0 | Vector3.Barycentric |

**Collision and Geometry** - BoundingBox, BoundingSphere, BoundingFrustum, Plane and Ray

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 1.5.01 | `col-box-box` | Box vs Box | XNA 4.0 | BoundingBox.Intersects, BoundingBox.Contains, ContainmentType |
| 1.5.02 | `col-sphere-box` | Sphere vs Box | XNA 4.0 | BoundingSphere.Intersects, BoundingSphere.Contains |
| 1.5.03 | `col-ray-box` | Ray vs Box | XNA 4.0 | Ray.Intersects, BoundingBox |
| 1.5.04 | `col-ray-sphere` | Ray vs Sphere | XNA 4.0 | Ray.Intersects, BoundingSphere |
| 1.5.05 | `col-ray-plane` | Ray vs Plane | XNA 4.0 | Ray.Intersects, Plane |
| 1.5.06 | `col-plane-box` | Which Side of the Plane | XNA 4.0 | Plane.Intersects, Plane.DotCoordinate, Plane.DotNormal, PlaneIntersectionType |
| 1.5.07 | `col-merge` | Fitting and Merging Bounds | XNA 4.0 | BoundingBox.CreateFromPoints, BoundingSphere.CreateFromPoints, BoundingSphere.CreateMerged, BoundingSphere.CreateFromBoundingBox, BoundingBox.CreateFromSphere |
| 1.5.08 | `col-frustum` | Frustum Culling | XNA 4.0 | BoundingFrustum.Contains, BoundingFrustum, BoundingFrustum.GetCorners, BoundingSphere.CreateFromFrustum, Matrix.CreatePerspectiveFieldOfView |
| 1.5.09 | `col-pick` | Picking | XNA 4.0 | Ray.Intersects, BoundingBox, BoundingSphere |

**Cameras** - Orbit, chase, top-down, security, split-screen and mirror cameras

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 1.6.01 | `cam-orbit` | Orbit Camera | XNA 4.0 | Matrix.CreateLookAt, RenderTarget2D, BoundingFrustum |
| 1.6.02 | `cam-chase` | Chase Camera | XNA 4.0 | Matrix.CreateLookAt, Vector3.Lerp, GameTime |
| 1.6.03 | `cam-topdown` | Top-Down and Orthographic | XNA 4.0 | Matrix.CreateOrthographic, Matrix.CreateLookAt |
| 1.6.04 | `cam-security` | Security Camera | XNA 4.0 | Matrix.CreatePerspectiveFieldOfView, BoundingFrustum |
| 1.6.05 | `cam-fov` | Field of View | XNA 4.0 | Matrix.CreatePerspectiveFieldOfView, RenderTarget2D |
| 1.6.06 | `cam-clip` | Near and Far Planes | XNA 4.0 | Matrix.CreatePerspectiveFieldOfView, DepthFormat, DepthStencilState |
| 1.6.07 | `cam-transition` | Camera Transitions | XNA 4.0 | Vector3.SmoothStep, Quaternion.Slerp, Matrix.CreateWorld |
| 1.6.08 | `cam-split` | Split Screen | XNA 4.0 | Viewport, GraphicsDevice.Viewport, RenderTarget2D |
| 1.6.09 | `cam-mirror` | Rear-View Mirror | XNA 4.0 | RenderTarget2D, Matrix.CreateLookAt |

**Value Types** - Packed vectors, Rectangle and Point, Color: the small structures everything else is built from

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 1.7.01 | `vt-packed` | Packed Vector Formats | XNA 4.0 | IPackedVector, IPackedVectorT.PackedValue, Byte4, NormalizedByte4, NormalizedShort4, Short4, HalfVector4, Rgba1010102, Rgba64, Bgra4444, Bgra5551, Bgr565, Rg32, Short2, NormalizedShort2, NormalizedByte2, HalfVector2 |
| 1.7.02 | `vt-rectangle` | Rectangle and Point | XNA 4.0 | Rectangle, Rectangle.Intersects, Rectangle.Intersect, Rectangle.Union, Rectangle.Contains, Rectangle.Inflate, Rectangle.Bottom, Rectangle.Location, Rectangle.IsEmpty, Point |
| 1.7.03 | `vt-color` | Colour | XNA 4.0 | Color, Color.Lerp, Color.Multiply, Color.FromNonPremultiplied, Color.PackedValue, Color.ToVector3, Color.PackFromVector4 |
| 1.7.04 | `vt-colors` | The 141 Named Colours | XNA 4.0 | Color, Color.Transparent, Color.AliceBlue, Color.CornflowerBlue, Color.Gold, Color.Red, Color.Green, Color.Blue, Color.White, Color.Black, Color.YellowGreen, Color.R, Color.G, Color.B, Color.A, Color.PackedValue, Color.ToString |
| 1.7.05 | `vt-converters` | Type Converters | XNA 4.0 | Vector3Converter, Vector2Converter, Vector4Converter, QuaternionConverter, ColorConverter, RectangleConverter, PointConverter, BoundingBoxConverter, BoundingSphereConverter, PlaneConverter, RayConverter, MatrixConverter, MathTypeConverter, MathTypeConverter.CanConvertFrom, MathTypeConverter.GetProperties, Vector3Converter.ConvertTo, Vector3Converter.ConvertFrom |

### Floor 2: Two-Dimensional Rendering

SpriteBatch, SpriteFont and the techniques built on them

**SpriteBatch Basics** - Position, rotation, origin, scale, flips, source rectangles, tint

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 2.1.01 | `sprite-basics` | Sprites: Draw and Its Arguments | XNA 4.0 | SpriteBatch.Begin, SpriteBatch.Draw, SpriteEffects, RenderTarget2D |
| 2.1.02 | `sprite-origin` | Origin and Rotation | XNA 4.0 | SpriteBatch.Draw, Vector2 |
| 2.1.03 | `sprite-sheet` | Sprite Sheets | XNA 4.0 | SpriteBatch.Draw, Rectangle, SamplerState.PointClamp |
| 2.1.04 | `sprite-tint` | Tint and Alpha | XNA 4.0 | Color, Color.Lerp, BlendState.NonPremultiplied |

**Blending, Sorting and Sampling** - BlendState, SpriteSortMode and SamplerState compared visually

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 2.2.01 | `sprite-blend` | BlendState Gallery | XNA 4.0 | BlendState, BlendState.Additive, BlendState.AlphaBlend, Blend, BlendFunction |
| 2.2.02 | `sprite-sort` | SpriteSortMode and Layer Depth | XNA 4.0 | SpriteSortMode, SpriteBatch.Draw |
| 2.2.03 | `sprite-sampler` | Filtering and Addressing | XNA 4.0 | SamplerState, TextureAddressMode |

**Sprite Techniques** - Parallax, UI nine-slice, atlases, scissoring, 2D cameras and batching

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 2.3.01 | `sprite-parallax` | Scrolling and Parallax | XNA 4.0 | SamplerState.LinearWrap, SpriteBatch.Draw |
| 2.3.02 | `sprite-ui` | UI Panels and Nine-Slice | XNA 4.0 | Texture2D.SetData, SpriteBatch.Draw, Rectangle |
| 2.3.03 | `sprite-atlas` | Texture Atlases | XNA 4.0 | Texture2D.GetData, Texture2D.SetData, SpriteBatch.Draw |
| 2.3.04 | `sprite-scissor` | Scissor Clipping | XNA 4.0 | RasterizerState.ScissorTestEnable, GraphicsDevice.ScissorRectangle |
| 2.3.05 | `sprite-camera` | A 2D Camera Matrix | XNA 4.0 | SpriteBatch.Begin, Matrix.CreateTranslation, Matrix.CreateScale, Matrix.CreateRotationZ |
| 2.3.06 | `sprite-hit` | Per-Pixel Collision | XNA 4.0 | Rectangle.Intersects, Rectangle.Intersect, Texture2D.GetData, SpriteBatch.Draw |
| 2.3.07 | `sprite-anim` | Sprite Animation | XNA 4.0 | SpriteBatch.Draw, Rectangle, SpriteEffects.FlipHorizontally, Texture2D.SetData |
| 2.3.08 | `sprite-tilemap` | Tile Maps and Culling | XNA 4.0 | Texture2D.GetData, SpriteBatch.Draw, Rectangle.Intersects, SpriteBatch.Begin, Matrix.CreateScale |
| 2.3.09 | `sprite-lights` | 2D Lighting with a Light Map | XNA 4.0 | RenderTarget2D, BlendState.Additive, Blend.DestinationColor, SpriteBatch.Draw, SamplerState.LinearClamp |
| 2.3.10 | `sprite-stress` | Thousands of Sprites | XNA 4.0 | SpriteBatch, SpriteBatch.Draw |

**Text and Fonts** - SpriteFont: layout, measurement, transforms, glyphs, HUDs and world labels

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 2.4.01 | `text-align` | Alignment and Wrapping | XNA 4.0 | SpriteFont.MeasureString, SpriteBatch.DrawString |
| 2.4.02 | `text-measure` | Measuring Text | XNA 4.0 | SpriteFont.MeasureString, SpriteFont.Spacing, SpriteFont.LineSpacing |
| 2.4.03 | `text-transform` | Rotating and Scaling Text | XNA 4.0 | SpriteBatch.DrawString, SpriteEffects |
| 2.4.04 | `text-glyphs` | Glyphs and the Default Character | XNA 4.0 | SpriteFont.GlyphBoundsEXT, SpriteFont.CroppingEXT, SpriteFont.KerningEXT, SpriteFont.TextureEXT, SpriteFont.Characters, SpriteFont.DefaultCharacter |
| 2.4.05 | `text-hud` | HUD Text and StringBuilder | XNA 4.0 | SpriteBatch.DrawString, System.Text.StringBuilder |
| 2.4.06 | `text-labels` | World Labels | XNA 4.0 | Viewport.Project, SpriteBatch.DrawString |

### Floor 3: Three-Dimensional Fundamentals

Vertex data, buffers, primitives and textures

**Vertices and Buffers** - Triangles, indices, primitive types, dynamic and large meshes, instancing

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 3.1.01 | `geo-triangle` | A Triangle and Its Winding | XNA 4.0 | VertexPositionColor, GraphicsDevice.DrawUserPrimitives, RasterizerState.CullMode |
| 3.1.02 | `geo-vertextypes` | Vertex Types | XNA 4.0 | VertexDeclaration, VertexDeclaration.GetVertexElements, VertexDeclaration.VertexStride, VertexElement, VertexElementFormat, VertexElementUsage, VertexPositionColor, VertexPositionTexture, VertexPositionColorTexture, VertexPositionNormalTexture, VertexBuffer, BasicEffect.VertexColorEnabled, BasicEffect.TextureEnabled |
| 3.1.03 | `geo-indexed` | Vertex and Index Buffers | XNA 4.0 | VertexBuffer, IndexBuffer, GraphicsDevice.DrawIndexedPrimitives, GraphicsDevice.DrawPrimitives |
| 3.1.04 | `geo-primitives` | Primitive Types | XNA 4.0 | PrimitiveType, GraphicsDevice.DrawUserPrimitives |
| 3.1.05 | `geo-dynamic` | Dynamic Vertex Buffers | XNA 4.0 | DynamicVertexBuffer, SetDataOptions, VertexPositionNormalTexture, GraphicsDevice.DrawUserIndexedPrimitives |
| 3.1.06 | `geo-procedural` | Procedural Geometry | XNA 4.0 | VertexBuffer, IndexBuffer, RasterizerState.FillMode |
| 3.1.07 | `geo-index32` | 32-Bit Indices | XNA 4.0 | IndexBuffer, IndexElementSize |
| 3.1.08 | `geo-manybuffers` | Many Small Buffers | XNA 4.0 | VertexBuffer, GraphicsDevice.DrawIndexedPrimitives |
| 3.1.09 | `geo-instancing` | Hardware Instancing | XNA 4.0 | VertexBufferBinding, GraphicsDevice.GetVertexBuffers, GraphicsDevice.SetVertexBuffers, GraphicsDevice.DrawInstancedPrimitives, GraphicsCapability.Instancing, GraphicsDevice.DrawInstancedPrimitivesBaseInstanceEXT, RendererFeature.BaseInstanceDrawing |
| 3.1.10 | `geo-dynindex` | Dynamic Index Buffers | XNA 4.0 | DynamicIndexBuffer, SetDataOptions.Discard, GraphicsDevice.DrawIndexedPrimitives, IndexBuffer |
| 3.1.11 | `geo-terrain` | Heightmap Terrain | XNA 4.0 | Texture2D.GetData, VertexBuffer, IndexBuffer, VertexPositionNormalTexture, GraphicsDevice.DrawIndexedPrimitives, RasterizerState.FillMode, MathHelper.Lerp |

**Textures** - Mip levels, filters, addressing, UVs, formats, alpha, cube and volume textures, image encode/decode

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 3.2.01 | `tex-mip` | Mip Levels and Filtering | XNA 4.0 | Texture2D, SamplerState.Filter, TextureFilter.MinLinearMagPointMipLinear, TextureFilter.MinPointMagLinearMipPoint, SamplerState.MipMapLevelOfDetailBias, SamplerState.MaxMipLevel, SamplerState.MaxAnisotropy |
| 3.2.02 | `tex-filter` | Magnification Filters | XNA 4.0 | SamplerState.PointClamp, SamplerState.LinearClamp, SamplerState.AnisotropicClamp |
| 3.2.03 | `tex-address` | Addressing Modes | XNA 4.0 | TextureAddressMode, SamplerState.AddressU |
| 3.2.04 | `tex-uv` | UV Transforms | XNA 4.0 | VertexPositionColorTexture, SamplerState.LinearWrap |
| 3.2.05 | `tex-dynamic` | Dynamic Textures and Readback | XNA 4.0 | Texture2D.SetData, Texture2D.GetData |
| 3.2.06 | `tex-formats` | Surface Formats | XNA 4.0 | SurfaceFormat, PackedVector.Bgr565, PackedVector.Bgra4444, PackedVector.Alpha8, PackedVector.HalfVector4 |
| 3.2.07 | `tex-alpha` | Alpha: Test, Blend, Opaque | XNA 4.0 | AlphaTestEffect, BlendState.NonPremultiplied, CompareFunction |
| 3.2.08 | `tex-cube` | Cube Maps and Environment Maps | XNA 4.0 | TextureCube, EnvironmentMapEffect, CubeMapFace, TextureCube.SetData, TextureCube.GetData |
| 3.2.09 | `tex-volume` | Volume Textures | XNA 4.0 | Texture3D, Texture3D.SetData, Texture3D.GetData, Texture3D.GetData(level, left, front), SurfaceFormat.Single, SurfaceFormat.Vector4 |
| 3.2.10 | `tex-volshader` | Sampling a Volume in a Shader | CNA extension | ShaderEffect.SetTexture(Texture3D), SamplerState.AddressW, SamplerState.AddressU, SamplerState.AddressV, Texture3D, TextureAddressMode, TextureFilter |
| 3.2.11 | `tex-io` | Encoding and Decoding Images | XNA 4.0 | Texture2D.SaveAsPng, Texture2D.SaveAsJpeg, Texture2D.FromStream, System.IO.MemoryStream |

### Floor 4: Effects and Lighting

BasicEffect, the stock effects, lighting and custom shaders

**BasicEffect Super-Wing** - Every BasicEffect property with a control, plus DualTextureEffect and the Effect model

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 4.1.01 | `be-combos` | Texture, Vertex Colour, Diffuse | XNA 4.0 | BasicEffect.TextureEnabled, BasicEffect.VertexColorEnabled, BasicEffect.DiffuseColor, BasicEffect.Alpha |
| 4.1.02 | `be-lights` | Lights and Ambient | XNA 4.0 | BasicEffect.LightingEnabled, BasicEffect.DirectionalLight0, BasicEffect.AmbientLightColor, DirectionalLight, BasicEffect.EnableDefaultLighting |
| 4.1.03 | `be-material` | Material | XNA 4.0 | BasicEffect.SpecularColor, BasicEffect.SpecularPower, BasicEffect.EmissiveColor |
| 4.1.04 | `be-perpixel` | Per-Vertex and Per-Pixel Lighting | XNA 4.0 | BasicEffect.PreferPerPixelLighting |
| 4.1.05 | `be-fog` | Fog | XNA 4.0 | BasicEffect.FogEnabled, BasicEffect.FogColor, BasicEffect.FogStart, BasicEffect.FogEnd |
| 4.1.06 | `be-alpha` | Alpha and Sorting | XNA 4.0 | BasicEffect.Alpha, BlendState.NonPremultiplied, DepthStencilState.DepthRead |
| 4.1.07 | `be-punctual` | Point and Spot Lights | CNA extension | PunctualLightEXT, BasicEffect.setPunctualLightEXT |
| 4.1.08 | `be-dual` | DualTextureEffect | XNA 4.0 | DualTextureEffect, DualTextureEffect.Texture2 |
| 4.1.09 | `be-model` | The Effect Object Model | XNA 4.0 | Effect, EffectParameter, EffectParameterClass, EffectParameterType, EffectParameterCollection, EffectTechnique, EffectPass, IEffectMatrices, IEffectLights, IEffectFog, Effect.Clone, SpriteEffect, SkinnedEffect, EffectParameter.GetValueInt32, EffectParameter.GetValueMatrix, EffectParameter.GetValueMatrixArray, EffectParameter.GetValueMatrixTranspose, EffectParameter.GetValueMatrixTransposeArray, EffectParameter.GetValueVector3, EffectParameter.GetValueVector4, EffectParameter.GetValueTexture2D |

**Lighting Laboratory** - Directional, point and spot lights, attenuation, colour, many lights, flashlight, emissive

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 4.2.01 | `light-types` | Directional, Point and Spot | CNA extension | DirectionalLight, PunctualLightEXT, BasicEffect |
| 4.2.02 | `light-falloff` | Attenuation and Range | CNA extension | PunctualLightEXT.Range, BasicEffect |
| 4.2.03 | `light-coloured` | Coloured Lights | CNA extension | PunctualLightEXT.DiffuseColor, BlendState.Additive |
| 4.2.04 | `light-many` | Many Lights | CNA extension | PunctualLightEXT, BlendState.Additive, DepthStencilState.DepthBufferFunction |
| 4.2.05 | `light-flashlight` | Flashlight | CNA extension | PunctualLightKindEXT.Spot, PunctualLightEXT.OuterAngle |
| 4.2.06 | `light-emissive` | Emissive Materials | XNA 4.0 | BasicEffect.EmissiveColor, BlendState.Additive |

**Shader Laboratory** - CNA extension: GLSL fragment and vertex shaders through ShaderEffect and SpriteBatch

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 4.3.01 | `fx-grade` | Colour Grading | CNA extension | ShaderEffect, ShaderEffect.SetUniformFloat, ShaderEffect.DeclareUniformBlockEXT, GraphicsDevice.GetShaderDialectEXT, GraphicsDevice.SupportsShaderLanguageEXT, SpriteBatch.Begin(effect), RenderTarget2D |
| 4.3.02 | `fx-blur` | Blur | CNA extension | ShaderEffect, ShaderEffect.SetUniformFloat, ShaderEffect.DeclareUniformBlockEXT, GraphicsDevice.GetShaderDialectEXT, GraphicsDevice.SupportsShaderLanguageEXT, SpriteBatch.Begin(effect), RenderTarget2D |
| 4.3.03 | `fx-pixelate` | Pixelate and Posterise | CNA extension | ShaderEffect, ShaderEffect.SetUniformFloat, ShaderEffect.DeclareUniformBlockEXT, GraphicsDevice.GetShaderDialectEXT, GraphicsDevice.SupportsShaderLanguageEXT, SpriteBatch.Begin(effect), RenderTarget2D |
| 4.3.04 | `fx-ripple` | Water Ripple | CNA extension | ShaderEffect, ShaderEffect.SetUniformFloat, ShaderEffect.DeclareUniformBlockEXT, GraphicsDevice.GetShaderDialectEXT, GraphicsDevice.SupportsShaderLanguageEXT, SpriteBatch.Begin(effect), RenderTarget2D |
| 4.3.05 | `fx-edge` | Edge Detection | CNA extension | ShaderEffect, ShaderEffect.SetUniformFloat, ShaderEffect.DeclareUniformBlockEXT, GraphicsDevice.GetShaderDialectEXT, GraphicsDevice.SupportsShaderLanguageEXT, SpriteBatch.Begin(effect), RenderTarget2D |
| 4.3.06 | `fx-crt` | CRT Look | CNA extension | ShaderEffect, ShaderEffect.SetUniformFloat, ShaderEffect.DeclareUniformBlockEXT, GraphicsDevice.GetShaderDialectEXT, GraphicsDevice.SupportsShaderLanguageEXT, SpriteBatch.Begin(effect), RenderTarget2D |
| 4.3.07 | `fx-plasma` | Procedural Shader | CNA extension | ShaderEffect, ShaderEffect.SetUniformFloat, ShaderEffect.DeclareUniformBlockEXT, GraphicsDevice.GetShaderDialectEXT, GraphicsDevice.SupportsShaderLanguageEXT, SpriteBatch.Begin(effect), RenderTarget2D |
| 4.3.08 | `fx-swirl` | Swirl | CNA extension | ShaderEffect, ShaderEffect.SetUniformFloat, ShaderEffect.DeclareUniformBlockEXT, GraphicsDevice.GetShaderDialectEXT, GraphicsDevice.SupportsShaderLanguageEXT, SpriteBatch.Begin(effect), RenderTarget2D |
| 4.3.09 | `fx-chromatic` | Chromatic Aberration | CNA extension | ShaderEffect, ShaderEffect.SetUniformFloat, ShaderEffect.DeclareUniformBlockEXT, GraphicsDevice.GetShaderDialectEXT, GraphicsDevice.SupportsShaderLanguageEXT, SpriteBatch.Begin(effect), RenderTarget2D |
| 4.3.10 | `fx-kernel` | Convolution Kernels | CNA extension | ShaderEffect, ShaderEffect.SetUniformFloat, ShaderEffect.DeclareUniformBlockEXT, GraphicsDevice.GetShaderDialectEXT, GraphicsDevice.SupportsShaderLanguageEXT, SpriteBatch.Begin(effect), RenderTarget2D |
| 4.3.11 | `fx-halftone` | Halftone | CNA extension | ShaderEffect, ShaderEffect.SetUniformFloat, ShaderEffect.DeclareUniformBlockEXT, GraphicsDevice.GetShaderDialectEXT, GraphicsDevice.SupportsShaderLanguageEXT, SpriteBatch.Begin(effect), RenderTarget2D |
| 4.3.12 | `fx-vertexwave` | Vertex Shader | CNA extension | ShaderEffect, ShaderEffect.SetUniformFloat, ShaderEffect.DeclareUniformBlockEXT, GraphicsDevice.GetShaderDialectEXT, GraphicsDevice.SupportsShaderLanguageEXT, SpriteBatch.Begin(effect), RenderTarget2D |
| 4.3.13 | `fx-colormatrix` | Colour Matrix | CNA extension | ShaderEffect, ShaderEffect.DeclareUniformBlockEXT, GraphicsDevice.GetShaderDialectEXT, GraphicsDevice.SupportsShaderLanguageEXT, SpriteBatch.Begin(effect), RenderTarget2D, ColorMatrixEffect, ColorMatrixEffect.SetColorMatrix, ColorMatrixEffect.SetColorOffset, Matrix |
| 4.3.14 | `fx-compiled` | Compiled XNA Effects | XNA 4.0 | Effect, Effect(GraphicsDevice, byte[]), Effect.Parameters, Effect.Techniques, EffectParameter, EffectParameter.SetValue, EffectParameter.GetValueSingle, EffectParameter.GetValueVector2, EffectTechnique, EffectPass, EffectParameterClass, EffectParameterType, GraphicsCapability.CompiledEffects, GraphicsDevice.VertexTextures, GraphicsDevice.VertexSamplerStates, SpriteBatch.Begin, SamplerStateCollection.operator[], EffectContent, EffectContent.EffectCode, EffectImporter, EffectImporter.Import, EffectProcessor, EffectProcessor.DebugMode, EffectProcessor.Defines, EffectProcessor.Process, CompiledEffectContent, CompiledEffectContent.GetEffectCode, ContentCompiler.Compile(CompiledEffectContent), ContentManager.Load<Effect> |
| 4.3.15 | `fx-errors` | Shader Compile Errors | CNA extension | ShaderEffect, ShaderEffect.IsEffectValid, ShaderEffect.GetCompileErrorEXT, ShaderEffect.GetShaderDiagnosticsEXT, CNA::ShaderDiagnosticEXT, ShaderDiagnosticEXT.parseCompilerLog, GraphicsDevice.ExecutesShaderEffectSourceEXT, GraphicsDevice.GetShaderDialectEXT |

### Floor 5: Render State and Render Targets

RasterizerState, DepthStencilState, BlendState and off-screen rendering

**Render State Gallery** - RasterizerState, DepthStencilState and BlendState, one idea per room

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 5.1.01 | `rs-cull` | Culling | XNA 4.0 | RasterizerState.CullMode, CullMode |
| 5.1.02 | `rs-fill` | Fill Mode | XNA 4.0 | RasterizerState.FillMode, GraphicsCapability.WireFrame |
| 5.1.03 | `rs-depthbias` | Depth Bias and Z-Fighting | XNA 4.0 | RasterizerState.DepthBias, RasterizerState.SlopeScaleDepthBias |
| 5.1.04 | `rs-scissor` | Scissor Rectangle | XNA 4.0 | RasterizerState.ScissorTestEnable, GraphicsDevice.ScissorRectangle, RenderTarget2D |
| 5.1.05 | `rs-depthfunc` | Depth Test | XNA 4.0 | DepthStencilState.DepthBufferFunction, DepthStencilState.DepthBufferEnable, CompareFunction |
| 5.1.06 | `rs-depthread` | Depth Read-Only for Glass | XNA 4.0 | DepthStencilState.DepthRead, DepthStencilState.DepthBufferWriteEnable |
| 5.1.07 | `rs-portal` | Stencil: a Portal | XNA 4.0 | DepthStencilState.StencilEnable, DepthStencilState.StencilFunction, StencilOperation, ColorWriteChannels |
| 5.1.08 | `rs-outline` | Stencil: an Outline | XNA 4.0 | DepthStencilState.ReferenceStencil, DepthStencilState.StencilPass, CompareFunction.NotEqual |
| 5.1.09 | `rs-shadow` | Stencil: a Planar Shadow | XNA 4.0 | Matrix.CreateShadow, DepthStencilState.StencilPass, StencilOperation.Increment |
| 5.1.10 | `rs-stencilcount` | Stencil: Counting and Masks | XNA 4.0 | DepthStencilState.StencilPass, DepthStencilState.StencilFail, DepthStencilState.StencilDepthBufferFail, DepthStencilState.DepthBufferFunction, DepthStencilState.StencilWriteMask, DepthStencilState.StencilMask, DepthStencilState.TwoSidedStencilMode, DepthStencilState.CounterClockwiseStencilPass, DepthStencilState.CounterClockwiseStencilFunction, StencilOperation, GraphicsDevice.Clear |
| 5.1.11 | `rs-blend` | BlendState Gallery | XNA 4.0 | BlendState, Blend, BlendFunction |
| 5.1.12 | `rs-colorwrite` | Color Write Channels | XNA 4.0 | BlendState.ColorWriteChannels, ColorWriteChannels |
| 5.1.13 | `rs-blendfactor` | Blend Factor | XNA 4.0 | BlendState.BlendFactor, Blend.BlendFactor, Blend.InverseBlendFactor |
| 5.1.14 | `rs-occlusion` | Occlusion Query | XNA 4.0 | OcclusionQuery, OcclusionQuery.PixelCount, OcclusionQuery.IsComplete, GraphicsCapability.OcclusionQuery |

**Render Target Complex** - Off-screen rendering: screens, mirrors, feedback, low resolution and MRT

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 5.2.01 | `rt-basic` | Render Target Basics | XNA 4.0 | RenderTarget2D, GraphicsDevice.SetRenderTarget, DepthFormat |
| 5.2.02 | `rt-lowres` | Low-Resolution Rendering | XNA 4.0 | RenderTarget2D, SamplerState.PointClamp |
| 5.2.03 | `rt-control` | Control Room | XNA 4.0 | RenderTarget2D, GraphicsDevice.SetRenderTarget |
| 5.2.04 | `rt-recursive` | Feedback | XNA 4.0 | RenderTarget2D, RenderTargetUsage.PreserveContents, SpriteBatch.Draw |
| 5.2.05 | `rt-mirror` | A Planar Mirror | XNA 4.0 | RenderTarget2D, Matrix.CreateLookAt, RasterizerState.CullCounterClockwise |
| 5.2.06 | `rt-mrt` | Multiple Render Targets | CNA extension | GraphicsDevice.SetRenderTargets, GraphicsDevice.GetRenderTargets, RenderTargetBinding, RenderTargetBinding.RenderTarget, BlendState.ColorWriteChannels1, BlendState.ColorWriteChannels2, BlendState.ColorWriteChannels3, ShaderEffect |
| 5.2.07 | `rt-formats` | Render Target Formats | CNA extension | SurfaceFormat, Texture.Format, GraphicsDevice.SupportsSurfaceFormatAsRenderTargetEXT |
| 5.2.08 | `rt-cube` | A Live Cube Map | XNA 4.0 | RenderTargetCube, GraphicsDevice.SetRenderTarget, GraphicsDevice.GetRenderTargets, RenderTargetBinding, RenderTargetBinding.RenderTarget, RenderTargetBinding.CubeMapFace, CubeMapFace, EnvironmentMapEffect, EnvironmentMapEffect.EnvironmentMap |
| 5.2.09 | `rt-bloom` | Bloom from Blending | XNA 4.0 | BlendFunction.ReverseSubtract, BlendState.Additive, RenderTarget2D, SpriteBatch.Begin, SamplerState.LinearClamp |
| 5.2.10 | `rt-msaa` | Multisampled Targets | XNA 4.0 | RenderTarget2D.MultiSampleCount, GraphicsCapability.MultiSampleAntiAliasing, RenderTarget2D, RasterizerState.MultiSampleAntiAlias, BlendState.MultiSampleMask |

### Floor 6: Models, Animation and Particles

glTF models and their effects, procedural and skeletal animation, particle systems

**Model Museum** - glTF import, PbrEffect, rigid and skinned animation, instances, bounds, effects

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 6.1.01 | `model-load` | Loading Models | CNA extension | ContentManager.Load<Model>, Model, ModelMesh, ModelMeshPart, Model.BoundingSphereEXT, Model.Root, ModelBone.Children, Model.CopyBoneTransformsTo, Model.CopyBoneTransformsFrom, ModelMeshPart.PrimitiveTypeEXT, ModelMeshPart.SamplerStatesEXT |
| 6.1.02 | `model-pbr` | Metallic and Rough | CNA extension | PbrEffect, PbrEffect.MetallicFactor, PbrEffect.RoughnessFactor, PbrEffect.EmissiveFactor |
| 6.1.03 | `model-rigid` | Rigid Animation | CNA extension | ModelAnimationsEXT, ApplyClipToBonesEXT, Model.CopyAbsoluteBoneTransformsTo, ModelBone |
| 6.1.04 | `model-skinned` | Skinned Animation | CNA extension | ApplyBindPoseBoneTransformsEXT, SkinningData, AnimationPlayer, SkinnedPbrEffect.SetBoneTransforms |
| 6.1.05 | `model-instances` | Many Instances | CNA extension | Model.Draw, BoundingFrustum.Contains, Model.BoundingSphereEXT |
| 6.1.06 | `model-bounds` | Bounding Volumes | CNA extension | ModelMesh.BoundingSphere, Model.BoundingSphereEXT, BoundingSphere.Transform |
| 6.1.07 | `model-effects` | Swapping Effects | CNA extension | ModelMeshPart.Effect, BasicEffect, PbrEffect |
| 6.1.08 | `model-report` | Import Report | CNA extension | GltfImportReportEXT, GltfImportDiagnosticEXT |
| 6.1.09 | `model-material` | PBR Material Maps | CNA extension | PbrEffect.NormalMap, PbrEffect.EmissiveMap, TextureTransformEXT, AlphaModeEXT, PbrEffect.AlphaCutoffEXT |
| 6.1.10 | `model-sampler` | Sampler States from a glTF File | CNA extension | ModelMeshPart.SamplerStatesEXT, ModelMesh.Draw, Model.CopyAbsoluteBoneTransformsTo, GraphicsDevice.SamplerStates, SamplerState, TextureAddressMode |
| 6.1.11 | `model-morph` | Morph Targets | CNA extension | MorphTargetDataEXT, SetMorphWeightsEXT, EvaluateMorphWeightsEXT, ModelMeshPart.Tag |
| 6.1.12 | `model-variants` | Material Variants | CNA extension | Model.MaterialVariantEXT, Model.MaterialVariantNamesEXT, PbrEffect |
| 6.1.13 | `model-ibl` | Image-Based Lighting | CNA extension | ImageBasedLightEXT, PbrEffect.ImageBasedLightEXT, TextureCube, PbrEffect.MetallicFactor, PbrEffect.RoughnessFactor, GraphicsDevice.SupportsImageBasedLightingEXT |
| 6.1.14 | `model-skinfx` | SkinnedEffect | XNA 4.0 | SkinnedEffect, SkinnedEffect.SetBoneTransforms, SkinnedEffect.WeightsPerVertex, SkinnedEffect.setPunctualLightEXT, PunctualLightEXT, VertexPositionNormalTextureSkinned, VertexBuffer, IndexBuffer |
| 6.1.15 | `model-skinext` | Your Own Skinned Model | CNA extension | SkinnedModelEXT, SkinnedModelEXT.ComputeBoneTransformsEXT, SkinnedModelEXT.AttachPartEXT, SkinnedModelEXT.RemovePartEXT, SkinnedModelEXT.AddPartEXT, AnimationClipEXT, BoneTrackEXT, KeyframeEXT, SkinnedEffect.SetBoneTransforms, VertexPositionNormalTextureSkinned |
| 6.1.16 | `model-cameras` | Cameras in a glTF File | CNA extension | Model.CamerasEXT, ModelCameraEXT, ModelCameraEXT.Projection, ModelCameraEXT.WorldTransform, ModelCameraEXT.IsPerspective, ModelCameraEXT.HasInfiniteFarPlane, BoundingFrustum, Matrix.Invert |

**Animation Complex** - Procedural animation: motion built from maths and matrix hierarchies

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 6.2.01 | `an-pendulum` | Pendulum Wave | XNA 4.0 | MathHelper, GameTime, Vector3 |
| 6.2.02 | `an-clock` | A Clock | XNA 4.0 | Matrix.CreateRotationZ, Matrix.Multiply |
| 6.2.03 | `an-gears` | Meshing Gears | XNA 4.0 | Matrix.CreateRotationY, Matrix.Multiply |
| 6.2.04 | `an-conveyor` | Conveyor | XNA 4.0 | VertexPositionColorTexture, SamplerState.LinearWrap, BoundingBox |
| 6.2.05 | `an-doors` | Proximity Doors | XNA 4.0 | MathHelper.Lerp, MathHelper.SmoothStep, Vector3.Distance |
| 6.2.06 | `an-kinetic` | Kinetic Sculpture | XNA 4.0 | Matrix.CreateRotationZ, Matrix.Multiply |
| 6.2.07 | `an-gyroscope` | Gyroscope | XNA 4.0 | Matrix.CreateRotationX, Matrix.CreateRotationY, Matrix.CreateRotationZ |
| 6.2.08 | `an-walker` | A Walking Figure | XNA 4.0 | Matrix.Multiply, Matrix.CreateRotationZ, MathHelper.WrapAngle |

**Particle Complex** - Eleven effects from one emitter: every control changes the effect live

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 6.3.01 | `pt-fire` | Fire | XNA 4.0 | DynamicVertexBuffer, SetDataOptions.Discard, IndexBuffer, BlendState.Additive |
| 6.3.02 | `pt-smoke` | Smoke | XNA 4.0 | DynamicVertexBuffer, SetDataOptions.Discard, IndexBuffer, BlendState.Additive |
| 6.3.03 | `pt-sparks` | Sparks | XNA 4.0 | DynamicVertexBuffer, SetDataOptions.Discard, IndexBuffer, BlendState.Additive |
| 6.3.04 | `pt-snow` | Snow | XNA 4.0 | DynamicVertexBuffer, SetDataOptions.Discard, IndexBuffer, BlendState.Additive |
| 6.3.05 | `pt-rain` | Rain | XNA 4.0 | DynamicVertexBuffer, SetDataOptions.Discard, IndexBuffer, BlendState.Additive |
| 6.3.06 | `pt-fountain` | Fountain | XNA 4.0 | DynamicVertexBuffer, SetDataOptions.Discard, IndexBuffer, BlendState.Additive |
| 6.3.07 | `pt-dust` | Dust Motes | XNA 4.0 | DynamicVertexBuffer, SetDataOptions.Discard, IndexBuffer, BlendState.Additive |
| 6.3.08 | `pt-fireworks` | Fireworks | XNA 4.0 | DynamicVertexBuffer, SetDataOptions.Discard, IndexBuffer, BlendState.Additive |
| 6.3.09 | `pt-bubbles` | Bubbles | XNA 4.0 | DynamicVertexBuffer, SetDataOptions.Discard, IndexBuffer, BlendState.Additive |
| 6.3.10 | `pt-leaves` | Falling Leaves | XNA 4.0 | DynamicVertexBuffer, SetDataOptions.Discard, IndexBuffer, BlendState.Additive |
| 6.3.11 | `pt-magic` | Magic Spiral | XNA 4.0 | DynamicVertexBuffer, SetDataOptions.Discard, IndexBuffer, BlendState.Additive |

**Simulation Lab** - Flocking, collisions, numerical integration and inverse kinematics

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 6.4.01 | `sim-flock` | Flocking | XNA 4.0 | Vector3, Vector3.Normalize, Vector3.LengthSquared, Matrix.CreateScale, Matrix.CreateTranslation |
| 6.4.02 | `sim-balls` | Bouncing Balls | XNA 4.0 | BoundingSphere.Intersects, Vector3.Dot, BoundingBox |
| 6.4.03 | `sim-integrators` | Numerical Integrators | XNA 4.0 | MathHelper, Vector3, GameTime |
| 6.4.04 | `sim-ik` | Inverse Kinematics | XNA 4.0 | Vector3.Cross, Vector3.Dot, MathHelper.ToRadians, MathHelper.ToDegrees |

### Floor 7: Audio and Input

Sound, music, keyboards, mice and game pads

**Audio Complex** - SoundEffect, instances, 3D audio, synthesis, generated sounds, music and the microphone

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 7.1.01 | `au-basics` | SoundEffect.Play | XNA 4.0 | SoundEffect.Play, SoundEffect, ContentManager.Load<SoundEffect> |
| 7.1.02 | `au-layers` | Instances and Layers | XNA 4.0 | SoundEffectInstance, SoundEffectInstance.IsLooped, SoundEffectInstance.Volume, SoundEffectInstance.Pitch, SoundState |
| 7.1.03 | `au-spatial` | 3D Audio | XNA 4.0 | SoundEffectInstance.Apply3D, AudioEmitter, AudioListener, SoundEffect.DopplerScale, SoundEffect.DistanceScale, SoundEffect.SpeedOfSound |
| 7.1.04 | `au-synth` | A Synthesiser | XNA 4.0 | DynamicSoundEffectInstance, DynamicSoundEffectInstance.SubmitBuffer, DynamicSoundEffectInstance.BufferNeeded, DynamicSoundEffectInstance.SubmitFloatBufferEXT, AudioChannels, DynamicSoundEffectInstance.PendingBufferCount, DynamicSoundEffectInstance.GetSampleSizeInBytes, SoundEffect.GetSampleDuration |
| 7.1.05 | `au-generate` | Generated Sounds | XNA 4.0 | SoundEffect(buffer, sampleRate, channels), AudioChannels |
| 7.1.06 | `au-master` | Master Volume and Settings | XNA 4.0 | SoundEffect.MasterVolume, SoundEffect.DistanceScale, SoundEffect.DopplerScale, SoundEffect.SpeedOfSound |
| 7.1.07 | `au-music` | Song and MediaPlayer | XNA 4.0 | Song, Song.Duration, Song.TrackNumber, MediaPlayer.ActiveSongChanged, MediaPlayer.MediaStateChanged, MediaPlayer.GameHasControl, Song.PlayCount, Song.Rating, Song.IsRated, Song.IsProtected, Song.Artist, MediaPlayer.Play, MediaPlayer.Volume, SongCollection, MediaQueue, MediaPlayer.Queue, MediaPlayer.MoveNext, MediaPlayer.MovePrevious, MediaPlayer.IsShuffled, MediaPlayer.IsVisualizationEnabled, MediaPlayer.GetVisualizationData, VisualizationData, MediaState |
| 7.1.08 | `au-video` | Video | XNA 4.0 | Video, VideoPlayer, VideoPlayer.Play, VideoPlayer.Pause, VideoPlayer.Resume, VideoPlayer.Stop, VideoPlayer.GetTexture, VideoPlayer.IsDisposed, VideoPlayer.IsLooped, VideoPlayer.IsMuted, VideoPlayer.Video, VideoPlayer.Volume, VideoPlayer.PlayPosition, VideoPlayer.State, WmvImporter, WmvImporter.Import, VideoContent, Processors::VideoProcessor, Processors::VideoProcessor.Process, Processors::VideoProcessor.VideoSoundtrackType, VideoSoundtrackType, ContentCompiler.Compile(VideoContent), ContentManager.Load<Video> |
| 7.1.09 | `au-mic` | The Microphone | XNA 4.0 | Microphone, Microphone.GetData, Microphone.SampleRate, Microphone.BufferDuration, Microphone.GetSampleSizeInBytes, Microphone.IsHeadset |
| 7.1.10 | `au-xact` | XACT Cues and Banks | XNA 4.0 | AudioEngine, AudioEngine.Update, AudioEngine.GetCategory, AudioCategory.SetVolume, AudioCategory.Pause, WaveBank, SoundBank, SoundBank.GetCue, SoundBank.PlayCue, Cue.Play, Cue.Pause, Cue.Resume, Cue.Stop, AudioStopOptions, Cue.SetVariable, Cue.GetVariable, AudioEngine.SetGlobalVariable, AudioEngine.GetGlobalVariable, AudioEngine.RendererDetails, RendererDetail.FriendlyName, Cue.IsCreated, Cue.IsPreparing, Cue.IsStopping, SoundBank.IsInUse, WaveBank.IsInUse |

**Input Complex** - Keyboard, mouse, game pads, text input, joysticks and the machine's other devices

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 7.2.01 | `in-keyboard` | The Keyboard | XNA 4.0 | Keyboard.GetState, KeyboardState.IsKeyDown, KeyboardState.GetPressedKeys, Keyboard.GetKeyNameEXT, Keyboard.GetModStateEXT, Keyboard.GetKeyFromNameEXT, Keyboard.GetScancodeNameEXT, Keyboard.GetScancodeFromNameEXT, Keyboard.GetKeyFromScancodeEXT |
| 7.2.02 | `in-edges` | Pressed, Held, Released | XNA 4.0 | KeyboardState, Keys |
| 7.2.03 | `in-mouse` | The Mouse | XNA 4.0 | Mouse.GetState, Mouse.SetPosition, Mouse.WindowHandle, MouseState, ButtonState, Mouse.IsRelativeMouseModeEXT, Mouse.SetCursor, MouseCursor, MouseCursor.FromTexture2D, Mouse.SetCaptureEXT |
| 7.2.04 | `in-pad` | The Game Pad | XNA 4.0 | GamePad.GetState, GamePad.GetCapabilities, GamePadState, GamePadCapabilities, GamePadCapabilities.IsConnected, GamePadCapabilities.HasAButton, GamePadCapabilities.HasBButton, GamePadCapabilities.HasXButton, GamePadCapabilities.HasYButton, GamePadCapabilities.HasBackButton, GamePadCapabilities.HasStartButton, GamePadCapabilities.HasBigButton, GamePadCapabilities.HasDPadUpButton, GamePadCapabilities.HasDPadDownButton, GamePadCapabilities.HasDPadLeftButton, GamePadCapabilities.HasDPadRightButton, GamePadCapabilities.HasLeftShoulderButton, GamePadCapabilities.HasRightShoulderButton, GamePadCapabilities.HasLeftStickButton, GamePadCapabilities.HasRightStickButton, GamePadCapabilities.HasLeftXThumbStick, GamePadCapabilities.HasLeftYThumbStick, GamePadCapabilities.HasRightXThumbStick, GamePadCapabilities.HasRightYThumbStick, GamePadCapabilities.HasLeftTrigger, GamePadCapabilities.HasRightTrigger, GamePadCapabilities.HasLeftVibrationMotor, GamePadCapabilities.HasRightVibrationMotor, GamePadCapabilities.HasVoiceSupport, GamePadDeadZone, Buttons, Buttons.BigButton, Buttons.LeftThumbstickUp, Buttons.RightThumbstickLeft, GamePadState.IsButtonDown, GamePadState.IsButtonUp, GamePadState.Equals, GamePad.SetVibration, GamePad.KeyboardEmulationEnabledEXT, GamePad.SetTriggerVibrationEXT, GamePad.SetLightBarEXT, GamePad.GetGUIDEXT, GamePad.GetGyroEXT, GamePad.GetPlayerIndexEXT, GamePad.GetButtonLabelEXT, GamePad.GetTouchpadFingerCountEXT, GamePadCapabilities.GamePadType |
| 7.2.05 | `in-players` | Four Player Slots | XNA 4.0 | PlayerIndex, GamePad.GetCapabilities, GamePadCapabilities, GamePad.GetNameEXT |
| 7.2.06 | `in-text` | Text Input and Clipboard | CNA extension | TextInputEXT.TextInput, Clipboard.SetTextEXT, Clipboard.GetTextEXT, Clipboard.SetDataEXT, Clipboard.GetMimeTypesEXT, TextInputEXT.StartTextInputWithTypeEXT, TextInputTypeEXT, Clipboard.SetPrimarySelectionTextEXT, Clipboard.GetPrimarySelectionTextEXT, Clipboard.HasPrimarySelectionTextEXT |
| 7.2.07 | `in-joystick` | Raw Joysticks | CNA extension | Joysticks.GetJoysticksEXT, Joysticks.GetCapabilitiesEXT, Joysticks.GetStateEXT |
| 7.2.08 | `in-touch` | Touch and Gestures | XNA 4.0 | TouchPanel.GetState, TouchPanel.ReadGesture, TouchPanel.EnabledGestures, GestureType, GestureSample.Position2, GestureSample.Delta2, GestureSample.Timestamp, TouchLocation.Id, TouchLocation.State, TouchLocation.TryGetPreviousLocation, TouchPanel.MouseTouchEmulationEnabledEXT |
| 7.2.09 | `in-devices` | Other Input Devices | CNA extension | InputDevices.GetMiceEXT, TouchPanel.GetState, TouchPanel.MouseTouchEmulationEnabledEXT, Sensors.GetSensorsEXT, Power.GetInfoEXT, GamePad.GetConnectionStateEXT, GamePad.GetPowerInfoEXT, GamePad.GetTouchpadCountEXT, GamePad.GetFirmwareVersionEXT, GamePad.GetSerialEXT, Mouse.GetGlobalPositionEXT, Haptics.GetHapticsEXT, Haptics.IsMouseHapticEXT, Haptics.IsJoystickHapticEXT |
| 7.2.10 | `in-phone` | Phone Sensors | XNA 4.0 | Accelerometer, Accelerometer.IsSupported, Accelerometer.Start, Accelerometer.Stop, Accelerometer.State, SensorBase.CurrentValue, SensorBase.CurrentValueChanged, Accelerometer.KeyboardEmulationEnabledEXT, AccelerometerReading.Acceleration, Gyroscope, Gyroscope.IsSupported, GyroscopeReading.RotationRate, Compass, Compass.IsSupported, CompassReading.MagneticHeading, Motion, Motion.IsSupported, MotionReading.Attitude, MotionReading.Gravity, AttitudeReading, SensorState, SensorReadingEventArgs, VibrateController, VibrateController.Default, VibrateController.Start, VibrateController.IsSupported, Microsoft.Devices.Environment.DeviceType, DeviceType |

### Floor 8: Content, Platform and CNA

How assets arrive, what the platform reports, and what CNA adds to XNA

**Content Pipeline** - ContentManager, loose files, compiled .cnb and .xnb, load failures, storage and resource lifetime

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 8.1.01 | `ct-load` | Loading Assets | XNA 4.0 | ContentManager.Load<Texture2D>, ContentManager.Load<SoundEffect>, ContentManager.Load<Model>, ContentManager.RootDirectory, ContentManager.ServiceProvider |
| 8.1.02 | `ct-cache` | The ContentManager Cache | XNA 4.0 | ContentManager.Load, ContentManager.Unload, GraphicsResource.IsDisposed |
| 8.1.03 | `ct-formats` | Loose Image Formats | CNA extension | ContentManager.Load<Texture2D>, Texture2D.GetData, Texture2D.DDSFromStreamEXT, SurfaceFormat.Dxt1 |
| 8.1.04 | `ct-failure` | When Loading Fails | XNA 4.0 | ContentLoadException, ContentManager.Load |
| 8.1.05 | `ct-pipeline` | The Content Compiler | CNA extension | ContentManager.Load, ContentManager.GetContentManifest |
| 8.1.06 | `ct-intermediate` | The Pipeline in the Game | XNA 4.0 | IntermediateSerializer, IntermediateSerializer.Serialize, IntermediateSerializer.Deserialize, IntermediateSerializer.GetTypeSerializer, XmlImporter, XmlImporter.Import, XmlImporter.Attribute, ContentImporterAttribute, ContentItem, ContentTypeDescriptor, InvalidContentException, ContentSerializerAttribute, ContentSerializerCollectionItemNameAttribute, ContentSerializerCollectionItemNameAttribute.CollectionItemName |
| 8.1.07 | `ct-processor` | Processing a Texture | XNA 4.0 | TextureImporter, TextureImporter.Import, Processors::TextureProcessor, Processors::TextureProcessor.Process, Processors::TextureProcessor.ColorKeyEnabled, Processors::TextureProcessor.ColorKeyColor, Processors::TextureProcessor.PremultiplyAlpha, Processors::TextureProcessor.GenerateMipmaps, Processors::TextureProcessor.ResizeToPowerOfTwo, Processors::TextureProcessor.TextureFormat, TextureProcessorOutputFormat, Texture2DContent, MipmapChain, BitmapContent, PixelBitmapContent |
| 8.1.08 | `ct-fontbuild` | Building a Font | XNA 4.0 | Graphics::FontDescription, Microsoft::Xna::Framework::Content::Pipeline::FontDescriptionImporter, Microsoft::Xna::Framework::Content::Pipeline::FontDescriptionImporter.Import, Graphics::FontDescription.FontName, Graphics::FontDescription.Size, Graphics::FontDescription.Spacing, Graphics::FontDescription.Style, Graphics::FontDescription.UseKerning, Processors::FontDescriptionProcessor, Graphics::FontDescriptionStyle, SpriteFontContent, Graphics::FontDescription.Characters, Graphics::FontDescription.DefaultCharacter, ContentIdentity, SpriteFont, SpriteFont.MeasureString, SpriteBatch.DrawString |
| 8.1.09 | `ct-meshbuilder` | Building a Model | XNA 4.0 | MeshBuilder, MeshBuilder.StartMesh, MeshBuilder.CreatePosition, MeshBuilder.CreateVertexChannel, MeshBuilder.SetVertexChannelData, MeshBuilder.AddTriangleVertex, MeshBuilder.FinishMesh, MeshBuilder.MergeDuplicatePositions, MeshBuilder.SwapWindingOrder, MeshContent, VertexChannelNames, Processors::ModelProcessor, Processors::ModelProcessor.Scale, Processors::ModelProcessor.RotationY, Processors::ModelProcessor.GenerateTangentFrames, Processors::ModelProcessor.SwapWindingOrder, ModelContent, ModelMeshContent, ModelMeshPartContent, VertexBufferContent, VertexDeclarationContent |
| 8.1.10 | `ct-modelimport` | Importing a Model File | XNA 4.0 | XImporter, FbxImporter, Microsoft::Xna::Framework::Content::Pipeline::ContentImporter, ContentImporterAttribute, Microsoft::Xna::Framework::Content::Pipeline::ContentImporterContext, NodeContent, NodeContent.Children, MeshContent, GeometryContent, MaterialContent, BasicMaterialContent, BasicMaterialContent.DiffuseColor, Processors::ModelProcessor, ModelContent, ModelMeshContent, ModelMeshPartContent, ModelBoneContent, ContentProcessorContext.Convert |
| 8.1.11 | `ct-compiler` | Writing an .xnb | XNA 4.0 | ContentCompiler, ContentCompiler.Compile, ContentCompiler.GetTypeWriter, CompileOptions, CompileOptions.targetPlatform, CompileOptions.targetProfile, CompileOptions.compressContent, Microsoft::Xna::Framework::Content::Pipeline::TargetPlatform, Texture2DContent, Processors::TextureProcessor, TextureImporter, ContentManager.OpenStream, ContentManager.ReadAsset, ContentManager.RouteLoadsThroughOpenStreamEXT |
| 8.1.12 | `ct-audiopipeline` | Importing and Processing Audio | XNA 4.0 | Microsoft::Xna::Framework::Content::Pipeline::WavImporter, Microsoft::Xna::Framework::Content::Pipeline::WavImporter.Import, Mp3Importer, Mp3Importer.Import, Mp3Importer.Attribute, ContentImporterAttribute, AudioContent, AudioContent.Format, AudioContent.Data, Audio::AudioFormat, Audio::AudioFormat.SampleRate, Audio::AudioFormat.ChannelCount, Audio::AudioFormat.BitsPerSample, Audio::AudioFormat.NativeWaveFormat, ConversionQuality, Processors::SoundEffectProcessor, Processors::SoundEffectProcessor.Quality, SoundEffectContent, Processors::SongProcessor, SongContent, SoundEffect.FromStream, ContentCompiler.Compile(SoundEffectContent), ContentCompiler.Compile(SongContent) |
| 8.1.13 | `ct-storage` | Title Files and Saved Data | XNA 4.0 | TitleContainer.OpenStream, StorageDevice, StorageDevice.BeginShowSelector, StorageDevice.EndShowSelector, StorageDevice.BeginOpenContainer, StorageDevice.EndOpenContainer, StorageDevice.IsConnected, StorageDevice.DeviceChanged, StorageDevice.SetAppNameEXT, StorageContainer, StorageContainer.DisplayName, StorageContainer.CreateFile, StorageContainer.OpenFile, StorageContainer.FileExists, StorageContainer.DeleteFile, StorageContainer.CreateDirectory, StorageContainer.DeleteDirectory, StorageContainer.DirectoryExists, StorageContainer.GetFileNames, StorageContainer.GetDirectoryNames, StorageDevice.GetStorageRootEXT, StorageDevice.DeleteContainer, StorageDevice.TotalSpace, StorageDevice.FreeSpace |
| 8.1.14 | `ct-custom` | Teaching ContentManager a New Type | CNA extension | ContentManager.RegisterTypeReader, LooseFileContentTypeReader, ContentManager.RegisterCnjLoader, ContentLoadException |
| 8.1.15 | `ct-reader` | Content Readers | XNA 4.0 | ContentTypeReader, ContentTypeReaderManager.AddTypeCreator, ContentTypeReaderManager.GetTypeReader, ContentReader, ContentReader.ContentManager, ContentReader.AssetName, ContentReader.Platform, ContentReader.Version, ContentReader.ReadBoundingSphere, ContentReader.ReadColor, ContentReader.ReadMatrix, ContentReader.ReadQuaternion, ContentReader.ReadVector2, ContentReader.ReadVector3, ContentReader.ReadVector4, ContentManager.Load |
| 8.1.16 | `ct-lifetime` | Resource Lifetime | XNA 4.0 | GraphicsResource.Dispose, GraphicsResource.IsDisposed, GraphicsResource.Disposing, GraphicsDevice.ResourceCreated, GraphicsDevice.ResourceDestroyed, ResourceCreatedEventArgs, ResourceCreatedEventArgs.Resource, ResourceDestroyedEventArgs, ResourceDestroyedEventArgs.Name, ResourceDestroyedEventArgs.Tag, ContentManager.Unload |
| 8.1.17 | `ct-resource` | Content from Memory | XNA 4.0 | ResourceContentManager, ResourceContentManager.OpenStream, System.Resources.ResourceManager, ContentManager.Load, ContentLoadException |

**CNA Extensions** - What CNA adds to XNA 4.0: DebugDraw, post-process effects, engine statistics, point lists and device queries

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 8.2.01 | `cx-debugdraw` | DebugDraw | CNA extension | CNA::Graphics::DebugDraw, DebugDraw.addBox, DebugDraw.addSphere, DebugDraw.addFrustum, DebugDraw.setDepthTested |
| 8.2.02 | `cx-crt` | CRT Effect | CNA extension | CNA::Graphics::CRTEffect, CRTMaskType, SpriteBatch.Begin(effect) |
| 8.2.03 | `cx-depth` | Colour Depth Effect | CNA extension | CNA::Graphics::DepthEffect, DepthEffectMode, DitherMode |
| 8.2.04 | `cx-ascii` | ASCII Post-Process | CNA extension | CNA::Graphics::AsciiPostProcessEffect, AsciiQuantizeMode |
| 8.2.05 | `cx-package` | Shader Packages | CNA extension | CNA::Graphics::ShaderPackageEXT, ShaderPackageEXT.selectFor, ShaderCodeEXT, ShaderEffect(device, package), ShaderLanguageEXT, ShaderEffect.DeclareUniformBlockEXT |
| 8.2.06 | `cx-shadow` | Shadow Mapping | CNA extension | IShadowReceiverEXT, BasicEffect.ShadowMapEXT, BasicEffect.LightViewProjectionEXT, BasicEffect.ShadowDepthBiasEXT, BasicEffect.ShadowFilterRadiusEXT, ShadowCascadeStateEXT, BasicEffect.ShadowCascadesEXT, DynamicVertexBuffer, VertexPositionColor, BasicEffect.VertexColorEnabled, Matrix.CreateOrthographic |
| 8.2.07 | `cx-stats` | Engine Statistics | CNA extension | CNA::Diagnostics::GetProvider, Snapshot.metrics, Snapshot.resources, SetRuntimeMode |
| 8.2.08 | `cx-points` | Point Lists | CNA extension | PrimitiveType.PointListEXT, GraphicsDevice.DrawUserPrimitives, GraphicsDevice.SetStringMarkerEXT |
| 8.2.09 | `cx-devices` | Device Extensions | CNA extension | CNA::Devices::SystemInfo, CNA::Devices::Locale, CNA::Devices::PowerInfo, CNA::Devices::DisplayInfo |

**Platform and Renderer** - The window, the display adapter, the active renderer and the platform, as CNA reports them

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 8.3.01 | `pl-exceptions` | Exceptions CNA Throws | XNA 4.0 | System.ObjectDisposedException, System.ArgumentOutOfRangeException, System.InvalidOperationException, ContentLoadException, System.IO.FileNotFoundException, GamerServicesNotAvailableException, GamerPrivilegeException, Guide.ShowMarketplace, SignedInGamer.IsFriend, GuideAlreadyVisibleException, StorageContainer, TitleContainer.OpenStream, SpriteBatch.Begin, SpriteBatch.End |
| 8.3.02 | `pl-log` | CNA's Log | CNA extension | CNA::Logger, Logger.Log, Logger.InfoIf, Logger.WarnIf, Logger.SetMinimumLevel, Logger.GetMinimumLevel, Logger.SetSink, Logger.ResetSink, CNA::LogLevel, CNA::LogCategory |
| 8.3.03 | `pl-diagnostics` | Measuring Your Own Code | CNA extension | CNA::Diagnostics::CounterHandle, CNA::Diagnostics::GaugeHandle, CNA::Diagnostics::FrameCounterHandle, CNA::Diagnostics::NameHandle, CNA::Diagnostics::ZoneScope, CNA::Diagnostics::MarkEvent, CNA::Diagnostics::ResourceHandle, CNA::Diagnostics::ResourceDescriptor, CNA::Diagnostics::IDiagnosticsSource, CNA::Diagnostics::RegisterSource, CNA::Diagnostics::SourceRegistration, CNA::Diagnostics::FrameStatisticsSink, CNA::Diagnostics::GetProvider, IDiagnosticsProvider.ReadEvents, IDiagnosticsProvider.ResolveName, CNA::Diagnostics::SetRuntimeMode, CNA::Diagnostics::StartRecording, CNA::Diagnostics::StopRecording, CNA::Diagnostics::Trace, Trace.WriteChromeTrace, Trace.WriteBinary, Trace.ReadBinary, CNA::Diagnostics::EventRecord, CNA::Diagnostics::EventBatch, CNA::Diagnostics::MetricKind, CNA::Diagnostics::ResourceKind |
| 8.3.04 | `pl-inspector` | The Inspector | CNA extension | CNA::Inspector::Agent, Agent.Start, Agent.Stop, Agent.GetPort, Agent.GetAuthenticationToken, AgentConfiguration, CNA::Inspector::Client, Client.Connect, Client.Ping, Client.CaptureSnapshot, Client.ReadEvents, Client.RequestPreview, HelloResponse, SnapshotResponse, EventsResponse, PreviewResponse, Capability, SnapshotPart |
| 8.3.05 | `pl-window` | The Game Window | XNA 4.0 | GameWindow.GetNativeWindowHandleEXT, GameWindow.CurrentOrientation, GameWindow.OrientationChanged, GraphicsDeviceManager.SupportedOrientations, CNA::Platform::NativeWindowHandle, GameWindow.Title, GameWindow.ClientBounds, GameWindow.AllowUserResizing, GameWindow.ClientSizeChanged, Game.Activated, GameWindow.FileDropEXT, PhoneApplicationService, PhoneApplicationService.AttachEXT, PhoneApplicationService.Launching, PhoneApplicationService.Activated, PhoneApplicationService.Deactivated, PhoneApplicationService.Closing, PhoneApplicationService.State, LaunchingEventArgs, ActivatedEventArgs, DeactivatedEventArgs, ClosingEventArgs |
| 8.3.06 | `pl-adapter` | Adapter and Back Buffer | XNA 4.0 | GraphicsAdapter, GraphicsAdapter.UseNullDevice, GraphicsAdapter.UseReferenceDevice, GraphicsAdapter.GetCurrentDeviceTypeEXT, GraphicsAdapter.GetRequiredRendererEXT, Graphics::DisplayMode, GraphicsDevice.GraphicsProfile, PresentationParameters, GraphicsAdapter.IsProfileSupported, GraphicsAdapter.QueryRenderTargetFormat, GraphicsAdapter.QueryBackBufferFormat, GraphicsAdapter.Adapters, GraphicsAdapter.IsWideScreenAspectRatioEXT |
| 8.3.07 | `pl-renderer` | The Active Renderer | CNA extension | GraphicsDevice.GetRendererCapabilityProfileEXT, GraphicsDevice.GetRendererCapabilityReportEXT, RendererFeature, RendererLimit, RendererFormatUsage, GraphicsDevice.GetGraphicsRendererName, GraphicsDevice.GetDisplayColorSpaceEXT, GraphicsDevice.SupportsDisplayColorSpaceEXT, GraphicsDevice.SetDisplayColorSpaceEXT, GraphicsDevice.SupportsImageBasedLightingEXT, GraphicsDevice.GetTrackedResourceCount, GraphicsDevice.GetLogicalSizeEXT |
| 8.3.08 | `pl-platform` | Platform Capabilities | CNA extension | Game.GetPlatformCapabilitiesEXT, PlatformCapabilities, Game.GetPlatformEXT, CNA::Platform::IPlatform, IPlatform.GetName, IPlatform.IsSubsystemInitialized, IPlatform.GetPerformanceFrequency, IPlatform.GetTicksMilliseconds, IPlatform.GetKeyboard |

**Players and Networking** - GamerServices and Net: signed-in players, achievements, the Guide, packets and a session between two local players

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 8.4.01 | `gs-players` | Signed-in Players | XNA 4.0 | Gamer.SignedInGamers, SignedInGamer.Presence, SignedInGamer.Privileges, SignedInGamer.GameDefaults, Gamer.GetProfile, GamerProfile, GamerProfile.Motto, GamerProfile.Region, GamerProfile.GetGamerPicture, GameDefaults.ControllerSensitivity, GameDefaults.RacingCameraAngle, GameDefaults.AutoCenter, GameDefaults.ManualTransmission, GameDefaults.AccelerateWithButtons, GameDefaults.PrimaryColor, SignedInGamer.SignedIn, GamerPresenceMode |
| 8.4.02 | `gs-achievements` | Achievements | XNA 4.0 | SignedInGamer.AwardAchievement, SignedInGamer.GetAchievements, Achievement, AchievementCollection |
| 8.4.03 | `gs-leaderboards` | Leaderboards | XNA 4.0 | LeaderboardWriter, LeaderboardWriter.GetLeaderboard, LeaderboardEntry, LeaderboardEntry.Gamer, LeaderboardEntry.Rating, LeaderboardEntry.Columns, LeaderboardReader, LeaderboardIdentity, LeaderboardKey, LeaderboardOutcome, PropertyDictionary, PropertyDictionary.Count, PropertyDictionary.Keys, PropertyDictionary.Values, PropertyDictionary.ContainsKey, PropertyDictionary.SetValue, PropertyDictionary.GetValueInt32, PropertyDictionary.GetValueOutcome, Gamer.LeaderboardWriter, NetworkSession.WriteArbitratedLeaderboard, NetworkSession.WriteUnarbitratedLeaderboard, NetworkSession.WriteTrueSkill, WriteLeaderboardsEventArgs.Gamer, WriteLeaderboardsEventArgs.IsLeaving |
| 8.4.04 | `gs-guide` | The Guide | XNA 4.0 | Guide.BeginShowMessageBox, Guide.EndShowMessageBox, Guide.BeginShowKeyboardInput, Guide.IsTrialMode, Guide.SimulateTrialMode, Guide.IsVisible, Guide.IsScreenSaverEnabled, Guide.DelayNotifications, GamerServicesDispatcher.IsInitialized, GamerServicesDispatcher.WindowHandle, Guide.NotificationPosition, MessageBoxIcon |
| 8.4.05 | `gs-avatar` | Avatars | XNA 4.0 | AvatarDescription, AvatarDescription.CreateRandom, AvatarDescription.BeginGetFromGamer, AvatarRenderer, AvatarAnimation, AvatarAnimationPreset, AvatarExpression, AvatarBodyType, AvatarBone, AvatarEye, AvatarMouth, AvatarEyebrow, AvatarRendererState, IAvatarAnimation |
| 8.4.06 | `net-packets` | Packets | XNA 4.0 | PacketWriter, PacketWriter.Length, PacketWriter.Position, PacketWriter.Write, PacketReader, PacketReader.Length, PacketReader.Position, PacketReader.ReadVector3, PacketReader.ReadQuaternion, PacketReader.ReadMatrix |
| 8.4.07 | `net-session` | A Network Session | XNA 4.0 | NetworkSession.AllowHostMigration, NetworkSession.SessionType, NetworkSession.MaxGamers, NetworkSession.PrivateGamerSlots, NetworkSession.Host, NetworkSession.RemoteGamers, NetworkSession.PreviousGamers, QualityOfService.MinimumRoundtripTime, QualityOfService.BytesPerSecondDownstream, NetworkSession.ResetReady, NetworkSession.AddLocalGamer, NetworkSession.FindGamerById, NetworkGamer.Id, NetworkGamer.IsPrivateSlot, NetworkGamer.HasVoice, NetworkGamer.IsTalking, NetworkGamer.IsMutedByLocalUser, NetworkGamer.HasLeftSession, NetworkSession.Create, NetworkSession.Update, NetworkSession.StartGame, NetworkSession.EndGame, NetworkSession.SimulatedLatency, NetworkSession.SimulatedPacketLoss, NetworkSession.SessionState, LocalNetworkGamer.SendData, LocalNetworkGamer.ReceiveData, SendDataOptions, NetworkGamer.IsReady, NetworkSession.GamerJoined, NetworkSession.GameStarted, NetworkSession.BeginFind, NetworkSession.EndFind, AvailableNetworkSession, AvailableNetworkSession.CurrentGamerCount, AvailableNetworkSession.HostGamertag, AvailableNetworkSession.OpenPrivateGamerSlots, AvailableNetworkSession.OpenPublicGamerSlots, AvailableNetworkSession.QualityOfService, AvailableNetworkSession.SessionProperties, AvailableNetworkSessionCollection, AvailableNetworkSessionCollection.IsDisposed, QualityOfService, NetworkSessionProperties, NetworkSession.SessionProperties, NetworkSession.AllowJoinInProgress, NetworkSession.BeginJoin, NetworkSession.EndJoin, NetworkGamer.IsGuest, NetworkGamer.IsLocal, NetworkGamer.IsHost, NetworkGamer.Machine, NetworkGamer.RoundtripTime, NetworkGamer.Session, NetworkMachine, NetworkMachine.Gamers, NetworkMachine.RemoveFromSession, NetworkSession.AllGamers, NetworkSessionJoinException, NetworkSessionJoinError, NetworkSessionEndReason, NetworkSessionEndedEventArgs, NetworkSessionEndedEventArgs.EndReason, HostChangedEventArgs, HostChangedEventArgs.NewHost, HostChangedEventArgs.OldHost, NetworkSession.HostChanged, NetworkSession.SessionEnded, NetworkSession.InviteAccepted, NetworkSession.BeginJoinInvited, NetworkSession.EndJoinInvited, InviteAcceptedEventArgs, InviteAcceptedEventArgs.Gamer, InviteAcceptedEventArgs.IsCurrentSession, NetworkSessionType.PlayerMatch |

**Online Service** - CNA's account service: accounts, friends, messages and parties through the Guide (offline unless tools/online_stack.sh runs one)

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 8.5.01 | `on-account` | Accounts and the Guide | XNA 4.0 | Guide.ShowSignIn, Guide.ShowAchievementsEXT, Guide.ShowMarketplace, SignedInGamer.IsSignedInToLive, GamerProfile.GamerScore, GamerProfile.Reputation, GamerProfile.TotalAchievements, CNA::GamerServices::resolveConfiguration |
| 8.5.02 | `on-friends` | Friends and Presence | XNA 4.0 | SignedInGamer.GetFriends, FriendCollection, FriendCollection.IsDisposed, FriendGamer, FriendGamer.IsOnline, FriendGamer.IsAway, FriendGamer.IsBusy, FriendGamer.IsPlaying, FriendGamer.IsJoinable, FriendGamer.Presence, Guide.ShowFriends, Guide.ShowPlayers, Guide.ShowGamerCard, Guide.ShowPlayerReview, Guide.ShowFriendRequest |
| 8.5.03 | `on-messages` | Messages | XNA 4.0 | Guide.ShowMessages, Guide.ShowComposeMessage |
| 8.5.04 | `on-party` | Parties | XNA 4.0 | Guide.ShowParty, Guide.ShowPartySessions, SignedInGamer.PartySize |

**History and Versions** - Where XNA came from, and what this very build of CNA is made of

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 8.6.01 | `hist-xna` | XNA, 2004 to Today | XNA 4.0 | SpriteBatch.DrawString |
| 8.6.02 | `hist-cna` | This Build of CNA | CNA extension | CNA::GraphicsRendererSelection.GetAvailable, CNA::GraphicsRendererSelection.GetSelected, CNA::GraphicsRendererSelection.GetActive, CNA::GraphicsRendererSelection.IsFallbackEnabled, CNA::GraphicsRendererSelection.GetFallbackHistory, CNA::GraphicsRendererFallbackReason, CNA::getGraphicsRendererMaturity, CNA::getGraphicsRendererName |
| 8.6.03 | `hist-compare` | XNA 4.0 and What CNA Adds | CNA extension | SpriteBatch.Draw, SpriteBatch.DrawString, BlendState.NonPremultiplied |

### Floor 9: Performance and Validation

Stress tests, renderer self-checks, reference scenes and the observation deck

**Performance Lab** - Stress levels for draw calls, sprites, state changes, uploads, render targets, overdraw and particles

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 9.1.01 | `pf-dashboard` | Frame-Time Dashboard | CNA extension | GameTime.ElapsedGameTime, GameTime.IsRunningSlowly, CNA::Diagnostics::GetProvider, CNA::Diagnostics::IDiagnosticsProvider.CaptureSnapshot, CNA::Diagnostics::Snapshot.metrics, CNA::Diagnostics::MetricSample.name, CNA::Diagnostics::MetricSample.value |
| 9.1.02 | `pf-objects` | Draw Calls and Instancing | XNA 4.0 | GraphicsDevice.DrawIndexedPrimitives, GraphicsDevice.DrawInstancedPrimitives, VertexBufferBinding, GraphicsCapability.Instancing |
| 9.1.03 | `pf-sprites` | SpriteBatch Volume | XNA 4.0 | SpriteBatch.Draw, SpriteSortMode, SpriteBatch.Begin |
| 9.1.04 | `pf-state` | State Changes and Batching | CNA extension | BasicEffect.Texture, EffectPass.Apply, CNA::Diagnostics::GetProvider, CNA::Diagnostics::IDiagnosticsProvider.CaptureSnapshot, CNA::Diagnostics::Snapshot.metrics |
| 9.1.05 | `pf-buffers` | Buffer Uploads | XNA 4.0 | DynamicVertexBuffer.SetData, SetDataOptions.Discard, GraphicsDevice.DrawUserPrimitives, VertexBuffer |
| 9.1.06 | `pf-targets` | Render-Target Switches | CNA extension | GraphicsDevice.SetRenderTarget, RenderTarget2D, CNA::Diagnostics::GetProvider, CNA::Diagnostics::IDiagnosticsProvider.CaptureSnapshot, CNA::Diagnostics::Snapshot.metrics |
| 9.1.07 | `pf-overdraw` | Overdraw | XNA 4.0 | SpriteBatch.Draw, BlendState.NonPremultiplied |
| 9.1.08 | `pf-particles` | CPU Particles | CNA extension | PrimitiveType.PointListEXT, GraphicsDevice.DrawUserPrimitives |

**Renderer Validation** - Self-checking test images: every renderer must pass the same checks with the same stock calls

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 9.2.01 | `rv-geometry` | Geometry and Coverage | XNA 4.0 | RasterizerState.CullMode, GraphicsDevice.Viewport, GraphicsDevice.ScissorRectangle, GraphicsDevice.DrawUserPrimitives |
| 9.2.02 | `rv-sprites` | SpriteBatch Pixels | XNA 4.0 | SpriteBatch.Draw, SpriteEffects, SamplerState.PointClamp, SamplerState.LinearClamp |
| 9.2.03 | `rv-blend` | Blending | XNA 4.0 | BlendState, BlendFunction, ColorWriteChannels, Blend.BlendFactor |
| 9.2.04 | `rv-depth` | Depth Buffer | XNA 4.0 | DepthStencilState, CompareFunction, DepthFormat.Depth24 |
| 9.2.05 | `rv-shading` | Interpolation, Lighting and Fog | XNA 4.0 | BasicEffect.VertexColorEnabled, BasicEffect.DirectionalLight0, BasicEffect.FogEnabled |
| 9.2.06 | `rv-sampling` | Texture Sampling | XNA 4.0 | SamplerState.AddressU, TextureAddressMode |
| 9.2.07 | `rv-targets` | Render Targets | XNA 4.0 | RenderTarget2D, GraphicsDevice.SetRenderTarget, Texture2D.GetData |
| 9.2.08 | `rv-camera` | Camera Matrices | XNA 4.0 | Matrix.CreateLookAt, Matrix.CreatePerspectiveFieldOfView, Matrix.CreateOrthographic, BasicEffect.World |
| 9.2.09 | `rv-formats` | Surface Formats | XNA 4.0 | SurfaceFormat, Texture2D.SetData, Texture2D.GetData, PackedVector.Bgr565, PackedVector.Rgba64, PackedVector.HalfVector4 |
| 9.2.10 | `rv-effects` | Stock Effects | XNA 4.0 | BasicEffect, AlphaTestEffect, DualTextureEffect |

**Observation Deck** - Open-air rooms under the museum sky: the clock, a garden, a telescope and a weather station

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 9.3.01 | `rf-deck` | Sun, Moon and the Museum Clock | XNA 4.0 | Vector3, BasicEffect.EmissiveColor, MathHelper |
| 9.3.02 | `rf-garden` | Terrace Garden | XNA 4.0 | DynamicVertexBuffer, BlendState.NonPremultiplied, Texture2D, SoundEffectInstance |
| 9.3.03 | `rf-telescope` | Telescope | XNA 4.0 | Matrix.CreatePerspectiveFieldOfView, RenderTarget2D, BasicEffect.FogEnabled, Matrix.CreateLookAt |
| 9.3.04 | `rf-weather` | Weather Station | XNA 4.0 | PrimitiveType.LineList, DynamicVertexBuffer, SetDataOptions.Discard, SpriteBatch |

**Reference Scenes** - Named viewpoints that the visual regression tool renders and compares

| Room | Key | Title | Origin | APIs |
|---|---|---|---|---|
| 9.4.01 | `vr-index` | Reference Scenes | CNA extension | SpriteBatch.DrawString |
<!-- END MAP -->

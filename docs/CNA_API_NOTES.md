# CNA API survey (working notes)

> **Historical (2026-10-13).** These planning notes are kept for orientation only and are not maintained. Several statements
> are no longer true: the renderer list below names identities CNA has since retired (VULKAN, SDL_RENDERER, DIRECTX11 and
> METAL are gone; the live set is in `../cna/modules/core/include/CNA/GraphicsRendererType.hpp`), and compiled-effect
> support moved behind per-renderer `CNA_<RENDERER>_COMPILED_EFFECTS` options. The current public headers are the source of
> truth; the audited denominator is `docs/CNA_API_INVENTORY.md` and the evidence is `docs/CNA_COVERAGE_AUDIT.md`.

Compiled on 2026-10-13 by reading CNA's headers and docs while planning the museum. It is a **map, not a specification**: verify signatures against the headers in `../cna/modules/*/include` before relying on a name. Where it disagrees with CNA's `handoff.md` section 5, the handoff wins.

CNA API SURVEY for museum-simulator
Repo: /rv/data/development/github.com/libcna/cna   (read-only survey, nothing built or edited)
Header roots: modules/<m>/include/. Unless noted, "path" below is relative to modules/<module>/include/.
Tags: [XNA] = standard XNA 4.0 (property convention: getFooProperty()/setFooProperty()); [CNAEXT] = CNA extension (marked CNAEXT in comments and/or an EXT suffix). [PLAT] = platform- or build-option-specific. [EXP] = experimental/unstable.
Namespaces: Microsoft::Xna::Framework (math, Game), ::Graphics, ::Audio, ::Media, ::Input, ::Content, ::Storage. CNA::Graphics for graphics-ext. CNA:: for the capability and renderer-selection headers.

======================================================================
0. BUILD-LEVEL FACTS THAT DECIDE WHAT YOU CAN DEMO
======================================================================
- Renderer = CMake -DCNA_GRAPHICS_RENDERER=<X>. Multi-renderer builds use -DCNA_GRAPHICS_RENDERERS=...
- Live renderer identities (14): SDL_RENDERER, OPENGLES3, OPENGL33, WEBGL2, VULKAN, WEBGPU, HEADLESS, SOFTWARE, STUB, DIRECTX11, DIRECTX9, SDL_GPU, METAL, FNA3D.
- Maturity (core/include/CNA/GraphicsBackendMaturity.hpp, getGraphicsBackendMaturity):
  - Production: SDL_RENDERER, OPENGLES3, OPENGL33, VULKAN, DIRECTX9, DIRECTX11.
  - Supported: WEBGL2, HEADLESS, STUB, SDL_GPU, METAL.
  - Experimental: WEBGPU, SOFTWARE, FNA3D.
- Default CMake options:
  - CNA_CNAEXT=OFF. It gates modules/graphics-ext: DebugDraw, Ascii/CRT/Depth effects, ShaderPackage.
  - CNA_DEVICES=OFF. It gates modules/devices-ext (CNA::Devices::*).
  - CNA_ENABLE_NET=ON and CNA_ENABLE_GAMER_SERVICES follow it.
  - CNA_DIAGNOSTICS=OFF (OFF|STATS|FULL). CNA_BUILD_INSPECTOR=OFF.
  - CNA_ENABLE_VIDEO=AUTO (FFmpeg). CNA_ENABLE_FONT_PIPELINE=AUTO (FreeType, build-time only).
  - CNA_<RENDERER>_COMPILED_EFFECTS=OFF for every renderer (see 1.4).
  - CNA_BUILD_C_API=OFF. CNA_PLATFORM=SDL3 default. CNA_AUDIO_PLATFORM selects the audio backend.
- Build-mode switch for the ShaderEffect package constructors: they exist only under #ifdef CNA_CNAEXT.
- Compile-time identity: CNA::getCurrentGraphicsRendererType() (constexpr, core/include/CNA/GraphicsRendererType.hpp). It is the build default only.
- Frozen or stable: CNB container 1.0 and the existing built-in CNB schemas are frozen. XNB output is XNA-compatible.
- Experimental: the custom C++ content-pipeline component API and the C ABI (modules/c-api).
- Stale docs: several older docs are out of date. The feature matrix, easygl_bugs.md, migration-guide.md and texture3d-texturecube-support.md have rows that are stale. The authoritative current bug list is handoff.md section 5.

======================================================================
1. GRAPHICS EFFECTS (graphics/include/Microsoft/Xna/Framework/Graphics/)
======================================================================
1.0 Common [XNA]
- Effect.hpp
  - Effect : GraphicsResource. Members: getCurrentTechniqueProperty/setCurrentTechniqueProperty(EffectTechnique*), getParametersProperty() (EffectParameterCollection, indexable by name, e.g. ["Threshold"]->SetValue(...)), getTechniquesProperty(), Clone().
  - [CNAEXT] Apply(), GetTypeName(), IsStockEffectEXT().
  - Pattern: for (auto& pass : effect.getCurrentTechniqueProperty()->getPassesProperty()) { pass.Apply(); device.Draw...; }
- EffectTechnique.hpp: getNameProperty, getPassesProperty, getAnnotationsProperty.
- EffectPass.hpp: getNameProperty, getAnnotationsProperty, Apply().
- EffectParameter.hpp:
  - Properties: getNameProperty, getSemanticProperty, getRowCountProperty, getColumnCountProperty, getParameterClassProperty, getParameterTypeProperty, getElementsProperty, getStructureMembersProperty, getAnnotationsProperty.
  - GetValueBoolean / Int32 / Single / String / Matrix / MatrixTranspose / Quaternion / Vector2 / Vector3 / Vector4 / Texture2D / Texture3D / TextureCube.
  - *Array(count) variants of the getters.
  - SetValue overloads: bool, int, float, string, Matrix, Quaternion, Vector2/3/4, vectors of them, Texture*, Texture2D*, Texture3D*, TextureCube*.
  - SetValueTranspose.
- Also present: EffectParameterCollection/TechniqueCollection/PassCollection/Annotation(Collection).hpp, EffectParameterClass, EffectParameterType (Void, Bool, Int32, Single, String, Texture, Texture1D, Texture2D, Texture3D, TextureCube).
- Interfaces: IEffectMatrices (World/View/Projection), IEffectFog (FogColor/FogEnabled/FogStart/FogEnd), IEffectLights (AmbientLightColor, DirectionalLight0/1/2, LightingEnabled, EnableDefaultLighting()). Each uses the getXProperty/setXProperty pattern.
- DirectionalLight.hpp: getDiffuseColorProperty, getDirectionProperty, getSpecularColorProperty, getEnabledProperty (and setters).
  - Defaults: Direction is Zero and Enabled is false for a fresh effect; EnableDefaultLighting() sets the XNA constants.

1.1 BasicEffect.hpp [XNA]
- Construction: BasicEffect(GraphicsDevice&). Clone().
- World/View/Projection are PUBLIC FIELDS: effect.World, effect.View, effect.Projection (Matrix). Accessors getWorldProperty/setWorldProperty etc. also exist.
- VertexColorEnabled is a public bool field, DEFAULT FALSE. Also getVertexColorEnabledProperty/setVertexColorEnabledProperty.
- Properties (get/set pairs):
  - TextureEnabled (default false), Texture (Texture2D*).
  - DiffuseColor (Vector3), EmissiveColor (Vector3), SpecularColor (Vector3), SpecularPower (float), Alpha (float).
  - LightingEnabled (default false), AmbientLightColor, DirectionalLight0/1/2 (getters return DirectionalLight&), EnableDefaultLighting().
  - PreferPerPixelLighting (per-pixel lighting toggle).
  - FogEnabled, FogColor, FogStart, FogEnd.
- [CNAEXT] on BasicEffect:
  - SetOwnedTexture(std::shared_ptr<Texture2D>) keeps the texture alive.
  - IShadowReceiverEXT: setShadowMapEXT, setLightViewProjectionEXT, setShadowsEnabledEXT, setShadowDepthBiasEXT, setShadowFilterRadiusEXT, setShadowCascadesEXT (ShadowCascadeStateEXT, kMaxCascades=4). These are receiver state only; there is no shadow-generation engine (removed).
  - setPunctualLightEXT(const PunctualLightEXT&) / getPunctualLightEXT().
- PunctualLightEXT.hpp:
  - Kind: PunctualLightKindEXT {None, Point, Spot}.
  - Fields: Position, Direction (default 0,-1,0), DiffuseColor, Range (20), InnerAngle (0.35), OuterAngle (0.5), ShadowCube (TextureCube*), ShadowMap (Texture2D*), ShadowViewProjection, ShadowDepthBias (0.004).
  - One punctual light per draw. The shadow textures are optional (null = lit but unshadowed). It is "distance" shadow data you produce yourself.
- Draw flow: set Vertex/Index buffers, then effect.Apply or pass.Apply(), then DrawPrimitives.
- BasicEffect gotchas:
  - XNA default: no lighting, no texture, no vertex colour.
  - DiffuseColor above 1 is saturated per vertex on the unlit path (docs/basiceffect-support.md section 6).
  - Vulkan refuses some lit vertex layouts; stick to Position+Normal(+Texture) / PositionNormalTexture / PositionColor / PositionColorTexture / PositionTexture.
  - DIRECTX9 validates the declaration by required semantics rather than exact stride; all 32 stock BasicEffect buckets are reachable. Its SM3 extension variants add point/spot lighting and directional/point/spot shadow reception.

1.2 Other stock effects [XNA] (same file naming: <Name>.hpp)
- AlphaTestEffect: World/View/Projection, DiffuseColor, Alpha, FogColor/Enabled/Start/End, Texture, VertexColorEnabled, AlphaFunction (CompareFunction), ReferenceAlpha (int). SetOwnedTexture [CNAEXT].
- DualTextureEffect: World/View/Projection, DiffuseColor, Alpha, Fog*, Texture, Texture2 (getTexture2Property/setTexture2Property), VertexColorEnabled. [CNAEXT] SetOwnedTexture/SetOwnedTexture2.
- EnvironmentMapEffect: World/View/Projection, DiffuseColor, EmissiveColor, Alpha, AmbientLightColor, LightingEnabled, DirectionalLight0/1/2, EnableDefaultLighting, Fog*, Texture, EnvironmentMap (TextureCube*), EnvironmentMapAmount, EnvironmentMapSpecular (Vector3), FresnelFactor. [CNAEXT] SetOwnedTexture/SetOwnedEnvironmentMap.
- SkinnedEffect: static const int MaxBones = 72. World/View/Projection, DiffuseColor, EmissiveColor, SpecularColor, SpecularPower, Alpha, PreferPerPixelLighting, lights as above, Fog*, Texture, WeightsPerVertex (1/2/4), SetBoneTransforms(const std::vector<Matrix>&), GetBoneTransforms(count).
  - [CNAEXT] public field bool VertexColorEnabled (default false), setPunctualLightEXT, shadow receiver API.
- SpriteEffect.hpp: SpriteEffect(GraphicsDevice&). [CNAEXT] IsExactStockSpriteEffectEXT().
- Clone() on every effect.

1.3 CNAEXT effects (core graphics module)
- PbrEffect.hpp [CNAEXT], and SkinnedPbrEffect.hpp [CNAEXT] (same API plus skinning, 68-byte stride; also SetBoneTransforms).
  - Has the IEffectMatrices, IEffectFog, IEffectLights, IShadowReceiverEXT interfaces.
  - Properties: DiffuseColor (base colour factor), Alpha, Texture, NormalMap, MetallicRoughnessMap, EmissiveMap, OcclusionMap, MetallicFactor, RoughnessFactor, EmissiveFactor, NormalScaleEXT, OcclusionStrengthEXT, IorEXT, SpecularFactorEXT, SpecularColorFactorEXT, plus Specular map/UV-set/transform/sRGB properties.
  - Also AlphaModeEXT (Opaque/Mask/Blend), AlphaCutoffEXT, DoubleSidedEXT, per-slot TextureCoordinateSet/TextureTransform (TextureTransformEXT), BaseColorTextureIsSrgbEXT, EmissiveTextureIsSrgbEXT, EncodeOutputToSrgbEXT.
  - ImageBasedLightEXT (ImageBasedLightEXT.hpp: struct with PrefilteredMipCount, Intensity, IsValidEXT(), cube textures) via setImageBasedLightEXT/getImageBasedLightEXT. Also setPunctualLightEXT and the shadow receiver API.
  - Public field bool VertexColorEnabledEXT (default false).
  - Each map has a SetOwned...Map shared_ptr variant.
  - Gotcha: PbrEffect defaults to ZERO ambient and all lights disabled, so a glTF model renders BLACK until you call EnableDefaultLighting() or set lights (docs/gltf-conventions.md). Lights from KHR_lights_punctual import as up to 3 directional lights.
- ShaderEffect.hpp [CNAEXT] (class ShaderEffect : Effect, IEffectMatrices). See 1.4.
- ColorMatrixEffect.hpp [CNAEXT]: SetColorMatrix(std::array<float,16>), SetColorOffset, SetGrayscale, Reset. Documented as a SOFTWARE-renderer extension only. Do not demo on GL or Vulkan.
- EffectMaterial.hpp [CNAEXT]: EffectMaterial(Effect& cloneSource), RetainParameterTextureEXT.
- graphics-ext effects (need CNA_CNAEXT=ON; headers graphics-ext/include/CNA/Graphics/, namespace CNA::Graphics):
  - CRTEffect : ShaderEffect. Props: ScanlineIntensity, Curvature, VignetteIntensity, MaskIntensity, MaskType (CRTMaskType {None, ApertureGrille, ShadowMask}).
    - These use plain getter/setter names (getScanlineIntensity()/setScanlineIntensity()), NOT the ...Property() form.
    - Usable as the Effect* in SpriteBatch::Begin.
  - DepthEffect : ShaderEffect. getMode/setMode with DepthEffectMode {Color16Bit, Color8Bit, Grayscale4Bit, Grayscale2Bit, Grayscale1Bit, Palette256, Palette16}; getDitherMode/setDitherMode with DitherMode {None, Bayer4x4, Bayer8x8}. SpriteBatch effect.
  - AsciiPostProcessEffect. Constructed with a GraphicsDevice&. setCellSize(w,h)/getCellSize, setQuantizeMode(AsciiQuantizeMode {BlackWhite, Color}), Draw(Texture2D& source) and Draw(source, Rectangle destination), GetLastGridDimensions(cols, rows).
    - It is CPU quantization via GetData. Slow for large frames; the source is typically a RenderTarget2D.
  - DebugDraw: see section 9.

1.4 Custom effects: loading, compiling, renderer support
- (a) Source-based ShaderEffect [CNAEXT]. Capability: GraphicsCapability::CustomEffects / RendererFeature::ShaderEffects / ShaderEffectSourceExecution.
  - Constructors: ShaderEffect(GraphicsDevice&, const std::string& vertSrc, const std::string& fragSrc). With CNA_CNAEXT also (device, ShaderCodeEXT vertex, ShaderCodeEXT fragment) and (device, ShaderPackageEXT).
  - API: IsEffectValid(), HasRenderer(), GetCompileErrorEXT(), GetShaderDiagnosticsEXT() (vector<ShaderDiagnosticEXT>), GetSelectedShaderLanguageEXT(), DeclareUniformBlockEXT(...).
    - Uniforms: SetUniformMat4(name, const float*) (column-major) / Vec4 / Vec3 / Vec2 / Float / Int, and Float/Vec2/Vec3/Mat4 Array variants.
    - Textures: SetTexture(int unit, Texture2D& / TextureCube& / Texture3D&).
    - Matrices: getWorldProperty/setWorldProperty/View/Projection. Also getVertexSourceProperty, getFragmentSourceProperty.
  - On OPENGLES3 / OPENGL33 / WEBGL2 (EasyGL) you hand it GLSL ES 3.00 source text compiled at runtime (#version 300 es; OPENGL33 adapts it to 330 core).
    - Vertex attributes are layout(location=N) in the order of your VertexDeclaration. Uniform names are yours; set them with SetUniform*().
    - Examples with full source: modules/renderers/easygl/examples/easygl_*_shader_test.cpp (35 files), e.g. easygl_blur_shader_test.cpp (attributes aPosition/aTexCoord, uniforms set by name). SpriteBatch-effect ABI example: modules/graphics-ext/src/shaders/post_process/{fullscreen.es.vert.glsl, crt.es.frag.glsl} (attributes aPos/aTexCoord/aColor, uniform projection, sampler texture1).
    - The GLSL is compiled as written: GL clip depth is [-1,1] (stock effects remap to XNA's [0,1]).
  - VULKAN: ShaderEffect takes SPIR-V, not GLSL text (a GLSL string is refused). GetShaderDialectEXT() == ShaderDialectEXT::SpirV, ExecutesShaderEffectSourceEXT() false (docs/vulkan-renderer.md). Use ShaderPackageEXT with SPIR-V variants.
  - Other renderers: HLSL (DX), MSL (Metal), WGSL (WebGPU) payloads via ShaderPackageEXT. Query device.SupportsShaderLanguageEXT(ShaderLanguageEXT, ShaderStageEXT) and device.GetShaderDialectEXT() (CNA/ShaderDialectEXT.hpp).
  - Dialect enums (graphics/include/CNA/ShaderLanguageEXT.hpp): ShaderLanguageEXT {Unknown, GlslDesktop, GlslEs, GlslVulkan, Hlsl, Msl, Wgsl, SpirV, Dxil}; ShaderStageEXT {Unknown, Vertex, Fragment}.
  - ShaderPackageEXT / ShaderCodeEXT: portable payload variants. Selection: package.selectFor(...). Exceptions: ShaderCompilationExceptionEXT.
  - SOFTWARE and HEADLESS/STUB: custom ShaderEffect is outside scope (no source execution).
- (b) Compiled XNA/FNA .fx bytecode: Effect(GraphicsDevice&, const std::vector<SharpRuntime::bytecs>&) or content.Load<std::shared_ptr<Effect>>("name") from an .xnb. Capability: GraphicsCapability::CompiledEffects / RendererFeature::CompiledXnaEffects. See docs/fx-compiled-effects.md.
  - It is an XNA/FNA Direct3D 9 Effect Framework binary (.fxb or XNB Effect payload). The runtime does NOT compile HLSL .fx source: CNA embeds no HLSL compiler.
  - Offline: cna-content can compile .fx via an external fxc (--fx-compiler, optionally under wine), --format xnb only. CNB has no Effect schema.
  - It works on EasyGL (OPENGLES3, OPENGL33, WEBGL2) only if CNA was built with -DCNA_EASYGL_COMPILED_EFFECTS=ON (OFF by default, pulls MojoShader). Same for VULKAN, SDL_GPU, WEBGPU, SOFTWARE, DX9, DX11, METAL.
    - FNA3D has it natively. Without the option the capability is false and construction throws NotSupportedException. It is never silently replaced by a stock shader.
    - Always check device.SupportsCapability(CNA::GraphicsCapability::CompiledEffects) first.
  - EasyGL specifics: after a GL context recreation a compiled Effect must be recreated from bytes; a compiled effect cannot sample the RenderTarget2D it draws into.
  - Untrusted bytecode is hardened by fuzzing but not proven safe. MonoGame MGFX is rejected.
  - MaxBones = 72 applies to the skinned path.
- Runtime-from-source summary: only ShaderEffect (GLSL/SPIR-V/etc. per renderer). Compiled .fx needs offline compile plus the build option.

======================================================================
2. GRAPHICS RESOURCES AND STATE (graphics/include/Microsoft/Xna/Framework/Graphics/)
======================================================================
2.1 Textures [XNA unless noted]
- Texture.hpp: base. getFormatProperty, getLevelCountProperty. [CNAEXT] static helpers GetFormatSizeEXT, ValidateFormat, Is*AllowedByProfileEXT.
- Texture2D.hpp
  - Constructors: Texture2D(GraphicsDevice&, int w, int h); Texture2D(GraphicsDevice&, w, h, bool mipMap, SurfaceFormat format). [CNAEXT] default ctor, (assetName[, device]); copy/move.
  - getWidthProperty/getHeightProperty/getBoundsProperty (getLevelCountProperty, getFormatProperty inherited).
  - SetData(const T* data, int elementCount); SetData(int level, const Rectangle* rect, const T* data, int startIndex, int elementCount) with T = Color and the packed vectors (Bgr565, Bgra4444, Bgra5551, Rgba1010102, Alpha8, Rg32, Rgba64, NormalizedByte2/4, HalfSingle/2/4, Vector2/4, float, uint8, uint16) and trivially copyable T. rect==nullptr means whole level.
  - GetData(...) mirrors it (Color* data, int elementCount / int startIndex, int elementCount / int level, const Rectangle* rect, T*, startIndex, count). Format must match element size (ValidateGetDataFormat).
  - static Texture2D FromStream(GraphicsDevice&, System::IO::Stream&) (+ overload with size), SaveAsPng(Stream*, w, h), SaveAsJpeg(Stream*, w, h). Returned by value, not a pointer.
  - [CNAEXT] DDSFromStreamEXT, SaveAsPng(filename), SaveAsJpeg(filename), SetDataPointerEXT.
  - Mipmaps: a mipMap=true texture has a full chain allocated. On EasyGL, level 0 SetData may not auto-generate lower mips (easygl_bugs.md, "still confirmed 2026-07-11"). Upload each level yourself with SetData(level,...). RenderTargets do auto-mip on unbind.
- Texture3D.hpp: Texture3D(device, w, h, depth, bool mipMap, SurfaceFormat); getWidthProperty/getHeightProperty/getDepthProperty; SetData(level, left, top, right, bottom, front, back, data, startIndex, count) and whole-texture SetData(data, count); matching GetData. Texture3D sampling is queryable via RendererFeature::Texture3DSampling (see 11); the old doc claiming "never" is stale.
- TextureCube.hpp: TextureCube(device, int size, bool mipMap, SurfaceFormat); getSizeProperty; SetData(CubeMapFace face, const T* data, int count); SetData(face, level, rect, data, startIndex, count); GetData mirrors. [CNAEXT] static DDSFromStreamEXT. CubeMapFace {PositiveX, NegativeX, PositiveY, NegativeY, PositiveZ, NegativeZ}. Cube sampled by EnvironmentMapEffect and ShaderEffect::SetTexture.
- RenderTarget2D.hpp: RenderTarget2D(device, w, h) and (device, w, h, bool mipMap, SurfaceFormat preferredFormat, DepthFormat preferredDepth[, int preferredMultiSampleCount[, RenderTargetUsage]]); getRenderTargetUsageProperty, getDepthStencilFormatProperty, getMultiSampleCountProperty, getIsContentLostProperty; ContentLost event. It IS a Texture2D (sample it, SaveAsPng, GetData).
- RenderTargetCube.hpp: RenderTargetCube(device, size, mipMap, SurfaceFormat, DepthFormat[, ms, usage]). RenderTargetBinding.hpp: RenderTargetBinding(RenderTarget2D*) / (RenderTargetCube*, CubeMapFace) for MRT (device.SetRenderTargets(std::vector<RenderTargetBinding>)).
- Preferred formats can fall back (e.g. Alpha8 target becomes Color); check RenderTarget2D.getFormatProperty() or device.SupportsSurfaceFormatAsRenderTargetEXT(fmt).
- SurfaceFormat {Color, Bgr565, Bgra5551, Bgra4444, Dxt1, Dxt3, Dxt5, NormalizedByte2, NormalizedByte4, Rgba1010102, Rg32, Rgba64, Alpha8, Single, Vector2, Vector4, HalfSingle, HalfVector2, HalfVector4, HdrBlendable} [XNA]; [CNAEXT]: ColorBgraEXT, ColorSrgbEXT, Dxt5SrgbEXT, Bc7EXT, Bc7SrgbEXT, ByteEXT, UShortEXT.
- DepthFormat {None, Depth16, Depth24, Depth24Stencil8}. RenderTargetUsage {DiscardContents, PreserveContents, PlatformContents} (EasyGL treats all as Discard).
- PackedVector headers (Graphics/PackedVector/*.hpp) [XNA]: Alpha8, Bgr565, Bgra4444, Bgra5551, Byte4, HalfSingle, HalfVector2, HalfVector4, NormalizedByte2/4, NormalizedShort2/4, Rg32, Rgba1010102, Rgba64, Short2, Short4, HalfTypeHelper, IPackedVector. Properties via getPackedValueProperty/setPackedValueProperty.

2.2 Buffers, declarations, vertex types
- VertexBuffer.hpp: VertexBuffer(GraphicsDevice&, const VertexDeclaration&, int vertexCount, BufferUsage) (+ (device, Type/decl, ...) overloads; [CNAEXT] VertexBuffer(device, vertexCount)). SetData/GetData templates for any IVertexType-derived TVertex and typed overloads for built-ins; SetData(int offsetInBytes, data, startIndex, count, int vertexStride). getVertexCountProperty. [CNAEXT] SetDataRaw, SetDataRawAtEXT, GetDataRawEXT.
- DynamicVertexBuffer.hpp: SetData(data, startIndex, count, SetDataOptions) with SetDataOptions {None, Discard, NoOverwrite}; getIsContentLostProperty/ContentLost.
- IndexBuffer.hpp: IndexBuffer(device, IndexElementSize {SixteenBits=0, ThirtyTwoBits=1}, int indexCount, BufferUsage) and (device, Type, count, usage); SetData/GetData with uint16/uint32. DynamicIndexBuffer.hpp. 32-bit indices work on every 3D renderer except SDL_RENDERER (2D-only).
- BufferUsage {None, WriteOnly}.
- VertexDeclaration.hpp: VertexDeclaration(std::initializer_list<VertexElement>), (std::vector<VertexElement>), (int vertexStride, elements...), GetVertexElements(), vertex stride getter. VertexElement.hpp: VertexElement(int offset, VertexElementFormat, VertexElementUsage, int usageIndex).
- VertexElementFormat {Single, Vector2, Vector3, Vector4, Color, Byte4, Short2, Short4, NormalizedShort2, NormalizedShort4, HalfVector2, HalfVector4}. VertexElementUsage {Position, Color, TextureCoordinate, Normal, Binormal, Tangent, BlendIndices, BlendWeight, Depth, Fog, PointSize, Sample, TessellateFactor}. Depth/Fog/PointSize/Sample/TessellateFactor/Binormal are not consumed by stock shaders (docs/vertex-format-support.md).
- IVertexType.hpp: your struct derives from it and overrides getVertexDeclarationProperty() const (plus a static getVertexDeclarationStatic() by convention). [CNAEXT] VertexTypeRegistryEXT.hpp: CNA::Graphics::VertexTypeRegistryEXT::Register<TVertex>() (so FromType-style lookups work). TVertex must be default-constructible.
- Built-in vertex types [XNA]: VertexPositionColor {Position, Color}, VertexPositionColorTexture {Position, Color, TextureCoordinate}, VertexPositionNormalTexture {Position, Normal, TextureCoordinate}, VertexPositionTexture {Position, TextureCoordinate}. Each has a static getVertexDeclarationStatic().
  - [CNAEXT]: VertexPositionNormalTextureSkinned (52 B), VertexPositionNormalTangentTexture, VertexPositionNormalTangentTextureSkinned (68 B, the Pbr/SkinnedPbr layout).
- DrawUserPrimitives / DrawUserIndexedPrimitives take stock vertex arrays or any IVertexType type; for non-stock types pass a VertexDeclaration (the call without one will not compile). std::vector/array overloads exist.
- PrimitiveType {TriangleList=0, TriangleStrip=1, LineList=2, LineStrip=3, PointListEXT=4 [CNAEXT]}. Point size is NOT settable (each point covers at least its pixel; PointSize usage unsupported). GraphicsDevice::PrimitiveVerts(type, count) [CNAEXT] static.

2.3 Instancing
- VertexBufferBinding.hpp: VertexBufferBinding(VertexBuffer*, int vertexOffset=0, int instanceFrequency=0). getVertexBufferProperty/getVertexOffsetProperty/getInstanceFrequencyProperty.
- device.SetVertexBuffers(std::vector<VertexBufferBinding>) then device.DrawInstancedPrimitives(PrimitiveType, int baseVertex, int minVertexIndex, int numVertices, int startIndex, int primitiveCount, int instanceCount). Indexed only; needs an index buffer set.
  - Stock effects use the first four elements of the per-instance stream as a per-instance world matrix (CNA convention; confirmed in Metal doc, EasyGL REMED-GFX-122).
  - [CNAEXT] DrawInstancedPrimitivesBaseInstanceEXT(..., instanceCount, firstInstance); needs RendererFeature::BaseInstanceDrawing.
- Capabilities: GraphicsCapability::Instancing, MultiStreamVertexInput. SDL_GPU lacks both. Vulkan has known multi-stream bugs (handoff 5.1 item 8).

2.4 OcclusionQuery.hpp [XNA]
- OcclusionQuery(GraphicsDevice&); Begin(); End(); getIsCompleteProperty(); getPixelCountProperty() (throws InvalidOperationException if not complete). [CNAEXT] isPixelCountPreciseEXT().
- OPENGLES3/WEBGL2 return boolean-only counts (0 or 1) because ES 3.0 lacks GL_SAMPLES_PASSED; desktop GL (OPENGL33) and Vulkan give true counts. Check isPixelCountPreciseEXT() before using PixelCount as an area. SDL_RENDERER and SDL_GPU cannot construct one.
- Vulkan: IsComplete does not flush; poll after Present (handoff 5.1 item 8).

2.5 State objects [XNA]
- All have a default ctor, copy ctor [CNAEXT], and expose properties via getXProperty/setXProperty. Presets are static const objects.
- BlendState.hpp
  - Presets: BlendState::Additive, AlphaBlend, NonPremultiplied, Opaque.
  - Properties: ColorSourceBlend, ColorDestinationBlend, ColorBlendFunction, AlphaSourceBlend, AlphaDestinationBlend, AlphaBlendFunction, ColorWriteChannels (+1/+2/+3 for MRT), BlendFactor (Color), MultiSampleMask (int).
  - Blend {One, Zero, SourceColor, InverseSourceColor, SourceAlpha, InverseSourceAlpha, DestinationColor, InverseDestinationColor, DestinationAlpha, InverseDestinationAlpha, BlendFactor, InverseBlendFactor, SourceAlphaSaturation}. BlendFunction {Add, Subtract, ReverseSubtract, Min, Max}. ColorWriteChannels {None=0, Red=1, Green=2, Blue=4, Alpha=8, All=15}.
- DepthStencilState.hpp
  - Presets: DepthStencilState::Default, DepthRead, None.
  - Properties: DepthBufferEnable, DepthBufferWriteEnable, DepthBufferFunction, StencilEnable, StencilFunction, StencilMask, StencilWriteMask, ReferenceStencil, StencilFail, StencilDepthBufferFail, StencilPass, TwoSidedStencilMode, CounterClockwiseStencilFunction, CounterClockwiseStencilFail, CounterClockwiseStencilDepthBufferFail, CounterClockwiseStencilPass.
  - CompareFunction {Always, Never, Less, LessEqual, Equal, GreaterEqual, Greater, NotEqual}. StencilOperation {Keep, Zero, Replace, Increment, Decrement, IncrementSaturation, DecrementSaturation, Invert}.
  - Stencil needs a Depth24Stencil8 back buffer (setPreferredDepthStencilFormatProperty). Depth24 has no stencil.
- RasterizerState.hpp
  - Presets: RasterizerState::CullClockwise, CullCounterClockwise (default), CullNone.
  - Properties: CullMode, FillMode, DepthBias, SlopeScaleDepthBias, ScissorTestEnable, MultiSampleAntiAlias.
  - CullMode {None, CullClockwiseFace, CullCounterClockwiseFace}. FillMode {Solid, WireFrame}.
  - WireFrame is emulated with GL_LINES on EasyGL and VK_POLYGON_MODE_LINE on Vulkan. DepthBias is in XNA normalized units; Vulkan applies it unscaled so small values are invisible on VULKAN (handoff 5.1 item 1); other renderers scale it properly.
  - Winding gotcha: default is CCW cull; a mis-wound mesh renders empty (house3d_demo sets RasterizerState::CullNone for this reason).
- SamplerState.hpp
  - Presets: SamplerState::AnisotropicClamp, AnisotropicWrap, LinearClamp, LinearWrap, PointClamp, PointWrap.
  - Properties: AddressU, AddressV, AddressW, Filter, MaxAnisotropy (default 4), MaxMipLevel, MipMapLevelOfDetailBias.
  - TextureAddressMode {Wrap, Clamp, Mirror}. TextureFilter {Linear, Point, Anisotropic, LinearMipPoint, PointMipLinear, MinLinearMagPointMipLinear, MinLinearMagPointMipPoint, MinPointMagLinearMipLinear, MinPointMagLinearMipPoint}.
  - Mip-aware filters on a single-level texture are the caller's problem.
  - Static presets are immutable/shared; construct your own SamplerState/BlendState instance to customise.
- device.getSamplerStatesProperty()[i] and getTexturesProperty()[i], slots 0..15 (SamplerStateCollection::MaxSamplers = 16, TextureCollection::MaxTextures = 16). Vertex-texture collections: getVertexTexturesProperty/getVertexSamplerStatesProperty.
- Viewport.hpp: Viewport(x, y, w, h) / (Rectangle); getX/Y/Width/Height/MinDepth/MaxDepth/AspectRatio/Bounds/TitleSafeAreaProperty; Project(...), Unproject(...) (Vector3, Matrix proj, view, world).
- ClearOptions {Target=1, DepthBuffer=2, Stencil=4}.

2.6 GraphicsDevice.hpp (all [XNA] unless noted)
- State and properties: getGraphicsDeviceStatusProperty, getAdapterProperty, getGraphicsProfileProperty, getPresentationParametersProperty, getDisplayModeProperty, getTexturesProperty, getSamplerStatesProperty, getBlendStateProperty/setBlendStateProperty, getDepthStencilStateProperty/set, getRasterizerStateProperty/set, getScissorRectangleProperty/set (needs RasterizerState.ScissorTestEnable), getViewportProperty/set, getBlendFactorProperty/set (Color), getMultiSampleMaskProperty/set (int), getReferenceStencilProperty/set, getIndicesProperty/set (const IndexBuffer*).
- Clear: Clear(Color); Clear(ClearOptions, Color, float depth, int stencil); Clear(ClearOptions, Vector4, ...). [CNAEXT] Clear(float r,g,b,a), Clear(Color, depth).
- Present(); Present(optional<Rectangle> src, optional<Rectangle> dst, uintptr_t overrideWindowHandle); Reset() / Reset(PresentationParameters) / Reset(pp, GraphicsAdapter&).
- Readback: GetBackBufferData(Color* data, int elementCount), (T*, startIndex, count), (const Rectangle* rect, T*, startIndex, count). Not available on HEADLESS/STUB.
- Binding: SetRenderTarget(RenderTarget2D*) (nullptr = backbuffer), SetRenderTarget(RenderTargetCube*, CubeMapFace), SetRenderTargets(vector<RenderTargetBinding>), GetRenderTargets(), SetVertexBuffer(const VertexBuffer*[, int vertexOffset]), SetVertexBuffers(vector<VertexBufferBinding>), GetVertexBuffers(). [CNAEXT] SetIndexBuffer/GetVertexBuffer/GetIndexBuffer, Indices(const IndexBuffer*) setter shorthand as in house3d_demo.
- Draw: DrawPrimitives(type, vertexStart, primitiveCount); DrawIndexedPrimitives(type, baseVertex, minVertexIndex, numVertices, startIndex, primitiveCount); DrawInstancedPrimitives (see 2.3); DrawUserPrimitives / DrawUserIndexedPrimitives (many overloads).
- [CNAEXT] device methods:
  - Quick toggles: SetDepthTestEnabled(bool), SetBlendEnabled(bool), SetDepthWriteEnabled(bool), SetStringMarkerEXT(string).
  - Policy and window: SetGraphicsProfileEXT, SetContextRecoveryEnabled, GetLogicalSizeEXT(w,h), GetMaxTextureDimension(), SetUnsupported3DGraphicsCallBehavior(CNA::Unsupported3DGraphicsCallBehavior {Throw, WarnAndStub}) / Get...().
  - Colour space: GetDisplayColorSpaceEXT / SetDisplayColorSpaceEXT / SupportsDisplayColorSpaceEXT (DisplayColorSpace {Srgb, Scrgb, Hdr10}).
  - Renderer identity and capability queries: see section 11.
- GraphicsAdapter.hpp: static getDefaultAdapterProperty(), getAdaptersProperty(); getCurrentDisplayModeProperty, getSupportedDisplayModesProperty, getDescriptionProperty, getDeviceNameProperty, getVendorIdProperty, getDeviceIdProperty, getIsWideScreenProperty, getMonitorHandleProperty; IsProfileSupported(GraphicsProfile); QueryRenderTargetFormat(...) / QueryBackBufferFormat(...); static UseNullDevice/UseReferenceDevice flags; [CNAEXT] GetCurrentDeviceTypeEXT(), GetRequiredRendererEXT().
- DisplayMode.hpp: getWidthProperty/getHeightProperty/getAspectRatioProperty/getFormatProperty/getTitleSafeAreaProperty. DisplayModeCollection.hpp: range-for, getCountProperty [CNAEXT].
- PresentationParameters.hpp: BackBufferFormat/Width/Height, Bounds, DeviceWindowHandle, DepthStencilFormat, IsFullScreen, MultiSampleCount, PresentationInterval (PresentInterval {Default, One, Two, Immediate}), DisplayOrientation, RenderTargetUsage, Clone(). [CNAEXT] HeadlessEXT.
- GraphicsProfile {Reach, HiDef}. Reach limits MRT to 1; HiDef allows 4. Texture, vertex-stream and primitive-count profile limits are enforced.
- GraphicsDeviceStatus {Normal, Lost, NotReset}. Exceptions: DeviceLostException, DeviceNotResetException, NoSuitableGraphicsDeviceException.
- IGraphicsDeviceService.hpp: device created/reset/disposing/resetting events.
- Resource lifetime: GraphicsResource.hpp. ResourceCreatedEventArgs / ResourceDestroyedEventArgs. Content-lost: IContentLosable (internal).

======================================================================
3. SPRITEBATCH / SPRITEFONT (graphics/include/Microsoft/Xna/Framework/Graphics/)
======================================================================
- SpriteBatch.hpp: SpriteBatch(GraphicsDevice&) [XNA]. [CNAEXT] default ctor and ctor taking a renderer.
- Begin() overloads:
  - Begin(); Begin(SpriteSortMode, BlendState).
  - Begin(SpriteSortMode, BlendState, const SamplerState*, const DepthStencilState*, const RasterizerState*).
  - Same with a trailing Effect*; same with Effect* and Matrix transformMatrix. Each also exists with `const BlendState*` for the first parameter.
  - Null state pointers mean defaults. Default blend is AlphaBlend (premultiplied convention).
- End().
- SpriteSortMode {Deferred, Immediate, Texture, BackToFront, FrontToBack}. SpriteEffects {None=0, FlipHorizontally=1, FlipVertically=2}: NO bitwise operators; flip both with static_cast<SpriteEffects>(3).
- Draw overloads, all taking `const Texture2D&` (a reference):
  - (tex, Vector2 pos, Color) and (tex, pos, optional<Rectangle> src, Color).
  - (tex, pos, src, Color, float rotation, Vector2 origin, float scale, SpriteEffects, float layerDepth).
  - Same with Vector2 scale.
  - (tex, const Rectangle& dest, Color) and (tex, dest, optional<Rectangle> src, Color).
  - (tex, dest, src, color, rotation, origin, effects, layerDepth).
  - [CNAEXT] Draw(tex, float x, float y) and (tex, Rectangle dest, Rectangle src, Color) variants.
- DrawString overloads (std::string UTF-8, or System::Text::StringBuilder): (font, text, Vector2 pos, Color); (…, rotation, Vector2 origin, float scale, SpriteEffects, float layerDepth); same with Vector2 scale. SpriteBatch::DrawMeshEXT was REMOVED (do not use).
- SpriteBatch.End leaves the blend/depth/raster state it set on the device. Reset device.setBlendStateProperty/DepthStencil/Rasterizer before 3D after a SpriteBatch pass (also SamplerStates). EasyGL: a full-backbuffer SpriteBatch draw before 3D in the same frame can break that frame's 3D (migration-guide, root cause not isolated); draw 3D first, then sprites.
- SDL_RENDERER ignores TextureAddressMode Wrap/Mirror in SpriteBatch (draws clamped).
- SpriteFont.hpp [XNA]
  - getCharactersProperty() (vector<charcs>), getDefaultCharacterProperty()/setDefaultCharacterProperty(optional<charcs>), getLineSpacingProperty()/set, getSpacingProperty()/set, MeasureString(const String&) / (StringBuilder) returning Vector2, getIndexForCharacter.
  - [CNAEXT] constructor SpriteFont(Texture2D, glyphBounds, cropping, characters, lineSpacing, spacing, kerning, defaultCharacter) for hand-built fonts; getTextureEXT, getGlyphBoundsEXT, getCroppingEXT, getKerningEXT.
  - charcs is a 16-bit char key: glyph keys are BMP only; invalid UTF-8 renders '?'. A missing glyph throws if there is no DefaultCharacter.
- Font sources:
  1. Content pipeline: .spritefont (XNA FontDescription XML) built with cna-content; it rasterizes a TrueType/OpenType font with FreeType at BUILD time, then ContentManager::Load<SpriteFont>("Fonts/ui").
     - Font file resolution is by filename match beside the .spritefont (reproducible default) or a configured font search dir (SetFontSearchDirectoriesEXT), optionally system fonts.
     - Needs CNA_ENABLE_FONT_PIPELINE (AUTO).
     - Default builds refuse missing glyphs unless --xna-compatible.
  2. .cnj SpriteFont JSON (bitmap atlas) via CnjImporter.
  3. XNB SpriteFont (XNA-built), runtime Load<SpriteFont>.
  4. A built-in 5x7 bitmap ASCII 32-126 font for quick demos: CNA::Internal::Graphics::makeSystemFont(GraphicsDevice&) returning unique_ptr<SpriteFont>. Header modules/graphics/include/CNA/Internal/Graphics/SystemFont.hpp (INTERNAL namespace, so unstable; wrapper in modules/graphics/examples/common/SimpleFontEXT.hpp: CNAExamplesEXT::MakeSimpleFontEXT(device)). No runtime TTF loading.
- No rich text, nine-slice, atlas, or text-effect classes exist in CNA. SpriteBatch + SpriteFont only.

======================================================================
4. MATH (math/include/Microsoft/Xna/Framework/*.hpp)  [XNA unless noted]
======================================================================
- Static constants are mixed: Vector2/3/4::Zero, One, UnitX/Y(/Z/W) (Vector3 also Up, Down, Right, Left, Forward, Backward); Quaternion::Identity; Point::getZeroProperty(); Rectangle::getEmptyProperty()/Empty; Matrix::getIdentityProperty() (static function, NOT a constant). Color named colours are static const Color members (141, e.g. Color::CornflowerBlue, Color::Transparent).
- Vector2.hpp / Vector3.hpp / Vector4.hpp: fields X, Y, (Z, W). Static functions: Add, Subtract, Multiply, Divide, Negate, Dot, Distance, DistanceSquared, Lerp, Clamp, Min, Max, SmoothStep, Hermite, CatmullRom, Barycentric, Normalize, Transform (by Matrix/Quaternion). Vector2/3: Reflect, TransformNormal. Vector3: Cross.
  - Instance: Length, LengthSquared, Normalize, Equals, GetHashCode, ToString. Operators ==, !=, +, -, *, /.
  - [CNAEXT]: compound operator+=, -=, *=, /= (not in XNA); getDebugDisplayStringProperty.
- Matrix.hpp: fields M11..M44. getIdentityProperty(), getForwardProperty/Backward/Up/Down/Left/Right/Translation (+ setters).
  - Create*: CreateBillboard, CreateConstrainedBillboard, CreateFromAxisAngle, CreateFromQuaternion, CreateFromYawPitchRoll, CreateLookAt, CreateOrthographic, CreateOrthographicOffCenter, CreatePerspective, CreatePerspectiveFieldOfView, CreatePerspectiveOffCenter, CreateReflection, CreateRotationX/Y/Z, CreateScale, CreateShadow, CreateTranslation, CreateWorld.
  - Others: Add, Subtract, Multiply, Divide, Negate, Invert, Transpose, Lerp, Transform(Matrix, Quaternion), Decompose, Determinant. Multiplication is row-vector XNA order.
  - [CNAEXT]: *=, ToColumnMajor(float[16]), CheckForNaNs.
- Quaternion.hpp: Add, Concatenate, Conjugate, CreateFromAxisAngle, CreateFromRotationMatrix, CreateFromYawPitchRoll, Divide, Dot, Inverse, Length, LengthSquared, Lerp, Slerp, Multiply, Negate, Normalize, Subtract.
- Plane.hpp: Dot, DotCoordinate, DotNormal, Normalize, Transform, Intersects (box/sphere/frustum/point: IntersectsPoint), fields Normal, D. Ray.hpp: Intersects (BoundingBox, BoundingSphere, Plane, BoundingFrustum), fields Position, Direction.
- BoundingBox.hpp: Contains, Intersects, CreateFromPoints, CreateFromSphere, CreateMerged, GetCorners; fields Min, Max. BoundingSphere.hpp: Contains, Intersects, CreateFromBoundingBox, CreateFromFrustum, CreateFromPoints, CreateMerged, Transform. BoundingFrustum.hpp: Contains, Intersects, GetCorners, getMatrixProperty/setMatrixProperty, getNear/Far/Left/Right/Top/BottomProperty, IntersectionPoint. ContainmentType {Disjoint, Contains, Intersects}; PlaneIntersectionType.
  - BoundingFrustum::Intersects(Ray) throws System::NotImplementedException for the uncomputed case (handoff 5.2). Avoid.
- Curve.hpp / CurveKey.hpp / CurveKeyCollection.hpp: Curve(), Evaluate(position), ComputeTangent(index, CurveTangent) / ComputeTangents(CurveTangent), getKeysProperty, getPreLoopProperty/getPostLoopProperty (CurveLoopType {Constant, Cycle, CycleOffset, Oscillate, Linear}), getIsConstantProperty, Clone. CurveKey(position, value[, tangentIn, tangentOut[, CurveContinuity {Smooth, Step}]]). CurveKeyCollection.Add/Remove/IndexOf/Clear; [CNAEXT] begin()/end() range-for.
- MathHelper.hpp: constants E, Log10E, Log2E, Pi, PiOver2, PiOver4, TwoPi (static constexpr float); functions Barycentric, CatmullRom, Clamp, Distance, Hermite, Lerp, Max, Min, SmoothStep, ToDegrees, ToRadians, WrapAngle; [CNAEXT-ish] ClosestMSAAPower, WithinEpsilon, GetMachineEpsilonFloat.
- Point.hpp {X, Y}, Rectangle.hpp: Contains, Intersects, Intersect, Union, Inflate, Offset, getLeft/Right/Top/Bottom/Center/Location/IsEmpty properties; fields X, Y, Width, Height.
- Color.hpp: ctors Color(), (Vector3), (Vector4), (float r,g,b[,a]), (intcs r,g,b[,a]), (UInt32 packed); properties getR/G/B/AProperty (0-255, + setters), getPackedValueProperty; static Lerp, Multiply; FromNonPremultiplied(Vector4) / (r,g,b,a); ToVector3/ToVector4; PackFromVector4. Operators: *, ==, !=. Colour is premultiplied-aware via FromNonPremultiplied (no Premultiply method).
- Half types: PackedVector/HalfSingle, HalfVector2, HalfVector4, HalfTypeHelper. Other packed: NormalizedByte2/4, NormalizedShort2/4, Short2/4, Byte4, Rg32, Rgba64, Rgba1010102, Bgr565, Bgra4444, Bgra5551, Alpha8.
- XmlSerializationEXT.hpp [CNAEXT]: XML serialization hooks for the math types. Design/ converters (design/include/Microsoft/Xna/Framework/Design/*.hpp) are type converters (BoundingBoxConverter, ColorConverter, MatrixConverter, Vector3Converter, etc.). Low demo value.

======================================================================
5. CONTENT
======================================================================
5.1 ContentManager (content/include/Microsoft/Xna/Framework/Content/ContentManager.hpp) [XNA]
- Game exposes game.getContentProperty() (a ContentManager&). Constructors: ContentManager(System::IServiceProvider*) / (provider, rootDirectory); [CNAEXT] default ctor. DEF_PROP RootDirectory (getRootDirectoryProperty/setRootDirectoryProperty). Resolved under the title directory (the exe dir) when present.
- T Load<T>(const std::string& assetName): cached per name; with no extension it tries each extension the reader declares (.cnb first via CNB, .xnb, .cnj, loose files). Unload(), Dispose(). Throws ContentLoadException (content/include/.../ContentLoadException.hpp; ctor (msg) / (msg, inner exception)).
- Return types by value: Load<Texture2D>, Load<SpriteFont>, Load<SoundEffect>, Load<Song>, Load<Video>, Load<Model>, Load<Curve>, Load<AnimationClipEXT> [CNAEXT]. Shared pointers: Load<std::shared_ptr<Effect>>, Load<std::shared_ptr<Texture3D>>, Load<std::shared_ptr<SkinnedModelEXT>>; TextureCube by value (cm.Load<TextureCube>("sky")).
- Built-in readers: Texture2D, Texture3D, TextureCube, SpriteFont, SoundEffect, Song, Video, Model, Effect (XNB compiled .fx only), Curve, AnimationClipEXT, plus generic reflective readers and std::map<string,any>/vector<string> (ReflectiveTypeReader).
- Loose files load without a pipeline: Texture2D from .png .jpg .jpeg .bmp .gif .tga .tif .tiff .qoi; SoundEffect .wav; Song .mp3 .ogg .wav .flac .opus .aac .wma; Video .mp4 .ogv .webm .mkv .avi .mov; Model .cnj, .gltf, .glb; SkinnedModelEXT .skinnedmodel.json; SpriteFont and Texture3D/Cube/Effect .cnj only.
- [CNAEXT]: RegisterCnbLoaderEXT<T>(assetTypeId, canonicalName, factory) (custom range ids via CnbAssetTypeIdFromName), RegisterCnjLoader, RefreshContentManifest, GetContentManifest, GetXnbReaderUsageSummary. ResourceContentManager.hpp, ContentTypeReader/ContentTypeReaderManager (custom XNB readers).
- Readers are linked lazily on first Load<T> of a type (link-size architecture).

5.2 CNB and the pipeline tool
- CNB = CNA's compiled content format (.cnb), container 1.0 frozen (docs/cnb-format.md). Output of the CNA Content Pipeline; XNB output also available (--format xnb, lzx compression --xnb-compress lzx).
- Tool: cna-content (source tools/content/content_main.cpp). cna-content build <SrcDir> -o <OutDir> [--format cnb|xnb] [--workers N] [--explain] [--xna-compatible]; cna-content clean <OutDir>. Config .cnj/.cna-content.json optional. Other tools in tools/: gltf_to_cnb, gltf_to_cnj, cnj_to_cnb, source_to_cnb, cnb_info, avatar_asset_pipeline.
- Importers (docs/content-pipeline.md, "Built-in routes"):
  - Images .png .jpg .jpeg .bmp .tga .gif .psd .hdr .pic .pnm to Texture2D.
  - Audio .wav to SoundEffect; Song .mp3 .ogg .oga .qoa .flac .opus .aac .wma; Video .mp4 .ogv .webm .mkv .avi .mov.
  - Models .gltf/.glb. FBX is NOT supported.
  - .cnj JSON (Texture2D, SoundEffect, Model, SpriteFont, Texture3D, TextureCube, Curve, AnimationClip).
  - .xnb (supported built-in roots re-encode to CNB).
  - .spritefont, .fxb, .fx (the last two XNB-only output).
- Song/Video media stays an external file referenced by the .cnb (XREF); deploy it beside the content.
- DDS is a contained TextureCube sidecar, not a direct route.

5.3 Models (graphics/include/.../Graphics/)
- Model.hpp [XNA]: getBonesProperty, getMeshesProperty, getRootProperty, getTagProperty/set, CopyAbsoluteBoneTransformsTo(vector<Matrix>&), CopyBoneTransformsFrom/To, Draw(world, view, projection) (uses each part's Effect).
  - [CNAEXT]: ctor Model(GraphicsDevice*, bones, meshes[, rootBoneIndex]); getCamerasEXTProperty (ModelCameraEXT), getSkinsEXTProperty (ModelSkinEXT {Name, SkinningData* Data, Meshes}), getGltfImportReportEXTProperty (GltfImportReportEXT.hpp; stable diagnostic Codes), getBoundingSphereEXTProperty, getMaterialVariantNamesEXTProperty, get/setMaterialVariantEXTProperty (KHR_materials_variants).
  - Model::Draw throws on SDL_RENDERER.
- ModelMesh.hpp: getNameProperty, getParentBoneProperty, getBoundingSphereProperty, getEffectsProperty, getMeshPartsProperty, Draw(), getTagProperty. ModelMeshPart.hpp: getVertexBufferProperty, getIndexBufferProperty, getEffectProperty/setEffectProperty, getNumVerticesProperty, getPrimitiveCountProperty, getStartIndexProperty, getVertexOffsetProperty, getTagProperty; [CNAEXT] getPrimitiveTypeEXTProperty, per-slot sampler states. ModelBone.hpp: getNameProperty, getIndexProperty, getTransformProperty/setTransformProperty, getParentProperty, getChildrenProperty. Collections: ModelBoneCollection, ModelMeshCollection, ModelMeshPartCollection, ModelEffectCollection.
- Animation [CNAEXT], AnimationPlayer.hpp: SkinningData {BoneCount, SkeletonHierarchy, BindPose, InverseBindPose, AnimationClips (unordered_map<string, AnimationClip=AnimationClipEXT>)}; AnimationPlayer(const SkinningData&): StartClip(clip), Update(System::TimeSpan, bool relativeToCurrentTime, bool loop=true), getCurrentPositionProperty, getCurrentClipProperty, GetBoneTransforms(), GetWorldTransforms(), GetSkinTransforms() (feed SkinnedEffect/SkinnedPbrEffect SetBoneTransforms, max 72 bones). Free functions ApplyClipToBonesEXT(Model&, clip, time), ApplyBindPoseBoneTransformsEXT. ModelAnimationsEXT {Clips} for rigid animation.
  - Clips bake channels onto union key times; CUBICSPLINE loses accuracy up to about 9.6 percent (documented). Morph targets apply on the CPU at import (MorphTargetEXT.hpp).
- SkinnedModelEXT.hpp [CNAEXT, standalone]: loaded via .skinnedmodel.json manifest (mesh + skeleton.bin + clip.bin); ComputeBoneTransformsEXT(clipName, position, loop, out), AddPartEXT/AttachPartEXT/RemovePartEXT.
- glTF import (via Load<Model> on .gltf/.glb or after cna-content): effect chosen per material model: metallic-roughness (default, incl. no material) -> PbrEffect (48 B stride) / SkinnedPbrEffect (68 B); KHR_materials_unlit -> BasicEffect (lighting off) / SkinnedEffect; colored vertices -> BasicEffect/SkinnedEffect; non-PBR with base+occlusion -> DualTextureEffect.
  - Supported extensions: texture_transform, mesh_quantization, emissive_strength, materials_variants, ior, draco (build-dependent), lights_punctual (approximated as 3 directional lights).
  - Parsed but ignored: clearcoat, sheen, volume. Unsupported: basisu, webp, meshopt, mesh_gpu_instancing. Transmission approximated.
  - See docs/gltf-limitations.md and docs/gltf-conventions.md.
  - Imported models render black until lights are enabled (see 1.3).
- Loading failures: ContentLoadException with asset context; malformed/unsupported content is explicit, never silent.

======================================================================
6. AUDIO (audio/include/Microsoft/Xna/Framework/Audio/ and media/include/.../Media/)
======================================================================
- SoundEffect.hpp [XNA]
  - Constructors: SoundEffect(vector<byte> buffer, int sampleRate, AudioChannels) and (buffer, offset, count, sampleRate, channels, loopStart, loopLength); [CNAEXT] SoundEffect(const std::string& assetName).
  - Static: FromStream(std::istream&) (WAV; caller owns the returned SoundEffect*), GetSampleDuration, GetSampleSizeInBytes.
  - Instance: getDurationProperty, getNameProperty/set, CreateInstance(), Play(), Play(volume, pitch, pan), Dispose.
  - Static properties: getMasterVolumeProperty/setMasterVolumeProperty, getDistanceScaleProperty/set, getDopplerScaleProperty/set, getSpeedOfSoundProperty/set.
- SoundEffectInstance.hpp: Play, Stop(), Stop(bool immediate), Pause, Resume, Dispose, Apply3D(listener, emitter) (+ listener array/vector overloads), getVolumeProperty/set, getPanProperty/set, getPitchProperty/set, getIsLoopedProperty/set (settable before first Play only), getStateProperty (SoundState {Playing, Paused, Stopped}). Instance limit throws InstancePlayLimitException.
- AudioEmitter.hpp / AudioListener.hpp: Position, Forward, Up, Velocity (+ DopplerScale on the emitter), all via get/set...Property.
- DynamicSoundEffectInstance.hpp: DynamicSoundEffectInstance(sampleRate, AudioChannels {Mono=1, Stereo=2}); SubmitBuffer(vector<byte> PCM16) (+ offset/count overload); event BufferNeeded (System::EventHandler<EventArgs>, register with += ); getPendingBufferCountProperty; Play/Stop/Pause/Resume; GetSampleDuration/GetSampleSizeInBytes. [CNAEXT] SubmitFloatBufferEXT(vector<float>).
  - A bug exists under parallel test load, not in normal use. Do not rely on BufferNeeded firing a fixed number of times.
- FrameworkDispatcher.hpp: FrameworkDispatcher::Update() is required in XNA to pump audio/media events; Game does this itself.
- Microphone.hpp [XNA]: getAllProperty, getDefaultProperty, getSampleRateProperty, getBufferDurationProperty/set, getStateProperty (MicrophoneState {Started, Stopped}), GetData(vector<byte>&), Start, Stop; NoMicrophoneConnectedException. [PLAT] needs a capture device.
- XACT: AudioEngine(settingsFile), WaveBank(engine, file[, streaming]), SoundBank(engine, file), Cue (getIsPlayingProperty etc.), AudioCategory, RendererDetail. Requires real XACT files (.xgs/.xwb/.xsb); engine.Update() every frame.
- Media/: MediaPlayer (all static: Play(Song*), Play(SongCollection[, index]), Pause, Resume, Stop, MoveNext, MovePrevious, getVolumeProperty/set, getIsMutedProperty/set, getIsRepeatingProperty/set, getIsShuffledProperty/set, getStateProperty (MediaState {Stopped, Playing, Paused}), getPlayPositionProperty, getQueueProperty, getGameHasControlProperty, visualization data), Song, SongCollection, MediaLibrary (+ Album/Artist/Genre/Playlist/Picture/...; MediaLibrary has a platform-skip on Windows), Video/Video.hpp, Video/VideoPlayer.hpp (VideoPlayer(); Play(Video*), Pause, Resume, Stop, GetTexture() (Texture2D* of the current frame; draw it with SpriteBatch), getStateProperty, getIsLoopedProperty/set, getVolumeProperty/set, getIsMutedProperty/set, getPlayPositionProperty; [CNAEXT] SetAudioTrackEXT/SetVideoTrackEXT). Video needs FFmpeg (CNA_ENABLE_VIDEO).
- Formats: WAV (PCM8/16, float32 and MS/IMA ADPCM when the SDL3 decoder is built) for SoundEffect; Song accepts mp3/ogg/oga/qoa/flac/opus/aac/wma/wav. XMA2 rejected. Audio backend is SDL3 (SDL_mixer) by default; ALSA/CNA mixer or NULL backend for SDL-free builds (NULL audio: Play returns false).
- CNAEXT audio: the reverb and low/high/band-pass filter hooks (INTERNAL_apply*) exist only for XACT track/RPC DSP, not a public effect API. Streaming exists only through DynamicSoundEffectInstance, Song and Video.

======================================================================
7. INPUT (input/include/Microsoft/Xna/Framework/Input/, CNA/Input/)
======================================================================
- Keyboard.hpp [XNA]: static GetState() / GetState(PlayerIndex). KeyboardState: IsKeyDown(Keys), IsKeyUp(Keys), GetPressedKeys(), getItem(Keys); [CNAEXT] default ctor, ToString. [CNAEXT, FNA-style EXT] Keyboard::GetModStateEXT() (CNA::Input::KeyModifiersEXT), GetKeyFromScancodeEXT, GetScancodeNameEXT, GetKeyNameEXT, GetKeyFromNameEXT, GetScancodeFromNameEXT. Keys.hpp, KeyState {Up, Down}.
- Mouse.hpp [XNA]: static GetState(), SetPosition(x, y), getWindowHandleProperty/set. MouseState: getXProperty, getYProperty, getLeftButtonProperty, getRightButtonProperty, getMiddleButtonProperty, getXButton1Property, getXButton2Property (ButtonState {Released, Pressed}), getScrollWheelValueProperty; [CNAEXT] getHorizontalScrollWheelValueEXTProperty.
  - [CNAEXT, FNA-style EXT]: Mouse::getIsRelativeMouseModeEXTProperty()/setIsRelativeMouseModeEXTProperty(bool) (relative-delta mode for look controls), SetCaptureEXT(bool), GetGlobalPositionEXT, WarpGlobalEXT, ClickedEXT event, SetCursor(MouseCursor&).
  - MouseCursor.hpp [CNAEXT]: system cursors getArrowProperty, getCrosshairProperty, getHandProperty, getIBeamProperty, getNoProperty, getSizeAllProperty, getSizeNESWProperty, getSizeNSProperty, getSizeNWSEProperty, getSizeWEProperty, getWaitProperty, getWaitArrowProperty; FromTexture2D(texture, originX, originY).
  - Game::setIsMouseVisibleProperty(bool).
- GamePad.hpp [XNA]: static GetCapabilities(PlayerIndex), GetState(PlayerIndex[, GamePadDeadZone]), SetVibration(player, left, right). GamePadState: getIsConnectedProperty, getPacketNumberProperty, getButtonsProperty, getDPadProperty, getThumbSticksProperty, getTriggersProperty, IsButtonDown/IsButtonUp(Buttons). GamePadCapabilities: Has*Property for each button/stick/trigger/motor, getGamePadTypeProperty, plus [CNAEXT] HasLightBar/HasTriggerVibrationMotors/HasTouchPad/HasGyro/HasAccelerometer/paddles. GamePadDeadZone {None, IndependentAxes, Circular}. PlayerIndex {One..Four} in core/include/Microsoft/Xna/Framework/PlayerIndex.hpp.
  - [CNAEXT, FNA-style EXT]: GetGUIDEXT, GetNameEXT, GetPathEXT, GetSerialEXT, GetConnectionStateEXT, GetPowerInfoEXT, GetButtonLabelEXT, SetLightBarEXT, SetTriggerVibrationEXT, GetGyroEXT, GetAccelerometerEXT, touchpad queries.
  - Opt-in: GamePad::setKeyboardEmulationEnabledEXT(bool) (keyboard-driven virtual pad), off by default. Constants LeftDeadZone, RightDeadZone, TriggerThreshold.
- Joystick (raw): CNA/Input/Joysticks.hpp [CNAEXT] CNA::Input::Joysticks::GetJoysticksEXT(), GetCapabilitiesEXT(id), GetStateEXT(id). Haptics.hpp (OpenEXT/OpenFromJoystickEXT/HapticDevice), InputDevices.hpp (GetMiceEXT/GetKeyboardsEXT/GetTouchDevicesEXT), Sensors.hpp (GetSensorsEXT, GetAccelerometerEXT, GetGyroscopeEXT), Power.hpp (GetInfoEXT), Clipboard.hpp (CNA::Input::Clipboard: GetTextEXT/SetTextEXT/HasTextEXT, primary selection, MIME data).
- Touch (Input/Touch/): TouchPanel static: GetState() (TouchCollection), ReadGesture() (GestureSample), getEnabledGesturesProperty/set (GestureType flags), getIsGestureAvailableProperty, getDisplayWidth/Height/OrientationProperty, [CNAEXT] getTouchDeviceExistsProperty/MAX_TOUCHES=8, GetCapabilities. [PLAT] needs a touch device or emulation; supported on mobile/touch desktops only.
- Text input events [CNAEXT, FNA-style]: TextInputEXT (static events TextInput (charcs), TextEditing, TextEditingCandidatesEXT; StartTextInput()/StopTextInput(), IsTextInputActive(), SetInputRectangle(Rectangle)). Window drag-and-drop: GameWindow::FileDropEXT/TextDropEXT events.
- Clipboard: CNA::Input::Clipboard (always compiled) and CNA::Devices::Clipboard (devices-ext, needs CNA_DEVICES).
- Devices module (modules/devices, Microsoft::Devices[::Sensors]): Accelerometer, Compass, Gyroscope, Motion (SensorBase<T>: Start/Stop/Dispose/CurrentValue, CurrentValueChanged events, getIsSupportedProperty()), VibrateController (Start(duration), Stop, [CNAEXT] intensity/StartLeftRight), Environment, DeviceType. Desktop: usually unsupported; Accelerometer::setKeyboardEmulationEnabledEXT(true) makes it supported on desktop with arrow keys (docs/keyboard-device-emulation.md). Also window orientation emulation (GameWindow::setKeyboardOrientationEmulationEnabledEXT).
- devices-ext (CNA_DEVICES=ON required; namespace CNA::Devices; each has getIsSupportedProperty()): PowerInfo, Locale, Clipboard, UrlLauncher::Open(url), SystemInfo, DisplayInfo (content scale, safe area), FileDialog (async), SystemTray, MessageBox, Camera (poll RGBA8 frames into a Texture2D with TryAcquireFrame; CameraState {NotSupported, Closed, Opening, Denied, Ready, Lost}). Stability: most are straightforward desktop-usable wrappers; Camera/SystemTray/FileDialog depend on platform support; headless/terminal report unsupported. Test coverage uses canned platforms only (no real dialogs/tray/camera in tests), so treat them as lightly hardware-validated.

======================================================================
8. FRAMEWORK / APP MODEL (runtime/include/Microsoft/Xna/Framework/)
======================================================================
- Game.hpp [XNA]: class Game; override Initialize(), LoadContent(), UnloadContent(), Update(GameTime&) (non-const ref in CNA), Draw(const GameTime&), BeginDraw/EndDraw, BeginRun/EndRun. Run(), Tick(), RunOneFrame(), Exit(), ResetElapsedTime(), SuppressDraw().
  - Properties: getComponentsProperty (GameComponentCollection&), getContentProperty, getGraphicsDeviceProperty, getServicesProperty (GameServiceContainer&), getWindowProperty (GameWindow&), getIsActiveProperty, getIsFixedTimeStepProperty/set, getTargetElapsedTimeProperty/set (System::TimeSpan), getInactiveSleepTimeProperty/set, getIsMouseVisibleProperty/set, getLaunchParametersProperty.
  - Events (System::EventHandler<EventArgs>, subscribe with +=): Activated, Deactivated, Exiting, Disposed. Virtuals OnExiting/OnActivated/OnDeactivated.
  - [CNAEXT]: Game(unique_ptr<IPlatform>), RunFrameEXT(), GetTypeName(), getTargetFPSProperty, getTargetMsFrameTimeProperty, fpsToMillisecondsPerFrame, GetPlatformEXT, GetPlatformCapabilitiesEXT.
  - Idiom (house3d_demo.cpp): create `std::make_unique<GraphicsDeviceManager>(this)` in the Game constructor (Game takes pointer), call setPreferredDepthStencilFormatProperty(DepthFormat::Depth24) BEFORE the device exists to get a depth buffer (without it, depth clears throw on most renderers). Create effects and buffers in Initialize()/LoadContent() after Game::Initialize(). Target FPS: setTargetElapsedTimeProperty(System::TimeSpan::FromTicks(...)). int main() constructs the Game and calls game.Run().
- GameTime.hpp: getTotalGameTimeProperty, getElapsedGameTimeProperty (System::TimeSpan), getIsRunningSlowlyProperty. Use TimeSpan accessors (check System/TimeSpan.hpp in sharp-runtime for TotalSeconds spelling).
- GameComponent.hpp: GameComponent(Game&); Initialize(), Update(GameTime&); getEnabledProperty, getUpdateOrderProperty, EnabledChanged/UpdateOrderChanged events. DrawableGameComponent.hpp: Draw(const GameTime&), LoadContent/UnloadContent, getVisibleProperty, getDrawOrderProperty, getGraphicsDeviceProperty. GameComponentCollection.hpp: Add(IGameComponent*), [CNAEXT] Add(shared_ptr) (owning), Remove, Contains, Clear, IndexOf; range-for.
- GameServiceContainer.hpp: template AddService<T>(T*), GetService<T>(), RemoveService<T>() (+ type_info overloads). Not owning.
- GameWindow.hpp: getClientBoundsProperty, getAllowUserResizingProperty/set, getTitleProperty/set, getCurrentOrientationProperty, getScreenDeviceNameProperty, getHandleProperty; events ClientSizeChanged, OrientationChanged, ScreenDeviceNameChanged; [CNAEXT] getIsBorderlessEXTProperty/set, MinimizeEXT, RestoreEXT, GetNativeWindowHandleEXT, FileDropEXT, TextDropEXT.
- GraphicsDeviceManager.hpp: GraphicsDeviceManager(Game*); getGraphicsProfileProperty/set, getIsFullScreenProperty/set, getPreferMultiSamplingProperty/set, getPreferredBackBufferFormat/Width/HeightProperty/set (default 800x480-ish: DefaultBackBufferWidth = 800), getPreferredDepthStencilFormatProperty/set, getSynchronizeWithVerticalRetraceProperty/set, getSupportedOrientationsProperty/set, ApplyChanges(), ToggleFullScreen(), CreateDevice(), events DeviceCreated/DeviceDisposing/DeviceReset/DeviceResetting (PreparingDeviceSettings via virtual OnPreparingDeviceSettings). [CNAEXT] getPreferredPresentationModeProperty/set with PresentationMode {Letterbox=0, Overscan=1, Stretch=2, NativeBackBuffer=3, FixedHeightDynamicWidth=4}.
  - MSAA gotchas: MultiSampleCount applies at renderer construction; changing PreferMultiSampling later does not change an already-created renderer on EasyGL. Set it before the device is created; ApplyChanges() recreates the renderer for MSAA changes via RecreateRendererForMultiSampleCount (device [CNAEXT]).
- Storage (storage/include/.../Storage/): StorageDevice (static BeginShowSelector/EndShowSelector, BeginOpenContainer/EndOpenContainer, DeleteContainer, getIsConnectedProperty, getFreeSpaceProperty, getTotalSpaceProperty, DeviceChanged; [CNAEXT] SetAppNameEXT(string), GetStorageRootEXT()), StorageContainer (CreateFile, OpenFile, FileExists, DeleteFile, GetFileNames, CreateDirectory, DirectoryExists, DeleteDirectory, GetDirectoryNames, Dispose, getDisplayNameProperty). TitleContainer::OpenStream(name) reads from the title (install) dir. [CNAEXT] ReadToPointer/FreePointer.
- GamerServices (gamer-services/include/.../GamerServices/: Guide, SignedInGamer, Gamer, Achievement, Leaderboard*, AvatarRenderer/AvatarDescription/AvatarAnimation, GamerServicesComponent/Dispatcher, ...) and Net (net/include/.../Net/: NetworkSession, NetworkGamer, LocalNetworkGamer, PacketReader/Writer, SendDataOptions, ...) are enabled by default (CNA_ENABLE_NET=ON). Online sessions are native-only (browser builds refuse). SendDataOptions Chat cannot combine with Reliable/InOrder (known bug). Not covered further.
- Video playback: VideoPlayer (see section 6) requires CNA_ENABLE_VIDEO (FFmpeg).
- Diagnostics (diagnostics/include/CNA/Diagnostics/Diagnostics.hpp, Instrumentation.hpp; namespace CNA::Diagnostics) [CNAEXT, off by default]: build option CNA_DIAGNOSTICS=OFF|STATS|FULL, compile-time level CNA_DIAGNOSTICS_LEVEL; SetRuntimeMode / GetRuntimeMode / GetBuildMode (can only lower from build mode). Macros: CNA_PROFILE_SCOPE(name), CNA_PROFILE_SCOPE_CATEGORY, CNA_DIAGNOSTICS_COUNTER_ADD / GAUGE_SET / GAUGE_ADD / FRAME_COUNTER_ADD, CNA_DIAGNOSTICS_EVENT, CNA_DIAGNOSTICS_FRAME_SCOPE. Handles: NameHandle, CounterHandle, GaugeHandle, FrameCounterHandle, ZoneScope, FrameScope; IDiagnosticsProvider via GetProvider(): CaptureSnapshot() (Snapshot {metrics, recentFrames (240 frames), resources, drop counters}), ReadEvents(afterSequence,...), ResolveName. Recording: StartRecording(maxEvents) / StopRecording() returns Trace with WriteChromeTrace(ostream). Built-in engine metrics: Graphics/DrawCalls, IndexedDrawCalls, NonIndexedDrawCalls, SubmittedPrimitives, RenderTargetChanges, EffectChanges, TextureBindingChanges, SpriteSubmissions (STATS/FULL), plus Game/Tick, Game/Update, Game/Draw zones (FULL). Without CNA_DIAGNOSTICS the macros compile to nothing, so demo code must tolerate empty snapshots.
- Inspector (inspector/include/CNA/Inspector/Agent.hpp, Client.hpp, Protocol.hpp): CNA_BUILD_INSPECTOR=ON plus diagnostics STATS/FULL. Agent::Start(...) returns unique_ptr<Agent> (localhost TCP, auth token via GetAuthenticationToken(), GetPort(), IsRunning(), Stop()); a separate cna-inspector process serves a local browser UI. Development tool, not a UI to embed.

======================================================================
9. CNAEXT "ENGINE LAYER" (docs/cnaext-engine-layer.md, modules/graphics-ext, plus always-compiled EXT features)
======================================================================
- Doc states graphics-ext (CNA_CNAEXT=ON) contains EXACTLY: AsciiPostProcessEffect (+AsciiQuantizeMode), CRTEffect (+CRTMaskType), DepthEffect (+DepthEffectMode, DitherMode), DebugDraw, ShaderCodeEXT / ShaderPackageEXT / ShaderPackageSelectionEXT (+ShaderBindingTypeEXT, ShaderBindingRequirementEXT). The post-process chain, HDR pipeline, shadow engine, clustered rendering, probes, compute helpers and PBR material abstraction were REMOVED in the alpha scope reduction (docs/graphics-ext-scope-reduction.md). PbrEffect, SkinnedPbrEffect, glTF and CNB/CNJ stay in core. A planned "DebugGizmos" is only design intent. Use "render target + SpriteBatch effect" for post-processing.
- DebugDraw (graphics-ext/include/CNA/Graphics/DebugDraw.hpp, namespace CNA::Graphics, final, non-copyable, uses core BasicEffect): DebugDraw(GraphicsDevice&); begin(view, projection); addLine(from, to, Color); addBox(BoundingBox, Color); addSphere(centre, radius, Color, segments=24) / addSphere(BoundingSphere, Color, segments=24); addFrustum(BoundingFrustum, Color); addCross(position, size, Color); clear(); end() (flushes the draw); isDepthTested()/setDepthTested(bool); getLineCount(). Lower-case method names (not Property style). Stable, simple, desktop-usable.
- Always-compiled [CNAEXT] facilities (no CNA_CNAEXT needed): renderer/capability queries (section 11); glTF Model import and report; PbrEffect/SkinnedPbrEffect; SkinnedModelEXT/AnimationPlayer; ShaderEffect (string form); punctual light / shadow receiver state / ImageBasedLightEXT on lit effects; PrimitiveType::PointListEXT; DrawInstancedPrimitivesBaseInstanceEXT; DEBUG helpers GraphicsDevice::SetStringMarkerEXT; diagnostics counters; ContentManager CNB/CNJ extension hooks.
- No camera helpers, scene graph, lighting manager, or shadow generator exist in public headers.
- Avatars: the XNA avatar system is a CNA-provided feature in GamerServices (AvatarRenderer, AvatarDescription, AvatarAnimation), separate from CNAEXT; see docs/avatars.md, docs/avatar-demos.md. Needs CNA_ENABLE_GAMER_SERVICES.

======================================================================
10. DO-NOT-DEMO / RETIRED / UNSUPPORTED
======================================================================
- Retired renderer identities (refused at configure time and at runtime; docs/removed-renderers.md; C ABI values permanently reserved): OPENGLES2, WEBGL1, BGFX, MAGNUM, DIRECTX12, DIRECT2D, CANVAS, HTML_DOM, SKIA, BLEND2D, FREEDIRECT, DIRECTX1/2/3/5/6/7/8/10, OPENGLES1, OPENGL4, OPENGL1, OPENGL2, WICKED, SOKOL, DILIGENT, GLIDE, GDI, LLGL, SVG_DOM, OPENVG, PORTABLEGL, TINYGL, IGL, PIXIJS, NANOVG, RLGL. Also retired: CNA's direct WIN32/X11/WAYLAND platforms (SDL3 is the platform; HEADLESS/Terminal platforms exist).
- Removed APIs: SpriteBatch::DrawMeshEXT (and its C route), the HDR render pipeline, pass framework, shadow engine, clustered rendering, probes, compute-driven engine helpers, PBR material abstraction, compute shaders / indirect drawing / GPU timers feature ordinals (18, 19, 20, 23 reserved).
- Known defects / limitations from handoff.md section 5 (affects what to demo):
  1. VULKAN: RasterizerState.DepthBias is passed unscaled; realistic values (about -1e-4) have no visible effect. Do not demo depth bias on Vulkan.
  2. SDL_RENDERER: ignores TextureAddressMode Wrap/Mirror; 2D only.
  3. The museum's `cx-shadow` caster is portable: it projects orthographic depth on the CPU and writes it through `VertexPositionColor` plus a stock `BasicEffect`. DIRECTX9 and SOFTWARE therefore exercise their real stock-effect shadow receivers without needing a source-shader dialect.
  4. FNA3D: stock effects do not saturate COLOR0 like D3D9 (minor pixel differences).
  5. VULKAN: multi-stream instancing with duplicate usages and OcclusionQuery::IsComplete (no flush) have known bugs; WEBGPU counts only post-Clear draws in an occlusion run. SDL_GPU cannot construct OcclusionQuery.
  6. BoundingFrustum::Intersects(Ray) throws NotImplementedException.
  7. Native Alpha8 RenderTarget storage unavailable (falls back to Color).
  8. SendDataOptions::Chat cannot be combined (Net).
  9. EasyGL: MSAA fixed at renderer construction; RenderTargetUsage always Discard; wireframe is emulated; ES 3.0 occlusion is boolean-only (OPENGLES3/WEBGL2); Texture2D mip generation on SetData is not automatic.
  10. EasyGL doc easygl_bugs.md is mostly dated 2026-06-27; many rows are stale (anisotropic fixed, ColorWriteChannels and cube-map depth since changed). Prefer handoff.md section 5.
- Experimental: WEBGPU, SOFTWARE (called "first-class" in docs but flagged Experimental in code), FNA3D; the C ABI; custom content-pipeline components API.
- Do not demo (unsupported / platform-limited): ColorMatrixEffect on non-SOFTWARE renderers; compiled .fx effects unless built with the renderer's COMPILED_EFFECTS option; GPS/Location (no implementation; considered and rejected), Bluetooth/NFC; Texture3D sampling in stock effects (no stock effect uses it); KHR_materials_clearcoat/sheen/volume (parsed but ignored); KTX2/WebP/meshopt textures; FBX; HLSL source compiled at runtime; XMA2 audio; Touch on desktop without a touch device; Compass/Motion on desktop (unsupported unless emulated).

======================================================================
11. RUNTIME RENDERER QUERIES AND PER-RENDERER BEHAVIOUR
======================================================================
- Which renderer: device.GetGraphicsRendererName() (std::string_view, e.g. "OPENGLES3", matches the CNA_GRAPHICS_RENDERER spelling) and device.GetGraphicsRendererType() (CNA::GraphicsRendererType enum: SdlRenderer, OpenGLES3, OpenGL33, WebGL2, Vulkan, WebGPU, Headless, Software, Stub, DirectX11, DirectX9, SdlGpu, Metal, Fna3d; core/include/CNA/GraphicsRendererType.hpp). Free functions in core: CNA::getGraphicsRendererName(type), CNA::getGraphicsBackendMaturity(type).
- Before a device exists: CNA::GraphicsRendererSelection (core/include/CNA/GraphicsRendererSelection.hpp, class CNAEXT): static SetPreferred(type or "NAME"), GetSelected(), GetActive(), GetAvailable() (span of compiled-in renderers), IsAvailable, IsLatched, SetFallbackChain(span), EnableAutomaticFallback(bool), IsFallbackEnabled, GetFallbackHistory(). Precedence: SetPreferred, then env CNA_GRAPHICS_RENDERER, then the compile-time default. Naming a renderer that is not compiled in THROWS. Default is hard failure; fallback is opt-in. Needs a multi-renderer build (CNA_GRAPHICS_RENDERERS). Demo: modules/graphics/examples/renderer_selection/renderer_selection_demo.cpp. The renderer can only be switched before the window exists, i.e. before constructing the GraphicsDevice/Game.
- Legacy capability query: device.SupportsCapability(CNA::GraphicsCapability::X) (graphics/include/CNA/GraphicsCapability.hpp). Values: ThreeD, DepthStencilBuffer, MultiSampleAntiAliasing, MultipleRenderTargets, AnisotropicFiltering, WireFrame, OcclusionQuery, CustomEffects, Texture3D, MultiStreamVertexInput, Instancing, StencilBuffer, AdditiveBlending, CompiledEffects, FloatRenderTargets, HalfFloatRenderTargets, HalfFloatTextureLinearFiltering (17).
- Detailed query (graphics/include/CNA/RendererCapabilityProfile.hpp): device.GetRendererCapabilityProfileEXT() returns a cached CNA::RendererCapabilityProfile with GetRendererName(), GetFeature(RendererFeature) -> RendererFeatureInfo {support, note}, Supports(RendererFeature), GetLimit(RendererLimit) -> RendererLimitValue {known, value}, GetSurfaceFormatSupport(ordinal) -> RendererFormatSupport {IsKnown/Supports(RendererFormatUsage)}, GetAdditionalLimitationsText(), GetEnglishReport(). Convenience on the device: GetRendererFeatureSupportEXT(feature), SupportsRendererFeatureEXT(feature), GetRendererLimitEXT(limit), GetRendererSurfaceFormatSupportEXT(SurfaceFormat), GetRendererCapabilityReportEXT() (long English text, diagnostic only; do not parse), SupportsSurfaceFormatAsRenderTargetEXT, ExecutesShaderEffectSourceEXT, SupportsShadowSamplingEXT, SupportsImageBasedLightingEXT, GetShaderDialectEXT, SupportsShaderLanguageEXT(lang, stage).
  - RendererFeature ordinals: ThreeDimensionalPipeline 0, DepthStencilBuffer 1, MultiSampleAntiAliasing 2, MultipleRenderTargets 3, AnisotropicFiltering 4, WireFrameRasterization 5, OcclusionQueries 6, ShaderEffects 7, ShaderEffectSourceExecution 8, Texture3DStorage 9, MultiStreamVertexInput 10, InstancedDrawing 11, StencilBuffer 12, AdditiveBlending 13, CompiledXnaEffects 14, Float32RenderTargets 15, Float16RenderTargets 16, Float16TextureLinearFiltering 17, ShadowSampling 21, ImageBasedLighting 22, ShaderDialectGlslDesktop 24, ShaderDialectGlslEs 25, ShaderDialectGlslVulkan 26, ShaderDialectHlsl 27, ShaderDialectMsl 28, ShaderDialectWgsl 29, Texture3DSampling 30, BaseInstanceDrawing 31.
  - RendererFeatureSupport {Unknown, Unsupported, Supported, Restricted}. RendererLimit {MaxTextureDimension 0, MaxVertexStreams 1, MaxUniformBufferBytes 11, MaxSampledTexturesPerShaderStage 14, MaxVertexInputBindings 16, MaxVertexInputAttributes 17, MaxColorAttachments 18, MinUniformBufferOffsetAlignment 20}. RendererFormatUsage bitmask {TextureStorage, Sampled, Filterable, RenderTarget, Blendable, TransferSource, TransferDestination, Mipmapped, Multisample, ColorTransfer}.
  - Example probe: modules/graphics/examples/renderer_capability_probe.cpp.
  - Unsupported 3D calls behaviour: SetUnsupported3DGraphicsCallBehavior(Throw|WarnAndStub).
- Per-renderer behaviour to know (desktop OPENGLES3 / OPENGL33 / VULKAN / SOFTWARE):
  - OPENGLES3 and OPENGL33 are the same EasyGL implementation; OPENGL33 is a GL 3.3 core context with GLSL ES 3.00 shaders auto-adapted to #version 330 core. OPENGLES3 is the project's most mature 3D path (docs recommend it for 3D demos). Linux default for non-Emscripten.
    - OPENGLES3 specifics: boolean-only occlusion counts; wireframe is emulated lines (works); anisotropic filtering works when GL_EXT_texture_filter_anisotropic exists; non-Color SurfaceFormat for real GPU sampling is limited (Task 732 "BLOCKED" in older docs; test via GetRendererSurfaceFormatSupportEXT).
    - Compiled effects only with CNA_EASYGL_COMPILED_EFFECTS.
    - Fog is computed on clip-space z in some shader paths (older doc; may diverge at oblique angles).
    - Unlit stock effects apply DiffuseColor above 1 unclamped per-fragment (small difference from XNA/Vulkan).
    - Render-target textures are stored upside-down relative to uploaded textures for custom shaders (compiled-effect doc 10.2).
  - VULKAN: ShaderEffect needs SPIR-V; has DepthBias bug; precise occlusion counts; supports Texture3D sampling, base-instance, wireframe; DepthBias aside, stock effects match XNA saturation.
  - SOFTWARE (CPU rasteriser): complete classic XNA contract per its companion matrix (all six stock effects, SpriteBatch/Font, models, MRT, MSAA 4x, occlusion counts, Texture3D/Cube storage); no ShaderEffect source execution, PBR, or ColorMatrix is the exception (ColorMatrixEffect works). Slow; no native window (present is a CPU buffer, windowless). Experimental maturity.
  - HEADLESS and STUB make no pixels (no readback); SDL_RENDERER is 2D-only (3D throws; MRT>1 throws).
  - All renderers share the XNA state-object semantics; DEPTH24 back buffer must be requested for depth clears.
- Docs that describe renderer differences: docs/graphics-renderer-feature-matrix.md (partially stale; use handoff.md section 5), docs/renderer-capability-profiles.md, docs/renderer-registry.md, docs/runtime-renderer-selection.md, docs/opengl33-renderer.md, docs/vulkan-renderer.md, docs/software-renderer.md, docs/occlusionquery-support.md, docs/fx-compiled-effects.md, docs/texture3d-texturecube-support.md (stale on sampling).

======================================================================
12. GOTCHAS (docs and headers)
======================================================================
1. BasicEffect.VertexColorEnabled defaults false (as does TextureEnabled, LightingEnabled); stock lit effects render black with no lights enabled; call EnableDefaultLighting(). PbrEffect/SkinnedPbrEffect have zero ambient and no lights by default; glTF models appear black until lit. KHR_lights_punctual import yields at most 3 directional lights.
2. Depth buffer is NOT created by default. Call graphicsDeviceManager.setPreferredDepthStencilFormatProperty(DepthFormat::Depth24) (or Depth24Stencil8 for stencil) before the device is created, and device.SetDepthTestEnabled(true) or set DepthStencilState::Default. Clearing depth without one throws.
3. World/View/Projection on stock effects are public fields on BasicEffect (and property accessors on all). Matrices are XNA row-vector convention; ShaderEffect::SetUniformMat4 wants a column-major float[16] (use Matrix::ToColumnMajor).
4. Default RasterizerState culls counter-clockwise faces (XNA); use RasterizerState::CullNone or flip winding when geometry vanishes.
5. SpriteBatch::End leaves its blend/depth/rasterizer/sampler state on the device; reset states for 3D. SpriteBatch::Begin defaults to AlphaBlend (premultiplied alpha; use BlendState::NonPremultiplied for straight alpha). SpriteBatch Draw takes const Texture2D& (a reference), and Texture2D::FromStream returns by value.
6. SpriteEffects has no bitwise ops (cast 3 for both flips). SpriteFont keys are BMP 16-bit chars; missing glyphs throw unless DefaultCharacter is set.
7. Static state presets (BlendState::Additive, SamplerState::LinearClamp, ...) are shared constants; copy-construct to customise.
8. Textures need explicit texture state: set effect.setTextureProperty(&tex) and setTextureEnabledProperty(true) for BasicEffect; sampler state is per slot (device.getSamplerStatesProperty()[0]); Texture2D mip levels must be uploaded via SetData(level, rect, ...) on EasyGL.
9. Vertex buffer binding for custom vertices needs a VertexDeclaration (IVertexType::getVertexDeclarationProperty()); unknown strides without a declaration are refused; PointSize usage unsupported; only stock layouts work on stock effects (Position, Normal, Color, TextureCoordinate, plus Tangent for PBR, BlendIndices/BlendWeight for skinned).
10. DrawIndexedPrimitives needs both a vertex buffer (SetVertexBuffer) and an index buffer (device.Indices(...) / SetIndexBuffer / setIndicesProperty) set; 32-bit indices via IndexElementSize::ThirtyTwoBits.
11. GetBackBufferData / Texture2D::GetData / SaveAsPng readbacks stall the GPU; HEADLESS/STUB have no readback.
12. Compiled .fx effects need a build option and offline-compiled FX bytes (no runtime HLSL). ShaderEffect(source) is renderer-specific (GLSL ES 3.00 on OPENGLES3/OPENGL33/WEBGL2, SPIR-V on VULKAN); query SupportsShaderLanguageEXT first.
13. Properties use getXProperty()/setXProperty() except a few CNAEXT classes that use plain get/set (graphics-ext effects, DebugDraw) and CNAEXT EXT-suffixed ones (setShadowMapEXT etc.). Events are System::EventHandler<T> members: use `+=` with a lambda `(System::Object* sender, const EventArgs&)`. Static class members (Keyboard, Mouse, GamePad, MediaPlayer, SoundEffect master volume) are static methods.
14. Game::Update takes GameTime& (non-const); Draw takes const GameTime&. GraphicsDeviceManager must be created in the Game constructor.
15. Audio: SoundEffectInstance::IsLooped can only be set before the first Play; Song/Video media files must be deployed beside the .cnb; DynamicSoundEffectInstance expects PCM16.
16. Content: Load<T> caches by name; RootDirectory is relative to the title directory; content loading failure is ContentLoadException (catch it to show a friendly "missing asset" exhibit). Built-in readers install lazily on first Load<T>.
17. Mouse relative mode (CNAEXT): enable with Mouse::setIsRelativeMouseModeEXTProperty(true) for FPS-style look; Game::setIsMouseVisibleProperty(false) to hide the cursor.
18. Window/back buffer: default back buffer 800 wide; fullscreen is exclusive (switches monitor mode) via setIsFullScreenProperty + ApplyChanges(). PresentationMode selects letterbox/stretch behaviour.
19. Docs index docs/README.md marks which docs were verified; most per-feature docs are dated and unevenly maintained. Rely on headers and handoff.md section 5 for truth.

======================================================================
13. USEFUL EXAMPLES TO COPY FROM
======================================================================
- modules/graphics/examples/house3d_demo.cpp: complete Game with BasicEffect + vertex colours, VertexBuffer/IndexBuffer, CullNone, depth buffer, first-person camera, SpriteBatch overlay, fullscreen option.
- modules/graphics/examples/demo_2d/: SpriteBatch demo with Content.
- modules/graphics/examples/renderer_selection/renderer_selection_demo.cpp and renderer_capability_probe.cpp: renderer selection and capability queries.
- modules/graphics/examples/*effect*_test.cpp, rendertarget*_test.cpp, occlusion_query_test.cpp, instanced_textured_draw_test.cpp, texture2d_getdata_contract_test.cpp, spritebatch_*_test.cpp, stencil_matrix_contract_test.cpp, mrt_stock_effect_contract_test.cpp (small API-usage samples, mostly tests).
- modules/graphics/examples/parity/*.cpp: effect parity fixtures.
- modules/renderers/easygl/examples/easygl_*_shader_test.cpp: ShaderEffect GLSL ES 3.00 examples.
- modules/graphics-ext/examples/: ascii_posteffect_pixel_test.cpp, crt_effect_demo_test.cpp, depth_effect_demo_test.cpp.
- modules/audio/examples, modules/input/examples, modules/content/examples, modules/devices/examples (not individually read).
- examples/xvfb_screenshot_demo.cpp (SpriteBatch rotation + screenshot).
- Not verified in this survey: Keys/Buttons enum members, GamePadButtons/DPad/ThumbSticks/Triggers accessors, exact ContentManager RootDirectory accessor spelling (DEF_PROP => getRootDirectoryProperty/setRootDirectoryProperty by the project's convention), System::TimeSpan member spelling, Game-loop delivery of FrameworkDispatcher::Update (assumed automatic), exact IVertexType usage-index conventions for the Skinned vertex field names.

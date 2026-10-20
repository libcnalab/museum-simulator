#!/usr/bin/env python3
"""Writes the small, legal source files the Pipeline Workshop rooms read (Content/Workshop/).

    tools/gen_workshop.py            # glTF files always; media files need ffmpeg (wmav2, wmv2, libmp3lame)

glTF (room wk-gltf): every file is self-contained JSON with its buffers and images as base64 data URIs, and every
number the room checks is written here, so the room's expectations are the values in this script:

    uvsets.gltf     a quad with TEXCOORD_0 and TEXCOORD_1, a PBR material whose base-colour and normal maps carry
                    KHR_texture_transform (the normal map's transform also overrides its texCoord to 1), an occlusion
                    map on texCoord 1, KHR_materials_ior and KHR_materials_specular
    sparse.gltf     a quad whose POSITION accessor has sparse overrides on top of a bufferView, and whose TEXCOORD_0
                    accessor is sparse with no bufferView at all (zeros plus overrides)
    morph.gltf      a quad with two morph targets (one dense, one sparse without a bufferView) and default weights
    required.gltf   a triangle that lists an extension nobody implements in extensionsRequired
    used.gltf       the same triangle listing that extension only in extensionsUsed
    instancing.gltf a triangle on a node that uses EXT_mesh_gpu_instancing (three TRANSLATION instances), optional

Processor and material sources (rooms wk-processor, wk-material):

    plaque.png      4x4 RGBA: magenta (the colour key), red at alpha 128, green, blue, then twelve (64, 128, 192, 255)
    plaque.label    one line of text a game's own importer reads ("Pipeline Workshop")
    caption.label   a second label, built as a nested asset ("caption text")
    plaque.notes    a sidecar the processor records as a dependency

Build-route, incremental-build and task sources (rooms wk-routes, wk-incremental, wk-tasks):

    curve.cnj       a CNA Curve document: a Step key 2.5 at position 0, 7.5 at position 2, constant loops
    texture.cnj     a CNA Texture2D document naming plaque.png as its sidecar
    unknown.cnj     a .cnj of type "Effect", which CNA's compiler refuses by name
    tone8.wav       80 frames of 8-bit unsigned mono PCM at 8000 Hz, sample k = 128 + 10 * (k % 10), no loop
    loop16.wav      40 frames of 16-bit stereo PCM at 8000 Hz (left 100 k, right -100 k) with a smpl loop over frames 10..29
    deep24.wav      10 frames of 24-bit mono PCM, a width XNA's WavImporter refuses
    vector.xml      XNA intermediate XML of a Vector3 (1.5, -2, 3.25)
    workshop.xap    an XACT project header for BuildXact's validation (never compiled here)
    workshop.contentproj  an XNA Game Studio 4.0 content project (room wk-contentproject): plaque.png (TextureImporter,
                    TextureProcessor), tone8.wav (WavImporter, SoundEffectProcessor) and vector.xml (XmlImporter,
                    PassThroughProcessor) as Compile items, plaque.notes as a copied None item, Windows / HiDef

Animation (room wk-animation) and a reflectively written .xnb (room wk-typewriter):

    arm.x           DirectX text: frames Root > Arm > Hand, AnimTicksPerSecond 10, an AnimationSet "Wave" with position keys
                    on Arm (0, 1, 2 along +X at 0, 0.5, 1 s) and matrix keys on Hand (a 0.25 lift along +Y at 1 s)
    reflective.xnb  a game type without a ContentTypeWriter, as XNA 4.0's ReflectiveWriter writes it (see reflective_xnb)

These live in Content/Workshop rather than Content/Pipeline/Workshop: the museum's build runs cna-content over everything under
Content/Pipeline, and that build would (rightly) refuse required.gltf and the tone.mp3/tone.wma pair sharing one asset name.

Media (room wk-importers), made by ffmpeg from its own sine and test-pattern sources:

    tone.mp3        0.5 s of 440 Hz, mono, 22050 Hz, MPEG-2 layer III 32 kbit/s
    tone.wma        the same tone as Windows Media Audio 2, 32 kbit/s
    clip.wmv        1 s of a 32x24 test pattern at 10 frames per second, Windows Media Video 8, no audio
    clip_audio.wmv  the same with the tone (WMA 2) as its sound track
"""
import base64
import json
import pathlib
import shutil
import struct
import subprocess
import sys
import zlib

HERE = pathlib.Path(__file__).resolve().parent.parent
OUT = HERE / "Content" / "Workshop"


def png(rgba_rows):
    """A tiny RGBA PNG from rows of (r, g, b, a) tuples."""
    height, width = len(rgba_rows), len(rgba_rows[0])
    raw = b"".join(b"\x00" + b"".join(bytes(px) for px in row) for row in rgba_rows)

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")


def data_uri(mime, payload):
    return "data:%s;base64,%s" % (mime, base64.b64encode(payload).decode("ascii"))


class Buffer:
    """Accumulates one binary buffer and the bufferViews/accessors that point into it."""

    def __init__(self):
        self.data = bytearray()
        self.views = []
        self.accessors = []

    def view(self, payload, target=None):
        while len(self.data) % 4:
            self.data.append(0)
        view = {"buffer": 0, "byteOffset": len(self.data), "byteLength": len(payload)}
        if target is not None:
            view["target"] = target
        self.data += payload
        self.views.append(view)
        return len(self.views) - 1

    def floats(self, rows, kind, with_bounds=False, target=34962):
        width = len(rows[0])
        index = self.view(struct.pack("<%df" % (len(rows) * width), *[v for row in rows for v in row]), target)
        accessor = {"bufferView": index, "componentType": 5126, "count": len(rows), "type": kind}
        if with_bounds:
            accessor["min"] = [min(r[i] for r in rows) for i in range(width)]
            accessor["max"] = [max(r[i] for r in rows) for i in range(width)]
        self.accessors.append(accessor)
        return len(self.accessors) - 1

    def indices(self, values):
        index = self.view(struct.pack("<%dH" % len(values), *values), 34963)
        self.accessors.append({"bufferView": index, "componentType": 5123, "count": len(values), "type": "SCALAR"})
        return len(self.accessors) - 1

    def sparse(self, count, kind, overrides, base_rows=None, bounds_rows=None):
        """An accessor of `count` elements: base_rows from a bufferView (or zeros when None) with `overrides`
        ({index: row}) applied through glTF's sparse storage (UNSIGNED_BYTE indices, tightly packed values)."""
        width = {"SCALAR": 1, "VEC2": 2, "VEC3": 3}[kind]
        accessor = {"componentType": 5126, "count": count, "type": kind}
        if base_rows is not None:
            accessor["bufferView"] = self.view(struct.pack("<%df" % (count * width), *[v for row in base_rows for v in row]))
        keys = sorted(overrides)
        index_view = self.view(bytes(keys))
        value_view = self.view(struct.pack("<%df" % (len(keys) * width), *[v for k in keys for v in overrides[k]]))
        accessor["sparse"] = {"count": len(keys), "indices": {"bufferView": index_view, "componentType": 5121}, "values": {"bufferView": value_view}}
        if bounds_rows is not None:
            accessor["min"] = [min(r[i] for r in bounds_rows) for i in range(width)]
            accessor["max"] = [max(r[i] for r in bounds_rows) for i in range(width)]
        self.accessors.append(accessor)
        return len(self.accessors) - 1

    def document(self, extra):
        doc = {"asset": {"version": "2.0", "generator": "museum tools/gen_workshop.py"}, "scene": 0}
        doc.update(extra)
        doc["buffers"] = [{"byteLength": len(self.data), "uri": data_uri("application/octet-stream", bytes(self.data))}]
        doc["bufferViews"] = self.views
        doc["accessors"] = self.accessors
        return doc


# A quad in the XY plane facing +Z; every corner has its own position so the room can match vertices by position.
QUAD_POSITIONS = [(-1.0, -1.0, 0.0), (1.0, -1.0, 0.0), (1.0, 1.0, 0.0), (-1.0, 1.0, 0.0)]
QUAD_NORMALS = [(0.0, 0.0, 1.0)] * 4
QUAD_INDICES = [0, 1, 2, 0, 2, 3]
QUAD_UV0 = [(0.0, 1.0), (1.0, 1.0), (1.0, 0.0), (0.0, 0.0)]
QUAD_UV1 = [(0.125, 0.875), (0.625, 0.875), (0.625, 0.375), (0.125, 0.375)]


def write(name, doc):
    (OUT / name).write_text(json.dumps(doc, indent=1) + "\n")


def scene_with(mesh_node):
    return {"scenes": [{"nodes": [0]}], "nodes": [mesh_node]}


def uvsets():
    b = Buffer()
    pos = b.floats(QUAD_POSITIONS, "VEC3", True)
    nrm = b.floats(QUAD_NORMALS, "VEC3")
    uv0 = b.floats(QUAD_UV0, "VEC2")
    uv1 = b.floats(QUAD_UV1, "VEC2")
    idx = b.indices(QUAD_INDICES)
    white = png([[(255, 255, 255, 255), (200, 200, 200, 255)], [(200, 200, 200, 255), (255, 255, 255, 255)]])
    flat = png([[(128, 128, 255, 255)] * 2] * 2)
    images = [{"uri": data_uri("image/png", white)}, {"uri": data_uri("image/png", flat)}]
    doc = scene_with({"mesh": 0, "name": "UvQuad"})
    doc.update({
        "extensionsUsed": ["KHR_texture_transform", "KHR_materials_ior", "KHR_materials_specular"],
        "meshes": [{"name": "UvQuad", "primitives": [{"attributes": {"POSITION": pos, "NORMAL": nrm, "TEXCOORD_0": uv0, "TEXCOORD_1": uv1}, "indices": idx, "material": 0}]}],
        "materials": [{
            "name": "TwoSets",
            "pbrMetallicRoughness": {
                "baseColorTexture": {"index": 0, "texCoord": 0, "extensions": {"KHR_texture_transform": {"offset": [0.25, 0.5], "rotation": 0.5, "scale": [2.0, 3.0]}}},
                "metallicFactor": 0.25,
                "roughnessFactor": 0.75,
            },
            "normalTexture": {"index": 1, "texCoord": 0, "extensions": {"KHR_texture_transform": {"offset": [0.1, 0.2], "scale": [0.5, 0.5], "texCoord": 1}}},
            "occlusionTexture": {"index": 0, "texCoord": 1, "strength": 0.5},
            "extensions": {"KHR_materials_ior": {"ior": 1.33}, "KHR_materials_specular": {"specularFactor": 0.7, "specularColorFactor": [0.9, 0.8, 0.7]}},
        }],
        "textures": [{"source": 0}, {"source": 1}],
        "images": images,
    })
    write("uvsets.gltf", b.document(doc))


SPARSE_OVERRIDES = {1: (2.0, -1.5, 0.25), 3: (-1.25, 2.0, -0.5)}
SPARSE_UV = {2: (0.5, 0.75)}


def sparse():
    b = Buffer()
    final = [SPARSE_OVERRIDES.get(i, p) for i, p in enumerate(QUAD_POSITIONS)]
    pos = b.sparse(4, "VEC3", SPARSE_OVERRIDES, base_rows=QUAD_POSITIONS, bounds_rows=final)
    nrm = b.floats(QUAD_NORMALS, "VEC3")
    uv0 = b.sparse(4, "VEC2", SPARSE_UV)
    idx = b.indices(QUAD_INDICES)
    doc = scene_with({"mesh": 0, "name": "SparseQuad"})
    doc["meshes"] = [{"name": "SparseQuad", "primitives": [{"attributes": {"POSITION": pos, "NORMAL": nrm, "TEXCOORD_0": uv0}, "indices": idx}]}]
    write("sparse.gltf", b.document(doc))


MORPH_DENSE = [(0.0, 0.0, 0.5), (0.0, 0.0, 0.25), (0.0, 0.0, -0.25), (0.0, 0.0, -0.5)]
MORPH_SPARSE = {2: (0.0, 1.0, 1.0)}
MORPH_WEIGHTS = [0.25, 0.5]


def morph():
    b = Buffer()
    pos = b.floats(QUAD_POSITIONS, "VEC3", True)
    nrm = b.floats(QUAD_NORMALS, "VEC3")
    idx = b.indices(QUAD_INDICES)
    t0 = b.floats(MORPH_DENSE, "VEC3", True, target=None)
    dense1 = [MORPH_SPARSE.get(i, (0.0, 0.0, 0.0)) for i in range(4)]
    t1 = b.sparse(4, "VEC3", MORPH_SPARSE, bounds_rows=dense1)
    doc = scene_with({"mesh": 0, "name": "MorphQuad"})
    doc["meshes"] = [{"name": "MorphQuad", "weights": MORPH_WEIGHTS, "extras": {"targetNames": ["Bulge", "Lift"]},
                      "primitives": [{"attributes": {"POSITION": pos, "NORMAL": nrm}, "indices": idx, "targets": [{"POSITION": t0}, {"POSITION": t1}]}]}]
    write("morph.gltf", b.document(doc))


TRIANGLE = [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)]
IMAGINARY = "EXT_museum_imaginary_feature"


def triangle_doc(extra_node=None):
    b = Buffer()
    pos = b.floats(TRIANGLE, "VEC3", True)
    node = {"mesh": 0, "name": "Triangle"}
    if extra_node:
        node.update(extra_node(b))
    doc = scene_with(node)
    doc["meshes"] = [{"name": "Triangle", "primitives": [{"attributes": {"POSITION": pos}}]}]
    return b, doc


def required_and_used():
    b, doc = triangle_doc()
    doc["extensionsUsed"] = [IMAGINARY]
    doc["extensionsRequired"] = [IMAGINARY]
    write("required.gltf", b.document(doc))
    b, doc = triangle_doc()
    doc["extensionsUsed"] = [IMAGINARY]
    write("used.gltf", b.document(doc))


INSTANCES = [(0.0, 0.0, 0.0), (2.0, 0.0, 0.0), (4.0, 0.0, 0.0)]


def instancing():
    def node(b):
        translation = b.floats(INSTANCES, "VEC3", target=None)
        return {"extensions": {"EXT_mesh_gpu_instancing": {"attributes": {"TRANSLATION": translation}}}}

    b, doc = triangle_doc(node)
    doc["extensionsUsed"] = ["EXT_mesh_gpu_instancing"]
    write("instancing.gltf", b.document(doc))


PLAQUE_PIXELS = [[(255, 0, 255, 255), (255, 0, 0, 128), (0, 255, 0, 255), (0, 0, 255, 255)]] + [[(64, 128, 192, 255)] * 4] * 3


def sources():
    (OUT / "plaque.png").write_bytes(png(PLAQUE_PIXELS))
    (OUT / "plaque.label").write_text("Pipeline Workshop\n")
    (OUT / "caption.label").write_text("caption text\n")
    (OUT / "plaque.notes").write_text("curated in the workshop\n")


def wav(name, bits, channels, rate, samples, loop=None):
    """A PCM WAV: `samples` are already-encoded little-endian frame bytes; `loop` = (first, last) frame of a smpl loop,
    last inclusive as the RIFF smpl chunk stores it."""
    block = channels * bits // 8
    fmt = struct.pack("<HHIIHH", 1, channels, rate, rate * block, block, bits)
    body = b"WAVE" + b"fmt " + struct.pack("<I", len(fmt)) + fmt + b"data" + struct.pack("<I", len(samples)) + samples
    if len(samples) % 2:
        body += b"\x00"
    if loop is not None:
        # smpl: manufacturer, product, sample period, MIDI unity note, pitch fraction, SMPTE format/offset, loop count, sampler data,
        # then one loop: cue id, type 0 (forward), start, end (inclusive), fraction, play count 0 (infinite).
        smpl = struct.pack("<9I", 0, 0, 1000000000 // rate, 60, 0, 0, 0, 1, 0) + struct.pack("<6I", 0, 0, loop[0], loop[1], 0, 0)
        body += b"smpl" + struct.pack("<I", len(smpl)) + smpl
    (OUT / name).write_bytes(b"RIFF" + struct.pack("<I", len(body)) + body)


TONE8_FRAMES = 80                                      # tone8.wav: 8-bit unsigned, mono, 8000 Hz; sample k = 128 + 10 * (k % 10)
LOOP16_FRAMES = 40                                     # loop16.wav: 16-bit stereo 8000 Hz; left = 100 k, right = -100 k; loop frames 10..29


def build_sources():
    """Sources of the C06 rooms (wk-routes, wk-incremental) and the task room (wk-tasks)."""
    # A Curve in CNA's own .cnj: a step from 2.5 to 7.5 at position 2, constant outside the keys.
    (OUT / "curve.cnj").write_text(json.dumps({"cnjVersion": 1, "type": "Curve", "preLoop": "Constant", "postLoop": "Constant",
                                               "keys": [{"position": 0, "value": 2.5, "continuity": "Step"}, {"position": 2, "value": 7.5}]}) + "\n")
    # A Texture2D .cnj naming plaque.png as its sidecar (no colour key requested).
    (OUT / "texture.cnj").write_text(json.dumps({"cnjVersion": 1, "type": "Texture2D", "sourceFile": "plaque.png"}) + "\n")
    # A .cnj of a type CNA's compiler does not know: refused by name.
    (OUT / "unknown.cnj").write_text(json.dumps({"cnjVersion": 1, "type": "Effect"}) + "\n")
    wav("tone8.wav", 8, 1, 8000, bytes(128 + 10 * (k % 10) for k in range(TONE8_FRAMES)))
    wav("loop16.wav", 16, 2, 8000, b"".join(struct.pack("<hh", 100 * k, -100 * k) for k in range(LOOP16_FRAMES)), loop=(10, 29))
    wav("deep24.wav", 24, 1, 8000, b"".join(struct.pack("<i", 1000 * k)[:3] for k in range(10)))
    # XNA's intermediate XML for a framework value type.
    (OUT / "vector.xml").write_text('<?xml version="1.0" encoding="utf-8"?>\n<XnaContent>\n  <Asset Type="Microsoft.Xna.Framework.Vector3">1.5 -2 3.25</Asset>\n</XnaContent>\n')
    # An XACT project file for BuildXact's validation (never compiled: XactBld3.exe is Microsoft's tool).
    (OUT / "workshop.contentproj").write_text(CONTENT_PROJECT)
    (OUT / "workshop.xap").write_text("Signature = XACT3;\nVersion = 18;\nContent Version = 46;\nRelease = February 2010;\n")


CONTENT_PROJECT = """<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <ProjectGuid>{6F7A2D1C-0000-4000-8000-00000000C0DE}</ProjectGuid>
    <XnaFrameworkVersion>v4.0</XnaFrameworkVersion>
    <XnaPlatform>Windows</XnaPlatform>
    <XnaProfile>HiDef</XnaProfile>
    <XnaCompressContent>False</XnaCompressContent>
  </PropertyGroup>
  <PropertyGroup Condition=" '$(Configuration)' == '' ">
    <Configuration>Debug</Configuration>
  </PropertyGroup>
  <ItemGroup>
    <Compile Include="plaque.png">
      <Name>plaque</Name>
      <Importer>TextureImporter</Importer>
      <Processor>TextureProcessor</Processor>
    </Compile>
    <Compile Include="tone8.wav">
      <Name>tone8</Name>
      <Importer>WavImporter</Importer>
      <Processor>SoundEffectProcessor</Processor>
    </Compile>
    <Compile Include="vector.xml">
      <Name>vector</Name>
      <Importer>XmlImporter</Importer>
      <Processor>PassThroughProcessor</Processor>
    </Compile>
  </ItemGroup>
  <ItemGroup>
    <None Include="plaque.notes">
      <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>
    </None>
  </ItemGroup>
</Project>
"""


ANIMATED_X = """xof 0303txt 0032
// Written by tools/gen_workshop.py for the Pipeline Workshop (room wk-animation). Two frames, no mesh, one AnimationSet that
// moves the Arm frame along +X with position keys at ticks 0, 5 and 10, at 10 ticks per second (0 s, 0.5 s, 1 s), and a
// second animation on the Hand frame with matrix keys at ticks 0 and 10 (a lift of 0.25 along +Y).

AnimTicksPerSecond {
  10;
}

Frame Root {
  FrameTransformMatrix {
    1.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,1.0;;
  }
  Frame Arm {
    FrameTransformMatrix {
      1.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,1.0;;
    }
    Frame Hand {
      FrameTransformMatrix {
        1.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,1.0;;
      }
    }
  }
}

AnimationSet Wave {
  Animation {
    { Arm }
    AnimationKey {
      2;
      3;
      0;3;0.0,0.0,0.0;;,
      5;3;1.0,0.0,0.0;;,
      10;3;2.0,0.0,0.0;;;
    }
  }
  Animation {
    { Hand }
    AnimationKey {
      4;
      2;
      0;16;1.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,1.0;;,
      10;16;1.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.25,0.0,1.0;;;
    }
  }
}
"""


def seven_bit(value):
    out = bytearray()
    while value >= 0x80:
        out.append((value & 0x7F) | 0x80)
        value >>= 7
    out.append(value)
    return bytes(out)


def xnb_string(text):
    data = text.encode("utf-8")
    return seven_bit(len(data)) + data


GAME = ", MuseumGame, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null"
FRAMEWORK = ", Microsoft.Xna.Framework, Version=4.0.0.0, Culture=neutral, PublicKeyToken=842cf8be1de50553"


def reflective_xnb():
    """An .xnb as XNA 4.0 writes a game type that has no ContentTypeWriter of its own (room wk-typewriter): the implicit
    ReflectiveWriter<T>, read at run time by ReflectiveReader<T>. The game's C# types:

        namespace Museum.Workshop {
            public enum Wing { North, South, East, West }
            public class SettingsBase { public int Revision; }
            public class Curator { public string Name; public int Years; }
            public class ExhibitSettings : SettingsBase {
                public string Title; public int Visitors; public float Scale; public Vector3 Position; public bool Open;
                public Wing Wing; public TimeSpan Opening; public Color Tint; public Curator Curator; public Curator Deputy;
            }
        }

    ReflectiveReader<T>.Read (XNA 4.0) reads the base type's members first, then T's properties, then its fields, in declaration
    order; ReflectiveReaderMemberHelper reads a value-type member inline and a reference-type member (string, Curator) as a
    1-based type-reader index followed by the object, 0 meaning null. The type table holds every reader the members name."""
    readers = [
        "Microsoft.Xna.Framework.Content.ReflectiveReader`1[[Museum.Workshop.ExhibitSettings" + GAME + "]]" + FRAMEWORK,   # 1
        "Microsoft.Xna.Framework.Content.ReflectiveReader`1[[Museum.Workshop.SettingsBase" + GAME + "]]" + FRAMEWORK,      # 2
        "Microsoft.Xna.Framework.Content.Int32Reader" + FRAMEWORK,                                                         # 3
        "Microsoft.Xna.Framework.Content.StringReader" + FRAMEWORK,                                                        # 4
        "Microsoft.Xna.Framework.Content.SingleReader" + FRAMEWORK,                                                        # 5
        "Microsoft.Xna.Framework.Content.Vector3Reader" + FRAMEWORK,                                                       # 6
        "Microsoft.Xna.Framework.Content.BooleanReader" + FRAMEWORK,                                                       # 7
        "Microsoft.Xna.Framework.Content.EnumReader`1[[Museum.Workshop.Wing" + GAME + "]]" + FRAMEWORK,                    # 8
        "Microsoft.Xna.Framework.Content.TimeSpanReader" + FRAMEWORK,                                                      # 9
        "Microsoft.Xna.Framework.Content.ColorReader" + FRAMEWORK,                                                         # 10
        "Microsoft.Xna.Framework.Content.ReflectiveReader`1[[Museum.Workshop.Curator" + GAME + "]]" + FRAMEWORK,           # 11
    ]
    body = seven_bit(len(readers)) + b"".join(xnb_string(r) + struct.pack("<i", 0) for r in readers) + seven_bit(0)
    body += seven_bit(1)                                     # the root object: ReflectiveReader<ExhibitSettings>
    body += struct.pack("<i", 7)                             # SettingsBase.Revision
    body += seven_bit(4) + xnb_string("Hall of Pipelines")   # Title (string: reader index 4)
    body += struct.pack("<i", 1234)                          # Visitors
    body += struct.pack("<f", 0.75)                          # Scale
    body += struct.pack("<3f", 1.5, -2.0, 3.25)              # Position
    body += b"\x01"                                          # Open
    body += struct.pack("<i", 2)                             # Wing.East
    body += struct.pack("<q", 342000000000)                  # Opening: 9 h 30 min in 100 ns ticks
    body += bytes((10, 20, 30, 255))                         # Tint: Color(10, 20, 30, 255), packed R, G, B, A
    body += seven_bit(11) + seven_bit(4) + xnb_string("Ada") + struct.pack("<i", 12)   # Curator { Name "Ada", Years 12 }
    body += seven_bit(0)                                     # Deputy: null
    header = b"XNBw" + bytes((5, 0))
    (OUT / "reflective.xnb").write_bytes(header + struct.pack("<I", len(header) + 4 + len(body)) + body)


def media():
    if shutil.which("ffmpeg") is None:
        print("ffmpeg not found: media files not written", file=sys.stderr)
        return
    sine = ["-f", "lavfi", "-i", "sine=frequency=440:sample_rate=22050:duration=0.5"]
    common = ["-hide_banner", "-loglevel", "error", "-y"]
    exact = ["-map_metadata", "-1", "-fflags", "+bitexact", "-flags:a", "+bitexact"]
    subprocess.run(["ffmpeg", *common, *sine, "-ac", "1", "-c:a", "libmp3lame", "-b:a", "32k", "-id3v2_version", "0", "-write_xing", "0", *exact, str(OUT / "tone.mp3")], check=True)
    subprocess.run(["ffmpeg", *common, *sine, "-ac", "1", "-c:a", "wmav2", "-b:a", "32k", *exact, str(OUT / "tone.wma")], check=True)
    pattern = ["-f", "lavfi", "-i", "testsrc=size=32x24:rate=10:duration=1"]
    video = ["-c:v", "wmv2", "-b:v", "64k", "-flags:v", "+bitexact"]
    subprocess.run(["ffmpeg", *common, *pattern, "-an", *video, *exact, str(OUT / "clip.wmv")], check=True)
    subprocess.run(["ffmpeg", *common, *pattern, *sine, "-map", "0:v", "-map", "1:a", *video, "-c:a", "wmav2", "-b:a", "32k", "-ac", "1", *exact, str(OUT / "clip_audio.wmv")], check=True)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    uvsets()
    sparse()
    morph()
    required_and_used()
    instancing()
    sources()
    build_sources()
    (OUT / "arm.x").write_text(ANIMATED_X)
    reflective_xnb()
    media()
    for path in sorted(OUT.iterdir()):
        print("%8d %s" % (path.stat().st_size, path.relative_to(HERE)))


if __name__ == "__main__":
    main()

#include "Rendering/WireMesh.hpp"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "Diagnostics/Log.hpp"
#include "Microsoft/Xna/Framework/Graphics/BufferUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexElementSize.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelMesh.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelMeshPart.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColor.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionNormalTangentTexture.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

std::unique_ptr<WireMesh> WireMesh::FromModel(GraphicsDevice& device, Model& model, const Color& colour)
{
    try {
        std::vector<VertexPositionColor> lines;
        std::set<std::pair<std::uint32_t, std::uint32_t>> seen;
        for (ModelMesh* mesh : model.getMeshesProperty()) {
            for (int p = 0; p < mesh->getMeshPartsProperty().getCountProperty(); ++p) {
                ModelMeshPart* part = mesh->getMeshPartsProperty()[p];
                VertexBuffer* vb = part->getVertexBufferProperty();
                IndexBuffer* ib = part->getIndexBufferProperty();
                if (vb == nullptr || ib == nullptr || vb->getVertexDeclarationProperty().getVertexStrideProperty() != 48) continue;
                std::vector<VertexPositionNormalTangentTexture> vertices(static_cast<std::size_t>(vb->getVertexCountProperty()));
                vb->GetData(vertices.data(), static_cast<int>(vertices.size()));
                std::vector<std::uint16_t> indices(static_cast<std::size_t>(ib->getIndexCountProperty()));
                if (ib->getIndexElementSizeProperty() != IndexElementSize::SixteenBits) continue;
                ib->GetData(indices.data(), static_cast<int>(indices.size()));
                const int first = part->getStartIndexProperty();
                const int last = first + part->getPrimitiveCountProperty() * 3;
                const int base = part->getVertexOffsetProperty();
                for (int i = first; i + 2 < last + 0 && i + 2 < static_cast<int>(indices.size()); i += 3) {
                    for (int e = 0; e < 3; ++e) {
                        const std::uint32_t a = indices[static_cast<std::size_t>(i + e)] + static_cast<std::uint32_t>(base);
                        const std::uint32_t b = indices[static_cast<std::size_t>(i + (e + 1) % 3)] + static_cast<std::uint32_t>(base);
                        if (!seen.insert({std::min(a, b), std::max(a, b)}).second) continue;
                        lines.emplace_back(vertices[a].Position, colour);
                        lines.emplace_back(vertices[b].Position, colour);
                    }
                }
            }
        }
        if (lines.empty()) return nullptr;
        std::unique_ptr<WireMesh> wire(new WireMesh());
        wire->edges_ = static_cast<int>(lines.size()) / 2;
        wire->buffer_ = std::make_unique<VertexBuffer>(device, VertexPositionColor::getVertexDeclarationStatic(),
                                                       static_cast<int>(lines.size()), BufferUsage::WriteOnly);
        wire->buffer_->SetData(lines.data(), static_cast<int>(lines.size()));
        return wire;
    } catch (const std::exception& e) {
        Museum::Log("wire", "cannot build a wireframe from the model: %s", e.what());
        Museum::ReportExhibitError("wire", (std::string("cannot build a wireframe from the model: ") + e.what()).c_str());
        return nullptr;
    }
}

void WireMesh::Draw(GraphicsDevice& device) const
{
    device.SetVertexBuffer(buffer_.get());
    device.DrawPrimitives(PrimitiveType::LineList, 0, edges_);
}

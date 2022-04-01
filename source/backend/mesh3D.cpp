#include "mesh3D.hpp"

void Mesh3D::destroy(VkDevice device)
{
    m_vertices.destroy(device);
    m_indices.destroy(device);
}

void Mesh3D::load(const VulkanDevice *device,
                  VkQueue queue,
                  view<MeshVertex> vertices,
                  view<MeshIndex> indices)
{
    auto vertexInfo = vkInits::bufferCreateInfo(vertices.size(), USAGE_VERTEX_TRANSFER_SRC);
    auto vertexTransfer = VulkanBuffer();
    vertexTransfer.create(device, &vertexInfo, MEM_FLAG_HOST_VISIBLE, vertices.data);

    auto indexInfo = vkInits::bufferCreateInfo(indices.size(), USAGE_INDEX_TRANSFER_SRC);
    auto indexTransfer = VulkanBuffer();
    indexTransfer.create(device, &indexInfo, MEM_FLAG_HOST_VISIBLE, indices.data);

    vertexInfo.usage = USAGE_VERTEX_TRANSFER_DST;
    m_vertices.create(device, &vertexInfo, MEM_FLAG_GPU_LOCAL);

    indexInfo.usage = USAGE_INDEX_TRANSFER_DST;
    m_indices.create(device, &indexInfo, MEM_FLAG_GPU_LOCAL);

    auto command = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    vertexTransfer.copy(command, &m_vertices);
    indexTransfer.copy(command, &m_indices);

    device->flushCommandBuffer(command, queue);

    vertexTransfer.destroy(device->device);
    indexTransfer.destroy(device->device);

    m_indexCount = indices.count;
}

void Mesh3D::loadSphere(const VulkanDevice* device, VkQueue queue)
{
    constexpr auto N_STACKS = clamp<uint32_t>(64, 2, 128);
    constexpr auto N_SLICES = clamp<uint32_t>(64, 2, 128);

    auto vertices = data_buffer<MeshVertex>((N_STACKS - 1) * N_SLICES + 2);
    auto vertex = vertices.data();

    // Top vertex
    vertex->position = vec3(0.0f, 1.0f, 0.0f);
    vertex->normal = vertex->position;
    vertex++;

    // Vertex fill
    for (size_t i = 0; i < N_STACKS - 1; i++){
        for (size_t j = 0; j < N_SLICES; j++){
            const auto xSegment = float(j) / float(N_SLICES);
            const auto ySegment = float(i + 1) / float(N_STACKS);
            const auto theta = 2 * PI32 * xSegment;
            const auto phi = PI32 * ySegment;
            vertex->position.x = std::sin(phi) * std::cos(theta);
            vertex->position.y = std::cos(phi);
            vertex->position.z = std::sin(phi) * std::sin(theta);
            vertex->normal = vertex->position;
#if 1
            vertex->uv.x = (std::asin(-vertex->normal.x) / PI32) + 0.5f;
            vertex->uv.y = (std::asin(-vertex->normal.y) / PI32) + 0.5f;
#else
            vertex->uv = vec2(xSegment, ySegment);
#endif
            vertex++;
        }
    }

    // Bottom vertex
    vertex->position = vec3(0.0f, -1.0f, 0.0f);
    vertex->normal = vertex->position;

    // 3 idx per triangle * (top + bottom) * num slices
    constexpr auto idxCountTriangles = 3 * 2 * N_SLICES;
    // 6 idx per quad * (only quad stacks) * num slices
    constexpr auto idxCountQuads = 6 * (N_STACKS - 2) * N_SLICES;

    auto indices = data_buffer<MeshIndex>(idxCountTriangles + idxCountQuads);
    auto index = indices.data();

    const auto indexLast = uint32_t(vertices.capacity() - 1);

    // Top and bottom triangles
    for (uint32_t i = 0; i < N_SLICES; i++){
        auto i0 = i + 1;
        auto i1 = i0 % N_SLICES + 1;
        *(index++) = 0;
        *(index++) = i1;
        *(index++) = i0;

        i0 += N_SLICES * (N_STACKS - 2);
        i1 += N_SLICES * (N_STACKS - 2);
        *(index++) = indexLast;
        *(index++) = i0;
        *(index++) = i1;
    }

    // Quad fill the rest of the sphere
    for (uint32_t i = 0; i < N_STACKS - 2; i++){
        const auto i0 = i * N_SLICES + 1;
        const auto i1 = (i + 1) * N_SLICES + 1;
        for (uint32_t j = 0; j < N_SLICES; j++){
            const auto j0 = i0 + j;
            const auto j1 = i0 + (j + 1) % N_SLICES;
            const auto j2 = i1 + (j + 1) % N_SLICES;
            const auto j3 = i1 + j;

            *(index++) = j0;
            *(index++) = j1;
            *(index++) = j2;
            *(index++) = j0;
            *(index++) = j2;
            *(index++) = j3;
        }
    }

    load(device, queue, vertices.getView(), indices.getView());
}

void Mesh3D::loadCube(const VulkanDevice* device, VkQueue queue)
{
    constexpr auto normalFront = vec3(0.0f, 0.0f, -1.0f);
    constexpr auto normalBack = vec3(0.0f, 0.0f, 1.0f);
    constexpr auto normalTop = vec3(0.0f, 1.0f, 0.0f);
    constexpr auto normalBottom = vec3(0.0f, -1.0f, 0.0f);
    constexpr auto normalLeft = vec3(1.0f, 0.0f, 0.0f);
    constexpr auto normalRight = vec3(-1.0f, 0.0f, 0.0f);

    const float size = 0.5f;
    const vec3<float> points[] = {
        vec3(-size, -size, -size),
        vec3(size, -size, -size),
        vec3(size, size, -size),
        vec3(-size, size, -size),
        vec3(-size, -size, size),
        vec3(size, -size, size),
        vec3(size, size, size),
        vec3(-size, size, size),
    };

    constexpr vec2<float> uvs[] = {
        vec2(0.0f, 0.0f),
        vec2(1.0f, 0.0f),
        vec2(1.0f, 1.0f),
        vec2(0.0f, 1.0f)
    };

    auto vertices = data_buffer<MeshVertex>(24);
    vertices[0] = {points[0], normalFront, uvs[3]};
    vertices[1] = {points[3], normalFront, uvs[0]};
    vertices[2] = {points[2], normalFront, uvs[1]};
    vertices[3] = {points[1], normalFront, uvs[2]};

    vertices[4] = {points[4], normalBack, uvs[0]};
    vertices[5] = {points[5], normalBack, uvs[1]};
    vertices[6] = {points[6], normalBack, uvs[2]};
    vertices[7] = {points[7], normalBack, uvs[3]};

    vertices[8] = {points[3], normalTop, uvs[0]};
    vertices[9] = {points[7], normalTop, uvs[1]};
    vertices[10] = {points[6], normalTop, uvs[2]};
    vertices[11] = {points[2], normalTop, uvs[3]};

    vertices[12] = {points[4], normalBottom, uvs[0]};
    vertices[13] = {points[0], normalBottom, uvs[1]};
    vertices[14] = {points[1], normalBottom, uvs[2]};
    vertices[15] = {points[5], normalBottom, uvs[3]};

    vertices[16] = {points[4], normalRight, uvs[0]};
    vertices[17] = {points[7], normalRight, uvs[1]};
    vertices[18] = {points[3], normalRight, uvs[2]};
    vertices[19] = {points[0], normalRight, uvs[3]};

    vertices[20] = {points[1], normalLeft, uvs[0]};
    vertices[21] = {points[2], normalLeft, uvs[1]};
    vertices[22] = {points[6], normalLeft, uvs[2]};
    vertices[23] = {points[5], normalLeft, uvs[3]};

    auto indices = data_buffer<MeshIndex>(36);
    for (uint32_t i = 0; i < 6; i++){
        const auto index = 6 * i;
        const auto offset = 4 * i;
        indices[index] = offset;
        indices[index + 1] = offset + 1;
        indices[index + 2] = offset + 2;
        indices[index + 3] = offset + 2;
        indices[index + 4] = offset + 3;
        indices[index + 5] = offset;
    }

    load(device, queue, vertices.getView(), indices.getView());
}

void Mesh3D::draw(VkCommandBuffer cmd, VkPipelineLayout layout)
{
    const VkDeviceSize vertexOffset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &m_vertices.data, &vertexOffset);

    const VkDeviceSize indexOffset = 0;
    vkCmdBindIndexBuffer(cmd, m_indices.data, indexOffset, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(cmd, uint32_t(m_indexCount), 1, 0, 0, 0);
}
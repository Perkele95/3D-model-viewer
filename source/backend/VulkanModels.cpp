#include "VulkanModels.hpp"

void ModelBase::draw(VkCommandBuffer cmd)
{
    const VkDeviceSize vertexOffset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &m_vertices.data, &vertexOffset);

    const VkDeviceSize indexOffset = 0;
    vkCmdBindIndexBuffer(cmd, m_indices.data, indexOffset, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(cmd, m_indexCount, 1, 0, 0, 0);
}

void ModelBase::destroy(VkDevice device)
{
    m_vertices.destroy(device);
    m_indices.destroy(device);
}

VkPushConstantRange Model3D::pushConstant()
{
    VkPushConstantRange range{};
    range.offset = 0;
    range.size = sizeof(transform);
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    return range;
}

void Model3D::loadSpherePrimitive(const VulkanDevice *device, VkQueue queue)
{
    constexpr auto N_STACKS = 64;
    constexpr auto N_SLICES = 64;

    const uint32_t vertexCount = (N_STACKS - 1) * N_SLICES + 2;
    m_indexCount = 6 * N_SLICES * (N_STACKS - 1);

    VulkanBuffer vertexTransfer, indexTransfer;
    device->createBuffer(USAGE_VERTEX_TRANSFER_SRC,
                         MEM_FLAG_HOST_VISIBLE,
                         vertexCount * sizeof(Vertex)   ,
                         vertexTransfer);
    device->createBuffer(USAGE_INDEX_TRANSFER_SRC,
                         MEM_FLAG_HOST_VISIBLE,
                         m_indexCount * sizeof(Index),
                         indexTransfer);

    vertexTransfer.map(device->device);
    auto vertices = static_cast<Vertex*>(vertexTransfer.mapped);
    auto vertex = vertices;

    // Top vertex
    vertex->position = vec3(0.0f, 1.0f, 0.0f);
    vertex->normal = vertex->position;
    vertex++;

    // Vertex fill
    for (size_t i = 0; i < N_STACKS - 1; i++)
    {
        for (size_t j = 0; j < N_SLICES; j++)
        {
            const auto xSegment = float(j) / float(N_SLICES);
            const auto ySegment = float(i + 1) / float(N_STACKS);
            const auto theta = 2 * PI32 * xSegment;
            const auto phi = PI32 * ySegment;
            vertex->position.x = std::sin(phi) * std::cos(theta);
            vertex->position.y = std::cos(phi);
            vertex->position.z = std::sin(phi) * std::sin(theta);
            vertex->normal = vertex->position;
            vertex->uv.x = (std::asin(-vertex->normal.x) / PI32) + 0.5f;
            vertex->uv.y = (std::asin(-vertex->normal.y) / PI32) + 0.5f;
            vertex++;
        }
    }

    // Bottom vertex
    vertex->position = vec3(0.0f, -1.0f, 0.0f);
    vertex->normal = vertex->position;
    vertexTransfer.unmap(device->device);

    indexTransfer.map(device->device);
    auto indices = static_cast<Index*>(indexTransfer.mapped);
    auto index = indices;

    // Top and bottom triangles
    for (uint32_t i = 0; i < N_SLICES; i++)
    {
        auto i0 = i + 1;
        auto i1 = i0 % N_SLICES + 1;
        *(index++) = 0;
        *(index++) = i1;
        *(index++) = i0;

        i0 += N_SLICES * (N_STACKS - 2);
        i1 += N_SLICES * (N_STACKS - 2);
        *(index++) = vertexCount - 1;
        *(index++) = i0;
        *(index++) = i1;
    }

    // Quad fill the rest of the sphere
    for (uint32_t i = 0; i < N_STACKS - 2; i++)
    {
        const auto i0 = i * N_SLICES + 1;
        const auto i1 = (i + 1) * N_SLICES + 1;
        for (uint32_t j = 0; j < N_SLICES; j++)
        {
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

    indexTransfer.unmap(device->device);

    device->createBuffer(USAGE_VERTEX_TRANSFER_DST,
                         MEM_FLAG_GPU_LOCAL,
                         vertexTransfer.size,
                         m_vertices);
    device->createBuffer(USAGE_INDEX_TRANSFER_DST,
                         MEM_FLAG_GPU_LOCAL,
                         indexTransfer.size,
                         m_indices);

    auto cmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    vertexTransfer.transfer(cmd, m_vertices);
    indexTransfer.transfer(cmd, m_indices);

    device->flushCommandBuffer(cmd, queue);

    vertexTransfer.destroy(device->device);
    indexTransfer.destroy(device->device);
}

void Model3D::loadCubePrimitive(const VulkanDevice *device, VkQueue queue)
{
    constexpr auto normalPosZ = vec3(0.0f, 0.0f, 1.0f);
    constexpr auto normalNegZ = vec3(0.0f, 0.0f, -1.0f);
    constexpr auto normalPosY = vec3(0.0f, 1.0f, 0.0f);
    constexpr auto normalNegY = vec3(0.0f, -1.0f, 0.0f);
    constexpr auto normalPosX = vec3(1.0f, 0.0f, 0.0f);
    constexpr auto normalNegX = vec3(-1.0f, 0.0f, 0.0f);

    constexpr float SIZE = 0.5f;
    constexpr vec3<float> points[] = {
        vec3(-SIZE, -SIZE, -SIZE),
        vec3(SIZE, -SIZE, -SIZE),
        vec3(SIZE, SIZE, -SIZE),
        vec3(-SIZE, SIZE, -SIZE),
        vec3(-SIZE, -SIZE, SIZE),
        vec3(SIZE, -SIZE, SIZE),
        vec3(SIZE, SIZE, SIZE),
        vec3(-SIZE, SIZE, SIZE),
    };

    constexpr vec2<float> uvs[] = {
        vec2(0.0f, 0.0f),
        vec2(1.0f, 0.0f),
        vec2(1.0f, 1.0f),
        vec2(0.0f, 1.0f)
    };

    constexpr size_t VERTEX_COUNT = 24;
    m_indexCount = 36;

    VulkanBuffer vertexTransfer, indexTransfer;
    device->createBuffer(USAGE_VERTEX_TRANSFER_SRC,
                         MEM_FLAG_HOST_VISIBLE,
                         VERTEX_COUNT * sizeof(Vertex)   ,
                         vertexTransfer);
    device->createBuffer(USAGE_INDEX_TRANSFER_SRC,
                         MEM_FLAG_HOST_VISIBLE,
                         m_indexCount * sizeof(Index),
                         indexTransfer);

    vertexTransfer.map(device->device);
    auto vertices = static_cast<Vertex*>(vertexTransfer.mapped);

    vertices[0] = {points[0], normalNegZ, uvs[3]};
    vertices[1] = {points[3], normalNegZ, uvs[0]};
    vertices[2] = {points[2], normalNegZ, uvs[1]};
    vertices[3] = {points[1], normalNegZ, uvs[2]};

    vertices[4] = {points[4], normalPosZ, uvs[0]};
    vertices[5] = {points[5], normalPosZ, uvs[1]};
    vertices[6] = {points[6], normalPosZ, uvs[2]};
    vertices[7] = {points[7], normalPosZ, uvs[3]};

    vertices[8] = {points[3], normalPosY, uvs[0]};
    vertices[9] = {points[7], normalPosY, uvs[1]};
    vertices[10] = {points[6], normalPosY, uvs[2]};
    vertices[11] = {points[2], normalPosY, uvs[3]};

    vertices[12] = {points[4], normalNegY, uvs[0]};
    vertices[13] = {points[0], normalNegY, uvs[1]};
    vertices[14] = {points[1], normalNegY, uvs[2]};
    vertices[15] = {points[5], normalNegY, uvs[3]};

    vertices[16] = {points[4], normalNegX, uvs[0]};
    vertices[17] = {points[7], normalNegX, uvs[1]};
    vertices[18] = {points[3], normalNegX, uvs[2]};
    vertices[19] = {points[0], normalNegX, uvs[3]};

    vertices[20] = {points[1], normalPosX, uvs[0]};
    vertices[21] = {points[2], normalPosX, uvs[1]};
    vertices[22] = {points[6], normalPosX, uvs[2]};
    vertices[23] = {points[5], normalPosX, uvs[3]};

    vertexTransfer.unmap(device->device);

    indexTransfer.map(device->device);
    auto indices = static_cast<Index*>(indexTransfer.mapped);

    for (uint32_t i = 0; i < 6; i++)
    {
        const auto index = 6 * i;
        const auto offset = 4 * i;
        indices[index] = offset;
        indices[index + 1] = offset + 1;
        indices[index + 2] = offset + 2;
        indices[index + 3] = offset + 2;
        indices[index + 4] = offset + 3;
        indices[index + 5] = offset;
    }

    indexTransfer.unmap(device->device);

    device->createBuffer(USAGE_VERTEX_TRANSFER_DST,
                         MEM_FLAG_GPU_LOCAL,
                         vertexTransfer.size,
                         m_vertices);
    device->createBuffer(USAGE_INDEX_TRANSFER_DST,
                         MEM_FLAG_GPU_LOCAL,
                         indexTransfer.size,
                         m_indices);

    auto cmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    vertexTransfer.transfer(cmd, m_vertices);
    indexTransfer.transfer(cmd, m_indices);

    device->flushCommandBuffer(cmd, queue);

    vertexTransfer.destroy(device->device);
    indexTransfer.destroy(device->device);
}

void CubemapModel::load(const VulkanDevice *device, VkQueue queue)
{
    constexpr size_t VERTEX_COUNT = 8;
    m_indexCount = 36;

    VulkanBuffer vertexTransfer, indexTransfer;
    device->createBuffer(USAGE_VERTEX_TRANSFER_SRC,
                         MEM_FLAG_HOST_VISIBLE,
                         VERTEX_COUNT * sizeof(Vertex)   ,
                         vertexTransfer);
    device->createBuffer(USAGE_INDEX_TRANSFER_SRC,
                         MEM_FLAG_HOST_VISIBLE,
                         m_indexCount * sizeof(Index),
                         indexTransfer);

    vertexTransfer.map(device->device);
    auto vertices = static_cast<Vertex*>(vertexTransfer.mapped);

    constexpr float SIZE = 10.0f;
    vertices[0] = {vec3(-SIZE, -SIZE, -SIZE)};
    vertices[1] = {vec3(SIZE, -SIZE, -SIZE)};
    vertices[2] = {vec3(SIZE, SIZE, -SIZE)};
    vertices[3] = {vec3(-SIZE, SIZE, -SIZE)};
    vertices[4] = {vec3(-SIZE, -SIZE, SIZE)};
    vertices[5] = {vec3(SIZE, -SIZE, SIZE)};
    vertices[6] = {vec3(SIZE, SIZE, SIZE)};
    vertices[7] = {vec3(-SIZE, SIZE, SIZE)};

    vertexTransfer.unmap(device->device);

    indexTransfer.map(device->device);
    auto indices = static_cast<Index*>(indexTransfer.mapped);

    indices[0] = 0;
    indices[1] = 3;
    indices[2] = 2;
    indices[3] = 2;
    indices[4] = 1;
    indices[5] = 0;

    indices[6] = 4;
    indices[7] = 5;
    indices[8] = 6;
    indices[9] = 6;
    indices[10] = 7;
    indices[11] = 4;

    indices[12] = 3;
    indices[13] = 7;
    indices[14] = 6;
    indices[15] = 6;
    indices[16] = 2;
    indices[17] = 3;

    indices[18] = 4;
    indices[19] = 0;
    indices[20] = 1;
    indices[21] = 1;
    indices[22] = 5;
    indices[23] = 4;

    indices[24] = 4;
    indices[25] = 7;
    indices[26] = 3;
    indices[27] = 3;
    indices[28] = 0;
    indices[29] = 4;

    indices[30] = 1;
    indices[31] = 2;
    indices[32] = 6;
    indices[33] = 6;
    indices[34] = 5;
    indices[35] = 1;

    indexTransfer.unmap(device->device);

    device->createBuffer(USAGE_VERTEX_TRANSFER_DST,
                         MEM_FLAG_GPU_LOCAL,
                         vertexTransfer.size,
                         m_vertices);
    device->createBuffer(USAGE_INDEX_TRANSFER_DST,
                         MEM_FLAG_GPU_LOCAL,
                         indexTransfer.size,
                         m_indices);

    auto cmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    vertexTransfer.transfer(cmd, m_vertices);
    indexTransfer.transfer(cmd, m_indices);

    device->flushCommandBuffer(cmd, queue);

    vertexTransfer.destroy(device->device);
    indexTransfer.destroy(device->device);
}
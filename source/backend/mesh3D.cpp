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

    const auto vertexCount = (N_STACKS - 1) * N_SLICES + 2;
    const auto indexCount = 6 * N_SLICES * (N_STACKS - 1);

    auto bufferInfo = vkInits::bufferCreateInfo(vertexCount * sizeof(MeshVertex), USAGE_VERTEX_TRANSFER_SRC);
    auto vertexTransfer = VulkanBuffer();
    vertexTransfer.create(device, &bufferInfo, MEM_FLAG_HOST_VISIBLE);

    bufferInfo.usage = USAGE_INDEX_TRANSFER_SRC;
    bufferInfo.size = indexCount * sizeof(uint32_t);
    auto indexTransfer = VulkanBuffer();
    indexTransfer.create(device, &bufferInfo, MEM_FLAG_HOST_VISIBLE);

    MeshVertex *vertices = nullptr;
    vkMapMemory(device->device, vertexTransfer.memory, 0, vertexTransfer.size, 0, (void**)(&vertices));
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

    vkUnmapMemory(device->device, vertexTransfer.memory);

    MeshIndex *indices = nullptr;
    vkMapMemory(device->device, indexTransfer.memory, 0, indexTransfer.size, 0, (void**)(&indices));
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
        *(index++) = uint32_t(vertexCount - 1);
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

    vkUnmapMemory(device->device, indexTransfer.memory);

    bufferInfo.usage = USAGE_VERTEX_TRANSFER_DST;
    bufferInfo.size = vertexTransfer.size;
    m_vertices.create(device, &bufferInfo, MEM_FLAG_GPU_LOCAL);

    bufferInfo.usage = USAGE_INDEX_TRANSFER_DST;
    bufferInfo.size = indexTransfer.size;
    m_indices.create(device, &bufferInfo, MEM_FLAG_GPU_LOCAL);

    auto cmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    vertexTransfer.copy(cmd, &m_vertices);
    indexTransfer.copy(cmd, &m_indices);

    device->flushCommandBuffer(cmd, queue);

    vertexTransfer.destroy(device->device);
    indexTransfer.destroy(device->device);

    m_indexCount = indexCount;
}

void Mesh3D::loadCube(const VulkanDevice *device, VkQueue queue)
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
    constexpr size_t INDEX_COUNT = 36;

    auto bufferInfo = vkInits::bufferCreateInfo(VERTEX_COUNT * sizeof(MeshVertex), USAGE_VERTEX_TRANSFER_SRC);
    auto vertexTransfer = VulkanBuffer();
    vertexTransfer.create(device, &bufferInfo, MEM_FLAG_HOST_VISIBLE);

    bufferInfo.usage = USAGE_INDEX_TRANSFER_SRC;
    bufferInfo.size = INDEX_COUNT * sizeof(uint32_t);
    auto indexTransfer = VulkanBuffer();
    indexTransfer.create(device, &bufferInfo, MEM_FLAG_HOST_VISIBLE);

    MeshVertex *vertices = nullptr;
    vkMapMemory(device->device, vertexTransfer.memory, 0, vertexTransfer.size, 0, (void**)(&vertices));

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

    vkUnmapMemory(device->device, vertexTransfer.memory);

    MeshIndex *indices = nullptr;
    vkMapMemory(device->device, indexTransfer.memory, 0, indexTransfer.size, 0, (void**)(&indices));

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

    vkUnmapMemory(device->device, indexTransfer.memory);

    bufferInfo.usage = USAGE_VERTEX_TRANSFER_DST;
    bufferInfo.size = vertexTransfer.size;
    m_vertices.create(device, &bufferInfo, MEM_FLAG_GPU_LOCAL);

    bufferInfo.usage = USAGE_INDEX_TRANSFER_DST;
    bufferInfo.size = indexTransfer.size;
    m_indices.create(device, &bufferInfo, MEM_FLAG_GPU_LOCAL);

    auto cmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    vertexTransfer.copy(cmd, &m_vertices);
    indexTransfer.copy(cmd, &m_indices);

    device->flushCommandBuffer(cmd, queue);

    vertexTransfer.destroy(device->device);
    indexTransfer.destroy(device->device);

    m_indexCount = INDEX_COUNT;
}

void Mesh3D::draw(VkCommandBuffer cmd, VkPipelineLayout layout)
{
    const VkDeviceSize vertexOffset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &m_vertices.data, &vertexOffset);

    const VkDeviceSize indexOffset = 0;
    vkCmdBindIndexBuffer(cmd, m_indices.data, indexOffset, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(cmd, uint32_t(m_indexCount), 1, 0, 0, 0);
}
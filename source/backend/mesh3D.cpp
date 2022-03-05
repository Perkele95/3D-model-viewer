#include "mesh3D.hpp"

void mesh3D::destroy(VkDevice device)
{
    m_vertices.destroy(device);
    m_indices.destroy(device);
}

void mesh3D::load(const vulkan_device *device,
                  VkCommandPool cmdPool,
                  view<mesh_vertex> vertices,
                  view<mesh_index> indices)
{
    auto vertexInfo = vkInits::bufferCreateInfo(vertices.size(), USAGE_VERTEX_TRANSFER_SRC);
    auto vertexTransfer = buffer_t();
    vertexTransfer.create(device, &vertexInfo, MEM_FLAG_HOST_VISIBLE, vertices.data);

    auto indexInfo = vkInits::bufferCreateInfo(indices.size(), USAGE_INDEX_TRANSFER_SRC);
    auto indexTransfer = buffer_t();
    indexTransfer.create(device, &indexInfo, MEM_FLAG_HOST_VISIBLE, indices.data);

    vertexInfo.usage = USAGE_VERTEX_TRANSFER_DST;
    m_vertices.create(device, &vertexInfo, MEM_FLAG_GPU_LOCAL);

    indexInfo.usage = USAGE_INDEX_TRANSFER_DST;
    m_indices.create(device, &indexInfo, MEM_FLAG_GPU_LOCAL);

    auto command = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, cmdPool);

    vertexTransfer.copy(command, &m_vertices);
    indexTransfer.copy(command, &m_indices);

    device->flushCommandBuffer(command, device->graphics.queue, cmdPool);

    vertexTransfer.destroy(device->device);
    indexTransfer.destroy(device->device);
}

void mesh3D::loadSphere(const vulkan_device* device, VkCommandPool cmdPool)
{
    constexpr auto N_STACKS = clamp<uint32_t>(32, 2, 128);
    constexpr auto N_SLICES = clamp<uint32_t>(32, 2, 128);

    auto vertices = dyn_array<mesh_vertex>((N_STACKS - 1) * N_SLICES + 2);
    auto vertex = vertices.data();

    // Top vertex
    vertex->position = vec3(0.0f, 1.0f, 0.0f);
    vertex->normal = vertex->position;
    vertex++;

    // Vertex fill
    for (size_t i = 0; i < N_STACKS - 1; i++){
        const auto phi = PI32 * float(i + 1) / float(N_STACKS);
        for (size_t j = 0; j < N_SLICES; j++){
            const auto theta = 2.0f * PI32 * float(j) / float(N_SLICES);
            vertex->position.x = std::sin(phi) * std::cos(theta);
            vertex->position.y = std::cos(phi);
            vertex->position.z = std::sin(phi) * std::sin(theta);
            vertex->normal = vertex->position;

            vertex->uv.x = (asinf(vertex->normal.x) / PI32) + 0.5f;
            vertex->uv.y = (asinf(vertex->normal.y) / PI32) + 0.5f;

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

    auto indices = dyn_array<mesh_index>(idxCountTriangles + idxCountQuads);
    auto index = indices.data();

    const auto indexLast = uint32_t(vertices.count() - 1);

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

    load(device, cmdPool, vertices, indices);
}

void mesh3D::loadCube(const vulkan_device* device, VkCommandPool cmdPool)
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

    auto vertices = dyn_array<mesh_vertex>(24);
    vertices[0] = {points[0], normalFront};
    vertices[1] = {points[3], normalFront};
    vertices[2] = {points[2], normalFront};
    vertices[3] = {points[1], normalFront};

    vertices[4] = {points[4], normalBack};
    vertices[5] = {points[5], normalBack};
    vertices[6] = {points[6], normalBack};
    vertices[7] = {points[7], normalBack};

    vertices[8] = {points[3], normalTop};
    vertices[9] = {points[7], normalTop};
    vertices[10] = {points[6], normalTop};
    vertices[11] = {points[2], normalTop};

    vertices[12] = {points[4], normalBottom};
    vertices[13] = {points[0], normalBottom};
    vertices[14] = {points[1], normalBottom};
    vertices[15] = {points[5], normalBottom};

    vertices[16] = {points[4], normalRight};
    vertices[17] = {points[7], normalRight};
    vertices[18] = {points[3], normalRight};
    vertices[19] = {points[0], normalRight};

    vertices[20] = {points[1], normalLeft};
    vertices[21] = {points[2], normalLeft};
    vertices[22] = {points[6], normalLeft};
    vertices[23] = {points[5], normalLeft};

    auto indices = dyn_array<mesh_index>(36);
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

    load(device, cmdPool, vertices, indices);
}

void mesh3D::draw(VkCommandBuffer cmd, VkPipelineLayout layout)
{
    const VkDeviceSize vertexOffset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &m_vertices.data, &vertexOffset);

    const VkDeviceSize indexOffset = 0;
    vkCmdBindIndexBuffer(cmd, m_indices.data, indexOffset, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(cmd, uint32_t(m_indexCount), 1, 0, 0, 0);
}
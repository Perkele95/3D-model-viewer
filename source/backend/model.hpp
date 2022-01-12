#pragma once

#include "vulkan_initialisers.hpp"
#include "vulkan_device.hpp"
#include "material.hpp"

struct mesh_vertex
{
    static constexpr VkVertexInputAttributeDescription positionAttribute()
    {
        return {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(mesh_vertex, position)};
    }

    static constexpr VkVertexInputAttributeDescription normalAttribute()
    {
        return {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(mesh_vertex, normal)};
    }

    vec3<float> position;
    vec3<float> normal;
};
using mesh_index = uint32_t;

struct model3D
{
    model3D() = default;
    model3D(material3D material)
    {
        m_indexCount = 0;
        m_material = material;
    }

    void load(const vulkan_device *device, VkCommandPool cmdPool,
              view<mesh_vertex> vertices, view<mesh_index> indices)
    {
        auto vertexTransfer = buffer_t(sizeof(mesh_vertex) * vertices.count);
        auto indexTransfer = buffer_t(sizeof(mesh_index) * indices.count);
        m_indexCount = indices.count;

        vertexTransfer.create(device, USAGE_VERTEX_TRANSFER_SRC, MEM_FLAG_HOST_VISIBLE);
        indexTransfer.create(device, USAGE_INDEX_TRANSFER_SRC, MEM_FLAG_HOST_VISIBLE);

        vertexTransfer.fill(device->device, vertices.data, vertexTransfer.size);
        indexTransfer.fill(device->device, indices.data, indexTransfer.size);

        m_vertices.size = vertexTransfer.size;
        m_indices.size = indexTransfer.size;
        m_vertices.create(device, USAGE_VERTEX_TRANSFER_DST, MEM_FLAG_GPU_LOCAL);
        m_indices.create(device, USAGE_INDEX_TRANSFER_DST, MEM_FLAG_GPU_LOCAL);

        VkCommandBuffer copyCmds[] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
        auto cmdInfo = vkInits::commandBufferAllocateInfo(cmdPool, arraysize(copyCmds));
        vkAllocateCommandBuffers(device->device, &cmdInfo, copyCmds);

        vertexTransfer.copyToBuffer(copyCmds[0], &m_vertices);
        indexTransfer.copyToBuffer(copyCmds[1], &m_indices);

        auto submitInfo = vkInits::submitInfo(copyCmds, arraysize(copyCmds));
        vkQueueSubmit(device->graphics.queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(device->graphics.queue);

        vkFreeCommandBuffers(device->device, cmdPool, uint32_t(arraysize(copyCmds)), copyCmds);

        vertexTransfer.destroy(device->device);
        indexTransfer.destroy(device->device);
    }

    void destroy(VkDevice device)
    {
        m_vertices.destroy(device);
        m_indices.destroy(device);
    }

    void draw(VkCommandBuffer cmd, VkPipelineLayout layout)
    {
        m_material.bind(cmd, layout);

        const VkDeviceSize vertexOffset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &m_vertices.data, &vertexOffset);

        const VkDeviceSize indexOffset = 0;
        vkCmdBindIndexBuffer(cmd, m_indices.data, indexOffset, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(cmd, uint32_t(m_indexCount), 1, 0, 0, 0);
    }

private:
    buffer_t m_vertices;
    buffer_t m_indices;
    size_t m_indexCount;
    material3D m_material;
};

static constexpr auto s_MeshSzf = 0.5f;
constexpr auto TINT_BRONZE = GetColour(0xb08d57FF);
constexpr auto TINT_GOLD = GetColour(0xFFD700FF);

static constexpr auto s_NormalFront = vec3(0.0f, 0.0f, -1.0f);
static constexpr auto s_NormalBack = vec3(0.0f, 0.0f, 1.0f);
static constexpr auto s_NormalTop = vec3(0.0f, 1.0f, 0.0f);
static constexpr auto s_NormalBottom = vec3(0.0f, -1.0f, 0.0f);
static constexpr auto s_NormalLeft = vec3(1.0f, 0.0f, 0.0f);
static constexpr auto s_NormalRight = vec3(-1.0f, 0.0f, 0.0f);

static mesh_vertex s_MeshVertices[] = {
    // Front
    {vec3(-s_MeshSzf, -s_MeshSzf, -s_MeshSzf), s_NormalFront},
    {vec3(s_MeshSzf, -s_MeshSzf, -s_MeshSzf), s_NormalFront},
    {vec3(s_MeshSzf, s_MeshSzf, -s_MeshSzf), s_NormalFront},
    {vec3(-s_MeshSzf, s_MeshSzf, -s_MeshSzf), s_NormalFront},
    // Back
    {vec3(-s_MeshSzf, -s_MeshSzf, s_MeshSzf), s_NormalBack},
    {vec3(s_MeshSzf, -s_MeshSzf, s_MeshSzf), s_NormalBack},
    {vec3(s_MeshSzf, s_MeshSzf, s_MeshSzf), s_NormalBack},
    {vec3(-s_MeshSzf, s_MeshSzf, s_MeshSzf), s_NormalBack},
    // Top
    {vec3(-s_MeshSzf, s_MeshSzf, -s_MeshSzf), s_NormalTop},
    {vec3(s_MeshSzf, s_MeshSzf, -s_MeshSzf), s_NormalTop},
    {vec3(s_MeshSzf, s_MeshSzf, s_MeshSzf), s_NormalTop},
    {vec3(-s_MeshSzf, s_MeshSzf, s_MeshSzf), s_NormalTop},
    // Bottom
    {vec3(-s_MeshSzf, -s_MeshSzf, s_MeshSzf), s_NormalBottom},
    {vec3(s_MeshSzf, -s_MeshSzf, s_MeshSzf), s_NormalBottom},
    {vec3(s_MeshSzf, -s_MeshSzf, -s_MeshSzf), s_NormalBottom},
    {vec3(-s_MeshSzf, -s_MeshSzf, -s_MeshSzf), s_NormalBottom},
    // Left
    {vec3(-s_MeshSzf, -s_MeshSzf, s_MeshSzf), s_NormalLeft},
    {vec3(-s_MeshSzf, -s_MeshSzf, -s_MeshSzf), s_NormalLeft},
    {vec3(-s_MeshSzf, s_MeshSzf, -s_MeshSzf), s_NormalLeft},
    {vec3(-s_MeshSzf, s_MeshSzf, s_MeshSzf), s_NormalLeft},
    // Right
    {vec3(s_MeshSzf, -s_MeshSzf, -s_MeshSzf), s_NormalRight},
    {vec3(s_MeshSzf, -s_MeshSzf, s_MeshSzf), s_NormalRight},
    {vec3(s_MeshSzf, s_MeshSzf, s_MeshSzf), s_NormalRight},
    {vec3(s_MeshSzf, s_MeshSzf, -s_MeshSzf), s_NormalRight},
};

static mesh_index s_MeshIndices[] = {
    0, 1, 2, 2, 3, 0, // Front
    4, 7, 6, 6, 5, 4, // Back
    8, 9, 10, 10, 11, 8, // Top
    12, 13, 14, 14, 15, 12, // Bottom
    16, 17, 18, 18, 19, 16, // Left
    20, 21, 22, 22, 23, 20, // Right
};

template<int N_STACKS = 32, int N_SLICES = 32>
model3D UVSphere(const vulkan_device *device, VkCommandPool cmdPool)
{
    auto model = model3D(MATERIAL_TEST);
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
            vertex->position.x = sinf(phi) * cosf(theta);
            vertex->position.y = cosf(phi);
            vertex->position.z = sinf(phi) * sinf(theta);
            vertex->normal = vertex->position;
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
    //TODO(arle): check if N - 2 is 0 or less

    auto indices = dyn_array<mesh_index>(idxCountTriangles + idxCountQuads);
    auto index = indices.data();

    const auto indexLast = uint32_t(vertices.count() - 1);

    // Top and bottom triangles
    for (uint32_t i = 0; i < N_SLICES; i++){
        auto i0 = i + 1;
        auto i1 = i0 % N_SLICES + 1;
        *index = 0;
        index++;
        *index = i1;
        index++;
        *index = i0;
        index++;

        i0 += N_SLICES * (N_STACKS - 2);
        i1 += N_SLICES * (N_STACKS - 2);
        *index = indexLast;
        index++;
        *index = i0;
        index++;
        *index = i1;
        index++;
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

            *index = j0;
            index++;
            *index = j1;
            index++;
            *index = j2;
            index++;
            *index = j0;
            index++;
            *index = j2;
            index++;
            *index = j3;
            index++;
        }
    }

    model.load(device, cmdPool, vertices, indices);

    return model;
}
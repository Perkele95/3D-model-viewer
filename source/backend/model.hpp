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

    void load(const vulkan_device *device, view<mesh_vertex> vertices,
                                           view<mesh_index> indices)
    {
        //
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
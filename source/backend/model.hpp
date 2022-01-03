#pragma once

#include "vulkan_initialisers.hpp"

struct material3D
{
    material3D() = default;
    constexpr material3D(vec3<float> rgb, float r, float m, float ao)
    : albedo(rgb), roughness(r), metallic(r), ambientOcclusion(ao) {}

    static VkPushConstantRange pushConstant()
    {
        VkPushConstantRange range;
        range.offset = 0;
        range.size = sizeof(material3D);
        range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        return range;
    }

    void bind(VkCommandBuffer commandBuffer, VkPipelineLayout layout) const
    {
        const auto pc = pushConstant();
        vkCmdPushConstants(commandBuffer,
                           layout,
                           pc.stageFlags,
                           pc.offset,
                           pc.size,
                           this);
    }

    vec3<float> albedo;
    float roughness;
    float metallic;
    float ambientOcclusion;
};

static_assert(sizeof(material3D) == 24 && alignof(material3D) == 4);

constexpr auto MATERIAL_TEST = material3D(
    vec3(1.0f, 0.0f, 0.0f), 0.1f, 1.0f, 0.5f
);

struct mesh3D
{
    struct vertex
    {
        vec3<float> position;
        vec3<float> normal;
    };
    using index = uint32_t;

    mesh3D() = default;

    static constexpr VkVertexInputAttributeDescription positionAttribute()
    {
        return {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(mesh3D::vertex, position)};
    }

    static constexpr VkVertexInputAttributeDescription normalAttribute()
    {
        return {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(mesh3D::vertex, normal)};
    }

    void load(view<vertex> vertices, view<index> indices)
    {
        //
    }

    void unload()
    {
        //
    }

    void bind()
    {
        //
    }

private:
    buffer_t m_vertices, m_indices;
};

struct model3D
{
    model3D() = default;
    model3D(const vulkan_device *device)
    {
        m_device = device;
    }

    void draw(VkCommandBuffer cmd, VkDescriptorSet set)
    {
        //
    }

private:
    mesh3D m_mesh;
    material3D m_material;
    const vulkan_device *m_device;
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

static mesh3D::vertex s_MeshVertices[] = {
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

static mesh3D::index s_MeshIndices[] = {
    0, 1, 2, 2, 3, 0, // Front
    4, 7, 6, 6, 5, 4, // Back
    8, 9, 10, 10, 11, 8, // Top
    12, 13, 14, 14, 15, 12, // Bottom
    16, 17, 18, 18, 19, 16, // Left
    20, 21, 22, 22, 23, 20, // Right
};
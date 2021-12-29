#pragma once

#include "vulkan_initialisers.hpp"

struct alignas(16) material3D
{
    material3D() = default;
    constexpr material3D(vec3<float> amb, vec3<float> diff, vec3<float> spec, float shine)
    : ambient(amb, 1.0f), diffuse(diff, 1.0f), specular(spec), shininess(shine) {}

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

    vec4<float> ambient;
    vec4<float> diffuse;
    vec3<float> specular;
    float shininess;
};

constexpr auto MATERIAL_BRONZE = material3D(
    vec3(0.2125f, 0.1275f, 0.054f),
    vec3(0.714f, 0.4284f, 0.18144f),
    vec3(0.393548f, 0.271906f, 0.166721f),
    0.2f
);

struct mesh3D
{
    struct vertex
    {
        vec3<float> position;
        vec3<float> normal;
        vec4<float> colour;
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

    static constexpr VkVertexInputAttributeDescription colourAttribute()
    {
        return {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(mesh3D::vertex, colour)};
    }

    view<vertex> vertices;
    view<index> indices;
};

struct model3D
{
    model3D() = default;

    mesh3D mesh;
    material3D material;
};

static constexpr auto s_MeshSzf = 0.5f;
constexpr auto CUBE_TINT = GetColour(0xb08d57FF);

static constexpr auto s_NormalFront = vec3(0.0f, 0.0f, -1.0f);
static constexpr auto s_NormalBack = vec3(0.0f, 0.0f, 1.0f);
static constexpr auto s_NormalTop = vec3(0.0f, 1.0f, 0.0f);
static constexpr auto s_NormalBottom = vec3(0.0f, -1.0f, 0.0f);
static constexpr auto s_NormalLeft = vec3(1.0f, 0.0f, 0.0f);
static constexpr auto s_NormalRight = vec3(-1.0f, 0.0f, 0.0f);

static mesh3D::vertex s_MeshVertices[] = {
    // Front
    {vec3(-s_MeshSzf, -s_MeshSzf, -s_MeshSzf), s_NormalFront, CUBE_TINT},
    {vec3(s_MeshSzf, -s_MeshSzf, -s_MeshSzf), s_NormalFront, CUBE_TINT},
    {vec3(s_MeshSzf, s_MeshSzf, -s_MeshSzf), s_NormalFront, CUBE_TINT},
    {vec3(-s_MeshSzf, s_MeshSzf, -s_MeshSzf), s_NormalFront, CUBE_TINT},
    // Back
    {vec3(-s_MeshSzf, -s_MeshSzf, s_MeshSzf), s_NormalBack, CUBE_TINT},
    {vec3(s_MeshSzf, -s_MeshSzf, s_MeshSzf), s_NormalBack, CUBE_TINT},
    {vec3(s_MeshSzf, s_MeshSzf, s_MeshSzf), s_NormalBack, CUBE_TINT},
    {vec3(-s_MeshSzf, s_MeshSzf, s_MeshSzf), s_NormalBack, CUBE_TINT},
    // Top
    {vec3(-s_MeshSzf, s_MeshSzf, -s_MeshSzf), s_NormalTop, CUBE_TINT},
    {vec3(s_MeshSzf, s_MeshSzf, -s_MeshSzf), s_NormalTop, CUBE_TINT},
    {vec3(s_MeshSzf, s_MeshSzf, s_MeshSzf), s_NormalTop, CUBE_TINT},
    {vec3(-s_MeshSzf, s_MeshSzf, s_MeshSzf), s_NormalTop, CUBE_TINT},
    // Bottom
    {vec3(-s_MeshSzf, -s_MeshSzf, s_MeshSzf), s_NormalBottom, CUBE_TINT},
    {vec3(s_MeshSzf, -s_MeshSzf, s_MeshSzf), s_NormalBottom, CUBE_TINT},
    {vec3(s_MeshSzf, -s_MeshSzf, -s_MeshSzf), s_NormalBottom, CUBE_TINT},
    {vec3(-s_MeshSzf, -s_MeshSzf, -s_MeshSzf), s_NormalBottom, CUBE_TINT},
    // Left
    {vec3(-s_MeshSzf, -s_MeshSzf, s_MeshSzf), s_NormalLeft ,CUBE_TINT},
    {vec3(-s_MeshSzf, -s_MeshSzf, -s_MeshSzf), s_NormalLeft, CUBE_TINT},
    {vec3(-s_MeshSzf, s_MeshSzf, -s_MeshSzf), s_NormalLeft, CUBE_TINT},
    {vec3(-s_MeshSzf, s_MeshSzf, s_MeshSzf), s_NormalLeft, CUBE_TINT},
    // Right
    {vec3(s_MeshSzf, -s_MeshSzf, -s_MeshSzf), s_NormalRight, CUBE_TINT},
    {vec3(s_MeshSzf, -s_MeshSzf, s_MeshSzf), s_NormalRight, CUBE_TINT},
    {vec3(s_MeshSzf, s_MeshSzf, s_MeshSzf), s_NormalRight, CUBE_TINT},
    {vec3(s_MeshSzf, s_MeshSzf, -s_MeshSzf), s_NormalRight, CUBE_TINT},
};

static mesh3D::index s_MeshIndices[] = {
    0, 1, 2, 2, 3, 0, // Front
    4, 7, 6, 6, 5, 4, // Back
    8, 9, 10, 10, 11, 8, // Top
    12, 13, 14, 14, 15, 12, // Bottom
    16, 17, 18, 18, 19, 16, // Left
    20, 21, 22, 22, 23, 20, // Right
};
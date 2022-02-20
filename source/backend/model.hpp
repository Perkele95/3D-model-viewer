#pragma once

#include "vulkan_initialisers.hpp"
#include "vulkan_device.hpp"
#include "material.hpp"
#include "buffer.hpp"

struct alignas(16) transform3D
{
    constexpr transform3D();

    static VkPushConstantRange pushConstant();

    void bind(VkCommandBuffer commandBuffer, VkPipelineLayout layout) const;

    mat4x4 matrix;
};

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

class model3D
{
public:
    model3D() = default;
    model3D(material3D material);

    void load(const vulkan_device *device, VkCommandPool cmdPool,
              view<mesh_vertex> vertices, view<mesh_index> indices);

    void destroy(VkDevice device);

    void draw(VkCommandBuffer cmd, VkPipelineLayout layout);

    void UVSphere(const vulkan_device *device, VkCommandPool cmdPool);
    void Box(const vulkan_device *device, VkCommandPool cmdPool);

    transform3D transform;

private:
    buffer_t m_vertices;
    buffer_t m_indices;
    size_t m_indexCount;
    material3D m_material;
};

constexpr auto TINT_BRONZE = GetColour(0xb08d57FF);
constexpr auto TINT_GOLD = GetColour(0xFFD700FF);
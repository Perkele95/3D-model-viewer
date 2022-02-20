#pragma once

#include "vulkan_initialisers.hpp"
#include "vulkan_device.hpp"
#include "texture2D.hpp"
#include "buffer.hpp"

struct alignas(16) transform3D
{
    constexpr transform3D();

    static VkPushConstantRange transform3D::pushConstant();

    void bind(VkCommandBuffer commandBuffer, VkPipelineLayout layout) const;

    mat4x4 matrix;
};

struct mesh_vertex
{
    vec3<float> position;
    vec3<float> normal;
    vec2<float> uv;
};

static constexpr VkVertexInputAttributeDescription MeshAttributes[] = {
    {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(mesh_vertex, position)},
    {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(mesh_vertex, normal)},
    {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(mesh_vertex, uv)}
};

using mesh_index = uint32_t;

enum class texture_type
{
    albedo,
    normal,
    rougness,
    metallic,
    ambient,
};

struct texture_load_info
{
    const void *source;
    VkExtent2D extent;
};

class model3D
{
public:
    model3D() = default;
    model3D(const vulkan_device *device);
    void destroy();

    VkDescriptorImageInfo descriptor(texture_type type){return m_textures[size_t(type)].descriptor();}

    void load(VkCommandBuffer cmd, view<mesh_vertex> vertices);
    void load(VkCommandBuffer cmd, view<mesh_index> indices);
    void load(VkCommandPool cmdPool, texture_load_info (&textureInfos)[5]);

    void draw(VkCommandBuffer cmd, VkPipelineLayout layout);

    void UVSphere(const vulkan_device *device, VkCommandPool cmdPool);
    void Box(const vulkan_device *device, VkCommandPool cmdPool);

    transform3D transform;

private:
    const vulkan_device *m_device;

    buffer_t m_vertices;
    buffer_t m_indices;
    size_t m_indexCount;

    texture2D m_textures[5];
};

constexpr auto TINT_BRONZE = GetColour(0xb08d57FF);
constexpr auto TINT_GOLD = GetColour(0xFFD700FF);
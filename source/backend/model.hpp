#pragma once

#include "vulkan_initialisers.hpp"
#include "vulkan_device.hpp"
#include "buffer.hpp"
#include "texture2D.hpp"

struct alignas(16) transform3D
{
    constexpr transform3D();

    static VkPushConstantRange pushConstant();

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

enum class pbr_material
{
    albedo,
    normal,
    rougness,
    metallic,
    ambient
};

class model3D
{
public:
    model3D() = default;
    model3D(const vulkan_device *device);
    void destroy();

    VkDescriptorImageInfo descriptor(pbr_material material){return m_textures[size_t(material)].getDescriptor();}

    void load(VkCommandPool cmdPool, view<mesh_vertex> vertices, view<mesh_index> indices);

    void load(pbr_material material, VkCommandPool cmdPool, VkExtent2D extent, const void* src)
    {
        const auto index = static_cast<size_t>(material);
        m_textures[index].load(m_device, cmdPool, VK_FORMAT_R8G8B8A8_SRGB, extent, src);
    }

    void draw(VkCommandBuffer cmd, VkPipelineLayout layout);

    void UVSphere(VkCommandPool cmdPool);
    void Box(VkCommandPool cmdPool);

    transform3D transform;

private:
    const vulkan_device*    m_device;
    buffer_t                m_vertices;
    buffer_t                m_indices;
    size_t                  m_indexCount;
    texture2D               m_textures[5];
};
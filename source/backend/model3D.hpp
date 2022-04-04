#pragma once

#include "vulkan_initialisers.hpp"
#include "VulkanDevice.hpp"
#include "buffer.hpp"
#include "texture.hpp"
#include "mesh3D.hpp"

class PBRModel
{
public:
    static VkPushConstantRange pushConstant();

    void destroy(VkDevice device);
    void draw(VkCommandBuffer cmd, VkPipelineLayout layout);

    mat4x4 transform;
    Mesh3D mesh;

    // Material

    Texture2D albedo;
    Texture2D normal;
    Texture2D roughness;
    Texture2D metallic;
    Texture2D ao;
};

struct CubeMapVertex
{
    vec3<float> position;
};

class CubeMapModel
{
public:
    static constexpr VkVertexInputAttributeDescription Attributes[] = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(CubeMapVertex, position)}
    };

    void load(const VulkanDevice *device, VkQueue queue);
    void destroy(VkDevice device);
    void draw(VkCommandBuffer cmd);

    TextureCubeMap  map;
private:
    VulkanBuffer    m_vertices;
    VulkanBuffer    m_indices;
    uint32_t        m_indexCount;
};
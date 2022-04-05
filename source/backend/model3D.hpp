#pragma once

#include "vulkan_initialisers.hpp"
#include "VulkanDevice.hpp"
#include "buffer.hpp"
#include "VulkanTexture.hpp"
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
    static constexpr float SIZE = 10.0f;
    static constexpr uint32_t VERTEX_COUNT = 8;
    static constexpr uint32_t INDEX_COUNT = 36;

    VulkanBuffer    m_vertices;
    VulkanBuffer    m_indices;
};
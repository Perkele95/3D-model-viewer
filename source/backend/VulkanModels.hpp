#pragma once

#include "VulkanDevice.hpp"

class ModelBase
{
public:
    using Index = uint32_t;

    void draw(VkCommandBuffer cmd);
    void destroy(VkDevice device);

protected:
    VulkanBuffer    m_vertices;
    VulkanBuffer    m_indices;
    uint32_t        m_indexCount;
};

class Model3D : public ModelBase
{
public:
    struct Vertex
    {
        vec3<float> position;
        vec3<float> normal;
        vec2<float> uv;
    };

    static constexpr VkVertexInputAttributeDescription Attributes[] = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)},
        {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)}
    };

    static VkPushConstantRange pushConstant();
    void loadSpherePrimitive(const VulkanDevice* device, VkQueue queue);
    void loadCubePrimitive(const VulkanDevice* device, VkQueue queue);

    mat4x4 transform;
};

class CubemapModel : public ModelBase
{
public:
    struct Vertex
    {
        vec3<float> position;
    };

    static constexpr VkVertexInputAttributeDescription Attributes[] = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)}
    };

    void load(const VulkanDevice *device, VkQueue queue);
};
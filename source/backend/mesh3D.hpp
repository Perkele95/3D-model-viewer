#include "vulkan_initialisers.hpp"
#include "VulkanDevice.hpp"
#include "buffer.hpp"

struct MeshVertex
{
    vec3<float> position;
    vec3<float> normal;
    vec2<float> uv;
};

using MeshIndex = uint32_t;

class Mesh3D
{
public:
    static constexpr VkVertexInputAttributeDescription Attributes[] = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(MeshVertex, position)},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(MeshVertex, normal)},
        {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(MeshVertex, uv)}
    };

    void destroy(VkDevice device);

    void load(const VulkanDevice *device,
              VkQueue queue,
              view<MeshVertex> vertices,
              view<MeshIndex> indices);

    void loadSphere(const VulkanDevice *device, VkQueue queue);
    void loadCube(const VulkanDevice *device, VkQueue queue);

    void draw(VkCommandBuffer cmd, VkPipelineLayout layout);

private:
    VulkanBuffer    m_vertices;
    VulkanBuffer    m_indices;
    size_t          m_indexCount;
};
#include "vulkan_initialisers.hpp"
#include "vulkan_device.hpp"
#include "buffer.hpp"

struct mesh_vertex
{
    vec3<float> position;
    vec3<float> normal;
    vec2<float> uv;
};

using mesh_index = uint32_t;

class mesh3D
{
public:
    static constexpr VkVertexInputAttributeDescription Attributes[] = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(mesh_vertex, position)},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(mesh_vertex, normal)},
        {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(mesh_vertex, uv)}
    };

    mesh3D() : m_vertices(), m_indices(), m_indexCount(0){}

    void destroy(VkDevice device);

    void load(const vulkan_device *device,
              VkCommandPool cmdPool,
              view<mesh_vertex> vertices,
              view<mesh_index> indices);

    void loadSphere(const vulkan_device* device, VkCommandPool cmdPool);
    void loadCube(const vulkan_device* device, VkCommandPool cmdPool);

    void draw(VkCommandBuffer cmd, VkPipelineLayout layout);

private:
    buffer_t    m_vertices;
    buffer_t    m_indices;
    size_t      m_indexCount;
};
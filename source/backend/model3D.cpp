#include "model3D.hpp"

void model3D::destroy()
{
    if (m_mesh != nullptr) {
        m_mesh->destroy(m_device->device);
        m_mesh = nullptr;
    }
    if (m_material != nullptr) {
        m_material->destroy(m_device->device);
        m_material = nullptr;
    }
}

void model3D::draw(VkCommandBuffer cmd, VkPipelineLayout layout)
{
    const auto pc = pushConstant();
    vkCmdPushConstants(cmd,
                       layout,
                       pc.stageFlags,
                       pc.offset,
                       pc.size,
                       &transform);

    m_mesh->draw(cmd, layout);
}

VkPushConstantRange model3D::pushConstant()
{
    VkPushConstantRange range{};
    range.offset = 0;
    range.size = sizeof(transform);
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    return range;
}

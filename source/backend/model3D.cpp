#include "model3D.hpp"

void model3D::destroy()
{
    if (mesh != nullptr) {
        mesh->destroy(m_device->device);
        mesh = nullptr;
    }
    if (material != nullptr) {
        material->destroy(m_device->device);
        material = nullptr;
    }
}

void model3D::draw(VkCommandBuffer cmd, VkPipelineLayout layout)
{
    const auto [stageFlags, offset, size] = pushConstant();
    vkCmdPushConstants(cmd,
                       layout,
                       stageFlags,
                       offset,
                       size,
                       &transform);

    mesh->draw(cmd, layout);
}

VkPushConstantRange model3D::pushConstant()
{
    VkPushConstantRange range{};
    range.offset = 0;
    range.size = sizeof(transform);
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    return range;
}

#include "model3D.hpp"

VkPushConstantRange Model3D::pushConstant()
{
    VkPushConstantRange range{};
    range.offset = 0;
    range.size = sizeof(transform);
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    return range;
}

void Model3D::destroy(VkDevice device)
{
    mesh.destroy(device);
    albedo.destroy(device);
    normal.destroy(device);
    roughness.destroy(device);
    metallic.destroy(device);
    ao.destroy(device);
}

void Model3D::draw(VkCommandBuffer cmd, VkPipelineLayout layout)
{
    const auto [stageFlags, offset, size] = pushConstant();
    vkCmdPushConstants(cmd,
                       layout,
                       stageFlags,
                       offset,
                       size,
                       &transform);

    mesh.draw(cmd, layout);
}
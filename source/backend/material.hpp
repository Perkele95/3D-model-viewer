#pragma once

#include "vulkan_initialisers.hpp"

struct alignas(4) material3D
{
    constexpr material3D()
    : m_albedo(vec3(1.0f)), m_roughness(0.5f), m_metallic(0.5f), m_ao(1.0f)
    {
    }

    constexpr material3D(vec3<float> albedo, float roughness, float metallic, float ao)
    : m_albedo(albedo), m_roughness(roughness), m_metallic(metallic), m_ao(ao)
    {
    }

    static VkPushConstantRange pushConstant()
    {
        VkPushConstantRange range;
        range.offset = 0;
        range.size = sizeof(material3D);
        range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        return range;
    }

    void bind(VkCommandBuffer commandBuffer, VkPipelineLayout layout) const
    {
        const auto pc = pushConstant();
        vkCmdPushConstants(commandBuffer,
                           layout,
                           pc.stageFlags,
                           pc.offset,
                           pc.size,
                           this);
    }

private:
    vec3<float> m_albedo;
    float m_roughness;
    float m_metallic;
    float m_ao;
};

constexpr auto MATR_COLOUR = GetColour(0xeeeeeeff);

constexpr auto MATERIAL_TEST = material3D(
    vec3(MATR_COLOUR.x, MATR_COLOUR.y, MATR_COLOUR.z), 0.2f, 0.8f, 0.02f
);
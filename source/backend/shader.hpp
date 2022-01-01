#pragma once

#include "../base.hpp"
#include "vulkan_initialisers.hpp"

struct shader_object
{
    shader_object() = default;
    shader_object(const char *path, VkShaderStageFlagBits stageFlag)
    {
        m_stage = stageFlag;
        m_path = path;
    }

    VkResult load(VkDevice device)
    {
        auto src = Platform::io::read(m_path);
        const auto loadInfo = vkInits::shaderModuleCreateInfo(&src);
        const auto result = vkCreateShaderModule(device, &loadInfo, nullptr, &m_module);
        Platform::io::close(&src);
        return result;
    }

    void destroy(VkDevice device)
    {
        vkDestroyShaderModule(device, m_module, nullptr);
    }

    VkPipelineShaderStageCreateInfo shaderStage()
    {
        auto pipelineStage = vkInits::shaderStageInfo(m_stage, m_module);
        return pipelineStage;
    }

private:
    VkShaderStageFlagBits m_stage;
    VkShaderModule m_module;
    const char *m_path;
};
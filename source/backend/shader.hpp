#pragma once

#include "../base.hpp"
#include "vulkan_initialisers.hpp"

class shader_object
{
public:
    shader_object() = default;
    shader_object(plt::file_path path, VkShaderStageFlagBits stageFlag)
    {
        m_stage = stageFlag;
        m_path = path;
    }

    VkResult load(VkDevice device)
    {
        auto src = plt::filesystem::read(m_path);
        const auto loadInfo = vkInits::shaderModuleCreateInfo(&src);
        const auto result = vkCreateShaderModule(device, &loadInfo, nullptr, &m_module);
        plt::filesystem::close(src);
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
    plt::file_path m_path;
};
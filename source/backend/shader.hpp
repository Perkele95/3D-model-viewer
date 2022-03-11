#pragma once

#include "../base.hpp"
#include "vulkan_initialisers.hpp"

class shader_object
{
public:
    shader_object() = default;
    shader_object(const char *path, VkShaderStageFlagBits stageFlag)
    {
        m_module = VK_NULL_HANDLE;
        m_stage = stageFlag;
        m_path = path;
    }

    bool load(VkDevice device)
    {
        auto file = io::Open<io::cmd::read>(m_path);

        if(io::IsValid(file) == false)
            return false;

        auto map = io::Map(file);

        const auto loadInfo = vkInits::shaderModuleCreateInfo(map.data, map.size);
        vkCreateShaderModule(device, &loadInfo, nullptr, &m_module);

        io::Unmap(map);
        io::Close(file);
        return true;
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
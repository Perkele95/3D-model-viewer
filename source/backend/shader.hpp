#pragma once

#include "../base.hpp"
#include "vulkan_initialisers.hpp"

class Shader
{
public:
    void destroy(VkDevice device)
    {
        vkDestroyShaderModule(device, m_module, nullptr);
    }

    VkPipelineShaderStageCreateInfo shaderStage()
    {
        auto pipelineStage = vkInits::shaderStageInfo(m_stage, m_module);
        return pipelineStage;
    }

protected:
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

    VkShaderStageFlagBits   m_stage;
    VkShaderModule          m_module;
    const char*             m_path;
};

class VertexShader : public Shader
{
public:
    void load(VkDevice device, const char *path)
    {
        m_module = VK_NULL_HANDLE;
        m_stage = VK_SHADER_STAGE_VERTEX_BIT;
        m_path = path;
        Shader::load(device);
    }
};

class FragmentShader : public Shader
{
public:
    void load(VkDevice device, const char *path)
    {
        m_module = VK_NULL_HANDLE;
        m_stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        m_path = path;
        Shader::load(device);
    }
};
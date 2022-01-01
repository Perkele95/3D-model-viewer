#pragma once

#include "vulkan_initialisers.hpp"

constexpr size_t LIGHTS_COUNT = 4;

struct alignas(16) light_data
{
    static VkDescriptorPoolSize poolSize(size_t count)
    {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = uint32_t(count);
        return poolSize;
    }

    static VkDescriptorSetLayoutBinding binding()
    {
        VkDescriptorSetLayoutBinding setBinding{};
        setBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        setBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        setBinding.descriptorCount = uint32_t(1);
        setBinding.binding = 1;
        return setBinding;
    }

    static VkWriteDescriptorSet descriptorWrite(VkDescriptorSet dstSet, const VkDescriptorBufferInfo *pInfo)
    {
        VkWriteDescriptorSet set{};
        set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        set.dstBinding = 1;
        set.descriptorCount = uint32_t(1);
        set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        set.dstSet = dstSet;
        set.pBufferInfo = pInfo;
        return set;
    }

    static VkBufferUsageFlags usageFlags()
    {
        return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }

    static VkMemoryPropertyFlags bufferMemFlags()
    {
        return VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    }

    vec4<float> positions[LIGHTS_COUNT];
    vec4<float> colours[LIGHTS_COUNT];
};

struct light_object
{
    vec3<float> position;
    vec4<float> colour;
};
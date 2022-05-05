#pragma once

#include "vulkan_initialisers.hpp"

class VulkanBuffer
{
public:
    VkResult bind(VkDevice device)
    {
        return vkBindBufferMemory(device, data, memory, 0);
    }

    void updateDescriptor()
    {
        descriptor.buffer = data;
        descriptor.offset = 0;
        descriptor.range = size;
    }

    void destroy(VkDevice device)
    {
        vkDestroyBuffer(device, data, nullptr);
        vkFreeMemory(device, memory, nullptr);
    }

    VkResult map(VkDevice device)
    {
        return vkMapMemory(device, memory, 0, size, 0, &mapped);
    }

    VkResult map(VkDevice device, VkDeviceSize offset, VkDeviceSize range)
    {
        return vkMapMemory(device, memory, offset, range, 0, &mapped);
    }

    void unmap(VkDevice device)
    {
        vkUnmapMemory(device, memory);
        mapped = nullptr;
    }

    void transfer(VkCommandBuffer command, VulkanBuffer &dst)
    {
        const auto copyRegion = vkInits::bufferCopy(size);
        vkCmdCopyBuffer(command, data, dst.data, 1, &copyRegion);
    }

    VkDeviceSize            size;
    VkBuffer                data;
    VkDeviceMemory          memory;
    VkDescriptorBufferInfo  descriptor;
    void*                   mapped;
};
#pragma once

#include "vulkan_initialisers.hpp"
#include "VulkanDevice.hpp"

class VulkanBuffer
{
public:
    void create(const VulkanDevice *device,
                const VkBufferCreateInfo *pCreateInfo,
                VkMemoryPropertyFlags memFlags,
                const void *src = nullptr);

    void destroy(VkDevice device);

    VkDescriptorBufferInfo descriptor(VkDeviceSize offset = 0);
    void copy(VkCommandBuffer cmd, VulkanBuffer *pDst);
    void fill(VkDevice device, const void *src);

    VkDeviceSize    size;
    VkBuffer        data;
    VkDeviceMemory  memory;
};
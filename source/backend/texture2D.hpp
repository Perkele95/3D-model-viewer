#pragma once

#include "vulkan_initialisers.hpp"
#include "buffer.hpp"

class texture2D
{
public:
    texture2D() = default;
    texture2D(const vulkan_device *device, const VkImageCreateInfo *pCreateInfo,
              VkImageAspectFlags aspect);

    void load(const vulkan_device *device, VkCommandPool cmdPool, const void *src);
    void destroy(VkDevice device);

    static VkDescriptorType descriptorType(){return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;}
    VkDescriptorImageInfo descriptor();

    VkImageView view(){return m_view;}

private:
    VkImage m_image;
    VkImageView m_view;
    VkDeviceMemory m_memory;
    VkExtent2D m_extent;
    VkSampler m_sampler;
};
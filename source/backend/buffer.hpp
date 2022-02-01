#pragma once

#include "vulkan_initialisers.hpp"
#include "vulkan_device.hpp"

class buffer_t;

class image_buffer
{
public:
    image_buffer() = default;
    image_buffer(const vulkan_device *device, const VkImageCreateInfo *pCreateInfo,
                 VkImageAspectFlags aspect);

    void destroy(VkDevice device);
    VkDescriptorImageInfo descriptor(VkSampler sampler);
    VkResult copyFromBuffer(const vulkan_device *device, VkCommandPool cmdPool, buffer_t *pSrc);

    VkImageView view;
private:
    VkImage m_image;
    VkDeviceMemory m_memory;
    VkExtent2D m_extent;
    VkFormat m_format;

    friend buffer_t;
};

class buffer_t
{
public:
    buffer_t() = default;
    buffer_t(const vulkan_device *device, const VkBufferCreateInfo *pCreateInfo,
             VkMemoryPropertyFlags memFlags);

    void destroy(VkDevice device);

    VkDescriptorBufferInfo descriptor(VkDeviceSize offset);
    VkResult fill(VkDevice device, const void *src, size_t sz);
    void copyToBuffer(VkCommandBuffer cmd, buffer_t *pDst);
    void copyToImage(VkCommandBuffer cmd, image_buffer *pDst, VkExtent2D extent);

    VkDeviceSize size;
    VkBuffer data;
    VkDeviceMemory memory;
};
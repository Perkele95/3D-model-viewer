#pragma once

#include "vulkan_initialisers.hpp"
#include "vulkan_device.hpp"

class image_buffer
{
public:
    image_buffer() = default;
    image_buffer(const vulkan_device *device, const VkImageCreateInfo *pCreateInfo,
                 VkImageAspectFlags aspect);

    void destroy(VkDevice device);
    VkDescriptorImageInfo descriptor(VkSampler sampler);

    VkImageView view(){return m_view;}

private:
    VkImage         m_image;
    VkImageView     m_view;
    VkDeviceMemory  m_memory;
    VkExtent2D      m_extent;
    VkFormat        m_format;
};

class buffer_t
{
public:
    buffer_t() = default;

    void create(const vulkan_device *device,
                const VkBufferCreateInfo *pCreateInfo,
                VkMemoryPropertyFlags memFlags,
                const void *src = nullptr);

    void destroy(VkDevice device);

    VkDescriptorBufferInfo descriptor(VkDeviceSize offset);
    void copy(VkCommandBuffer cmd, buffer_t *pDst);
    void fill(VkDevice device, const void *src);

    VkDeviceSize    size;
    VkBuffer        data;
    VkDeviceMemory  memory;
};
#pragma once

#include "vulkan_initialisers.hpp"
#include "buffer.hpp"

constexpr uint8_t TEX2D_DEFAULT[] = {255, 255, 255, 255};

class texture
{
public:
    VkDescriptorImageInfo getDescriptor();
    void destroy(VkDevice device);

    VkImageView view(){return m_view;}

protected:
    void setImageLayout(VkCommandBuffer cmd,
                        VkImageLayout newLayout,
                        VkPipelineStageFlags srcStage,
                        VkPipelineStageFlags dstStage);

    VkImage         m_image;
    VkImageView     m_view;
    VkDeviceMemory  m_memory;
    VkExtent2D      m_extent;
    VkSampler       m_sampler;
	uint32_t        m_mipLevels;
	VkImageLayout   m_layout;
};

class texture2D : public texture
{
public:
    texture2D();

    void load(const vulkan_device *device, VkCommandPool cmdPool, VkFormat format, VkExtent2D extent, const void *src);
};
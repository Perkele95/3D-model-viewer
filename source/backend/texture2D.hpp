#pragma once

#include "vulkan_initialisers.hpp"
#include "buffer.hpp"

constexpr uint8_t TEX2D_DEFAULT[] = {255, 255, 255, 255};

class texture
{
public:
    void destroy(VkDevice device);

    VkDescriptorImageInfo descriptor;
protected:
    void setImageLayout(VkCommandBuffer cmd,
                        VkImageLayout newLayout,
                        VkPipelineStageFlags srcStage,
                        VkPipelineStageFlags dstStage);

    void updateDescriptor();

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

    void loadFromMemory(const vulkan_device *device,
                        VkCommandPool cmdPool,
                        VkFormat format,
                        VkExtent2D extent,
                        const void *src);

    void loadFromFile(const vulkan_device* device,
                      VkCommandPool cmdPool,
                      VkFormat format,
                      const char *filepath);
};
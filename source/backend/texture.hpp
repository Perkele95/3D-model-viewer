#pragma once

#include "vulkan_initialisers.hpp"
#include "buffer.hpp"

constexpr uint8_t TEX2D_DEFAULT[] = {255, 255, 255, 255};

class Texture
{
public:
    void destroy(VkDevice device);

    VkDescriptorImageInfo descriptor;
protected:
    void insertMemoryBarrier(VkCommandBuffer cmd,
                             VkImageLayout oldLayout,
                             VkImageLayout newLayout,
                             VkAccessFlags srcAccessMask,
                             VkAccessFlags dstAccessMask,
                             VkPipelineStageFlags srcStageMask,
                             VkPipelineStageFlags dstStageMask,
                             VkImageSubresourceRange subresourceRange);

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
//TODO(arle): include non-mipmapped load from memory
class Texture2D : public Texture
{
public:
    // Includes untime mipmap generation, some formats do not support this
    CoreResult loadFromMemory(const VulkanDevice *device,
                              VkQueue queue,
                              VkFormat format,
                              VkExtent2D extent,
                              const void *src);

    CoreResult loadFromFile(const VulkanDevice* device,
                            VkQueue queue,
                            VkFormat format,
                            const char *filepath);

private:
    void loadFallbackTexture(const VulkanDevice* device, VkQueue queue);
};
#pragma once

#include "vulkan_initialisers.hpp"
#include "VulkanDevice.hpp"

constexpr uint8_t TEX2D_DEFAULT[] = {255, 0, 225, 255};

class Texture
{
public:
    void destroy(VkDevice device);

    VkDescriptorImageInfo descriptor;

protected:
    void updateDescriptor();

    VkImage         m_image;
    VkImageView     m_view;
    VkDeviceMemory  m_memory;
    VkExtent2D      m_extent;
    VkSampler       m_sampler;
	uint32_t        m_mipLevels;
};

class Texture2D : public Texture
{
public:
    CoreResult loadFromMemory(const VulkanDevice *device,
                              VkQueue queue,
                              VkFormat format,
                              VkExtent2D extent,
                              size_t channels,
                              const void *src);

    CoreResult loadAddMipMap(const VulkanDevice *device,
                             VkQueue queue,
                             VkFormat format,
                             VkExtent2D extent,
                             size_t channels,
                             const void *src);

    CoreResult loadRGBA(const VulkanDevice* device,
                        VkQueue queue,
                        const char *filepath,
                        bool genMipMap = false);

private:
    void loadDefault(const VulkanDevice *device, VkQueue queue);
};

class TextureCubeMap : public Texture
{
public:
    static constexpr size_t LAYER_COUNT = 6;

    void load(const VulkanDevice* device,VkQueue queue,
              const char *(&files)[LAYER_COUNT]);
};
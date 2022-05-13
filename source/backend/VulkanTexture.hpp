#pragma once

#include "vulkan_initialisers.hpp"
#include "VulkanDevice.hpp"

constexpr uint8_t TEX2D_DEFAULT[] = {255, 255, 225, 255};

class TextureBase
{
public:
    void destroy(VkDevice device);

    VkImage                 image;
    VkImageView             view;
    VkExtent2D              extent;
	uint32_t                mipLevels;
    VkFormat                format;
    VkDescriptorImageInfo   descriptor;

protected:
    void updateDescriptor();

    VkDeviceMemory          memory;
    VkSampler               sampler;
};

struct ImageFile
{
    uint8_t*    pixels;
    VkExtent2D  extent;
    VkFormat    format;
    uint32_t    bytesPerPixel;
    bool        heapFreeFlag;
};

class Texture2D : public TextureBase
{
public:
    static ImageFile loadFile(const char *filename, VkFormat format, uint32_t reqComp = 4);
    static void freeFile(ImageFile &file);

    void create(const VulkanDevice *device,
                      VkQueue queue,
                      const ImageFile &file,
                      bool genMipMaps = false);
};

// Loads HDR images with 32-bit float precision
class HDRImage : public TextureBase
{
public:
    CoreResult load(const VulkanDevice *device, VkQueue queue, const char *filename);
};

class TextureCubeMap : public TextureBase
{
public:
    void prepare(const VulkanDevice *device);
};
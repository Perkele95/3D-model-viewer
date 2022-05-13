#pragma once

#include "../base.hpp"
#include "vulkan_initialisers.hpp"
#include "VulkanBuffer.hpp"
#include "VulkanTools.hpp"

constexpr size_t MAX_IMAGES_IN_FLIGHT = 2;

constexpr const char *ValidationLayers[] = {"VK_LAYER_KHRONOS_validation"};
constexpr const char *DeviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
constexpr const char *RequiredExtensions[] = {"VK_KHR_surface", "VK_KHR_win32_surface"};

enum MEMORY_FLAGS : VkMemoryPropertyFlags
{
    MEM_FLAG_HOST_VISIBLE = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    MEM_FLAG_GPU_LOCAL = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
};

enum USAGE_FLAGS : VkBufferUsageFlags
{
    USAGE_VERTEX_TRANSFER_SRC = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    USAGE_VERTEX_TRANSFER_DST = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    USAGE_INDEX_TRANSFER_SRC = VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    USAGE_INDEX_TRANSFER_DST = VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
};

struct QueueBits
{
    uint32_t graphics;
    uint32_t present;
};

struct OffscreenBuffer
{
    VkImage         image;
    VkImageView     view;
    VkDeviceMemory  memory;
    VkFramebuffer   framebuffer;
};

class VulkanDevice
{
public:

    void create(bool validation);
    void destroy();

    constexpr operator VkDevice()
    {
        return this->device;
    }

    // Tools

    VkMemoryAllocateInfo getMemoryAllocInfo(VkMemoryRequirements memReqs, VkMemoryPropertyFlags flags) const;
    bool linearFilterSupport(VkFormat format, VkImageTiling tiling) const;
    VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, bool begin = true) const;
    void flushCommandBuffer(VkCommandBuffer command, VkQueue queue, bool free = true) const;

    void createBuffer(VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags memFlags,
                      VkDeviceSize size,
                      VulkanBuffer &buffer,
                      const void *src = nullptr) const;

    void createOffscreenBuffer(VkFormat format,
                               VkExtent2D dimension,
                               VkRenderPass renderPass,
                               VkQueue queue,
                               OffscreenBuffer &buffer) const;

    // ~Tools

    VkPhysicalDevice    gpu;
    QueueBits           queueBits;
    VkDevice            device;
    VkCommandPool       commandPool;
};
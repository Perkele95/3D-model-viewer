#pragma once

#include "../base.hpp"
#include "vulkan_initialisers.hpp"

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

struct queue_data
{
    VkQueue queue;
    uint32_t family;
};

class vulkan_device
{
public:
    // TODO(arle): replace with method & add settings field
    vulkan_device(pltf::logical_device device,
                  debug_message_callback messageCallback,
                  bool validation, bool vSync);
    ~vulkan_device();

    void refresh();

    // Tools

    VkFormat getDepthFormat() const;
    VkResult buildSwapchain(VkSwapchainKHR oldSwapchain, VkSwapchainKHR *pSwapchain) const;
    VkMemoryAllocateInfo getMemoryAllocInfo(VkMemoryRequirements memReqs, VkMemoryPropertyFlags flags) const;
    VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, VkCommandPool pool, bool begin = true) const;
    void flushCommandBuffer(VkCommandBuffer command, VkQueue queue, VkCommandPool pool, bool free = true) const;

    // ~Tools

    VkPhysicalDevice            gpu;
    VkDevice                    device;
    queue_data                  graphics, present;
    VkSurfaceFormatKHR          surfaceFormat;
    VkPresentModeKHR            presentMode;
    VkSurfaceCapabilitiesKHR    capabilities;
    VkExtent2D                  extent;
    VkSampleCountFlagBits       sampleCount;
    uint32_t                    minImageCount;
    float                       aspectRatio;

private:
    void pickPhysicalDevice();
    void prepareLogicalDevice(bool validation);
    void pickSurfaceFormat();
    void pickPresentMode(VkPresentModeKHR preferredMode);

    void getSampleCount();

    debug_message_callback      m_messageCallback;
    VkInstance                  m_instance;
    VkSurfaceKHR                m_surface;
};
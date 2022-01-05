#pragma once

#include "../base.hpp"
#include "vulkan_initialisers.hpp"

constexpr VkMemoryPropertyFlags VISIBLE_BUFFER_FLAGS = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

struct queue_data
{
    VkQueue queue;
    uint32_t family;
};

struct vulkan_device
{
    void init(Platform::lDevice platformDevice, linear_storage *transient, bool validation, bool vSync);
    ~vulkan_device();
    void refresh();

    // Tools

    VkFormat getDepthFormat() const;
    VkResult loadShader(const file_t *src, VkShaderModule *pModule) const;
    VkResult buildSwapchain(VkSwapchainKHR oldSwapchain, VkSwapchainKHR *pSwapchain) const;

    VkMemoryAllocateInfo getMemoryAllocInfo(VkMemoryRequirements memReqs, VkMemoryPropertyFlags flags) const;

    // ~Tools

    VkPhysicalDevice gpu;
    VkDevice device;
    queue_data graphics, present;
    VkSurfaceFormatKHR surfaceFormat;
    VkPresentModeKHR presentMode;
    VkSurfaceCapabilitiesKHR capabilities;
    VkExtent2D extent;
    VkSampleCountFlagBits sampleCount;
    uint32_t minImageCount;

private:
    void pickPhysicalDevice(linear_storage *transient);
    void prepareLogicalDevice(bool validation);
    void pickSurfaceFormat(linear_storage *transient);
    void pickPresentMode(linear_storage *transient, VkPresentModeKHR preferredMode);

    void getSampleCount();

    VkInstance m_instance;
    VkSurfaceKHR m_surface;
};
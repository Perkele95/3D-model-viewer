#pragma once

#include "../base.hpp"
#include "../mv_allocator.hpp"
#include "vulkan_tools.hpp"

constexpr VkMemoryPropertyFlags VISIBLE_BUFFER_FLAGS = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

struct queue_data
{
    VkQueue queue;
    uint32_t family;
};

struct vulkan_device
{
    void create(Platform::lDevice platformDevice, mv_allocator *allocator, bool validation, bool vSync);
    void destroy();
    void refresh();

    // Tools

    VkFormat getDepthFormat() const;
    VkResult loadShader(const file_t *src, VkShaderModule *pModule) const;
    VkResult buildSwapchain(VkSwapchainKHR oldSwapchain, VkSwapchainKHR *pSwapchain) const;

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
    void pickPhysicalDevice(mv_allocator *allocator);
    void prepareLogicalDevice(bool validation);
    void pickSurfaceFormat(mv_allocator *allocator);
    void pickPresentMode(mv_allocator *allocator, VkPresentModeKHR preferredMode);

    void getSampleCount();

    VkInstance instance;
    VkSurfaceKHR surface;
};
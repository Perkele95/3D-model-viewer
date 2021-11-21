#pragma once

#include "../base.hpp"
#include "../mv_allocator.hpp"
#include "vulkan/vulkan.h"
#include "vk_initialisers.hpp"

constexpr VkMemoryPropertyFlags VISIBLE_BUFFER_FLAGS = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

struct queue_data
{
    VkQueue queue;
    uint32_t family;
};

struct buffer_t
{
    void destroy(VkDevice device);

    VkBuffer data;
    VkDeviceMemory memory;
    VkDeviceSize size;
};

struct image_buffer
{
    void destroy(VkDevice device);

    VkImage image;
    VkImageView view;
    VkDeviceMemory memory;
};

VkMemoryAllocateInfo GetMemoryAllocInfo(VkPhysicalDevice gpu, VkMemoryRequirements memReqs,
                                        VkMemoryPropertyFlags flags);
void SetSurface(VkInstance instance, VkSurfaceKHR *pSurface);

struct vulkan_device
{
    void create(mv_allocator *allocator, bool validation, bool vSync);
    void destroy();
    void refresh();

    // Tools

    VkExtent2D getExtent() const;
    uint32_t getMinImageCount() const;
    VkSampleCountFlagBits getSampleCount() const;
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

private:
    void pickPhysicalDevice(mv_allocator *allocator);
    void prepareLogicalDevice(bool validation);
    void pickSurfaceFormat(mv_allocator *allocator);
    void pickPresentMode(mv_allocator *allocator, VkPresentModeKHR preferredMode);

    VkInstance instance;
    VkSurfaceKHR surface;
};
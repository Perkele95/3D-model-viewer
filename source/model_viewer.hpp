#pragma once

#include "base.hpp"
#include "mv_allocator.hpp"
#include "backend/vulkan_device.hpp"
#include "backend/vk_text_overlay.hpp"

constexpr size_t MAX_IMAGES_IN_FLIGHT = 2;

struct model_viewer
{
    model_viewer(mv_allocator *allocator, vec2<int32_t> extent, uint32_t flags);
    ~model_viewer();

    model_viewer(const model_viewer &src) = delete;
    model_viewer(const model_viewer &&src) = delete;
    model_viewer &operator=(const model_viewer &src) = delete;
    model_viewer &operator=(const model_viewer &&src) = delete;

    void run(mv_allocator *allocator, uint32_t flags, float dt);

private:
    void onWindowResize();
    void buildResources(mv_allocator *allocator);
    void buildMsaa();
    void buildDepth();
    void buildRenderPass();
    void buildFramebuffers();
    void buildSyncObjects();

    vulkan_device *hDevice;
    text_overlay *hOverlay;

    VkExtent2D extent;
    size_t imageCount, currentFrame;
    VkCommandPool cmdPool;
    VkRenderPass renderPass;
    VkSwapchainKHR swapchain;
    VkImage *swapchainImages;
    VkImageView *swapchainViews;
    VkFramebuffer *framebuffers;

    image_buffer msaa;
    image_buffer depth;
    VkSampleCountFlagBits sampleCount;
    VkFormat depthFormat;

    VkSemaphore imageAvailableSPs[MAX_IMAGES_IN_FLIGHT];
    VkSemaphore renderFinishedSPs[MAX_IMAGES_IN_FLIGHT];
    VkFence inFlightFences[MAX_IMAGES_IN_FLIGHT];
    VkFence *imagesInFlight;
};
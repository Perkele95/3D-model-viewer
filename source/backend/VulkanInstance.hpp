#pragma once

#include "vulkan_initialisers.hpp"
#include "VulkanDevice.hpp"
#include "storage.hpp"

enum class VSyncMode
{
    Off             = VK_PRESENT_MODE_IMMEDIATE_KHR,
    DoubleBuffered  = VK_PRESENT_MODE_FIFO_KHR,
    TripleBuffered  = VK_PRESENT_MODE_MAILBOX_KHR
};

struct VulkanInstanceSettings
{
    const char*         title;
    bool                enValidation;
    VSyncMode           syncMode;
};

struct SyncData
{
    VkSemaphore         imageAvailableSPs[MAX_IMAGES_IN_FLIGHT];
    VkSemaphore         renderFinishedSPs[MAX_IMAGES_IN_FLIGHT];
    VkFence             inFlightFences[MAX_IMAGES_IN_FLIGHT];
};

struct ImageResource
{
    VkImage             image;
    VkImageView         view;
    VkDeviceMemory      memory;
};

class VulkanInstance : public linear_storage
{
public:
    VulkanInstance(size_t storageSize): linear_storage(storageSize)
    {
        resizeRequired = false;
        imageCount = 0;
        imageIndex = 0;
        currentFrame = 1;
    }

protected:
    void prepare();
    void destroy();
    void onResize();
    void prepareFrame();
    void submitFrame();

    // Settings

    VulkanInstanceSettings            settings;
    pltf::logical_device        platformDevice;
    VulkanDevice                device;
    log_message_callback        coreMessage;

    // Shared

    VkFormat                    depthFormat;
    VkSampleCountFlagBits       sampleCount;
    VkSurfaceFormatKHR          surfaceFormat;
    VkExtent2D                  extent;
    float                       aspectRatio;
    bool                        resizeRequired;
    size_t                      imageCount;
    uint32_t                    imageIndex;
    size_t                      currentFrame;
    VkCommandBuffer             commandBuffers[MAX_IMAGES_IN_FLIGHT];
    VkSubmitInfo                submitInfo;
    VkPresentInfoKHR            presentInfo;
    VkQueue                     graphicsQueue;
    VkFramebuffer*              framebuffers;
    VkRenderPass                renderPass;

private:
    void pickPhysicalDevice();
    void getSwapchainImages();
    VkResult prepareSwapchain(VkSwapchainKHR oldSwapchain);
    void prepareSwapchainViews();
    void prepareSampleCount();
    void prepareMsaa();
    void prepareDepthFormat();
    void prepareDepth();
    VkResult prepareRenderpass();
    void prepareFramebuffers();
    void refreshCapabilities();
    void prepareSurfaceFormat();
    void preparePresentMode(VkPresentModeKHR preferredMode);

    VkInstance                  m_instance;
    VkSurfaceKHR                m_surface;
    VkPresentModeKHR            m_presentMode;
    VkSurfaceCapabilitiesKHR    m_capabilities;
    VkQueue                     m_presentQueue;
    VkSwapchainKHR              m_swapchain;
    VkImage*                    m_swapchainImages;
    VkImageView*                m_swapchainViews;
    SyncData                    m_sync;
    ImageResource               m_msaa;
    ImageResource               m_depth;
    VkDescriptorPool            m_descriptorPool;
};
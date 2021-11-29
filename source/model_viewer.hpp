#pragma once

#include "base.hpp"
#include "mv_allocator.hpp"
#include "backend/vulkan_tools.hpp"
#include "backend/vulkan_device.hpp"
#include "backend/vk_text_overlay.hpp"
#include "backend/camera.hpp"

#include "mv_utils/mat4.hpp"

constexpr size_t MAX_IMAGES_IN_FLIGHT = 2;

struct alignas(4) mesh_vertex
{
    vec3<float> position;
    vec4<float> colour;
};
using mesh_index = uint32_t;

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
    void onWindowResize(mv_allocator *allocator);

    void buildResources(mv_allocator *allocator);
    void buildSwapchainViews();
    void buildMsaa();
    void buildDepth();
    void buildRenderPass();
    void buildFramebuffers();
    void buildSyncObjects();
    void buildDescriptorSets();
    void buildPipeline();
    void buildMeshBuffers();

    void updateCmdBuffers();

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

    // scene resources

    //VkDescriptorPool descriptorPool;
    //VkDescriptorSetLayout setLayout;
    //VkDescriptorSet *descriptorSets;
    VkCommandBuffer *commandBuffers;
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
    VkShaderModule vertShaderModule, fragShaderModule;
    buffer_t vertexBuffer, indexBuffer;

    camera mainCamera;
};
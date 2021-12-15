#pragma once

#include "base.hpp"

#include "mv_allocator.hpp"
#include "backend/vulkan_device.hpp"
#include "backend/vulkan_text_overlay.hpp"
#include "backend/camera.hpp"

#include "mv_utils/mat4.hpp"

constexpr size_t MAX_IMAGES_IN_FLIGHT = 2;

struct alignas(4) mesh_vertex
{
    vec3<float> position;
    vec3<float> normal;
    vec4<float> colour;
};
using mesh_index = uint32_t;

struct model_viewer
{
    model_viewer(Platform::lDevice platformDevice);
    ~model_viewer();

    model_viewer(const model_viewer &src) = delete;
    model_viewer(const model_viewer &&src) = delete;
    model_viewer &operator=(const model_viewer &src) = delete;
    model_viewer &operator=(const model_viewer &&src) = delete;

    void run(const input_state *input, uint32_t flags, float dt);

private:
    void testProc(const input_state *input, float dt);
    void onWindowResize();

    void buildResources();
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

    mv_allocator allocator;

    vulkan_device *hDevice;
    text_overlay *hOverlay;

    size_t imageCount, currentFrame;
    VkCommandPool cmdPool;
    VkRenderPass renderPass;
    VkSwapchainKHR swapchain;
    VkImage *swapchainImages;
    VkImageView *swapchainViews;
    VkFramebuffer *framebuffers;

    image_buffer msaa;
    image_buffer depth;
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
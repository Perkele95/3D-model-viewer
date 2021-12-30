#pragma once

#include "base.hpp"

#include "mv_allocator.hpp"
#include "backend/vulkan_device.hpp"
#include "backend/vulkan_text_overlay.hpp"
#include "backend/camera.hpp"
#include "backend/model.hpp"

#include "mv_utils/mat4.hpp"

constexpr size_t MAX_IMAGES_IN_FLIGHT = 2;

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
    void buildUniformBuffers();
    void buildDescriptorSets();
    void buildPipeline();
    void buildMeshBuffers();
    void updateCmdBuffers();

    mv_allocator m_allocator;

    vulkan_device *m_device;
    text_overlay *m_overlay;

    size_t m_imageCount, m_currentFrame;
    VkCommandPool m_cmdPool;
    VkRenderPass m_renderPass;
    VkSwapchainKHR m_swapchain;
    VkImage *m_swapchainImages;
    VkImageView *m_swapchainViews;
    VkFramebuffer *m_framebuffers;

    image_buffer m_msaa;
    image_buffer m_depth;
    VkFormat m_depthFormat;

    VkSemaphore m_imageAvailableSPs[MAX_IMAGES_IN_FLIGHT];
    VkSemaphore m_renderFinishedSPs[MAX_IMAGES_IN_FLIGHT];
    VkFence m_inFlightFences[MAX_IMAGES_IN_FLIGHT];
    VkFence *m_imagesInFlight;

    VkCommandBuffer *m_commandBuffers;
    VkPipeline m_pipeline;
    VkPipelineLayout m_pipelineLayout;
    VkShaderModule m_vertShaderModule, m_fragShaderModule;
    buffer_t m_vertexBuffer, m_indexBuffer;

    camera m_mainCamera;

    VkDescriptorPool m_descriptorPool;
    VkDescriptorSetLayout m_descriptorSetLayout;
    VkDescriptorSet *m_descriptorSets;
    buffer_t *m_uniformBuffers;

    model3D m_model;
};
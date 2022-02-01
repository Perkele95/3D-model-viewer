#pragma once

#include "base.hpp"

#include "mv_utils/linear_storage.hpp"
#include "backend/vulkan_device.hpp"
#include "backend/vulkan_text_overlay.hpp"

#include "backend/shader.hpp"
#include "backend/camera.hpp"
#include "backend/lights.hpp"
#include "backend/model.hpp"

#include "mv_utils/mat4.hpp"

constexpr size_t MAX_IMAGES_IN_FLIGHT = 2;

struct uniform_buffer
{
    buffer_t camera;
    buffer_t lights;
};

class model_viewer
{
public:
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
    void buildDescriptorPool();
    void buildUniformBuffers();
    void buildDescriptorSets();
    void buildPipeline();

    void updateCamera();
    void updateLights();
    void updateCmdBuffers();

    linear_storage m_permanentStorage;

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
    shader_object m_shaders[2];

    camera m_mainCamera;
    uniform_buffer *m_uniformBuffers;

    VkDescriptorPool m_descriptorPool;
    VkDescriptorSetLayout m_descriptorSetLayout;
    VkDescriptorSet *m_descriptorSets;

    model3D m_model;
};
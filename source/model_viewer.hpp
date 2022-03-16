#pragma once

#include "base.hpp"

#include "storage.hpp"
#include "backend/vulkan_device.hpp"
#include "backend/vulkan_text_overlay.hpp"

#include "backend/shader.hpp"
#include "backend/camera.hpp"
#include "backend/lights.hpp"
#include "backend/model3D.hpp"

#include "mv_utils/mat4.hpp"

constexpr size_t MAX_IMAGES_IN_FLIGHT = 2;

class model_viewer : public linear_storage
{
public:
    model_viewer(pltf::logical_device device);
    ~model_viewer();

    model_viewer(const model_viewer &src) = delete;
    model_viewer(const model_viewer &&src) = delete;
    model_viewer &operator=(const model_viewer &src) = delete;
    model_viewer &operator=(const model_viewer &&src) = delete;

    void swapBuffers(pltf::logical_device device);
    void onKeyEvent(pltf::logical_device device, pltf::key_code key, pltf::modifier mod);
    void onMouseButtonEvent(pltf::logical_device device, pltf::mouse_button button);

private:
    void onWindowResize();

    void buildResources();
    void loadModel();
    void buildSwapchainViews();
    void buildMsaa();
    void buildDepth();
    void buildRenderPass();
    void buildFramebuffers();
    void buildSyncObjects();
    void buildDescriptors(const pbr_material* pMaterial);
    void buildPipeline();

    void gameUpdate(float dt);
    void updateLights();
    void updateCmdBuffers(size_t imageIndex);

    vulkan_device*          m_device;
    text_overlay*           m_overlay;

    size_t                  m_imageCount, m_currentFrame;
    VkCommandPool           m_cmdPool;
    VkRenderPass            m_renderPass;
    VkSwapchainKHR          m_swapchain;
    VkImage*                m_swapchainImages;
    VkImageView*            m_swapchainViews;
    VkFramebuffer*          m_framebuffers;

    image_buffer            m_msaa;
    image_buffer            m_depth;
    VkFormat                m_depthFormat;

    VkSemaphore             m_imageAvailableSPs[MAX_IMAGES_IN_FLIGHT];
    VkSemaphore             m_renderFinishedSPs[MAX_IMAGES_IN_FLIGHT];
    VkFence                 m_inFlightFences[MAX_IMAGES_IN_FLIGHT];
    VkFence*                m_imagesInFlight;

    VkCommandBuffer*        m_commandBuffers;
    VkPipeline              m_pipeline;
    VkPipelineLayout        m_pipelineLayout;
    shader_object           m_shaders[2];

    camera                  m_mainCamera;
    lights                  m_lights;

    VkDescriptorPool        m_descriptorPool;
    VkDescriptorSetLayout   m_descriptorSetLayout;
    VkDescriptorSet*        m_descriptorSets;

    model3D                 m_model;
};
#pragma once

#include "base.hpp"
#include "backend/VulkanInstance.hpp"
#include "backend/vulkan_text_overlay.hpp"

#include "backend/shader.hpp"
#include "backend/camera.hpp"
#include "backend/lights.hpp"
#include "backend/model3D.hpp"

#include "mv_utils/mat4.hpp"

struct PipelineData
{
    void destroy(VkDevice device)
    {
        vkDestroyPipeline(device, pipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
        vertexShader.destroy(device);
        fragmentShader.destroy(device);
        model->destroy(device);

        for (size_t i = 0; i < MAX_IMAGES_IN_FLIGHT; i++)
        {
            cameraUniforms[i].destroy(device);
            lightUniforms[i].destroy(device);
        }
    }

    void bind(VkCommandBuffer cmd, size_t currentFrame)
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdBindDescriptorSets(cmd,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipelineLayout,
                                0,
                                1,
                                &descriptorSets[currentFrame],
                                0,
                                nullptr);
    }

    VkPipeline              pipeline;
    VkPipelineLayout        pipelineLayout;
    VertexShader            vertexShader;
    FragmentShader          fragmentShader;
    VkDescriptorSetLayout   setLayout;
    VkDescriptorSet         descriptorSets[MAX_IMAGES_IN_FLIGHT];
    VulkanBuffer            cameraUniforms[MAX_IMAGES_IN_FLIGHT];
    VulkanBuffer            lightUniforms[MAX_IMAGES_IN_FLIGHT];
    Model3D*                model;
};

class ModelViewer : public VulkanInstance
{
public:
    ModelViewer(pltf::logical_device platform);
    ~ModelViewer();

    ModelViewer(const ModelViewer &src) = delete;
    ModelViewer(const ModelViewer &&src) = delete;
    ModelViewer &operator=(const ModelViewer &src) = delete;
    ModelViewer &operator=(const ModelViewer &&src) = delete;

    void run();
    void onWindowSize(int32_t width, int32_t height);
    void onKeyEvent(pltf::key_code key, pltf::modifier mod);
    void onMouseButtonEvent(pltf::mouse_button button);
    void onScrollWheelEvent(double x, double y);

private:
    void buildModelData();
    void buildUniformBuffers();
    void buildDescriptors();
    void buildPipelines(); // TODO(arle): split into pipeline & layout
    // pipelinelayout does not need to be recreated upon window resize event

    void updateCamera(float dt);
    void updateLights();
    void recordFrame(VkCommandBuffer cmdBuffer);

    // Scene & gui commands to be submitted

    VkCommandBuffer         m_commands[2];
    VulkanTextOverlay*      m_overlay;
    Camera                  m_mainCamera;
    SceneLights             m_lights;
    VkDescriptorPool        m_descriptorPool;
    PipelineData            m_pbr;
};
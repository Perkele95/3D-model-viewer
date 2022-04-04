#pragma once

#include "base.hpp"
#include "backend/VulkanInstance.hpp"
#include "backend/VulkanTextOverlay.hpp"

#include "backend/shader.hpp"
#include "backend/camera.hpp"
#include "backend/lights.hpp"
#include "backend/model3D.hpp"

#include "mv_utils/mat4.hpp"

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
    void buildScene();
    void buildCubeMap();
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

    struct Scene
    {
        VkPipeline              pipeline;
        VkPipelineLayout        pipelineLayout;
        VertexShader            vertexShader;
        FragmentShader          fragmentShader;
        VkDescriptorSetLayout   setLayout;
        VkDescriptorSet         descriptorSets[MAX_IMAGES_IN_FLIGHT];
        VulkanBuffer            cameraBuffers[MAX_IMAGES_IN_FLIGHT];
        VulkanBuffer            lightBuffers[MAX_IMAGES_IN_FLIGHT];
        PBRModel*               model;
    }scene;

    struct CubeMap
    {
        VkPipeline              pipeline;
        VkPipelineLayout        pipelineLayout;
        VertexShader            vertexShader;
        FragmentShader          fragmentShader;
        VkDescriptorSetLayout   setLayout;
        VkDescriptorSet         descriptorSets[MAX_IMAGES_IN_FLIGHT];
        CubeMapModel*           model;
    }cubeMap;
};
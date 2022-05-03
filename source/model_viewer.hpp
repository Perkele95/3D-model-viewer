#pragma once

#include "base.hpp"
#include "backend/VulkanInstance.hpp"
#include "backend/VulkanTextOverlay.hpp"

#include "backend/shader.hpp"
#include "backend/camera.hpp"
#include "backend/lights.hpp"
#include "backend/model3D.hpp"

template<typename T>
class EventDispatcher
{
public:
    static void WindowSize(pltf::logical_device device, int32_t width, int32_t height)
    {
        auto app = static_cast<T*>(pltf::DeviceGetHandle(device));
        app->onWindowSize(width, height);
    }

    static void KeyEvent(pltf::logical_device device, pltf::key_code key, pltf::modifier mod)
    {
        auto app = static_cast<T*>(pltf::DeviceGetHandle(device));
        app->onKeyEvent(key, mod);
    }

    static void MouseEvent(pltf::logical_device device, pltf::mouse_button button)
    {
        auto app = static_cast<T*>(pltf::DeviceGetHandle(device));
        app->onMouseButtonEvent(button);
    }

    static void ScrollWheel(pltf::logical_device device, double x, double y)
    {
        auto app = static_cast<T*>(pltf::DeviceGetHandle(device));
        app->onScrollWheelEvent(x, y);
    }

    EventDispatcher(pltf::logical_device device, T *handle)
    {
        pltf::DeviceSetHandle(device, handle);

        pltf::EventCallbacks procs;
        procs.windowSize = WindowSize;
        procs.keyEvent = KeyEvent;
        procs.mouseMove = nullptr;
        procs.mouseButton = MouseEvent;
        procs.scrollWheel = ScrollWheel;
        pltf::EventsBindCallbacks(device, procs);
    }
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
    // Pre-generated pbr textures

    void generateBrdfLUT();
    void generateIrradianceMap();
    void generatePrefilteredCube();

    void loadResources();
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

    struct PreGenerated
    {
        VkImage                 image;
        VkImageView             view;
        VkDeviceMemory          memory;
        VkSampler               sampler;
        VkDescriptorImageInfo   descriptor;
    }brdf, irradiance;

    // TODO(arle): separate models and textures away from structs
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

    struct Skybox
    {
        VkPipeline              pipeline;
        VkPipelineLayout        pipelineLayout;
        VertexShader            vertexShader;
        FragmentShader          fragmentShader;
        VkDescriptorSetLayout   setLayout;
        VkDescriptorSet         descriptorSets[MAX_IMAGES_IN_FLIGHT];
        CubeMapModel*           model;
    }skybox;
};
#pragma once

#include "base.hpp"
#include "backend/VulkanInstance.hpp"
#include "backend/VulkanTextOverlay.hpp"
#include "backend/VulkanModels.hpp"

#include "backend/shader.hpp"
#include "backend/camera.hpp"
#include "backend/lights.hpp"

class ModelViewer : public VulkanInstance
{
public:
    ModelViewer();
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
    void generateBrdfLUT();
    void generateIrradianceMap();
    void generatePrefilteredMap();

    void loadHDRSkybox(const char *filename);
    void loadResources();
    void buildUniformBuffers();
    void buildDescriptors();
    void buildPipelines(); // TODO(arle): split into pipeline & layout
    // pipelinelayout does not need to be recreated upon window resize event

    void updateCamera(float dt);
    void updateLights();
    void recordFrame(VkCommandBuffer cmdBuffer);

    VkCommandBuffer         m_commands[2];
    VulkanTextOverlay*      m_overlay;
    Camera                  m_mainCamera;
    SceneLights             m_lights;
    VkDescriptorPool        m_descriptorPool;

    struct ModelAssets
    {
        Model3D                 object;
        CubemapModel            skybox;
    }models;

    struct TextureAssets
    {
        Texture2D               albedo;
        Texture2D               normal;
        Texture2D               roughness;
        Texture2D               metallic;
        Texture2D               ao;
        TextureCubeMap          environment;
    }textures;

    TextureCubeMap              irradiance;
    TextureCubeMap              prefiltered;

    struct
    {
        VkImage                 image;
        VkImageView             view;
        VkDeviceMemory          memory;
        VkSampler               sampler;
        VkDescriptorImageInfo   descriptor;
    }brdf;

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
    }scene;

    struct Skybox
    {
        VkPipeline              pipeline;
        VkPipelineLayout        pipelineLayout;
        VertexShader            vertexShader;
        FragmentShader          fragmentShader;
        VkDescriptorSetLayout   setLayout;
        VkDescriptorSet         descriptorSets[MAX_IMAGES_IN_FLIGHT];
    }skybox;
};

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
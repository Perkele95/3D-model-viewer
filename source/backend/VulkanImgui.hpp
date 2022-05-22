#pragma once

#include "VulkanDevice.hpp"
#include "shader.hpp"
#include "VulkanTexture.hpp"

class VulkanImgui
{
public:
    enum class Alignment
    {
        Left,
        Centre,
        Right
    };

    struct CreateInfo
    {
        VkFormat                depthFormat;
        VkSurfaceFormatKHR      surfaceFormat;
        VkSampleCountFlagBits   sampleCount;
    };

    struct alignas(4) Vertex
    {
        vec3<float> position;
        vec2<float> texCoord;
        vec4<float> colour;
    };

    static VulkanImgui &Instance()
    {
        static VulkanImgui s;
        return s;
    }

    VulkanImgui(const VulkanImgui &rhs) = delete;
    VulkanImgui(const VulkanImgui &&rhs) = delete;

    void init(const CreateInfo &info, VkQueue queue);
    void destroy();
    void onWindowResize(VkSampleCountFlagBits sampleCount);

    // UI Layout

    void begin();
    void end();
    void text(view<const char> stringView, vec2<float> position);
    void box(vec2<float> topLeft, vec2<float> bottomRight);
    void recordFrame(size_t currentFrame, VkFramebuffer framebuffer);

    const VulkanDevice*     device;
    VkExtent2D              extent;
    VkCommandBuffer         commandBuffers[MAX_IMAGES_IN_FLIGHT];

    struct
    {
        vec4<float>         tint;
        float               size;
        Alignment           alignment;
    }settings;

private:
    VulkanImgui() = default;
    ~VulkanImgui() = default;

    // Backend

    void preparePipeline(VkSampleCountFlagBits sampleCount);

    VkRenderPass            renderPass;
    VkDescriptorPool        descriptorPool;
    VkDescriptorSetLayout   setLayout;
    VkDescriptorSet         descriptorSets[MAX_IMAGES_IN_FLIGHT];
    VkPipeline              pipeline;
    VkPipelineLayout        pipelineLayout;
    VertexShader            vertexShader;
    FragmentShader          fragmentShader;
    VulkanBuffer            vertexBuffer;
    VulkanBuffer            indexBuffer;
    Texture2D               glyphAtlas;
    Vertex*                 mappedVertices;
    uint32_t*               mappedIndices;
    uint32_t                quadCount;
    float                   zOrder;

    // Ui state

    int32_t                 hotITem;
    int32_t                 activeItem;
    int32_t                 itemCounter;
    bool                    buttonPressed;
    vec2<int32_t>           mousePosition;
};
#pragma once

#include "VulkanDevice.hpp"
#include "storage.hpp"
#include "buffer.hpp"
#include "shader.hpp"
#include "texture.hpp"

struct alignas(4) quad_vertex
{
    vec3<float> position;
    vec2<float> texCoord;
    vec4<float> colour;
};
using quad_index = uint32_t;

enum class text_align{left, centre, right};
enum class text_coord_type{absolute, relative};

class VulkanTextOverlay
{
public:
    void init(const VulkanDevice* device, linear_storage *storage);
    void destroy();

    VulkanTextOverlay(const VulkanTextOverlay &src) = delete;
    VulkanTextOverlay(const VulkanTextOverlay &&src) = delete;
    VulkanTextOverlay &operator=(const VulkanTextOverlay &src) = delete;
    VulkanTextOverlay &operator=(const VulkanTextOverlay &&src) = delete;

    void onWindowResize();

    void begin();
    void draw(view<const char> stringView, vec2<float> position);
    void end();
    void recordFrame(size_t currentFrame, uint32_t imageIndex);

    // Shared resources

    VkFormat                depthFormat;
    VkSurfaceFormatKHR      surfaceFormat;
    VkSampleCountFlagBits   sampleCount;
    size_t                  imageCount;
    VkQueue                 graphicsQueue;
    const VkFramebuffer*    frameBuffers;
    VkCommandBuffer         commandBuffers[MAX_IMAGES_IN_FLIGHT];
    VkExtent2D              extent;

    // Text settings

    vec4<float>             textTint;
    text_align              textAlignment;
    text_coord_type         textType;
    float                   textSize;

private:
    void prepareRenderpass();
    void prepareFontTexture();
    void prepareDescriptors();
    void preparePipeline();
    void prepareRenderBuffers();

    const VulkanDevice*     m_device;
    VkCommandPool           m_commandPool;
    VkRenderPass            m_renderPass;
    VkDescriptorPool        m_descriptorPool;
    VkDescriptorSetLayout   m_setLayout;
    VkDescriptorSet         m_descriptorSets[MAX_IMAGES_IN_FLIGHT];
    VkPipeline              m_pipeline;
    VkPipelineLayout        m_pipelineLayout;
    VertexShader            m_vertexShader;
    FragmentShader          m_fragmentShader;
    VulkanBuffer            m_vertexBuffer;
    VulkanBuffer            m_indexBuffer;
    Texture2D               m_fontTexture;
    quad_vertex*            m_mappedVertices;
    quad_index*             m_mappedIndices;
    size_t                  m_quadCount;
    float                   m_zOrder;
};
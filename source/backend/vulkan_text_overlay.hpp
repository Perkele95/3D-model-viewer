#pragma once

#include "vulkan_device.hpp"
#include "buffer.hpp"
#include "shader.hpp"
#include "texture2D.hpp"

struct alignas(4) quad_vertex
{
    vec3<float> position;
    vec2<float> texCoord;
    vec4<float> colour;
};
using quad_index = uint32_t;

enum class text_align{left, centre, right};
enum class text_coord_type{absolute, relative};

struct text_overlay_create_info
{
    linear_storage *sharedPermanent;
    const vulkan_device *device;
    VkCommandPool cmdPool;
    size_t imageCount;
    VkFormat depthFormat;
};

class text_overlay
{
public:
    text_overlay(const text_overlay_create_info *pInfo);
    ~text_overlay();

    text_overlay(const text_overlay &src) = delete;
    text_overlay(const text_overlay &&src) = delete;
    text_overlay &operator=(const text_overlay &src) = delete;
    text_overlay &operator=(const text_overlay &&src) = delete;

    void onWindowResize(VkCommandPool commandPool);

    void begin();
    void draw(view<const char> stringView, vec2<float> position);
    void end();
    void updateCmdBuffers(const VkFramebuffer *pFramebuffers);

    VkCommandBuffer *cmdBuffers;

    vec4<float> textTint;
    text_align textAlignment;
    text_coord_type textType;
    float textSize;

private:
    void prepareRenderpass();
    void prepareFontTexture();
    void prepareDescriptorSets();
    void preparePipeline();
    void prepareRenderBuffers();

    const vulkan_device *m_device;
    VkFormat m_depthFormat;

    VkCommandPool m_cmdPool;
    VkRenderPass m_renderPass;
    size_t m_imageCount;

    VkDescriptorPool m_descriptorPool;
    VkDescriptorSetLayout m_setLayout;
    VkDescriptorSet *m_descriptorSets;
    VkPipeline m_pipeline;
    VkPipelineLayout m_pipelineLayout;
    shader_object m_shaders[2];
    buffer_t m_vertexBuffer, m_indexBuffer;

    texture2D m_fontTexture;

    quad_vertex *m_mappedVertices;
    quad_index *m_mappedIndices;

    size_t m_quadCount;
    float m_zOrder;
};
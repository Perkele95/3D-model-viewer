#pragma once

#include "vulkan_device.hpp"
#include "vulkan_tools.hpp"

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
    mv_allocator *allocator;
    const vulkan_device *device;
    VkCommandPool cmdPool;
    size_t imageCount;
    VkShaderModule vertex, fragment;
    VkFormat depthFormat;
};

struct text_overlay
{
    void create(const text_overlay_create_info *pInfo);
    void destroy();

    text_overlay(const text_overlay &src) = delete;
    text_overlay(const text_overlay &&src) = delete;
    text_overlay &operator=(const text_overlay &src) = delete;
    text_overlay &operator=(const text_overlay &&src) = delete;

    void onWindowResize(mv_allocator *allocator, VkCommandPool commandPool);

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
    void prepareFontBuffer(const void *src, VkExtent2D bitmapExtent);
    void prepareDescriptorSets(mv_allocator *allocator);
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
    VkShaderModule m_vertShaderModule, m_fragShaderModule;
    buffer_t m_vertexBuffer, m_indexBuffer;

    VkSampler m_sampler;
    image_buffer m_fontBuffer;

    quad_vertex *m_mappedVertices;
    quad_index *m_mappedIndices;

    size_t m_quadCount;
    float m_zOrder;
};
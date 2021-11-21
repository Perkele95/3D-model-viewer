#pragma once

#include "vulkan_device.hpp"

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
    VkRenderPass renderPass;
    size_t imageCount;
    VkShaderModule vertex, fragment;
};

struct text_overlay
{
    void create(const text_overlay_create_info *pInfo);
    void destroy();

    text_overlay(const text_overlay &src) = delete;
    text_overlay(const text_overlay &&src) = delete;
    text_overlay &operator=(const text_overlay &src) = delete;
    text_overlay &operator=(const text_overlay &&src) = delete;

    void onWindowResize(mv_allocator *allocator, const vulkan_device *vulkanDevice,
                        VkCommandPool commandPool, VkRenderPass sharedRenderPass);
    void begin();
    void draw(view<const char> stringView, vec2<float> position,
              vec4<float> tint, text_align alignment, text_coord_type type,
              float size);
    void end();
    void updateCmdBuffers(const VkClearValue (*clearValues)[2], const VkFramebuffer *pFramebuffers);
    VkSubmitInfo getSubmitData();

private:
    void bind(const vulkan_device *vulkanDevice, VkCommandPool commandPool,
              VkRenderPass sharedRenderPass);
    void prepareCommandbuffers();
    void prepareFontBuffer(const void *src, VkExtent2D bitmapExtent);
    void prepareDescriptorSets(mv_allocator *allocator);
    void preparePipeline();
    void prepareRenderBuffers();

    VkDevice device;
    VkPhysicalDevice gpu;
    VkQueue graphicsQueue;

    VkRenderPass renderPass;
    VkCommandPool cmdPool;
    VkSampleCountFlagBits sampleCount;
    VkFramebuffer *framebuffers;
    VkCommandBuffer *cmdBuffers;
    size_t imageCount;
    VkExtent2D extent;

    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout setLayout;
    VkDescriptorSet *descriptorSets;
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
    VkShaderModule vertShaderModule, fragShaderModule;
    buffer_t vertexBuffer, indexBuffer;

    VkSampler sampler;
    image_buffer fontBuffer;

    quad_vertex *mappedVertices;
    quad_index *mappedIndices;

    size_t quadCount;
    float zOrder;
};
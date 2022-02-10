#pragma once

#include "../base.hpp"
#include "vulkan\vulkan.h"

#define INIT_API constexpr auto [[nodiscard]]

namespace vkInits
{
    INIT_API applicationInfo(const char *appName)
    {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = appName;
        appInfo.pEngineName = appName;
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;
        return appInfo;
    }

    INIT_API instanceCreateInfo()
    {
        VkInstanceCreateInfo iInfo{};
        iInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        return iInfo;
    }

    INIT_API commandPoolCreateInfo(uint32_t queueFamilyIndex)
    {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = queueFamilyIndex;
        return poolInfo;
    }

    INIT_API swapchainCreateInfo(VkSwapchainKHR oldSwapchain)
    {
        VkSwapchainCreateInfoKHR info{};
        info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        info.oldSwapchain = oldSwapchain;
        info.imageArrayLayers = 1;
        info.clipped = VK_TRUE;
        return info;
    }

    INIT_API framebufferCreateInfo()
    {
        VkFramebufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.layers = 1;
        return info;
    }

    INIT_API descriptorPoolCreateInfo()
    {
        VkDescriptorPoolCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        return info;
    }

    INIT_API descriptorSetAllocateInfo(VkDescriptorPool pool)
    {
        VkDescriptorSetAllocateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        info.descriptorPool = pool;
        return info;
    }

    INIT_API descriptorSetLayoutBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags stageFlags)
    {
        VkDescriptorSetLayoutBinding layoutBinding = {};
        layoutBinding.binding = binding;
        layoutBinding.descriptorType = type;
        layoutBinding.descriptorCount = 1;
        layoutBinding.stageFlags = stageFlags;
        layoutBinding.pImmutableSamplers = nullptr;
        return layoutBinding;
    }

    INIT_API attachmentDescription(VkFormat format)
    {
        VkAttachmentDescription attachmentDescription{};
        attachmentDescription.format = format;
        attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
        attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        return attachmentDescription;
    }

    INIT_API shaderModuleCreateInfo(const plt::file *file)
    {
        VkShaderModuleCreateInfo shaderModuleInfo{};
        shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderModuleInfo.codeSize = file->size;
        shaderModuleInfo.pCode = static_cast<const uint32_t*>(file->handle);
        return shaderModuleInfo;
    }

    INIT_API bufferCreateInfo(VkDeviceSize size, VkBufferUsageFlags usageFlags)
    {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usageFlags;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        return bufferInfo;
    }

    INIT_API commandBufferAllocateInfo(VkCommandPool commandPool, size_t count)
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = static_cast<uint32_t>(count);
        return allocInfo;
    }

    INIT_API commandBufferBeginInfo(VkCommandBufferUsageFlags flags)
    {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = flags;
        return beginInfo;
    }

    INIT_API renderPassBeginInfo(VkRenderPass renderPass, VkExtent2D extent)
    {
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = extent;
        return renderPassInfo;
    }

    INIT_API submitInfo(const VkCommandBuffer *pCommandBuffers, size_t commandBufferCount)
    {
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = static_cast<uint32_t>(commandBufferCount);
        submitInfo.pCommandBuffers = pCommandBuffers;
        return submitInfo;
    }

    INIT_API imageCreateInfo()
    {
        VkImageCreateInfo imageCreateInfo{};
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.extent.depth = 1;
        imageCreateInfo.mipLevels = 1;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        return imageCreateInfo;
    }

    INIT_API imageViewCreateInfo()
    {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.pNext = nullptr;
        viewInfo.components = {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
        };
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        return viewInfo;
    }

    INIT_API bufferImageCopy(VkExtent2D extent)
    {
        VkBufferImageCopy copyRegion{};
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageOffset = {0, 0, 0};
        copyRegion.imageExtent = {extent.width, extent.height, 1};
        return copyRegion;
    }

    INIT_API imageMemoryBarrier(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
                                VkAccessFlags srcAccess, VkAccessFlags dstAccess)
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.image = image;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcAccessMask = srcAccess;
        barrier.dstAccessMask = dstAccess;
        return barrier;
    }

    INIT_API descriptorBufferInfo(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range)
    {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = buffer;
        bufferInfo.offset = offset;
        bufferInfo.range = range;
        return bufferInfo;
    }

    INIT_API semaphoreCreateInfo()
    {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        return semaphoreInfo;
    }

    INIT_API fenceCreateInfo()
    {
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        return fenceInfo;
    }

    INIT_API shaderStageInfo(VkShaderStageFlagBits stage, VkShaderModule module)
    {
        VkPipelineShaderStageCreateInfo stageInfo{};
        stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.stage = stage;
        stageInfo.module = module;
        stageInfo.pName = "main";
        return stageInfo;
    }

    INIT_API vertexBindingDescription(uint32_t stride)
    {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = stride;
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    INIT_API inputAssemblyInfo()
    {
        VkPipelineInputAssemblyStateCreateInfo result{};
        result.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        result.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        result.primitiveRestartEnable = VK_FALSE;
        return result;
    }

    INIT_API viewportInfo(VkExtent2D extent)
    {
        VkViewport result{};
        result.width = static_cast<float>(extent.width);
        result.height = static_cast<float>(extent.height);
        result.minDepth = 0.0f;
        result.maxDepth = 1.0f;
        return result;
    }

    INIT_API scissorInfo(VkExtent2D extent)
    {
        VkRect2D result{};
        result.offset = {0, 0};
        result.extent = extent;
        return result;
    }

    INIT_API rasterizationStateInfo(VkFrontFace frontFace)
    {
        VkPipelineRasterizationStateCreateInfo result = {};
        result.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        result.depthClampEnable = VK_FALSE;
        result.rasterizerDiscardEnable = VK_FALSE;
        result.polygonMode = VK_POLYGON_MODE_FILL;
        result.lineWidth = 1.0f;
        result.cullMode = VK_CULL_MODE_BACK_BIT;
        result.frontFace = frontFace;
        result.depthBiasEnable = VK_FALSE;
        return result;
    }

    INIT_API depthStencilStateInfo()
    {
        VkPipelineDepthStencilStateCreateInfo result = {};
        result.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        result.depthTestEnable = VK_TRUE;
        result.depthWriteEnable = VK_TRUE;
        result.depthCompareOp = VK_COMPARE_OP_LESS;
        result.depthBoundsTestEnable = VK_FALSE;
        result.stencilTestEnable = VK_FALSE;
        return result;
    }

    INIT_API colourWriteMask()
    {
        VkColorComponentFlags writeMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                                        | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        return writeMask;
    }

    INIT_API pipelineColorBlendStateCreateInfo()
    {
        VkPipelineColorBlendStateCreateInfo colourblend{};
        colourblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colourblend.logicOpEnable = VK_FALSE;
        colourblend.logicOp = VK_LOGIC_OP_COPY;
        colourblend.blendConstants[0] = 0.0f;
        colourblend.blendConstants[1] = 0.0f;
        colourblend.blendConstants[2] = 0.0f;
        colourblend.blendConstants[3] = 0.0f;
        return colourblend;
    }

    INIT_API pipelineMultisampleStateCreateInfo(VkSampleCountFlagBits rasterizationSamples)
    {
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = rasterizationSamples;
        return multisampling;
    }

    INIT_API pipelineColorBlendAttachmentState()
    {
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = colourWriteMask();
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        return colorBlendAttachment;
    }

    INIT_API writeDescriptorSet(uint32_t dstBinding)
    {
        VkWriteDescriptorSet set{};
        set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        set.dstBinding = dstBinding;
        return set;
    }
}

#undef INIT_API
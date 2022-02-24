#include "texture2D.hpp"

VkDescriptorImageInfo texture::getDescriptor()
{
    VkDescriptorImageInfo desc{};
    desc.imageLayout = m_layout;
    desc.imageView = m_view;
    desc.sampler = m_sampler;
    return desc;
}

void texture::destroy(VkDevice device)
{
    vkDestroySampler(device, m_sampler, nullptr);
    vkFreeMemory(device, m_memory, nullptr);
    vkDestroyImageView(device, m_view, nullptr);
    vkDestroyImage(device, m_image, nullptr);
}

void texture::setImageLayout(VkCommandBuffer cmd,
                             VkImageLayout newLayout,
                             VkPipelineStageFlags srcStage,
                             VkPipelineStageFlags dstStage)
{
    auto imageBarrier = vkInits::imageMemoryBarrier(m_image, m_layout, newLayout);

    switch (m_layout){
        case VK_IMAGE_LAYOUT_UNDEFINED:
            imageBarrier.srcAccessMask = VK_ACCESS_NONE;
            break;
        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            imageBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            imageBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            imageBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            imageBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            break;
        default: break;
    }

    switch (newLayout){
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            imageBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            imageBarrier.dstAccessMask = imageBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            if(imageBarrier.srcAccessMask == VK_ACCESS_NONE)
                imageBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
            imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            break;
        default: break;
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr,
                         0, nullptr, 1, &imageBarrier);
    m_layout = newLayout;
}

texture2D::texture2D()
{
    m_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_mipLevels = 1;
}

void texture2D::load(const vulkan_device *device, VkCommandPool cmdPool, VkFormat format, VkExtent2D extent, const void *src)
{
    m_extent = extent;

    auto imageInfo = vkInits::imageCreateInfo();
    imageInfo.extent = {m_extent.width, m_extent.height, 1};
    imageInfo.format = format;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.mipLevels = m_mipLevels;
    vkCreateImage(device->device, &imageInfo, nullptr, &m_image);

    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(device->device, m_image, &memReqs);

    auto allocInfo = device->getMemoryAllocInfo(memReqs, MEM_FLAG_GPU_LOCAL);
    vkAllocateMemory(device->device, &allocInfo, nullptr, &m_memory);
    vkBindImageMemory(device->device, m_image, m_memory, 0);

    auto viewInfo = vkInits::imageViewCreateInfo();
    viewInfo.image = m_image;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vkCreateImageView(device->device, &viewInfo, nullptr, &m_view);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    vkCreateSampler(device->device, &samplerInfo, nullptr, &m_sampler);

    const VkDeviceSize size = 4 * m_extent.width * m_extent.height;

    auto transferInfo = vkInits::bufferCreateInfo(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    auto transfer = buffer_t();
    transfer.create(device, &transferInfo, MEM_FLAG_HOST_VISIBLE, src);

    auto command = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, cmdPool);

    setImageLayout(command,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                   VK_PIPELINE_STAGE_TRANSFER_BIT);

    const auto copyRegion = vkInits::bufferImageCopy(m_extent);
    vkCmdCopyBufferToImage(command, transfer.data, m_image, m_layout, 1, &copyRegion);

    setImageLayout(command,
                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                   VK_PIPELINE_STAGE_TRANSFER_BIT,
                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    device->flushCommandBuffer(command, device->graphics.queue, cmdPool);

    transfer.destroy(device->device);
}
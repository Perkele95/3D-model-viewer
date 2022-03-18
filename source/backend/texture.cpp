#include "texture.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "../../vendor/stb/stb_image.h"

void texture::destroy(VkDevice device)
{
    vkDestroySampler(device, m_sampler, nullptr);
    vkFreeMemory(device, m_memory, nullptr);
    vkDestroyImageView(device, m_view, nullptr);
    vkDestroyImage(device, m_image, nullptr);
}

void texture::insertMemoryBarrier(VkCommandBuffer cmd,
                                  VkImageLayout oldLayout,
                                  VkImageLayout newLayout,
                                  VkAccessFlags srcAccessMask,
                                  VkAccessFlags dstAccessMask,
                                  VkPipelineStageFlags srcStageMask,
                                  VkPipelineStageFlags dstStageMask,
                                  VkImageSubresourceRange subresourceRange)
{
    auto barrier = vkInits::imageMemoryBarrier(m_image, oldLayout, newLayout);
    barrier.srcAccessMask = srcAccessMask;
    barrier.dstAccessMask = dstAccessMask;
    barrier.subresourceRange = subresourceRange;
    vkCmdPipelineBarrier(cmd, srcStageMask, dstStageMask,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void texture::setImageLayout(VkCommandBuffer cmd,
                             VkImageLayout newLayout,
                             VkPipelineStageFlags srcStage,
                             VkPipelineStageFlags dstStage)
{
    auto imageBarrier = vkInits::imageMemoryBarrier(m_image, m_layout, newLayout);

    switch (m_layout)
    {
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
    default:
        break;
    }

    switch (newLayout)
    {
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
        if (imageBarrier.srcAccessMask == VK_ACCESS_NONE)
            imageBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        break;
    default:
        break;
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr,
                         0, nullptr, 1, &imageBarrier);
    m_layout = newLayout;
}

void texture::updateDescriptor()
{
    descriptor.imageLayout = m_layout;
    descriptor.imageView = m_view;
    descriptor.sampler = m_sampler;
}

constexpr VkDeviceSize GetSizeFactor(VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_R8G8B8A8_UNORM:
        return 4;
    case VK_FORMAT_R8_SRGB:
    case VK_FORMAT_R8_UNORM:
        return 1;
    default:
        break;
    }
    return 1;
}

CoreResult texture2D::loadFromMemory(const vulkan_device *device,
                                     VkCommandPool cmdPool,
                                     VkFormat format,
                                     VkExtent2D extent,
                                     const void *src)
{
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(device->gpu, format, &props);

    const auto features = VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;

    if ((props.optimalTilingFeatures & features) != features)
        return CoreResult::Format_Not_Supported;

    m_extent = extent;
    m_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_mipLevels = 1 + uint32_t(std::floor(std::log2(max(m_extent.width, m_extent.height))));

    // Transfer buffer

    const VkDeviceSize size = GetSizeFactor(format) * m_extent.width * m_extent.height;
    auto transferInfo = vkInits::bufferCreateInfo(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    auto transfer = buffer_t();
    transfer.create(device, &transferInfo, MEM_FLAG_HOST_VISIBLE, src);

    // Create target image resource

    auto imageInfo = vkInits::imageCreateInfo();
    imageInfo.extent = {m_extent.width, m_extent.height, 1};
    imageInfo.format = format;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.mipLevels = m_mipLevels;
    vkCreateImage(device->device, &imageInfo, nullptr, &m_image);

    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(device->device, m_image, &memReqs);

    auto allocInfo = device->getMemoryAllocInfo(memReqs, MEM_FLAG_GPU_LOCAL);
    vkAllocateMemory(device->device, &allocInfo, nullptr, &m_memory);
    vkBindImageMemory(device->device, m_image, m_memory, 0);

    // Create & prepare base mip level

    auto copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, cmdPool);

    VkImageSubresourceRange subResourceRange{};
    subResourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subResourceRange.baseArrayLayer = 0;
    subResourceRange.baseMipLevel = 0;
    subResourceRange.layerCount = 1;
    subResourceRange.levelCount = 1;

    insertMemoryBarrier(copyCmd,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_ACCESS_NONE,
                        VK_ACCESS_TRANSFER_WRITE_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        subResourceRange);

    const auto copyRegion = vkInits::bufferImageCopy(m_extent);
    vkCmdCopyBufferToImage(copyCmd, transfer.data, m_image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    insertMemoryBarrier(copyCmd,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        VK_ACCESS_TRANSFER_WRITE_BIT,
                        VK_ACCESS_TRANSFER_READ_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        subResourceRange);

    device->flushCommandBuffer(copyCmd, device->graphics.queue, cmdPool);
    transfer.destroy(device->device);

    auto blitCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, cmdPool);

    for (uint32_t i = 1; i < m_mipLevels; i++)
    {
        VkImageBlit blit{};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.layerCount = 1;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcOffsets[0] = {};
        blit.srcOffsets[1].x = int32_t(m_extent.width >> (i - 1));
        blit.srcOffsets[1].y = int32_t(m_extent.height >> (i - 1));
        blit.srcOffsets[1].z = 1;

        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.layerCount = 1;
        blit.dstSubresource.mipLevel = i;
        blit.dstOffsets[0] = {};
        blit.dstOffsets[1].x = int32_t(m_extent.width >> i);
        blit.dstOffsets[1].y = int32_t(m_extent.height >> i);
        blit.dstOffsets[1].z = 1;

        VkImageSubresourceRange dstSubRange{};
        dstSubRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        dstSubRange.baseMipLevel = i;
        dstSubRange.layerCount = 1;
        dstSubRange.levelCount = 1;

        insertMemoryBarrier(blitCmd,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_ACCESS_NONE,
                            VK_ACCESS_TRANSFER_WRITE_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            dstSubRange);

        vkCmdBlitImage(blitCmd,
                        m_image,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        m_image,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        1,
                        &blit,
                        VK_FILTER_LINEAR);

        insertMemoryBarrier(blitCmd,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            VK_ACCESS_TRANSFER_WRITE_BIT,
                            VK_ACCESS_TRANSFER_READ_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            dstSubRange);
    }

    subResourceRange.levelCount = m_mipLevels;
    insertMemoryBarrier(blitCmd,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_ACCESS_TRANSFER_READ_BIT,
                        VK_ACCESS_SHADER_READ_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        subResourceRange);

    device->flushCommandBuffer(blitCmd, device->graphics.queue, cmdPool);
    m_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    auto viewInfo = vkInits::imageViewCreateInfo();
    viewInfo.image = m_image;
    viewInfo.format = format;
    viewInfo.subresourceRange = subResourceRange;
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
    samplerInfo.maxLod = float(m_mipLevels);
    vkCreateSampler(device->device, &samplerInfo, nullptr, &m_sampler);

    updateDescriptor();
    return CoreResult::Success;
}

CoreResult texture2D::loadFromFile(const vulkan_device *device,
                                   VkCommandPool cmdPool,
                                   VkFormat format,
                                   const char *filepath)
{
    int x, y, channels;
    auto pixels = stbi_load(filepath, &x, &y, &channels, STBI_rgb_alpha);

    if (pixels)
    {
        const VkExtent2D extent = {uint32_t(x), uint32_t(y)};
        const auto result = loadFromMemory(device, cmdPool, format, extent, pixels);
        stbi_image_free(pixels);
        return result;
    }
    else
    {
        loadFallbackTexture(device, cmdPool);
        return CoreResult::Source_Missing;
    }
}

void texture2D::loadFallbackTexture(const vulkan_device *device,
                                    VkCommandPool cmdPool)
{
    auto src = TEX2D_DEFAULT;
    auto format = VK_FORMAT_R8G8B8A8_SRGB;
    m_extent = {1, 1};
    m_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_mipLevels = 1;

    auto transferInfo = vkInits::bufferCreateInfo(sizeof(TEX2D_DEFAULT), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    auto transfer = buffer_t();
    transfer.create(device, &transferInfo, MEM_FLAG_HOST_VISIBLE, src);

    // Create target image resource

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

    auto viewInfo = vkInits::imageViewCreateInfo();
    viewInfo.image = m_image;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = m_mipLevels;
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

    updateDescriptor();
}
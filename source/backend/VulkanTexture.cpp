#include "VulkanTexture.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "../../vendor/stb/stb_image.h"

void Texture::destroy(VkDevice device)
{
    vkDestroySampler(device, m_sampler, nullptr);
    vkFreeMemory(device, m_memory, nullptr);
    vkDestroyImageView(device, m_view, nullptr);
    vkDestroyImage(device, m_image, nullptr);
}

void Texture::insertMemoryBarrier(VkCommandBuffer cmd,
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

void Texture::setImageLayout(VkCommandBuffer cmd,
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

void Texture::updateDescriptor()
{
    descriptor.imageLayout = m_layout;
    descriptor.imageView = m_view;
    descriptor.sampler = m_sampler;
}

CoreResult Texture2D::loadFromMemory(const VulkanDevice *device,
                                     VkQueue queue,
                                     VkFormat format,
                                     VkExtent2D extent,
                                     size_t channels,
                                     const void *src)
{
    m_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_extent = extent;
    m_mipLevels = 1;

    const auto size = channels * m_extent.width * m_extent.height;
    auto transferInfo = vkInits::bufferCreateInfo(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    auto transfer = VulkanBuffer();
    transfer.create(device, &transferInfo, MEM_FLAG_HOST_VISIBLE, src);

    auto imageInfo = vkInits::imageCreateInfo();
    imageInfo.extent = {m_extent.width, m_extent.height, 1};
    imageInfo.format = format;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.mipLevels = m_mipLevels;
    vkCreateImage(device->device, &imageInfo, nullptr, &m_image);

    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(device->device, m_image, &memReqs);

    auto allocInfo = device->getMemoryAllocInfo(memReqs, MEM_FLAG_GPU_LOCAL);
    vkAllocateMemory(device->device, &allocInfo, nullptr, &m_memory);
    vkBindImageMemory(device->device, m_image, m_memory, 0);

    auto command = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);

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

    device->flushCommandBuffer(command, queue);

    transfer.destroy(device->device);

    auto viewInfo = vkInits::imageViewCreateInfo();
    viewInfo.image = m_image;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = m_mipLevels;
    vkCreateImageView(device->device, &viewInfo, nullptr, &m_view);

    auto samplerInfo = vkInits::samplerCreateInfo();
    vkCreateSampler(device->device, &samplerInfo, nullptr, &m_sampler);

    updateDescriptor();

    return CoreResult::Success;
}

CoreResult Texture2D::loadAddMipMap(const VulkanDevice *device,
                                    VkQueue queue,
                                    VkFormat format,
                                    VkExtent2D extent,
                                    size_t channels,
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

    const auto size = channels * m_extent.width * m_extent.height;
    auto transferInfo = vkInits::bufferCreateInfo(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    auto transfer = VulkanBuffer();
    transfer.create(device, &transferInfo, MEM_FLAG_HOST_VISIBLE, src);

    auto imageInfo = vkInits::imageCreateInfo();
    imageInfo.extent = {m_extent.width, m_extent.height, 1};
    imageInfo.format = format;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                      VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.mipLevels = m_mipLevels;
    vkCreateImage(device->device, &imageInfo, nullptr, &m_image);

    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(device->device, m_image, &memReqs);

    auto allocInfo = device->getMemoryAllocInfo(memReqs, MEM_FLAG_GPU_LOCAL);
    vkAllocateMemory(device->device, &allocInfo, nullptr, &m_memory);
    vkBindImageMemory(device->device, m_image, m_memory, 0);

    // Create & prepare base mip level

    auto copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);

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

    device->flushCommandBuffer(copyCmd, queue);
    transfer.destroy(device->device);

    auto blitCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);

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

    device->flushCommandBuffer(blitCmd, queue);
    m_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    auto viewInfo = vkInits::imageViewCreateInfo();
    viewInfo.image = m_image;
    viewInfo.format = format;
    viewInfo.subresourceRange = subResourceRange;
    vkCreateImageView(device->device, &viewInfo, nullptr, &m_view);

    auto samplerInfo = vkInits::samplerCreateInfo(float(m_mipLevels));
    vkCreateSampler(device->device, &samplerInfo, nullptr, &m_sampler);

    updateDescriptor();
    return CoreResult::Success;
}

CoreResult Texture2D::loadRGBA(const VulkanDevice *device,
                               VkQueue queue,
                               const char *filepath,
                               bool genMipMap)
{
    int x, y, channels;
    auto pixels = stbi_load(filepath, &x, &y, &channels, STBI_rgb_alpha);

    if (pixels)
    {
        const auto format = VK_FORMAT_R8G8B8A8_SRGB;
        const VkExtent2D extent = {uint32_t(x), uint32_t(y)};

        auto result = CoreResult::Success;
        if(genMipMap)
            result = loadAddMipMap(device, queue, format, extent, 4, pixels);
        else
            result = loadFromMemory(device, queue, format, extent, 4, pixels);

        stbi_image_free(pixels);
        return result;
    }
    else
    {
        loadDefault(device, queue);
        return CoreResult::Source_Missing;
    }
}

void Texture2D::loadDefault(const VulkanDevice *device, VkQueue queue)
{
    auto src = TEX2D_DEFAULT;
    auto format = VK_FORMAT_R8G8B8A8_SRGB;
    m_extent = {1, 1};
    m_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_mipLevels = 1;

    auto transferInfo = vkInits::bufferCreateInfo(sizeof(TEX2D_DEFAULT), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    auto transfer = VulkanBuffer();
    transfer.create(device, &transferInfo, MEM_FLAG_HOST_VISIBLE, src);

    auto imageInfo = vkInits::imageCreateInfo();
    imageInfo.extent = {m_extent.width, m_extent.height, 1};
    imageInfo.format = format;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.mipLevels = m_mipLevels;
    vkCreateImage(device->device, &imageInfo, nullptr, &m_image);

    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(device->device, m_image, &memReqs);

    auto allocInfo = device->getMemoryAllocInfo(memReqs, MEM_FLAG_GPU_LOCAL);
    vkAllocateMemory(device->device, &allocInfo, nullptr, &m_memory);
    vkBindImageMemory(device->device, m_image, m_memory, 0);

    auto command = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);

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

    device->flushCommandBuffer(command, queue);

    transfer.destroy(device->device);

    auto viewInfo = vkInits::imageViewCreateInfo();
    viewInfo.image = m_image;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = m_mipLevels;
    vkCreateImageView(device->device, &viewInfo, nullptr, &m_view);

    auto samplerInfo = vkInits::samplerCreateInfo();
    vkCreateSampler(device->device, &samplerInfo, nullptr, &m_sampler);

    updateDescriptor();
}

void TextureCubeMap::load(const VulkanDevice *device, VkQueue queue)
{
    m_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_mipLevels = 1;

    int x, y, channels;
    auto pixels = stbi_load(filenames[0], &x, &y, &channels, STBI_rgb_alpha);

    const auto faceSize = 4 * x * y;
    const auto size = LAYER_COUNT * faceSize;

    auto transfer = VulkanBuffer();
    auto bufferInfo = vkInits::bufferCreateInfo(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    transfer.create(device, &bufferInfo, MEM_FLAG_HOST_VISIBLE);

    void *data = nullptr;
    vkMapMemory(device->device, transfer.memory, 0, faceSize, 0, &data);
    memcpy(data, pixels, faceSize);
    vkUnmapMemory(device->device, transfer.memory);

    stbi_image_free(pixels);

    for (uint32_t layer = 1; layer < LAYER_COUNT; layer++)
    {
        pixels = stbi_load(filenames[layer], &x, &y, &channels, STBI_rgb_alpha);

        auto offset = layer * faceSize;
        vkMapMemory(device->device, transfer.memory, offset, faceSize, 0, &data);
        memcpy(data, pixels, faceSize);
        vkUnmapMemory(device->device, transfer.memory);

        stbi_image_free(pixels);
    }

    VkBufferImageCopy bufferImageCopies[LAYER_COUNT] = {};

    for (uint32_t layer = 0; layer < LAYER_COUNT; layer++)
    {
        bufferImageCopies[layer].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        bufferImageCopies[layer].imageSubresource.mipLevel = 0;
        bufferImageCopies[layer].imageSubresource.baseArrayLayer = layer;
        bufferImageCopies[layer].imageSubresource.layerCount = 1;
        bufferImageCopies[layer].imageExtent = {uint32_t(x), uint32_t(y), 1};
        bufferImageCopies[layer].bufferOffset = layer * faceSize;
    }

    auto imageInfo = vkInits::imageCreateInfo();
    imageInfo.extent = {uint32_t(x), uint32_t(y), 1};
    imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.mipLevels = m_mipLevels;
    imageInfo.arrayLayers = LAYER_COUNT;
    imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    vkCreateImage(device->device, &imageInfo, nullptr, &m_image);

    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(device->device, m_image, &memReqs);

    auto allocInfo = device->getMemoryAllocInfo(memReqs, MEM_FLAG_GPU_LOCAL);
    vkAllocateMemory(device->device, &allocInfo, nullptr, &m_memory);
    vkBindImageMemory(device->device, m_image, m_memory, 0);

    auto cmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    VkImageSubresourceRange subResourceRange{};
    subResourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subResourceRange.layerCount = LAYER_COUNT;
    subResourceRange.levelCount = m_mipLevels;

    insertMemoryBarrier(cmd,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_ACCESS_NONE,
                        VK_ACCESS_TRANSFER_WRITE_BIT,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        subResourceRange);

    vkCmdCopyBufferToImage(cmd, transfer.data, m_image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           uint32_t(arraysize(bufferImageCopies)),
                           bufferImageCopies);

    insertMemoryBarrier(cmd,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_ACCESS_TRANSFER_WRITE_BIT,
                        VK_ACCESS_SHADER_READ_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        subResourceRange);

    device->flushCommandBuffer(cmd, queue);

    transfer.destroy(device->device);

    auto samplerInfo = vkInits::samplerCreateInfo();
    vkCreateSampler(device->device, &samplerInfo, nullptr, &m_sampler);

    auto viewInfo = vkInits::imageViewCreateInfo();
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewInfo.image = m_image;
    viewInfo.format = imageInfo.format;
    viewInfo.subresourceRange = subResourceRange;
    vkCreateImageView(device->device, &viewInfo, nullptr, &m_view);

    m_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    updateDescriptor();
}
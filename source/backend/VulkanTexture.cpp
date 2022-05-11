#include "VulkanTexture.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "../../vendor/stb/stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION

void Texture::destroy(VkDevice device)
{
    vkDestroySampler(device, m_sampler, nullptr);
    vkFreeMemory(device, m_memory, nullptr);
    vkDestroyImageView(device, m_view, nullptr);
    vkDestroyImage(device, m_image, nullptr);
}

void Texture::updateDescriptor()
{
    descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
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
    m_extent = extent;
    m_mipLevels = 1;

    VulkanBuffer transfer;
    device->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MEM_FLAG_HOST_VISIBLE,
                         channels * m_extent.width * m_extent.height,
                         transfer, src);

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

    vkTools::SetImageLayout(command,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            m_image);

    const auto copyRegion = vkInits::bufferImageCopy(m_extent);
    vkCmdCopyBufferToImage(command, transfer.data, m_image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    vkTools::SetImageLayout(command,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                            m_image);

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
    m_mipLevels = 1 + uint32_t(std::floor(std::log2(max(m_extent.width, m_extent.height))));

    // Transfer buffer

    VulkanBuffer transfer;
    device->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MEM_FLAG_HOST_VISIBLE,
                         channels * m_extent.width * m_extent.height,
                         transfer, src);

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

    vkTools::InsertMemoryarrier(copyCmd,
                                VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_ACCESS_NONE,
                                VK_ACCESS_TRANSFER_WRITE_BIT,
                                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                subResourceRange,
                                m_image);

    const auto copyRegion = vkInits::bufferImageCopy(m_extent);
    vkCmdCopyBufferToImage(copyCmd, transfer.data, m_image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    vkTools::InsertMemoryarrier(copyCmd,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                VK_ACCESS_TRANSFER_WRITE_BIT,
                                VK_ACCESS_TRANSFER_READ_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                subResourceRange,
                                m_image);

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

        vkTools::InsertMemoryarrier(blitCmd,
                                    VK_IMAGE_LAYOUT_UNDEFINED,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    VK_ACCESS_NONE,
                                    VK_ACCESS_TRANSFER_WRITE_BIT,
                                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                                    dstSubRange, m_image);

        vkCmdBlitImage(blitCmd,
                        m_image,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        m_image,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        1,
                        &blit,
                        VK_FILTER_LINEAR);

        vkTools::InsertMemoryarrier(blitCmd,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                    VK_ACCESS_TRANSFER_WRITE_BIT,
                                    VK_ACCESS_TRANSFER_READ_BIT,
                                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                                    dstSubRange, m_image);
    }

    subResourceRange.levelCount = m_mipLevels;

    vkTools::InsertMemoryarrier(blitCmd,
                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_ACCESS_TRANSFER_READ_BIT,
                                VK_ACCESS_SHADER_READ_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                subResourceRange, m_image);

    device->flushCommandBuffer(blitCmd, queue);

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
    m_mipLevels = 1;

    VulkanBuffer transfer;
    device->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MEM_FLAG_HOST_VISIBLE,
                         sizeof(TEX2D_DEFAULT), transfer, src);

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

    vkTools::SetImageLayout(command,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            m_image);

    const auto copyRegion = vkInits::bufferImageCopy(m_extent);
    vkCmdCopyBufferToImage(command, transfer.data, m_image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    vkTools::SetImageLayout(command,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                            m_image);

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

void TextureBase::destroy(VkDevice device)
{
    vkDestroySampler(device, sampler, nullptr);
    vkFreeMemory(device, memory, nullptr);
    vkDestroyImageView(device, view, nullptr);
    vkDestroyImage(device, image, nullptr);
}

void TextureBase::updateDescriptor()
{
    descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    descriptor.imageView = view;
    descriptor.sampler = sampler;
}

CoreResult HDRImage::load(const VulkanDevice *device, VkQueue queue, const char *filename)
{
    int x = 0, y = 0, channels = 0;
    auto pixels = stbi_loadf(filename, &x, &y, &channels, 4);

    format = VK_FORMAT_R32G32B32A32_SFLOAT;
    extent = {uint32_t(x), uint32_t(y)};
    mipLevels = 1;

    if(pixels != nullptr)
    {
        VulkanBuffer transfer;
        device->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MEM_FLAG_HOST_VISIBLE,
                             16 * extent.width * extent.height,
                             transfer, pixels);

        stbi_image_free(pixels);

        auto imageInfo = vkInits::imageCreateInfo();
        imageInfo.extent.width = extent.width;
        imageInfo.extent.height = extent.height;
        imageInfo.format = format;
        imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                          VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.mipLevels = mipLevels;
        imageInfo.arrayLayers = 1;
        vkCreateImage(device->device, &imageInfo, nullptr, &image);

        VkMemoryRequirements memReqs{};
        vkGetImageMemoryRequirements(device->device, image, &memReqs);

        auto allocInfo = device->getMemoryAllocInfo(memReqs, MEM_FLAG_GPU_LOCAL);
        vkAllocateMemory(device->device, &allocInfo, nullptr, &memory);
        vkBindImageMemory(device->device, image, memory, 0);

        auto samplerInfo = vkInits::samplerCreateInfo();
        vkCreateSampler(device->device, &samplerInfo, nullptr, &sampler);

        auto viewInfo = vkInits::imageViewCreateInfo();
        viewInfo.image = image;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = mipLevels;
        vkCreateImageView(device->device, &viewInfo, nullptr, &view);

        auto cmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        vkTools::SetImageLayout(cmd,
                                VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                image);

        const auto bufferCopy = vkInits::bufferImageCopy(extent);
        vkCmdCopyBufferToImage(cmd, transfer.data, image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               1, &bufferCopy);

        vkTools::SetImageLayout(cmd,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                image);

        device->flushCommandBuffer(cmd, queue);

        transfer.destroy(device->device);

        updateDescriptor();

        return CoreResult::Success;
    }

    return CoreResult::Source_Missing;
}

void TextureCubeMap2::prepare(const VulkanDevice *device)
{
    auto imageInfo = vkInits::imageCreateInfo();
    imageInfo.format = format;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.extent = {extent.width, extent.height, 1};
    imageInfo.arrayLayers = 6;
    imageInfo.mipLevels = mipLevels;
    imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    vkCreateImage(device->device, &imageInfo, nullptr, &image);

    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(device->device, image, &memReqs);

    auto allocInfo = device->getMemoryAllocInfo(memReqs, MEM_FLAG_GPU_LOCAL);
    vkAllocateMemory(device->device, &allocInfo, nullptr, &memory);
    vkBindImageMemory(device->device, image, memory, 0);

    auto samplerInfo = vkInits::samplerCreateInfo(float(mipLevels));
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    vkCreateSampler(device->device, &samplerInfo, nullptr, &sampler);

    auto viewInfo = vkInits::imageViewCreateInfo();
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewInfo.image = image;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.layerCount = 6;
    vkCreateImageView(device->device, &viewInfo, nullptr, &view);

    updateDescriptor();
}

void TextureCubeMap::load(const VulkanDevice *device, VkQueue queue,
                          const char *(&files)[LAYER_COUNT])
{
    m_mipLevels = 1;

    int x, y, channels;
    auto pixels = stbi_load(files[0], &x, &y, &channels, STBI_rgb_alpha);

    const auto faceSize = 4 * x * y;

    VulkanBuffer transfer;
    device->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         MEM_FLAG_HOST_VISIBLE,
                         LAYER_COUNT * faceSize,
                         transfer);

    transfer.map(device->device, 0, faceSize);
    memcpy(transfer.mapped, pixels, faceSize);
    transfer.unmap(device->device);

    stbi_image_free(pixels);

    for (uint32_t layer = 1; layer < LAYER_COUNT; layer++)
    {
        pixels = stbi_load(files[layer], &x, &y, &channels, STBI_rgb_alpha);

        transfer.map(device->device, layer * faceSize, faceSize);
        memcpy(transfer.mapped, pixels, faceSize);
        transfer.unmap(device->device);

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

    vkTools::InsertMemoryarrier(cmd,
                                VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_ACCESS_NONE,
                                VK_ACCESS_TRANSFER_WRITE_BIT,
                                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                subResourceRange, m_image);

    vkCmdCopyBufferToImage(cmd, transfer.data, m_image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           LAYER_COUNT, bufferImageCopies);

    vkTools::InsertMemoryarrier(cmd,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_ACCESS_TRANSFER_WRITE_BIT,
                                VK_ACCESS_SHADER_READ_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                subResourceRange, m_image);

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

    updateDescriptor();
}
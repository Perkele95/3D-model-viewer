#include "VulkanTexture.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "../../vendor/stb/stb_image.h"
#undef STB_IMAGE_IMPLE

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

ImageFile Texture2D::loadFile(const char *filename, VkFormat format, uint32_t reqComp)
{
    ImageFile file;
    int x = 0, y = 0, channels = 0;
    auto pixels = stbi_load(filename, &x, &y, &channels, reqComp);

    if(pixels == nullptr)
    {
        file.pixels = const_cast<uint8_t*>(TEX2D_DEFAULT);
        file.extent = {1, 1};
        file.bytesPerPixel = 4;
        file.format = VK_FORMAT_R8G8B8A8_SRGB;
        file.heapFreeFlag = false;
    }
    else
    {
        file.pixels = pixels;
        file.extent = {static_cast<uint32_t>(x), static_cast<uint32_t>(y)};
        file.bytesPerPixel = reqComp;
        file.format = format;
        file.heapFreeFlag = true;
    }

    return file;
}

void Texture2D::freeFile(ImageFile &file)
{
    if(file.heapFreeFlag)
        stbi_image_free(file.pixels);
    file.pixels = nullptr;
}

void Texture2D::create(const VulkanDevice *device,
                              VkQueue queue,
                              const ImageFile &file,
                              bool genMipMaps)
{
    extent = file.extent;
    format = file.format;
    mipLevels = 1;

    VulkanBuffer transfer;
    device->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MEM_FLAG_HOST_VISIBLE,
                         file.bytesPerPixel * extent.width * extent.height,
                         transfer, file.pixels);

    if(genMipMaps && device->linearFilterSupport(format, VK_IMAGE_TILING_OPTIMAL))
    {
        mipLevels = 1 + uint32_t(std::floor(std::log2(max(extent.width, extent.height))));

        auto imageInfo = vkInits::imageCreateInfo();
        imageInfo.extent = {extent.width, extent.height, 1};
        imageInfo.format = format;
        imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                          VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.mipLevels = mipLevels;
        vkCreateImage(device->device, &imageInfo, nullptr, &image);

        VkMemoryRequirements memReqs{};
        vkGetImageMemoryRequirements(device->device, image, &memReqs);

        auto allocInfo = device->getMemoryAllocInfo(memReqs, MEM_FLAG_GPU_LOCAL);
        vkAllocateMemory(device->device, &allocInfo, nullptr, &memory);
        vkBindImageMemory(device->device, image, memory, 0);

        VkImageSubresourceRange subResourceRange{};
        subResourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subResourceRange.baseArrayLayer = 0;
        subResourceRange.baseMipLevel = 0;
        subResourceRange.layerCount = 1;
        subResourceRange.levelCount = 1;

        auto command = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        vkTools::InsertMemoryarrier(command,
                                    VK_IMAGE_LAYOUT_UNDEFINED,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    VK_ACCESS_NONE,
                                    VK_ACCESS_TRANSFER_WRITE_BIT,
                                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                                    subResourceRange,
                                    image);

        const auto copyRegion = vkInits::bufferImageCopy(extent);
        vkCmdCopyBufferToImage(command, transfer.data, image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        vkTools::InsertMemoryarrier(command,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                    VK_ACCESS_TRANSFER_WRITE_BIT,
                                    VK_ACCESS_TRANSFER_READ_BIT,
                                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                                    subResourceRange,
                                    image);

        device->flushCommandBuffer(command, queue);
        command = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        for (uint32_t level = 1; level < mipLevels; level++)
        {
            VkImageBlit blit{};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.layerCount = 1;
            blit.srcSubresource.mipLevel = level - 1;
            blit.srcOffsets[0] = {};
            blit.srcOffsets[1].x = int32_t(extent.width >> (level - 1));
            blit.srcOffsets[1].y = int32_t(extent.height >> (level - 1));
            blit.srcOffsets[1].z = 1;

            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.layerCount = 1;
            blit.dstSubresource.mipLevel = level;
            blit.dstOffsets[0] = {};
            blit.dstOffsets[1].x = int32_t(extent.width >> level);
            blit.dstOffsets[1].y = int32_t(extent.height >> level);
            blit.dstOffsets[1].z = 1;

            VkImageSubresourceRange dstSubRange{};
            dstSubRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            dstSubRange.baseMipLevel = level;
            dstSubRange.layerCount = 1;
            dstSubRange.levelCount = 1;

            vkTools::InsertMemoryarrier(command,
                                        VK_IMAGE_LAYOUT_UNDEFINED,
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                        VK_ACCESS_NONE,
                                        VK_ACCESS_TRANSFER_WRITE_BIT,
                                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                                        dstSubRange, image);

            vkCmdBlitImage(command,
                           image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &blit,
                           VK_FILTER_LINEAR);

            vkTools::InsertMemoryarrier(command,
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                        VK_ACCESS_TRANSFER_WRITE_BIT,
                                        VK_ACCESS_TRANSFER_READ_BIT,
                                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                                        dstSubRange, image);
        }

        subResourceRange.levelCount = mipLevels;
        vkTools::InsertMemoryarrier(command,
                                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                    VK_ACCESS_TRANSFER_READ_BIT,
                                    VK_ACCESS_SHADER_READ_BIT,
                                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                    subResourceRange, image);

        device->flushCommandBuffer(command, queue);
    }
    else
    {
        auto imageInfo = vkInits::imageCreateInfo();
        imageInfo.extent = {extent.width, extent.height, 1};
        imageInfo.format = format;
        imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.mipLevels = mipLevels;
        vkCreateImage(device->device, &imageInfo, nullptr, &image);

        VkMemoryRequirements memReqs{};
        vkGetImageMemoryRequirements(device->device, image, &memReqs);

        auto allocInfo = device->getMemoryAllocInfo(memReqs, MEM_FLAG_GPU_LOCAL);
        vkAllocateMemory(device->device, &allocInfo, nullptr, &memory);
        vkBindImageMemory(device->device, image, memory, 0);

        auto command = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        vkTools::SetImageLayout(command,
                                VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                image);

        const auto copyRegion = vkInits::bufferImageCopy(extent);
        vkCmdCopyBufferToImage(command, transfer.data, image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        vkTools::SetImageLayout(command,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                image);

        device->flushCommandBuffer(command, queue);
    }

    transfer.destroy(device->device);

    auto viewInfo = vkInits::imageViewCreateInfo();
    viewInfo.image = image;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = mipLevels;
    vkCreateImageView(device->device, &viewInfo, nullptr, &view);

    auto samplerInfo = vkInits::samplerCreateInfo();
    vkCreateSampler(device->device, &samplerInfo, nullptr, &sampler);

    updateDescriptor();
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

void TextureCubeMap::prepare(const VulkanDevice *device)
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
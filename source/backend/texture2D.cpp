#include "texture2D.hpp"

texture2D::texture2D(const vulkan_device *device, VkCommandPool cmdPool, const texture2D_create_info *pInfo)
{
    m_extent.width = pInfo->extent.width;
    m_extent.height = pInfo->extent.height;

    auto imageInfo = vkInits::imageCreateInfo();
    imageInfo.extent = {m_extent.width, m_extent.height, 1};
    imageInfo.format = pInfo->format;
    imageInfo.samples = pInfo->samples;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    vkCreateImage(device->device, &imageInfo, nullptr, &m_image);

    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(device->device, m_image, &memReqs);

    auto allocInfo = device->getMemoryAllocInfo(memReqs, MEM_FLAG_GPU_LOCAL);
    vkAllocateMemory(device->device, &allocInfo, nullptr, &m_memory);
    vkBindImageMemory(device->device, m_image, m_memory, 0);

    auto viewInfo = vkInits::imageViewCreateInfo();
    viewInfo.image = m_image;
    viewInfo.format = pInfo->format;
    viewInfo.subresourceRange.aspectMask = pInfo->aspectFlags;
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

    const VkDeviceSize size = m_extent.width * m_extent.height;

    auto transferInfo = vkInits::bufferCreateInfo(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    auto transfer = buffer_t(device, &transferInfo, MEM_FLAG_HOST_VISIBLE);

    transfer.fill(device->device, pInfo->source, size);

    VkCommandBuffer imageCmds[] = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};

    auto cmdBufferAllocInfo = vkInits::commandBufferAllocateInfo(cmdPool, arraysize(imageCmds));
    vkAllocateCommandBuffers(device->device, &cmdBufferAllocInfo, imageCmds);

    auto cmdBufferBeginInfo = vkInits::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    vkBeginCommandBuffer(imageCmds[0], &cmdBufferBeginInfo);
    auto imageBarrier = vkInits::imageMemoryBarrier(m_image,
                                                    VK_IMAGE_LAYOUT_UNDEFINED,
                                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                    VkAccessFlags(0),
                                                    VK_ACCESS_TRANSFER_WRITE_BIT);
    vkCmdPipelineBarrier(imageCmds[0], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &imageBarrier);
    vkEndCommandBuffer(imageCmds[0]);

    vkBeginCommandBuffer(imageCmds[1], &cmdBufferBeginInfo);

    auto cpyRegion = vkInits::bufferImageCopy(m_extent);
    vkCmdCopyBufferToImage(imageCmds[1], transfer.data, m_image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cpyRegion);

    vkEndCommandBuffer(imageCmds[1]);

    vkBeginCommandBuffer(imageCmds[2], &cmdBufferBeginInfo);
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(imageCmds[2], VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                        nullptr, 0, nullptr, 1, &imageBarrier);
    vkEndCommandBuffer(imageCmds[2]);

    auto queueSubmitInfo = vkInits::submitInfo(imageCmds, arraysize(imageCmds));
    vkQueueSubmit(device->graphics.queue, 1, &queueSubmitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(device->graphics.queue);

    vkFreeCommandBuffers(device->device, cmdPool, uint32_t(arraysize(imageCmds)), imageCmds);

    transfer.destroy(device->device);
}

void texture2D::destroy(VkDevice device)
{
    vkDestroySampler(device, m_sampler, nullptr);
    vkFreeMemory(device, m_memory, nullptr);
    vkDestroyImageView(device, m_view, nullptr);
    vkDestroyImage(device, m_image, nullptr);
}

VkDescriptorImageInfo texture2D::descriptor()
{
    VkDescriptorImageInfo desc;
    desc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    desc.imageView = m_view;
    desc.sampler = m_sampler;
    return desc;
}
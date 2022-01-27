#include "buffer.hpp"

image_buffer::image_buffer(const vulkan_device *device, const VkImageCreateInfo *pCreateInfo,
                           VkImageAspectFlags aspect)
{
    vkCreateImage(device->device, pCreateInfo, nullptr, &m_image);
    m_extent.width = pCreateInfo->extent.width;
    m_extent.height = pCreateInfo->extent.height;

    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(device->device, m_image, &memReqs);

    auto allocInfo = device->getMemoryAllocInfo(memReqs, MEM_FLAG_GPU_LOCAL);
    vkAllocateMemory(device->device, &allocInfo, nullptr, &m_memory);
    vkBindImageMemory(device->device, m_image, m_memory, 0);

    auto viewInfo = vkInits::imageViewCreateInfo();
    viewInfo.image = m_image;
    viewInfo.format = pCreateInfo->format;
    viewInfo.subresourceRange.aspectMask = aspect;
    vkCreateImageView(device->device, &viewInfo, nullptr, &view);
}

void image_buffer::destroy(VkDevice device)
{
    vkDestroyImage(device, m_image, nullptr);
    vkDestroyImageView(device, view, nullptr);
    vkFreeMemory(device, m_memory, nullptr);
    m_image = VK_NULL_HANDLE;
    m_memory = VK_NULL_HANDLE;
    view = VK_NULL_HANDLE;
}

VkDescriptorImageInfo image_buffer::descriptor(VkSampler sampler)
{
    VkDescriptorImageInfo info;
    info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    info.imageView = view;
    info.sampler = sampler;
    return info;
}

VkResult image_buffer::copyFromBuffer(const vulkan_device *device, VkCommandPool cmdPool, buffer_t *pSrc)
{
    const VkDeviceSize size = m_extent.width * m_extent.height;
    if(size < pSrc->size)
        return VK_ERROR_MEMORY_MAP_FAILED;

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

    pSrc->copyToImage(imageCmds[1], this, m_extent);

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

    return VK_SUCCESS;
}

buffer_t::buffer_t(const vulkan_device *device, const VkBufferCreateInfo *pCreateInfo,
                   VkMemoryPropertyFlags memFlags)
                   : size(pCreateInfo->size)
{
    vkCreateBuffer(device->device, pCreateInfo, nullptr, &this->data);

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device->device, this->data, &memReqs);

    auto allocInfo = device->getMemoryAllocInfo(memReqs, memFlags);
    vkAllocateMemory(device->device, &allocInfo, nullptr, &this->memory);
    vkBindBufferMemory(device->device, this->data, this->memory, 0);
}

void buffer_t::destroy(VkDevice device)
{
    vkDestroyBuffer(device, this->data, nullptr);
    vkFreeMemory(device, this->memory, nullptr);
    this->data = VK_NULL_HANDLE;
    this->memory = VK_NULL_HANDLE;
    this->size = 0;
}

VkDescriptorBufferInfo buffer_t::descriptor(VkDeviceSize offset)
{
    VkDescriptorBufferInfo desc{};
    desc.buffer = this->data;
    desc.range = this->size;
    desc.offset = offset;
    return desc;
}

VkResult buffer_t::fill(VkDevice device, const void *src, size_t sz)
{
    void *mapped = nullptr;
    const auto result = vkMapMemory(device, this->memory, 0, sz, 0, &mapped);
    memcpy(mapped, src, sz);
    vkUnmapMemory(device, this->memory);
    return result;
}

void buffer_t::copyToBuffer(VkCommandBuffer cmd, buffer_t *pDst)
{
    auto cmdBegin = vkInits::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    vkBeginCommandBuffer(cmd, &cmdBegin);

    VkBufferCopy cpyRegion{};
    cpyRegion.dstOffset = 0;
    cpyRegion.srcOffset = 0;
    cpyRegion.size = this->size;
    vkCmdCopyBuffer(cmd, this->data, pDst->data, 1, &cpyRegion);

    vkEndCommandBuffer(cmd);
}

void buffer_t::copyToImage(VkCommandBuffer cmd, image_buffer *pDst, VkExtent2D extent)
{
    auto cmdBegin = vkInits::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    vkBeginCommandBuffer(cmd, &cmdBegin);

    auto cpyRegion = vkInits::bufferImageCopy(extent);
    vkCmdCopyBufferToImage(cmd, this->data, pDst->m_image,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cpyRegion);

    vkEndCommandBuffer(cmd);
}
#include "buffer.hpp"

void image_buffer::create(const vulkan_device *device,
                          const VkImageCreateInfo *pCreateInfo,
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
    vkCreateImageView(device->device, &viewInfo, nullptr, &m_view);
}

void image_buffer::destroy(VkDevice device)
{
    vkDestroyImage(device, m_image, nullptr);
    vkDestroyImageView(device, m_view, nullptr);
    vkFreeMemory(device, m_memory, nullptr);
    m_image = VK_NULL_HANDLE;
    m_memory = VK_NULL_HANDLE;
    m_view = VK_NULL_HANDLE;
}

VkDescriptorImageInfo image_buffer::descriptor(VkSampler sampler)
{
    VkDescriptorImageInfo info;
    info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    info.imageView = m_view;
    info.sampler = sampler;
    return info;
}

void buffer_t::create(const vulkan_device *device,
                      const VkBufferCreateInfo *pCreateInfo,
                      VkMemoryPropertyFlags memFlags,
                      const void *src)
{
    vkCreateBuffer(device->device, pCreateInfo, nullptr, &this->data);
    this->size = pCreateInfo->size;

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device->device, this->data, &memReqs);

    auto allocInfo = device->getMemoryAllocInfo(memReqs, memFlags);
    vkAllocateMemory(device->device, &allocInfo, nullptr, &this->memory);
    vkBindBufferMemory(device->device, this->data, this->memory, 0);

    if(src != nullptr)
        fill(device->device, src);
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

void buffer_t::copy(VkCommandBuffer cmd, buffer_t *pDst)
{
    const auto copyRegion = vkInits::bufferCopy(this->size);
    vkCmdCopyBuffer(cmd, this->data, pDst->data, 1, &copyRegion);
}

void buffer_t::fill(VkDevice device, const void *src)
{
    void *mapped = nullptr;
    vkMapMemory(device, this->memory, 0, this->size, 0, &mapped);
    memcpy(mapped, src, this->size);
    vkUnmapMemory(device, this->memory);
}
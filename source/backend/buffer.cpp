#include "buffer.hpp"

void VulkanBuffer::create(const VulkanDevice *device,
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

void VulkanBuffer::destroy(VkDevice device)
{
    vkDestroyBuffer(device, this->data, nullptr);
    vkFreeMemory(device, this->memory, nullptr);
    this->data = VK_NULL_HANDLE;
    this->memory = VK_NULL_HANDLE;
    this->size = 0;
}

VkDescriptorBufferInfo VulkanBuffer::descriptor(VkDeviceSize offset)
{
    VkDescriptorBufferInfo desc{};
    desc.buffer = this->data;
    desc.range = this->size;
    desc.offset = offset;
    return desc;
}

void VulkanBuffer::copy(VkCommandBuffer cmd, VulkanBuffer *pDst)
{
    const auto copyRegion = vkInits::bufferCopy(this->size);
    vkCmdCopyBuffer(cmd, this->data, pDst->data, 1, &copyRegion);
}

void VulkanBuffer::fill(VkDevice device, const void *src)
{
    void *mapped = nullptr;
    vkMapMemory(device, this->memory, 0, this->size, 0, &mapped);
    memcpy(mapped, src, this->size);
    vkUnmapMemory(device, this->memory);
}
#pragma once

#include "vulkan_initialisers.hpp"
#include "vulkan_device.hpp"

struct image_buffer
{
    VkResult create(const vulkan_device *device, VkFormat format, VkExtent2D imageExtent,
                    VkImageUsageFlags usage, VkMemoryPropertyFlags flags)
    {
        auto imageInfo = vkInits::imageCreateInfo();
        imageInfo.extent = {imageExtent.width, imageExtent.height, 1};
        imageInfo.format = format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = usage;
        vkCreateImage(device->device, &imageInfo, nullptr, &this->image);

        VkMemoryRequirements memReqs{};
        vkGetImageMemoryRequirements(device->device, this->image, &memReqs);

        auto allocInfo = device->getMemoryAllocInfo(memReqs, flags);
        vkAllocateMemory(device->device, &allocInfo, nullptr, &this->memory);
        return vkBindImageMemory(device->device, this->image, this->memory, 0);
    }

    void destroy(VkDevice device)
    {
        vkDestroyImage(device, this->image, nullptr);
        vkDestroyImageView(device, this->view, nullptr);
        vkFreeMemory(device, this->memory, nullptr);
        this->image = VK_NULL_HANDLE;
        this->view = VK_NULL_HANDLE;
        this->memory = VK_NULL_HANDLE;
    }

    VkImage image;
    VkImageView view;
    VkDeviceMemory memory;
};

struct buffer_t
{
    buffer_t() = default;
    buffer_t(size_t bufferSize) : size(bufferSize) {}

    VkResult create(const vulkan_device *device, VkBufferUsageFlags usage,
                    VkMemoryPropertyFlags memFlags)
    {
        auto bufferInfo = vkInits::bufferCreateInfo(this->size, usage);
        vkCreateBuffer(device->device, &bufferInfo, nullptr, &this->data);

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device->device, this->data, &memReqs);

        auto allocInfo = device->getMemoryAllocInfo(memReqs, memFlags);
        vkAllocateMemory(device->device, &allocInfo, nullptr, &this->memory);
        return vkBindBufferMemory(device->device, this->data, this->memory, 0);
    }

    void destroy(VkDevice device)
    {
        vkDestroyBuffer(device, this->data, nullptr);
        vkFreeMemory(device, this->memory, nullptr);
        this->data = VK_NULL_HANDLE;
        this->memory = VK_NULL_HANDLE;
        this->size = 0;
    }

    VkDescriptorBufferInfo descriptor(VkDeviceSize offset)
    {
        VkDescriptorBufferInfo desc{};
        desc.buffer = this->data;
        desc.range = this->size;
        desc.offset = offset;
        return desc;
    }

    VkResult fill(VkDevice device, const void *src, size_t sz)
    {
        void *mapped = nullptr;
        const auto result = vkMapMemory(device, this->memory, 0, sz, 0, &mapped);
        memcpy(mapped, src, sz);
        vkUnmapMemory(device, this->memory);
        return result;
    }

    void copyToBuffer(VkCommandBuffer cmd, buffer_t *pDst)
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

    void copyToImage(VkCommandBuffer cmd, image_buffer *pDst, VkExtent2D extent)
    {
        auto cmdBegin = vkInits::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        vkBeginCommandBuffer(cmd, &cmdBegin);

        auto cpyRegion = vkInits::bufferImageCopy(extent);
        vkCmdCopyBufferToImage(cmd, this->data, pDst->image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cpyRegion);

        vkEndCommandBuffer(cmd);
    }

    VkBuffer data;
    VkDeviceMemory memory;
    VkDeviceSize size;
};
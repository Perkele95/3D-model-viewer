#pragma once

#include "vulkan_initialisers.hpp"

struct image_buffer
{
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

    buffer_t(VkBufferUsageFlags usage, VkMemoryPropertyFlags mem, VkDeviceSize sz)
    {
        usageFlags = usage;
        memFlags = mem;
        size = sz;
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

    VkBufferUsageFlags usageFlags;
    VkMemoryPropertyFlags memFlags;
};
#pragma once

#include "vulkan_initialisers.hpp"
#include <string.h>

struct buffer_t
{
    void destroy(VkDevice device)
    {
        vkDestroyBuffer(device, this->data, nullptr);
        vkFreeMemory(device, this->memory, nullptr);
        this->data = VK_NULL_HANDLE;
        this->memory = VK_NULL_HANDLE;
        this->size = 0;
    }

    VkBuffer data;
    VkDeviceMemory memory;
    VkDeviceSize size;
};

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

#define TOOLS_API inline auto [[nodiscard]]

namespace vkTools
{
    TOOLS_API GetMemoryAllocInfo(VkPhysicalDevice gpu, VkMemoryRequirements memReqs,
                                 VkMemoryPropertyFlags flags)
    {
        VkPhysicalDeviceMemoryProperties memProps{};
        vkGetPhysicalDeviceMemoryProperties(gpu, &memProps);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = 0;

        for (uint32_t i = 0; i < memProps.memoryTypeCount; i++){
            if((memReqs.memoryTypeBits & BIT(i)) && (memProps.memoryTypes[i].propertyFlags & flags)){
                allocInfo.memoryTypeIndex = i;
                break;
            }
        }
        return allocInfo;
    }

    TOOLS_API CreateBuffer(VkDevice device, VkPhysicalDevice gpu, VkDeviceSize size,
                           VkBufferUsageFlags usage, VkMemoryPropertyFlags flags, buffer_t *pBuffer)
    {
        pBuffer->size = size;
        auto bufferInfo = vkInits::bufferCreateInfo(size, usage);
        vkCreateBuffer(device, &bufferInfo, nullptr, &pBuffer->data);

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device, pBuffer->data, &memReqs);

        auto allocInfo = vkTools::GetMemoryAllocInfo(gpu, memReqs, flags);
        vkAllocateMemory(device, &allocInfo, nullptr, &pBuffer->memory);
        vkBindBufferMemory(device, pBuffer->data, pBuffer->memory, 0);
    }

    TOOLS_API CreateImageBuffer(VkDevice device, VkPhysicalDevice gpu, VkFormat format, VkExtent2D extent,
                                VkImageUsageFlags usage, VkMemoryPropertyFlags flags, image_buffer *pBuffer)
    {
        auto imageInfo = vkInits::imageCreateInfo();
        imageInfo.extent = {extent.width, extent.height, 1};
        imageInfo.format = format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = usage;
        vkCreateImage(device, &imageInfo, nullptr, &pBuffer->image);

        VkMemoryRequirements memReqs{};
        vkGetImageMemoryRequirements(device, pBuffer->image, &memReqs);

        auto allocInfo = vkTools::GetMemoryAllocInfo(gpu, memReqs, flags);
        vkAllocateMemory(device, &allocInfo, nullptr, &pBuffer->memory);
        vkBindImageMemory(device, pBuffer->image, pBuffer->memory, 0);
    }

    TOOLS_API FillBuffer(VkDevice device, buffer_t *pDst, const void *src, size_t size)
    {
        void *mapped = nullptr;
        vkMapMemory(device, pDst->memory, 0, size, 0, &mapped);
        memcpy(mapped, src, size);
        vkUnmapMemory(device, pDst->memory);
    }

    TOOLS_API CmdCopyBuffer(VkCommandBuffer cpyCmd, buffer_t *pDst, const buffer_t *pSrc)
    {
        vol_assert(pDst->size == pSrc->size)

        auto cmdBegin = vkInits::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        vkBeginCommandBuffer(cpyCmd, &cmdBegin);

        VkBufferCopy cpyRegion{};
        cpyRegion.dstOffset = 0;
        cpyRegion.srcOffset = 0;
        cpyRegion.size = pSrc->size;
        vkCmdCopyBuffer(cpyCmd, pSrc->data, pDst->data, 1, &cpyRegion);

        vkEndCommandBuffer(cpyCmd);
    }

    TOOLS_API CmdCopyBufferToImage(VkCommandBuffer cpyCmd, image_buffer *pDst,
                                   const buffer_t *pSrc, VkExtent2D extent)
    {
        vol_assert((extent.width * extent.height) == pSrc->size)

        auto cmdBegin = vkInits::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        vkBeginCommandBuffer(cpyCmd, &cmdBegin);

        auto cpyRegion = vkInits::bufferImageCopy(extent);
        vkCmdCopyBufferToImage(cpyCmd, pSrc->data, pDst->image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cpyRegion);

        vkEndCommandBuffer(cpyCmd);
    }
}

#undef TOOLS_API
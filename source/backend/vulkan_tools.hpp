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
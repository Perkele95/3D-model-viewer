#pragma once

#include "vulkan_initialisers.hpp"

namespace vkTools
{
#define TOOLS_API inline

    // TODO(arle): getsupporteddepthformat
    // getpresentmodes, surfaceformats, etc

    template<int N>
    TOOLS_API uint32_t descriptorPoolMaxSets(const VkDescriptorPoolSize (&poolSizes)[N])
    {
        uint32_t maxSets = 0;
        for (uint32_t i = 0; i < N; i++)
            maxSets += poolSizes[i].descriptorCount;

        return maxSets;
    }

    TOOLS_API bool LinearFilterSupport(VkPhysicalDevice gpu,
                                       VkFormat format,
                                       VkImageTiling tiling)
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(gpu, format, &props);

        switch (tiling)
        {
            case VK_IMAGE_TILING_LINEAR:
                return props.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
            case VK_IMAGE_TILING_OPTIMAL:
                return props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
            default: return false;
        }
    }

    TOOLS_API void InsertMemoryarrier(VkCommandBuffer cmd,
                                      VkImageLayout oldLayout,
                                      VkImageLayout newLayout,
                                      VkAccessFlags srcAccessMask,
                                      VkAccessFlags dstAccessMask,
                                      VkPipelineStageFlags srcStageMask,
                                      VkPipelineStageFlags dstStageMask,
                                      const VkImageSubresourceRange &subresourceRange,
                                      VkImage image)
    {
        auto barrier = vkInits::imageMemoryBarrier(image, oldLayout, newLayout);
        barrier.srcAccessMask = srcAccessMask;
        barrier.dstAccessMask = dstAccessMask;
        barrier.subresourceRange = subresourceRange;
        vkCmdPipelineBarrier(cmd, srcStageMask, dstStageMask,
                            0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    TOOLS_API void SetImageLayout(VkCommandBuffer cmd,
                                  VkImageLayout oldLayout,
                                  VkImageLayout newLayout,
                                  VkPipelineStageFlags srcStageMask,
                                  VkPipelineStageFlags dstStageMask,
                                  VkImage image,
                                  VkImageSubresourceRange subresourceRange = vkInits::imageSubresourceRange())
    {
        auto imageBarrier = vkInits::imageMemoryBarrier(image, oldLayout, newLayout);
        imageBarrier.subresourceRange = subresourceRange;

        switch (oldLayout)
        {
            case VK_IMAGE_LAYOUT_UNDEFINED:
                imageBarrier.srcAccessMask = VK_ACCESS_NONE;
                break;
            case VK_IMAGE_LAYOUT_PREINITIALIZED:
                imageBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
                break;
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                imageBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                break;
            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                imageBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                break;
            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                break;
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                break;
            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                imageBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
                break;
            default: break;
        }

        switch (newLayout)
        {
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                break;
            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                break;
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                imageBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                break;
            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                imageBarrier.dstAccessMask = imageBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                break;
            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                if (imageBarrier.srcAccessMask == VK_ACCESS_NONE)
                    imageBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
                imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                break;
            default: break;
        }

        vkCmdPipelineBarrier(cmd, srcStageMask, dstStageMask, 0, 0, nullptr,
                             0, nullptr, 1, &imageBarrier);
    }
#undef TOOLS_API
}
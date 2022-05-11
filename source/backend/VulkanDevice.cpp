#include "VulkanDevice.hpp"
#include <string.h>

void VulkanDevice::create(bool validation)
{
    const uint32_t queueFamilies[] = {queueBits.graphics, queueBits.present};
    const uint32_t queueCount = (queueBits.graphics != queueBits.present) ? 2 : 1;

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfos[2];
    for(size_t i = 0; i < queueCount; i++)
    {
        queueCreateInfos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfos[i].queueFamilyIndex = queueFamilies[i];
        queueCreateInfos[i].queueCount = queueCount;
        queueCreateInfos[i].pQueuePriorities = &queuePriority;
        queueCreateInfos[i].flags = 0;
        queueCreateInfos[i].pNext = nullptr;
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.sampleRateShading = VK_TRUE;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = queueCount;
    createInfo.pQueueCreateInfos = queueCreateInfos;
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(arraysize(DeviceExtensions));
    createInfo.ppEnabledExtensionNames = DeviceExtensions;

    if(validation)
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(arraysize(ValidationLayers));
        createInfo.ppEnabledLayerNames = ValidationLayers;
    }

    vkCreateDevice(gpu, &createInfo, nullptr, &device);

    auto poolInfo = vkInits::commandPoolCreateInfo(queueBits.graphics);
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);
}

void VulkanDevice::destroy()
{
    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroyDevice(device, nullptr);
}

VkMemoryAllocateInfo VulkanDevice::getMemoryAllocInfo(VkMemoryRequirements memReqs,
                                                      VkMemoryPropertyFlags flags) const
{
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(gpu, &memProps);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = 0;

    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++)
    {
        if((memReqs.memoryTypeBits & BIT(i)) && (memProps.memoryTypes[i].propertyFlags & flags))
        {
            allocInfo.memoryTypeIndex = i;
            break;
        }
    }
    return allocInfo;
}

VkCommandBuffer VulkanDevice::createCommandBuffer(VkCommandBufferLevel level, bool begin) const
{
    VkCommandBuffer command = VK_NULL_HANDLE;

    auto allocateInfo = vkInits::commandBufferAllocateInfo(commandPool, 1);
    allocateInfo.level = level;
    vkAllocateCommandBuffers(device, &allocateInfo, &command);

    if(begin)
    {
        const auto beginInfo = vkInits::commandBufferBeginInfo(0);
        vkBeginCommandBuffer(command, &beginInfo);
    }
    return command;
}

void VulkanDevice::flushCommandBuffer(VkCommandBuffer command, VkQueue queue, bool free) const
{
    vkEndCommandBuffer(command);

    auto fenceInfo = vkInits::fenceCreateInfo(0);
    VkFence fence = VK_NULL_HANDLE;
    vkCreateFence(device, &fenceInfo, nullptr, &fence);

    auto submitInfo = vkInits::submitInfo(&command, 1);
    vkQueueSubmit(queue, 1, &submitInfo, fence);
    vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(device, fence, nullptr);

    if(free)
        vkFreeCommandBuffers(device, commandPool, 1, &command);
}

void VulkanDevice::createBuffer(VkBufferUsageFlags usage,
                                VkMemoryPropertyFlags memFlags,
                                VkDeviceSize size,
                                VulkanBuffer &buffer,
                                const void *src) const
{
    auto bufferInfo = vkInits::bufferCreateInfo(size, usage);
    vkCreateBuffer(device, &bufferInfo, nullptr, &buffer.data);

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, buffer.data, &memReqs);

    auto allocInfo = getMemoryAllocInfo(memReqs, memFlags);
    vkAllocateMemory(device, &allocInfo, nullptr, &buffer.memory);

    buffer.size = size;
    buffer.bind(device);
    buffer.updateDescriptor();

    if(src != nullptr)
    {
        buffer.map(device);
        memcpy(buffer.mapped, src, size);
        buffer.unmap(device);
    }
}

void VulkanDevice::createOffscreenBuffer(VkFormat format,
                                         VkExtent2D dimension,
                                         VkRenderPass renderPass,
                                         VkQueue queue,
                                         OffscreenBuffer &buffer) const
{
    auto imageInfo = vkInits::imageCreateInfo();
    imageInfo.format = format;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.extent = {dimension.width, dimension.height, 1};
    imageInfo.arrayLayers = 1;
    imageInfo.mipLevels = 1;
    vkCreateImage(device, &imageInfo, nullptr, &buffer.image);

    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(device, buffer.image, &memReqs);

    auto allocInfo = getMemoryAllocInfo(memReqs, MEM_FLAG_GPU_LOCAL);
    vkAllocateMemory(device, &allocInfo, nullptr, &buffer.memory);
    vkBindImageMemory(device, buffer.image, buffer.memory, 0);

    auto viewInfo = vkInits::imageViewCreateInfo();
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.image = buffer.image;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    vkCreateImageView(device, &viewInfo, nullptr, &buffer.view);

    auto framebufferInfo = vkInits::framebufferCreateInfo();
    framebufferInfo.width = dimension.width;
    framebufferInfo.height = dimension.height;
    framebufferInfo.renderPass = renderPass;
    framebufferInfo.pAttachments = &buffer.view;
    framebufferInfo.attachmentCount = 1;
    vkCreateFramebuffer(device, &framebufferInfo, nullptr, &buffer.framebuffer);

    auto cmd = createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    vkTools::SetImageLayout(cmd,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                            buffer.image);
    flushCommandBuffer(cmd, queue);
}
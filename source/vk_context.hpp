#pragma once

#include "base.hpp"
#include "mv_allocator.hpp"
#include "vulkan/vulkan.h"
#include "vk_initialisers.hpp"
#include <string.h>

constexpr char *ValidationLayers[] = {"VK_LAYER_KHRONOS_validation"};
constexpr char *DeviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
constexpr char *RequiredExtensions[] = {"VK_KHR_surface", "VK_KHR_win32_surface"};

constexpr size_t MAX_IMAGES_IN_FLIGHT = 2;
constexpr VkBufferUsageFlags VERTEX_USAGE_DST_FLAGS = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
constexpr VkBufferUsageFlags INDEX_USAGE_DST_FLAGS = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                        VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
constexpr VkMemoryPropertyFlags VISIBLE_BUFFER_FLAGS = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

struct queue_data
{
    VkQueue queue;
    uint32_t family;
};

struct depth_data
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

    VkFormat format;
    VkImage image;
    VkImageView view;
    VkDeviceMemory memory;
};

struct msaa_data
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
    VkSampleCountFlagBits sampleCount;
};

void SetSurface(VkInstance instance, VkSurfaceKHR *pSurface);

struct vulkan_context
{
    vulkan_context(mv_allocator *allocator, bool validation, bool vSync)
    {
        // Instance
        auto appInfo = vkInits::applicationInfo("3D model viewer");
        auto instanceInfo = vkInits::instanceCreateInfo();
        instanceInfo.pApplicationInfo = &appInfo;
        instanceInfo.enabledExtensionCount = uint32_t(arraysize(RequiredExtensions));
        instanceInfo.ppEnabledExtensionNames = RequiredExtensions;
        if(validation){
            instanceInfo.enabledLayerCount = uint32_t(arraysize(ValidationLayers));
            instanceInfo.ppEnabledLayerNames = ValidationLayers;
        }
        VkResult result = vkCreateInstance(&instanceInfo, 0, &this->instance);

        // Surface
        SetSurface(this->instance, &this->surface);

        // Physical device
        uint32_t physDeviceCount = 0;
        vkEnumeratePhysicalDevices(this->instance, &physDeviceCount, nullptr);
        auto physDevices = allocator->allocTransient<VkPhysicalDevice>(physDeviceCount);
        vkEnumeratePhysicalDevices(this->instance, &physDeviceCount, physDevices);

        const auto findExtensionProperty = [](view<VkExtensionProperties> availableExtensions)
        {
            for(size_t i = 0; i < arraysize(DeviceExtensions); i++){
                for(size_t j = 0; j < availableExtensions.count; j++){
                    if(strcmp(DeviceExtensions[i], availableExtensions[j].extensionName) == 0)
                        return true;
                }
            }
            return false;
        };

        for(size_t i = 0; i < physDeviceCount; i++){
            uint32_t extensionCount = 0;
            vkEnumerateDeviceExtensionProperties(physDevices[i], nullptr, &extensionCount, nullptr);
            auto availableExtensions = allocator->allocTransient<VkExtensionProperties>(extensionCount);
            vkEnumerateDeviceExtensionProperties(physDevices[i], nullptr, &extensionCount, availableExtensions);
            const bool extensionsSupported = findExtensionProperty({availableExtensions, extensionCount});

            VkSurfaceCapabilitiesKHR capabilities{};
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDevices[i], this->surface, &capabilities);
            uint32_t formatCount = 0, presentModeCount = 0;
            vkGetPhysicalDeviceSurfaceFormatsKHR(physDevices[i], this->surface, &formatCount, nullptr);
            vkGetPhysicalDeviceSurfacePresentModesKHR(physDevices[i], this->surface, &presentModeCount, nullptr);
            const bool adequateCapabilities = (formatCount > 0) && (presentModeCount > 0);

            uint32_t propCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(physDevices[i], &propCount, nullptr);
            auto queueFamilyProps = allocator->allocTransient<VkQueueFamilyProperties>(propCount);
            vkGetPhysicalDeviceQueueFamilyProperties(physDevices[i], &propCount, queueFamilyProps);

            this->graphics.family = 0;
            this->present.family = 0;
            bool hasGraphicsFamily = false, hasPresentFamily = false;
            for(uint32_t queueIndex = 0; queueIndex < propCount; queueIndex++){
                if(queueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT){
                    hasGraphicsFamily = true;
                    this->graphics.family = queueIndex;
                }

                VkBool32 presentSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(physDevices[i], queueIndex, this->surface, &presentSupport);
                if(presentSupport){
                    hasPresentFamily = true;
                    this->present.family = queueIndex;
                }
                if(hasGraphicsFamily && hasPresentFamily)
                    break;
            }
            const bool isValid = extensionsSupported && adequateCapabilities &&
                                 hasGraphicsFamily && hasPresentFamily;
            if(isValid){
                this->gpu = physDevices[i];
                break;
            }
        }
        vol_assert(this->gpu != VK_NULL_HANDLE)

        // Logical Device
        const uint32_t queueFamilies[] = {this->graphics.family, this->present.family};
        const uint32_t queueCount = (this->graphics.family != this->present.family) ? 2 : 1;

        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfos[2];
        for(size_t i = 0; i < queueCount; i++){
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

        if(validation){
            createInfo.enabledLayerCount = static_cast<uint32_t>(arraysize(ValidationLayers));
            createInfo.ppEnabledLayerNames = ValidationLayers;
        }

        result = vkCreateDevice(this->gpu, &createInfo, nullptr, &this->device);

        vkGetDeviceQueue(this->device, this->graphics.family, 0, &this->graphics.queue);
        vkGetDeviceQueue(this->device, this->present.family, 0, &this->present.queue);

        // Surface format
        uint32_t count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(this->gpu, this->surface, &count, nullptr);
        auto surfaceFormats = allocator->allocTransient<VkSurfaceFormatKHR>(count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(this->gpu, this->surface, &count, surfaceFormats);

        this->surfaceFormat = surfaceFormats[0];
        for(size_t i = 0; i < count; i++){
            const bool formatCheck = surfaceFormats[i].format == VK_FORMAT_B8G8R8A8_SRGB;
            const bool colourSpaceCheck = surfaceFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
            if(formatCheck && colourSpaceCheck){
                this->surfaceFormat = surfaceFormats[i];
                break;
            }
        }

        // Present mode
        count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(this->gpu, this->surface, &count, nullptr);
        auto presentModes = allocator->allocTransient<VkPresentModeKHR>(count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(this->gpu, this->surface, &count, presentModes);

        const auto preferred = vSync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_MAILBOX_KHR;
        this->presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        for(size_t i = 0; i < count; i++){
            if(presentModes[i] == preferred){
                this->presentMode = preferred;
                break;
            }
        }

        prepareCapabilities();

        // Command pool
        auto poolInfo = vkInits::commandPoolCreateInfo();
        poolInfo.queueFamilyIndex = this->graphics.family;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        result = vkCreateCommandPool(this->device, &poolInfo, nullptr, &this->cmdPool);

        // Swapchain
        auto info = vkInits::swapchainCreateInfo(VK_NULL_HANDLE);
        info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        info.surface = this->surface;
        info.minImageCount = static_cast<uint32_t>(this->minImageCount);
        info.imageFormat = this->surfaceFormat.format;
        info.imageColorSpace = this->surfaceFormat.colorSpace;
        info.imageExtent = this->extent;
        info.preTransform = this->preTransform;
        info.presentMode = this->presentMode;

        const uint32_t queueFamilyIndices[] = {this->graphics.family, this->present.family};
        if(this->graphics.family != this->present.family){
            info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            info.queueFamilyIndexCount = 2;
            info.pQueueFamilyIndices = queueFamilyIndices;
        }
        else{
            info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            info.queueFamilyIndexCount = 0;
            info.pQueueFamilyIndices = nullptr;
        }

        result = vkCreateSwapchainKHR(this->device, &info, nullptr, &this->swapchain);

        uint32_t imageCountLocal = 0;
        vkGetSwapchainImagesKHR(this->device, this->swapchain, &imageCountLocal, nullptr);
        this->imageCount = imageCountLocal;

        this->swapchainImages = allocator->allocPermanent<VkImage>(this->imageCount);
        this->swapchainViews = allocator->allocPermanent<VkImageView>(this->imageCount);

        vkGetSwapchainImagesKHR(this->device, this->swapchain, &imageCountLocal, this->swapchainImages);
        for(size_t i = 0; i < this->imageCount; i++){
            auto imageViewInfo = vkInits::imageViewCreateInfo();
            imageViewInfo.format = this->surfaceFormat.format;
            imageViewInfo.pNext = nullptr;
            imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageViewInfo.image = this->swapchainImages[i];
            result = vkCreateImageView(this->device, &imageViewInfo, nullptr, &this->swapchainViews[i]);
        }

        // Allocate permanent resources
        this->commandbuffers = allocator->allocPermanent<VkCommandBuffer>(this->imageCount);
        this->framebuffers = allocator->allocPermanent<VkFramebuffer>(this->imageCount);
        // TODO(arle): check if this needs allocation or if it's just copying other fences
        this->imagesInFlight = allocator->allocPermanent<VkFence>(this->imageCount);

        // MSAA samplecount
        VkPhysicalDeviceProperties physicalDeviceProperties{};
        vkGetPhysicalDeviceProperties(this->gpu, &physicalDeviceProperties);
        const auto colourBits = physicalDeviceProperties.limits.framebufferDepthSampleCounts;
        const auto depthBits = physicalDeviceProperties.limits.framebufferDepthSampleCounts;
        const VkSampleCountFlags samples = colourBits & depthBits;

        this->msaa.sampleCount = VK_SAMPLE_COUNT_1_BIT;
        for(VkFlags bit = VK_SAMPLE_COUNT_64_BIT; bit != VK_SAMPLE_COUNT_1_BIT; bit >>= 1){
            if(samples & bit){
                this->msaa.sampleCount = VkSampleCountFlagBits(bit);
                break;
            }
        }

        prepareMsaaBuffer();

        // Depth format
        this->depth.format = VK_FORMAT_UNDEFINED;
        constexpr VkFormat depthFormats[] = {
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT
        };

        for(size_t i = 0; i < arraysize(depthFormats); i++){
            VkFormatProperties props{};
            vkGetPhysicalDeviceFormatProperties(this->gpu, depthFormats[i], &props);
            if(props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
                this->depth.format = depthFormats[i];
        }

        prepareDepthBuffer();
        prepareRenderPass();
        prepareFramebuffers();
        prepareCommandbuffers();

        // Sync objects
        auto semaphoreInfo = vkInits::semaphoreCreateInfo();
        auto fenceInfo = vkInits::fenceCreateInfo();
        for(size_t i = 0; i < MAX_IMAGES_IN_FLIGHT; i++){
            vkCreateSemaphore(this->device, &semaphoreInfo, nullptr, &this->imageAvailableSPs[i]);
            vkCreateSemaphore(this->device, &semaphoreInfo, nullptr, &this->renderFinishedSPs[i]);
            vkCreateFence(this->device, &fenceInfo, nullptr, &this->inFlightFences[i]);
        }
    }

    ~vulkan_context()
    {
        vkDeviceWaitIdle(this->device);

        for(size_t i = 0; i < MAX_IMAGES_IN_FLIGHT; i++){
            vkDestroySemaphore(this->device, this->imageAvailableSPs[i], nullptr);
            vkDestroySemaphore(this->device, this->renderFinishedSPs[i], nullptr);
            vkDestroyFence(this->device, this->inFlightFences[i], nullptr);
        }

        this->depth.destroy(this->device);
        this->msaa.destroy(this->device);

        for(size_t i = 0; i < this->imageCount; i++)
            vkDestroyFramebuffer(this->device, this->framebuffers[i], nullptr);

        vkDestroyRenderPass(this->device, this->renderPass, nullptr);
        vkDestroyCommandPool(this->device, this->cmdPool, nullptr);

        for(size_t i = 0; i < this->imageCount; i++)
            vkDestroyImageView(this->device, this->swapchainViews[i], nullptr);
        vkDestroySwapchainKHR(this->device, this->swapchain, nullptr);

        vkDestroyDevice(this->device, nullptr);
        vkDestroySurfaceKHR(this->instance, this->surface, nullptr);
        vkDestroyInstance(this->instance, nullptr);
    }

    void onWindowResize()
    {
        vkDeviceWaitIdle(this->device);

        this->depth.destroy(this->device);
        this->msaa.destroy(this->device);

        for(size_t i = 0; i < this->imageCount; i++)
            vkDestroyFramebuffer(this->device, this->framebuffers[i], nullptr);

        vkResetCommandPool(this->device, this->cmdPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
        vkDestroyRenderPass(this->device, this->renderPass, nullptr);

        for(size_t i = 0; i < this->imageCount; i++)
            vkDestroyImageView(this->device, this->swapchainViews[i], nullptr);

        prepareCapabilities();

        auto oldSwapChain = this->swapchain;
        auto swapChainInfo = vkInits::swapchainCreateInfo(oldSwapChain);
        swapChainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapChainInfo.surface = this->surface;
        swapChainInfo.minImageCount = static_cast<uint32_t>(this->minImageCount);
        swapChainInfo.imageFormat = this->surfaceFormat.format;
        swapChainInfo.imageColorSpace = this->surfaceFormat.colorSpace;
        swapChainInfo.imageExtent = this->extent;
        swapChainInfo.preTransform = this->preTransform;
        swapChainInfo.presentMode = this->presentMode;

        const uint32_t queueFamilyIndices[] = {this->graphics.family, this->present.family};
        if(this->graphics.family != this->present.family){
            swapChainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            swapChainInfo.queueFamilyIndexCount = 2;
            swapChainInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else{
            swapChainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            swapChainInfo.queueFamilyIndexCount = 0;
            swapChainInfo.pQueueFamilyIndices = nullptr;
        }

        VkResult result = vkCreateSwapchainKHR(this->device, &swapChainInfo, nullptr, &this->swapchain);
        vkDestroySwapchainKHR(this->device, oldSwapChain, nullptr);

        uint32_t localImageCount = 0;
        result = vkGetSwapchainImagesKHR(this->device, this->swapchain, &localImageCount, nullptr);
        result = vkGetSwapchainImagesKHR(this->device, this->swapchain, &localImageCount, this->swapchainImages);
        this->imageCount = localImageCount;

        for(size_t i = 0; i < this->imageCount; i++){
            auto imageViewInfo = vkInits::imageViewCreateInfo();
            imageViewInfo.format = this->surfaceFormat.format;
            imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageViewInfo.image = this->swapchainImages[i];
            vkCreateImageView(this->device, &imageViewInfo, nullptr, &this->swapchainViews[i]);
        }

        prepareMsaaBuffer();
        prepareDepthBuffer();
        prepareRenderPass();
        prepareFramebuffers();
        prepareCommandbuffers();
    }

    VkMemoryAllocateInfo getMemoryAllocInfo(VkMemoryRequirements memReqs, VkMemoryPropertyFlags flags) const
    {
        VkPhysicalDeviceMemoryProperties memProperties{};
        vkGetPhysicalDeviceMemoryProperties(this->gpu, &memProperties);

        const auto getMemTypeIndex = [&memProperties](uint32_t typeFilter, VkMemoryPropertyFlags propFlags){
            for(uint32_t i = 0; i < memProperties.memoryTypeCount; i++){
                const uint32_t typeFilterResult = typeFilter & (1 << i);
                const VkMemoryPropertyFlags memoryProps = memProperties.memoryTypes[i].propertyFlags;
                if(typeFilterResult && (memoryProps & propFlags))
                    return i;
            }
            return 0U;
        };

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = getMemTypeIndex(memReqs.memoryTypeBits, flags);
        return allocInfo;
    }

    void prepareCapabilities()
    {
        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(this->gpu, this->surface, &capabilities);

        if(capabilities.currentExtent.width == 0xFFFFFFFF){
            this->extent.width = clamp(this->extent.width, capabilities.minImageExtent.width,
                                         capabilities.maxImageExtent.width);
            this->extent.height = clamp(this->extent.height, capabilities.minImageExtent.height,
                                         capabilities.maxImageExtent.height);
        }
        else{
            this->extent = capabilities.currentExtent;
        }

        this->minImageCount = capabilities.minImageCount + 1;
        if((capabilities.maxImageCount > 0) && (this->minImageCount > capabilities.maxImageCount))
            this->minImageCount = capabilities.maxImageCount;

        this->preTransform = capabilities.currentTransform;
    }

    void prepareMsaaBuffer()
    {
        auto imageInfo = vkInits::imageCreateInfo();
        imageInfo.samples = this->msaa.sampleCount;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.extent = {this->extent.width, this->extent.height, 1};
        imageInfo.format = this->surfaceFormat.format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        VkResult result = vkCreateImage(this->device, &imageInfo, nullptr, &this->msaa.image);

        VkMemoryRequirements memReqs{};
        vkGetImageMemoryRequirements(this->device, this->msaa.image, &memReqs);

        auto allocInfo = getMemoryAllocInfo(memReqs, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        result = vkAllocateMemory(this->device, &allocInfo, nullptr, &this->msaa.memory);
        result = vkBindImageMemory(this->device, this->msaa.image, this->msaa.memory, 0);

        auto viewInfo = vkInits::imageViewCreateInfo();
        viewInfo.image = this->msaa.image;
        viewInfo.format = this->surfaceFormat.format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        result = vkCreateImageView(this->device, &viewInfo, nullptr, &this->msaa.view);
    }

    void prepareDepthBuffer()
    {
        auto imageInfo = vkInits::imageCreateInfo();
        imageInfo.extent = {this->extent.width, this->extent.height, 1};
        imageInfo.format = this->depth.format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        VkResult result = vkCreateImage(this->device, &imageInfo, nullptr, &this->depth.image);

        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(this->device, this->depth.image, &memReqs);

        auto allocInfo = getMemoryAllocInfo(memReqs, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        result = vkAllocateMemory(this->device, &allocInfo, nullptr, &this->depth.memory);
        result = vkBindImageMemory(this->device, this->depth.image, this->depth.memory, 0);

        auto depthCreateInfo = vkInits::imageViewCreateInfo();
        depthCreateInfo.image = this->depth.image;
        depthCreateInfo.format = this->depth.format;
        depthCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        result = vkCreateImageView(this->device, &depthCreateInfo, nullptr, &this->depth.view);
    }

    void prepareRenderPass()
    {
        constexpr auto finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        auto colourAttachment = vkInits::attachmentDescription(this->surfaceFormat.format, finalLayout);
        colourAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

        constexpr auto finalLayoutDepth = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        auto depthAttachment = vkInits::attachmentDescription(this->depth.format, finalLayoutDepth);
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

        constexpr auto finalLayoutResolve = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        auto colourResolve = vkInits::attachmentDescription(this->surfaceFormat.format, finalLayoutResolve);
        colourResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colourResolve.samples = VK_SAMPLE_COUNT_1_BIT;

        const VkAttachmentDescription attachments[] = {colourAttachment, depthAttachment, colourResolve};

        VkAttachmentReference coluorAttachmentRef{};
        coluorAttachmentRef.attachment = 0;
        coluorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colourResolveRef{};
        colourResolveRef.attachment = 2;
        colourResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &coluorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;
        subpass.pResolveAttachments = &colourResolveRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(arraysize(attachments));
        renderPassInfo.pAttachments = attachments;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        vkCreateRenderPass(this->device, &renderPassInfo, nullptr, &this->renderPass);
    }

    void prepareFramebuffers()
    {
        for(size_t i = 0; i < this->imageCount; i++){
            const VkImageView frameBufferAttachments[] = {
                this->msaa.view,
                this->depth.view,
                this->swapchainViews[i]
            };

            auto framebufferInfo = vkInits::framebufferCreateInfo();
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = this->renderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(arraysize(frameBufferAttachments));
            framebufferInfo.pAttachments = frameBufferAttachments;
            framebufferInfo.width = this->extent.width;
            framebufferInfo.height = this->extent.height;
            vkCreateFramebuffer(this->device, &framebufferInfo, nullptr, &this->framebuffers[i]);
        }
    }

    void prepareCommandbuffers()
    {
        auto cmdBufAllocInfo = vkInits::commandBufferAllocateInfo(this->cmdPool, this->imageCount);
        vkAllocateCommandBuffers(this->device, &cmdBufAllocInfo, this->commandbuffers);
    }

    VkInstance instance;
    VkPhysicalDevice gpu;
    VkSurfaceKHR surface;
    VkDevice device;

    queue_data graphics, present;
    VkCommandPool cmdPool;
    VkRenderPass renderPass;

    VkPresentModeKHR presentMode;
    VkSurfaceFormatKHR surfaceFormat;
    VkSurfaceTransformFlagBitsKHR preTransform;
    VkExtent2D extent;
    VkSwapchainKHR swapchain;
    size_t imageCount, minImageCount;

    VkImage *swapchainImages;
    VkImageView *swapchainViews;
    VkFramebuffer *framebuffers;
    VkCommandBuffer *commandbuffers;
    uint32_t currentFrame;

    depth_data depth;
    msaa_data msaa;

    VkSemaphore imageAvailableSPs[MAX_IMAGES_IN_FLIGHT];
    VkSemaphore renderFinishedSPs[MAX_IMAGES_IN_FLIGHT];
    VkFence inFlightFences[MAX_IMAGES_IN_FLIGHT];
    VkFence *imagesInFlight;
};

struct buffer_t
{
    buffer_t() = default;

    void createTransfer(const vulkan_context *context, size_t bufSize)
    {
        this->size = bufSize;
        auto bufferInfo = vkInits::bufferCreateInfo(bufSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        vkCreateBuffer(context->device, &bufferInfo, nullptr, &this->data);

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(context->device, this->data, &memReqs);

        auto allocInfo = context->getMemoryAllocInfo(memReqs, VISIBLE_BUFFER_FLAGS);
        vkAllocateMemory(context->device, &allocInfo, nullptr, &this->memory);
        vkBindBufferMemory(context->device, this->data, this->memory, 0);
    }

    void createGpuLocal(const vulkan_context *context, buffer_t *src, VkBufferUsageFlags usage)
    {
        this->size = src->size;
        auto bufferInfo = vkInits::bufferCreateInfo(src->size, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        vkCreateBuffer(context->device, &bufferInfo, nullptr, &this->data);

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(context->device, this->data, &memReqs);

        auto allocInfo = context->getMemoryAllocInfo(memReqs, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkAllocateMemory(context->device, &allocInfo, nullptr, &this->memory);
        vkBindBufferMemory(context->device, this->data, this->memory, 0);

        VkCommandBuffer transientCmd = VK_NULL_HANDLE;
        auto cmdAlloc = vkInits::commandBufferAllocateInfo(context->cmdPool, 1);
        vkAllocateCommandBuffers(context->device, &cmdAlloc, &transientCmd);

        auto beginInfo = vkInits::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        vkBeginCommandBuffer(transientCmd, &beginInfo);

        VkBufferCopy copyRegion{};
        copyRegion.size = src->size;
        vkCmdCopyBuffer(transientCmd, src->data, this->data, 1, &copyRegion);

        vkEndCommandBuffer(transientCmd);

        auto queueSubmitInfo = vkInits::submitInfo(&transientCmd, 1);
        vkQueueSubmit(context->graphics.queue, 1, &queueSubmitInfo, VK_NULL_HANDLE);

        vkQueueWaitIdle(context->graphics.queue);
        vkFreeCommandBuffers(context->device, context->cmdPool, 1, &transientCmd);

        src->destroy(context->device);
    }

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
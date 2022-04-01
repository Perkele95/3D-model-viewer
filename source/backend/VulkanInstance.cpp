#include "VulkanInstance.hpp"

void VulkanInstance::prepare()
{
    auto appInfo = vkInits::applicationInfo(settings.title);
    auto instanceInfo = vkInits::instanceCreateInfo();
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.enabledExtensionCount = uint32_t(arraysize(RequiredExtensions));
    instanceInfo.ppEnabledExtensionNames = RequiredExtensions;
    if(settings.enValidation)
    {
        instanceInfo.enabledLayerCount = uint32_t(arraysize(ValidationLayers));
        instanceInfo.ppEnabledLayerNames = ValidationLayers;
    }
    vkCreateInstance(&instanceInfo, 0, &m_instance);

    pltf::SurfaceCreate(platformDevice, m_instance, &m_surface);

    pickPhysicalDevice();

    refreshCapabilities();

    device.create(settings.enValidation);

    // Queues

    vkGetDeviceQueue(device, device.queueBits.graphics, 0, &graphicsQueue);
    vkGetDeviceQueue(device, device.queueBits.present, 0, &m_presentQueue);

    prepareSurfaceFormat();
    const auto mode = static_cast<VkPresentModeKHR>(settings.syncMode);
    preparePresentMode(mode);

    prepareSwapchain(VK_NULL_HANDLE);

    // Get image count

    uint32_t imageCountLocal = 0;
    vkGetSwapchainImagesKHR(device, m_swapchain, &imageCountLocal, nullptr);
    imageCount = imageCountLocal;

    // Allocate and aquire images

    m_swapchainImages = push<VkImage>(imageCount);
    vkGetSwapchainImagesKHR(device, m_swapchain, &imageCountLocal, m_swapchainImages);

    m_swapchainViews = push<VkImageView>(imageCount);
    prepareSwapchainViews();

    prepareSampleCount();
    prepareMsaa();
    prepareDepthFormat();
    prepareDepth();
    prepareRenderpass();

    framebuffers = push<VkFramebuffer>(imageCount);
    prepareFramebuffers();

    // Sync data

    auto semaphoreInfo = vkInits::semaphoreCreateInfo();
    auto fenceInfo = vkInits::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    for (size_t i = 0; i < MAX_IMAGES_IN_FLIGHT; i++)
    {
        vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_sync.imageAvailableSPs[i]);
        vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_sync.renderFinishedSPs[i]);
        vkCreateFence(device, &fenceInfo, nullptr, &m_sync.inFlightFences[i]);
    }

    // Command buffers

    auto cmdInfo = vkInits::commandBufferAllocateInfo(device.commandPool, MAX_IMAGES_IN_FLIGHT);
    vkAllocateCommandBuffers(device, &cmdInfo, commandBuffers);

    // Init submit & present infos

    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = nullptr;
    submitInfo.pCommandBuffers = nullptr;
    submitInfo.commandBufferCount = 0;
    submitInfo.pWaitSemaphores = nullptr;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = nullptr;
    submitInfo.signalSemaphoreCount = 1;

    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = nullptr;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_swapchain;
    presentInfo.pImageIndices = nullptr;
    presentInfo.pNext = nullptr;
    presentInfo.pResults = nullptr;
}

void VulkanInstance::destroy()
{
    for (size_t i = 0; i < MAX_IMAGES_IN_FLIGHT; i++)
    {
        vkDestroyFence(device, m_sync.inFlightFences[i], nullptr);
        vkDestroySemaphore(device, m_sync.renderFinishedSPs[i], nullptr);
        vkDestroySemaphore(device, m_sync.imageAvailableSPs[i], nullptr);
    }

    for (size_t i = 0; i < imageCount; i++)
        vkDestroyFramebuffer(device, framebuffers[i], nullptr);

    vkDestroyRenderPass(device, renderPass, nullptr);

    vkDestroyImage(device, m_depth.image, nullptr);
    vkDestroyImageView(device, m_depth.view, nullptr);
    vkFreeMemory(device, m_depth.memory, nullptr);

    vkDestroyImage(device, m_msaa.image, nullptr);
    vkDestroyImageView(device, m_msaa.view, nullptr);
    vkFreeMemory(device, m_msaa.memory, nullptr);

    for (size_t i = 0; i < imageCount; i++)
        vkDestroyImageView(device, m_swapchainViews[i], nullptr);

    vkDestroySwapchainKHR(device, m_swapchain, nullptr);

    device.destroy();
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    vkDestroyInstance(m_instance, nullptr);
}

void VulkanInstance::onResize()
{
    vkQueueWaitIdle(graphicsQueue);

    refreshCapabilities();

    for (size_t i = 0; i < imageCount; i++)
        vkDestroyFramebuffer(device, framebuffers[i], nullptr);

    vkDestroyImage(device, m_depth.image, nullptr);
    vkDestroyImageView(device, m_depth.view, nullptr);
    vkFreeMemory(device, m_depth.memory, nullptr);

    vkDestroyImage(device, m_msaa.image, nullptr);
    vkDestroyImageView(device, m_msaa.view, nullptr);
    vkFreeMemory(device, m_msaa.memory, nullptr);

    for (size_t i = 0; i < MAX_IMAGES_IN_FLIGHT; i++)
        vkResetCommandBuffer(commandBuffers[i], VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

    auto oldSwapchain = m_swapchain;
    prepareSwapchain(oldSwapchain);

    for (size_t i = 0; i < imageCount; i++)
        vkDestroyImageView(device, m_swapchainViews[i], nullptr);

    vkDestroySwapchainKHR(device, oldSwapchain, nullptr);

    auto imageCountLocal = uint32_t(imageCount);
    vkGetSwapchainImagesKHR(device, m_swapchain, &imageCountLocal, m_swapchainImages);

    prepareSwapchainViews();
    prepareDepth();
    prepareMsaa();
    prepareFramebuffers();
}

void VulkanInstance::prepareFrame()
{
    currentFrame = (currentFrame + 1) % MAX_IMAGES_IN_FLIGHT;
    vkQueueWaitIdle(graphicsQueue);

    vkWaitForFences(device, 1, &m_sync.inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &m_sync.inFlightFences[currentFrame]);
    auto result = vkAcquireNextImageKHR(device,
                                        m_swapchain,
                                        UINT64_MAX,
                                        m_sync.imageAvailableSPs[currentFrame],
                                        VK_NULL_HANDLE,
                                        &imageIndex);

    resizeRequired = (result == VK_ERROR_OUT_OF_DATE_KHR);
}

void VulkanInstance::submitFrame()
{
    const VkSemaphore imageAvailableSPs[] = {m_sync.imageAvailableSPs[currentFrame]};
    const VkSemaphore renderFinishedSPs[] = {m_sync.renderFinishedSPs[currentFrame]};
    const VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    submitInfo.waitSemaphoreCount = uint32_t(arraysize(imageAvailableSPs));
    submitInfo.pWaitSemaphores = imageAvailableSPs;
    submitInfo.signalSemaphoreCount = uint32_t(arraysize(renderFinishedSPs));
    submitInfo.pSignalSemaphores = renderFinishedSPs;
    submitInfo.pWaitDstStageMask = waitStages;
    vkQueueSubmit(graphicsQueue, 1, &submitInfo, m_sync.inFlightFences[currentFrame]);

    presentInfo.waitSemaphoreCount = uint32_t(arraysize(renderFinishedSPs));
    presentInfo.pWaitSemaphores = renderFinishedSPs;
    presentInfo.pImageIndices = &imageIndex;
    const auto result = vkQueuePresentKHR(m_presentQueue, &presentInfo);

    switch (result)
    {
        case VK_SUCCESS: break;
        case VK_SUBOPTIMAL_KHR:
        case VK_ERROR_OUT_OF_DATE_KHR: resizeRequired = true; break;
        default: /*CORE ERROR*/ break;
    }
}

void VulkanInstance::pickPhysicalDevice()
{
    uint32_t physDeviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &physDeviceCount, nullptr);
    auto physDevices = data_buffer<VkPhysicalDevice>(physDeviceCount);
    vkEnumeratePhysicalDevices(m_instance, &physDeviceCount, physDevices.data());

    const auto findExtensionProperty = [](view<VkExtensionProperties> availableExtensions)
    {
        for(size_t i = 0; i < arraysize(DeviceExtensions); i++)
        {
            for(size_t j = 0; j < availableExtensions.count; j++)
            {
                if(strcmp(DeviceExtensions[i], availableExtensions[j].extensionName) == 0)
                    return true;
            }
        }
        return false;
    };

    for(size_t i = 0; i < physDeviceCount; i++)
    {
        uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(physDevices[i], nullptr, &extensionCount, nullptr);
        auto availableExtensions = data_buffer<VkExtensionProperties>(extensionCount);
        vkEnumerateDeviceExtensionProperties(physDevices[i], nullptr, &extensionCount, availableExtensions.data());
        const bool extensionsSupported = findExtensionProperty(availableExtensions.getView());

        uint32_t formatCount = 0, presentModeCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physDevices[i], m_surface, &formatCount, nullptr);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physDevices[i], m_surface, &presentModeCount, nullptr);
        const bool adequateCapabilities = (formatCount > 0) && (presentModeCount > 0);

        uint32_t propCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physDevices[i], &propCount, nullptr);
        auto queueFamilyProps = data_buffer<VkQueueFamilyProperties>(propCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physDevices[i], &propCount, queueFamilyProps.data());

        device.queueBits.graphics = 0;
        device.queueBits.present = 0;
        bool hasGraphicsFamily = false, hasPresentFamily = false;
        for(uint32_t queueIndex = 0; queueIndex < propCount; queueIndex++)
        {
            if(queueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                hasGraphicsFamily = true;
                device.queueBits.graphics = queueIndex;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physDevices[i], queueIndex, m_surface, &presentSupport);
            if(presentSupport)
            {
                hasPresentFamily = true;
                device.queueBits.present = queueIndex;
            }
            if(hasGraphicsFamily && hasPresentFamily)
                break;
        }
        const bool isValid = extensionsSupported && adequateCapabilities &&
                                hasGraphicsFamily && hasPresentFamily;
        if(isValid)
        {
            device.gpu = physDevices[i];
            break;
        }
    }
    if(device.gpu == VK_NULL_HANDLE)
        coreMessage(log_level::error, "No suitable GPU found");
}

VkResult VulkanInstance::prepareSwapchain(VkSwapchainKHR oldSwapchain)
{
    auto info = vkInits::swapchainCreateInfo(oldSwapchain);
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = m_surface;
    info.minImageCount = m_capabilities.minImageCount;
    info.imageFormat = surfaceFormat.format;
    info.imageColorSpace = surfaceFormat.colorSpace;
    info.imageExtent = extent;
    info.preTransform = m_capabilities.currentTransform;
    info.presentMode = m_presentMode;

    const uint32_t queueFamilyIndices[] = {
        device.queueBits.graphics,
        device.queueBits.present
    };

    if(device.queueBits.graphics != device.queueBits.present)
    {
        info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        info.queueFamilyIndexCount = 2;
        info.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.queueFamilyIndexCount = 0;
        info.pQueueFamilyIndices = nullptr;
    }

    return vkCreateSwapchainKHR(device, &info, nullptr, &m_swapchain);
}

void VulkanInstance::prepareSwapchainViews()
{
    for (size_t i = 0; i < imageCount; i++)
    {
        auto imageViewInfo = vkInits::imageViewCreateInfo();
        imageViewInfo.format = surfaceFormat.format;
        imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewInfo.image = m_swapchainImages[i];
        vkCreateImageView(device, &imageViewInfo, nullptr, &m_swapchainViews[i]);
    }
}

void VulkanInstance::prepareSampleCount()
{
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(device.gpu, &props);

    const auto samples = props.limits.framebufferDepthSampleCounts &
                         props.limits.framebufferColorSampleCounts;

    sampleCount = VK_SAMPLE_COUNT_1_BIT;
    for (uint32_t bit = VK_SAMPLE_COUNT_64_BIT; bit != VK_SAMPLE_COUNT_1_BIT; bit >>= 1)
    {
        if(samples & bit)
        {
            sampleCount = VkSampleCountFlagBits(bit);
            break;
        }
    }
}

void VulkanInstance::prepareMsaa()
{
    auto msaaInfo = vkInits::imageCreateInfo();
    msaaInfo.samples = sampleCount;
    msaaInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    msaaInfo.extent = {extent.width, extent.height, 1};
    msaaInfo.format = surfaceFormat.format;
    msaaInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    msaaInfo.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    vkCreateImage(device, &msaaInfo, nullptr, &m_msaa.image);

    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(device, m_msaa.image, &memReqs);

    auto allocInfo = device.getMemoryAllocInfo(memReqs, MEM_FLAG_GPU_LOCAL);
    vkAllocateMemory(device, &allocInfo, nullptr, &m_msaa.memory);
    vkBindImageMemory(device, m_msaa.image, m_msaa.memory, 0);

    auto viewInfo = vkInits::imageViewCreateInfo();
    viewInfo.image = m_msaa.image;
    viewInfo.format = msaaInfo.format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vkCreateImageView(device, &viewInfo, nullptr, &m_msaa.view);
}

void VulkanInstance::prepareDepthFormat()
{
    depthFormat = VK_FORMAT_UNDEFINED;
    constexpr VkFormat formats[] = {
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT
    };

    for(size_t i = 0; i < arraysize(formats); i++)
    {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(device.gpu, formats[i], &props);
        if(props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            depthFormat = formats[i];
            break;
        }
    }
}

void VulkanInstance::prepareDepth()
{
    auto depthInfo = vkInits::imageCreateInfo();
    depthInfo.extent = {extent.width, extent.height, 1};
    depthInfo.format = depthFormat;
    depthInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    depthInfo.samples = sampleCount;
    depthInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    vkCreateImage(device, &depthInfo, nullptr, &m_depth.image);

    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(device, m_depth.image, &memReqs);

    auto allocInfo = device.getMemoryAllocInfo(memReqs, MEM_FLAG_GPU_LOCAL);
    vkAllocateMemory(device, &allocInfo, nullptr, &m_depth.memory);
    vkBindImageMemory(device, m_depth.image, m_depth.memory, 0);

    auto viewInfo = vkInits::imageViewCreateInfo();
    viewInfo.image = m_depth.image;
    viewInfo.format = depthInfo.format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    vkCreateImageView(device, &viewInfo, nullptr, &m_depth.view);
}

VkResult VulkanInstance::prepareRenderpass()
{
    auto colourAttachment = vkInits::attachmentDescription(surfaceFormat.format);
    colourAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colourAttachment.samples = sampleCount;

    auto depthAttachment = vkInits::attachmentDescription(depthFormat);
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.samples = sampleCount;

    auto colourResolve = vkInits::attachmentDescription(surfaceFormat.format);
    colourResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
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

    VkSubpassDependency dependencies[2];
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = uint32_t(arraysize(attachments));
    renderPassInfo.pAttachments = attachments;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = uint32_t(arraysize(dependencies));
    renderPassInfo.pDependencies = dependencies;

    return vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass);
}

void VulkanInstance::prepareFramebuffers()
{
    for (size_t i = 0; i < imageCount; i++)
    {
        const VkImageView frameBufferAttachments[] = {
            m_msaa.view,
            m_depth.view,
            m_swapchainViews[i]
        };

        auto framebufferInfo = vkInits::framebufferCreateInfo();
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = uint32_t(arraysize(frameBufferAttachments));
        framebufferInfo.pAttachments = frameBufferAttachments;
        framebufferInfo.width = extent.width;
        framebufferInfo.height = extent.height;
        vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffers[i]);
    }
}

void VulkanInstance::refreshCapabilities()
{
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.gpu, m_surface, &m_capabilities);

    // Ask for one more image and clamp to max

    m_capabilities.minImageCount++;
    if(m_capabilities.maxImageCount > 0)
        m_capabilities.minImageCount %= m_capabilities.maxImageCount + 1;

    // Refresh extent

    extent = m_capabilities.currentExtent;
    if(extent.width == 0xFFFFFFFF)
    {
        extent.width = clamp(extent.width, m_capabilities.minImageExtent.width,
                             m_capabilities.maxImageExtent.width);
        extent.height = clamp(extent.height, m_capabilities.minImageExtent.height,
                             m_capabilities.maxImageExtent.height);
    }

    // Refresh aspect ratio

    aspectRatio = float(extent.width) / float(extent.height);
}

void VulkanInstance::prepareSurfaceFormat()
{
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device.gpu, m_surface, &count, nullptr);
    auto surfaceFormats = new VkSurfaceFormatKHR[count];
    vkGetPhysicalDeviceSurfaceFormatsKHR(device.gpu, m_surface, &count, surfaceFormats);

    surfaceFormat = surfaceFormats[0];
    for(size_t i = 0; i < count; i++)
    {
        const bool formatCheck = surfaceFormats[i].format == VK_FORMAT_B8G8R8A8_SRGB;
        const bool colourSpaceCheck = surfaceFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        if(formatCheck && colourSpaceCheck)
        {
            surfaceFormat = surfaceFormats[i];
            break;
        }
    }

    delete surfaceFormats;
}

void VulkanInstance::preparePresentMode(VkPresentModeKHR preferredMode)
{
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device.gpu, m_surface, &count, nullptr);
    auto presentModes = new VkPresentModeKHR[count];
    vkGetPhysicalDeviceSurfacePresentModesKHR(device.gpu, m_surface, &count, presentModes);

    m_presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for(size_t i = 0; i < count; i++)
    {
        if(presentModes[i] == preferredMode)
        {
            m_presentMode = preferredMode;
            break;
        }
    }

    delete presentModes;
}
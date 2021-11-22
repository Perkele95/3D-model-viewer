#include "vulkan_device.hpp"
#include <string.h>

constexpr char *ValidationLayers[] = {"VK_LAYER_KHRONOS_validation"};
constexpr char *DeviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
constexpr char *RequiredExtensions[] = {"VK_KHR_surface", "VK_KHR_win32_surface"};

VkMemoryAllocateInfo GetMemoryAllocInfo(VkPhysicalDevice gpu, VkMemoryRequirements memReqs,
                                        VkMemoryPropertyFlags flags)
{
    VkPhysicalDeviceMemoryProperties memProperties{};
    vkGetPhysicalDeviceMemoryProperties(gpu, &memProperties);

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

void buffer_t::destroy(VkDevice device)
{
    vkDestroyBuffer(device, this->data, nullptr);
    vkFreeMemory(device, this->memory, nullptr);
    this->data = VK_NULL_HANDLE;
    this->memory = VK_NULL_HANDLE;
    this->size = 0;
}

void image_buffer::destroy(VkDevice device)
{
    vkDestroyImage(device, this->image, nullptr);
    vkDestroyImageView(device, this->view, nullptr);
    vkFreeMemory(device, this->memory, nullptr);
    this->image = VK_NULL_HANDLE;
    this->view = VK_NULL_HANDLE;
    this->memory = VK_NULL_HANDLE;
}

void vulkan_device::create(mv_allocator *allocator, bool validation, bool vSync)
{
    auto appInfo = vkInits::applicationInfo("3D model viewer");
    auto instanceInfo = vkInits::instanceCreateInfo();
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.enabledExtensionCount = uint32_t(arraysize(RequiredExtensions));
    instanceInfo.ppEnabledExtensionNames = RequiredExtensions;
    if(validation){
        instanceInfo.enabledLayerCount = uint32_t(arraysize(ValidationLayers));
        instanceInfo.ppEnabledLayerNames = ValidationLayers;
    }
    vkCreateInstance(&instanceInfo, 0, &this->instance);

    SetSurface(this->instance, &this->surface);

    pickPhysicalDevice(allocator);
    prepareLogicalDevice(validation);

    vkGetDeviceQueue(this->device, this->graphics.family, 0, &this->graphics.queue);
    vkGetDeviceQueue(this->device, this->present.family, 0, &this->present.queue);

    pickSurfaceFormat(allocator);
    const auto preferredMode = vSync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_MAILBOX_KHR;
    pickPresentMode(allocator, preferredMode);
}

void vulkan_device::destroy()
{
    vkDestroyDevice(this->device, nullptr);
    vkDestroySurfaceKHR(this->instance, this->surface, nullptr);
    vkDestroyInstance(this->instance, nullptr);
}

void vulkan_device::refresh()
{
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(this->gpu, this->surface, &this->capabilities);
}

VkExtent2D vulkan_device::getExtent() const
{
    VkExtent2D extent = this->capabilities.currentExtent;
    if(this->capabilities.currentExtent.width == 0xFFFFFFFF){
        extent.width = clamp(extent.width, this->capabilities.minImageExtent.width,
                                     this->capabilities.maxImageExtent.width);
        extent.height = clamp(extent.height, this->capabilities.minImageExtent.height,
                                      this->capabilities.maxImageExtent.height);
    }
    return extent;
}

uint32_t vulkan_device::getMinImageCount() const
{
    uint32_t minImageCount = this->capabilities.minImageCount + 1;
    if(this->capabilities.maxImageCount > 0 && minImageCount > this->capabilities.maxImageCount)
        minImageCount = this->capabilities.maxImageCount;
    return minImageCount;
}

VkSampleCountFlagBits vulkan_device::getSampleCount() const
{
    VkPhysicalDeviceProperties physicalDeviceProperties{};
    vkGetPhysicalDeviceProperties(this->gpu, &physicalDeviceProperties);
    const auto colourBits = physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    const auto depthBits = physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    const VkSampleCountFlags samples = colourBits & depthBits;

    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
    for(VkSampleCountFlags bit = VK_SAMPLE_COUNT_64_BIT; bit != VK_SAMPLE_COUNT_1_BIT; bit >>= 1){
        if(samples & bit){
            sampleCount = VkSampleCountFlagBits(bit);
            break;
        }
    }
    return sampleCount;
}

VkFormat vulkan_device::getDepthFormat() const
{
    auto format = VK_FORMAT_UNDEFINED;
    constexpr VkFormat depthFormats[] = {
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
    };

    for(size_t i = 0; i < arraysize(depthFormats); i++){
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(this->gpu, depthFormats[i], &props);
        if(props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT){
            format = depthFormats[i];
            break;
        }
    }

    return format;
}

VkResult vulkan_device::loadShader(const file_t *src, VkShaderModule *pModule) const
{
    auto shaderInfo = vkInits::shaderModuleCreateInfo(src);
    VkResult result = vkCreateShaderModule(this->device, &shaderInfo, nullptr, pModule);
    return result;
}

VkResult vulkan_device::buildSwapchain(VkSwapchainKHR oldSwapchain, VkSwapchainKHR *pSwapchain) const
{
    auto info = vkInits::swapchainCreateInfo(oldSwapchain);
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = this->surface;
    info.minImageCount = this->getMinImageCount();
    info.imageFormat = this->surfaceFormat.format;
    info.imageColorSpace = this->surfaceFormat.colorSpace;
    info.imageExtent = this->getExtent();
    info.preTransform = this->capabilities.currentTransform;
    info.presentMode = this->presentMode;

    const uint32_t queueFamilyIndices[] = {
        this->graphics.family,
        this->present.family
    };

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

    return vkCreateSwapchainKHR(this->device, &info, nullptr, pSwapchain);
}

void vulkan_device::pickPhysicalDevice(mv_allocator *allocator)
{
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

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDevices[i], this->surface, &this->capabilities);

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
}

void vulkan_device::prepareLogicalDevice(bool validation)
{
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

    vkCreateDevice(this->gpu, &createInfo, nullptr, &this->device);
}

void vulkan_device::pickSurfaceFormat(mv_allocator *allocator)
{
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
}

void vulkan_device::pickPresentMode(mv_allocator *allocator, VkPresentModeKHR preferredMode)
{
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(this->gpu, this->surface, &count, nullptr);
    auto presentModes = allocator->allocTransient<VkPresentModeKHR>(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(this->gpu, this->surface, &count, presentModes);

    this->presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    for(size_t i = 0; i < count; i++){
        if(presentModes[i] == preferredMode){
            this->presentMode = preferredMode;
            break;
        }
    }
}
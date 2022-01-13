#include "vulkan_device.hpp"
#include <string.h>

constexpr char *ValidationLayers[] = {"VK_LAYER_KHRONOS_validation"};
constexpr char *DeviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
constexpr char *RequiredExtensions[] = {"VK_KHR_surface", "VK_KHR_win32_surface"};

void vulkan_device::init(Platform::lDevice platformDevice, bool validation, bool vSync)
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
    vkCreateInstance(&instanceInfo, 0, &m_instance);

    Platform::SetupVkSurface(platformDevice, m_instance, &m_surface);

    pickPhysicalDevice();
    prepareLogicalDevice(validation);

    vkGetDeviceQueue(this->device, this->graphics.family, 0, &this->graphics.queue);
    vkGetDeviceQueue(this->device, this->present.family, 0, &this->present.queue);

    pickSurfaceFormat();
    const auto preferredMode = vSync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_MAILBOX_KHR;
    pickPresentMode(preferredMode);

    refresh();
    getSampleCount();
}

vulkan_device::~vulkan_device()
{
    vkDestroyDevice(this->device, nullptr);
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    vkDestroyInstance(m_instance, nullptr);
}

void vulkan_device::refresh()
{
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(this->gpu, m_surface, &this->capabilities);

    this->minImageCount = this->capabilities.minImageCount + 1;
    if(this->capabilities.maxImageCount > 0 && this->minImageCount > this->capabilities.maxImageCount)
        this->minImageCount = this->capabilities.maxImageCount;

    this->extent = this->capabilities.currentExtent;
    if(this->capabilities.currentExtent.width == 0xFFFFFFFF){
        this->extent.width = clamp(this->extent.width, this->capabilities.minImageExtent.width,
                                   this->capabilities.maxImageExtent.width);
        this->extent.height = clamp(this->extent.height, this->capabilities.minImageExtent.height,
                                    this->capabilities.maxImageExtent.height);
    }
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
    info.surface = m_surface;
    info.minImageCount = this->minImageCount;
    info.imageFormat = this->surfaceFormat.format;
    info.imageColorSpace = this->surfaceFormat.colorSpace;
    info.imageExtent = this->extent;
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

VkMemoryAllocateInfo vulkan_device::getMemoryAllocInfo(VkMemoryRequirements memReqs,
                                                       VkMemoryPropertyFlags flags) const
{
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(this->gpu, &memProps);

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

void vulkan_device::pickPhysicalDevice()
{
    uint32_t physDeviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &physDeviceCount, nullptr);
    auto physDevices = dyn_array<VkPhysicalDevice>(physDeviceCount);
    vkEnumeratePhysicalDevices(m_instance, &physDeviceCount, physDevices.data());

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
        auto availableExtensions = dyn_array<VkExtensionProperties>(extensionCount);
        vkEnumerateDeviceExtensionProperties(physDevices[i], nullptr, &extensionCount, availableExtensions.data());
        const bool extensionsSupported = findExtensionProperty(availableExtensions);

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDevices[i], m_surface, &this->capabilities);

        uint32_t formatCount = 0, presentModeCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physDevices[i], m_surface, &formatCount, nullptr);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physDevices[i], m_surface, &presentModeCount, nullptr);
        const bool adequateCapabilities = (formatCount > 0) && (presentModeCount > 0);

        uint32_t propCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physDevices[i], &propCount, nullptr);
        auto queueFamilyProps = dyn_array<VkQueueFamilyProperties>(propCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physDevices[i], &propCount, queueFamilyProps.data());

        this->graphics.family = 0;
        this->present.family = 0;
        bool hasGraphicsFamily = false, hasPresentFamily = false;
        for(uint32_t queueIndex = 0; queueIndex < propCount; queueIndex++){
            if(queueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT){
                hasGraphicsFamily = true;
                this->graphics.family = queueIndex;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physDevices[i], queueIndex, m_surface, &presentSupport);
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

void vulkan_device::pickSurfaceFormat()
{
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(this->gpu, m_surface, &count, nullptr);
    auto surfaceFormats = dyn_array<VkSurfaceFormatKHR>(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(this->gpu, m_surface, &count, surfaceFormats.data());

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

void vulkan_device::pickPresentMode(VkPresentModeKHR preferredMode)
{
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(this->gpu, m_surface, &count, nullptr);
    auto presentModes = dyn_array<VkPresentModeKHR>(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(this->gpu, m_surface, &count, presentModes.data());

    this->presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    for(size_t i = 0; i < count; i++){
        if(presentModes[i] == preferredMode){
            this->presentMode = preferredMode;
            break;
        }
    }
}

void vulkan_device::getSampleCount()
{
    VkPhysicalDeviceProperties physicalDeviceProperties{};
    vkGetPhysicalDeviceProperties(this->gpu, &physicalDeviceProperties);
    const auto colourBits = physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    const auto depthBits = physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    const VkSampleCountFlags samples = colourBits & depthBits;

    this->sampleCount = VK_SAMPLE_COUNT_1_BIT;
    for(VkSampleCountFlags bit = VK_SAMPLE_COUNT_64_BIT; bit != VK_SAMPLE_COUNT_1_BIT; bit >>= 1){
        if(samples & bit){
            this->sampleCount = VkSampleCountFlagBits(bit);
            break;
        }
    }
}
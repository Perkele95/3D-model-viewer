#include "model_viewer.hpp"

model_viewer::model_viewer(mv_allocator *allocator, vec2<int32_t> extent, uint32_t flags)
{
    this->hDevice = allocator->allocPermanent<vulkan_device>(1);
    this->hOverlay = allocator->allocPermanent<text_overlay>(1);

    this->hDevice->create(allocator, flags & CORE_FLAG_ENABLE_VALIDATION, flags & CORE_FLAG_ENABLE_VSYNC);

    this->extent = this->hDevice->getExtent();
    this->depthFormat = this->hDevice->getDepthFormat();
    this->sampleCount = this->hDevice->getSampleCount();
    buildResources(allocator);

    VkShaderModule vertexModule, fragmentModule;
    auto shader = io::read("../shaders/gui_vert.spv");
    this->hDevice->loadShader(&shader, &vertexModule);
    io::close(shader);

    shader = io::read("../shaders/gui_frag.spv");
    this->hDevice->loadShader(&shader, &fragmentModule);
    io::close(shader);

    text_overlay_create_info overlayInfo;
    overlayInfo.allocator = allocator;
    overlayInfo.device = this->hDevice;
    overlayInfo.cmdPool = this->cmdPool;
    overlayInfo.imageCount = this->imageCount;
    overlayInfo.renderPass = this->renderPass;
    overlayInfo.vertex = vertexModule;
    overlayInfo.fragment = fragmentModule;
    this->hOverlay->create(&overlayInfo);

    this->currentFrame = 0;

    this->hOverlay->setTextTint(vec4(1.0f));
    this->hOverlay->setTextAlignment(text_align::centre);
    this->hOverlay->setTextType(text_coord_type::relative);
    this->hOverlay->setTextSize(1.0f);

    this->hOverlay->begin();

    constexpr auto titleString = view("3D model viewer");
    this->hOverlay->draw(titleString, vec2(50.0f, 20.0f));

    this->hOverlay->end();
    this->hOverlay->updateCmdBuffers(this->framebuffers);
}

model_viewer::~model_viewer()
{
    vkDeviceWaitIdle(this->hDevice->device);
    this->hOverlay->destroy();

    vkDestroyCommandPool(this->hDevice->device, this->cmdPool, nullptr);

    for (size_t i = 0; i < this->imageCount; i++)
        vkDestroyFramebuffer(this->hDevice->device, this->framebuffers[i], nullptr);

    vkDestroyRenderPass(this->hDevice->device, this->renderPass, nullptr);
    this->depth.destroy(this->hDevice->device);
    this->msaa.destroy(this->hDevice->device);

    for (size_t i = 0; i < this->imageCount; i++)
        vkDestroyImageView(this->hDevice->device, this->swapchainViews[i], nullptr);

    vkDestroySwapchainKHR(this->hDevice->device, this->swapchain, nullptr);

    for (size_t i = 0; i < MAX_IMAGES_IN_FLIGHT; i++){
        vkDestroyFence(this->hDevice->device, inFlightFences[i], nullptr);
        vkDestroySemaphore(this->hDevice->device, this->renderFinishedSPs[i], nullptr);
        vkDestroySemaphore(this->hDevice->device, this->imageAvailableSPs[i], nullptr);
    }

    this->hDevice->destroy();
}

void model_viewer::run(mv_allocator *allocator, uint32_t flags, float dt)
{
    if(this->extent.width == 0 || this->extent.height == 0)
        return;

    vkDeviceWaitIdle(this->hDevice->device);

    // draw

    vkWaitForFences(this->hDevice->device, 1, &this->inFlightFences[this->currentFrame],
                    VK_TRUE, UINT64_MAX);

    uint32_t imageIndex = 0;
    VkResult result = vkAcquireNextImageKHR(this->hDevice->device, this->swapchain, UINT64_MAX,
                                            this->imageAvailableSPs[this->currentFrame], VK_NULL_HANDLE, &imageIndex);

    switch(result){
        case VK_ERROR_OUT_OF_DATE_KHR: onWindowResize(allocator); break;
        case VK_SUBOPTIMAL_KHR: //DebugLog("Suboptimal swapchain"); break;
        default: break;
    }

    if(this->imagesInFlight[imageIndex] != VK_NULL_HANDLE)
        vkWaitForFences(this->hDevice->device, 1, &this->imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);

    this->imagesInFlight[imageIndex] = this->inFlightFences[this->currentFrame];

    vkDeviceWaitIdle(this->hDevice->device);

    //update command buffers

    const VkSemaphore waitSemaphores[] = {this->imageAvailableSPs[this->currentFrame]};
    const VkSemaphore signalSemaphores[] = {this->renderFinishedSPs[this->currentFrame]};
    const VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    auto overlaySubmit = this->hOverlay->getSubmitData();
    overlaySubmit.waitSemaphoreCount = 1;
    overlaySubmit.pWaitSemaphores = waitSemaphores;
    overlaySubmit.pWaitDstStageMask = waitStages;
    overlaySubmit.signalSemaphoreCount = 1;
    overlaySubmit.pSignalSemaphores = signalSemaphores;

    vkResetFences(this->hDevice->device, 1, &this->inFlightFences[this->currentFrame]);
    vkQueueSubmit(this->hDevice->graphics.queue, 1, &overlaySubmit,
                  this->inFlightFences[this->currentFrame]);

    this->currentFrame = (this->currentFrame + 1) % MAX_IMAGES_IN_FLIGHT;

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &this->swapchain;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(this->hDevice->present.queue, &presentInfo);
    switch(result){
        case VK_ERROR_OUT_OF_DATE_KHR:
        case VK_SUBOPTIMAL_KHR: onWindowResize(allocator); break;
        default: break;
    }

    if(flags & CORE_FLAG_WINDOW_RESIZED)
        onWindowResize(allocator);
}
// TODO(arle)
void model_viewer::onWindowResize(mv_allocator *allocator)
{
    vkDeviceWaitIdle(this->hDevice->device);

    for (size_t i = 0; i < this->imageCount; i++)
        vkDestroyFramebuffer(this->hDevice->device, this->framebuffers[i], nullptr);

    this->msaa.destroy(this->hDevice->device);
    this->depth.destroy(this->hDevice->device);
    vkResetCommandPool(this->hDevice->device, this->cmdPool, 0);// VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT
    vkDestroyRenderPass(this->hDevice->device, this->renderPass, nullptr);

    for (size_t i = 0; i < this->imageCount; i++)
        vkDestroyImageView(this->hDevice->device, this->swapchainViews[i], nullptr);

    this->hDevice->refresh();
    this->extent = this->hDevice->getExtent();

    auto oldSwapchain = this->swapchain;
    this->hDevice->buildSwapchain(this->swapchain, &this->swapchain);
    vkDestroySwapchainKHR(this->hDevice->device, oldSwapchain, nullptr);

    uint32_t localImageCount = 0;
    vkGetSwapchainImagesKHR(this->hDevice->device, this->swapchain, &localImageCount, nullptr);
    vkGetSwapchainImagesKHR(this->hDevice->device, this->swapchain, &localImageCount, this->swapchainImages);
    this->imageCount = localImageCount;

    buildSwapchainViews();
    buildRenderPass();
    buildDepth();
    buildMsaa();
    buildFramebuffers();

    this->hOverlay->onWindowResize(allocator, this->hDevice, this->cmdPool, this->renderPass);
    this->hOverlay->updateCmdBuffers(this->framebuffers);
}

void model_viewer::buildResources(mv_allocator *allocator)
{
    auto poolInfo = vkInits::commandPoolCreateInfo(this->hDevice->graphics.family);
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCreateCommandPool(this->hDevice->device, &poolInfo, nullptr, &this->cmdPool);

    this->hDevice->buildSwapchain(VK_NULL_HANDLE, &this->swapchain);

    uint32_t localImageCount = 0;
    vkGetSwapchainImagesKHR(this->hDevice->device, this->swapchain, &localImageCount, nullptr);
    this->imageCount = localImageCount;

    this->swapchainImages = allocator->allocPermanent<VkImage>(this->imageCount);
    this->swapchainViews = allocator->allocPermanent<VkImageView>(this->imageCount);
    this->framebuffers = allocator->allocPermanent<VkFramebuffer>(this->imageCount);
    this->imagesInFlight = allocator->allocPermanent<VkFence>(this->imageCount);

    vkGetSwapchainImagesKHR(this->hDevice->device, this->swapchain, &localImageCount, this->swapchainImages);

    buildSwapchainViews();
    buildMsaa();
    buildDepth();
    buildRenderPass();
    buildFramebuffers();
    buildSyncObjects();
}

void model_viewer::buildSwapchainViews()
{
    for(size_t i = 0; i < this->imageCount; i++){
        auto imageViewInfo = vkInits::imageViewCreateInfo();
        imageViewInfo.format = this->hDevice->surfaceFormat.format;
        imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewInfo.image = this->swapchainImages[i];
        vkCreateImageView(this->hDevice->device, &imageViewInfo, nullptr, &this->swapchainViews[i]);
    }
}

void model_viewer::buildMsaa()
{
    auto imageInfo = vkInits::imageCreateInfo();
    imageInfo.samples = this->sampleCount;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.extent = {this->extent.width, this->extent.height, 1};
    imageInfo.format = this->hDevice->surfaceFormat.format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    vkCreateImage(this->hDevice->device, &imageInfo, nullptr, &this->msaa.image);

    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(this->hDevice->device, this->msaa.image, &memReqs);

    auto allocInfo = GetMemoryAllocInfo(this->hDevice->gpu, memReqs, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(this->hDevice->device, &allocInfo, nullptr, &this->msaa.memory);
    vkBindImageMemory(this->hDevice->device, this->msaa.image, this->msaa.memory, 0);

    auto viewInfo = vkInits::imageViewCreateInfo();
    viewInfo.image = this->msaa.image;
    viewInfo.format = this->hDevice->surfaceFormat.format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vkCreateImageView(this->hDevice->device, &viewInfo, nullptr, &this->msaa.view);
}

void model_viewer::buildDepth()
{
    auto imageInfo = vkInits::imageCreateInfo();
    imageInfo.extent = {this->extent.width, this->extent.height, 1};
    imageInfo.format = this->depthFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.samples = this->sampleCount;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    VkResult result = vkCreateImage(this->hDevice->device, &imageInfo, nullptr, &this->depth.image);

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(this->hDevice->device, this->depth.image, &memReqs);

    auto allocInfo = GetMemoryAllocInfo(this->hDevice->gpu, memReqs, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    result = vkAllocateMemory(this->hDevice->device, &allocInfo, nullptr, &this->depth.memory);
    result = vkBindImageMemory(this->hDevice->device, this->depth.image, this->depth.memory, 0);

    auto depthCreateInfo = vkInits::imageViewCreateInfo();
    depthCreateInfo.image = this->depth.image;
    depthCreateInfo.format = this->depthFormat;
    depthCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    result = vkCreateImageView(this->hDevice->device, &depthCreateInfo, nullptr, &this->depth.view);
}

void model_viewer::buildRenderPass()
{
    constexpr auto finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    auto colourAttachment = vkInits::attachmentDescription(this->hDevice->surfaceFormat.format, finalLayout);
    colourAttachment.samples = this->sampleCount;

    constexpr auto finalLayoutDepth = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    auto depthAttachment = vkInits::attachmentDescription(this->depthFormat, finalLayoutDepth);
    depthAttachment.samples = this->sampleCount;

    constexpr auto finalLayoutResolve = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    auto colourResolve = vkInits::attachmentDescription(this->hDevice->surfaceFormat.format, finalLayoutResolve);
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

    vkCreateRenderPass(this->hDevice->device, &renderPassInfo, nullptr, &this->renderPass);
}

void model_viewer::buildFramebuffers()
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
        vkCreateFramebuffer(this->hDevice->device, &framebufferInfo, nullptr, &this->framebuffers[i]);
    }
}

void model_viewer::buildSyncObjects()
{
    auto semaphoreInfo = vkInits::semaphoreCreateInfo();
    auto fenceInfo = vkInits::fenceCreateInfo();

    for (size_t i = 0; i < MAX_IMAGES_IN_FLIGHT; i++){
        vkCreateSemaphore(this->hDevice->device, &semaphoreInfo, nullptr, &this->imageAvailableSPs[i]);
        vkCreateSemaphore(this->hDevice->device, &semaphoreInfo, nullptr, &this->renderFinishedSPs[i]);
        vkCreateFence(this->hDevice->device, &fenceInfo, nullptr, &this->inFlightFences[i]);
    }
}
#include "model_viewer.hpp"
#include "backend/vulkan_tools.hpp"

constexpr static VkVertexInputAttributeDescription s_MeshAttributes[] = {
    VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(mesh_vertex, position)},
    VkVertexInputAttributeDescription{1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(mesh_vertex, normal)},
    VkVertexInputAttributeDescription{2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(mesh_vertex, colour)}
};

static constexpr auto s_MeshSzf = 0.5f;
constexpr auto CUBE_TINT = vec4(0.1f, 1.0f, 0.1f, 1.0f);

static constexpr auto s_NormalFront = vec3(0.0f, 0.0f, -1.0f);
static constexpr auto s_NormalBack = vec3(0.0f, 0.0f, 1.0f);
static constexpr auto s_NormalTop = vec3(0.0f, 1.0f, 0.0f);
static constexpr auto s_NormalBottom = vec3(0.0f, -1.0f, 0.0f);
static constexpr auto s_NormalLeft = vec3(1.0f, 0.0f, 0.0f);
static constexpr auto s_NormalRight = vec3(-1.0f, 0.0f, 0.0f);

static mesh_vertex s_MeshVertices[] = {
    // Front
    {vec3(-s_MeshSzf, -s_MeshSzf, -s_MeshSzf), s_NormalFront, CUBE_TINT},
    {vec3(s_MeshSzf, -s_MeshSzf, -s_MeshSzf), s_NormalFront, CUBE_TINT},
    {vec3(s_MeshSzf, s_MeshSzf, -s_MeshSzf), s_NormalFront, CUBE_TINT},
    {vec3(-s_MeshSzf, s_MeshSzf, -s_MeshSzf), s_NormalFront, CUBE_TINT},
    // Back
    {vec3(-s_MeshSzf, -s_MeshSzf, s_MeshSzf), s_NormalBack, CUBE_TINT},
    {vec3(s_MeshSzf, -s_MeshSzf, s_MeshSzf), s_NormalBack, CUBE_TINT},
    {vec3(s_MeshSzf, s_MeshSzf, s_MeshSzf), s_NormalBack, CUBE_TINT},
    {vec3(-s_MeshSzf, s_MeshSzf, s_MeshSzf), s_NormalBack, CUBE_TINT},
    // Top
    {vec3(-s_MeshSzf, s_MeshSzf, -s_MeshSzf), s_NormalTop, CUBE_TINT},
    {vec3(s_MeshSzf, s_MeshSzf, -s_MeshSzf), s_NormalTop, CUBE_TINT},
    {vec3(s_MeshSzf, s_MeshSzf, s_MeshSzf), s_NormalTop, CUBE_TINT},
    {vec3(-s_MeshSzf, s_MeshSzf, s_MeshSzf), s_NormalTop, CUBE_TINT},
    // Bottom
    {vec3(-s_MeshSzf, -s_MeshSzf, s_MeshSzf), s_NormalBottom, CUBE_TINT},
    {vec3(s_MeshSzf, -s_MeshSzf, s_MeshSzf), s_NormalBottom, CUBE_TINT},
    {vec3(s_MeshSzf, -s_MeshSzf, -s_MeshSzf), s_NormalBottom, CUBE_TINT},
    {vec3(-s_MeshSzf, -s_MeshSzf, -s_MeshSzf), s_NormalBottom, CUBE_TINT},
    // Left
    {vec3(-s_MeshSzf, -s_MeshSzf, s_MeshSzf), s_NormalLeft ,CUBE_TINT},
    {vec3(-s_MeshSzf, -s_MeshSzf, -s_MeshSzf), s_NormalLeft, CUBE_TINT},
    {vec3(-s_MeshSzf, s_MeshSzf, -s_MeshSzf), s_NormalLeft, CUBE_TINT},
    {vec3(-s_MeshSzf, s_MeshSzf, s_MeshSzf), s_NormalLeft, CUBE_TINT},
    // Right
    {vec3(s_MeshSzf, -s_MeshSzf, -s_MeshSzf), s_NormalRight, CUBE_TINT},
    {vec3(s_MeshSzf, -s_MeshSzf, s_MeshSzf), s_NormalRight, CUBE_TINT},
    {vec3(s_MeshSzf, s_MeshSzf, s_MeshSzf), s_NormalRight, CUBE_TINT},
    {vec3(s_MeshSzf, s_MeshSzf, -s_MeshSzf), s_NormalRight, CUBE_TINT},
};

static mesh_index s_MeshIndices[] = {
    0, 1, 2, 2, 3, 0, // Front
    4, 7, 6, 6, 5, 4, // Back
    8, 9, 10, 10, 11, 8, // Top
    12, 13, 14, 14, 15, 12, // Bottom
    16, 17, 18, 18, 19, 16, // Left
    20, 21, 22, 22, 23, 20, // Right
};

model_viewer::model_viewer(mv_allocator *allocator, vec2<int32_t> extent, uint32_t flags)
{
    this->hDevice = allocator->allocPermanent<vulkan_device>(1);
    this->hOverlay = allocator->allocPermanent<text_overlay>(1);

    this->hDevice->create(allocator, flags & CORE_FLAG_ENABLE_VALIDATION, flags & CORE_FLAG_ENABLE_VSYNC);

    this->depthFormat = this->hDevice->getDepthFormat();
    this->mainCamera = camera(float(this->hDevice->extent.width) / float(this->hDevice->extent.height));

    buildResources(allocator);

    VkShaderModule vertexModule, fragmentModule;
    auto shader = io::read("../shaders/gui_vert.spv");
    this->hDevice->loadShader(&shader, &vertexModule);
    io::close(&shader);

    shader = io::read("../shaders/gui_frag.spv");
    this->hDevice->loadShader(&shader, &fragmentModule);
    io::close(&shader);

    text_overlay_create_info overlayInfo;
    overlayInfo.allocator = allocator;
    overlayInfo.device = this->hDevice;
    overlayInfo.cmdPool = this->cmdPool;
    overlayInfo.imageCount = this->imageCount;
    overlayInfo.vertex = vertexModule;
    overlayInfo.fragment = fragmentModule;
    overlayInfo.depthFormat = this->depthFormat;
    this->hOverlay->create(&overlayInfo);

    this->currentFrame = 0;

    this->hOverlay->setTextTint(vec4(1.0f));
    this->hOverlay->setTextAlignment(text_align::centre);
    this->hOverlay->setTextType(text_coord_type::relative);
    this->hOverlay->setTextSize(1.0f);

    this->hOverlay->begin();

    constexpr auto titleString = view("3D model viewer");
    this->hOverlay->draw(titleString, vec2(50.0f, 12.0f));

    this->hOverlay->end();
    this->hOverlay->updateCmdBuffers(this->framebuffers);
}

model_viewer::~model_viewer()
{
    vkDeviceWaitIdle(this->hDevice->device);
    this->hOverlay->destroy();

    vkDestroyCommandPool(this->hDevice->device, this->cmdPool, nullptr);

    vkDestroyPipeline(this->hDevice->device, this->pipeline, nullptr);
    vkDestroyPipelineLayout(this->hDevice->device, this->pipelineLayout, nullptr);
    vkDestroyShaderModule(this->hDevice->device, this->vertShaderModule, nullptr);
    vkDestroyShaderModule(this->hDevice->device, this->fragShaderModule, nullptr);
    this->vertexBuffer.destroy(this->hDevice->device);
    this->indexBuffer.destroy(this->hDevice->device);

    for (size_t i = 0; i < this->imageCount; i++)
        vkDestroyFramebuffer(this->hDevice->device, this->framebuffers[i], nullptr);

    this->depth.destroy(this->hDevice->device);
    this->msaa.destroy(this->hDevice->device);
    vkDestroyRenderPass(this->hDevice->device, this->renderPass, nullptr);

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

void model_viewer::testProc(const input_state *input)
{
    if(input->mouseWheel){
        const float factor = input->mouseWheel * 0.05f;
        this->mainCamera.model *= mat4x4::rotateY(factor);
    }
}

void model_viewer::run(mv_allocator *allocator, const input_state *input, uint32_t flags, float dt)
{
    if(this->hDevice->extent.width == 0 || this->hDevice->extent.height == 0)
        return;

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

    testProc(input);

    vkQueueWaitIdle(this->hDevice->graphics.queue);//TODO(arle): replace with fence
    updateCmdBuffers();

    if(this->imagesInFlight[imageIndex] != VK_NULL_HANDLE)
        vkWaitForFences(this->hDevice->device, 1, &this->imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);

    this->imagesInFlight[imageIndex] = this->inFlightFences[this->currentFrame];

    const VkSemaphore waitSemaphores[] = {this->imageAvailableSPs[this->currentFrame]};
    const VkSemaphore signalSemaphores[] = {this->renderFinishedSPs[this->currentFrame]};
    const VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    vkResetFences(this->hDevice->device, 1, &this->inFlightFences[this->currentFrame]);

    VkCommandBuffer submitCmds[] = {
        this->commandBuffers[imageIndex],
        this->hOverlay->cmdBuffers[imageIndex]
    };

    auto submitInfo = vkInits::submitInfo(submitCmds, arraysize(submitCmds));
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
    vkQueueSubmit(this->hDevice->graphics.queue, 1, &submitInfo,
                  this->inFlightFences[this->currentFrame]);

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &this->swapchain;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(this->hDevice->present.queue, &presentInfo);

    if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
       flags & CORE_FLAG_WINDOW_RESIZED)
    {
        onWindowResize(allocator);
    }

    this->currentFrame = (this->currentFrame + 1) % MAX_IMAGES_IN_FLIGHT;
}

void model_viewer::onWindowResize(mv_allocator *allocator)
{
    vkDeviceWaitIdle(this->hDevice->device);

    this->hDevice->refresh();

    if(this->hDevice->extent.width == 0 || this->hDevice->extent.height == 0)
        return;

    for (size_t i = 0; i < this->imageCount; i++)
        vkDestroyFramebuffer(this->hDevice->device, this->framebuffers[i], nullptr);

    this->msaa.destroy(this->hDevice->device);
    this->depth.destroy(this->hDevice->device);
    vkResetCommandPool(this->hDevice->device, this->cmdPool, 0);// VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT
    vkDestroyRenderPass(this->hDevice->device, this->renderPass, nullptr);

    for (size_t i = 0; i < this->imageCount; i++)
        vkDestroyImageView(this->hDevice->device, this->swapchainViews[i], nullptr);

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

    this->hOverlay->onWindowResize(allocator, this->cmdPool);
    this->hOverlay->updateCmdBuffers(this->framebuffers);
    this->mainCamera.update(float(this->hDevice->extent.width) / float(this->hDevice->extent.height));
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
    this->commandBuffers = allocator->allocPermanent<VkCommandBuffer>(this->imageCount);

    vkGetSwapchainImagesKHR(this->hDevice->device, this->swapchain, &localImageCount, this->swapchainImages);

    buildSwapchainViews();
    buildMsaa();
    buildDepth();
    buildRenderPass();
    buildFramebuffers();
    buildSyncObjects();

    auto cmdInfo = vkInits::commandBufferAllocateInfo(this->cmdPool, this->imageCount);
    vkAllocateCommandBuffers(this->hDevice->device, &cmdInfo, this->commandBuffers);

#if 0
    auto descriptorPoolInfo = vkInits::descriptorPoolCreateInfo();
    descriptorPoolInfo.pPoolSizes = poolSizes;
    descriptorPoolInfo.poolSizeCount = uint32_t(arraysize(poolSizes));
    descriptorPoolInfo.maxSets = 0;
    vkCreateDescriptorPool(this->hDevice->device, &descriptorPoolInfo, nullptr, &this->descriptorPool);

    // build set layout
#endif
    auto vertexShader = io::read("../shaders/scene_vert.spv");
    auto fragmentShader = io::read("../shaders/scene_frag.spv");

    hDevice->loadShader(&vertexShader, &this->vertShaderModule);
    hDevice->loadShader(&fragmentShader, &this->fragShaderModule);

    io::close(&vertexShader);
    io::close(&fragmentShader);

    buildDescriptorSets();
    buildPipeline();
    buildMeshBuffers();

    updateCmdBuffers();
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
    imageInfo.samples = this->hDevice->sampleCount;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.extent = {this->hDevice->extent.width, this->hDevice->extent.height, 1};
    imageInfo.format = this->hDevice->surfaceFormat.format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    vkCreateImage(this->hDevice->device, &imageInfo, nullptr, &this->msaa.image);

    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(this->hDevice->device, this->msaa.image, &memReqs);

    auto allocInfo = vkTools::GetMemoryAllocInfo(this->hDevice->gpu, memReqs, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
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
    imageInfo.extent = {this->hDevice->extent.width, this->hDevice->extent.height, 1};
    imageInfo.format = this->depthFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.samples = this->hDevice->sampleCount;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    VkResult result = vkCreateImage(this->hDevice->device, &imageInfo, nullptr, &this->depth.image);

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(this->hDevice->device, this->depth.image, &memReqs);

    auto allocInfo = vkTools::GetMemoryAllocInfo(this->hDevice->gpu, memReqs, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
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
    auto colourAttachment = vkInits::attachmentDescription(this->hDevice->surfaceFormat.format);
    colourAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colourAttachment.samples = this->hDevice->sampleCount;

    auto depthAttachment = vkInits::attachmentDescription(this->depthFormat);
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.samples = this->hDevice->sampleCount;

    auto colourResolve = vkInits::attachmentDescription(this->hDevice->surfaceFormat.format);
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

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = uint32_t(arraysize(attachments));
    renderPassInfo.pAttachments = attachments;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = uint32_t(arraysize(dependencies));
    renderPassInfo.pDependencies = dependencies;

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
        framebufferInfo.width = this->hDevice->extent.width;
        framebufferInfo.height = this->hDevice->extent.height;
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

void model_viewer::buildDescriptorSets()
{
    // TODO(arle)
    // NOTE(arle): not yet required
}

void model_viewer::buildPipeline()
{
    const VkPipelineShaderStageCreateInfo shaderStages[] = {
        vkInits::shaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT, this->vertShaderModule),
        vkInits::shaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, this->fragShaderModule),
    };

    auto bindingDescription = vkInits::vertexBindingDescription(sizeof(mesh_vertex));

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = uint32_t(arraysize(s_MeshAttributes));
    vertexInputInfo.pVertexAttributeDescriptions = s_MeshAttributes;

    auto inputAssembly = vkInits::inputAssemblyInfo();
    auto viewport = vkInits::viewportInfo(this->hDevice->extent);
    auto scissor = vkInits::scissorInfo(this->hDevice->extent);

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    auto rasterizer = vkInits::rasterizationStateInfo(VK_FRONT_FACE_CLOCKWISE);
    auto multisampling = vkInits::pipelineMultisampleStateCreateInfo(this->hDevice->sampleCount);

    auto depthStencil = vkInits::depthStencilStateInfo();
    auto colorBlendAttachment = vkInits::pipelineColorBlendAttachmentState();
    auto colourBlend = vkInits::pipelineColorBlendStateCreateInfo();
    colourBlend.attachmentCount = 1;
    colourBlend.pAttachments = &colorBlendAttachment;

    auto cameraPushConstant = camera_matrix::pushConstant();

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pSetLayouts = nullptr;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &cameraPushConstant;//TODO(arle): Add descriptor layout info
    vkCreatePipelineLayout(this->hDevice->device, &pipelineLayoutInfo, nullptr, &this->pipelineLayout);

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = uint32_t(arraysize(shaderStages));
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colourBlend;
    pipelineInfo.layout = this->pipelineLayout;
    pipelineInfo.renderPass = this->renderPass;
    vkCreateGraphicsPipelines(this->hDevice->device, VK_NULL_HANDLE, 1,
                              &pipelineInfo, nullptr, &this->pipeline);
}

void model_viewer::buildMeshBuffers()
{
    constexpr auto tint = vec4(1.0f);

    buffer_t vertexTransfer{}, indexTransfer{};
    vkTools::CreateBuffer(this->hDevice->device,
                          this->hDevice->gpu,
                          sizeof(s_MeshVertices),
                          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                          VISIBLE_BUFFER_FLAGS,
                          &vertexTransfer);

    vkTools::CreateBuffer(this->hDevice->device,
                          this->hDevice->gpu,
                          vertexTransfer.size,
                          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                          &this->vertexBuffer);

    vkTools::CreateBuffer(this->hDevice->device,
                          this->hDevice->gpu,
                          sizeof(s_MeshIndices),
                          VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                          VISIBLE_BUFFER_FLAGS,
                          &indexTransfer);

    vkTools::CreateBuffer(this->hDevice->device,
                          this->hDevice->gpu,
                          indexTransfer.size,
                          VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                          &this->indexBuffer);

    vkTools::FillBuffer(this->hDevice->device, &vertexTransfer, s_MeshVertices, sizeof(s_MeshVertices));
    vkTools::FillBuffer(this->hDevice->device, &indexTransfer, s_MeshIndices, sizeof(s_MeshIndices));

    VkCommandBuffer cpyCmds[] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
    auto cmdInfo = vkInits::commandBufferAllocateInfo(this->cmdPool, arraysize(cpyCmds));
    vkAllocateCommandBuffers(this->hDevice->device, &cmdInfo, cpyCmds);

    vkTools::CmdCopyBuffer(cpyCmds[0], &this->vertexBuffer, &vertexTransfer);
    vkTools::CmdCopyBuffer(cpyCmds[1], &this->indexBuffer, &indexTransfer);

    auto submitInfo = vkInits::submitInfo(cpyCmds, arraysize(cpyCmds));
    vkQueueSubmit(this->hDevice->graphics.queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(this->hDevice->graphics.queue);

    vkFreeCommandBuffers(this->hDevice->device, this->cmdPool, uint32_t(arraysize(cpyCmds)), cpyCmds);

    vertexTransfer.destroy(this->hDevice->device);
    indexTransfer.destroy(this->hDevice->device);
}

void model_viewer::updateCmdBuffers()
{
    VkClearValue colourValue;
    colourValue.color = {};
    VkClearValue depthStencilValue;
    depthStencilValue.depthStencil = {1.0f, 0};

    const VkClearValue clearValues[] = {colourValue, depthStencilValue};

    auto cmdBeginInfo = vkInits::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);
    auto renderBeginInfo = vkInits::renderPassBeginInfo(this->renderPass, this->hDevice->extent);
    renderBeginInfo.clearValueCount = 1;
    renderBeginInfo.pClearValues = clearValues;
    renderBeginInfo.clearValueCount = uint32_t(arraysize(clearValues));

    for (size_t i = 0; i < this->imageCount; i++){
        const auto cmdBuffer = this->commandBuffers[i];
        vkBeginCommandBuffer(cmdBuffer, &cmdBeginInfo);

        renderBeginInfo.framebuffer = this->framebuffers[i];
        vkCmdBeginRenderPass(cmdBuffer, &renderBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        auto cameraConstant = camera_matrix::pushConstant();
        auto cameraData = camera_matrix();
        cameraData.model = this->mainCamera.model;
        cameraData.view = this->mainCamera.view;
        cameraData.proj = this->mainCamera.proj;
        vkCmdPushConstants(cmdBuffer,
                           this->pipelineLayout,
                           cameraConstant.stageFlags,
                           cameraConstant.offset,
                           cameraConstant.size,
                           &cameraData);

        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, this->pipeline);
        //vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, this->pipelineLayout,
        //                        0, 1, &this->descriptorSets[i], 0, nullptr);

        const VkDeviceSize vertexOffset = 0;
        vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &this->vertexBuffer.data, &vertexOffset);

        const VkDeviceSize indexOffset = 0;
        vkCmdBindIndexBuffer(cmdBuffer, this->indexBuffer.data, indexOffset, VK_INDEX_TYPE_UINT32);

        const auto indexCount = uint32_t(arraysize(s_MeshIndices));
        vkCmdDrawIndexed(cmdBuffer, indexCount, 1, 0, 0, 0);

        vkCmdEndRenderPass(cmdBuffer);
        vkEndCommandBuffer(cmdBuffer);
    }
}
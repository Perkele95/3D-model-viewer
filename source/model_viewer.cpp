#include "model_viewer.hpp"

#if defined(DEBUG)
static const bool C_VALIDATION = true;
#else
static const bool C_VALIDATION = false;
#endif

model_viewer::model_viewer(pltf::logical_device device)
: m_permanentStorage(MegaBytes(64)), m_currentFrame(0)
{
    m_device = m_permanentStorage.push<vulkan_device>(1);
    m_overlay = m_permanentStorage.push<text_overlay>(1);

    new (m_device) vulkan_device(device, C_VALIDATION, false);

    m_depthFormat = m_device->getDepthFormat();

    buildResources();

    text_overlay_create_info overlayInfo;
    overlayInfo.sharedPermanent = &m_permanentStorage;
    overlayInfo.device = m_device;
    overlayInfo.cmdPool = m_cmdPool;
    overlayInfo.imageCount = m_imageCount;
    overlayInfo.depthFormat = m_depthFormat;
    new (m_overlay) text_overlay(&overlayInfo);

    m_overlay->textTint = vec4(1.0f);
    m_overlay->textAlignment = text_align::centre;
    m_overlay->textType = text_coord_type::relative;
    m_overlay->textSize = 1.0f;

    m_overlay->begin();

    constexpr auto titleString = view("PBR demo");
    m_overlay->draw(titleString, vec2(50.0f, 12.0f));

    m_overlay->end();
    m_overlay->updateCmdBuffers(m_framebuffers);
}

model_viewer::~model_viewer()
{
    vkDeviceWaitIdle(m_device->device);
    m_overlay->~text_overlay();

    vkDestroyCommandPool(m_device->device, m_cmdPool, nullptr);

    vkDestroyPipeline(m_device->device, m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device->device, m_pipelineLayout, nullptr);

    for (size_t i = 0; i < arraysize(m_shaders); i++)
        m_shaders[i].destroy(m_device->device);

    vkDestroyDescriptorPool(m_device->device, m_descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_device->device, m_descriptorSetLayout, nullptr);

    m_mainCamera.destroy(m_device->device);

    for(size_t i = 0; i < m_imageCount; i++)
        m_uniformBuffers[i].lights.destroy(m_device->device);

    m_model.destroy();

    for (size_t i = 0; i < m_imageCount; i++)
        vkDestroyFramebuffer(m_device->device, m_framebuffers[i], nullptr);

    m_depth.destroy(m_device->device);
    m_msaa.destroy(m_device->device);
    vkDestroyRenderPass(m_device->device, m_renderPass, nullptr);

    for (size_t i = 0; i < m_imageCount; i++)
        vkDestroyImageView(m_device->device, m_swapchainViews[i], nullptr);

    vkDestroySwapchainKHR(m_device->device, m_swapchain, nullptr);

    for (size_t i = 0; i < MAX_IMAGES_IN_FLIGHT; i++){
        vkDestroyFence(m_device->device, m_inFlightFences[i], nullptr);
        vkDestroySemaphore(m_device->device, m_renderFinishedSPs[i], nullptr);
        vkDestroySemaphore(m_device->device, m_imageAvailableSPs[i], nullptr);
    }

    m_device->~vulkan_device();
}

void model_viewer::swapBuffers(pltf::logical_device device)
{
    if(m_device->extent.width == 0 || m_device->extent.height == 0)
        return;

    gameUpdate(pltf::GetTimestep(device));

    vkWaitForFences(m_device->device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex = 0;
    VkResult result = vkAcquireNextImageKHR(m_device->device, m_swapchain, UINT64_MAX,
                                            m_imageAvailableSPs[m_currentFrame], VK_NULL_HANDLE, &imageIndex);

    switch(result){
        case VK_ERROR_OUT_OF_DATE_KHR: onWindowResize(); break;
        case VK_SUBOPTIMAL_KHR: //DebugLog("Suboptimal swapchain"); break;
        default: break;
    }

    const float aspectRatio = float(m_device->extent.width) / float(m_device->extent.height);
    m_mainCamera.update(m_device->device, aspectRatio, imageIndex);
    vkQueueWaitIdle(m_device->graphics.queue);//TODO(arle): replace with fence
    updateCmdBuffers(imageIndex);

    if(m_imagesInFlight[imageIndex] != VK_NULL_HANDLE)
        vkWaitForFences(m_device->device, 1, &m_imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);

    m_imagesInFlight[imageIndex] = m_inFlightFences[m_currentFrame];

    const VkSemaphore waitSemaphores[] = {m_imageAvailableSPs[m_currentFrame]};
    const VkSemaphore signalSemaphores[] = {m_renderFinishedSPs[m_currentFrame]};
    const VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    vkResetFences(m_device->device, 1, &m_inFlightFences[m_currentFrame]);

    VkCommandBuffer submitCmds[] = {
        m_commandBuffers[imageIndex],
        m_overlay->cmdBuffers[imageIndex]
    };

    auto submitInfo = vkInits::submitInfo(submitCmds, arraysize(submitCmds));
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
    vkQueueSubmit(m_device->graphics.queue, 1, &submitInfo,
                  m_inFlightFences[m_currentFrame]);

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_swapchain;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(m_device->present.queue, &presentInfo);

    const auto vulkanErrorResize = result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR;
    if(vulkanErrorResize || pltf::HasResized())
        onWindowResize();

    m_currentFrame = (m_currentFrame + 1) % MAX_IMAGES_IN_FLIGHT;
}

void model_viewer::onKeyEvent(pltf::logical_device device, pltf::key_code key, pltf::modifier mod)
{
    if(mod & pltf::MODIFIER_ALT){
        if(key == pltf::key_code::F4)
            pltf::WindowClose();

        if(key == pltf::key_code::F){
            if(pltf::IsFullscreen())
                pltf::WindowSetMinimised(device);
            else
                pltf::WindowSetFullscreen(device);
        }
    }
}

void model_viewer::onMouseButtonEvent(pltf::logical_device device, pltf::mouse_button button)
{
    return;
}

void model_viewer::onWindowResize()
{
    vkDeviceWaitIdle(m_device->device);// TODO(arle): replace with faster sync

    m_device->refresh();

    if(m_device->extent.width == 0 || m_device->extent.height == 0)
        return;

    for (size_t i = 0; i < m_imageCount; i++)
        vkDestroyFramebuffer(m_device->device, m_framebuffers[i], nullptr);

    m_msaa.destroy(m_device->device);
    m_depth.destroy(m_device->device);
    vkResetCommandPool(m_device->device, m_cmdPool, 0);// VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT

    vkDestroyPipeline(m_device->device, m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device->device, m_pipelineLayout, nullptr);

    vkDestroyRenderPass(m_device->device, m_renderPass, nullptr);

    for (size_t i = 0; i < m_imageCount; i++)
        vkDestroyImageView(m_device->device, m_swapchainViews[i], nullptr);

    auto oldSwapchain = m_swapchain;
    m_device->buildSwapchain(m_swapchain, &m_swapchain);
    vkDestroySwapchainKHR(m_device->device, oldSwapchain, nullptr);

    uint32_t localImageCount = 0;
    vkGetSwapchainImagesKHR(m_device->device, m_swapchain, &localImageCount, nullptr);
    vkGetSwapchainImagesKHR(m_device->device, m_swapchain, &localImageCount, m_swapchainImages);
    m_imageCount = localImageCount;

    buildSwapchainViews();
    buildRenderPass();
    buildPipeline();
    buildDepth();
    buildMsaa();
    buildFramebuffers();

    m_overlay->onWindowResize(m_cmdPool);
    m_overlay->updateCmdBuffers(m_framebuffers);
}

void model_viewer::buildResources()
{
    auto poolInfo = vkInits::commandPoolCreateInfo(m_device->graphics.family);
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCreateCommandPool(m_device->device, &poolInfo, nullptr, &m_cmdPool);

    m_device->buildSwapchain(VK_NULL_HANDLE, &m_swapchain);

    uint32_t localImageCount = 0;
    vkGetSwapchainImagesKHR(m_device->device, m_swapchain, &localImageCount, nullptr);
    m_imageCount = localImageCount;

    m_swapchainImages = m_permanentStorage.push<VkImage>(m_imageCount);
    m_swapchainViews = m_permanentStorage.push<VkImageView>(m_imageCount);
    m_framebuffers = m_permanentStorage.push<VkFramebuffer>(m_imageCount);
    m_imagesInFlight = m_permanentStorage.push<VkFence>(m_imageCount);
    m_commandBuffers = m_permanentStorage.push<VkCommandBuffer>(m_imageCount);
    m_descriptorSets = m_permanentStorage.push<VkDescriptorSet>(m_imageCount);
    m_uniformBuffers = m_permanentStorage.push<uniform_buffer>(m_imageCount);

    vkGetSwapchainImagesKHR(m_device->device, m_swapchain, &localImageCount, m_swapchainImages);

    buildSwapchainViews();
    buildMsaa();
    buildDepth();
    buildRenderPass();
    buildFramebuffers();
    buildSyncObjects();

    new (&m_mainCamera) camera(m_device, m_permanentStorage.pushView<buffer_t>(m_imageCount));
    buildUniformBuffers();

    loadModel();

    auto cmdInfo = vkInits::commandBufferAllocateInfo(m_cmdPool, m_imageCount);
    vkAllocateCommandBuffers(m_device->device, &cmdInfo, m_commandBuffers);

    new (&m_shaders[0]) shader_object(INTERNAL_DIR "shaders/pbr_vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    new (&m_shaders[1]) shader_object(INTERNAL_DIR "shaders/pbr_frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    for (size_t i = 0; i < arraysize(m_shaders); i++)
        m_shaders[i].load(m_device->device);

    buildPipeline();

    updateLights();
}

void model_viewer::loadModel()
{
    auto mesh = m_permanentStorage.push<mesh3D>(1);
    mesh->loadSphere(m_device, m_cmdPool);

    auto material = m_permanentStorage.push<pbr_material>(1);

    new (material) pbr_material();

    material->albedo.loadFromFile(m_device, m_cmdPool, INTERNAL_DIR MATERIALS_DIR "patterned-bw-vinyl-bl/albedo.png");
    material->normal.loadFromFile(m_device, m_cmdPool, INTERNAL_DIR MATERIALS_DIR "patterned-bw-vinyl-bl/normal.png");
    material->roughness.loadFromFile(m_device, m_cmdPool, INTERNAL_DIR MATERIALS_DIR "patterned-bw-vinyl-bl/roughness.png");
    material->metallic.loadFromFile(m_device, m_cmdPool, INTERNAL_DIR MATERIALS_DIR "patterned-bw-vinyl-bl/metallic.png");
    material->ao.loadFromFile(m_device, m_cmdPool, INTERNAL_DIR MATERIALS_DIR "patterned-bw-vinyl-bl/ao.png");

    new (&m_model) model3D(m_device, mesh, material);

    buildDescriptors(material);
}

void model_viewer::buildSwapchainViews()
{
    for(size_t i = 0; i < m_imageCount; i++){
        auto imageViewInfo = vkInits::imageViewCreateInfo();
        imageViewInfo.format = m_device->surfaceFormat.format;
        imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewInfo.image = m_swapchainImages[i];
        vkCreateImageView(m_device->device, &imageViewInfo, nullptr, &m_swapchainViews[i]);
    }
}

void model_viewer::buildMsaa()
{
    auto msaaInfo = vkInits::imageCreateInfo();
    msaaInfo.samples = m_device->sampleCount;
    msaaInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    msaaInfo.extent = {m_device->extent.width, m_device->extent.height, 1};
    msaaInfo.format = m_device->surfaceFormat.format;
    msaaInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    msaaInfo.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    new (&m_msaa) image_buffer(m_device, &msaaInfo, VK_IMAGE_ASPECT_COLOR_BIT);
}

void model_viewer::buildDepth()
{
    auto depthInfo = vkInits::imageCreateInfo();
    depthInfo.extent = {m_device->extent.width, m_device->extent.height, 1};
    depthInfo.format = m_depthFormat;
    depthInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    depthInfo.samples = m_device->sampleCount;
    depthInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    new (&m_depth) image_buffer(m_device, &depthInfo, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void model_viewer::buildRenderPass()
{
    auto colourAttachment = vkInits::attachmentDescription(m_device->surfaceFormat.format);
    colourAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colourAttachment.samples = m_device->sampleCount;

    auto depthAttachment = vkInits::attachmentDescription(m_depthFormat);
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.samples = m_device->sampleCount;

    auto colourResolve = vkInits::attachmentDescription(m_device->surfaceFormat.format);
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

    vkCreateRenderPass(m_device->device, &renderPassInfo, nullptr, &m_renderPass);
}

void model_viewer::buildFramebuffers()
{
    for(size_t i = 0; i < m_imageCount; i++){
        const VkImageView frameBufferAttachments[] = {
            m_msaa.view(),
            m_depth.view(),
            m_swapchainViews[i]
        };

        auto framebufferInfo = vkInits::framebufferCreateInfo();
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(arraysize(frameBufferAttachments));
        framebufferInfo.pAttachments = frameBufferAttachments;
        framebufferInfo.width = m_device->extent.width;
        framebufferInfo.height = m_device->extent.height;
        vkCreateFramebuffer(m_device->device, &framebufferInfo, nullptr, &m_framebuffers[i]);
    }
}

void model_viewer::buildSyncObjects()
{
    auto semaphoreInfo = vkInits::semaphoreCreateInfo();
    auto fenceInfo = vkInits::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);

    for (size_t i = 0; i < MAX_IMAGES_IN_FLIGHT; i++){
        vkCreateSemaphore(m_device->device, &semaphoreInfo, nullptr, &m_imageAvailableSPs[i]);
        vkCreateSemaphore(m_device->device, &semaphoreInfo, nullptr, &m_renderFinishedSPs[i]);
        vkCreateFence(m_device->device, &fenceInfo, nullptr, &m_inFlightFences[i]);
    }
}

void model_viewer::buildUniformBuffers()
{
    for (size_t i = 0; i < m_imageCount; i++){
        auto lightsBufferInfo = vkInits::bufferCreateInfo(sizeof(light_data), light_data::usageFlags());
        m_uniformBuffers[i].lights.create(m_device, &lightsBufferInfo, MEM_FLAG_HOST_VISIBLE);
    }
}

void model_viewer::buildDescriptors(const pbr_material *pMaterial)
{
    const VkDescriptorPoolSize poolSizes[] = {
        vkInits::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_imageCount),
        vkInits::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_imageCount),
        vkInits::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5 * m_imageCount)
    };

    const auto descriptorPoolInfo = vkInits::descriptorPoolCreateInfo(poolSizes, 7 * m_imageCount);
    vkCreateDescriptorPool(m_device->device, &descriptorPoolInfo, nullptr, &m_descriptorPool);

    constexpr auto combinedStage = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    const VkDescriptorSetLayoutBinding bindings[] = {
        vkInits::descriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, combinedStage),
        vkInits::descriptorSetLayoutBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkInits::descriptorSetLayoutBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkInits::descriptorSetLayoutBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkInits::descriptorSetLayoutBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkInits::descriptorSetLayoutBinding(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkInits::descriptorSetLayoutBinding(6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
    };

    const auto setLayoutInfo = vkInits::descriptorSetLayoutCreateInfo(bindings);
    vkCreateDescriptorSetLayout(m_device->device, &setLayoutInfo, nullptr, &m_descriptorSetLayout);

    // Sets

    auto layouts = dyn_array<VkDescriptorSetLayout>(m_imageCount);
    layouts.fill(m_descriptorSetLayout);

    auto allocInfo = vkInits::descriptorSetAllocateInfo(m_descriptorPool, layouts);
    vkAllocateDescriptorSets(m_device->device, &allocInfo, m_descriptorSets);

    for (size_t i = 0; i < m_imageCount; i++) {
        const auto cameraBufferInfo = m_mainCamera.descriptor(i);
        const auto lightBufferInfo = m_uniformBuffers[i].lights.descriptor(0);

        auto& setRef = m_descriptorSets[i];
        const VkWriteDescriptorSet writes[] = {
            vkInits::writeDescriptorSet(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, setRef, &cameraBufferInfo),
            vkInits::writeDescriptorSet(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, setRef, &lightBufferInfo),
            vkInits::writeDescriptorSet(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, setRef, &pMaterial->albedo.descriptor),
            vkInits::writeDescriptorSet(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, setRef, &pMaterial->normal.descriptor),
            vkInits::writeDescriptorSet(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, setRef, &pMaterial->roughness.descriptor),
            vkInits::writeDescriptorSet(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, setRef, &pMaterial->metallic.descriptor),
            vkInits::writeDescriptorSet(6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, setRef, &pMaterial->ao.descriptor)
        };

        vkUpdateDescriptorSets(m_device->device, uint32_t(arraysize(writes)), writes, 0, nullptr);
    }
}

void model_viewer::buildPipeline()
{
    const VkPipelineShaderStageCreateInfo shaderStages[] = {
        m_shaders[0].shaderStage(), m_shaders[1].shaderStage()
    };

    auto bindingDescription = vkInits::vertexBindingDescription(sizeof(mesh_vertex));

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = uint32_t(arraysize(mesh3D::Attributes));
    vertexInputInfo.pVertexAttributeDescriptions = mesh3D::Attributes;

    auto inputAssembly = vkInits::inputAssemblyInfo();
    auto viewport = vkInits::viewportInfo(m_device->extent);
    auto scissor = vkInits::scissorInfo(m_device->extent);

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    auto rasterizer = vkInits::rasterizationStateInfo(VK_FRONT_FACE_COUNTER_CLOCKWISE);
    auto multisampling = vkInits::pipelineMultisampleStateCreateInfo(m_device->sampleCount);

    auto depthStencil = vkInits::depthStencilStateInfo();
    auto colorBlendAttachment = vkInits::pipelineColorBlendAttachmentState();
    auto colourBlend = vkInits::pipelineColorBlendStateCreateInfo();
    colourBlend.attachmentCount = 1;
    colourBlend.pAttachments = &colorBlendAttachment;

    const VkPushConstantRange pushConstants[] = {
        model3D::pushConstant()
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = pushConstants;
    pipelineLayoutInfo.pushConstantRangeCount = uint32_t(arraysize(pushConstants));
    vkCreatePipelineLayout(m_device->device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout);

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
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = m_renderPass;
    vkCreateGraphicsPipelines(m_device->device, VK_NULL_HANDLE, 1,
                              &pipelineInfo, nullptr, &m_pipeline);
}

void model_viewer::gameUpdate(float dt)
{
    if(pltf::IsKeyDown(pltf::key_code::W))
        m_mainCamera.rotate(camera::direction::up, dt);
    else if(pltf::IsKeyDown(pltf::key_code::S))
        m_mainCamera.rotate(camera::direction::down, dt);

    if(pltf::IsKeyDown(pltf::key_code::A))
        m_mainCamera.rotate(camera::direction::left, dt);
    else if(pltf::IsKeyDown(pltf::key_code::D))
        m_mainCamera.rotate(camera::direction::right, dt);

    if(pltf::IsKeyDown(pltf::key_code::Up))
        m_mainCamera.move(camera::direction::forward, dt);
    else if(pltf::IsKeyDown(pltf::key_code::Down))
        m_mainCamera.move(camera::direction::backward, dt);

    if(pltf::IsKeyDown(pltf::key_code::Left))
        m_mainCamera.move(camera::direction::left, dt);
    else if(pltf::IsKeyDown(pltf::key_code::Right))
        m_mainCamera.move(camera::direction::right, dt);
}

void model_viewer::updateLights()
{
    auto lights = light_data();
    lights.positions[0] = vec4(-10.0f, 10.0f, -10.0f, 0.0f);
    lights.colours[0] = GetColour(0xea00d9ff);

    lights.positions[1] = vec4(10.0f, 10.0f, -10.0f, 0.0f);
    lights.colours[1] = GetColour(0x0abdc6ff);

    lights.positions[2] = vec4(-10.0f, -10.0f, -10.0f, 0.0f);
    lights.colours[2] = GetColour(0x133e7cff);

    lights.positions[3] = vec4(10.0f, -10.0f, -10.0f, 0.0f);
    lights.colours[3] = GetColour(0x711c91ff);

    constexpr float lightStrength = 200.0f;
    for (size_t i = 0; i < arraysize(lights.colours); i++){
        lights.colours[i].x *= lightStrength;
        lights.colours[i].y *= lightStrength;
        lights.colours[i].z *= lightStrength;
    }

    for (size_t i = 0; i < m_imageCount; i++)
        m_uniformBuffers[i].lights.fill(m_device->device, &lights);
}

void model_viewer::updateCmdBuffers(size_t imageIndex)
{
    VkClearValue colourValue;
    colourValue.color = {};
    VkClearValue depthStencilValue;
    depthStencilValue.depthStencil = {1.0f, 0};

    const VkClearValue clearValues[] = {colourValue, depthStencilValue};

    auto cmdBeginInfo = vkInits::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);
    auto renderBeginInfo = vkInits::renderPassBeginInfo(m_renderPass, m_device->extent);
    renderBeginInfo.clearValueCount = 1;
    renderBeginInfo.pClearValues = clearValues;
    renderBeginInfo.clearValueCount = uint32_t(arraysize(clearValues));
#if 1
    const auto cmdBuffer = m_commandBuffers[imageIndex];
    vkBeginCommandBuffer(cmdBuffer, &cmdBeginInfo);

    renderBeginInfo.framebuffer = m_framebuffers[imageIndex];
    vkCmdBeginRenderPass(cmdBuffer, &renderBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    vkCmdBindDescriptorSets(cmdBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_pipelineLayout,
                            0,
                            1,
                            &m_descriptorSets[imageIndex],
                            0,
                            nullptr);

    m_model.draw(cmdBuffer, m_pipelineLayout);

    vkCmdEndRenderPass(cmdBuffer);
    vkEndCommandBuffer(cmdBuffer);
#else
    for (size_t i = 0; i < m_imageCount; i++){
        const auto cmdBuffer = m_commandBuffers[i];
        vkBeginCommandBuffer(cmdBuffer, &cmdBeginInfo);

        renderBeginInfo.framebuffer = m_framebuffers[i];
        vkCmdBeginRenderPass(cmdBuffer, &renderBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

        vkCmdBindDescriptorSets(cmdBuffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_pipelineLayout,
                                0,
                                1,
                                &m_descriptorSets[i],
                                0,
                                nullptr);

        m_model.draw(cmdBuffer, m_pipelineLayout);

        vkCmdEndRenderPass(cmdBuffer);
        vkEndCommandBuffer(cmdBuffer);
    }
#endif
}
#include "model_viewer.hpp"

#if defined(DEBUG)
static const bool ENABLE_VALIDATION = true;
#else
static const bool ENABLE_VALIDATION = false;
#endif

void CoreMessageCallback(log_level level, const char *string)
{
    pltf::DebugString(string);
    pltf::DebugBreak();
}

ModelViewer::ModelViewer(pltf::logical_device platform) : VulkanInstance(platform, MegaBytes(64))
{
    settings.title = "PBR Demo";
    settings.syncMode = VSyncMode::Off;
    settings.enValidation = ENABLE_VALIDATION;

    auto dispatcher = EventDispatcher<ModelViewer>(platform, this);

    VulkanInstance::coreMessage = CoreMessageCallback;
    VulkanInstance::prepare();

    loadResources();

    auto sb = StringbBuilder(100);

    sb << SHADERS_PATH << view("pbr_vert.spv");
    scene.vertexShader.load(device, sb.c_str());
    sb.flush() << SHADERS_PATH << view("pbr_frag.spv");
    scene.fragmentShader.load(device, sb.c_str());
    sb.flush() << SHADERS_PATH << view("skybox_vert.spv");
    skybox.vertexShader.load(device, sb.c_str());
    sb.flush() << SHADERS_PATH << view("skybox_frag.spv");
    skybox.fragmentShader.load(device, sb.c_str());

    sb.destroy();

    generateBrdfLUT();
    generateIrradianceMap();
    generatePrefilteredMap();

    m_mainCamera.init();
    m_lights.init();

    buildUniformBuffers();
    buildDescriptors();

    buildPipelines();

    // Prepare text overlay

    m_overlay = allocate<VulkanTextOverlay>(1);

    m_overlay->depthFormat = depthFormat;
    m_overlay->surfaceFormat = surfaceFormat;
    m_overlay->sampleCount = sampleCount;
    m_overlay->imageCount = imageCount;
    m_overlay->graphicsQueue = graphicsQueue;
    m_overlay->frameBuffers = framebuffers;
    m_overlay->extent = extent;
    m_overlay->init(&device, this); // obj slicing

    m_overlay->textTint = vec4(1.0f);
    m_overlay->textAlignment = text_align::centre;
    m_overlay->textType = text_coord_type::relative;
    m_overlay->textSize = 1.0f;

    VkPhysicalDeviceProperties deviceProps;
    vkGetPhysicalDeviceProperties(device.gpu, &deviceProps);
    const char *deviceNameString = deviceProps.deviceName;
    const auto deviceNameView = StringbBuilder::MakeView(deviceNameString);

    m_overlay->begin();
    m_overlay->draw(StringbBuilder::MakeView(settings.title), vec2(50.0f, 12.0f));

    m_overlay->textSize = 0.8f;
    m_overlay->textAlignment = text_align::left;
    m_overlay->draw("Device:", vec2(5.0f, 88.0f));
    m_overlay->draw(deviceNameView, vec2(5.0f, 90.0f));
    m_overlay->end();
}

ModelViewer::~ModelViewer()
{
    vkDeviceWaitIdle(device);

    const auto destroyPreGenerated = [](VkDevice vkDev, PreGenerated &resource)
    {
        vkDestroySampler(vkDev, resource.sampler, nullptr);
        vkDestroyImageView(vkDev, resource.view, nullptr);
        vkDestroyImage(vkDev, resource.image, nullptr);
        vkFreeMemory(vkDev, resource.memory, nullptr);
    };

    destroyPreGenerated(device, brdf);
    destroyPreGenerated(device, irradiance);
    destroyPreGenerated(device, prefiltered);

    m_overlay->destroy();

    vkDestroyPipeline(device, skybox.pipeline, nullptr);
    vkDestroyPipelineLayout(device, skybox.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, skybox.setLayout, nullptr);
    skybox.vertexShader.destroy(device);
    skybox.fragmentShader.destroy(device);

    vkDestroyPipeline(device, scene.pipeline, nullptr);
    vkDestroyPipelineLayout(device, scene.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, scene.setLayout, nullptr);
    scene.vertexShader.destroy(device);
    scene.fragmentShader.destroy(device);

    textures.albedo.destroy(device);
    textures.normal.destroy(device);
    textures.roughness.destroy(device);
    textures.metallic.destroy(device);
    textures.ao.destroy(device);
    textures.skybox.destroy(device);
    models.skybox.destroy(device);
    models.object.destroy(device);

    for (size_t i = 0; i < MAX_IMAGES_IN_FLIGHT; i++)
    {
        scene.cameraBuffers[i].destroy(device);
        scene.lightBuffers[i].destroy(device);
    }

    vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);

    VulkanInstance::destroy();
}

void ModelViewer::run()
{
    while(pltf::IsRunning())
    {
        pltf::EventsPoll(platformDevice);

        if(extent.width == 0 || extent.height == 0)
            continue;

        updateCamera(pltf::GetTimestep(platformDevice));

        VulkanInstance::prepareFrame();

        recordFrame(commandBuffers[currentFrame]);
        m_overlay->recordFrame(currentFrame, imageIndex);

        m_commands[0] = commandBuffers[currentFrame];
        m_commands[1] = m_overlay->commandBuffers[currentFrame];

        submitInfo.pCommandBuffers = m_commands;
        submitInfo.commandBufferCount = uint32_t(arraysize(m_commands));

        VulkanInstance::submitFrame();

        if(resizeRequired)
            onWindowSize(uint32_t(extent.width), uint32_t(extent.height));
    }
}

void ModelViewer::onWindowSize(int32_t width, int32_t height)
{
    if(width == 0 || height == 0)
        return;

    VulkanInstance::onResize();

    vkDestroyPipeline(device, scene.pipeline, nullptr);
    vkDestroyPipelineLayout(device, scene.pipelineLayout, nullptr);
    vkDestroyPipeline(device, skybox.pipeline, nullptr);
    vkDestroyPipelineLayout(device, skybox.pipelineLayout, nullptr);

    // NOTE(arle): pipeline layout does not need to be recreated
    buildPipelines();

    m_overlay->extent = extent;
    m_overlay->onWindowResize();
}

void ModelViewer::onKeyEvent(pltf::key_code key, pltf::modifier mod)
{
    if (mod & pltf::MODIFIER_ALT)
    {
        if (key == pltf::key_code::F4)
            pltf::WindowClose();

        if (key == pltf::key_code::F)
        {
            if (pltf::IsFullscreen())
                pltf::WindowSetMinimised(platformDevice);
            else
                pltf::WindowSetFullscreen(platformDevice);
        }
    }
}

void ModelViewer::onMouseButtonEvent(pltf::mouse_button button)
{
    return;
}

void ModelViewer::onScrollWheelEvent(double x, double y)
{
    m_mainCamera.fov -= GetRadians(float(y));
    m_mainCamera.fov = clamp(m_mainCamera.fov, Camera::FOV_LIMITS_LOW, Camera::FOV_LIMITS_HIGH);
}

void ModelViewer::createPregenRenderPass(VkRenderPass *pRenderPass, VkFormat format, VkImageLayout finalLayout)
{
    auto colourAttachment = vkInits::attachmentDescription(format);
    colourAttachment.finalLayout = finalLayout;

    auto colourAttachmentRef = vkInits::attachmentReference(0);
    colourAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colourAttachmentRef;

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
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colourAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = uint32_t(arraysize(dependencies));
    renderPassInfo.pDependencies = dependencies;
    vkCreateRenderPass(device, &renderPassInfo, nullptr, pRenderPass);
}

void ModelViewer::generateBrdfLUT()
{
    constexpr auto format = VK_FORMAT_R16G16_SFLOAT;
    constexpr uint32_t dimension = 512;

    // Target texture

    auto imageInfo = vkInits::imageCreateInfo();
    imageInfo.format = format;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.extent = {dimension, dimension, 1};
    vkCreateImage(device, &imageInfo, nullptr, &brdf.image);

    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(device, brdf.image, &memReqs);

    auto allocInfo = device.getMemoryAllocInfo(memReqs, MEM_FLAG_GPU_LOCAL);
    vkAllocateMemory(device, &allocInfo, nullptr, &brdf.memory);
    vkBindImageMemory(device, brdf.image, brdf.memory, 0);

    auto samplerInfo = vkInits::samplerCreateInfo();
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    vkCreateSampler(device, &samplerInfo, nullptr, &brdf.sampler);

    auto viewInfo = vkInits::imageViewCreateInfo();
    viewInfo.image = brdf.image;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    vkCreateImageView(device, &viewInfo, nullptr, &brdf.view);

    brdf.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    brdf.descriptor.imageView = brdf.view;
    brdf.descriptor.sampler = brdf.sampler;

    // Render pass

    VkRenderPass brdfRenderPass = VK_NULL_HANDLE;
    createPregenRenderPass(&brdfRenderPass, format, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // Framebuffer

    auto frameBufferInfo = vkInits::framebufferCreateInfo();
    frameBufferInfo.renderPass = brdfRenderPass;
    frameBufferInfo.attachmentCount = 1;
    frameBufferInfo.pAttachments = &brdf.view;
    frameBufferInfo.width = dimension;
    frameBufferInfo.height = dimension;

    VkFramebuffer brdfFramebuffer = VK_NULL_HANDLE;
    vkCreateFramebuffer(device, &frameBufferInfo, nullptr, &brdfFramebuffer);

    // Pipelinelayout

    VkPipelineVertexInputStateCreateInfo emptyInputState{};
    emptyInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    auto inputAssembly = vkInits::inputAssemblyInfo();
    auto viewport = vkInits::viewportInfo({dimension, dimension});
    auto scissor = vkInits::scissorInfo({dimension, dimension});

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    auto rasterizer = vkInits::rasterizationStateInfo(VK_FRONT_FACE_COUNTER_CLOCKWISE);
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    auto multisampling = vkInits::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
    auto depthStencil = vkInits::depthStencilStateInfo();
    auto colorBlendAttachment = vkInits::pipelineColorBlendAttachmentState();
    auto colourBlend = vkInits::pipelineColorBlendStateCreateInfo();
    colourBlend.attachmentCount = 1;
    colourBlend.pAttachments = &colorBlendAttachment;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pSetLayouts = VK_NULL_HANDLE;
    pipelineLayoutInfo.setLayoutCount = 0;

    VkPipelineLayout brdfPipelineLayout = VK_NULL_HANDLE;
    vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &brdfPipelineLayout);

    // shader stages & shader load

    auto sb = StringbBuilder(100);

    VertexShader brdfVertexShader;
    sb << SHADERS_PATH << view("brdf_vert.spv");
    brdfVertexShader.load(device, sb.c_str());

    FragmentShader brdfFragmentShader;
    sb.flush() << SHADERS_PATH << view("brdf_frag.spv");
    brdfFragmentShader.load(device, sb.c_str());

    sb.destroy();

    const VkPipelineShaderStageCreateInfo shaderStages[] = {
        brdfVertexShader.shaderStage(), brdfFragmentShader.shaderStage()
    };

    // Pipeline

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = uint32_t(arraysize(shaderStages));
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &emptyInputState;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colourBlend;
    pipelineInfo.layout = brdfPipelineLayout;
    pipelineInfo.renderPass = brdfRenderPass;

    VkPipeline brdfPipeline = VK_NULL_HANDLE;
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &brdfPipeline);

    // Render

    VkClearValue clearValue;
    clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

    auto renderBeginInfo = vkInits::renderPassBeginInfo(brdfRenderPass, {dimension, dimension});
    renderBeginInfo.framebuffer = brdfFramebuffer;
    renderBeginInfo.clearValueCount = 1;
    renderBeginInfo.pClearValues = &clearValue;

    auto drawCmd = device.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    vkCmdBeginRenderPass(drawCmd, &renderBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(drawCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, brdfPipeline);
    vkCmdDraw(drawCmd, 4, 1, 0, 0);
    vkCmdEndRenderPass(drawCmd);

    device.flushCommandBuffer(drawCmd, graphicsQueue);

    vkDestroyPipeline(device, brdfPipeline, nullptr);
    vkDestroyPipelineLayout(device, brdfPipelineLayout, nullptr);
    brdfVertexShader.destroy(device);
    brdfFragmentShader.destroy(device);
    vkDestroyFramebuffer(device, brdfFramebuffer, nullptr);
    vkDestroyRenderPass(device, brdfRenderPass, nullptr);
}

void ModelViewer::generateIrradianceMap()
{
    constexpr auto format = VK_FORMAT_R16G16B16A16_SFLOAT;
    constexpr uint32_t dimension = 32;
    const auto mipLevels = static_cast<uint32_t>(std::floor(std::log2(dimension))) + 1;

    // Irradiance cube map
    {
        auto imageInfo = vkInits::imageCreateInfo();
        imageInfo.format = format;
        imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.extent = {dimension, dimension, 1};
        imageInfo.arrayLayers = 6;
        imageInfo.mipLevels = mipLevels;
        imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        vkCreateImage(device, &imageInfo, nullptr, &irradiance.image);

        VkMemoryRequirements memReqs{};
        vkGetImageMemoryRequirements(device, irradiance.image, &memReqs);

        auto allocInfo = device.getMemoryAllocInfo(memReqs, MEM_FLAG_GPU_LOCAL);
        vkAllocateMemory(device, &allocInfo, nullptr, &irradiance.memory);
        vkBindImageMemory(device, irradiance.image, irradiance.memory, 0);

        auto samplerInfo = vkInits::samplerCreateInfo(float(mipLevels));
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        vkCreateSampler(device, &samplerInfo, nullptr, &irradiance.sampler);

        auto viewInfo = vkInits::imageViewCreateInfo();
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        viewInfo.image = irradiance.image;
        viewInfo.format = imageInfo.format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = mipLevels;
        viewInfo.subresourceRange.layerCount = 6;
        vkCreateImageView(device, &viewInfo, nullptr, &irradiance.view);

        irradiance.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        irradiance.descriptor.imageView = irradiance.view;
        irradiance.descriptor.sampler = irradiance.sampler;
    }

    // Render pass

    VkRenderPass irradianceRenderpass = VK_NULL_HANDLE;
    createPregenRenderPass(&irradianceRenderpass, format, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    OffscreenBuffer offscreen;
    {
        auto imageInfo = vkInits::imageCreateInfo();
        imageInfo.format = format;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.extent = {dimension, dimension, 1};
        imageInfo.arrayLayers = 1;
        imageInfo.mipLevels = 1;
        vkCreateImage(device, &imageInfo, nullptr, &offscreen.image);

        VkMemoryRequirements memReqs{};
        vkGetImageMemoryRequirements(device, offscreen.image, &memReqs);

        auto allocInfo = device.getMemoryAllocInfo(memReqs, MEM_FLAG_GPU_LOCAL);
        vkAllocateMemory(device, &allocInfo, nullptr, &offscreen.memory);
        vkBindImageMemory(device, offscreen.image, offscreen.memory, 0);

        auto viewInfo = vkInits::imageViewCreateInfo();
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.image = offscreen.image;
        viewInfo.format = imageInfo.format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        vkCreateImageView(device, &viewInfo, nullptr, &offscreen.view);

        auto framebufferInfo = vkInits::framebufferCreateInfo();
        framebufferInfo.width = dimension;
        framebufferInfo.height = dimension;
        framebufferInfo.renderPass = irradianceRenderpass;
        framebufferInfo.pAttachments = &offscreen.view;
        framebufferInfo.attachmentCount = 1;
        vkCreateFramebuffer(device, &framebufferInfo, nullptr, &offscreen.framebuffer);

        auto cmd = device.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        vkTools::SetImageLayout(cmd,
                                VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                offscreen.image);
        device.flushCommandBuffer(cmd, graphicsQueue);
    }

    // Descriptors

    VkDescriptorPool irradianceDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout irradianceSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet irradianceDescriptorSet = VK_NULL_HANDLE;

    const VkDescriptorPoolSize poolSizes[] = {
        vkInits::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
    };

    const auto descriptorPoolInfo = vkInits::descriptorPoolCreateInfo(poolSizes, 1);
    vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &irradianceDescriptorPool);

    const VkDescriptorSetLayoutBinding bindings[] = {
        vkInits::descriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                            VK_SHADER_STAGE_FRAGMENT_BIT)
    };

    auto setLayoutInfo = vkInits::descriptorSetLayoutCreateInfo(bindings);
    vkCreateDescriptorSetLayout(device, &setLayoutInfo, nullptr, &irradianceSetLayout);

    VkDescriptorSetLayout setLayouts[]  = {irradianceSetLayout};

    auto setAllocInfo = vkInits::descriptorSetAllocateInfo(irradianceDescriptorPool, setLayouts);
    vkAllocateDescriptorSets(device, &setAllocInfo, &irradianceDescriptorSet);

    const VkWriteDescriptorSet writes[] = {
        vkInits::writeDescriptorSet(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                    irradianceDescriptorSet, &textures.skybox.descriptor)
    };

    vkUpdateDescriptorSets(device, uint32_t(arraysize(writes)), writes, 0, nullptr);

    // Shaders

    auto sb = StringbBuilder(100);
    sb << SHADERS_PATH << view("irradiance_frag.spv");

    FragmentShader irradianceFragmentShader;
    irradianceFragmentShader.load(device, sb.c_str());
    sb.destroy();

    const VkPipelineShaderStageCreateInfo shaderStages[] = {
        skybox.vertexShader.shaderStage(), irradianceFragmentShader.shaderStage()
    };

    // Pipeline

    auto bindingDescription = vkInits::vertexBindingDescription(sizeof(vec3<float>));

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = uint32_t(arraysize(CubemapModel::Attributes));
    vertexInputInfo.pVertexAttributeDescriptions = CubemapModel::Attributes;

    auto inputAssembly = vkInits::inputAssemblyInfo();
    auto viewportState = vkInits::pipelineViewportStateCreateInfo(1, 1);
    auto rasterizer = vkInits::rasterizationStateInfo(VK_FRONT_FACE_COUNTER_CLOCKWISE);
    auto multisampling = vkInits::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
    auto depthStencil = vkInits::depthStencilStateInfo();
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;
    auto colorBlendAttachment = vkInits::pipelineColorBlendAttachmentState();
    auto colourBlend = vkInits::pipelineColorBlendStateCreateInfo();
    colourBlend.attachmentCount = 1;
    colourBlend.pAttachments = &colorBlendAttachment;

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    auto dynamicStateInfo = vkInits::pipelineDynamicStateCreateInfo();
    dynamicStateInfo.pDynamicStates = dynamicStates;
    dynamicStateInfo.dynamicStateCount = uint32_t(arraysize(dynamicStates));

    const auto pushConstant = ModelViewMatrix::pushConstant();

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pSetLayouts = &irradianceSetLayout;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstant;
    pipelineLayoutInfo.pushConstantRangeCount = 1;

    VkPipelineLayout irradiancePipelineLayout = VK_NULL_HANDLE;
    vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &irradiancePipelineLayout);

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
    pipelineInfo.layout = irradiancePipelineLayout;
    pipelineInfo.pDynamicState = &dynamicStateInfo;
    pipelineInfo.renderPass = irradianceRenderpass;

    VkPipeline irradiancePipeline = VK_NULL_HANDLE;
    vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &irradiancePipeline);

    // Render

    VkClearValue clearValue;
    clearValue.color = {{0.0f, 0.0f, 0.2f, 1.0f}};

    auto renderBeginInfo = vkInits::renderPassBeginInfo(irradianceRenderpass, {dimension, dimension});
    renderBeginInfo.framebuffer = offscreen.framebuffer;
    renderBeginInfo.clearValueCount = 1;
    renderBeginInfo.pClearValues = &clearValue;

    const mat4x4 viewMatrices[] = {
        mat4x4::lookAt(vec3(0.0f), vec3(-1.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f)),
        mat4x4::lookAt(vec3(0.0f), vec3(1.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f)),
        mat4x4::lookAt(vec3(0.0f), vec3(0.0f, -1.0f, 0.0f), vec3(0.0f, 0.0f, -1.0f)),
        mat4x4::lookAt(vec3(0.0f), vec3(0.0f, 1.0f, 0.0f), vec3(0.0f, 0.0f, 1.0f)),
        mat4x4::lookAt(vec3(0.0f), vec3(0.0f, 0.0f, -1.0f), vec3(0.0f, 1.0f, 0.0f)),
        mat4x4::lookAt(vec3(0.0f), vec3(0.0f, 0.0f, 1.0f), vec3(0.0f, 1.0f, 0.0f))
    };

    auto cmd = device.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    auto viewport = vkInits::viewportInfo({dimension, dimension});
    auto scissor = vkInits::scissorInfo({dimension, dimension});

    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    VkImageSubresourceRange subresourceRange{};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.levelCount = mipLevels;
    subresourceRange.layerCount = 6;

    vkTools::SetImageLayout(cmd,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                            irradiance.image,
                            subresourceRange);

    alignas(16) mat4x4 pushBlock[2] = {};
    pushBlock[1] = mat4x4::perspective(PI32 / 2, 1.0f, Camera::DEFAULT_ZNEAR, Camera::DEFAULT_ZFAR);

    for (uint32_t level = 0; level < mipLevels; level++)
    {
        for (uint32_t i = 0; i < 6; i++)
        {
            viewport.width = float(dimension * std::pow(0.5f, level));
            viewport.height = float(dimension * std::pow(0.5f, level));
            vkCmdSetViewport(cmd, 0, 1, &viewport);

            vkCmdBeginRenderPass(cmd, &renderBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, irradiancePipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    irradiancePipelineLayout, 0, 1,
                                    &irradianceDescriptorSet, 0, nullptr);

            pushBlock[0] = viewMatrices[i];

            vkCmdPushConstants(cmd, irradiancePipelineLayout,
                               pushConstant.stageFlags,
                               pushConstant.offset,
                               pushConstant.size,
                               &pushBlock);

            models.skybox.draw(cmd);

            vkCmdEndRenderPass(cmd);

            vkTools::SetImageLayout(cmd,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                    offscreen.image);

            VkImageCopy copyRegion{};
            copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.srcSubresource.baseArrayLayer = 0;
            copyRegion.srcSubresource.layerCount = 1;
            copyRegion.srcSubresource.mipLevel = 0;
            copyRegion.srcOffset = {};

            copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.dstSubresource.baseArrayLayer = i;
            copyRegion.dstSubresource.layerCount = 1;
            copyRegion.dstSubresource.mipLevel = level;
            copyRegion.dstOffset = {};

            copyRegion.extent.width = uint32_t(viewport.width);
            copyRegion.extent.height = uint32_t(viewport.height);
            copyRegion.extent.depth = 1;

            vkCmdCopyImage(cmd, offscreen.image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           irradiance.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &copyRegion);

            vkTools::SetImageLayout(cmd,
                                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                    offscreen.image);
        }
    }

    vkTools::SetImageLayout(cmd,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                            irradiance.image, subresourceRange);

    device.flushCommandBuffer(cmd, graphicsQueue);

    vkDestroyRenderPass(device, irradianceRenderpass, nullptr);
    vkDestroyFramebuffer(device, offscreen.framebuffer, nullptr);
    vkFreeMemory(device, offscreen.memory, nullptr);
    vkDestroyImageView(device, offscreen.view, nullptr);
    vkDestroyImage(device, offscreen.image, nullptr);
    vkDestroyDescriptorPool(device, irradianceDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, irradianceSetLayout, nullptr);
    vkDestroyPipeline(device, irradiancePipeline, nullptr);
    vkDestroyPipelineLayout(device, irradiancePipelineLayout, nullptr);
    irradianceFragmentShader.destroy(device);
}

void ModelViewer::generatePrefilteredMap()
{
    constexpr auto format = VK_FORMAT_R16G16B16A16_SFLOAT;
    constexpr uint32_t dimension = 512;
    const auto mipLevels = static_cast<uint32_t>(std::floor(std::log2(dimension))) + 1;

    // Prefiltered cube map
    {
        auto imageInfo = vkInits::imageCreateInfo();
        imageInfo.format = format;
        imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.extent = {dimension, dimension, 1};
        imageInfo.arrayLayers = 6;
        imageInfo.mipLevels = mipLevels;
        imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        vkCreateImage(device, &imageInfo, nullptr, &prefiltered.image);

        VkMemoryRequirements memReqs{};
        vkGetImageMemoryRequirements(device, prefiltered.image, &memReqs);

        auto allocInfo = device.getMemoryAllocInfo(memReqs, MEM_FLAG_GPU_LOCAL);
        vkAllocateMemory(device, &allocInfo, nullptr, &prefiltered.memory);
        vkBindImageMemory(device, prefiltered.image, prefiltered.memory, 0);

        auto samplerInfo = vkInits::samplerCreateInfo(float(mipLevels));
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        vkCreateSampler(device, &samplerInfo, nullptr, &prefiltered.sampler);

        auto viewInfo = vkInits::imageViewCreateInfo();
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        viewInfo.image = prefiltered.image;
        viewInfo.format = imageInfo.format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = mipLevels;
        viewInfo.subresourceRange.layerCount = 6;
        vkCreateImageView(device, &viewInfo, nullptr, &prefiltered.view);

        prefiltered.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        prefiltered.descriptor.imageView = prefiltered.view;
        prefiltered.descriptor.sampler = prefiltered.sampler;
    }

    // Render pass

    VkRenderPass prefilteredRenderpass = VK_NULL_HANDLE;
    createPregenRenderPass(&prefilteredRenderpass, format, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    OffscreenBuffer offscreen;
    {
        auto imageInfo = vkInits::imageCreateInfo();
        imageInfo.format = format;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.extent = {dimension, dimension, 1};
        imageInfo.arrayLayers = 1;
        imageInfo.mipLevels = 1;
        vkCreateImage(device, &imageInfo, nullptr, &offscreen.image);

        VkMemoryRequirements memReqs{};
        vkGetImageMemoryRequirements(device, offscreen.image, &memReqs);

        auto allocInfo = device.getMemoryAllocInfo(memReqs, MEM_FLAG_GPU_LOCAL);
        vkAllocateMemory(device, &allocInfo, nullptr, &offscreen.memory);
        vkBindImageMemory(device, offscreen.image, offscreen.memory, 0);

        auto viewInfo = vkInits::imageViewCreateInfo();
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.image = offscreen.image;
        viewInfo.format = imageInfo.format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        vkCreateImageView(device, &viewInfo, nullptr, &offscreen.view);

        auto framebufferInfo = vkInits::framebufferCreateInfo();
        framebufferInfo.width = dimension;
        framebufferInfo.height = dimension;
        framebufferInfo.renderPass = prefilteredRenderpass;
        framebufferInfo.pAttachments = &offscreen.view;
        framebufferInfo.attachmentCount = 1;
        vkCreateFramebuffer(device, &framebufferInfo, nullptr, &offscreen.framebuffer);

        auto cmd = device.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        vkTools::SetImageLayout(cmd,
                                VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                offscreen.image);
        device.flushCommandBuffer(cmd, graphicsQueue);
    }

    // Descriptors

    VkDescriptorPool prefilteredDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout prefilteredSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet prefilteredDescriptorSet = VK_NULL_HANDLE;

    const VkDescriptorPoolSize poolSizes[] = {
        vkInits::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
    };

    const auto descriptorPoolInfo = vkInits::descriptorPoolCreateInfo(poolSizes, 1);
    vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &prefilteredDescriptorPool);

    const VkDescriptorSetLayoutBinding bindings[] = {
        vkInits::descriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                            VK_SHADER_STAGE_FRAGMENT_BIT)
    };

    auto setLayoutInfo = vkInits::descriptorSetLayoutCreateInfo(bindings);
    vkCreateDescriptorSetLayout(device, &setLayoutInfo, nullptr, &prefilteredSetLayout);

    VkDescriptorSetLayout setLayouts[]  = {prefilteredSetLayout};

    auto setAllocInfo = vkInits::descriptorSetAllocateInfo(prefilteredDescriptorPool, setLayouts);
    vkAllocateDescriptorSets(device, &setAllocInfo, &prefilteredDescriptorSet);

    const VkWriteDescriptorSet writes[] = {
        vkInits::writeDescriptorSet(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                    prefilteredDescriptorSet, &textures.skybox.descriptor)
    };

    vkUpdateDescriptorSets(device, uint32_t(arraysize(writes)), writes, 0, nullptr);

    // Shaders

    auto sb = StringbBuilder(100);
    sb << SHADERS_PATH << view("prefiltered_frag.spv");

    FragmentShader prefilteredFragmentShader;
    prefilteredFragmentShader.load(device, sb.c_str());
    sb.destroy();

    const VkPipelineShaderStageCreateInfo shaderStages[] = {
        skybox.vertexShader.shaderStage(), prefilteredFragmentShader.shaderStage()
    };

    // Pipeline

    auto bindingDescription = vkInits::vertexBindingDescription(sizeof(vec3<float>));

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = uint32_t(arraysize(CubemapModel::Attributes));
    vertexInputInfo.pVertexAttributeDescriptions = CubemapModel::Attributes;

    auto inputAssembly = vkInits::inputAssemblyInfo();
    auto viewportState = vkInits::pipelineViewportStateCreateInfo(1, 1);
    auto rasterizer = vkInits::rasterizationStateInfo(VK_FRONT_FACE_COUNTER_CLOCKWISE);
    auto multisampling = vkInits::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
    auto depthStencil = vkInits::depthStencilStateInfo();
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;
    auto colorBlendAttachment = vkInits::pipelineColorBlendAttachmentState();
    auto colourBlend = vkInits::pipelineColorBlendStateCreateInfo();
    colourBlend.attachmentCount = 1;
    colourBlend.pAttachments = &colorBlendAttachment;

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    auto dynamicStateInfo = vkInits::pipelineDynamicStateCreateInfo();
    dynamicStateInfo.pDynamicStates = dynamicStates;
    dynamicStateInfo.dynamicStateCount = uint32_t(arraysize(dynamicStates));

    const VkPushConstantRange pushConstants[] = {
        ModelViewMatrix::pushConstant(),
        vkInits::pushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(float), 2 * sizeof(mat4x4))
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pSetLayouts = &prefilteredSetLayout;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = pushConstants;
    pipelineLayoutInfo.pushConstantRangeCount = uint32_t(arraysize(pushConstants));

    VkPipelineLayout prefilteredPipelineLayout = VK_NULL_HANDLE;
    vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &prefilteredPipelineLayout);

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
    pipelineInfo.layout = prefilteredPipelineLayout;
    pipelineInfo.pDynamicState = &dynamicStateInfo;
    pipelineInfo.renderPass = prefilteredRenderpass;

    VkPipeline prefilteredPipeline = VK_NULL_HANDLE;
    vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &prefilteredPipeline);

    // Render

    VkClearValue clearValue;
    clearValue.color = {{0.2f, 0.0f, 0.0f, 1.0f}};

    auto renderBeginInfo = vkInits::renderPassBeginInfo(prefilteredRenderpass, {dimension, dimension});
    renderBeginInfo.framebuffer = offscreen.framebuffer;
    renderBeginInfo.clearValueCount = 1;
    renderBeginInfo.pClearValues = &clearValue;

    const mat4x4 viewMatrices[] = {
        mat4x4::lookAt(vec3(0.0f), vec3(-1.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f)),
        mat4x4::lookAt(vec3(0.0f), vec3(1.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f)),
        mat4x4::lookAt(vec3(0.0f), vec3(0.0f, -1.0f, 0.0f), vec3(0.0f, 0.0f, -1.0f)),
        mat4x4::lookAt(vec3(0.0f), vec3(0.0f, 1.0f, 0.0f), vec3(0.0f, 0.0f, 1.0f)),
        mat4x4::lookAt(vec3(0.0f), vec3(0.0f, 0.0f, -1.0f), vec3(0.0f, 1.0f, 0.0f)),
        mat4x4::lookAt(vec3(0.0f), vec3(0.0f, 0.0f, 1.0f), vec3(0.0f, 1.0f, 0.0f))
    };

    auto cmd = device.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    auto viewport = vkInits::viewportInfo({dimension, dimension});
    auto scissor = vkInits::scissorInfo({dimension, dimension});

    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    VkImageSubresourceRange subresourceRange{};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.levelCount = mipLevels;
    subresourceRange.layerCount = 6;

    vkTools::SetImageLayout(cmd,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                            prefiltered.image,
                            subresourceRange);

    alignas(16) mat4x4 pushBlock[2] = {};
    pushBlock[1] = mat4x4::perspective(PI32 / 2, 1.0f, Camera::DEFAULT_ZNEAR, Camera::DEFAULT_ZFAR);

    for (uint32_t level = 0; level < mipLevels; level++)
    {
        const float roughness = float(level) / float(mipLevels - 1);
        vkCmdPushConstants(cmd, prefilteredPipelineLayout,
                            pushConstants[1].stageFlags,
                            pushConstants[1].offset,
                            pushConstants[1].size,
                            &roughness);

        for (uint32_t i = 0; i < 6; i++)
        {
            viewport.width = float(dimension * std::pow(0.5f, level));
            viewport.height = float(dimension * std::pow(0.5f, level));
            vkCmdSetViewport(cmd, 0, 1, &viewport);
            vkCmdBeginRenderPass(cmd, &renderBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, prefilteredPipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    prefilteredPipelineLayout, 0, 1,
                                    &prefilteredDescriptorSet, 0, nullptr);

            pushBlock[0] = viewMatrices[i];

            vkCmdPushConstants(cmd, prefilteredPipelineLayout,
                               pushConstants[0].stageFlags,
                               pushConstants[0].offset,
                               pushConstants[0].size,
                               &pushBlock);

            models.skybox.draw(cmd);

            vkCmdEndRenderPass(cmd);

            vkTools::SetImageLayout(cmd,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                    offscreen.image);

            VkImageCopy copyRegion{};
            copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.srcSubresource.baseArrayLayer = 0;
            copyRegion.srcSubresource.layerCount = 1;
            copyRegion.srcSubresource.mipLevel = 0;
            copyRegion.srcOffset = {};

            copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.dstSubresource.baseArrayLayer = i;
            copyRegion.dstSubresource.layerCount = 1;
            copyRegion.dstSubresource.mipLevel = level;
            copyRegion.dstOffset = {};

            copyRegion.extent.width = uint32_t(viewport.width);
            copyRegion.extent.height = uint32_t(viewport.height);
            copyRegion.extent.depth = 1;

            vkCmdCopyImage(cmd, offscreen.image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           prefiltered.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &copyRegion);

            vkTools::SetImageLayout(cmd,
                                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                    offscreen.image);
        }
    }

    vkTools::SetImageLayout(cmd,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                            prefiltered.image, subresourceRange);

    device.flushCommandBuffer(cmd, graphicsQueue);

    vkDestroyRenderPass(device, prefilteredRenderpass, nullptr);
    vkDestroyFramebuffer(device, offscreen.framebuffer, nullptr);
    vkFreeMemory(device, offscreen.memory, nullptr);
    vkDestroyImageView(device, offscreen.view, nullptr);
    vkDestroyImage(device, offscreen.image, nullptr);
    vkDestroyDescriptorPool(device, prefilteredDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, prefilteredSetLayout, nullptr);
    vkDestroyPipeline(device, prefilteredPipeline, nullptr);
    vkDestroyPipelineLayout(device, prefilteredPipelineLayout, nullptr);
    prefilteredFragmentShader.destroy(device);
}

void ModelViewer::loadResources()
{
    // Object
    {
        models.object.loadSpherePrimitive(&device, graphicsQueue);
        models.object.transform = mat4x4::identity();

        auto sb = StringbBuilder(100);
        constexpr auto materialPath = view("materials/alien-panels/");

        sb << ASSETS_PATH << materialPath << view("albedo.png");
        textures.albedo.loadRGBA(&device, graphicsQueue, sb.c_str(), true);

        sb.flush() << ASSETS_PATH << materialPath << view("normal.png");
        textures.normal.loadRGBA(&device, graphicsQueue, sb.c_str());

        sb.flush() << ASSETS_PATH << materialPath << view("roughness.png");
        textures.roughness.loadRGBA(&device, graphicsQueue, sb.c_str());

        sb.flush() << ASSETS_PATH << materialPath << view("metallic.png");
        textures.metallic.loadRGBA(&device, graphicsQueue, sb.c_str());

        sb.flush() << ASSETS_PATH << materialPath << view("ao.png");
        textures.ao.loadRGBA(&device, graphicsQueue, sb.c_str());

        sb.destroy();
    }

    // Skybox
    {
        models.skybox.load(&device, graphicsQueue);

        StringbBuilder sbs[] = {
            StringbBuilder(100),
            StringbBuilder(100),
            StringbBuilder(100),
            StringbBuilder(100),
            StringbBuilder(100),
            StringbBuilder(100)
        };

        constexpr auto skyboxPath = view("../../assets/skybox/");

        const char *files[] = {
            (sbs[0] << skyboxPath << view("px.png")).c_str(),
            (sbs[1] << skyboxPath << view("nx.png")).c_str(),
            (sbs[2] << skyboxPath << view("py.png")).c_str(),
            (sbs[3] << skyboxPath << view("ny.png")).c_str(),
            (sbs[4] << skyboxPath << view("pz.png")).c_str(),
            (sbs[5] << skyboxPath << view("nz.png")).c_str()
        };
        textures.skybox.load(&device, graphicsQueue, files);

        for (size_t i = 0; i < arraysize(sbs); i++)
            sbs[i].destroy();
    }
}

void ModelViewer::buildUniformBuffers()
{
    for (size_t i = 0; i < MAX_IMAGES_IN_FLIGHT; i++)
    {
        device.createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, MEM_FLAG_HOST_VISIBLE,
                            sizeof(mvp_matrix), scene.cameraBuffers[i]);
        device.createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, MEM_FLAG_HOST_VISIBLE,
                            sizeof(mvp_matrix), scene.lightBuffers[i]);
    }

    updateCamera(0.0f);
    updateLights();
}

void ModelViewer::buildDescriptors()
{
    const VkDescriptorPoolSize poolSizes[] = {
        vkInits::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_IMAGES_IN_FLIGHT),
        vkInits::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_IMAGES_IN_FLIGHT),
        vkInits::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 9 * MAX_IMAGES_IN_FLIGHT)
    };

    auto maxSets = vkTools::descriptorPoolMaxSets(poolSizes);

    const auto descriptorPoolInfo = vkInits::descriptorPoolCreateInfo(poolSizes, maxSets);
    vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &m_descriptorPool);

    // Scene

    constexpr auto combinedStage = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    const VkDescriptorSetLayoutBinding bindings[] = {
        vkInits::descriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, combinedStage),
        vkInits::descriptorSetLayoutBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkInits::descriptorSetLayoutBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkInits::descriptorSetLayoutBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkInits::descriptorSetLayoutBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkInits::descriptorSetLayoutBinding(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkInits::descriptorSetLayoutBinding(6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkInits::descriptorSetLayoutBinding(7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkInits::descriptorSetLayoutBinding(8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkInits::descriptorSetLayoutBinding(9, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
    };

    auto setLayoutInfo = vkInits::descriptorSetLayoutCreateInfo(bindings);
    vkCreateDescriptorSetLayout(device, &setLayoutInfo, nullptr, &scene.setLayout);

    VkDescriptorSetLayout layouts[MAX_IMAGES_IN_FLIGHT] = {};
    arrayfill(layouts, scene.setLayout);

    auto allocInfo = vkInits::descriptorSetAllocateInfo(m_descriptorPool, layouts);
    vkAllocateDescriptorSets(device, &allocInfo, scene.descriptorSets);

    for (size_t i = 0; i < MAX_IMAGES_IN_FLIGHT; i++)
    {
        auto &setRef = scene.descriptorSets[i];
        const VkWriteDescriptorSet writes[] = {
            vkInits::writeDescriptorSet(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, setRef, &scene.cameraBuffers[i].descriptor),
            vkInits::writeDescriptorSet(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, setRef, &scene.lightBuffers[i].descriptor),
            vkInits::writeDescriptorSet(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, setRef, &brdf.descriptor),
            vkInits::writeDescriptorSet(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, setRef, &irradiance.descriptor),
            vkInits::writeDescriptorSet(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, setRef, &prefiltered.descriptor),
            vkInits::writeDescriptorSet(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, setRef, &textures.albedo.descriptor),
            vkInits::writeDescriptorSet(6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, setRef, &textures.normal.descriptor),
            vkInits::writeDescriptorSet(7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, setRef, &textures.roughness.descriptor),
            vkInits::writeDescriptorSet(8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, setRef, &textures.metallic.descriptor),
            vkInits::writeDescriptorSet(9, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, setRef, &textures.ao.descriptor)
        };

        vkUpdateDescriptorSets(device, uint32_t(arraysize(writes)), writes, 0, nullptr);
    }

    // Skybox

    const VkDescriptorSetLayoutBinding skyboxBindings[] = {
        vkInits::descriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
    };

    setLayoutInfo = vkInits::descriptorSetLayoutCreateInfo(skyboxBindings);
    vkCreateDescriptorSetLayout(device, &setLayoutInfo, nullptr, &skybox.setLayout);

    arrayfill(layouts, skybox.setLayout);

    allocInfo = vkInits::descriptorSetAllocateInfo(m_descriptorPool, layouts);
    vkAllocateDescriptorSets(device, &allocInfo, skybox.descriptorSets);

    for (size_t i = 0; i < MAX_IMAGES_IN_FLIGHT; i++)
    {
        auto &setRef = skybox.descriptorSets[i];
        const VkWriteDescriptorSet writes[] = {
            vkInits::writeDescriptorSet(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, setRef, &textures.skybox.descriptor)
        };

        vkUpdateDescriptorSets(device, uint32_t(arraysize(writes)), writes, 0, nullptr);
    }
}

void ModelViewer::buildPipelines()
{
    VkPipelineShaderStageCreateInfo shaderStages[] = {
        scene.vertexShader.shaderStage(), scene.fragmentShader.shaderStage()
    };

    auto bindingDescription = vkInits::vertexBindingDescription(sizeof(Model3D::Vertex));

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = uint32_t(arraysize(Model3D::Attributes));
    vertexInputInfo.pVertexAttributeDescriptions = Model3D::Attributes;

    auto inputAssembly = vkInits::inputAssemblyInfo();
    auto viewport = vkInits::viewportInfo(extent);
    auto scissor = vkInits::scissorInfo(extent);

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    auto rasterizer = vkInits::rasterizationStateInfo(VK_FRONT_FACE_COUNTER_CLOCKWISE);
    auto multisampling = vkInits::pipelineMultisampleStateCreateInfo(sampleCount);

    auto depthStencil = vkInits::depthStencilStateInfo();
    auto colorBlendAttachment = vkInits::pipelineColorBlendAttachmentState();
    auto colourBlend = vkInits::pipelineColorBlendStateCreateInfo();
    colourBlend.attachmentCount = 1;
    colourBlend.pAttachments = &colorBlendAttachment;

    const VkPushConstantRange pushConstants[] = {Model3D::pushConstant()};

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pSetLayouts = &scene.setLayout;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = pushConstants;
    pipelineLayoutInfo.pushConstantRangeCount = uint32_t(arraysize(pushConstants));
    vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &scene.pipelineLayout);

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
    pipelineInfo.layout = scene.pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &scene.pipeline);

    // Skybox

    shaderStages[0] = skybox.vertexShader.shaderStage();
    shaderStages[1] = skybox.fragmentShader.shaderStage();

    bindingDescription.stride = sizeof(CubemapModel::Vertex);

    vertexInputInfo.vertexAttributeDescriptionCount = uint32_t(arraysize(CubemapModel::Attributes));
    vertexInputInfo.pVertexAttributeDescriptions = CubemapModel::Attributes;

    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;

    auto skyboxPushConstant = ModelViewMatrix::pushConstant();

    pipelineLayoutInfo.pSetLayouts = &skybox.setLayout;
    pipelineLayoutInfo.pPushConstantRanges = &skyboxPushConstant;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &skybox.pipelineLayout);

    pipelineInfo.layout = skybox.pipelineLayout;
    vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &skybox.pipeline);
}

void ModelViewer::updateCamera(float dt)
{
    m_mainCamera.controls.rotateUp = pltf::IsKeyDown(pltf::key_code::W);
    m_mainCamera.controls.rotateDown = pltf::IsKeyDown(pltf::key_code::S);
    m_mainCamera.controls.rotateLeft = pltf::IsKeyDown(pltf::key_code::A);
    m_mainCamera.controls.rotateRight = pltf::IsKeyDown(pltf::key_code::D);
    m_mainCamera.controls.moveForward = pltf::IsKeyDown(pltf::key_code::Up);
    m_mainCamera.controls.moveBackward = pltf::IsKeyDown(pltf::key_code::Down);
    m_mainCamera.controls.moveLeft = pltf::IsKeyDown(pltf::key_code::Left);
    m_mainCamera.controls.moveRight = pltf::IsKeyDown(pltf::key_code::Right);
    m_mainCamera.update(dt, aspectRatio);

    const auto ubo = m_mainCamera.getModelViewProjection();
    for (size_t i = 0; i < MAX_IMAGES_IN_FLIGHT; i++)
    {
        scene.cameraBuffers[i].map(device);
        *static_cast<mvp_matrix*>(scene.cameraBuffers[i].mapped) = ubo;
        scene.cameraBuffers[i].unmap(device);
    }
}

void ModelViewer::updateLights()
{
    const auto ubo = m_lights.getData();
    for (size_t i = 0; i < MAX_IMAGES_IN_FLIGHT; i++)
    {
        scene.lightBuffers[i].map(device);
        *static_cast<LightData*>(scene.lightBuffers[i].mapped) = ubo;
        scene.lightBuffers[i].unmap(device);
    }
}

void ModelViewer::recordFrame(VkCommandBuffer cmdBuffer)
{
    VkClearValue clearValues[2];
    clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
    clearValues[1].depthStencil = {1.0f, 0};

    auto cmdBeginInfo = vkInits::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);
    auto renderBeginInfo = vkInits::renderPassBeginInfo(renderPass, extent);
    renderBeginInfo.clearValueCount = 1;
    renderBeginInfo.pClearValues = clearValues;
    renderBeginInfo.clearValueCount = uint32_t(arraysize(clearValues));

    vkBeginCommandBuffer(cmdBuffer, &cmdBeginInfo);

    renderBeginInfo.framebuffer = framebuffers[imageIndex];
    vkCmdBeginRenderPass(cmdBuffer, &renderBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    {
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skybox.pipeline);
        vkCmdBindDescriptorSets(cmdBuffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                skybox.pipelineLayout,
                                0,
                                1,
                                &skybox.descriptorSets[currentFrame],
                                0,
                                nullptr);

        const auto [stage, offset, size] = ModelViewMatrix::pushConstant();
        auto modelView = m_mainCamera.getModelView();
        vkCmdPushConstants(cmdBuffer, skybox.pipelineLayout, stage, offset, size, &modelView);

        models.skybox.draw(cmdBuffer);
    }

    {
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, scene.pipeline);
        vkCmdBindDescriptorSets(cmdBuffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                scene.pipelineLayout,
                                0,
                                1,
                                &scene.descriptorSets[currentFrame],
                                0,
                                nullptr);

        const auto [stage, offset, size] = Model3D::pushConstant();
        vkCmdPushConstants(cmdBuffer, scene.pipelineLayout, stage, offset, size, &models.object.transform);

        models.object.draw(cmdBuffer);
    }

    vkCmdEndRenderPass(cmdBuffer);
    vkEndCommandBuffer(cmdBuffer);
}
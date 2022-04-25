#include "model_viewer.hpp"

#if defined(DEBUG)
static const bool C_VALIDATION = true;
#else
static const bool C_VALIDATION = false;
#endif

void CoreMessageCallback(log_level level, const char *string)
{
    pltf::DebugString(string);
    pltf::DebugBreak();
}

ModelViewer::ModelViewer(pltf::logical_device platform) : VulkanInstance(platform, MegaBytes(64))
{
    settings.title = "Pbr Demo";
    settings.syncMode = VSyncMode::Off;
    settings.enValidation = C_VALIDATION;

    auto dispatcher = EventDispatcher<ModelViewer>(platform, this);

    VulkanInstance::coreMessage = CoreMessageCallback;
    VulkanInstance::prepare();

    generateBDRF();

    m_mainCamera.init();
    m_lights.init();

    buildUniformBuffers();
    buildScene();
    buildSkybox();
    buildDescriptors();

    constexpr auto shaderPath = view("../../shaders/");
    auto sb = StringbBuilder(100);

    sb << shaderPath << view("pbr_vert.spv");
    scene.vertexShader.load(device, sb.c_str());
    sb.flush() << shaderPath << view("pbr_frag.spv");
    scene.fragmentShader.load(device, sb.c_str());
    sb.flush() << shaderPath << view("skybox_vert.spv");
    skybox.vertexShader.load(device, sb.c_str());
    sb.flush() << shaderPath << view("skybox_frag.spv");
    skybox.fragmentShader.load(device, sb.c_str());

    sb.destroy();

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
    const auto deviceString = view<const char>(deviceProps.deviceName, StringbBuilder::strlen(deviceProps.deviceName));

    m_overlay->begin();
    m_overlay->draw("PBR demo", vec2(50.0f, 12.0f));

    m_overlay->textSize = 0.8f;
    m_overlay->textAlignment = text_align::left;
    m_overlay->draw("Device:", vec2(5.0f, 88.0f));
    m_overlay->draw(deviceString, vec2(5.0f, 90.0f));
    m_overlay->end();
}

ModelViewer::~ModelViewer()
{
    vkDeviceWaitIdle(device);

    vkDestroySampler(device, brdf.sampler, nullptr);
    vkDestroyImageView(device, brdf.view, nullptr);
    vkDestroyImage(device, brdf.image, nullptr);
    vkFreeMemory(device, brdf.memory, nullptr);

    m_overlay->destroy();

    vkDestroyPipeline(device, skybox.pipeline, nullptr);
    vkDestroyPipelineLayout(device, skybox.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, skybox.setLayout, nullptr);
    skybox.vertexShader.destroy(device);
    skybox.fragmentShader.destroy(device);
    skybox.model->destroy(device);

    vkDestroyPipeline(device, scene.pipeline, nullptr);
    vkDestroyPipelineLayout(device, scene.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, scene.setLayout, nullptr);
    scene.vertexShader.destroy(device);
    scene.fragmentShader.destroy(device);
    scene.model->destroy(device);

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

void ModelViewer::generateBDRF()
{
    const auto format = VK_FORMAT_R16G16_SFLOAT;
    const uint32_t dimension = 512;

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

    auto colourAttachment = vkInits::attachmentDescription(format);
    colourAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colourAttachmentRef{};
    colourAttachmentRef.attachment = 0;
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

    VkRenderPass brdfRenderPass = VK_NULL_HANDLE;
    vkCreateRenderPass(device, &renderPassInfo, nullptr, &brdfRenderPass);

    // Framebuffer

    auto frameBufferInfo = vkInits::framebufferCreateInfo();
    frameBufferInfo.renderPass = brdfRenderPass;
    frameBufferInfo.attachmentCount = 1;
    frameBufferInfo.pAttachments = &brdf.view;
    frameBufferInfo.width = dimension;
    frameBufferInfo.height = dimension;

    VkFramebuffer brdfFramebuffer = VK_NULL_HANDLE;
    vkCreateFramebuffer(device, &frameBufferInfo, nullptr, &brdfFramebuffer);

    // Ddescriptors - none needed

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
    constexpr auto shaderPath = view("../../shaders/");

    VertexShader brdfVertexShader;
    sb << shaderPath << view("brdf_vert.spv");
    brdfVertexShader.load(device, sb.c_str());

    FragmentShader brdfFragmentShader;
    sb.flush() << shaderPath << view("brdf_frag.spv");
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

    // Draw

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

void ModelViewer::buildScene()
{
    auto model = allocate<PBRModel>(1);

    model->mesh.loadCube(&device, graphicsQueue);
    model->transform = mat4x4::identity();

    auto sb = StringbBuilder(100);
    constexpr auto assetsPath = view("../../assets/materials/");
    constexpr auto materialName = view("patterned-bw-vinyl-bl/");

    sb << assetsPath << materialName << view("albedo.png");
    model->albedo.loadRGBA(&device, graphicsQueue, sb.c_str(), true);

    sb.flush() << assetsPath << materialName << view("normal.png");
    model->normal.loadRGBA(&device, graphicsQueue, sb.c_str());

    sb.flush() << assetsPath << materialName << view("roughness.png");
    model->roughness.loadRGBA(&device, graphicsQueue, sb.c_str());

    sb.flush() << assetsPath << materialName << view("metallic.png");
    model->metallic.loadRGBA(&device, graphicsQueue, sb.c_str());

    sb.flush() << assetsPath << materialName << view("ao.png");
    model->ao.loadRGBA(&device, graphicsQueue, sb.c_str());

    sb.destroy();

    scene.model = model;
}

void ModelViewer::buildSkybox()
{
    auto model = allocate<CubeMapModel>(1);
    model->load(&device, graphicsQueue);

    StringbBuilder sbs[] = {
        StringbBuilder(100),
        StringbBuilder(100),
        StringbBuilder(100),
        StringbBuilder(100),
        StringbBuilder(100),
        StringbBuilder(100)
    };

    constexpr auto skyboxPath = view("../../assets/skybox/");

    model->map.filenames[0] = (sbs[0] << skyboxPath << "px.png").c_str();
    model->map.filenames[1] = (sbs[1] << skyboxPath << "nx.png").c_str();
    model->map.filenames[2] = (sbs[2] << skyboxPath << "py.png").c_str();
    model->map.filenames[3] = (sbs[3] << skyboxPath << "ny.png").c_str();
    model->map.filenames[4] = (sbs[4] << skyboxPath << "pz.png").c_str();
    model->map.filenames[5] = (sbs[5] << skyboxPath << "nz.png").c_str();
    model->map.load(&device, graphicsQueue);

    for (size_t i = 0; i < arraysize(sbs); i++)
        sbs[i].destroy();

    skybox.model = model;
}

void ModelViewer::buildUniformBuffers()
{
    const auto cameraInfo = vkInits::bufferCreateInfo(sizeof(mvp_matrix), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    const auto lightsInfo = vkInits::bufferCreateInfo(sizeof(LightData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    for (size_t i = 0; i < MAX_IMAGES_IN_FLIGHT; i++)
    {
        scene.cameraBuffers[i].create(&device, &cameraInfo, MEM_FLAG_HOST_VISIBLE);
        scene.lightBuffers[i].create(&device, &lightsInfo, MEM_FLAG_HOST_VISIBLE);
    }

    updateCamera(0.0f);
    updateLights();
}

void ModelViewer::buildDescriptors()
{
    const VkDescriptorPoolSize poolSizes[] = {
        vkInits::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_IMAGES_IN_FLIGHT),
        vkInits::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_IMAGES_IN_FLIGHT),
        vkInits::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5 * MAX_IMAGES_IN_FLIGHT),
        vkInits::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_IMAGES_IN_FLIGHT)
    };

    size_t maxSets = 0;
    for (size_t i = 0; i < arraysize(poolSizes); i++)
        maxSets += poolSizes[i].descriptorCount;

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
        vkInits::descriptorSetLayoutBinding(6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
    };

    auto setLayoutInfo = vkInits::descriptorSetLayoutCreateInfo(bindings);
    vkCreateDescriptorSetLayout(device, &setLayoutInfo, nullptr, &scene.setLayout);

    VkDescriptorSetLayout layouts[MAX_IMAGES_IN_FLIGHT] = {};
    arrayfill(layouts, scene.setLayout);

    auto allocInfo = vkInits::descriptorSetAllocateInfo(m_descriptorPool, layouts);
    vkAllocateDescriptorSets(device, &allocInfo, scene.descriptorSets);

    for (size_t i = 0; i < MAX_IMAGES_IN_FLIGHT; i++)
    {
        const auto cameraInfo = scene.cameraBuffers[i].descriptor();
        const auto lightsInfo = scene.lightBuffers[i].descriptor();

        auto &setRef = scene.descriptorSets[i];
        const VkWriteDescriptorSet writes[] = {
            vkInits::writeDescriptorSet(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, setRef, &cameraInfo),
            vkInits::writeDescriptorSet(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, setRef, &lightsInfo),
            vkInits::writeDescriptorSet(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, setRef, &scene.model->albedo.descriptor),
            vkInits::writeDescriptorSet(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, setRef, &scene.model->normal.descriptor),
            vkInits::writeDescriptorSet(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, setRef, &scene.model->roughness.descriptor),
            vkInits::writeDescriptorSet(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, setRef, &scene.model->metallic.descriptor),
            vkInits::writeDescriptorSet(6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, setRef, &scene.model->ao.descriptor)
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
            vkInits::writeDescriptorSet(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, setRef, &skybox.model->map.descriptor)
        };

        vkUpdateDescriptorSets(device, uint32_t(arraysize(writes)), writes, 0, nullptr);
    }
}

void ModelViewer::buildPipelines()
{
    VkPipelineShaderStageCreateInfo shaderStages[] = {
        scene.vertexShader.shaderStage(), scene.fragmentShader.shaderStage()
    };

    auto bindingDescription = vkInits::vertexBindingDescription(sizeof(MeshVertex));

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = uint32_t(arraysize(Mesh3D::Attributes));
    vertexInputInfo.pVertexAttributeDescriptions = Mesh3D::Attributes;

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

    const VkPushConstantRange pushConstants[] = {
        PBRModel::pushConstant()
    };

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
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &scene.pipeline);

    // Skybox

    shaderStages[0] = skybox.vertexShader.shaderStage();
    shaderStages[1] = skybox.fragmentShader.shaderStage();

    bindingDescription.stride = sizeof(CubeMapVertex);

    vertexInputInfo.vertexAttributeDescriptionCount = uint32_t(arraysize(CubeMapModel::Attributes));
    vertexInputInfo.pVertexAttributeDescriptions = CubeMapModel::Attributes;

    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;

    auto skyboxPushConstant = ModelViewMatrix::pushConstant();

    pipelineLayoutInfo.pSetLayouts = &skybox.setLayout;
    pipelineLayoutInfo.pPushConstantRanges = &skyboxPushConstant;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &skybox.pipelineLayout);

    pipelineInfo.layout = skybox.pipelineLayout;
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &skybox.pipeline);
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
        scene.cameraBuffers[i].fill(device, &ubo);
}

void ModelViewer::updateLights()
{
    const auto ubo = m_lights.getData();
    for (size_t i = 0; i < MAX_IMAGES_IN_FLIGHT; i++)
        scene.lightBuffers[i].fill(device, &ubo);
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

    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skybox.pipeline);
    vkCmdBindDescriptorSets(cmdBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            skybox.pipelineLayout,
                            0,
                            1,
                            &skybox.descriptorSets[currentFrame],
                            0,
                            nullptr);

    auto [stage, offset, size] = ModelViewMatrix::pushConstant();
    auto modelView = m_mainCamera.getModelView();
    vkCmdPushConstants(cmdBuffer, skybox.pipelineLayout, stage, offset, size, &modelView);

    skybox.model->draw(cmdBuffer);

    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, scene.pipeline);
    vkCmdBindDescriptorSets(cmdBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            scene.pipelineLayout,
                            0,
                            1,
                            &scene.descriptorSets[currentFrame],
                            0,
                            nullptr);

    scene.model->draw(cmdBuffer, scene.pipelineLayout);

    vkCmdEndRenderPass(cmdBuffer);
    vkEndCommandBuffer(cmdBuffer);
}
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

ModelViewer::ModelViewer(pltf::logical_device platform) : VulkanInstance(MegaBytes(64))
{
    settings.title = "Pbr Demo";
    settings.syncMode = VSyncMode::Off;
    settings.enValidation = C_VALIDATION;

    VulkanInstance::platformDevice = platform;
    VulkanInstance::coreMessage = CoreMessageCallback;

    pltf::WindowCreate(platformDevice, settings.title);

    VulkanInstance::prepare();

    m_mainCamera.init();
    m_lights.init();

    buildUniformBuffers();
    buildScene();
    buildSkybox();
    buildDescriptors();

    scene.vertexShader.load(device, INTERNAL_DIR "shaders/pbr_vert.spv");
    scene.fragmentShader.load(device, INTERNAL_DIR "shaders/pbr_frag.spv");
    skybox.vertexShader.load(device, INTERNAL_DIR "shaders/skybox_vert.spv");
    skybox.fragmentShader.load(device, INTERNAL_DIR "shaders/skybox_frag.spv");

    buildPipelines();

    // Prepare text overlay

    m_overlay = push<VulkanTextOverlay>(1);

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

    m_overlay->begin();
    m_overlay->draw("PBR demo", vec2(50.0f, 12.0f));
    m_overlay->end();
}

ModelViewer::~ModelViewer()
{
    vkDeviceWaitIdle(device);

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

void ModelViewer::buildScene()
{
    auto model = push<PBRModel>(1);

    model->mesh.loadSphere(&device, graphicsQueue);
    model->transform = mat4x4::identity();

#define MATERIAL_NAME "patterned-bw-vinyl-bl/"
    auto albedoPath = INTERNAL_DIR MATERIALS_DIR MATERIAL_NAME "albedo.png";
    auto normalPath = INTERNAL_DIR MATERIALS_DIR MATERIAL_NAME "normal.png";
    auto roughnessPath = INTERNAL_DIR MATERIALS_DIR MATERIAL_NAME "roughness.png";
    auto metallicPath = INTERNAL_DIR MATERIALS_DIR MATERIAL_NAME "metallic.png";
    auto aoPath = INTERNAL_DIR MATERIALS_DIR MATERIAL_NAME "ao.png";
#undef MATERIAL_NAME
    model->albedo.loadRGBA(&device, graphicsQueue, albedoPath, true);
    model->normal.loadRGBA(&device, graphicsQueue, normalPath);
    model->roughness.loadRGBA(&device, graphicsQueue, roughnessPath);
    model->metallic.loadRGBA(&device, graphicsQueue, metallicPath);
    model->ao.loadRGBA(&device, graphicsQueue, aoPath);

    scene.model = model;
}

void ModelViewer::buildSkybox()
{
    auto model = push<CubeMapModel>(1);
    model->load(&device, graphicsQueue);

    model->map.filenames[0] = INTERNAL_DIR "assets/skybox/px.png";
    model->map.filenames[1] = INTERNAL_DIR "assets/skybox/nx.png";
    model->map.filenames[2] = INTERNAL_DIR "assets/skybox/py.png";
    model->map.filenames[3] = INTERNAL_DIR "assets/skybox/ny.png";
    model->map.filenames[4] = INTERNAL_DIR "assets/skybox/pz.png";
    model->map.filenames[5] = INTERNAL_DIR "assets/skybox/nz.png";
    model->map.load(&device, graphicsQueue);

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
    VkClearValue colourValue;
    colourValue.color = {0.1f, 0.1f, 0.1f, 1.0f};
    VkClearValue depthStencilValue;
    depthStencilValue.depthStencil = {1.0f, 0};

    const VkClearValue clearValues[] = {colourValue, depthStencilValue};

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
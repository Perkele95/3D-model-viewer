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
    buildModelData();
    buildDescriptors();

    m_pbr.vertexShader.load(device, INTERNAL_DIR "shaders/pbr_vert.spv");
    m_pbr.fragmentShader.load(device, INTERNAL_DIR "shaders/pbr_frag.spv");

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

    constexpr auto titleString = view("PBR demo");
    m_overlay->draw(titleString, vec2(50.0f, 12.0f));

    m_overlay->end();
}

ModelViewer::~ModelViewer()
{
    vkDeviceWaitIdle(device);

    m_overlay->destroy();

    m_pbr.destroy(device);

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

    vkDestroyPipeline(device, m_pbr.pipeline, nullptr);
    vkDestroyPipelineLayout(device, m_pbr.pipelineLayout, nullptr);

    // NOTE(arle): pipeline layout does not need to be recreated
    buildPipelines();

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

void ModelViewer::buildModelData()
{
    auto model = push<Model3D>(1);

    model->mesh.loadSphere(&device, graphicsQueue);
    model->transform = mat4x4::identity();

    const auto format = VK_FORMAT_R8G8B8A8_SRGB;
#define MATERIAL_NAME "patterned-bw-vinyl-bl/"
    auto albedoPath = INTERNAL_DIR MATERIALS_DIR MATERIAL_NAME "albedo.png";
    auto normalPath = INTERNAL_DIR MATERIALS_DIR MATERIAL_NAME "normal.png";
    auto roughnessPath = INTERNAL_DIR MATERIALS_DIR MATERIAL_NAME "roughness.png";
    auto metallicPath = INTERNAL_DIR MATERIALS_DIR MATERIAL_NAME "metallic.png";
    auto aoPath = INTERNAL_DIR MATERIALS_DIR MATERIAL_NAME "ao.png";
#undef MATERIAL_NAME
    model->albedo.loadFromFile(&device, graphicsQueue, format, albedoPath);
    model->normal.loadFromFile(&device, graphicsQueue, format, normalPath);
    model->roughness.loadFromFile(&device, graphicsQueue, format, roughnessPath);
    model->metallic.loadFromFile(&device, graphicsQueue, format, metallicPath);
    model->ao.loadFromFile(&device, graphicsQueue, format, aoPath);

    m_pbr.model = model;
}

void ModelViewer::buildUniformBuffers()
{
    const auto cameraInfo = vkInits::bufferCreateInfo(sizeof(mvp_matrix), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    const auto lightsInfo = vkInits::bufferCreateInfo(sizeof(LightData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    for (size_t i = 0; i < MAX_IMAGES_IN_FLIGHT; i++)
    {
        m_pbr.cameraUniforms[i].create(&device, &cameraInfo, MEM_FLAG_HOST_VISIBLE);
        m_pbr.lightUniforms[i].create(&device, &lightsInfo, MEM_FLAG_HOST_VISIBLE);
    }

    updateCamera(0.0f);
    updateLights();
}

void ModelViewer::buildDescriptors()
{
    const VkDescriptorPoolSize poolSizes[] = {
        vkInits::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_IMAGES_IN_FLIGHT),
        vkInits::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_IMAGES_IN_FLIGHT),
        vkInits::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5 * MAX_IMAGES_IN_FLIGHT)
    };

    const auto descriptorPoolInfo = vkInits::descriptorPoolCreateInfo(poolSizes, 7 * MAX_IMAGES_IN_FLIGHT);
    vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &m_descriptorPool);

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
    vkCreateDescriptorSetLayout(device, &setLayoutInfo, nullptr, &m_pbr.setLayout);

    // Sets

    VkDescriptorSetLayout layouts[MAX_IMAGES_IN_FLIGHT] = {};
    for (size_t i = 0; i < MAX_IMAGES_IN_FLIGHT; i++)
        layouts[i] = m_pbr.setLayout;

    auto allocInfo = vkInits::descriptorSetAllocateInfo(m_descriptorPool, layouts);
    vkAllocateDescriptorSets(device, &allocInfo, m_pbr.descriptorSets);

    for (size_t i = 0; i < MAX_IMAGES_IN_FLIGHT; i++)
    {
        const auto cameraInfo = m_pbr.cameraUniforms[i].descriptor();
        const auto lightsInfo = m_pbr.lightUniforms[i].descriptor();

        auto &setRef = m_pbr.descriptorSets[i];
        const VkWriteDescriptorSet writes[] = {
            vkInits::writeDescriptorSet(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, setRef, &cameraInfo),
            vkInits::writeDescriptorSet(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, setRef, &lightsInfo),
            vkInits::writeDescriptorSet(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, setRef, &m_pbr.model->albedo.descriptor),
            vkInits::writeDescriptorSet(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, setRef, &m_pbr.model->normal.descriptor),
            vkInits::writeDescriptorSet(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, setRef, &m_pbr.model->roughness.descriptor),
            vkInits::writeDescriptorSet(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, setRef, &m_pbr.model->metallic.descriptor),
            vkInits::writeDescriptorSet(6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, setRef, &m_pbr.model->ao.descriptor)
        };

        vkUpdateDescriptorSets(device, uint32_t(arraysize(writes)), writes, 0, nullptr);
    }
}

void ModelViewer::buildPipelines()
{
    const VkPipelineShaderStageCreateInfo shaderStages[] = {
        m_pbr.vertexShader.shaderStage(), m_pbr.fragmentShader.shaderStage()
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
        Model3D::pushConstant()
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pSetLayouts = &m_pbr.setLayout;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = pushConstants;
    pipelineLayoutInfo.pushConstantRangeCount = uint32_t(arraysize(pushConstants));
    vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_pbr.pipelineLayout);

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
    pipelineInfo.layout = m_pbr.pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pbr.pipeline);
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
    m_mainCamera.update(dt);

    const auto ubo = m_mainCamera.calculateMatrix(aspectRatio);
    for (size_t i = 0; i < MAX_IMAGES_IN_FLIGHT; i++)
        m_pbr.cameraUniforms[i].fill(device, &ubo);
}

void ModelViewer::updateLights()
{
    const auto ubo = m_lights.getData();
    for (size_t i = 0; i < MAX_IMAGES_IN_FLIGHT; i++)
        m_pbr.lightUniforms[i].fill(device, &ubo);
}

void ModelViewer::recordFrame(VkCommandBuffer cmdBuffer)
{
    VkClearValue colourValue;
    colourValue.color = {};
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

    m_pbr.bind(cmdBuffer, currentFrame);
    m_pbr.model->draw(cmdBuffer, m_pbr.pipelineLayout);

    vkCmdEndRenderPass(cmdBuffer);
    vkEndCommandBuffer(cmdBuffer);
}
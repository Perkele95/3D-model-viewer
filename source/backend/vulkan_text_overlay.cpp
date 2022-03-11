#include "../../vendor/stb/stb_font_courier_40_latin1.inl"
#include "vulkan_text_overlay.hpp"
#include <string.h>

constexpr size_t QUAD_VERTEX_COUNT = 4;
constexpr size_t QUAD_INDEX_COUNT = 6;
constexpr size_t QUAD_VERTEX_STRIDE = QUAD_VERTEX_COUNT * sizeof(quad_vertex);
constexpr size_t QUAD_INDEX_STRIDE = QUAD_INDEX_COUNT * sizeof(quad_index);

constexpr size_t GUI_MAX_QUADS = KiloBytes(2);
constexpr size_t GUI_VERTEX_BUFFER_SIZE = GUI_MAX_QUADS * QUAD_VERTEX_STRIDE;
constexpr size_t GUI_INDEX_BUFFER_SIZE = GUI_MAX_QUADS * QUAD_INDEX_STRIDE;

constexpr float Z_ORDER_GUI_DEFAULT = 0.999f;
constexpr float Z_ORDER_GUI_INCREMENT = 0.001f;

constexpr static VkVertexInputAttributeDescription s_OverlayAttributes[] = {
    VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(quad_vertex, position)},
    VkVertexInputAttributeDescription{1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(quad_vertex, texCoord)},
    VkVertexInputAttributeDescription{2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(quad_vertex, colour)}
};

static stb_fontchar s_Fontdata[STB_SOMEFONT_NUM_CHARS];

text_overlay::text_overlay(const text_overlay_create_info *pInfo)
{
    m_imageCount = pInfo->imageCount;
    m_descriptorSets = pInfo->sharedPermanent->push<VkDescriptorSet>(m_imageCount);
    this->cmdBuffers = pInfo->sharedPermanent->push<VkCommandBuffer>(m_imageCount);
    m_quadCount = 0;
    m_zOrder = Z_ORDER_GUI_DEFAULT;
    m_device = pInfo->device;
    m_cmdPool = pInfo->cmdPool;
    m_depthFormat = pInfo->depthFormat;

    m_shaders[0] = shader_object(INTERNAL_DIR "shaders/gui_vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    m_shaders[1] = shader_object(INTERNAL_DIR "shaders/gui_frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    for (size_t i = 0; i < arraysize(m_shaders); i++)
        m_shaders[i].load(m_device->device);

    auto cmdInfo = vkInits::commandBufferAllocateInfo(m_cmdPool, m_imageCount);
    vkAllocateCommandBuffers(m_device->device, &cmdInfo, this->cmdBuffers);

    prepareFontTexture();

    const VkDescriptorPoolSize poolSizes[] = {
        vkInits::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_imageCount)
    };

    const auto poolInfo = vkInits::descriptorPoolCreateInfo(poolSizes, m_imageCount);
    auto result = vkCreateDescriptorPool(m_device->device, &poolInfo, nullptr, &m_descriptorPool);

    const VkDescriptorSetLayoutBinding bindings[] = {
        vkInits::descriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
    };

    const auto setLayoutInfo = vkInits::descriptorSetLayoutCreateInfo(bindings);
    result = vkCreateDescriptorSetLayout(m_device->device, &setLayoutInfo, nullptr, &m_setLayout);

    prepareDescriptorSets();
    prepareRenderpass();
    preparePipeline();
    prepareRenderBuffers();
}

text_overlay::~text_overlay()
{
    vkDestroyDescriptorPool(m_device->device, m_descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_device->device, m_setLayout, nullptr);

    m_vertexBuffer.destroy(m_device->device);
    m_indexBuffer.destroy(m_device->device);
    m_fontTexture.destroy(m_device->device);

    vkDestroyRenderPass(m_device->device, m_renderPass, nullptr);
    vkDestroyPipeline(m_device->device, m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device->device, m_pipelineLayout, nullptr);

    for (size_t i = 0; i < arraysize(m_shaders); i++)
        m_shaders[i].destroy(m_device->device);
}

void text_overlay::onWindowResize(VkCommandPool commandPool)
{
    m_cmdPool = commandPool;

    vkDestroyPipeline(m_device->device, m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device->device, m_pipelineLayout, nullptr);
    vkResetDescriptorPool(m_device->device, m_descriptorPool, VkFlags(0));

    prepareDescriptorSets();
    preparePipeline();
}

void text_overlay::begin()
{
    vkMapMemory(m_device->device, m_vertexBuffer.memory, 0,
                GUI_VERTEX_BUFFER_SIZE, 0, (void**)&m_mappedVertices);
    vkMapMemory(m_device->device, m_indexBuffer.memory, 0,
                GUI_INDEX_BUFFER_SIZE, 0, (void**)&m_mappedIndices);

    m_quadCount = 0;
    m_zOrder = Z_ORDER_GUI_DEFAULT;
}

void text_overlay::draw(view<const char> stringView, vec2<float> position)
{
    const uint32_t firstChar = STB_SOMEFONT_FIRST_CHAR;
    const float width = this->textSize / float(m_device->extent.width);
    const float height = this->textSize / float(m_device->extent.height);

    float x = 0.0f, y = 0.0f;//NOTE(arle): inspect asm on these switches
    switch (this->textType){
        case text_coord_type::absolute: {
            x = (float(position.x) / float(m_device->extent.width) * 2.0f) - 1.0f;
            y = (float(position.y) / float(m_device->extent.height) * 2.0f) - 1.0f;
        } break;
        case text_coord_type::relative: {
            x = (float(position.x) / 50.0f) - 1.0f;
            y = (float(position.y) / 50.0f) - 1.0f;
        } break;
    };

    float textWidth = 0.0f;
    for (size_t i = 0; i < stringView.count - 1; i++)
        textWidth += s_Fontdata[uint32_t(stringView[i]) - firstChar].advance * width;

    switch(this->textAlignment){
        case text_align::right: x -= textWidth; break;
        case text_align::centre: x -= textWidth / 2.0f; break;
        default: break;
    };

    for (size_t i = 0; i < stringView.count - 1; i++){
        auto charData = &s_Fontdata[uint32_t(stringView[i]) - firstChar];

        m_mappedVertices->position.x = x + charData->x0f * width;
        m_mappedVertices->position.y = y + charData->y0f * height;
        m_mappedVertices->position.z = m_zOrder;
        m_mappedVertices->texCoord = vec2(charData->s0, charData->t0);
        m_mappedVertices->colour = this->textTint;
        m_mappedVertices++;

        m_mappedVertices->position.x = x + charData->x1f * width;
        m_mappedVertices->position.y = y + charData->y0f * height;
        m_mappedVertices->position.z = m_zOrder;
        m_mappedVertices->texCoord = vec2(charData->s1, charData->t0);
        m_mappedVertices->colour = this->textTint;
        m_mappedVertices++;

        m_mappedVertices->position.x = x + charData->x1f * width;
        m_mappedVertices->position.y = y + charData->y1f * height;
        m_mappedVertices->position.z = m_zOrder;
        m_mappedVertices->texCoord = vec2(charData->s1, charData->t1);
        m_mappedVertices->colour = this->textTint;
        m_mappedVertices++;

        m_mappedVertices->position.x = x + charData->x0f * width;
        m_mappedVertices->position.y = y + charData->y1f * height;
        m_mappedVertices->position.z = m_zOrder;
        m_mappedVertices->texCoord = vec2(charData->s0, charData->t1);
        m_mappedVertices->colour = this->textTint;
        m_mappedVertices++;

        const auto offset = m_quadCount * QUAD_VERTEX_COUNT;
        m_mappedIndices[0] = quad_index(offset);
        m_mappedIndices[1] = quad_index(offset + 1);
        m_mappedIndices[2] = quad_index(offset + 2);
        m_mappedIndices[3] = quad_index(offset + 2);
        m_mappedIndices[4] = quad_index(offset + 3);
        m_mappedIndices[5] = quad_index(offset);
        m_mappedIndices += QUAD_INDEX_COUNT;

        m_quadCount++;
        m_zOrder -= Z_ORDER_GUI_INCREMENT;
        x += charData->advance * width;
    }
}

void text_overlay::end()
{
    vkUnmapMemory(m_device->device, m_vertexBuffer.memory);
    vkUnmapMemory(m_device->device, m_indexBuffer.memory);
    m_mappedVertices = nullptr;
    m_mappedIndices = nullptr;
}

void text_overlay::updateCmdBuffers(const VkFramebuffer *pFramebuffers)
{
    if(m_quadCount == 0)
        return;

    VkClearValue clearValues[2];
    clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
    clearValues[1].depthStencil = {1.0f, 0};

    auto cmdBeginInfo = vkInits::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);
    auto renderBeginInfo = vkInits::renderPassBeginInfo(m_renderPass, m_device->extent);
    renderBeginInfo.clearValueCount = 1;
    renderBeginInfo.pClearValues = clearValues;
    renderBeginInfo.clearValueCount = uint32_t(arraysize(clearValues));

    for (size_t i = 0; i < m_imageCount; i++){
        const auto cmdBuffer = this->cmdBuffers[i];
        vkBeginCommandBuffer(cmdBuffer, &cmdBeginInfo);

        renderBeginInfo.framebuffer = pFramebuffers[i];
        vkCmdBeginRenderPass(cmdBuffer, &renderBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout,
                                0, 1, &m_descriptorSets[i], 0, nullptr);

        const VkDeviceSize vertexOffset = 0;
        vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &m_vertexBuffer.data, &vertexOffset);

        const VkDeviceSize indexOffset = 0;
        vkCmdBindIndexBuffer(cmdBuffer, m_indexBuffer.data, indexOffset, VK_INDEX_TYPE_UINT32);

        const auto indexCount = uint32_t(m_quadCount * QUAD_INDEX_COUNT);
        vkCmdDrawIndexed(cmdBuffer, indexCount, 1, 0, 0, 0);

        vkCmdEndRenderPass(cmdBuffer);
        vkEndCommandBuffer(cmdBuffer);
    }
}

void text_overlay::prepareRenderpass()
{
    auto colourAttachment = vkInits::attachmentDescription(m_device->surfaceFormat.format);
    colourAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colourAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
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

void text_overlay::prepareFontTexture()
{
    static uint8_t fontpixels[STB_SOMEFONT_BITMAP_HEIGHT][STB_SOMEFONT_BITMAP_WIDTH];
    STB_SOMEFONT_CREATE(s_Fontdata, fontpixels, STB_SOMEFONT_BITMAP_HEIGHT);

    new (&m_fontTexture) texture2D();

    m_fontTexture.loadFromMemory(m_device,
                                 m_cmdPool,
                                 VK_FORMAT_R8_UNORM,
                                 {STB_SOMEFONT_BITMAP_WIDTH, STB_SOMEFONT_BITMAP_HEIGHT},
                                 fontpixels);
}

void text_overlay::prepareDescriptorSets()
{
    auto layouts = dyn_array<VkDescriptorSetLayout>(m_imageCount);
    layouts.fill(m_setLayout);

    auto allocInfo = vkInits::descriptorSetAllocateInfo(m_descriptorPool, layouts);
    vkAllocateDescriptorSets(m_device->device, &allocInfo, m_descriptorSets);

    const auto samplerImageDesc = m_fontTexture.descriptor;
    for(size_t i = 0; i < m_imageCount; i++){
        const auto samplerImageWrite = vkInits::writeDescriptorSet(0,
                                                                   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                                   m_descriptorSets[i],
                                                                   &samplerImageDesc);
        vkUpdateDescriptorSets(m_device->device, 1, &samplerImageWrite, 0, nullptr);
    }
}

void text_overlay::preparePipeline()
{
    const VkPipelineShaderStageCreateInfo shaderStages[] = {
        m_shaders[0].shaderStage(), m_shaders[1].shaderStage()
    };

    auto bindingDescription = vkInits::vertexBindingDescription(sizeof(quad_vertex));

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = uint32_t(arraysize(s_OverlayAttributes));
    vertexInputInfo.pVertexAttributeDescriptions = s_OverlayAttributes;

    auto inputAssembly = vkInits::inputAssemblyInfo();
    auto viewport = vkInits::viewportInfo(m_device->extent);
    auto scissor = vkInits::scissorInfo(m_device->extent);

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    auto rasterizer = vkInits::rasterizationStateInfo(VK_FRONT_FACE_CLOCKWISE);
    auto multisampling = vkInits::pipelineMultisampleStateCreateInfo(m_device->sampleCount);

    auto depthStencil = vkInits::depthStencilStateInfo();
    auto colorBlendAttachment = vkInits::pipelineColorBlendAttachmentState();
    auto colourBlend = vkInits::pipelineColorBlendStateCreateInfo();
    colourBlend.attachmentCount = 1;
    colourBlend.pAttachments = &colorBlendAttachment;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pSetLayouts = &m_setLayout;
    pipelineLayoutInfo.setLayoutCount = 1;
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

void text_overlay::prepareRenderBuffers()
{
    auto vertexInfo = vkInits::bufferCreateInfo(GUI_VERTEX_BUFFER_SIZE, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    m_vertexBuffer.create(m_device, &vertexInfo, MEM_FLAG_HOST_VISIBLE);

    auto indexInfo = vkInits::bufferCreateInfo(GUI_INDEX_BUFFER_SIZE, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    m_indexBuffer.create(m_device, &indexInfo, MEM_FLAG_HOST_VISIBLE);
}
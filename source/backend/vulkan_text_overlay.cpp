#include "../../vendor/stb/stb_font_courier_40_latin1.inl"
#include "vulkan_text_overlay.hpp"
#include "vulkan_tools.hpp"
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

void text_overlay::create(const text_overlay_create_info *pInfo)
{
    this->imageCount = pInfo->imageCount;
    this->descriptorSets = pInfo->allocator->allocPermanent<VkDescriptorSet>(this->imageCount);
    this->cmdBuffers = pInfo->allocator->allocPermanent<VkCommandBuffer>(this->imageCount);
    this->quadCount = 0;
    this->zOrder = Z_ORDER_GUI_DEFAULT;
    this->vertShaderModule = pInfo->vertex;
    this->fragShaderModule = pInfo->fragment;
    this->hDevice = pInfo->device;
    this->cmdPool = pInfo->cmdPool;
    this->depthFormat = pInfo->depthFormat;

    auto cmdInfo = vkInits::commandBufferAllocateInfo(this->cmdPool, this->imageCount);
    vkAllocateCommandBuffers(this->hDevice->device, &cmdInfo, this->cmdBuffers);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    VkResult result = vkCreateSampler(this->hDevice->device, &samplerInfo, nullptr, &this->sampler);

    uint8_t fontpixels[STB_SOMEFONT_BITMAP_HEIGHT][STB_SOMEFONT_BITMAP_WIDTH];
    STB_SOMEFONT_CREATE(s_Fontdata, fontpixels, STB_SOMEFONT_BITMAP_HEIGHT);

    prepareFontBuffer(fontpixels, {STB_SOMEFONT_BITMAP_WIDTH, STB_SOMEFONT_BITMAP_HEIGHT});

    const VkDescriptorPoolSize samplerImageSize = {
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, uint32_t(this->imageCount)
    };

    const VkDescriptorPoolSize poolSizes[] = {samplerImageSize};
    const uint32_t maxSets = poolSizes[0].descriptorCount;

    VkDescriptorPoolCreateInfo descPoolInfo{};
    descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descPoolInfo.poolSizeCount = uint32_t(arraysize(poolSizes));
    descPoolInfo.pPoolSizes = poolSizes;
    descPoolInfo.maxSets = maxSets;
    result = vkCreateDescriptorPool(this->hDevice->device, &descPoolInfo, nullptr, &this->descriptorPool);

    const auto samplerBinding = vkInits::descriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                                    VK_SHADER_STAGE_FRAGMENT_BIT);

    const VkDescriptorSetLayoutBinding bindings[] = {samplerBinding};

    VkDescriptorSetLayoutCreateInfo setLayoutInfo{};
    setLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    setLayoutInfo.pBindings = bindings;
    setLayoutInfo.bindingCount = uint32_t(arraysize(bindings));
    result = vkCreateDescriptorSetLayout(this->hDevice->device, &setLayoutInfo, nullptr, &this->setLayout);

    prepareDescriptorSets(pInfo->allocator);
    prepareRenderpass();
    preparePipeline();
    prepareRenderBuffers();
}

void text_overlay::destroy()
{
    vkDestroyDescriptorPool(this->hDevice->device, this->descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(this->hDevice->device, this->setLayout, nullptr);

    this->vertexBuffer.destroy(this->hDevice->device);
    this->indexBuffer.destroy(this->hDevice->device);
    this->fontBuffer.destroy(this->hDevice->device);

    vkDestroyRenderPass(this->hDevice->device, this->renderPass, nullptr);
    vkDestroyPipeline(this->hDevice->device, this->pipeline, nullptr);
    vkDestroyPipelineLayout(this->hDevice->device, this->pipelineLayout, nullptr);

    vkDestroyShaderModule(this->hDevice->device, this->vertShaderModule, nullptr);
    vkDestroyShaderModule(this->hDevice->device, this->fragShaderModule, nullptr);
    vkDestroySampler(this->hDevice->device, this->sampler, nullptr);
}

void text_overlay::onWindowResize(mv_allocator *allocator, VkCommandPool commandPool)
{
    this->cmdPool = commandPool;

    vkDestroyPipeline(this->hDevice->device, this->pipeline, nullptr);
    vkDestroyPipelineLayout(this->hDevice->device, this->pipelineLayout, nullptr);
    vkResetDescriptorPool(this->hDevice->device, this->descriptorPool, VkFlags(0));

    prepareDescriptorSets(allocator);
    preparePipeline();
}

void text_overlay::setTextTint(const vec4<float> tint)
{
    this->textTint = tint;
}

void text_overlay::setTextAlignment(text_align alignment)
{
    this->textAlignment = alignment;
}

void text_overlay::setTextType(text_coord_type type)
{
    this->textType = type;
}

void text_overlay::setTextSize(float size)
{
    this->textSize = size;
}

void text_overlay::begin()
{
    vkMapMemory(this->hDevice->device, this->vertexBuffer.memory, 0,
                GUI_VERTEX_BUFFER_SIZE, 0, (void**)&this->mappedVertices);
    vkMapMemory(this->hDevice->device, this->indexBuffer.memory, 0,
                GUI_INDEX_BUFFER_SIZE, 0, (void**)&this->mappedIndices);

    this->quadCount = 0;
    this->zOrder = Z_ORDER_GUI_DEFAULT;
}

void text_overlay::draw(view<const char> stringView, vec2<float> position)
{
    const uint32_t firstChar = STB_SOMEFONT_FIRST_CHAR;
    const float width = this->textSize / float(this->hDevice->extent.width);
    const float height = this->textSize / float(this->hDevice->extent.height);

    float x = 0.0f, y = 0.0f;//NOTE(arle): inspect asm on these switches
    switch (this->textType){
        case text_coord_type::absolute: {
            x = (float(position.x) / float(this->hDevice->extent.width) * 2.0f) - 1.0f;
            y = (float(position.y) / float(this->hDevice->extent.height) * 2.0f) - 1.0f;
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

        this->mappedVertices->position.x = x + charData->x0f * width;
        this->mappedVertices->position.y = y + charData->y0f * height;
        this->mappedVertices->position.z = this->zOrder;
        this->mappedVertices->texCoord = vec2(charData->s0, charData->t0);
        this->mappedVertices->colour = this->textTint;
        this->mappedVertices++;

        this->mappedVertices->position.x = x + charData->x1f * width;
        this->mappedVertices->position.y = y + charData->y0f * height;
        this->mappedVertices->position.z = this->zOrder;
        this->mappedVertices->texCoord = vec2(charData->s1, charData->t0);
        this->mappedVertices->colour = this->textTint;
        this->mappedVertices++;

        this->mappedVertices->position.x = x + charData->x1f * width;
        this->mappedVertices->position.y = y + charData->y1f * height;
        this->mappedVertices->position.z = this->zOrder;
        this->mappedVertices->texCoord = vec2(charData->s1, charData->t1);
        this->mappedVertices->colour = this->textTint;
        this->mappedVertices++;

        this->mappedVertices->position.x = x + charData->x0f * width;
        this->mappedVertices->position.y = y + charData->y1f * height;
        this->mappedVertices->position.z = this->zOrder;
        this->mappedVertices->texCoord = vec2(charData->s0, charData->t1);
        this->mappedVertices->colour = this->textTint;
        this->mappedVertices++;

        const auto offset = this->quadCount * QUAD_VERTEX_COUNT;
        this->mappedIndices[0] = quad_index(offset);
        this->mappedIndices[1] = quad_index(offset + 1);
        this->mappedIndices[2] = quad_index(offset + 2);
        this->mappedIndices[3] = quad_index(offset + 2);
        this->mappedIndices[4] = quad_index(offset + 3);
        this->mappedIndices[5] = quad_index(offset);
        this->mappedIndices += QUAD_INDEX_COUNT;

        this->quadCount++;
        this->zOrder -= Z_ORDER_GUI_INCREMENT;
        x += charData->advance * width;
    }
}

void text_overlay::end()
{
    vkUnmapMemory(this->hDevice->device, this->vertexBuffer.memory);
    vkUnmapMemory(this->hDevice->device, this->indexBuffer.memory);
    this->mappedVertices = nullptr;
    this->mappedIndices = nullptr;
}

void text_overlay::updateCmdBuffers(const VkFramebuffer *pFramebuffers)
{
    if(this->quadCount == 0)
        return;

    VkClearValue clearValues[2];
    clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
    clearValues[1].depthStencil = {1.0f, 0};

    auto cmdBeginInfo = vkInits::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);
    auto renderBeginInfo = vkInits::renderPassBeginInfo(this->renderPass, this->hDevice->extent);
    renderBeginInfo.clearValueCount = 1;
    renderBeginInfo.pClearValues = clearValues;
    renderBeginInfo.clearValueCount = uint32_t(arraysize(clearValues));

    for (size_t i = 0; i < this->imageCount; i++){
        const auto cmdBuffer = this->cmdBuffers[i];
        vkBeginCommandBuffer(cmdBuffer, &cmdBeginInfo);

        renderBeginInfo.framebuffer = pFramebuffers[i];
        vkCmdBeginRenderPass(cmdBuffer, &renderBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, this->pipeline);
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, this->pipelineLayout,
                                0, 1, &this->descriptorSets[i], 0, nullptr);

        const VkDeviceSize vertexOffset = 0;
        vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &this->vertexBuffer.data, &vertexOffset);

        const VkDeviceSize indexOffset = 0;
        vkCmdBindIndexBuffer(cmdBuffer, this->indexBuffer.data, indexOffset, VK_INDEX_TYPE_UINT32);

        const auto indexCount = uint32_t(this->quadCount * QUAD_INDEX_COUNT);
        vkCmdDrawIndexed(cmdBuffer, indexCount, 1, 0, 0, 0);

        vkCmdEndRenderPass(cmdBuffer);
        vkEndCommandBuffer(cmdBuffer);
    }
}

void text_overlay::prepareRenderpass()
{
    auto colourAttachment = vkInits::attachmentDescription(this->hDevice->surfaceFormat.format);
    colourAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colourAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
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

void text_overlay::prepareFontBuffer(const void *src, VkExtent2D bitmapExtent)
{
    vkTools::CreateImageBuffer(this->hDevice->device,
                               this->hDevice->gpu,
                               VK_FORMAT_R8_UNORM,
                               bitmapExtent,
                               VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                               &this->fontBuffer);

    const VkDeviceSize size = bitmapExtent.width * bitmapExtent.height;

    buffer_t transfer;
    vkTools::CreateBuffer(this->hDevice->device,
                          this->hDevice->gpu,
                          size,
                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                          VISIBLE_BUFFER_FLAGS,
                          &transfer);

    vkTools::FillBuffer(this->hDevice->device, &transfer, src, size);

    VkCommandBuffer imageCmds[] = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};

    auto cmdBufferAllocInfo = vkInits::commandBufferAllocateInfo(this->cmdPool, arraysize(imageCmds));
    vkAllocateCommandBuffers(this->hDevice->device, &cmdBufferAllocInfo, imageCmds);

    auto cmdBufferBeginInfo = vkInits::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    vkBeginCommandBuffer(imageCmds[0], &cmdBufferBeginInfo);
    auto imageBarrier = vkInits::imageMemoryBarrier(this->fontBuffer.image,
                                                    VK_IMAGE_LAYOUT_UNDEFINED,
                                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                    VkAccessFlags(0),
                                                    VK_ACCESS_TRANSFER_WRITE_BIT);
    vkCmdPipelineBarrier(imageCmds[0], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &imageBarrier);
    vkEndCommandBuffer(imageCmds[0]);

    vkTools::CmdCopyBufferToImage(imageCmds[1],
                                  &this->fontBuffer,
                                  &transfer,
                                  bitmapExtent);

    vkBeginCommandBuffer(imageCmds[2], &cmdBufferBeginInfo);
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(imageCmds[2], VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                         nullptr, 0, nullptr, 1, &imageBarrier);
    vkEndCommandBuffer(imageCmds[2]);

    auto queueSubmitInfo = vkInits::submitInfo(imageCmds, arraysize(imageCmds));
    vkQueueSubmit(this->hDevice->graphics.queue, 1, &queueSubmitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(this->hDevice->graphics.queue);

    vkFreeCommandBuffers(this->hDevice->device, this->cmdPool, uint32_t(arraysize(imageCmds)), imageCmds);

    transfer.destroy(this->hDevice->device);

    auto viewInfo = vkInits::imageViewCreateInfo();
    viewInfo.image = this->fontBuffer.image;
    viewInfo.format = VK_FORMAT_R8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vkCreateImageView(this->hDevice->device, &viewInfo, nullptr, &this->fontBuffer.view);
}

void text_overlay::prepareDescriptorSets(mv_allocator *allocator)
{
    auto layouts = allocator->allocTransient<VkDescriptorSetLayout>(this->imageCount);
    for(size_t i = 0; i < this->imageCount; i++)
        layouts[i] = this->setLayout;

    auto descriptorSetAllocInfo = vkInits::descriptorSetAllocateInfo(this->descriptorPool);
    descriptorSetAllocInfo.descriptorSetCount = uint32_t(this->imageCount);
    descriptorSetAllocInfo.pSetLayouts = layouts;
    vkAllocateDescriptorSets(this->hDevice->device, &descriptorSetAllocInfo, this->descriptorSets);

    for(size_t i = 0; i < this->imageCount; i++){
        VkDescriptorImageInfo samplerDescInfo{};
        samplerDescInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        samplerDescInfo.imageView = this->fontBuffer.view;
        samplerDescInfo.sampler = this->sampler;

        auto samplerImageWrite = vkInits::writeDescriptorSet(0);
        samplerImageWrite.dstSet = this->descriptorSets[i];
        samplerImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerImageWrite.descriptorCount = 1;
        samplerImageWrite.pImageInfo = &samplerDescInfo;
        vkUpdateDescriptorSets(this->hDevice->device, 1, &samplerImageWrite, 0, nullptr);
    }
}

void text_overlay::preparePipeline()
{
    const VkPipelineShaderStageCreateInfo shaderStages[] = {
        vkInits::shaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT, this->vertShaderModule),
        vkInits::shaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, this->fragShaderModule),
    };

    auto bindingDescription = vkInits::vertexBindingDescription(sizeof(quad_vertex));

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = uint32_t(arraysize(s_OverlayAttributes));
    vertexInputInfo.pVertexAttributeDescriptions = s_OverlayAttributes;

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

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pSetLayouts = &this->setLayout;
    pipelineLayoutInfo.setLayoutCount = 1;
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

void text_overlay::prepareRenderBuffers()
{
    vkTools::CreateBuffer(this->hDevice->device,
                          this->hDevice->gpu,
                          GUI_VERTEX_BUFFER_SIZE,
                          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                          VISIBLE_BUFFER_FLAGS,
                          &this->vertexBuffer);

    vkTools::CreateBuffer(this->hDevice->device,
                          this->hDevice->gpu,
                          GUI_INDEX_BUFFER_SIZE,
                          VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                          VISIBLE_BUFFER_FLAGS,
                          &this->indexBuffer);
}
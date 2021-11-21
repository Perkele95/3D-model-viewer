#include "../vendor/stb/stb_font_courier_40_latin1.inl"
#include "vk_text_overlay.hpp"
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

constexpr VkVertexInputAttributeDescription GuiAttributeDescriptions[] = {
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
    bind(pInfo->device, pInfo->cmdPool, pInfo->renderPass);

    prepareCommandbuffers();

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
    VkResult result = vkCreateSampler(this->device, &samplerInfo, nullptr, &this->sampler);

    uint8_t fontpixels[STB_SOMEFONT_BITMAP_HEIGHT][STB_SOMEFONT_BITMAP_WIDTH];
    STB_SOMEFONT_CREATE(s_Fontdata, fontpixels, STB_SOMEFONT_BITMAP_HEIGHT);

    prepareFontBuffer(fontpixels, {STB_SOMEFONT_BITMAP_WIDTH, STB_SOMEFONT_BITMAP_HEIGHT});

    const VkDescriptorPoolSize samplerImageSize = {
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(this->imageCount)
    };

    const VkDescriptorPoolSize poolSizes[] = {samplerImageSize};
    const uint32_t maxSets = poolSizes[0].descriptorCount;

    VkDescriptorPoolCreateInfo descPoolInfo{};
    descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descPoolInfo.poolSizeCount = static_cast<uint32_t>(arraysize(poolSizes));
    descPoolInfo.pPoolSizes = poolSizes;
    descPoolInfo.maxSets = maxSets;
    result = vkCreateDescriptorPool(this->device, &descPoolInfo, nullptr, &this->descriptorPool);

    const auto samplerBinding = vkInits::descriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                                    VK_SHADER_STAGE_FRAGMENT_BIT);

    const VkDescriptorSetLayoutBinding bindings[] = {samplerBinding};

    VkDescriptorSetLayoutCreateInfo setLayoutInfo{};
    setLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    setLayoutInfo.pBindings = bindings;
    setLayoutInfo.bindingCount = static_cast<uint32_t>(arraysize(bindings));
    result = vkCreateDescriptorSetLayout(this->device, &setLayoutInfo, nullptr, &this->setLayout);

    prepareDescriptorSets(pInfo->allocator);
    preparePipeline();
    prepareRenderBuffers();
}

void text_overlay::destroy()
{
    vkDestroyDescriptorPool(this->device, this->descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(this->device, this->setLayout, nullptr);

    this->vertexBuffer.destroy(this->device);
    this->indexBuffer.destroy(this->device);
    this->fontBuffer.destroy(this->device);

    vkDestroyPipeline(this->device, this->pipeline, nullptr);
    vkDestroyPipelineLayout(this->device, this->pipelineLayout, nullptr);

    vkDestroyShaderModule(this->device, this->vertShaderModule, nullptr);
    vkDestroyShaderModule(this->device, this->fragShaderModule, nullptr);
    vkDestroySampler(this->device, this->sampler, nullptr);
}

void text_overlay::onWindowResize(mv_allocator *allocator, const vulkan_device *vulkanDevice,
                                  VkCommandPool commandPool, VkRenderPass sharedRenderPass)
{
    bind(vulkanDevice, commandPool, sharedRenderPass);

    vkDestroyPipeline(this->device, this->pipeline, nullptr);
    vkDestroyPipelineLayout(this->device, this->pipelineLayout, nullptr);
    vkResetDescriptorPool(this->device, this->descriptorPool, VkFlags(0));

    prepareCommandbuffers();
    prepareDescriptorSets(allocator);
    preparePipeline();
}

void text_overlay::begin()
{
    vkMapMemory(this->device, this->vertexBuffer.memory, 0,
                GUI_VERTEX_BUFFER_SIZE, 0, (void**)&this->mappedVertices);
    vkMapMemory(this->device, this->indexBuffer.memory, 0,
                GUI_INDEX_BUFFER_SIZE, 0, (void**)&this->mappedIndices);

    this->quadCount = 0;
    this->zOrder = Z_ORDER_GUI_DEFAULT;
}

void text_overlay::draw(view<const char> stringView, vec2<float> position,
                        vec4<float> tint, text_align alignment, text_coord_type type,
                        float size)
{
    const uint32_t firstChar = STB_SOMEFONT_FIRST_CHAR;
    const float width = size / float(this->extent.width);
    const float height = size / float(this->extent.height);

    float x = 0.0f, y = 0.0f;//NOTE(arle): inspect asm on these switches
    switch (type){
        case text_coord_type::absolute: {
            x = (float(position.x) / float(this->extent.width) * 2.0f) - 1.0f;
            y = (float(position.y) / float(this->extent.height) * 2.0f) - 1.0f;
        } break;
        case text_coord_type::relative: {
            x = (float(position.x) / 50.0f) - 1.0f;
            y = (float(position.y) / 50.0f) - 1.0f;
        } break;
    };

    float textWidth = 0.0f;
    for (size_t i = 0; i < stringView.count - 1; i++)
        textWidth += s_Fontdata[uint32_t(stringView[i]) - firstChar].advance * width;

    switch(alignment){
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
        this->mappedVertices->colour = tint;
        this->mappedVertices++;

        this->mappedVertices->position.x = x + charData->x1f * width;
        this->mappedVertices->position.y = y + charData->y0f * height;
        this->mappedVertices->position.z = this->zOrder;
        this->mappedVertices->texCoord = vec2(charData->s1, charData->t0);
        this->mappedVertices->colour = tint;
        this->mappedVertices++;

        this->mappedVertices->position.x = x + charData->x1f * width;
        this->mappedVertices->position.y = y + charData->y1f * height;
        this->mappedVertices->position.z = this->zOrder;
        this->mappedVertices->texCoord = vec2(charData->s1, charData->t1);
        this->mappedVertices->colour = tint;
        this->mappedVertices++;

        this->mappedVertices->position.x = x + charData->x0f * width;
        this->mappedVertices->position.y = y + charData->y1f * height;
        this->mappedVertices->position.z = this->zOrder;
        this->mappedVertices->texCoord = vec2(charData->s0, charData->t1);
        this->mappedVertices->colour = tint;
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
    vkUnmapMemory(this->device, this->vertexBuffer.memory);
    vkUnmapMemory(this->device, this->indexBuffer.memory);
    this->mappedVertices = nullptr;
    this->mappedIndices = nullptr;
}

void text_overlay::updateCmdBuffers(const VkClearValue (*clearValues)[2], const VkFramebuffer *pFramebuffers)
{
    if(this->quadCount == 0)
        return;

    auto cmdBeginInfo = vkInits::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);
    auto renderBeginInfo = vkInits::renderPassBeginInfo(this->renderPass, this->extent);
    renderBeginInfo.framebuffer = VK_NULL_HANDLE;
    renderBeginInfo.clearValueCount = 1;
    renderBeginInfo.pClearValues = *clearValues;
    renderBeginInfo.clearValueCount = uint32_t(arraysize(*clearValues));

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

VkSubmitInfo text_overlay::getSubmitData()
{
    auto info = vkInits::submitInfo(this->cmdBuffers, this->imageCount);
    return info;
}

void text_overlay::bind(const vulkan_device *vulkanDevice, VkCommandPool commandPool,
                        VkRenderPass sharedRenderPass)
{
    this->device = vulkanDevice->device;
    this->gpu = vulkanDevice->gpu;
    this->graphicsQueue = vulkanDevice->graphics.queue;
    this->cmdPool = commandPool;
    this->sampleCount = vulkanDevice->getSampleCount();
    this->extent = vulkanDevice->getExtent();
    this->renderPass = sharedRenderPass;
    this->framebuffers = VK_NULL_HANDLE;
}

void text_overlay::prepareCommandbuffers()
{
    auto info = vkInits::commandBufferAllocateInfo(this->cmdPool, this->imageCount);
    vkAllocateCommandBuffers(this->device, &info, this->cmdBuffers);
}

void text_overlay::prepareFontBuffer(const void *src, VkExtent2D bitmapExtent)
{
    auto imageInfo = vkInits::imageCreateInfo();
    imageInfo.extent = {bitmapExtent.width, bitmapExtent.height, 1};
    imageInfo.format = VK_FORMAT_R8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    vkCreateImage(this->device, &imageInfo, nullptr, &this->fontBuffer.image);

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(this->device, this->fontBuffer.image, &memReqs);

    auto allocInfo = GetMemoryAllocInfo(this->gpu, memReqs, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(this->device, &allocInfo, nullptr, &this->fontBuffer.memory);
    vkBindImageMemory(this->device, this->fontBuffer.image, this->fontBuffer.memory, 0);

    buffer_t transfer;
    const VkDeviceSize size = bitmapExtent.width * bitmapExtent.height;
    auto bufferInfo = vkInits::bufferCreateInfo(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    vkCreateBuffer(this->device, &bufferInfo, nullptr, &transfer.data);

    vkGetBufferMemoryRequirements(this->device, transfer.data, &memReqs);

    allocInfo = GetMemoryAllocInfo(this->gpu, memReqs, VISIBLE_BUFFER_FLAGS);
    vkAllocateMemory(this->device, &allocInfo, nullptr, &transfer.memory);
    vkBindBufferMemory(this->device, transfer.data, transfer.memory, 0);

    void *dst = nullptr;
    vkMapMemory(this->device, transfer.memory, 0, size, 0, &dst);
    memcpy(dst, src, size);
    vkUnmapMemory(this->device, transfer.memory);

    VkCommandBuffer imageCmds[] = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};

    auto cmdBufferAllocInfo = vkInits::commandBufferAllocateInfo(this->cmdPool, arraysize(imageCmds));
    vkAllocateCommandBuffers(this->device, &cmdBufferAllocInfo, imageCmds);

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

    vkBeginCommandBuffer(imageCmds[1], &cmdBufferBeginInfo);
    auto copyRegion = vkInits::bufferImageCopy(bitmapExtent);
    vkCmdCopyBufferToImage(imageCmds[1], transfer.data, this->fontBuffer.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
    vkEndCommandBuffer(imageCmds[1]);

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
    vkQueueSubmit(this->graphicsQueue, 1, &queueSubmitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(this->graphicsQueue);
    vkFreeCommandBuffers(this->device, this->cmdPool, uint32_t(arraysize(imageCmds)), imageCmds);

    transfer.destroy(this->device);

    auto viewInfo = vkInits::imageViewCreateInfo();
    viewInfo.image = this->fontBuffer.image;
    viewInfo.format = VK_FORMAT_R8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vkCreateImageView(this->device, &viewInfo, nullptr, &this->fontBuffer.view);
}

void text_overlay::prepareDescriptorSets(mv_allocator *allocator)
{
    auto layouts = allocator->allocTransient<VkDescriptorSetLayout>(this->imageCount);
    for(size_t i = 0; i < this->imageCount; i++)
        layouts[i] = this->setLayout;

    VkDescriptorSetAllocateInfo descriptorSetAllocInfo{};
    descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocInfo.descriptorPool = this->descriptorPool;
    descriptorSetAllocInfo.descriptorSetCount = static_cast<uint32_t>(this->imageCount);
    descriptorSetAllocInfo.pSetLayouts = layouts;
    vkAllocateDescriptorSets(this->device, &descriptorSetAllocInfo, this->descriptorSets);

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
        vkUpdateDescriptorSets(this->device, 1, &samplerImageWrite, 0, nullptr);
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
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(arraysize(GuiAttributeDescriptions));
    vertexInputInfo.pVertexAttributeDescriptions = GuiAttributeDescriptions;

    auto inputAssembly = vkInits::inputAssemblyInfo();
    auto viewport = vkInits::viewportInfo(this->extent);
    auto scissor = vkInits::scissorInfo(this->extent);

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    auto rasterizer = vkInits::rasterizationStateInfo(VK_FRONT_FACE_CLOCKWISE);
    auto multisampling = vkInits::pipelineMultisampleStateCreateInfo(this->sampleCount);

    auto depthStencil = vkInits::depthStencilStateInfo();
    auto colorBlendAttachment = vkInits::pipelineColorBlendAttachmentState();
    auto colourBlend = vkInits::pipelineColorBlendStateCreateInfo();
    colourBlend.attachmentCount = 1;
    colourBlend.pAttachments = &colorBlendAttachment;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pSetLayouts = &this->setLayout;
    pipelineLayoutInfo.setLayoutCount = 1;
    vkCreatePipelineLayout(this->device, &pipelineLayoutInfo, nullptr, &this->pipelineLayout);

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<uint32_t>(arraysize(shaderStages));
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
    vkCreateGraphicsPipelines(this->device, VK_NULL_HANDLE, 1,
                              &pipelineInfo, nullptr, &this->pipeline);
}

void text_overlay::prepareRenderBuffers()
{
    auto bufferInfo = vkInits::bufferCreateInfo(GUI_VERTEX_BUFFER_SIZE, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    vkCreateBuffer(this->device, &bufferInfo, nullptr, &this->vertexBuffer.data);

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(this->device, this->vertexBuffer.data, &memReqs);

    auto allocInfo = GetMemoryAllocInfo(this->gpu, memReqs, VISIBLE_BUFFER_FLAGS);
    vkAllocateMemory(this->device, &allocInfo, nullptr, &this->vertexBuffer.memory);
    vkBindBufferMemory(this->device, this->vertexBuffer.data, this->vertexBuffer.memory, 0);

    bufferInfo.size = GUI_INDEX_BUFFER_SIZE;
    bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    vkCreateBuffer(this->device, &bufferInfo, nullptr, &this->indexBuffer.data);

    vkGetBufferMemoryRequirements(this->device, this->indexBuffer.data, &memReqs);

    allocInfo = GetMemoryAllocInfo(this->gpu, memReqs, VISIBLE_BUFFER_FLAGS);
    vkAllocateMemory(this->device, &allocInfo, nullptr, &this->indexBuffer.memory);
    vkBindBufferMemory(this->device, this->indexBuffer.data, this->indexBuffer.memory, 0);
}
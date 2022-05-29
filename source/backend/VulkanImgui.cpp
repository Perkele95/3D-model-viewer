#include "VulkanImgui.hpp"
#include "../../vendor/stb/stb_font_courier_40_latin1.inl"

// for sprintf
#include <cstdio>

enum GUI_ITEM_CONSTANTS : int32_t
{
    GUI_ITEM_NULL = -1,
    GUI_ITEM_UNAVAILABLE = -2
};

enum GUI_QUAD_CONSTANTS : uint32_t
{
    QUAD_VERTEX_COUNT = 4,
    QUAD_INDEX_COUNT = 6,
    GUI_MAX_QUADS = KiloBytes(2)
};

constexpr float Z_ORDER_GUI_DEFAULT = 0.999f;
constexpr float Z_ORDER_GUI_INCREMENT = 0.001f;

constexpr static VkVertexInputAttributeDescription s_GuiAttributes[] = {
    VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VulkanImgui::Vertex, position)},
    VkVertexInputAttributeDescription{1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(VulkanImgui::Vertex, texCoord)},
    VkVertexInputAttributeDescription{2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(VulkanImgui::Vertex, colour)}
};

static stb_fontchar s_Fontdata[STB_SOMEFONT_NUM_CHARS];

void VulkanImgui::init(const CreateInfo &info, VkQueue queue)
{
    quadCount = 0;
    zOrder = Z_ORDER_GUI_DEFAULT;

    auto sb = StringBuilder();

    sb << SHADERS_PATH << view("gui_vert.spv");
    vertexShader.load(device->device, sb.c_str());
    sb.flush() << SHADERS_PATH << view("gui_frag.spv");
    fragmentShader.load(device->device, sb.c_str());

    sb.destroy();

    // Command buffers
    {
        auto cmdInfo = vkInits::commandBufferAllocateInfo(device->commandPool, MAX_IMAGES_IN_FLIGHT);
        vkAllocateCommandBuffers(device->device, &cmdInfo, commandBuffers);
    }

    // Glyph Atlas
    {
        auto fontPixels = new uint8_t[STB_SOMEFONT_BITMAP_HEIGHT][STB_SOMEFONT_BITMAP_WIDTH];
        STB_SOMEFONT_CREATE(s_Fontdata, fontPixels, STB_SOMEFONT_BITMAP_HEIGHT);

        fontPixels[0][0] = 255;// White pixel for gui elements

        ImageFile file{};
        file.pixels = fontPixels[0];
        file.extent = {STB_SOMEFONT_BITMAP_WIDTH, STB_SOMEFONT_BITMAP_HEIGHT};
        file.format = VK_FORMAT_R8_UNORM;
        file.bytesPerPixel = 1;
        glyphAtlas.create(device, queue, file, false);

        delete[] fontPixels;
    }

    // Descriptors
    {
        const VkDescriptorPoolSize poolSizes[] = {
            vkInits::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_IMAGES_IN_FLIGHT)
        };

        const auto poolInfo = vkInits::descriptorPoolCreateInfo(poolSizes, MAX_IMAGES_IN_FLIGHT);
        vkCreateDescriptorPool(device->device, &poolInfo, nullptr, &descriptorPool);

        const VkDescriptorSetLayoutBinding bindings[] = {
            vkInits::descriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
        };

        const auto setLayoutInfo = vkInits::descriptorSetLayoutCreateInfo(bindings);
        vkCreateDescriptorSetLayout(device->device, &setLayoutInfo, nullptr, &setLayout);

        // Sets

        VkDescriptorSetLayout layouts[MAX_IMAGES_IN_FLIGHT] = {};
        arrayfill(layouts, setLayout);

        auto allocInfo = vkInits::descriptorSetAllocateInfo(descriptorPool, layouts);
        vkAllocateDescriptorSets(device->device, &allocInfo, descriptorSets);

        for(size_t i = 0; i < MAX_IMAGES_IN_FLIGHT; i++)
        {
            const VkWriteDescriptorSet writes[] = {
                vkInits::writeDescriptorSet(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                            descriptorSets[i], &glyphAtlas.descriptor)
            };
            vkUpdateDescriptorSets(device->device, uint32_t(arraysize(writes)), writes, 0, nullptr);
        }
    }

    // Render pass
    {
        auto colourAttachment = vkInits::attachmentDescription(info.surfaceFormat.format);
        colourAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colourAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        colourAttachment.samples = info.sampleCount;

        auto depthAttachment = vkInits::attachmentDescription(info.depthFormat);
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.samples = info.sampleCount;

        auto colourResolve = vkInits::attachmentDescription(info.surfaceFormat.format);
        colourResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        colourResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colourResolve.samples = VK_SAMPLE_COUNT_1_BIT;

        const VkAttachmentDescription attachments[] = {colourAttachment, depthAttachment, colourResolve};

        auto colourAttachmentRef = vkInits::attachmentReference(0);
        colourAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        auto depthAttachmentRef = vkInits::attachmentReference(1);
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        auto colourResolveRef = vkInits::attachmentReference(2);
        colourResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colourAttachmentRef;
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

        vkCreateRenderPass(device->device, &renderPassInfo, nullptr, &renderPass);
    }

    preparePipeline(info.sampleCount);

    // Render buffers
    device->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                         MEM_FLAG_HOST_VISIBLE,
                         GUI_MAX_QUADS * QUAD_VERTEX_COUNT * sizeof(Vertex),
                         vertexBuffer);

    device->createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                         MEM_FLAG_HOST_VISIBLE,
                         GUI_MAX_QUADS * QUAD_INDEX_COUNT * sizeof(uint32_t),
                         indexBuffer);
}

void VulkanImgui::destroy()
{
    vkDestroyDescriptorPool(device->device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device->device, setLayout, nullptr);

    vertexBuffer.destroy(device->device);
    indexBuffer.destroy(device->device);
    glyphAtlas.destroy(device->device);

    vkDestroyRenderPass(device->device, renderPass, nullptr);
    vkDestroyPipeline(device->device, pipeline, nullptr);
    vkDestroyPipelineLayout(device->device, pipelineLayout, nullptr);

    vertexShader.destroy(device->device);
    fragmentShader.destroy(device->device);
}

void VulkanImgui::onWindowResize(VkSampleCountFlagBits sampleCount)
{
    vkDestroyPipeline(device->device, pipeline, nullptr);
    vkDestroyPipelineLayout(device->device, pipelineLayout, nullptr);

    preparePipeline(sampleCount);
}

void VulkanImgui::begin()
{
    vertexBuffer.map(device->device);
    mappedVertices = static_cast<Vertex*>(vertexBuffer.mapped);
    indexBuffer.map(device->device);
    mappedIndices = static_cast<uint32_t*>(indexBuffer.mapped);

    quadCount = 0;
    zOrder = Z_ORDER_GUI_DEFAULT;
    state.itemCounter = 0;
    state.hotItem = GUI_ITEM_NULL;
}

void VulkanImgui::end()
{
    vertexBuffer.unmap(device->device);
    indexBuffer.unmap(device->device);
    mappedVertices = nullptr;
    mappedIndices = nullptr;
}

void VulkanImgui::text(view<const char> stringView, vec2<float> position)
{
    constexpr uint32_t firstChar = STB_SOMEFONT_FIRST_CHAR;
    const float width = settings.size / float(extent.width);
    const float height = settings.size / float(extent.height);

    float x = (float(position.x) / 50.0f) - 1.0f;
    float y = (float(position.y) / 50.0f) - 1.0f;

    float totalWidth = 0.0f;
    for (size_t i = 0; i < stringView.count - 1; i++)
        totalWidth += s_Fontdata[uint32_t(stringView[i]) - firstChar].advance * width;

    switch(settings.alignment)
    {
        case Alignment::Right: x -= totalWidth; break;
        case Alignment::Centre: x -= totalWidth / 2.0f; break;
        default: break;
    };

    for (size_t i = 0; i < stringView.count - 1; i++)
    {
        auto charData = &s_Fontdata[uint32_t(stringView[i]) - firstChar];

        mappedVertices->position.x = x + charData->x0f * width;
        mappedVertices->position.y = y + charData->y0f * height;
        mappedVertices->position.z = zOrder;
        mappedVertices->texCoord = vec2(charData->s0, charData->t0);
        mappedVertices->colour = settings.tint;
        mappedVertices++;

        mappedVertices->position.x = x + charData->x1f * width;
        mappedVertices->position.y = y + charData->y0f * height;
        mappedVertices->position.z = zOrder;
        mappedVertices->texCoord = vec2(charData->s1, charData->t0);
        mappedVertices->colour = settings.tint;
        mappedVertices++;

        mappedVertices->position.x = x + charData->x1f * width;
        mappedVertices->position.y = y + charData->y1f * height;
        mappedVertices->position.z = zOrder;
        mappedVertices->texCoord = vec2(charData->s1, charData->t1);
        mappedVertices->colour = settings.tint;
        mappedVertices++;

        mappedVertices->position.x = x + charData->x0f * width;
        mappedVertices->position.y = y + charData->y1f * height;
        mappedVertices->position.z = zOrder;
        mappedVertices->texCoord = vec2(charData->s0, charData->t1);
        mappedVertices->colour = settings.tint;
        mappedVertices++;

        const auto offset = quadCount * QUAD_VERTEX_COUNT;
        mappedIndices[0] = offset;
        mappedIndices[1] = offset + 1;
        mappedIndices[2] = offset + 2;
        mappedIndices[3] = offset + 2;
        mappedIndices[4] = offset + 3;
        mappedIndices[5] = offset;
        mappedIndices += QUAD_INDEX_COUNT;

        quadCount++;
        zOrder -= Z_ORDER_GUI_INCREMENT;
        x += charData->advance * width;
    }
}

void VulkanImgui::text(const char *cstring, vec2<float> position)
{
    constexpr uint32_t firstChar = STB_SOMEFONT_FIRST_CHAR;
    const float width = settings.size / float(extent.width);
    const float height = settings.size / float(extent.height);

    float x = (float(position.x) / 50.0f) - 1.0f;
    float y = (float(position.y) / 50.0f) - 1.0f;

    float totalWidth = 0.0f;
    for(auto c = cstring; *c; c++)
        totalWidth += s_Fontdata[uint32_t(*c) - firstChar].advance * width;

    switch(settings.alignment)
    {
        case Alignment::Right: x -= totalWidth; break;
        case Alignment::Centre: x -= totalWidth / 2.0f; break;
        default: break;
    };

    for(auto c = cstring; *c; c++)
    {
        auto charData = &s_Fontdata[uint32_t(*c) - firstChar];

        mappedVertices->position.x = x + charData->x0f * width;
        mappedVertices->position.y = y + charData->y0f * height;
        mappedVertices->position.z = zOrder;
        mappedVertices->texCoord = vec2(charData->s0, charData->t0);
        mappedVertices->colour = settings.tint;
        mappedVertices++;

        mappedVertices->position.x = x + charData->x1f * width;
        mappedVertices->position.y = y + charData->y0f * height;
        mappedVertices->position.z = zOrder;
        mappedVertices->texCoord = vec2(charData->s1, charData->t0);
        mappedVertices->colour = settings.tint;
        mappedVertices++;

        mappedVertices->position.x = x + charData->x1f * width;
        mappedVertices->position.y = y + charData->y1f * height;
        mappedVertices->position.z = zOrder;
        mappedVertices->texCoord = vec2(charData->s1, charData->t1);
        mappedVertices->colour = settings.tint;
        mappedVertices++;

        mappedVertices->position.x = x + charData->x0f * width;
        mappedVertices->position.y = y + charData->y1f * height;
        mappedVertices->position.z = zOrder;
        mappedVertices->texCoord = vec2(charData->s0, charData->t1);
        mappedVertices->colour = settings.tint;
        mappedVertices++;

        const auto offset = quadCount * QUAD_VERTEX_COUNT;
        mappedIndices[0] = offset;
        mappedIndices[1] = offset + 1;
        mappedIndices[2] = offset + 2;
        mappedIndices[3] = offset + 2;
        mappedIndices[4] = offset + 3;
        mappedIndices[5] = offset;
        mappedIndices += QUAD_INDEX_COUNT;

        quadCount++;
        zOrder -= Z_ORDER_GUI_INCREMENT;
        x += charData->advance * width;
    }
}

void VulkanImgui::textInt(int32_t value, vec2<float> position)
{
    static char buffer[256];
    sprintf_s(buffer, "%d", value);
    text(buffer, position);
}

void VulkanImgui::textFloat(float value, vec2<float> position)
{
    static char buffer[256];
    sprintf_s(buffer, "%f", value);
    text(buffer, position);
}

void VulkanImgui::box(vec2<float> topLeft, vec2<float> bottomRight)
{
    const auto tl = (topLeft / 50.0f) - vec2(1.0f);
    const auto br = (bottomRight / 50.0f) - vec2(1.0f);

    mappedVertices->position = vec3(tl, zOrder);
    mappedVertices->texCoord = vec2(0.0f);
    mappedVertices->colour = settings.tint;
    mappedVertices++;

    mappedVertices->position = vec3(br.x, tl.y, zOrder);
    mappedVertices->texCoord = vec2(0.0f);
    mappedVertices->colour = settings.tint;
    mappedVertices++;

    mappedVertices->position = vec3(br, zOrder);
    mappedVertices->texCoord = vec2(0.0f);
    mappedVertices->colour = settings.tint;
    mappedVertices++;

    mappedVertices->position = vec3(tl.x, br.y, zOrder);
    mappedVertices->texCoord = vec2(0.0f);
    mappedVertices->colour = settings.tint;
    mappedVertices++;

    const auto offset = quadCount * QUAD_VERTEX_COUNT;
    mappedIndices[0] = offset;
    mappedIndices[1] = offset + 1;
    mappedIndices[2] = offset + 2;
    mappedIndices[3] = offset + 2;
    mappedIndices[4] = offset + 3;
    mappedIndices[5] = offset;
    mappedIndices += QUAD_INDEX_COUNT;

    quadCount++;
    zOrder -= Z_ORDER_GUI_INCREMENT;
}

bool VulkanImgui::button(vec2<float> topLeft, vec2<float> bottomRight)
{
    auto id = createItem();
    auto result = false;
    auto tint = settings.tint;

    if(state.activeItem == GUI_ITEM_NULL && state.hotItem == GUI_ITEM_NULL)
    {
        if(hitCheck(topLeft, bottomRight))
        {
            if(buttonPressed)
            {
                state.activeItem = id;
                // draw pressed button
                settings.tint.x *= 0.75f;
                settings.tint.y *= 0.75f;
                settings.tint.z *= 0.75f;
            }
            else
            {
                state.hotItem = id;
                // draw hovered button
                settings.tint.x *= 0.9f;
                settings.tint.y *= 0.9f;
                settings.tint.z *= 0.9f;
            }
        }
    }
    else if(state.activeItem == id  && !buttonPressed)
    {
        if(hitCheck(topLeft, bottomRight))
        {
            result = true;
        }

        state.activeItem = GUI_ITEM_NULL;
        // draw pressed button
        settings.tint.x *= 0.75f;
        settings.tint.y *= 0.75f;
        settings.tint.z *= 0.75f;
    }

    box(topLeft, bottomRight);
    settings.tint = tint;

    return false;
}

void VulkanImgui::recordFrame(size_t currentFrame, VkFramebuffer framebuffer)
{
    if(quadCount == 0)
        return;

    VkClearValue clearValues[2];
    clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
    clearValues[1].depthStencil = {1.0f, 0};

    auto cmdBeginInfo = vkInits::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);
    auto renderBeginInfo = vkInits::renderPassBeginInfo(renderPass, extent);
    renderBeginInfo.clearValueCount = 1;
    renderBeginInfo.pClearValues = clearValues;
    renderBeginInfo.clearValueCount = uint32_t(arraysize(clearValues));
    renderBeginInfo.framebuffer = framebuffer;

    const auto command = commandBuffers[currentFrame];
    vkBeginCommandBuffer(command, &cmdBeginInfo);

    vkCmdBeginRenderPass(command, &renderBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
                            0, 1, &descriptorSets[currentFrame], 0, nullptr);

    const VkDeviceSize vertexOffset = 0;
    vkCmdBindVertexBuffers(command, 0, 1, &vertexBuffer.data, &vertexOffset);

    const VkDeviceSize indexOffset = 0;
    vkCmdBindIndexBuffer(command, indexBuffer.data, indexOffset, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(command, quadCount * QUAD_INDEX_COUNT, 1, 0, 0, 0);

    vkCmdEndRenderPass(command);
    vkEndCommandBuffer(command);
}

void VulkanImgui::preparePipeline(VkSampleCountFlagBits sampleCount)
{
    const VkPipelineShaderStageCreateInfo shaderStages[] = {
        vertexShader.shaderStage(), fragmentShader.shaderStage()
    };

    auto bindingDescription = vkInits::vertexBindingDescription(sizeof(Vertex));

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = uint32_t(arraysize(s_GuiAttributes));
    vertexInputInfo.pVertexAttributeDescriptions = s_GuiAttributes;

    auto inputAssembly = vkInits::inputAssemblyInfo();
    auto viewport = vkInits::viewportInfo(extent);
    auto scissor = vkInits::scissorInfo(extent);

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    auto rasterizer = vkInits::rasterizationStateInfo(VK_FRONT_FACE_CLOCKWISE);
    auto multisampling = vkInits::pipelineMultisampleStateCreateInfo(sampleCount);

    auto depthStencil = vkInits::depthStencilStateInfo();
    auto colorBlendAttachment = vkInits::pipelineColorBlendAttachmentState();
    colorBlendAttachment.blendEnable = VK_TRUE;

    auto colourBlend = vkInits::pipelineColorBlendStateCreateInfo();
    colourBlend.attachmentCount = 1;
    colourBlend.pAttachments = &colorBlendAttachment;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pSetLayouts = &setLayout;
    pipelineLayoutInfo.setLayoutCount = 1;
    vkCreatePipelineLayout(device->device, &pipelineLayoutInfo, nullptr, &pipelineLayout);

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
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    vkCreateGraphicsPipelines(device->device, VK_NULL_HANDLE, 1,
                              &pipelineInfo, nullptr, &pipeline);
}

bool VulkanImgui::hitCheck(vec2<float> topLeft, vec2<float> bottomRight)
{
    const auto x = (float(mousePosition.x) / float(extent.width)) * 100.0f;
    const auto y = (float(mousePosition.y) / float(extent.height)) * 100.0f;
    return (x >= topLeft.x && x < bottomRight.x && y >= topLeft.y && y < bottomRight.y);
}
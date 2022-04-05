#include "model3D.hpp"

VkPushConstantRange PBRModel::pushConstant()
{
    VkPushConstantRange range{};
    range.offset = 0;
    range.size = sizeof(transform);
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    return range;
}

void PBRModel::destroy(VkDevice device)
{
    mesh.destroy(device);
    albedo.destroy(device);
    normal.destroy(device);
    roughness.destroy(device);
    metallic.destroy(device);
    ao.destroy(device);
}

void PBRModel::draw(VkCommandBuffer cmd, VkPipelineLayout layout)
{
    const auto [stageFlags, offset, size] = pushConstant();
    vkCmdPushConstants(cmd,
                       layout,
                       stageFlags,
                       offset,
                       size,
                       &transform);

    mesh.draw(cmd, layout);
}

void CubeMapModel::load(const VulkanDevice *device, VkQueue queue)
{
    constexpr size_t VERTICES_SIZE = VERTEX_COUNT * sizeof(CubeMapVertex);
    constexpr size_t INDICES_SIZE = INDEX_COUNT * sizeof(uint32_t);

    auto vertexInfo = vkInits::bufferCreateInfo(VERTICES_SIZE, USAGE_VERTEX_TRANSFER_SRC);
    auto vertexTransfer = VulkanBuffer();
    vertexTransfer.create(device, &vertexInfo, MEM_FLAG_HOST_VISIBLE);

    auto indexInfo = vkInits::bufferCreateInfo(INDICES_SIZE, USAGE_INDEX_TRANSFER_SRC);
    auto indexTransfer = VulkanBuffer();
    indexTransfer.create(device, &indexInfo, MEM_FLAG_HOST_VISIBLE);

    CubeMapVertex *vertices = nullptr;
    vkMapMemory(device->device, vertexTransfer.memory, 0, VERTICES_SIZE, 0, (void**)(&vertices));

    vertices[0] = {vec3(-SIZE, -SIZE, -SIZE)};
    vertices[1] = {vec3(SIZE, -SIZE, -SIZE)};
    vertices[2] = {vec3(SIZE, SIZE, -SIZE)};
    vertices[3] = {vec3(-SIZE, SIZE, -SIZE)};
    vertices[4] = {vec3(-SIZE, -SIZE, SIZE)};
    vertices[5] = {vec3(SIZE, -SIZE, SIZE)};
    vertices[6] = {vec3(SIZE, SIZE, SIZE)};
    vertices[7] = {vec3(-SIZE, SIZE, SIZE)};

    vkUnmapMemory(device->device, vertexTransfer.memory);

    uint32_t *indices = nullptr;
    vkMapMemory(device->device, indexTransfer.memory, 0, INDICES_SIZE, 0, (void**)(&indices));

    indices[0] = 0;
    indices[1] = 3;
    indices[2] = 2;
    indices[3] = 2;
    indices[4] = 1;
    indices[5] = 0;

    indices[6] = 4;
    indices[7] = 5;
    indices[8] = 6;
    indices[9] = 6;
    indices[10] = 7;
    indices[11] = 4;

    indices[12] = 3;
    indices[13] = 7;
    indices[14] = 6;
    indices[15] = 6;
    indices[16] = 2;
    indices[17] = 3;

    indices[18] = 4;
    indices[19] = 0;
    indices[20] = 1;
    indices[21] = 1;
    indices[22] = 5;
    indices[23] = 4;

    indices[24] = 4;
    indices[25] = 7;
    indices[26] = 3;
    indices[27] = 3;
    indices[28] = 0;
    indices[29] = 4;

    indices[30] = 1;
    indices[31] = 2;
    indices[32] = 6;
    indices[33] = 6;
    indices[34] = 5;
    indices[35] = 1;

    vkUnmapMemory(device->device, indexTransfer.memory);

    vertexInfo.usage = USAGE_VERTEX_TRANSFER_DST;
    m_vertices.create(device, &vertexInfo, MEM_FLAG_GPU_LOCAL);

    indexInfo.usage = USAGE_INDEX_TRANSFER_DST;
    m_indices.create(device, &indexInfo, MEM_FLAG_GPU_LOCAL);

    auto command = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    vertexTransfer.copy(command, &m_vertices);
    indexTransfer.copy(command, &m_indices);

    device->flushCommandBuffer(command, queue);

    vertexTransfer.destroy(device->device);
    indexTransfer.destroy(device->device);
}

void CubeMapModel::destroy(VkDevice device)
{
    m_vertices.destroy(device);
    m_indices.destroy(device);
    map.destroy(device);
}

void CubeMapModel::draw(VkCommandBuffer cmd)
{
    const VkDeviceSize vertexOffset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &m_vertices.data, &vertexOffset);

    const VkDeviceSize indexOffset = 0;
    vkCmdBindIndexBuffer(cmd, m_indices.data, indexOffset, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(cmd, INDEX_COUNT, 1, 0, 0, 0);
}
#include "lights.hpp"

lights::lights(const vulkan_device *device, view<buffer_t> buffers)
{
    constexpr float LIGHT_STR = 300.0f;

    auto data = light_data();
    data.positions[0] = vec4(0.0f, 0.0f, -5.0f, 1.0f);
    data.colours[0] = GetColour(255, 247, 207) * LIGHT_STR;

    data.positions[1] = vec4(-5.0f, 0.0f, 0.0f, 1.0f);
    data.colours[1] = GetColour(125, 214, 250) * float(0);

    data.positions[2] = vec4(0.0f, 6.0f, 0.0f, 1.0f);
    data.colours[2] = GetColour(255, 54, 114) * float(0);

    data.positions[3] = vec4(-5.0f, 5.0f, 5.0f, 0.0f);
    data.colours[3] = GetColour(0x711c91ff) * float(0);

    m_buffers = buffers;

    const auto info = vkInits::bufferCreateInfo(sizeof(light_data), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    for (size_t i = 0; i < m_buffers.count; i++)
        m_buffers[i].create(device, &info, MEM_FLAG_HOST_VISIBLE, &data);
}

void lights::destroy(VkDevice device)
{
    for (size_t i = 0; i < m_buffers.count; i++)
        m_buffers[i].destroy(device);
}
#include "lights.hpp"

void lights::init(const vulkan_device *device, view<buffer_t> buffers)
{
    constexpr float LIGHT_STR = 300.0f;

    light_object lights[LIGHTS_COUNT] = {
        light_object(vec3(5.0f, 1.0f, -5.0f), GetColour(255, 247, 207) * LIGHT_STR),
        light_object(vec3(-5.0f, -1.0f, 5.0f), GetColour(125, 214, 250) * LIGHT_STR),
        light_object(vec3(0.0f, 0.0f, 0.0f), GetColour(255, 255, 255) * float(0)),
        light_object(vec3(0.0f, 0.0f, 0.0f), GetColour(255, 255, 255) * float(0))
    };

    auto data = light_data(lights);

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
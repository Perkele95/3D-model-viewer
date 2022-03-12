#pragma once

#include "vulkan_device.hpp"
#include "buffer.hpp"

constexpr size_t LIGHTS_COUNT = 4;

struct alignas(16) light_data
{
    // NOTE(arle): consider SoA -> AoS for a light object
    vec4<float> positions[LIGHTS_COUNT];
    vec4<float> colours[LIGHTS_COUNT];
};

class lights
{
public:
    lights() = default;
    lights(const vulkan_device *device, view<buffer_t> buffers);
    void destroy(VkDevice device);

    VkDescriptorBufferInfo descriptor(size_t imageIndex){return m_buffers[imageIndex].descriptor(0);}

private:
    view<buffer_t> m_buffers;
};
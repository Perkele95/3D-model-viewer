#pragma once

#include "vulkan_device.hpp"
#include "buffer.hpp"

constexpr size_t LIGHTS_COUNT = 4;

struct light_object
{
    light_object(vec3<float> pos, vec4<float> col):
        position(pos), colour(col)
    {
    }

    vec3<float> position;
    vec4<float> colour;
};

struct alignas(16) light_data
{
    light_data(light_object (&lights)[LIGHTS_COUNT])
    {
        for (size_t i = 0; i < LIGHTS_COUNT; i++){
            positions[i] = vec4(lights[i].position, 1.0f);
            colours[i] = lights[i].colour;
        }
    }

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
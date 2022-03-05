#pragma once

#include "vulkan_initialisers.hpp"

constexpr size_t LIGHTS_COUNT = 4;

struct alignas(16) light_data
{
    static VkBufferUsageFlags usageFlags() { return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT; }

    // NOTE(arle): consider SoA -> AoS for a light object
    vec4<float> positions[LIGHTS_COUNT];
    vec4<float> colours[LIGHTS_COUNT];
};

struct light_object
{
    vec3<float> position;
    vec4<float> colour;
};
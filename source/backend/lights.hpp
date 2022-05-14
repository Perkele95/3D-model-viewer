#pragma once

#include "VulkanDevice.hpp"

constexpr size_t LIGHTS_COUNT = 4;

struct alignas(16) LightData
{
    vec4<float> positions[LIGHTS_COUNT];
    vec4<float> colours[LIGHTS_COUNT];
    float exposure;
    float gamma;
    vec2<float> reserved;
};

struct PointLight
{
    float strength;
    vec3<float> position;
    vec4<float> colour;
};

class SceneLight
{
public:
    void init(const VulkanDevice *device);
    void destroy(VkDevice device);
    void update(VkDevice device);

    float           exposure;
    float           gamma;
    PointLight      pointLights[4];
    VulkanBuffer    buffers[MAX_IMAGES_IN_FLIGHT];
};
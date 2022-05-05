#pragma once

#include "VulkanDevice.hpp"

constexpr size_t LIGHTS_COUNT = 4;

struct alignas(16) LightData
{
    vec4<float> positions[LIGHTS_COUNT];
    vec4<float> colours[LIGHTS_COUNT];
};

class SceneLights
{
public:
    void init();

    LightData getData();

private:
    vec4<float> m_positions[LIGHTS_COUNT];
    vec4<float> m_colours[LIGHTS_COUNT];
};
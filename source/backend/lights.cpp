#include "lights.hpp"

void SceneLights::init()
{
    constexpr float LIGHT_STR = 200.0f;
    m_positions[0] = vec4(5.0f, 1.0f, -5.0f, 0.0f);
    m_positions[1] = vec4(-5.0f, -1.0f, 5.0f, 0.0f);
    m_positions[2] = vec4(0.0f);
    m_positions[3] = vec4(0.0f);

    m_colours[0] = GetColour(255, 247, 207) * LIGHT_STR;
    m_colours[1] = GetColour(125, 214, 250) * LIGHT_STR;
    m_colours[2] = vec4(0.0f);
    m_colours[3] = vec4(0.0f);
}

LightData SceneLights::getData()
{
    auto data = LightData();
    for (size_t i = 0; i < LIGHTS_COUNT; i++)
    {
        data.positions[i] = m_positions[i];
        data.colours[i] = m_colours[i];
    }
    return data;
}
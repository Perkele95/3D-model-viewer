#include "lights.hpp"

void SceneLight::init(const VulkanDevice *device)
{
    for (size_t i = 0; i < MAX_IMAGES_IN_FLIGHT; i++)
    {
        device->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, MEM_FLAG_HOST_VISIBLE,
                            sizeof(LightData), buffers[i]);
    }

    pointLights[0].strength = 200.0f;
    pointLights[0].position = vec3(5.0f, 1.0f, -5.0f);
    pointLights[0].colour = GetColour(255, 247, 207);

    pointLights[1].strength = 200.0f;
    pointLights[1].position = vec3(-5.0f, -1.0f, 5.0f);
    pointLights[1].colour = GetColour(125, 214, 250);

    pointLights[2].strength = 0.0f;
    pointLights[2].position = vec3(0.0f);
    pointLights[2].colour = vec4(0.0f);

    pointLights[3].strength = 0.0f;
    pointLights[3].position = vec3(0.0f);
    pointLights[3].colour = vec4(0.0f);

    update(device->device);
}

void SceneLight::destroy(VkDevice device)
{
    for (size_t i = 0; i < MAX_IMAGES_IN_FLIGHT; i++)
        buffers[i].destroy(device);
}

void SceneLight::update(VkDevice device)
{
    for (size_t image = 0; image < MAX_IMAGES_IN_FLIGHT; image++)
    {
        buffers[image].map(device);

        auto mapped = static_cast<LightData*>(buffers[image].mapped);
        for (size_t i = 0; i < arraysize(pointLights); i++)
        {
            mapped->positions[i] = vec4(pointLights[i].position, 1.0f);
            mapped->colours[i] = pointLights[i].colour * pointLights[i].strength;
        }

        mapped->exposure = exposure;
        mapped->gamma = gamma;

        buffers[image].unmap(device);
    }
}
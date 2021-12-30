#pragma once

#include "vulkan_initialisers.hpp"

struct alignas(16) light_properties
{
    vec4<float> position;
    vec4<float> ambient;
    vec4<float> diffuse;
    vec4<float> specular;
};

struct light_object
{
    vec3<float> position;
    vec3<float> ambient;
    vec3<float> diffuse;
    vec3<float> specular;
};
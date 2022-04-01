#pragma once

#include "vulkan_initialisers.hpp"
#include "VulkanDevice.hpp"
#include "buffer.hpp"
#include "texture.hpp"
#include "mesh3D.hpp"

class Model3D
{
public:
    static VkPushConstantRange pushConstant();

    void destroy(VkDevice device);
    void draw(VkCommandBuffer cmd, VkPipelineLayout layout);

    mat4x4 transform;
    Mesh3D mesh;

    // Material

    Texture2D albedo;
    Texture2D normal;
    Texture2D roughness;
    Texture2D metallic;
    Texture2D ao;
};
#pragma once

#include "vulkan_initialisers.hpp"
#include "vulkan_device.hpp"
#include "buffer.hpp"
#include "mesh3D.hpp"
#include "pbr_material.hpp"

class model3D
{
public:
    model3D(): m_device(nullptr),
               mesh(nullptr),
               material(nullptr),
               transform()
    {
    }

    model3D(const vulkan_device* device):
        m_device(device),
        mesh(nullptr),
        material(nullptr),
        transform(mat4x4::identity())
    {
    }

    void destroy();
    void draw(VkCommandBuffer cmd, VkPipelineLayout layout);

    static VkPushConstantRange pushConstant();

    mat4x4                  transform;
    mesh3D*                 mesh;
    pbr_material*           material;

private:
    const vulkan_device*    m_device;
};
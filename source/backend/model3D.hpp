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
               m_mesh(nullptr),
               m_material(nullptr),
               transform()
    {
    }

    model3D(const vulkan_device* device,
            mesh3D *mesh,
            pbr_material *material): m_device(device),
                                     m_mesh(mesh),
                                     m_material(material),
                                     transform(mat4x4::identity())
    {
    }

    void destroy();
    void draw(VkCommandBuffer cmd, VkPipelineLayout layout);

    static VkPushConstantRange pushConstant();

    mat4x4 transform;

private:
    const vulkan_device*    m_device;
    mesh3D*                 m_mesh;
    pbr_material*           m_material;
};
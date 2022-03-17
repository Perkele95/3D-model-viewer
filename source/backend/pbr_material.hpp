#include "vulkan_initialisers.hpp"
#include "vulkan_device.hpp"
#include "texture2D.hpp"

class pbr_material
{
public:
    void destroy(VkDevice device)
    {
        albedo.destroy(device);
        normal.destroy(device);
        roughness.destroy(device);
        metallic.destroy(device);
        ao.destroy(device);
    }

    texture2D albedo;
    texture2D normal;
    texture2D roughness;
    texture2D metallic;
    texture2D ao;
};
#include "vulkan_initialisers.hpp"
#include "VulkanDevice.hpp"
#include "VulkanTexture.hpp"

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

    Texture2D albedo;
    Texture2D normal;
    Texture2D roughness;
    Texture2D metallic;
    Texture2D ao;
};
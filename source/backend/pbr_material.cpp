#include "pbr_material.hpp"

void pbr_material::destroy(VkDevice device)
{
    albedo.destroy(device);
    normal.destroy(device);
    roughness.destroy(device);
    metallic.destroy(device);
    ao.destroy(device);
}

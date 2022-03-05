#include "vulkan_initialisers.hpp"
#include "vulkan_device.hpp"
#include "texture2D.hpp"

class pbr_material
{
public:
    pbr_material(){}

    static VkDescriptorPoolSize poolSize(size_t imageCount)
    {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = 5 * uint32_t(imageCount);
        return poolSize;
    }

    static VkDescriptorSetLayoutBinding binding(uint32_t binding)
    {
        return vkInits::descriptorSetLayoutBinding(binding,
                                                   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                   VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    static VkWriteDescriptorSet descriptorWrite(uint32_t binding,
                                                VkDescriptorSet dstSet,
                                                const VkDescriptorImageInfo* pInfo)
    {
        VkWriteDescriptorSet set{};
        set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        set.dstBinding = binding;
        set.descriptorCount = uint32_t(1);
        set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        set.dstSet = dstSet;
        set.pImageInfo = pInfo;
        return set;
    }

    void destroy(VkDevice device);

    texture2D albedo;
    texture2D normal;
    texture2D roughness;
    texture2D metallic;
    texture2D ao;
};
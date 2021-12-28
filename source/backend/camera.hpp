#pragma once

#include "../mv_utils/mat4.hpp"
#include "vulkan_initialisers.hpp"

struct alignas(16) mvp_matrix
{
    static VkPushConstantRange pushConstant()
    {
        VkPushConstantRange range;
        range.offset = 0;
        range.size = sizeof(mvp_matrix);
        range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        return range;
    }

    void bind(VkCommandBuffer commandBuffer, VkPipelineLayout layout)
    {
        const auto pc = pushConstant();
        vkCmdPushConstants(commandBuffer,
                           layout,
                           pc.stageFlags,
                           pc.offset,
                           pc.size,
                           this);
    }

    mat4x4 model;
    mat4x4 view;
    mat4x4 proj;
};

struct alignas(4) camera_data
{
    static VkDescriptorPoolSize poolSize(size_t count)
    {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = uint32_t(count);
        return poolSize;
    }

    static VkDescriptorSetLayoutBinding binding()
    {
        VkDescriptorSetLayoutBinding setBinding{};
        setBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        setBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        setBinding.descriptorCount = uint32_t(1);
        setBinding.binding = 0;
        return setBinding;
    }

    static VkWriteDescriptorSet descriptorWrite()
    {
        VkWriteDescriptorSet set{};
        set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        set.dstBinding = 0;
        set.descriptorCount = uint32_t(1);
        set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        return set;
    }

    static VkBufferUsageFlags usageFlags()
    {
        return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }

    static VkMemoryPropertyFlags bufferMemFlags()
    {
        return VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    }

    vec4<float> position;
};

// NOTE(arle): radians, not degrees

struct camera
{
    static constexpr float DEFAULT_FOV = GetRadians(75.0f);
    static constexpr float DEFAULT_ZNEAR = 0.005f;
    static constexpr float DEFAULT_ZFAR = 100.0f;
    static constexpr float DEFAULT_YAW = GetRadians(90.0f);
    static constexpr float PITCH_CLAMP = GetRadians(90.0f) - 0.01f;
    static constexpr float YAW_MOD = GetRadians(360.0f);
    static constexpr auto GLOBAL_UP = vec3(0.0f, 1.0f, 0.0f);

    enum class direction
    {
        forward,
        backward,
        left,
        right
    };

    camera() = default;

    camera(float aspectRatio)
    {
        this->model = mat4x4::identity();
        this->view = mat4x4::identity();
        this->proj = mat4x4::identity();
        this->fov = DEFAULT_FOV;
        this->zNear = DEFAULT_ZNEAR;
        this->zFar = DEFAULT_ZFAR;
        this->position = vec3(0.0f, 0.0f, -2.0f);
        this->sensitivity = 2.0f;
        this->yaw = DEFAULT_YAW;
        this->pitch = 0.0f;
        update(aspectRatio);
    }

    void move(direction dir, float dt)
    {
        switch (dir){
            case direction::forward: this->position += m_front * dt; break;
            case direction::backward: this->position -= m_front * dt; break;
            case direction::left: this->position += m_right * dt; break;
            case direction::right: this->position -= m_right * dt; break;
        };
    }

    void update(float aspectRatio)
    {
        if(this->pitch > PITCH_CLAMP)
            this->pitch = PITCH_CLAMP;
        else if(this->pitch < -PITCH_CLAMP)
            this->pitch = -PITCH_CLAMP;

        if(this->yaw > YAW_MOD)
            this->yaw = 0.0f;
        else if(this->yaw < 0.0f)
            this->yaw = YAW_MOD;

        const auto yawCosine = cosf(this->yaw);
        const auto yawSine = sinf(this->yaw);
        const auto pitchCosine = cosf(this->pitch);
        const auto pitchSine = sinf(this->pitch);

        m_front = vec3(yawCosine * pitchCosine, pitchSine, yawSine * pitchCosine).normalise();
        m_right = (m_front.crossProduct(GLOBAL_UP)).normalise();
        m_up = (m_front.crossProduct(m_right)).normalise();

        this->view = mat4x4::lookAt(this->position, this->position + m_front, m_up);
        this->proj = mat4x4::perspective(this->fov, aspectRatio, this->zNear, this->zFar);
    }

    void map(VkDevice device, VkDeviceMemory memory) const
    {
        auto pushData = camera_data();

        camera_data *mapped = nullptr;
        vkMapMemory(device, memory, 0, sizeof(pushData), 0, reinterpret_cast<void**>(&mapped));
        mapped->position = vec4(this->position, 1.0f);
        vkUnmapMemory(device, memory);
    }

    mat4x4 model;
    mat4x4 view;
    mat4x4 proj;

    vec3<float> position;
    float fov, sensitivity;
    float yaw, pitch;
    float zNear, zFar;

private:
    vec3<float> m_right;
    vec3<float> m_front;
    vec3<float> m_up;
};
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
    static constexpr float DEFAULT_FOV = PI32 / 2.0f;
    static constexpr float DEFAULT_ZNEAR = 0.1f;
    static constexpr float DEFAULT_ZFAR = 100.0f;
    static constexpr float DEFAULT_YAW = PI32 / 2.0f;
    static constexpr float PITCH_CLAMP = (PI32 / 2.0f) - 0.01f;
    static constexpr auto UP_VECTOR = vec3(0.0f, 1.0f, 0.0f);

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
        update(aspectRatio, vec2(0i32), 0.0f);
    }

    void refresh(float aspectRatio)
    {
        this->view = mat4x4::lookAt(this->position, this->position + this->front, UP_VECTOR);
        this->proj = mat4x4::perspective(this->fov, aspectRatio, this->zNear, this->zFar);
    }

    void update(float aspectRatio, vec2<int32_t> offset, float dt)
    {
        this->yaw += this->sensitivity * dt * float(offset.x);
        this->pitch -= this->sensitivity * dt * float(offset.y);

        if(this->pitch > PITCH_CLAMP)
            this->pitch = PITCH_CLAMP;
        else if(this->pitch < -PITCH_CLAMP)
            this->pitch = -PITCH_CLAMP;

        const auto yawCosine = cosf(this->yaw);
        const auto yawSine = sinf(this->yaw);
        const auto pitchCosine = cosf(this->pitch);
        const auto pitchSine = sinf(this->pitch);

        this->front.x = yawCosine * pitchCosine;
        this->front.y = pitchSine;
        this->front.z = yawSine * pitchCosine;
        //this->right = up.crossProduct(this->front).normalise();

        this->view = mat4x4::lookAt(this->position, this->position + this->front, UP_VECTOR);
        this->proj = mat4x4::perspective(this->fov, aspectRatio, this->zNear, this->zFar);
    }

    void resetView(float aspectRatio)
    {
        this->yaw = DEFAULT_YAW;
        this->pitch = 0.0f;
        const auto yawCosine = cosf(this->yaw);
        const auto yawSine = sinf(this->yaw);
        const auto pitchCosine = cosf(this->pitch);
        const auto pitchSine = sinf(this->pitch);

        this->front.x = yawCosine * pitchCosine;
        this->front.y = pitchSine;
        this->front.z = yawSine * pitchCosine;
        this->view = mat4x4::lookAt(this->position, this->position + this->front, UP_VECTOR);
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
    float fov, yaw, pitch;

private:
    float zNear, zFar;
    float sensitivity;

    vec3<float> right;
    vec3<float> front;
};
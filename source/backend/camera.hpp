#pragma once

#include "../mv_utils/mat4.hpp"
#include "vulkan_initialisers.hpp"

struct alignas(16) mvp_matrix
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
        setBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        setBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        setBinding.descriptorCount = uint32_t(1);
        setBinding.binding = 0;
        return setBinding;
    }

    static VkWriteDescriptorSet descriptorWrite(VkDescriptorSet dstSet, const VkDescriptorBufferInfo *pInfo)
    {
        VkWriteDescriptorSet set{};
        set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        set.dstBinding = 0;
        set.descriptorCount = uint32_t(1);
        set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        set.dstSet = dstSet;
        set.pBufferInfo = pInfo;
        return set;
    }

    static VkBufferUsageFlags usageFlags()
    {
        return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }

    mat4x4 view;
    mat4x4 proj;
    vec4<float> position;
};

// NOTE(arle): radians, not degrees

struct camera
{
    static constexpr float DEFAULT_FOV = GetRadians(60.0f);
    static constexpr float DEFAULT_ZNEAR = 0.1f;
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
        right,
        up,
        down
    };

    camera()
    {
        this->fov = DEFAULT_FOV;
        m_zNear = DEFAULT_ZNEAR;
        m_zFar = DEFAULT_ZFAR;
        m_position = vec3(0.0f, 0.0f, -5.0f);
        this->sensitivity = 2.0f;
        m_yaw = DEFAULT_YAW;
        m_pitch = 0.0f;
        update();
    }

    void rotate(direction dir, float dt)
    {
        const auto speed = this->sensitivity * dt;
        switch (dir){
            case direction::up: m_pitch += speed; break;
            case direction::down: m_pitch -= speed; break;
            case direction::left: m_yaw -= speed; break;
            case direction::right: m_yaw += speed; break;
            default: break;
        };
    }

    void move(direction dir, float dt)
    {
        switch (dir){
            case direction::forward: m_position += m_front * dt; break;
            case direction::backward: m_position -= m_front * dt; break;
            case direction::left: m_position -= m_right * dt; break;
            case direction::right: m_position += m_right * dt; break;
            default: break;
        };
    }

    void update()
    {
        if(m_pitch > PITCH_CLAMP)
            m_pitch = PITCH_CLAMP;
        else if(m_pitch < -PITCH_CLAMP)
            m_pitch = -PITCH_CLAMP;

        if(m_yaw > YAW_MOD)
            m_yaw = 0.0f;
        else if(m_yaw < 0.0f)
            m_yaw = YAW_MOD;

        const auto yawCosine = cosf(m_yaw);
        const auto yawSine = sinf(m_yaw);
        const auto pitchCosine = cosf(m_pitch);
        const auto pitchSine = sinf(m_pitch);

        m_front = vec3(yawCosine * pitchCosine, pitchSine, yawSine * pitchCosine).normalise();
        m_right = (m_front.crossProduct(GLOBAL_UP)).normalise();
        m_up = (m_right.crossProduct(m_front)).normalise();
    }

    mvp_matrix calculateMvp(float aspectRatio)
    {
        auto mvp = mvp_matrix();
        mvp.view = mat4x4::lookAt(m_position, m_position + m_front, m_up);
        mvp.proj = mat4x4::perspective(this->fov, aspectRatio, m_zNear, m_zFar);
        mvp.position = vec4(m_position, 1.0f);
        return mvp;
    }

    float fov, sensitivity;

private:
    float m_yaw, m_pitch;
    float m_zNear, m_zFar;

    vec3<float> m_position;
    vec3<float> m_right;
    vec3<float> m_front;
    vec3<float> m_up;
};
#pragma once

#include "../mv_utils/mat4.hpp"
#include "buffer.hpp"

struct alignas(16) mvp_matrix
{
    mat4x4 view;
    mat4x4 proj;
    vec4<float> position;
};

// NOTE(arle): radians, not degrees

class camera
{
public:
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

    camera() = default;
    camera(const vulkan_device *device, view<buffer_t> buffers);

    VkDescriptorBufferInfo descriptor(size_t imageIndex){return m_buffers[imageIndex].descriptor(0);}
    void update(VkDevice device, float aspectRatio, size_t imageIndex);
    void destroy(VkDevice device);

    // TODO(arle): static dispatch
    void rotate(direction dir, float dt);
    void move(direction dir, float dt);

    float fov, sensitivity;

private:
    void updateVectors();

    view<buffer_t> m_buffers;

    float m_yaw, m_pitch;
    float m_zNear, m_zFar;
    vec3<float> m_position;
    vec3<float> m_right;
    vec3<float> m_front;
    vec3<float> m_up;
};
#pragma once

#include "../mv_utils/mat4.hpp"
#include "VulkanDevice.hpp"

struct CameraControls
{
    bool rotateUp;
    bool rotateDown;
    bool rotateLeft;
    bool rotateRight;
    bool moveForward;
    bool moveBackward;
    bool moveLeft;
    bool moveRight;
};

struct alignas(16) mvp_matrix
{
    mat4x4 view;
    mat4x4 proj;
    vec4<float> position;
};

struct alignas(16) ModelViewMatrix
{
    static constexpr VkPushConstantRange pushConstant()
    {
        VkPushConstantRange constant{};
        constant.offset = 0;
        constant.size = sizeof(ModelViewMatrix);
        constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        return constant;
    }

    mat4x4 view;
    mat4x4 proj;
};

// NOTE(arle): radians, not degrees

class Camera
{
public:
    static constexpr float FOV_DEFAULT = GetRadians(60.0f);
    static constexpr float FOV_LIMITS_LOW = GetRadians(15.0f);
    static constexpr float FOV_LIMITS_HIGH = GetRadians(105.0f);

    static constexpr float DEFAULT_ZNEAR = 0.1f;
    static constexpr float DEFAULT_ZFAR = 100.0f;
    static constexpr float DEFAULT_YAW = GetRadians(90.0f);
    static constexpr float PITCH_CLAMP = GetRadians(90.0f) - 0.01f;
    static constexpr float YAW_MOD = GetRadians(360.0f);

    static constexpr auto GLOBAL_UP = vec3(0.0f, 1.0f, 0.0f);

    void init();
    void update(float dt, float aspectRatio);

    mvp_matrix getModelViewProjection();
    ModelViewMatrix getModelView();

    float           fov;
    float           sensitivity;
    CameraControls  controls;

private:
    void updateVectors();

    mat4x4          m_view, m_proj;
    float           m_yaw, m_pitch;
    float           m_zNear, m_zFar;
    vec3<float>     m_position;
    vec3<float>     m_right;
    vec3<float>     m_front;
    vec3<float>     m_up;
};
#pragma once

#include "../mv_utils/mat4.hpp"
#include "buffer.hpp"

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

// NOTE(arle): radians, not degrees

class Camera
{
public:
    static constexpr float FOV_DEFAULT = GetRadians(60.0f);
    static constexpr float FOV_LIMITS_LOW = GetRadians(15.0f);
    static constexpr float FOV_LIMITS_HIGH = GetRadians(120.0f);

    static constexpr float DEFAULT_ZNEAR = 0.1f;
    static constexpr float DEFAULT_ZFAR = 100.0f;
    static constexpr float DEFAULT_YAW = GetRadians(90.0f);
    static constexpr float PITCH_CLAMP = GetRadians(90.0f) - 0.01f;
    static constexpr float YAW_MOD = GetRadians(360.0f);

    static constexpr auto GLOBAL_UP = vec3(0.0f, 1.0f, 0.0f);

    void init();
    void update(float dt);
    mvp_matrix calculateMatrix(float aspectRatio);

    float           fov;
    float           sensitivity;
    CameraControls  controls;

private:
    void updateVectors();

    float           m_yaw, m_pitch;
    float           m_zNear, m_zFar;
    vec3<float>     m_position;
    vec3<float>     m_right;
    vec3<float>     m_front;
    vec3<float>     m_up;
};
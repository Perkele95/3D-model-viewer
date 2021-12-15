#pragma once

#include "../mv_utils/mat4.hpp"
#include "vulkan_initialisers.hpp"

struct alignas(16) camera_matrix
{
    static VkPushConstantRange pushConstant()
    {
        VkPushConstantRange range;
        range.offset = 0;
        range.size = sizeof(camera_matrix);
        range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        return range;
    }

    mat4x4 model;
    mat4x4 view;
    mat4x4 proj;
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
        this->position = vec3(0.8f, 0.8f, -2.0f);
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

    mat4x4 model;
    mat4x4 view;
    mat4x4 proj;

    float fov;

private:
    float zNear, zFar;
    float yaw, pitch;
    float sensitivity;

    vec3<float> position;
    vec3<float> right;
    vec3<float> front;
};
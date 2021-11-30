#pragma once

#include "../mv_utils/mat4.hpp"
#include "vk_initialisers.hpp"

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

struct camera
{
    static constexpr float DEFAULT_FOV = PI32 / 2.0f;
    static constexpr float DEFAULT_ZNEAR = 0.1f;
    static constexpr float DEFAULT_ZFAR = 100.0f;

    camera() = default;

    camera(float aspectRatio)
    {
        this->model = mat4x4::identity();
        this->view = mat4x4::identity();
        this->proj = mat4x4::identity();
        this->fov = DEFAULT_FOV;
        this->zNear = DEFAULT_ZNEAR;
        this->zFar = DEFAULT_ZFAR;
        update(aspectRatio);
    }

    void update(float aspectRatio)
    {
        constexpr auto eye = vec3(0.8f, 0.8f, -2.0f);
        constexpr auto centre = vec3(0.0f);
        constexpr auto up = vec3(0.0f, 1.0f, 0.0f);
        this->view = mat4x4::lookAt(eye, centre, up);
        this->proj = mat4x4::perspective(this->fov, aspectRatio, this->zNear, this->zFar);
    }

    mat4x4 model;
    mat4x4 view;
    mat4x4 proj;

    float fov, zNear, zFar;
};
#include "camera.hpp"

camera::camera(const vulkan_device *device, view<buffer_t> buffers)
{
    this->fov = DEFAULT_FOV;
    this->sensitivity = 2.0f;
    m_zNear = DEFAULT_ZNEAR;
    m_zFar = DEFAULT_ZFAR;
    m_position = vec3(0.0f, 0.0f, -5.0f);
    m_yaw = DEFAULT_YAW;
    m_pitch = 0.0f;
    updateVectors();

    m_buffers = buffers;

    const auto info = vkInits::bufferCreateInfo(sizeof(mvp_matrix), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    for (size_t i = 0; i < m_buffers.count; i++)
        m_buffers[i].create(device, &info, MEM_FLAG_HOST_VISIBLE);
}

void camera::update(VkDevice device, float aspectRatio, size_t imageIndex)
{
    updateVectors();

    auto mvp = mvp_matrix();
    mvp.view = mat4x4::lookAt(m_position, m_position + m_front, m_up);
    mvp.proj = mat4x4::perspective(this->fov, aspectRatio, m_zNear, m_zFar);
    mvp.position = vec4(m_position, 1.0f);

    m_buffers[imageIndex].fill(device, &mvp);
}

void camera::destroy(VkDevice device)
{
    for (size_t i = 0; i < m_buffers.count; i++)
        m_buffers[i].destroy(device);
}

void camera::rotate(direction dir, float dt)
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

void camera::move(direction dir, float dt)
{
    switch (dir){
        case direction::forward: m_position += m_front * dt; break;
        case direction::backward: m_position -= m_front * dt; break;
        case direction::left: m_position -= m_right * dt; break;
        case direction::right: m_position += m_right * dt; break;
        default: break;
    };
}

void camera::updateVectors()
{
    m_pitch = clamp(m_pitch, -PITCH_CLAMP, PITCH_CLAMP);
    m_yaw = clamp(m_yaw, -YAW_MOD, YAW_MOD);

    const auto yawCosine = std::cos(m_yaw);
    const auto yawSine = std::sin(m_yaw);
    const auto pitchCosine = std::cos(m_pitch);
    const auto pitchSine = std::sin(m_pitch);

    m_front = vec3(yawCosine * pitchCosine, pitchSine, yawSine * pitchCosine).normalise();
    m_right = (m_front.crossProduct(GLOBAL_UP)).normalise();
    m_up = (m_right.crossProduct(m_front)).normalise();
}
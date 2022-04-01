#include "camera.hpp"

void Camera::init()
{
    this->fov = FOV_DEFAULT;
    this->sensitivity = 2.0f;
    m_zNear = DEFAULT_ZNEAR;
    m_zFar = DEFAULT_ZFAR;
    m_position = vec3(0.0f, 0.0f, -5.0f);
    m_yaw = DEFAULT_YAW;
    m_pitch = 0.0f;
    updateVectors();
}

void Camera::update(float dt)
{
    const auto speed = sensitivity * dt;
    if(controls.rotateUp)
        m_pitch += speed;
    else if(controls.rotateDown)
        m_pitch -= speed;

    if(controls.rotateLeft)
        m_yaw -= speed;
    else if(controls.rotateRight)
        m_yaw += speed;

    if(controls.moveForward)
        m_position += m_front * dt;
    else if(controls.moveBackward)
        m_position -= m_front * dt;

    if(controls.moveLeft)
        m_position -= m_right * dt;
    else if(controls.moveRight)
        m_position += m_right * dt;

    updateVectors();
}

mvp_matrix Camera::calculateMatrix(float aspectRatio)
{
    auto mvp = mvp_matrix();
    mvp.view = mat4x4::lookAt(m_position, m_position + m_front, m_up);
    mvp.proj = mat4x4::perspective(this->fov, aspectRatio, m_zNear, m_zFar);
    mvp.position = vec4(m_position, 1.0f);
    return mvp;
}

void Camera::updateVectors()
{
    m_pitch = clamp(m_pitch, -PITCH_CLAMP, PITCH_CLAMP);
    m_yaw = m_yaw < -YAW_MOD ? m_yaw + YAW_MOD : m_yaw;
    m_yaw = m_yaw > YAW_MOD ? m_yaw - YAW_MOD : m_yaw;

    const auto yawCosine = std::cos(m_yaw);
    const auto yawSine = std::sin(m_yaw);
    const auto pitchCosine = std::cos(m_pitch);
    const auto pitchSine = std::sin(m_pitch);

    m_front = vec3(yawCosine * pitchCosine, pitchSine, yawSine * pitchCosine).normalise();
    m_right = (m_front.crossProduct(GLOBAL_UP)).normalise();
    m_up = (m_right.crossProduct(m_front)).normalise();
}
#pragma once

#include "vec.hpp"
#include <xmmintrin.h>

// NOTE(arle): Translate - Rotate - Scale

struct mat4x4
{
    mat4x4() = default;
    mat4x4(float value)
    {
        this->data[0][0] = value;
        this->data[0][1] = 0.0f;
        this->data[0][2] = 0.0f;
        this->data[0][3] = 0.0f;
        this->data[1][0] = 0.0f;
        this->data[1][1] = value;
        this->data[1][2] = 0.0f;
        this->data[1][3] = 0.0f;
        this->data[2][0] = 0.0f;
        this->data[2][1] = 0.0f;
        this->data[2][2] = value;
        this->data[2][3] = 0.0f;
        this->data[3][0] = 0.0f;
        this->data[3][1] = 0.0f;
        this->data[3][2] = 0.0f;
        this->data[3][3] = value;
    }

    float &operator()(size_t x, size_t y)
    {
        return data[x][y];
    }

    mat4x4 &translate(vec3<float> t)
    {
        this->data[0][3] = t.x;
        this->data[1][3] = t.y;
        this->data[2][3] = t.z;
        return *this;
    }

    mat4x4 &rotateX(float radians)
    {
        float c = cosf(radians);
        float s = sinf(radians);
        this->data[1][1] = c;
        this->data[1][2] = -s;
        this->data[2][1] = s;
        this->data[2][2] = c;
        return *this;
    }

    mat4x4 &rotateY(float radians)
    {
        float c = cosf(radians);
        float s = sinf(radians);
        this->data[0][0] = c;
        this->data[0][2] = s;
        this->data[2][0] = -s;
        this->data[2][2] = c;
        return *this;
    }

    mat4x4 &rotateZ(float radians)
    {
        float c = cosf(radians);
        float s = sinf(radians);
        this->data[0][0] = c;
        this->data[0][1] = -s;
        this->data[1][0] = s;
        this->data[1][1] = c;
        return *this;
    }

    static mat4x4 lookAt(vec3<float> eye, vec3<float> centre, vec3<float> up)
    {
        auto Z = vec3(eye - centre).normalise();
        auto Y = up;
        auto X = Y.crossProduct(Z);
        Y = Z.crossProduct(X);

        X.normalise();
        Y.normalise();

        mat4x4 result;
        result.data[0][0] = X.x;
        result.data[1][0] = X.y;
        result.data[2][0] = X.z;
        result.data[3][0] = -X.dotProduct(eye);

        result.data[0][1] = Y.x;
        result.data[1][1] = Y.y;
        result.data[2][1] = Y.z;
        result.data[3][1] = -Y.dotProduct(eye);

        result.data[0][2] = Z.x;
        result.data[1][2] = Z.y;
        result.data[2][2] = Z.z;
        result.data[3][2] = -Z.dotProduct(eye);

        result.data[0][3] = 0.0f;
        result.data[1][3] = 0.0f;
        result.data[2][3] = 0.0f;
        result.data[3][3] = 1.0f;
        return result;
    }

    static mat4x4 perspective(float fov, float aspectRatio, float zNear, float zFar)
    {
        float t = tanf(fov / 2.0f);
        auto result = mat4x4(1.0f);
        result.data[0][0] = 1.0f / aspectRatio * t;
        result.data[1][1] = -1.0f / t;
        result.data[2][2] = -(zFar + zNear) / (zFar - zNear);
        result.data[2][3] = -1.0f;
        result.data[3][2] = -2.0f * zFar * zNear / (zFar - zNear);
        return result;
    }

    static mat4x4 ortho(float left, float right, float bottom,
                        float top, float zNear, float zFar)
    {
        const float rightMinusLeft = right - left;
        const float topMinusBottom = top - bottom;
        const float zFarMinusZNear = zFar - zNear;
        auto result = mat4x4(1.0f);
        result.data[0][0] = 2.0f / rightMinusLeft;
        result.data[1][1] = 2.0f / topMinusBottom;
        result.data[2][2] = - 2.0f / zFarMinusZNear;
        result.data[3][0] = - (right + left) / rightMinusLeft;
        result.data[3][1] = - (top + bottom) / topMinusBottom;
        result.data[3][2] = - (zFar + zNear) / zFarMinusZNear;
        return result;
    }

private:
    float data[4][4];
};
#pragma once

#include "vec.hpp"
#include <xmmintrin.h>

// NOTE(arle): Translate - Rotate - Scale

struct mat4x4;
constexpr mat4x4 VECTOR_API operator*(mat4x4 a, mat4x4 b);

struct mat4x4
{
    mat4x4() = default;

    static constexpr mat4x4 identity()
    {
        mat4x4 result;
        result.data[0][0] = 1.0f;
        result.data[0][1] = 0.0f;
        result.data[0][2] = 0.0f;
        result.data[0][3] = 0.0f;
        result.data[1][0] = 0.0f;
        result.data[1][1] = 1.0f;
        result.data[1][2] = 0.0f;
        result.data[1][3] = 0.0f;
        result.data[2][0] = 0.0f;
        result.data[2][1] = 0.0f;
        result.data[2][2] = 1.0f;
        result.data[2][3] = 0.0f;
        result.data[3][0] = 0.0f;
        result.data[3][1] = 0.0f;
        result.data[3][2] = 0.0f;
        result.data[3][3] = 1.0f;
        return result;
    }

    constexpr float &operator()(size_t x, size_t y)
    {
        return data[x][y];
    }

    static mat4x4 VECTOR_API translate(vec3<float> t)
    {
        auto result = identity();
        result.data[3][0] = t.x;
        result.data[3][1] = t.y;
        result.data[3][2] = t.z;
        return result;
    }

    static mat4x4 rotateX(float radians)
    {
        auto result = identity();
        const auto c = std::cos(radians);
        const auto s = std::sin(radians);
        result.data[1][1] = c;
        result.data[1][2] = -s;
        result.data[2][1] = s;
        result.data[2][2] = c;
        return result;
    }

    static mat4x4 rotateY(float radians)
    {
        auto result = identity();
        const auto c = std::cos(radians);
        const auto s = std::sin(radians);
        result.data[0][0] = c;
        result.data[0][2] = s;
        result.data[2][0] = -s;
        result.data[2][2] = c;
        return result;
    }

    static mat4x4 rotateZ(float radians)
    {
        auto result = identity();
        const auto c = std::cos(radians);
        const auto s = std::sin(radians);
        result.data[0][0] = c;
        result.data[0][1] = -s;
        result.data[1][0] = s;
        result.data[1][1] = c;
        return result;
    }

    static mat4x4 VECTOR_API scale(vec3<float> s)
    {
        auto result = identity();
        result.data[0][0] = s.x;
        result.data[1][1] = s.y;
        result.data[2][2] = s.z;
        return result;
    }

    static mat4x4 VECTOR_API lookAt(vec3<float> eye, vec3<float> centre, vec3<float> up)
    {
        auto f = normalise(centre - eye);
        auto s = normalise(cross(f, up));
        auto u = cross(s, f);

        eye += f;

        auto result = mat4x4::identity();
        result(0, 0) = s.x;
        result(1, 0) = s.y;
        result(2, 0) = s.z;
        result(3, 0) = -dot(s, eye);
        result(0, 1) = u.x;
        result(1, 1) = u.y;
        result(2, 1) = u.z;
        result(3, 1) = -dot(u, eye);
        result(0, 2) = f.x;
        result(1, 2) = f.y;
        result(2, 2) = f.z;
        result(3, 2) = dot(f, eye);
        return result;
    }

    static mat4x4 VECTOR_API lookAt2(vec3<float> centre, vec3<float> forward,
                                     vec3<float> right, vec3<float> up)
    {
        auto result = mat4x4::identity();
        result(0, 0) = right.x;
        result(1, 0) = right.y;
        result(2, 0) = right.z;
        result(3, 0) = -dot(right, centre);
        result(0, 1) = up.x;
        result(1, 1) = up.y;
        result(2, 1) = up.z;
        result(3, 1) = -dot(up, centre);
        result(0, 2) = -forward.x;
        result(1, 2) = -forward.y;
        result(2, 2) = -forward.z;
        result(3, 2) = dot(forward, centre);
        return result;
    }

    static mat4x4 VECTOR_API perspective(float fov, float aspectRatio, float zNear, float zFar)
    {
        float t = tanf(fov / 2.0f);
        auto result = mat4x4::identity();
        result.data[0][0] = 1.0f / (aspectRatio * t);
        result.data[1][1] = -1.0f / t;
        result.data[2][2] = -(zFar + zNear) / (zFar - zNear);
        result.data[2][3] = -1.0f;
        result.data[3][2] = -(2.0f * zFar * zNear) / (zFar - zNear);
        return result;
    }

    static mat4x4 VECTOR_API ortho(float left, float right, float bottom,
                                   float top, float zNear, float zFar)
    {
        const float rightMinusLeft = right - left;
        const float topMinusBottom = top - bottom;
        const float zFarMinusZNear = zFar - zNear;
        auto result = mat4x4::identity();
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

constexpr mat4x4 VECTOR_API operator*(mat4x4 a, mat4x4 b)
{
    mat4x4 result;
    result(0, 0) = a(0, 0) * b(0, 0) + a(0, 1) * b(1, 0) + a(0, 2) * b(2, 0) + a(0, 3) * b(3, 0);
    result(1, 0) = a(1, 0) * b(0, 0) + a(1, 1) * b(1, 0) + a(1, 2) * b(2, 0) + a(1, 3) * b(3, 0);
    result(2, 0) = a(2, 0) * b(0, 0) + a(2, 1) * b(1, 0) + a(2, 2) * b(2, 0) + a(3, 3) * b(3, 0);
    result(3, 0) = a(3, 0) * b(0, 0) + a(3, 1) * b(1, 0) + a(3, 2) * b(2, 0) + a(3, 3) * b(3, 0);

    result(0, 1) = a(0, 0) * b(0, 1) + a(0, 1) * b(1, 1) + a(0, 2) * b(2, 1) + a(0, 3) * b(3, 1);
    result(1, 1) = a(1, 0) * b(0, 1) + a(1, 1) * b(1, 1) + a(1, 2) * b(2, 1) + a(1, 3) * b(3, 1);
    result(2, 1) = a(2, 0) * b(0, 1) + a(2, 1) * b(1, 1) + a(2, 2) * b(2, 1) + a(3, 3) * b(3, 1);
    result(3, 1) = a(3, 0) * b(0, 1) + a(3, 1) * b(1, 1) + a(3, 2) * b(2, 1) + a(3, 3) * b(3, 1);

    result(0, 2) = a(0, 0) * b(0, 2) + a(0, 1) * b(1, 2) + a(0, 2) * b(2, 2) + a(0, 3) * b(3, 2);
    result(1, 2) = a(1, 0) * b(0, 2) + a(1, 1) * b(1, 2) + a(1, 2) * b(2, 2) + a(1, 3) * b(3, 2);
    result(2, 2) = a(2, 0) * b(0, 2) + a(2, 1) * b(1, 2) + a(2, 2) * b(2, 2) + a(3, 3) * b(3, 2);
    result(3, 2) = a(3, 0) * b(0, 2) + a(3, 1) * b(1, 2) + a(3, 2) * b(2, 2) + a(3, 3) * b(3, 2);

    result(0, 3) = a(0, 0) * b(0, 3) + a(0, 1) * b(1, 3) + a(0, 2) * b(2, 3) + a(0, 3) * b(3, 3);
    result(1, 3) = a(1, 0) * b(0, 3) + a(1, 1) * b(1, 3) + a(1, 2) * b(2, 3) + a(1, 3) * b(3, 3);
    result(2, 3) = a(2, 0) * b(0, 3) + a(2, 1) * b(1, 3) + a(2, 2) * b(2, 3) + a(2, 3) * b(3, 3);
    result(3, 3) = a(3, 0) * b(0, 3) + a(3, 1) * b(1, 3) + a(3, 2) * b(2, 3) + a(3, 3) * b(3, 3);
    return result;
}

inline mat4x4& VECTOR_API operator*=(mat4x4 &a, mat4x4 b)
{
    a = a * b;
    return a;
}

inline vec3<float> VECTOR_API operator*(vec3<float> vec, mat4x4 mat)
{
    vec.x = vec.x * mat(0, 0) + vec.y * mat(0, 1) + vec.z * mat(0, 2) + mat(0, 3);
    vec.y = vec.x * mat(1, 0) + vec.y * mat(1, 1) + vec.z * mat(1, 2) + mat(1, 3);
    vec.z = vec.x * mat(2, 0) + vec.y * mat(2, 1) + vec.z * mat(2, 2) + mat(2, 3);
    return vec;
}

inline vec3<float> VECTOR_API operator*(mat4x4 matrix, vec3<float> vec)
{
    return vec * matrix;
}

inline vec3<float>& VECTOR_API operator*=(vec3<float> &vec, mat4x4 matrix)
{
    vec = vec * matrix;
    return vec;
}
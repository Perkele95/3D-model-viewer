#pragma once

#include <stdint.h>
#include <cmath>

#if defined(_MSC_VER)
    #define VECTOR_API __vectorcall
#elif defined(__clang__)
        #define VECTOR_API __attribute__((vectorcall))
#elif defined(__GNUC__)
        #define VECTOR_API __attribute__((vectorcall))
#else
    #define VECTOR_API
#endif

template<typename T>
struct vec2
{
    constexpr vec2() = default;
    constexpr explicit vec2(T value) : x(value), y(value) {}
    constexpr explicit vec2(T xVal, T yVal) : x(xVal), y(yVal) {}

    constexpr bool operator==(vec2<T> other) const
    {
        return (this->x == other.x && this->y == other.y);
    }

    constexpr bool operator!=(vec2<T> other) const
    {
        return (this->x != other.x && this->y != other.y);
    }

    T x, y;
};

template<typename T>
inline vec2<T> VECTOR_API operator+(vec2<T> a, vec2<T> b)
{
    return vec2(a.x + b.x, a.y + b.y);
}

template<typename T>
inline vec2<T>& VECTOR_API operator+=(vec2<T> &a, vec2<T> b)
{
    a.x += b.x;
    a.y += b.y;
    return a;
}

template<typename T>
inline vec2<T> VECTOR_API operator-(vec2<T> a, vec2<T> b)
{
    return vec2(a.x - b.x, a.y - b.y);
}

template<typename T>
inline vec2<T>& VECTOR_API operator-=(vec2<T> &a, vec2<T> b)
{
    a.x -= b.x;
    a.y -= b.y;
    return a;
}

template<typename T>
inline vec2<T> VECTOR_API operator*(vec2<T> a, T value)
{
    return vec2(a.x * value, a.y * value);
}

template<typename T>
inline vec2<T>& VECTOR_API operator*=(vec2<T> &a, T value)
{
    a.x *= value;
    a.y *= value;
    return a;
}

template<typename T>
inline vec2<T> VECTOR_API operator/(vec2<T> a, T value)
{
    return vec2(a.x / value, a.y / value);
}

template<typename T>
inline vec2<T>& VECTOR_API operator/=(vec2<T> &a, T value)
{
    a.x /= value;
    a.y /= value;
    return a;
}

template<typename T>
inline vec2<T> VECTOR_API operator-(vec2<T> a)
{
    return vec2(-a.x, -a.y);
}

// vec3

template<typename T>
struct vec3
{
    constexpr vec3() = default;
    constexpr explicit vec3(T value) : x(value), y(value), z(value) {}
    constexpr explicit vec3(T xVal, T yVal, T zVal) : x(xVal), y(yVal), z(zVal) {}
    constexpr explicit vec3(vec2<T> vec, T zVal) : x(vec.x), y(vec.y), z(zVal) {}

    constexpr bool operator==(vec3<T> other) const
    {
        return (this->x == other.x && this->y == other.y && this->z == other.z);
    }

    constexpr bool operator!=(vec3<T> other) const
    {
        return (this->x != other.x && this->y != other.y && this->z != other.z);
    }

    T x, y, z;
};

template<typename T>
inline vec3<T> VECTOR_API operator+(vec3<T> a, vec3<T> b)
{
    return vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

template<typename T>
inline vec3<T>& VECTOR_API operator+=(vec3<T> &a, vec3<T> b)
{
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
    return a;
}

template<typename T>
inline vec3<T> VECTOR_API operator-(vec3<T> a, vec3<T> b)
{
    return vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

template<typename T>
inline vec3<T>& VECTOR_API operator-=(vec3<T> &a, vec3<T> b)
{
    a.x -= b.x;
    a.y -= b.y;
    a.z -= b.z;
    return a;
}

template<typename T>
inline vec3<T> VECTOR_API operator*(vec3<T> a, T value)
{
    return vec3(a.x * value, a.y * value, a.z * value);
}

template<typename T>
inline vec3<T>& VECTOR_API operator*=(vec3<T> &a, T value)
{
    a.x *= value;
    a.y *= value;
    a.z *= value;
    return a;
}

template<typename T>
inline vec3<T> VECTOR_API operator/(vec3<T> a, T value)
{
    return vec3(a.x / value, a.y / value, a.z / value);
}

template<typename T>
inline vec3<T>& VECTOR_API operator/=(vec3<T> &a, T value)
{
    a.x /= value;
    a.y /= value;
    a.z /= value;
    return a;
}

template<typename T>
inline vec3<T> VECTOR_API operator-(vec3<T> a)
{
    return vec3(-a.x, -a.y, -a.z);
}

// vec4

template<typename T>
struct vec4
{
    constexpr vec4() = default;
    constexpr explicit vec4(vec3<T> value, T wVal) : x(value.x), y(value.y), z(value.z), w(wVal) {}
    constexpr explicit vec4(T value) : x(value), y(value), z(value), w(value) {}
    constexpr explicit vec4(T xVal, T yVal, T zVal, T wVal) : x(xVal), y(yVal), z(zVal), w(wVal) {}

    constexpr bool operator==(vec4<T> other) const
    {
        return (this->x == other.x && this->y == other.y &&
                this->z == other.z && this->w == other.w);
    }

    constexpr bool operator!=(vec4<T> other) const
    {
        return (this->x != other.x && this->y != other.y &&
                this->z != other.z && this->w != other.w);
    }

    T x, y, z, w;
};

template<typename T>
inline vec4<T> VECTOR_API operator+(vec4<T> a, vec4<T> b)
{
    return vec4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

template<typename T>
inline vec4<T>& VECTOR_API operator+=(vec4<T> &a, vec4<T> b)
{
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
    a.w += b.w;
    return a;
}

template<typename T>
inline vec4<T> VECTOR_API operator-(vec4<T> a, vec4<T> b)
{
    return vec4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

template<typename T>
inline vec4<T>& VECTOR_API operator-=(vec4<T> &a, vec4<T> b)
{
    a.x -= b.x;
    a.y -= b.y;
    a.z -= b.z;
    a.w -= b.w;
    return a;
}

template<typename T>
inline vec4<T> VECTOR_API operator*(vec4<T> a, T value)
{
    return vec4(a.x * value, a.y * value, a.z * value, a.w * value);
}

template<typename T>
inline vec4<T>& VECTOR_API operator*=(vec4<T> &a, T value)
{
    a.x *= value;
    a.y *= value;
    a.z *= value;
    a.w *= value;
    return a;
}

template<typename T>
inline vec4<T> VECTOR_API operator/(vec4<T> a, T value)
{
    return vec4(a.x / value, a.y / value, a.z / value, a.w / value);
}

template<typename T>
inline vec4<T>& VECTOR_API operator/=(vec4<T> &a, T value)
{
    a.x /= value;
    a.y /= value;
    a.z /= value;
    a.w /= value;
    return a;
}

template<typename T>
inline vec4<T> VECTOR_API operator-(vec4<T> a)
{
    return vec4(-a.x, -a.y, -a.z, -a.w);
}

// Float & double specific functions

inline float VECTOR_API length(vec2<float> a)
{
    return std::sqrt(a.x * a.x + a.y * a.y);
}

inline double VECTOR_API length(vec2<double> a)
{
    return std::sqrt(a.x * a.x + a.y * a.y);
}

inline float VECTOR_API length(vec3<float> a)
{
    return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
}

inline double VECTOR_API length(vec3<double> a)
{
    return std::sqrt(a.x * a.x + a.y * a.y) + a.z * a.z;
}

inline float VECTOR_API length(vec4<float> a)
{
    return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z + a.w * a.w);
}

inline double VECTOR_API length(vec4<double> a)
{
    return std::sqrt(a.x * a.x + a.y * a.y) + a.z * a.z + a.w * a.w;
}

inline vec2<float> VECTOR_API normalise(vec2<float> a)
{
    const auto len = length(a);
    a.x /= len;
    a.y /= len;
    return a;
}

inline vec2<double> VECTOR_API normalise(vec2<double> a)
{
    const auto len = length(a);
    a.x /= len;
    a.y /= len;
    return a;
}

inline vec3<float> VECTOR_API normalise(vec3<float> a)
{
    const auto len = length(a);
    a.x /= len;
    a.y /= len;
    a.z /= len;
    return a;
}

inline vec3<double> VECTOR_API normalise(vec3<double> a)
{
    const auto len = length(a);
    a.x /= len;
    a.y /= len;
    a.z /= len;
    return a;
}

inline vec4<float> VECTOR_API normalise(vec4<float> a)
{
    const auto len = length(a);
    a.x /= len;
    a.y /= len;
    a.z /= len;
    a.w /= len;
    return a;
}

inline vec4<double> VECTOR_API normalise(vec4<double> a)
{
    const auto len = length(a);
    a.x /= len;
    a.y /= len;
    a.z /= len;
    a.w /= len;
    return a;
}

inline float VECTOR_API dot(vec3<float> a, vec3<float> b)
{
    return (a.x * b.x + a.y * b.y + a.z * b.z);
}

inline double VECTOR_API dot(vec3<double> a, vec3<double> b)
{
    return (a.x * b.x + a.y * b.y + a.z * b.z);
}

inline vec3<float> VECTOR_API cross(vec3<float> a, vec3<float> b)
{
    auto result = vec3(
        (a.y * b.z) - (a.z * b.y),
        (a.z * b.x) - (a.x * b.z),
        (a.x * b.y) - (a.y * b.x)
    );
    return result;
}

inline vec3<double> VECTOR_API cross(vec3<double> a, vec3<double> b)
{
    auto result = vec3(
        (a.y * b.z) - (a.z * b.y),
        (a.z * b.x) - (a.x * b.z),
        (a.x * b.y) - (a.y * b.x)
    );
    return result;
}
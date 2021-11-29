#pragma once

#include <stdint.h>
#include <math.h>

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

    float length() const
    {
        auto result = sqrtf(float(this->x) * float(this->x) + float(this->y) * float(this->y));
        return result;
    }

    float lengthSquared() const
    {
        auto result = float(this->x) * float(this->x) + float(this->y) * float(this->y);
        return result;
    }

    vec2<T> normalise()
    {
        const auto length = this->length();
        this->x = T(float(this->x) / length);
        this->y = T(float(this->y) / length);
        return *this;
    }

    float dotProduct(vec2<T> other) const
    {
        auto result = float(this->x) * float(other.x) + float(this->y) * float(other.y);
        return result;
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
inline vec2<T> VECTOR_API operator*(vec2<T> a, float f)
{
    return vec2(a.x * f, a.y * f);
}

template<typename T>
inline vec2<T>& VECTOR_API operator*=(vec2<T> &a, float f)
{
    a.x *= static_cast<T>(f);
    a.y *= static_cast<T>(f);
    return a;
}

template<typename T>
inline vec2<T> VECTOR_API operator/(vec2<T> a, float f)
{
    return vec2(a.x / f, a.y / f);
}

template<typename T>
inline vec2<T>& VECTOR_API operator/=(vec2<T> &a, float f)
{
    a.x /= static_cast<T>(f);
    a.y /= static_cast<T>(f);
    return a;
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

    float length() const
    {
        auto result = sqrtf(float(this->x) * float(this->x) + float(this->y) * float(this->y) +
                            float(this->z) * float(this->z));
        return result;
    }

    float lengthSquared() const
    {
        auto result = float(this->x) * float(this->x) + float(this->y) * float(this->y) +
                      float(this->z) * float(this->z);
        return result;
    }

    vec3<T> normalise()
    {
        const auto length = this->length();
        this->x = T(float(this->x) / length);
        this->y = T(float(this->y) / length);
        this->z = T(float(this->z) / length);
        return *this;
    }

    float VECTOR_API dotProduct(vec3<T> other) const
    {
        auto result = float(this->x) * float(other.x) + float(this->y) * float(other.y) +
                      float(this->z) * float(other.z);
        return result;
    }

    vec3<T> VECTOR_API crossProduct(vec3<T> other) const
    {
        auto result = vec3(
            this->y * other.z - this->z * other.y,
            this->z * other.x - this->x * other.z,
            this->x * other.y - this->y * other.x
        );
        return result;
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
inline vec3<T> VECTOR_API operator*(vec3<T> a, float f)
{
    return vec3(a.x * f, a.y * f, a.z * f);
}

template<typename T>
inline vec3<T>& VECTOR_API operator*=(vec3<T> &a, float f)
{
    a.x *= static_cast<T>(f);
    a.y *= static_cast<T>(f);
    a.z *= static_cast<T>(f);
    return a;
}

template<typename T>
inline vec3<T> VECTOR_API operator/(vec3<T> a, float f)
{
    return vec3(a.x / f, a.y / f, a.z / f);
}

template<typename T>
inline vec3<T>& VECTOR_API operator/=(vec3<T> &a, float f)
{
    a.x /= static_cast<T>(f);
    a.y /= static_cast<T>(f);
    a.z /= static_cast<T>(f);
    return a;
}

// vec4

template<typename T>
struct vec4
{
    constexpr vec4() = default;
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

    float length() const
    {
        auto result = sqrtf(float(this->x) * float(this->x) + float(this->y) * float(this->y) +
                            float(this->z) * float(this->z) + float(this->w) * float(this->w));
        return result;
    }

    float lengthSquared() const
    {
        auto result = float(this->x) * float(this->x) + float(this->y) * float(this->y) +
                      float(this->z) * float(this->z) + float(this->w) * float(this->w);
        return result;
    }

    vec4<T> normalise()
    {
        const auto length = this->length();
        this->x = T(float(this->x) / length);
        this->y = T(float(this->y) / length);
        this->z = T(float(this->z) / length);
        this->w = T(float(this->w) / length);
        return *this;
    }

    float dotProduct(vec4<T> other) const
    {
        auto result = float(this->x) * float(other.x) + float(this->y) * float(other.y) +
                      float(this->z) * float(other.z) + float(this->w) * float(other.w);
        return result;
    }

    T x, y, z, w;
};

template<typename T>
inline vec4<T> VECTOR_API operator+(vec4<T> a, vec4<T> b)
{
    return vec4(a.x + b.x, a.y + b.y, a.z + b.z);
}

template<typename T>
inline vec4<T>& VECTOR_API operator+=(vec4<T> &a, vec4<T> b)
{
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
    return a;
}

template<typename T>
inline vec4<T> VECTOR_API operator-(vec4<T> a, vec4<T> b)
{
    return vec4(a.x - b.x, a.y - b.y, a.z - b.z);
}

template<typename T>
inline vec4<T>& VECTOR_API operator-=(vec4<T> &a, vec4<T> b)
{
    a.x -= b.x;
    a.y -= b.y;
    a.z -= b.z;
    return a;
}

template<typename T>
inline vec4<T> VECTOR_API operator*(vec4<T> a, float f)
{
    return vec4(a.x * f, a.y * f, a.z * f);
}

template<typename T>
inline vec4<T>& VECTOR_API operator*=(vec4<T> &a, float f)
{
    a.x *= static_cast<T>(f);
    a.y *= static_cast<T>(f);
    a.z *= static_cast<T>(f);
    return a;
}

template<typename T>
inline vec4<T> VECTOR_API operator/(vec4<T> a, float f)
{
    return vec4(a.x / f, a.y / f, a.z / f);
}

template<typename T>
inline vec4<T>& VECTOR_API operator/=(vec4<T> &a, float f)
{
    a.x /= static_cast<T>(f);
    a.y /= static_cast<T>(f);
    a.z /= static_cast<T>(f);
    return a;
}
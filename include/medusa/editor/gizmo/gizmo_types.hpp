#pragma once

#include <cmath>
#include <algorithm>
#include "medusa/core/medusa_types.hpp"

namespace medusa {

struct GizmoVec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    constexpr GizmoVec3() = default;
    constexpr GizmoVec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    GizmoVec3 operator+(const GizmoVec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    GizmoVec3 operator-(const GizmoVec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    GizmoVec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    GizmoVec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    GizmoVec3& operator+=(const GizmoVec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    GizmoVec3& operator-=(const GizmoVec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    GizmoVec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
};

inline float dot(const GizmoVec3& a, const GizmoVec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline GizmoVec3 cross(const GizmoVec3& a, const GizmoVec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

inline float length(const GizmoVec3& v) {
    return std::sqrt(dot(v, v));
}

inline GizmoVec3 normalized(const GizmoVec3& v) {
    const float len = length(v);
    return len > 1e-6f ? v * (1.0f / len) : GizmoVec3{};
}

struct GizmoViewBasis {
    GizmoVec3 position;
    GizmoVec3 forward;
    GizmoVec3 right;
    GizmoVec3 up;
    float verticalFov = 0.68f;
};

} // namespace medusa

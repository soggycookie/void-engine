#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/trigonometric.hpp"

using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;

using Mat4 = glm::mat4;

inline float Rad(float deg) { return glm::radians(deg); }

inline Mat4 LookAt(const Vec3 &eye, const Vec3 &center, const Vec3 &up)
{
    return glm::lookAt(eye, center, up);
}

inline Mat4 Perspective(float fov, float aspect, float nearPlane,
                        float farPlane)
{
    return glm::perspective(fov, aspect, nearPlane, farPlane);
}

inline Mat4 Translate(const Mat4 &m, const Vec3 &t)
{
    return glm::translate(m, t);
}

inline Mat4 Scale(const Mat4 &m, const Vec3 &s) { return glm::scale(m, s); }

inline Mat4 Rotate(const Mat4 &m, float rad, const Vec3 &axis)
{
    return glm::rotate(m, rad, axis);
}

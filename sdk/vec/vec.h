#pragma once

struct Vec2 {
    float x, y;
};

struct Vec3 {
    float x, y, z;

    Vec3 operator+(const Vec3& other) const { return { x + other.x, y + other.y, z + other.z }; }
    Vec3 operator-(const Vec3& other) const { return { x - other.x, y - other.y, z - other.z }; }
    Vec3 operator*(float scalar)      const { return { x * scalar, y * scalar, z * scalar }; }
    float dot(const Vec3& other)      const { return x * other.x + y * other.y + z * other.z; }
};

struct Mat4 {
    float m[16];
};

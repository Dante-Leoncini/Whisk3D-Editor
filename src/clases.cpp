#include "clases.h"

// Instancia global del cache
Cache precalculado;

// --- Vec3 ---
Vec3::Vec3() : x(0), y(0), z(0) {}
Vec3::Vec3(float X, float Y, float Z) : x(X), y(Y), z(Z) {}

Vec3 operator+(const Vec3& a, const Vec3& b) { return Vec3(a.x + b.x, a.y + b.y, a.z + b.z); }
Vec3 operator-(const Vec3& a, const Vec3& b) { return Vec3(a.x - b.x, a.y - b.y, a.z - b.z); }
Vec3 operator*(const Vec3& a, float s) { return Vec3(a.x * s, a.y * s, a.z * s); }
Vec3 operator*(float s, const Vec3& a) { return a * s; }
float Dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 Cross(const Vec3& a, const Vec3& b) { return Vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
float Len(const Vec3& a) { return sqrtf(Dot(a, a)); }
Vec3 Normalize(const Vec3& a) { float L = Len(a); if (L <= 1e-8f) return Vec3(0, 0, 1); return a * (1.0f / L); }
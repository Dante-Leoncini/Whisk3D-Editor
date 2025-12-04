#ifndef CLASES_H
#define CLASES_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include <cmath>
#include <GL/gl.h>

const float DEG2RAD = 3.14159265358979323846f / 180.0f;

// --- PRECALCULADOS ---
struct Cache {
    float radX = 0;
    float radY = 0;
    float cosX = 1, sinX = 0;
    float cosY = 1, sinY = 0;
};
extern Cache precalculado;

// --- VEC3 ---
struct Vec3 {
    float x, y, z;
    Vec3();
    Vec3(float X, float Y, float Z);
};

// Operadores y funciones
Vec3 operator+(const Vec3& a, const Vec3& b);
Vec3 operator-(const Vec3& a, const Vec3& b);
Vec3 operator*(const Vec3& a, float s);
Vec3 operator*(float s, const Vec3& a);
float Dot(const Vec3& a, const Vec3& b);
Vec3 Cross(const Vec3& a, const Vec3& b);
float Len(const Vec3& a);
Vec3 Normalize(const Vec3& a);

// --- CHILDREN ---
class Children {
public:
    int Id = -1;
    GLfloat OriginalScaleX = 0;
    GLfloat OriginalScaleY = 0;
    GLfloat OriginalScaleZ = 0;
};

#endif
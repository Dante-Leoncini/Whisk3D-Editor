#ifndef VECTOR3_H
#define VECTOR3_H

#include <cmath>

class Vector3 {
    public:
        float x, y, z;

        Vector3() : x(0), y(0), z(0) {}
        Vector3(float px, float py, float pz) : x(px), y(py), z(pz) {}

        // Operadores básicos
        Vector3 operator+(const Vector3& v) const {
            return Vector3(x + v.x, y + v.y, z + v.z);
        }

        Vector3 operator-(const Vector3& v) const {
            return Vector3(x - v.x, y - v.y, z - v.z);
        }

        Vector3 operator*(float s) const {
            return Vector3(x * s, y * s, z * s);
        }

        Vector3& operator+=(const Vector3& v) {
            x += v.x;
            y += v.y;
            z += v.z;
            return *this;
        }

        Vector3& operator-=(const Vector3& v) {
            x -= v.x;
            y -= v.y;
            z -= v.z;
            return *this;
        }

        // Operador unario - (invierte el vector)
        Vector3 operator-() const {
            return Vector3(-x, -y, -z);
        }

        // Normalizar
        Vector3 Normalized() const {
            float len = Length();
            return (len == 0.0f) ? Vector3(0,0,0) : Vector3(x/len, y/len, z/len);
        }

        float Dot(const Vector3& v) const {
            return x*v.x + y*v.y + z*v.z;
        }

        float LengthSq() const {
            return x*x + y*y + z*z; // El cuadrado de la longitud
        }

        float Length() const {
            return std::sqrt(LengthSq()); // Usar LengthSq aquí también
        }

        static Vector3 Cross(const Vector3& a, const Vector3& b) {
            return Vector3(
                a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x
            );
        }
};

#endif
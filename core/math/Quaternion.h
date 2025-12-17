#ifndef QUATERNION_H
#define QUATERNION_H

#include <math.h>
#include "Vector3.h"
#include <GL/gl.h>

class Matrix4;  // ← forward declaration

struct Quaternion {
    float w = 0.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Quaternion(float W=1, float X=0, float Y=0, float Z=0)
        : w(W), x(X), y(Y), z(Z) {}

    static Quaternion FromAxisAngle(float ax, float ay, float az, float angleDeg) {
        float rad = angleDeg * M_PI / 180.0f;
        float s = sinf(rad * 0.5f);
        float c = cosf(rad * 0.5f);
        return Quaternion(c, ax * s, ay * s, az * s);
    }

    //si queres mandarle un vector3
    static Quaternion FromAxisAngle(const Vector3& axis, float angleDeg) {
        return FromAxisAngle(axis.x, axis.y, axis.z, angleDeg);
    }

    Quaternion operator*(const Quaternion& b) const {
        return Quaternion(
            w*b.w - x*b.x - y*b.y - z*b.z,
            w*b.x + x*b.w + y*b.z - z*b.y,
            w*b.y - x*b.z + y*b.w + z*b.x,
            w*b.z + x*b.y - y*b.x + z*b.w
        );
    }

    static Quaternion Slerp(
        const Quaternion& a,
        const Quaternion& b,
        float t
    );

    static Quaternion FromEuler(float pitchDeg, float yawDeg, float rollDeg);

    Quaternion Inverted() const {
        return Quaternion(w, -x, -y, -z);
    }

    // Rota un vector usando q * v * q⁻¹
    Vector3 operator*(const Vector3& v) const {
        // q * v
        Quaternion qv(0, v.x, v.y, v.z);

        // q * v * q^-1
        Quaternion qConj(w, -x, -y, -z);
        Quaternion r = (*this) * qv * qConj;

        return Vector3(r.x, r.y, r.z);
    }

    void normalize() {
        float mag = sqrtf(w*w + x*x + y*y + z*z);
        if (mag > 0.0f) {
            float inv = 1.0f / mag;
            w *= inv;
            x *= inv;
            y *= inv;
            z *= inv;
        }
    }

    Quaternion Normalized() const {
        Quaternion q = *this;
        q.normalize();
        return q;
    }

    Matrix4 ToMatrix() const;

    void ToMatrix(float m[16]) const {
        float xx = x*x, yy = y*y, zz = z*z;
        float xy = x*y, xz = x*z, yz = y*z;
        float wx = w*x, wy = w*y, wz = w*z;

        m[0]  = 1 - 2*(yy + zz);
        m[1]  = 2*(xy + wz);
        m[2]  = 2*(xz - wy);
        m[3]  = 0;

        m[4]  = 2*(xy - wz);
        m[5]  = 1 - 2*(xx + zz);
        m[6]  = 2*(yz + wx);
        m[7]  = 0;

        m[8]  = 2*(xz + wy);
        m[9]  = 2*(yz - wx);
        m[10] = 1 - 2*(xx + yy);
        m[11] = 0;

        m[12] = m[13] = m[14] = 0;
        m[15] = 1;
    }

    Vector3 ToEulerYXZ() const;

    static Quaternion FromMatrix(const Matrix4& m);
    static Quaternion FromDirection(const Vector3& direction, const Vector3& worldUp);
};

// out = A * B  (column-major, como espera glMultMatrixf)
void MultiplyMatrix(GLfloat out[16], const GLfloat A[16], const GLfloat B[16]);

#endif
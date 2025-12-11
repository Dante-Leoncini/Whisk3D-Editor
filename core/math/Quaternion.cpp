#include "Quaternion.h"
#include "Matrix4.h"

Matrix4 Quaternion::ToMatrix() const {
    Matrix4 m;
    m.Identity();

    float xx = x * x;
    float yy = y * y;
    float zz = z * z;
    float xy = x * y;
    float xz = x * z;
    float yz = y * z;
    float wx = w * x;
    float wy = w * y;
    float wz = w * z;

    m.m[0]  = 1.0f - 2.0f * (yy + zz);
    m.m[1]  = 2.0f * (xy + wz);
    m.m[2]  = 2.0f * (xz - wy);

    m.m[4]  = 2.0f * (xy - wz);
    m.m[5]  = 1.0f - 2.0f * (xx + zz);
    m.m[6]  = 2.0f * (yz + wx);

    m.m[8]  = 2.0f * (xz + wy);
    m.m[9]  = 2.0f * (yz - wx);
    m.m[10] = 1.0f - 2.0f * (xx + yy);

    return m;
}

Quaternion Quaternion::FromMatrix(const Matrix4& m) {
    Quaternion q;
    float trace = m.m[0] + m.m[5] + m.m[10];

    if (trace > 0.0f) {
        float s = sqrtf(trace + 1.0f) * 2.0f;
        q.w = 0.25f * s;
        // La rama 'trace' está bien
        q.x = (m.m[6] - m.m[9]) / s;
        q.y = (m.m[8] - m.m[2]) / s;
        q.z = (m.m[1] - m.m[4]) / s;
    }
    // Rama 1: m.m[0] > m.m[5] y m.m[0] > m.m[10] (Máxima rotación alrededor de X)
    else if (m.m[0] > m.m[5] && m.m[0] > m.m[10]) {
        float s = sqrtf(1.0f + m.m[0] - m.m[5] - m.m[10]) * 2.0f;
        // CORRECCIÓN: Invertir signo de W para estabilidad
        q.w = (m.m[6] - m.m[9]) / s; // m12 - m21
        q.x = 0.25f * s;
        q.y = (m.m[1]  + m.m[4]) / s;
        q.z = (m.m[2]  + m.m[8]) / s;
    }
    // Rama 2: m.m[5] > m.m[10] (Máxima rotación alrededor de Y)
    else if (m.m[5] > m.m[10]) {
        float s = sqrtf(1.0f + m.m[5] - m.m[0] - m.m[10]) * 2.0f;
        // CORRECCIÓN: Invertir signo de W para estabilidad
        q.w = (m.m[8] - m.m[2]) / s; // m20 - m02
        q.x = (m.m[1]  + m.m[4]) / s;
        q.y = 0.25f * s;
        q.z = (m.m[6]  + m.m[9]) / s;
    }
    // Rama 3: (Máxima rotación alrededor de Z)
    else {
        float s = sqrtf(1.0f + m.m[10] - m.m[0] - m.m[5]) * 2.0f;
        // CORRECCIÓN: Invertir signo de W para estabilidad
        q.w = (m.m[4] - m.m[1]) / s; // m10 - m01
        q.x = (m.m[2]  + m.m[8]) / s;
        q.y = (m.m[6]  + m.m[9]) / s;
        q.z = 0.25f * s;
    }

    return q.Normalized();
}

Quaternion Quaternion::FromEuler(float pitchDeg, float yawDeg, float rollDeg){
    float xRad = pitchDeg * (M_PI / 180.0f);
    float yRad = yawDeg  * (M_PI / 180.0f);
    float zRad = rollDeg * (M_PI / 180.0f);

    float cx = cosf(xRad * 0.5f);
    float sx = sinf(xRad * 0.5f);

    float cy = cosf(yRad * 0.5f);
    float sy = sinf(yRad * 0.5f);

    float cz = cosf(zRad * 0.5f);
    float sz = sinf(zRad * 0.5f);

    Quaternion q;
    q.w = cx*cy*cz + sx*sy*sz;
    q.x = sx*cy*cz - cx*sy*sz;
    q.y = cx*sy*cz + sx*cy*sz;
    q.z = cx*cy*sz - sx*sy*cz;

    return q;
}

// out = A * B  (column-major, como espera glMultMatrixf)
void MultiplyMatrix(GLfloat out[16], const GLfloat A[16], const GLfloat B[16]) {
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            GLfloat sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += A[k*4 + r] * B[c*4 + k]; // acceso por columnas
            }
            out[c*4 + r] = sum;
        }
    }
}
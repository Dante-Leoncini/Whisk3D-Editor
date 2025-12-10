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
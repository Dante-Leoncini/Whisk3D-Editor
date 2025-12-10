#include "Matrix4.h"

Matrix4 Matrix4::operator*(const Matrix4& B) const {
    Matrix4 result;

    // OpenGL column-major multiplication: result = A * B
    // result[i][j] = sum_k A[k][j] * B[i][k]

    const float* A = this->m;
    const float* C = B.m;
    float* R = result.m;

    for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {

            R[col * 4 + row] =
                A[0 * 4 + row] * C[col * 4 + 0] +
                A[1 * 4 + row] * C[col * 4 + 1] +
                A[2 * 4 + row] * C[col * 4 + 2] +
                A[3 * 4 + row] * C[col * 4 + 3];
        }
    }

    return result;
}

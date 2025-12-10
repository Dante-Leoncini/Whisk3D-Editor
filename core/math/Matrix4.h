#ifndef MATRIX4_H
#define MATRIX4_H

#include "Vector3.h"

class Matrix4 {
    public:
        float m[16];

        void Identity() {
            for(int i=0;i<16;i++) m[i] = 0;
            m[0] = m[5] = m[10] = m[15] = 1;
        }

        // Multiplicación matriz × vector (asumiendo w = 1)
        Vector3 operator*(const Vector3& v) const {
            return Vector3(
                m[0] * v.x + m[4] * v.y + m[8]  * v.z + m[12],
                m[1] * v.x + m[5] * v.y + m[9]  * v.z + m[13],
                m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14]
            );
        }

        // Multiplicación de matrices
        Matrix4 operator*(const Matrix4& B) const;
};

#endif
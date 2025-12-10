#ifndef MATRIX4_H
#define MATRIX4_H

class Matrix4 {
    public:
        float m[16];

        void Identity() {
            for(int i=0;i<16;i++) m[i] = 0;
            m[0] = m[5] = m[10] = m[15] = 1;
        }

        // Multiplicación de matrices
        Matrix4 operator*(const Matrix4& B) const;
};

#endif
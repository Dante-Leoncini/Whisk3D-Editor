#include "Quaternion.h"
#include "Matrix4.h"

Quaternion Quaternion::Slerp(
    const Quaternion& a,
    const Quaternion& b,
    float t
) {
    // Clamp
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;

    // Producto punto
    float dot = a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;

    Quaternion end = b;

    // Si el dot es negativo, invertimos uno para tomar
    // el camino corto (MUY IMPORTANTE)
    if (dot < 0.0f) {
        dot = -dot;
        end.x = -end.x;
        end.y = -end.y;
        end.z = -end.z;
        end.w = -end.w;
    }

    // Si están muy cerca, usar LERP normal (más barato)
    const float DOT_THRESHOLD = 0.9995f;
    if (dot > DOT_THRESHOLD) {
        Quaternion result;
        result.x = a.x + t * (end.x - a.x);
        result.y = a.y + t * (end.y - a.y);
        result.z = a.z + t * (end.z - a.z);
        result.w = a.w + t * (end.w - a.w);
        return result.Normalized();
    }

    // SLERP real
    float theta0 = acosf(dot);
    float theta  = theta0 * t;

    float sinTheta  = sinf(theta);
    float sinTheta0 = sinf(theta0);

    float s0 = cosf(theta) - dot * sinTheta / sinTheta0;
    float s1 = sinTheta / sinTheta0;

    Quaternion result;
    result.x = (a.x * s0) + (end.x * s1);
    result.y = (a.y * s0) + (end.y * s1);
    result.z = (a.z * s0) + (end.z * s1);
    result.w = (a.w * s0) + (end.w * s1);

    return result;
}

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

// Método de ayuda dentro de tu clase Quaternion o utilidades:
Quaternion Quaternion::FromDirection(const Vector3& direction, const Vector3& worldUp) {
    
    // 1. Vector de Dirección (que apunta HASTA DONDE SE MUEVE el personaje)
    Vector3 moveDirection = direction.Normalized();

    // 2. CORRECCIÓN CLAVE: El eje Z local del personaje debe ser opuesto al movimiento.
    Vector3 forward = -moveDirection; // Eje Z local del personaje
    
    // 3. Eje Right (X local)
    // El orden Cross(WorldUp, Forward) asegura que el Roll sea cero.
    // Usamos el 'forward' ya invertido.
    Vector3 right = Vector3::Cross(worldUp, forward).Normalized();
    
    // 4. Eje Up (Y local)
    // Se calcula para ortogonalidad perfecta.
    Vector3 up = Vector3::Cross(forward, right).Normalized();
    
    // 5. Construir Matriz de Orientación (Positivo [Right | Up | Forward])
    Matrix4 M;
    M.Identity();

    // Columna X = Right
    M.m[0] = right.x; M.m[1] = right.y; M.m[2] = right.z;
    
    // Columna Y = Up
    M.m[4] = up.x; M.m[5] = up.y; M.m[6] = up.z;
    
    // Columna Z = Forward (es el vector -direction)
    M.m[8] = forward.x; M.m[9] = forward.y; M.m[10] = forward.z;

    return Quaternion::FromMatrix(M);
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

Vector3 Quaternion::ToEulerYXZ() const {
    Vector3 euler;

    // Pitch (X)
    float sinp = 2.0f * (w * x - y * z);
    if (fabs(sinp) >= 1.0f)
        euler.x = copysignf(M_PI / 2.0f, sinp);
    else
        euler.x = asinf(sinp);

    // Yaw (Y)
    float siny = 2.0f * (w * y + x * z);
    float cosy = 1.0f - 2.0f * (x * x + y * y);
    euler.y = atan2f(siny, cosy);

    // Roll (Z)
    float sinr = 2.0f * (w * z + x * y);
    float cosr = 1.0f - 2.0f * (x * x + z * z);
    euler.z = atan2f(sinr, cosr);

    // RAD → DEG
    const float RAD2DEG = 57.2957795f;
    euler.x *= RAD2DEG;
    euler.y *= RAD2DEG;
    euler.z *= RAD2DEG;

    return euler;
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
#include "GLES_Android_helpers.h"
#include <cmath>

#ifdef __ANDROID__
    GLfixed COLOR_CONVERT_FIXED(float x) {
        return static_cast<GLfixed>(x * 65536.0f);
    }

    void gluPerspectivef(float fovY, float aspect, float zNear, float zFar) {
        float fH = tanf(fovY / 360.0f * 3.14159265f) * zNear;
        float fW = fH * aspect;
        glFrustumf(-fW, fW, -fH, fH, zNear, zFar);
    }
#endif
    void SetColor(const ColorType c[4]) {
#ifdef __ANDROID__
    glColor4f(c[0], c[1], c[2], c[3]);
#else
    glColor4fv(c);
#endif
}

std::string getParentPath(const std::string &filepath) {
    size_t pos = filepath.find_last_of("/\\");
    if (pos == std::string::npos) return "";
    return filepath.substr(0, pos);
}

bool fileExists(const std::string &filename) {
    FILE *file = fopen(filename.c_str(), "r");
    if (file) {
        fclose(file);
        return true;
    }
    return false;
}

unsigned char sat(double v) {
    v *= 255.0;
    if (v < 0.0) v = 0.0;
    if (v > 255.0) v = 255.0;
    return (unsigned char)v;
}

signed char cv(double v) {
    v = ((v + 1.0) / 2.0) * 255.0 - 128.0;
    if (v < -128.0) v = -128.0;
    if (v > 127.0) v = 127.0;
    return (signed char)v;
}
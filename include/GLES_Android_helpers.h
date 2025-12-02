#include <cstdio>  // para fopen, fclose

#ifdef ANDROID
    // OpenGL ES 1.1 usa la extensión OES
    #define GL_POINT_SPRITE      GL_POINT_SPRITE_OES
    #define GL_COORD_REPLACE     GL_COORD_REPLACE_OES

    #define glOrtho glOrthof

    inline void gluPerspectivef(float fovY, float aspect, float zNear, float zFar) {
        float fH = tanf(fovY / 360.0f * 3.14159265f) * zNear;
        float fW = fH * aspect;
        glFrustumf(-fW, fW, -fH, fH, zNear, zFar);
    }

	//typedef GLfixed ColorType;
	typedef GLfloat ColorType;
	#define COLOR_CONVERT(x) (x)
	//#define COLOR_CONVERT(x) ((GLfixed)((x)*65536))

	void SetColor(const ColorType c[4]) {
		//glColor4x(c[0], c[1], c[2], c[3]);
        glColor4f(c[0], c[1], c[2], c[3]);	
	}
#else
	typedef GLfloat ColorType;
	#define COLOR_CONVERT(x) (x)
	void SetColor(const ColorType c[4]) {
		glColor4fv(c);
	}
#endif

inline GLfixed COLOR_CONVERT_FIXED(float x) {
    return static_cast<GLfixed>(x * 65536.0f);
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

inline unsigned char sat(double v) {
    v *= 255.0;
    if (v < 0.0) v = 0.0;
    if (v > 255.0) v = 255.0;
    return (unsigned char)v;
}

inline signed char cv(double v) {
    v = ((v + 1.0) / 2.0) * 255.0 - 128.0;
    if (v < -128.0) v = -128.0;
    if (v > 127.0) v = 127.0;
    return (signed char)v;
}
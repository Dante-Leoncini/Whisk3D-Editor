#ifndef GLESANDROIDHELPERS_H
#define GLESANDROIDHELPERS_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include <string>
#include <cstdio>   // fopen, fclose

#ifdef __ANDROID__
#include <GLES/gl.h>
#include <GLES/glext.h>

// OpenGL ES 1.1 usa la extensión OES
#define GL_POINT_SPRITE      GL_POINT_SPRITE_OES
#define GL_COORD_REPLACE     GL_COORD_REPLACE_OES
#define glOrtho glOrthof

#else
#include <GL/gl.h>
#include <GL/glext.h>
#endif

// Tipo de color común
typedef GLfloat ColorType;
#define COLOR_CONVERT(x) (x)

// Declaraciones de funciones
void SetColor(const ColorType c[4]);
std::string getParentPath(const std::string &filepath);
bool fileExists(const std::string &filename);

#ifdef __ANDROID__
GLfixed COLOR_CONVERT_FIXED(float x);
void gluPerspectivef(float fovY, float aspect, float zNear, float zFar);
#endif

unsigned char sat(double v);
signed char cv(double v);

#endif // GLESANDROIDHELPERS_H
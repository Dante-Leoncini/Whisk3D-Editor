#pragma once

typedef float ColorType;

void w3dSetColor(const ColorType c[4]);

#ifdef W3D_OPENGL
#include "opengl/w3dOpenGL.h"
#endif
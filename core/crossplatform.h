// compat.h
#pragma once

// ===============================
// Windows
// ===============================

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#endif

// ===============================
// STL
// ===============================

//#include <cmath>
//#include <algorithm>
//#include <string>
//#include <cstdio>

// ===============================
// Math defines
// ===============================

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ===============================
// OpenGL missing defines
// ===============================

//#ifndef GL_POINT_SPRITE
//#define GL_POINT_SPRITE 0x8861
//#endif

//#ifndef GL_COORD_REPLACE
//#define GL_COORD_REPLACE 0x8862
//#endif
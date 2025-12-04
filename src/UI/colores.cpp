#include "UI/colores.h"

// ---- FLOAT COLORS (desktop) ----
#ifndef __ANDROID__
float ListaColores[17][4] = {
    { 0.22f, 0.28f, 0.25f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f },
    { 0.91f, 0.50f, 0.98f, 1.0f },
    { 0.48f, 0.30f, 1.0f,  1.0f },
    { 0.0f,  0.0f,  0.0f,  1.0f },
    { 0.12f, 0.12f, 0.12f, 1.0f },
    { 0.94f, 0.59f, 0.17f, 0.25f },
    { 0.22f, 0.22f, 0.22f, 1.0f },
    { 0.0f,  0.0f,  0.0f, 0.25f },
    { 0.757f, 0.757f, 0.757f, 1.0f },
    { 0.757f, 0.757f, 0.757f, 1.0f },
    { 0.22f, 0.22f, 0.22f, 1.0f },
    { 0.56f, 0.23f, 0.28f, 1.0f },
    { 0.38f, 0.53f, 0.15f, 1.0f },
    { 0.88f, 0.48f, 0.54f, 1.0f },
    { 0.65f, 0.81f, 0.38f, 1.0f },
    { 0.46f, 0.67f, 0.89f, 1.0f }
};
#else
// ---- FIXED COLORS (Android) ----
GLfixed ListaColoresX[17][4] = {
    { COLOR_CONVERT_FIXED(0.22), COLOR_CONVERT_FIXED(0.28), COLOR_CONVERT_FIXED(0.25), COLOR_CONVERT_FIXED(1.0) },
    { COLOR_CONVERT_FIXED(1.0),  COLOR_CONVERT_FIXED(1.0),  COLOR_CONVERT_FIXED(1.0),  COLOR_CONVERT_FIXED(1.0) },
    { COLOR_CONVERT_FIXED(0.91), COLOR_CONVERT_FIXED(0.50), COLOR_CONVERT_FIXED(0.98), COLOR_CONVERT_FIXED(1.0) },
    // ... resto igual ...
};
#endif

// ---- UBYTE COLORS ----
GLubyte ListaColoresUbyte[17][4] = {
    { (GLubyte)(0.22*255), (GLubyte)(0.28*255), (GLubyte)(0.25*255), 255 },
    { 255, 255, 255, 255 },
    { (GLubyte)(0.91*255), (GLubyte)(0.50*255), (GLubyte)(0.98*255), 255 },
    { (GLubyte)(0.48*255), (GLubyte)(0.30*255), 255, 255 },
    { 0, 0, 0, 255 },
    { (GLubyte)(0.12*255), (GLubyte)(0.12*255), (GLubyte)(0.12*255), 255 },
    { (GLubyte)(0.94*255), (GLubyte)(0.59*255), (GLubyte)(0.17*255), (GLubyte)(0.25*255) },
    { (GLubyte)(0.22*255), (GLubyte)(0.22*255), (GLubyte)(0.22*255), 255 },
    { 0, 0, 0, (GLubyte)(0.25*255) },
    { (GLubyte)(0.757*255), (GLubyte)(0.757*255), (GLubyte)(0.757*255), 255 },
    { (GLubyte)(0.757*255), (GLubyte)(0.757*255), (GLubyte)(0.757*255), 255 },
    { (GLubyte)(0.22*255), (GLubyte)(0.22*255), (GLubyte)(0.22*255), 255 },
    { (GLubyte)(0.56*255), (GLubyte)(0.23*255), (GLubyte)(0.28*255), 255 },
    { (GLubyte)(0.38*255), (GLubyte)(0.53*255), (GLubyte)(0.15*255), 255 },
    { (GLubyte)(0.88*255), (GLubyte)(0.48*255), (GLubyte)(0.54*255), 255 },
    { (GLubyte)(0.65*255), (GLubyte)(0.81*255), (GLubyte)(0.38*255), 255 },
    { (GLubyte)(0.46*255), (GLubyte)(0.67*255), (GLubyte)(0.89*255), 255 }
};
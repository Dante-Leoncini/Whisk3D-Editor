#ifndef WITHBORDER_H
#define WITHBORDER_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include <GL/gl.h>
#include <SDL2/SDL.h>

#include "ViewPorts.h"

// Funciones globales
void CalcBorderUV(int texW, int texH);

// Variables UV/indices
extern GLubyte indicesBorder[];
extern GLfloat bourderUV[32];

// -----------------------------
// WithBorder
// -----------------------------
class WithBorder {
    public:
        // ------------------ Mesh ------------------
        GLshort borderMesh[32] = { 
            0,0,   6,0,   12,0,   18,0,
            0,6,   6,6,   12,6,   18,6,
            0,12,  6,12,  12,12,  18,12,
            0,18,  6,18,  12,18,  18,18
        };

        // ------------------ Funciones ------------------
        void DibujarBordes(ViewportBase* current);
        void ResizeBorder(int width, int height);
};

#endif
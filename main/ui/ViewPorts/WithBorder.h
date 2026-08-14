#ifndef WITHBORDER_H
#define WITHBORDER_H

#ifdef _WIN32
#ifndef W3D_SYMBIAN
    #include <windows.h>
#endif
#endif

#ifdef W3D_SYMBIAN
    #include <GLES/gl.h>
#else
    #include <GL/gl.h>
    #include <SDL2/SDL.h>
#endif

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
        GLshort borderMesh[32];
#ifdef W3D_SYMBIAN
        // el driver del N95 no dibuja glDrawElements en la fase 2D: la
        // version GLES expande los indices a 48 vertices sueltos (float)
        GLfloat borderMeshExp[96];
#endif

        WithBorder(); // C++03: el mesh default se arma en el constructor

        // ------------------ Funciones ------------------
        void DibujarBordes(ViewportBase* current);
        void ResizeBorder(int width, int height);
};

#endif
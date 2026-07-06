#ifndef SCROLLBAR_H
#define SCROLLBAR_H

#ifdef _WIN32
#ifndef W3D_SYMBIAN
    #include <windows.h>
#endif
#endif

#ifdef W3D_SYMBIAN
    #include <GLES/gl.h>
    extern bool middleMouseDown; // shims de variables.h (glesdraw.cpp)
    extern bool MouseWheel;
#else
    #include <GL/gl.h>
    #include <SDL2/SDL.h>
#endif

#include "ViewPorts.h"

// Funciones globales
void CalcScrollUV(int texW, int texH);

// Variables UV/indices
extern GLubyte indicesScrollbar[];
extern GLfloat ScrollbarUV[16];
extern GLfloat ScrollbarBigUV[16];

// -----------------------------
// Scrollable
// -----------------------------
class Scrollable {
    public:
        int PosX, PosY;          // (inicializados en el ctor: C++03)
        int MaxPosX, MaxPosY;
        bool scrollX, scrollY;
        bool mouseOverScrollY, mouseOverScrollX;
        bool mouseOverScrollYpress, mouseOverScrollXpress;

        int scrollTopOffset; // alto reservado arriba (barra de botones)
        float scrollPosFactor, scrollDragFactor;
        float scrollPosFactorX, scrollDragFactorX;

        ViewportBase* viewPortActive;

        GLshort scrollVerticalMesh[16];
        GLshort scrollVerticalBigMesh[16];
        GLshort scrollHorizontalMesh[16];
        GLshort scrollHorizontalBigMesh[16];

        // ------------------ Constructor ------------------
        Scrollable();

        // ------------------ Funciones ------------------
        void ScrollY(int dy);
        void ScrollX(int dx);
        void ScrollByTouch(int dx, int dy); // touch: arrastrar el CONTENIDO 1:1 (sin tocar la barra)
        void ScrollMouseOver(ViewportBase* current, int mx, int my);
        void ResizeScrollbar(int width, int height, int MaxX, int MaxY,
                             int topOffset = 0);
        void DibujarScrollbar(ViewportBase* current);
};

#endif
#ifndef SCROLLBAR_H
#define SCROLLBAR_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include <GL/gl.h>
#include <SDL2/SDL.h>

//#include "variables.h"
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
        int PosX = 0;
        int PosY = 0;
        int MaxPosX = 100;
        int MaxPosY = 0;
        bool scrollX = false;
        bool scrollY = false;
        bool mouseOverScrollY = false;
        bool mouseOverScrollX = false;
        bool mouseOverScrollYpress = false;
        bool mouseOverScrollXpress = false;

        float scrollPosFactor = 0.0f;
        float scrollDragFactor = 0.0f;
        float scrollPosFactorX = 0.0f;
        float scrollDragFactorX = 0.0f;

        ViewportBase* viewPortActive = nullptr;

        GLshort scrollVerticalMesh[16] = { 
            0,0,   6,0,   12,0,   18,0,
            0,6,   6,6,   12,6,   18,6
        };

        GLshort scrollVerticalBigMesh[16] = { 
            0,0,   6,0,   12,0,   18,0,
            0,6,   6,6,   12,6,   18,6
        };

        GLshort scrollHorizontalMesh[16] = { 
            0,0,   6,0,   12,0,   18,0,
            0,6,   6,6,   12,6,   18,6
        };

        GLshort scrollHorizontalBigMesh[16] = { 
            0,0,   6,0,   12,0,   18,0,
            0,6,   6,6,   12,6,   18,6
        };

        // ------------------ Constructor ------------------
        Scrollable();

        // ------------------ Funciones ------------------
        void ScrollY(int dy);
        void ScrollX(int dx);
        void ScrollMouseOver(ViewportBase* current, int mx, int my);
        void ResizeScrollbar(int width, int height, int MaxX, int MaxY);
        void DibujarScrollbar(ViewportBase* current);
};

#endif
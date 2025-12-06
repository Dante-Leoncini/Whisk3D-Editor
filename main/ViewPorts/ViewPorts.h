#ifndef VIEWPORTS_H
#define VIEWPORTS_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include <GL/gl.h>
#include <SDL2/SDL.h>
#include <iostream>

#include "UI/colores.h"
#include "UI/UI.h"
#include "UI/icons.h"
#include "variables.h"
#include "objects/Objects.h"

// Enum de vistas
enum class View {
    ViewPort3D,
    Outliner,
    Properties,
    UVeditor,
    Timeline,
    GraphEditor,
    Row,
    Column
};

// Adelantos de clases
class ViewportBase;
class WithBorder;
class ViewportRow;
class ViewportColumn;

// Variables globales
extern ViewportBase* viewPortActive;
extern ViewportBase* rootViewport;

// Funciones globales
void CalcBorderUV(int texW, int texH);
ViewportBase* FindViewportUnderMouse(ViewportBase* vp, int mx, int my);
void SetGlobalScale(int scale);
void CheckWarpMouseInViewport(int mx, int my, const ViewportBase* vp);

// Variables UV/indices
extern GLubyte indicesBorder[];
extern GLfloat bourderUV[32];

// -----------------------------
// Clase base de Viewport
// -----------------------------
class ViewportBase {
    public:
        int x = 0, y = 0;
        int width = 0, height = 0;
        ViewportBase* parent = nullptr;

        ViewportBase();
        
        // Métodos virtuales para que cada vista defina su comportamiento
        virtual ~ViewportBase();

        virtual bool Contains(int mx, int my) const;
        virtual bool isLeaf() const;

        virtual void event_mouse_motion(int mx, int my);
        virtual void button_left();
        virtual void event_key_down(SDL_Event &e);
        virtual void event_key_up(SDL_Event &e);

        virtual void event_mouse_wheel(SDL_Event &e);
        virtual void mouse_button_up(SDL_Event &e);
        virtual void event_finger_motion(float pinch);

        virtual void Render() = 0;
        virtual void Resize(int newW, int newH);
};

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

// -----------------------------
// ViewportRow
// -----------------------------
class ViewportRow : public ViewportBase {
    public:
        ViewportBase* childA = nullptr;
        ViewportBase* childB = nullptr;
        float splitFrac = 0.5f;       

        // ------------------ Constructor / Destructor ------------------
        ViewportRow(ViewportBase* a = nullptr, ViewportBase* b = nullptr, float frac = 0.5f);
        ~ViewportRow();

        // ------------------ Funciones override ------------------
        bool isLeaf() const override;

        void Resize(int newW, int newH) override;
        void Render() override;
        void button_left() override;
        void event_mouse_motion(int mx, int my) override;

        // ------------------ Funciones propias ------------------
        void SetSizeChildrens(int move);
};

// -----------------------------
// ViewportColumn
// -----------------------------
class ViewportColumn : public ViewportBase {
    public:
        ViewportBase* childA = nullptr;
        ViewportBase* childB = nullptr;
        float splitFrac = 0.5f;    

        ViewportColumn(ViewportBase* a = nullptr, ViewportBase* b = nullptr, float frac = 0.5f);
        ~ViewportColumn();

        bool isLeaf() const override;

        void Resize(int newW, int newH) override;
        void SetSizeChildrens(int move);
        void Render() override;

        void button_left() override;
        void event_mouse_motion(int mx, int my) override;
};

#endif
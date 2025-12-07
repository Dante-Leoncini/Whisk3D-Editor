#ifndef VIEWPORT3D_H
#define VIEWPORT3D_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include <vector>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glext.h> 
#include <SDL2/SDL.h>

#include "variables.h"
#include "sdl_key_compat.h"
#include "objects/Light.h"
#include "objects/Gamepad.h"
#include "objects/Camera.h"
#include "objects/Scene.h"
#include "objects/ObjectMode.h"
#include "ViewPorts/ViewPorts.h"
#include "GeometriaUI/Floor.h"
#include "render/render.h"
#include "lectura-escritura.h"
#include "WithBorder.h"

extern GLfloat LastRotX;
extern GLfloat LastRotY;
extern GLfloat LastPivotX;
extern GLfloat LastPivotY;
extern GLfloat LastPivotZ;

class Viewport3D : public ViewportBase, public WithBorder {
    public:
        bool orthographic;
        bool ViewFromCameraActive;
        bool showOverlays;
        bool ShowUi;
        bool showFloor;
        bool showYaxis;
        bool showXaxis;
        bool CameraToView;
        bool showOrigins;
        bool show3DCursor;
        bool ShowRelantionshipsLines;
        RenderType view;

        GLfloat nearClip;
        GLfloat farClip;
        GLfloat cameraDistance;
        GLfloat aspect;
        GLfloat posX, posY, posZ;
        GLfloat rotX, rotY;
        GLfloat PivotX, PivotY, PivotZ;

        Viewport3D();

        virtual ~Viewport3D();

        void ReloadLights();
        void ChangeViewType();
        void Resize(int newW, int newH) override;
        void SetShowOverlays(bool valor);
        void Render() override;

        void RenderFloor();
        void RenderAllAxisTransform();
        void RenderOverlay();  

        void RenderRelantionshipsLines();
        void Render3Dcursor();
        void RenderUI();
        void EnfocarObject();
        void RecalcViewPos();
        void SetViewpoint(int opcion);
        void RestaurarViewport();
        void ChangePerspective();
        void SetCursor3D();
        void Aceptar();
        void button_left() override; 
        void mouse_button_up(SDL_Event &e) override; 
        void event_mouse_wheel(SDL_Event &e) override; 
        void event_mouse_motion(int mx, int my) override; 
        void TeclaDerecha();
        void TeclaIzquierda();
        void TeclaArriba();
        void TeclaAbajo();
        void UpdatePrecalculos();
        void ClickD();
        void ClickE();
        void ClickQ();
        void SetEje(int eje);
        void SetViewFromCameraActive(bool value);
        void event_key_down(SDL_Event &e) override;
        void event_key_up(SDL_Event &e) override; 
        void key_down_return();
};

#endif
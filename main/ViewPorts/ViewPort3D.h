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
#include "math/Vector3.h"
#include "math/Matrix4.h"
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
extern GLfloat LastRotZ;
extern GLfloat LastPivotX;
extern GLfloat LastPivotY;
extern GLfloat LastPivotZ;

class Viewport3D : public ViewportBase, public WithBorder {
    public:
        bool orthographic = false;
        bool ViewFromCameraActive = false;
        bool showOverlays = true;
        bool ShowUi = true;
        bool showFloor = true;
        bool showYaxis = true;
        bool showXaxis = true;
        bool CameraToView = false;
        bool showOrigins = true;
        bool show3DCursor = true;
        bool ShowRelantionshipsLines = true;
        RenderType view = RenderType::MaterialPreview;

        GLfloat nearClip = 0.01f;
        GLfloat farClip = 1000.0f;
        GLfloat aspect = 1.0f;

        // --- Rotación orbital / libre ---
        Quaternion viewRot = Quaternion::FromEuler(-30.0f, -23.0f, 0.0f); // rotación de la cámara 
        Vector3    viewPos;                  // posición de la cámara precalculada
        Vector3    pivot;                    // punto de interés a orbitar
        float      orbitDistance = 10.0f;    // distancia al pivote (zoom) 

        /*GLfloat LastPosX = 0;
        GLfloat LastPosY = 0;  
        GLfloat LastPosZ = 0;  
        GLfloat LastZoom = 0;*/

        Viewport3D(Vector3 pos = Vector3(0,0,0));

        virtual ~Viewport3D();

        void ReloadLights();
        void ChangeViewType();
        void Resize(int newW, int newH) override;
        void SetShowOverlays(bool valor);
        void Render() override;

        void RenderFloor();
        void RenderAllAxisTransform();
        void RenderOverlay();  

        void UpdateViewOrbit();
        void RotateOrbit();
        void RollOrbit(float angleDeg);
        void RecalcOrbitPosition();
        void Pan();
        void Zoom(float delta);

        void RenderRelantionshipsLines();
        void Render3Dcursor();
        void RenderUI();
        void EnfocarObject();
        bool RecalcViewPos();
        void SetViewpoint(Viewpoint value);
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
        void SetEje(int eje);
        void SetViewFromCameraActive(bool value);
        void event_key_down(SDL_Event &e) override;
        void event_key_up(SDL_Event &e) override; 
        void key_down_return();
};

#endif
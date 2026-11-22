// ============================================================================
//  ClipRegion.cpp — ver ClipRegion.h
// ============================================================================
#include "objects/ClipRegion.h"
#include "objects/Camera.h"
#include "objects/CameraBase.h"
#include "objects/Light.h"          // W3dLightsPrePass: las luces se recalculan con la vista de la camara propia
#include "graphics/w3dGraphics.h"
#include <math.h>

namespace gfx = w3dEngine;

ClearZ::ClearZ(Object* parent, Vector3 pos) : Object(parent, "ClearZ", pos), active(true) {}

void ClearZ::RenderObject() {
    if (!active) return;
    // el Clear respeta el scissor: adentro de un Recorte solo borra su rectangulo
    gfx::DepthMask(true);
    gfx::Clear(gfx::DepthBuffer);
}

ClipRegion::ClipRegion(Object* parent, Vector3 pos)
    : Object(parent, "Clip", pos), active(true), x(0.0f), y(0.0f), width(1.0f), height(1.0f),
      clearZ(false), background(false) {
    color[0] = color[1] = color[2] = 0.0f; color[3] = 1.0f;
}

static int Round(float v) { return (int)floorf(v + 0.5f); }

void ClipRegion::RenderChildren() {
    if (!active) { Object::RenderChildren(); return; }
    int vp[4]; gfx::GetViewport(vp);
    int sc[4]; gfx::GetScissor(sc);
    const bool scOn = gfx::ScissorActive();
    // la PANTALLA del Recorte = el cuadro de la imagen adentro del viewport (g_renderFrame: mirando por una camara con
    // encuadre, sin las barras / el passepartout), el mismo al que se encaja la UI
    float mw = vp[2] * g_renderFrame[2], mh = vp[3] * g_renderFrame[3];
    float mx0 = vp[0] + vp[2] * 0.5f * (1.0f + g_renderFrame[0]) - mw * 0.5f;
    float my0 = vp[1] + vp[3] * 0.5f * (1.0f + g_renderFrame[1]) - mh * 0.5f;
    // el rectangulo en px (GL: y crece hacia ARRIBA, igual que +y del Recorte)
    float an = width < 0.0f ? 0.0f : width, to = height < 0.0f ? 0.0f : height;
    int w = Round(mw * an), h = Round(mh * to);
    int cx = Round(mx0 + mw * (0.5f + x)), cy = Round(my0 + mh * (0.5f + y));
    int rx = cx - w / 2, ry = cy - h / 2;
    if (w < 1 || h < 1) return;                    // rectangulo vacio: no se dibuja nada
    // recorte = el rectangulo INTERSECTADO con el recorte que ya habia (Recortes anidados, marco del render)
    int x0 = rx, y0 = ry, x1 = rx + w, y1 = ry + h;
    if (scOn) {
        if (x0 < sc[0]) x0 = sc[0];
        if (y0 < sc[1]) y0 = sc[1];
        if (x1 > sc[0] + sc[2]) x1 = sc[0] + sc[2];
        if (y1 > sc[1] + sc[3]) y1 = sc[1] + sc[3];
    }
    if (x1 <= x0 || y1 <= y0) return;
    gfx::Enable(gfx::ScissorTest);
    gfx::Scissor(x0, y0, x1 - x0, y1 - y0);
    if (background) {
        gfx::ClearColor(color[0], color[1], color[2], color[3]);
        gfx::DepthMask(true);
        gfx::Clear(gfx::ColorBuffer | gfx::DepthBuffer);
    } else if (clearZ) {
        gfx::DepthMask(true);
        gfx::Clear(gfx::DepthBuffer);
    }

    // con CAMARA: viewport = el rectangulo y la vista/proyeccion de esa camara (aspecto del rectangulo)
    Camera* cam = NULL;
    if (!camera.empty() && SceneCollection) {
        Object* o = W3dFindNameFrom(this, camera, SceneCollection);   // (por scope: la camara de SU prefab)
        if (o && o->getType() == ObjectType::camera) cam = (Camera*)o;
    }
    Matrix4 mine; GetWorldMatrix(mine);
    if (cam) {
        gfx::Viewport(rx, ry, w, h);
        CameraBase cb;
        cb.fov = cam->fov; cb.nearZ = cam->nearClip; cb.farZ = cam->farClip;
        Matrix4 P = cb.ProjectionMatrix((float)w / (float)h);
        Matrix4 cw; cam->GetWorldMatrix(cw);
        Matrix4 V = cw.Inverse();
        gfx::MatrixMode(gfx::Projection); gfx::PushMatrix(); gfx::LoadMatrix(P.m);
        gfx::MatrixMode(gfx::ModelView);  gfx::PushMatrix(); gfx::LoadMatrix(V.m);
        W3dLightsPrePass(SceneCollection);          // las luces en el espacio de ESTA camara
        gfx::MultMatrix(mine.m);                    // los hijos cuelgan de este objeto
        Object::RenderChildren();
        gfx::PopMatrix();
        gfx::MatrixMode(gfx::Projection); gfx::PopMatrix();
        gfx::MatrixMode(gfx::ModelView);
        gfx::Viewport(vp[0], vp[1], vp[2], vp[3]);
        // las luces vuelven a la vista de la escena (la matriz actual es vista * este objeto)
        Matrix4 inv = mine.Inverse();
        gfx::PushMatrix(); gfx::MultMatrix(inv.m);
        W3dLightsPrePass(SceneCollection);
        gfx::PopMatrix();
    } else {
        Object::RenderChildren();
    }
    if (scOn) gfx::Scissor(sc[0], sc[1], sc[2], sc[3]);
    else      gfx::Disable(gfx::ScissorTest);
}

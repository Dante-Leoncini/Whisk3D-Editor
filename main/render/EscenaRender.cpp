// ============================================================================
//  EscenaRender.cpp — ver EscenaRender.h. El pase 3D COMPARTIDO entre el
//  viewport del editor y el runtime de un juego compilado.
// ============================================================================
#include "EscenaRender.h"
#include "gfx/w3dGraphics.h"
#include "objects/Objects.h"
#include "objects/Mesh.h"        // w3dLoteStamp + los diferidos (calcomanias / luces aditivas)
#include "objects/Particulas.h"  // particulas diferidas (billboards translucidos)
#include "objects/Light.h"        // MAX_LIGHTS: apagar los GL lights antes del pase
#include "render/OpcionesRender.h"// RenderType + la luz de los modos de preview
#include <math.h>

namespace gfx = w3dEngine;

static const float W3D_PI = 3.14159265358979f;

// ---------------------------------------------------------------------------
//  ENCUADRE
// ---------------------------------------------------------------------------
void W3dEncuadreMarco(float aspectoVista, float aspectoDeclarado, float margen,
                      float* nx, float* ny) {
    float va = (aspectoVista    > 1e-4f) ? aspectoVista    : 1.0f;
    float ra = (aspectoDeclarado > 1e-4f) ? aspectoDeclarado : va;
    if (margen <= 0.0f) margen = 1.0f;
    // imagen mas ANCHA que el viewport -> entra por el ancho (bandas arriba/abajo);
    // mas ALTA -> entra por el alto (bandas a los costados).
    float x, y;
    if (ra >= va) { x = margen;              y = margen * va / ra; }
    else          { y = margen;              x = margen * ra / va; }
    if (nx) *nx = x;
    if (ny) *ny = y;
}

// ---------------------------------------------------------------------------
//  PROYECCION (+ publicacion de la lente al Core, para el culling en CPU)
// ---------------------------------------------------------------------------
void W3dEscena3DProyeccion(const W3dVista3D& v) {
    gfx::MatrixMode(gfx::Projection);
    gfx::LoadIdentity();

    // la LENTE con la que se dibuja de verdad: de aca saca el objeto Culling sus
    // 6 planos. Se publica el frustum de la IMAGEN (el aspecto declarado cuando
    // lo hay), no el del viewport: asi se cull-ea lo que el juego cull-earia.
    g_renderCamFov    = v.fov;
    g_renderCamNear   = v.nearC;
    g_renderCamFar    = v.farC;
    g_renderCamAspect = (v.aspectoImagen > 1e-4f) ? v.aspectoImagen : 1.0f;
    g_renderCamOrto   = v.orto;

    float nx = (v.marcoNX > 1e-4f) ? v.marcoNX : 1.0f;
    float ny = (v.marcoNY > 1e-4f) ? v.marcoNY : 1.0f;
    float ra = (v.aspectoImagen > 1e-4f) ? v.aspectoImagen : 1.0f;

    // inspeccion (el editor panea/zoomea la vista de camara sin mover la camara);
    // en el juego zoom=1 y pan=0, o sea que estas dos son la identidad.
    if (v.panX != 0.0f || v.panY != 0.0f) gfx::Translatef(v.panX, v.panY, 0.0f);
    if (v.zoom != 1.0f)                   gfx::Scalef(v.zoom, v.zoom, 1.0f);

    if (v.orto) {
        float size = (v.ortoSize > 0.001f) ? v.ortoSize : 0.001f;
        gfx::Ortho(-size * ra / nx, size * ra / nx, -size / ny, size / ny, v.nearC, v.farC);
    } else {
        float top = v.nearC * tanf(v.fov * 0.5f * W3D_PI / 180.0f);
        float T = top / ny, R = top * ra / nx;
        gfx::Frustum(-R, R, -T, T, v.nearC, v.farC);
    }

    gfx::MatrixMode(gfx::ModelView);
    gfx::LoadIdentity();
}

// ---------------------------------------------------------------------------
//  CAMARA: publicar la vista al Core + cargar la matriz de vista
// ---------------------------------------------------------------------------
void W3dEscena3DCamara(const W3dVista3D& v, const Vector3& luzPos, const Vector3& luzColor) {
    W3dVistaBind(v.cam);
    g_renderLightPos   = luzPos;
    g_renderLightColor = luzColor;
    gfx::LoadMatrix(v.cam.ViewMatrix().m);
}

// ---------------------------------------------------------------------------
//  MODO DE DIBUJO (flags del Core + luz del pase) a partir del RenderType
// ---------------------------------------------------------------------------
void W3dEscena3DLuces(int vista) {
    const RenderType::Enum v = (RenderType::Enum)vista;
    // apagar los 8 GL lights: si una luz de escena cambio de slot, el viejo quedaba
    // prendido (luz "fantasma"). Cada Light enciende el suyo al dibujarse.
    for (int i = 0; i < MAX_LIGHTS; i++) gfx::SetLightEnabled(GL_LIGHT0 + i, false);
    switch (v) {
        // Solid usa la MISMA luz que Material Preview (se ven igual; la unica
        // diferencia es que Solid no usa texturas ni los materiales reales)
        case RenderType::MaterialPreview:
        case RenderType::Solid:
            gfx::Enable(gfx::Light0);
            gfx::Light0fv(gfx::LightDiffuse,  MaterialPreviewDiffuse);
            gfx::Light0fv(gfx::LightAmbient,  MaterialPreviewAmbient);
            gfx::Light0fv(gfx::LightSpecular, MaterialPreviewSpecular);
            gfx::Light0f(gfx::LightConstantAtt, 1.0f);
            gfx::Light0f(gfx::LightLinearAtt, 0.0f);
            gfx::Light0f(gfx::LightQuadraticAtt, 0.0f);
            break;
        case RenderType::ZBuffer:      // profundidad: sin luz
        case RenderType::NormalView:   // color por normal (unlit), lo dibuja el Core
        case RenderType::Alpha:        // matte blanco unlit, lo dibuja el Core
            gfx::Disable(gfx::Light0);
            break;
        default: break;
    }
}

void W3dEscena3DModo(int vista) {
    const RenderType::Enum v = (RenderType::Enum)vista;
    w3dRenderWireframe   = (v == RenderType::Wireframe);
    w3dRenderSolido      = (v == RenderType::Solid);
    w3dRenderNormalColor = (v == RenderType::NormalView);
    w3dRenderSinLuz      = (v == RenderType::ZBuffer);
    w3dRenderLuces       = (v == RenderType::Rendered);
    w3dRenderAlpha       = (v == RenderType::Alpha);
    // POSICION de la luz frontal de los modos de preview. Va aparte de W3dEscena3DLuces
    // porque GL la transforma por la MODELVIEW DEL MOMENTO: tiene que ponerse con la
    // modelview en IDENTIDAD (o sea, antes de cargar la matriz de vista), o la escena
    // sale oscura. Ese es el unico motivo de que sean dos funciones y no una.
    if (v == RenderType::MaterialPreview || v == RenderType::Solid)
        gfx::Light0fv(gfx::LightPosition, MaterialPreviewPosition);
}

// ---------------------------------------------------------------------------
//  EL PASE
// ---------------------------------------------------------------------------
void W3dEscena3DPasada() {
    if (!SceneCollection) return;
    // por si un pase anterior dejo anotados sin dibujar
    W3dParticulasLimpiarPendientes();
    W3dDecalesLimpiarPendientes();
    W3dLucesLimpiarPendientes();
    // resync del cache de estado UNA vez por frame, ANTES del recorrido: lo que se
    // dibujo antes (chrome del editor, HUD del frame anterior) pudo tocar GL crudo.
    gfx::Invalidate();
    w3dLoteStamp++;              // sello del pase: las Collection con lote marcan a sus horneadas
    SceneCollection->Render();
    // CALCOMANIAS (sombras) y LUCES ADITIVAS (chispas/halos): no escriben z, asi que
    // van DESPUES de toda la escena opaca o cualquier opaco posterior las borra.
    W3dDecalesDibujarPendientes();
    W3dLucesDibujarPendientes();
    // PARTICULAS translucidas al final, con el z-buffer de la escena ya escrito.
    W3dParticulasDibujarPendientes();
}

// ---------------------------------------------------------------------------
//  BANDAS del encuadre
// ---------------------------------------------------------------------------
void W3dEscena3DBandas(float W, float H, float marcoNX, float marcoNY,
                       float zoom, float panX, float panY, bool opaco) {
    if (W < 1.0f || H < 1.0f) return;
    gfx::MatrixMode(gfx::Projection); gfx::LoadIdentity();
    gfx::Ortho(0, W, H, 0, -1, 1);
    gfx::MatrixMode(gfx::ModelView); gfx::LoadIdentity();
    // sin z-test, sin luz, sin textura... y SIN CULL: las bandas se dibujan en un
    // ortho con la Y invertida, o sea que su winding queda al reves del de la escena.
    // Con CULL_FACE prendido (que es como lo deja el pase 3D) desaparecian enteras:
    // el juego mostraba lo de AFUERA del encuadre en vez de taparlo con negro.
    gfx::Disable(gfx::DepthTest); gfx::Disable(gfx::Lighting); gfx::Disable(gfx::Texture2D);
    gfx::Disable(gfx::CullFace);
    gfx::Enable(gfx::Blend); gfx::BlendAlpha();
    gfx::DisableArray(gfx::TexCoordArray); gfx::DisableArray(gfx::NormalArray);
    gfx::DisableArray(gfx::ColorArray);    gfx::EnableArray(gfx::VertexArray);
    // el marco sigue el zoom/pan de inspeccion (la MISMA transform que la proyeccion)
    float cx = (panX * 0.5f + 0.5f) * W;
    float cy = (0.5f - panY * 0.5f) * H;
    float hw = marcoNX * zoom * W * 0.5f, hh = marcoNY * zoom * H * 0.5f;
    float x0 = cx - hw, x1 = cx + hw, y0 = cy - hh, y1 = cy + hh;
    // acotadas al rect (el marco puede quedar parcialmente afuera al panear/zoomear)
    float bx0 = x0<0?0:(x0>W?W:x0), bx1 = x1<0?0:(x1>W?W:x1);
    float by0 = y0<0?0:(y0>H?H:y0), by1 = y1<0?0:(y1>H?H:y1);
    if (opaco) gfx::Color4f(0.0f, 0.0f, 0.0f, 1.0f);
    else       gfx::Color4f(0.0f, 0.0f, 0.0f, 0.5f);
    GLfloat bandas[] = {
        0,0,   W,0,   W,by0,    0,0,   W,by0,  0,by0,        // arriba
        0,by1, W,by1, W,H,      0,by1, W,H,    0,H,          // abajo
        0,by0, bx0,by0, bx0,by1, 0,by0, bx0,by1, 0,by1,      // izquierda
        bx1,by0, W,by0, W,by1,  bx1,by0, W,by1, bx1,by1,     // derecha
    };
    gfx::VertexPointer2f(0, bandas);
    gfx::DrawTrianglesArray(24);
}

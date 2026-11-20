// ============================================================================
//  EscenaRender.cpp — ver EscenaRender.h. El pase 3D COMPARTIDO entre el
//  viewport del editor y el runtime de un juego compilado.
// ============================================================================
#include "objects/Rutina.h"   // g_w3dSoloJuego / W3dHayConstructor (el cache no se invalida)
#include "EscenaRender.h"
#include "objects/CameraBase.h"   // g_renderMarco: el cuadro de la imagen (lo miden los Recortes)
#include "graphics/w3dGraphics.h"
#include "objects/Objects.h"
#include "objects/Mesh.h"        // w3dLoteStamp + los diferidos (calcomanias / luces aditivas)
#include "objects/Particulas.h"  // particulas diferidas (billboards translucidos)
#include "objects/Light.h"        // MAX_LIGHTS: apagar los GL lights antes del pase
#include "objects/SondaLuz.h"     // SondaLuzNuevoPase: las sondas de luz del pase
#include "objects/SondaReflejo.h" // las sondas de reflejo de tiempo real: las dibuja la GPU antes del pase
#include "render/SombrasTiempoReal.h"   // las sombras en tiempo real: los mapas antes de la escena
#include "W3dRaices.h"              // las escenas abiertas a la vez en el editor
#include "script/W3dScript.h"       // W3dScriptHayPartida: jugando solo se ve la base
#include "render/OpcionesRender.h"// RenderType + la luz de los modos de preview
#include "objects/Niebla.h"        // la niebla arranca y termina apagada en cada pase
#include "W3dEscena.h"             // dibujar3D(false) de lua: el pase no se hace
#include "objects/Textures.h"      // las capturas de pantalla: texturas registradas por ruta
#include "graphics/w3dTexture.h"
#include <math.h>
#include <map>
#include <vector>
#ifndef W3D_SIN_EDITOR
#include "io/TexturaEditada.h"     // los pixeles CPU del fondo 360 (su copia borrosa)
#endif

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
    // el cuadro de la imagen adentro del viewport (con el zoom / pan de inspeccion del editor): ver g_renderMarco
    g_renderMarco[0] = v.panX; g_renderMarco[1] = v.panY;
    g_renderMarco[2] = v.marcoNX * v.zoom; g_renderMarco[3] = v.marcoNY * v.zoom;

    // la LENTE con la que se dibuja de verdad: de aca saca el objeto Culling sus
    // 6 planos. Se publica el frustum de la IMAGEN (el aspecto declarado cuando
    // lo hay), no el del viewport: asi se cull-ea lo que el juego cull-earia.
    g_renderCamFov    = v.fov;
    g_renderCamNear   = v.nearC;
    g_renderCamFar    = v.farC;
    g_renderCamAspect = (v.aspectoImagen > 1e-4f) ? v.aspectoImagen : 1.0f;
    g_renderCamOrto   = v.orto;
    if (v.altoImagenPx > 0.5f) g_renderImagenAltoPx = v.altoImagenPx;

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
// la LUZ FRONTAL de los modos de preview (Material / Solid) quedo prendida en el slot 0
static bool gLuzPreviewPuesta = false;
// LA LUZ AMBIENTE de la vista Render y del juego: la del FONDO (mas abajo, con la irradiancia del 360)
static void AmbienteDelFondo();
void W3dEscena3DLuces(int vista) {
    const RenderType::Enum v = (RenderType::Enum)vista;
    // un proyecto con CONSTRUCTOR (el modo avanzado) en Rendered: las luces son de sus rutinas (rutinas.md), el pase no
    // las toca. Apagar las 8 en cada cuadro eran 8 llamadas por cuadro despues de cualquier Invalidate.
    // Menos la luz frontal que dejo prendida un modo de preview (este viewport antes de pasar a Render, u otro viewport
    // en Material en el mismo cuadro): esa se apaga, antes de que la rutina y las luces de la escena pongan las suyas.
    // Si no, borrar la ultima luz de la escena dejaba el objeto iluminado igual
    // Y los canales que prendieron las luces de la escena (Light.h): las que siguen en la escena los vuelven a prender
    // al dibujarse; los de una luz BORRADA (viva para el Ctrl+Z) u oculta quedaban prendidos iluminando
    if (v == RenderType::Rendered && W3dHayConstructor()) {
        AmbienteDelFondo();   // (antes de la rutina: un paso "Ambient light" la puede pisar)
        if (gLuzPreviewPuesta) { gfx::SetLightEnabled(GL_LIGHT0, false); gLuzPreviewPuesta = false; }
        for (int i = 0; i < MAX_LIGHTS; i++)
            if (g_w3dLucesEscenaPrendidas & (1u << i)) gfx::SetLightEnabled(GL_LIGHT0 + i, false);
        g_w3dLucesEscenaPrendidas = 0;
        return;
    }
    gLuzPreviewPuesta = false;   // (abajo se apagan las 8; la de preview se vuelve a prender si es su modo)
    // EL AMBIENTE: en Render, la luz del fondo; en el resto el de GL de siempre (si no, el negro de un Render en otro
    // viewport oscurecia la vista previa)
    if (v == RenderType::Rendered) AmbienteDelFondo();
    else { static const float kAmb[4] = { 0.2f, 0.2f, 0.2f, 1.0f }; gfx::LightModelAmbient(kAmb); }
    // apagar los 8 GL lights: si una luz de escena cambio de slot, el viejo quedaba
    // prendido (luz "fantasma"). Cada Light enciende el suyo al dibujarse.
    for (int i = 0; i < MAX_LIGHTS; i++) gfx::SetLightEnabled(GL_LIGHT0 + i, false);
    g_w3dLucesEscenaPrendidas = 0;   // (las luces de la escena vuelven a anotar el suyo al dibujarse)
    switch (v) {
        // Solid usa la MISMA luz que Material Preview (se ven igual; la unica
        // diferencia es que Solid no usa texturas ni los materiales reales)
        case RenderType::MaterialPreview:
        case RenderType::Solid:
            gfx::Enable(gfx::Light0);
            gLuzPreviewPuesta = true;
            gfx::Light0fv(gfx::LightDiffuse,  MaterialPreviewDiffuse);
            gfx::Light0fv(gfx::LightAmbient,  MaterialPreviewAmbient);
            gfx::Light0fv(gfx::LightSpecular, MaterialPreviewSpecular);
            gfx::Light0f(gfx::LightConstantAtt, 1.0f);
            gfx::Light0f(gfx::LightLinearAtt, 0.0f);
            gfx::Light0f(gfx::LightQuadraticAtt, 0.0f);
#if !(defined(W3D_SYMBIAN) && !defined(W3D_GLES2))   // (el N95 no le pone cono a ninguna luz: Light.cpp)
            // SIN CONO: el slot 0 es el de la primera luz de la escena, y en Rendered un FOCO le deja su cono (corte,
            // exponente y direccion). Volver de Rendered a Material dejaba la luz del preview como ese foco: oscuro
            { static const float adelante[3] = { 0.0f, 0.0f, -1.0f };
              gfx::LightNSpot(0, 180.0f, 0.0f, adelante); }
#endif
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
//  EL CIELO: el fondo como TEXTURA 360 (equirectangular). Una esfera alrededor de la camara, vista de adentro, sin
//  escribir ni mirar la profundidad (queda detras de todo). Las UV de una esfera de latitud / longitud SON las de una
//  imagen equirectangular: anda igual en el pipeline fijo (el N95), en GLES2 y en Vulkan. Gira con la rotacion del fondo
//  (la misma cuenta que W3dFondoDireccionUV: los reflejos y el trazado de rayos ven lo mismo)
// ---------------------------------------------------------------------------
static void DibujarCielo(Texture* tex = NULL, const float* giro = NULL) {
    // (sin imagen: el fondo del render, con su giro; si no, la del paso "Clear" con fondo propio)
    if (!tex) { if (!W3dFondoConTextura()) return; tex = g_renderBgTex; giro = g_renderBgRot; }
    if (!tex || !tex->iID) return;
    static const float kSinGiro[3] = { 0.0f, 0.0f, 0.0f };
    if (!giro) giro = kSinGiro;
    // (en una GPU chica -la de los Android 2.3- la mitad de las divisiones: el cielo esta lejos y se ve igual)
    const bool chica = gfx::GpuLimitada();
    const int kLat = chica ? 12 : 24, kLon = chica ? 24 : 48;
    enum { kLatMax = 24, kLonMax = 48 };
    static float pos[(kLatMax + 1) * (kLonMax + 1) * 3], uv[(kLatMax + 1) * (kLonMax + 1) * 2];
    static unsigned short idx[kLatMax * kLonMax * 6];
    static int armado = 0;   // (las divisiones con que se armo)
    if (armado != kLat) {
        armado = kLat;
        for (int i = 0; i <= kLat; i++)
            for (int j = 0; j <= kLon; j++) {
                const float th = 3.14159265f * (float)i / (float)kLat;          // 0 = arriba (+Z)
                const float ph = -3.14159265f + 6.2831853f * (float)j / (float)kLon;
                const int k = i * (kLon + 1) + j;
                pos[k * 3 + 0] = sinf(th) * cosf(ph); pos[k * 3 + 1] = sinf(th) * sinf(ph); pos[k * 3 + 2] = cosf(th);
                uv[k * 2 + 0] = (float)j / (float)kLon; uv[k * 2 + 1] = (float)i / (float)kLat;
            }
        int n = 0;
        for (int i = 0; i < kLat; i++)
            for (int j = 0; j < kLon; j++) {
                const unsigned short a = (unsigned short)(i * (kLon + 1) + j), b = (unsigned short)(a + kLon + 1);
                idx[n++] = a; idx[n++] = b; idx[n++] = (unsigned short)(a + 1);
                idx[n++] = (unsigned short)(a + 1); idx[n++] = b; idx[n++] = (unsigned short)(b + 1);
            }
    }
    // (adentro del frustum: entre el plano cercano y el lejano, sin importar la escala de la escena)
    const float r = (g_renderCamNear + g_renderCamFar) * 0.5f;
    gfx::PushMatrix();
    gfx::Translatef(g_renderCamPos.x, g_renderCamPos.y, g_renderCamPos.z);
    gfx::Rotatef(-90.0f, 1.0f, 0.0f, 0.0f);   // (la esfera tiene la Z arriba y el motor la Y: ver W3dFondoMatriz)
    gfx::Rotatef(giro[2], 0.0f, 0.0f, 1.0f);
    gfx::Rotatef(giro[1], 0.0f, 1.0f, 0.0f);
    gfx::Rotatef(giro[0], 1.0f, 0.0f, 0.0f);
    gfx::Scalef(r, r, r);
    gfx::Disable(gfx::Lighting); gfx::Disable(gfx::Blend); gfx::Disable(gfx::CullFace); gfx::Disable(gfx::DepthTest);
    gfx::DepthMask(false);
    gfx::Enable(gfx::Texture2D);
    gfx::BindTexture(tex->iID);
    gfx::TexFilter(true); gfx::TexWrap(true);
    gfx::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
    gfx::DisableArray(gfx::NormalArray); gfx::DisableArray(gfx::ColorArray);
    gfx::EnableArray(gfx::VertexArray); gfx::EnableArray(gfx::TexCoordArray);
    gfx::VertexPointer3f(0, pos); gfx::TexCoordPointer2f(0, uv);
    gfx::DrawElements16(gfx::PrimTriangulos, kLat * kLon * 6, idx);
    gfx::DisableArray(gfx::TexCoordArray);
    gfx::DepthMask(true); gfx::Enable(gfx::DepthTest);
    gfx::PopMatrix();
}

// UNA COPIA REDUCIDA del fondo 360: sus pixeles (W x H, RGBA), cada uno el promedio de su zona; con 'pasadas' de
// desenfoque que da la vuelta a lo ancho (la imagen es un cilindro). Sale de los pixeles de la CPU: en el juego compilado
// (sin ellos) no hay copias y se usa la imagen misma
static bool ReducirFondo(int W, int H, int pasadas, std::vector<unsigned char>& px) {
#ifndef W3D_SIN_EDITOR
    if (!g_renderBgTex || !g_renderBgTex->iID) return false;
    TexturaEditable* te = TexEditObtener(g_renderBgTex);
    if (!te || te->w <= 0 || te->h <= 0) return false;
    std::vector<float> a((size_t)W * H * 3, 0.0f);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const int x0 = x * te->w / W, x1 = (x + 1) * te->w / W, y0 = y * te->h / H, y1 = (y + 1) * te->h / H;
            double sm[3] = { 0, 0, 0 }; int n = 0;
            for (int yy = y0; yy < (y1 > y0 ? y1 : y0 + 1); yy++)
                for (int xx = x0; xx < (x1 > x0 ? x1 : x0 + 1); xx++) {
                    const unsigned char* p = &te->rgba[((size_t)yy * te->w + xx) * 4];
                    sm[0] += p[0]; sm[1] += p[1]; sm[2] += p[2]; n++;
                }
            for (int k = 0; k < 3; k++) a[((size_t)y * W + x) * 3 + k] = n ? (float)(sm[k] / n) : 0.0f;
        }
    for (int pasada = 0; pasada < pasadas; pasada++) {   // (caja 5x3: a lo ancho da la vuelta, a lo alto se corta en los polos)
        std::vector<float> b(a.size(), 0.0f);
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                float sm[3] = { 0, 0, 0 }; int n = 0;
                for (int dy = -1; dy <= 1; dy++) {
                    const int yy = y + dy; if (yy < 0 || yy >= H) continue;
                    for (int dx = -2; dx <= 2; dx++) {
                        const int xx = (x + dx + W) % W;
                        for (int k = 0; k < 3; k++) sm[k] += a[((size_t)yy * W + xx) * 3 + k];
                        n++;
                    }
                }
                for (int k = 0; k < 3; k++) b[((size_t)y * W + x) * 3 + k] = sm[k] / n;
            }
        a.swap(b);
    }
    px.assign((size_t)W * H * 4, 255);
    for (size_t i = 0; i < (size_t)W * H; i++)
        for (int k = 0; k < 3; k++) { const float v = a[i * 3 + k]; px[i * 4 + k] = (unsigned char)(v < 0 ? 0 : v > 255 ? 255 : v + 0.5f); }
    return true;
#else
    (void)W; (void)H; (void)pasadas; (void)px;
    return false;
#endif
}
// LA IRRADIANCIA DEL FONDO 360: la luz difusa que le llega a una superficie segun hacia donde mira, como el ambiente de
// Blender o la "ambient probe" de Unity. Se PRECALCULA una vez por imagen: los armonicos esfericos de orden 2 (9
// coeficientes por color; Ramamoorthi y Hanrahan, 2001) sobre una grilla de 128 x 64 de la imagen (cada punto pesado por
// su angulo solido), evaluados en una imagen chica equirectangular de 64 x 32 (la que lee el shader para la luz difusa y
// los reflejos rugosos). 'prom' es el promedio: el ambiente de lo que no tiene luz por pixel (pipeline fijo, el N95)
struct IrrFondo { unsigned int de; bool ok; float prom[3]; std::vector<unsigned char> px; };
static IrrFondo gIrr;
static const int kIrrW = 64, kIrrH = 32;
// la IRRADIANCIA de una imagen 360 cualquiera (RGBA, tw x th): la de 64 x 32 en 'px' y el promedio en 'prom' (0..1).
// La usan el fondo y las SONDAS DE REFLEJO (su copia borrosa: la luz difusa y los reflejos rugosos del PBR)
static void IrradianciaDe(const unsigned char* rgba, int tw, int th, std::vector<unsigned char>& px, float prom[3]) {
    const int SW = 128, SHh = 64;
    const double PI = 3.14159265358979;
    double c[9][3]; for (int i = 0; i < 9; i++) c[i][0] = c[i][1] = c[i][2] = 0.0;
    for (int y = 0; y < SHh; y++) {
        const double th0 = (y + 0.5) / SHh * PI, st = sin(th0), ct = cos(th0);
        const double dw = (2.0 * PI / SW) * (PI / SHh) * st;   // (el angulo solido del punto)
        const int y0 = y * th / SHh, y1t = (y + 1) * th / SHh, y1 = y1t > y0 ? y1t : y0 + 1;
        const int pasoY = (y1 - y0) > 8 ? (y1 - y0) / 8 : 1;
        for (int x = 0; x < SW; x++) {
            const double ph = ((x + 0.5) / SW - 0.5) * 2.0 * PI;
            const double dx = st * cos(ph), dy = st * sin(ph), dz = ct;
            const int x0 = x * tw / SW, x1t = (x + 1) * tw / SW, x1 = x1t > x0 ? x1t : x0 + 1;
            const int pasoX = (x1 - x0) > 8 ? (x1 - x0) / 8 : 1;
            double col[3] = { 0, 0, 0 }; int n = 0;   // (el promedio de su zona, salteado hasta 8 x 8)
            for (int yy = y0; yy < y1 && yy < th; yy += pasoY)
                for (int xx = x0; xx < x1 && xx < tw; xx += pasoX) {
                    const unsigned char* p = &rgba[((size_t)yy * tw + xx) * 4];
                    col[0] += p[0]; col[1] += p[1]; col[2] += p[2]; n++;
                }
            if (n) { col[0] /= n; col[1] /= n; col[2] /= n; }
            const double Y[9] = { 0.282095, 0.488603 * dy, 0.488603 * dz, 0.488603 * dx, 1.092548 * dx * dy,
                                  1.092548 * dy * dz, 0.315392 * (3.0 * dz * dz - 1.0), 1.092548 * dx * dz,
                                  0.546274 * (dx * dx - dy * dy) };
            for (int i = 0; i < 9; i++)
                for (int k = 0; k < 3; k++) c[i][k] += col[k] * Y[i] * dw;
        }
    }
    // la luz difusa (Lambert) de una superficie que mira hacia n: sum (A_l / pi) c_lm Y_lm(n), A/pi = 1, 2/3, 1/4
    static const double kA[9] = { 1.0, 2.0 / 3.0, 2.0 / 3.0, 2.0 / 3.0, 0.25, 0.25, 0.25, 0.25, 0.25 };
    px.assign((size_t)kIrrW * kIrrH * 4, 255);
    for (int y = 0; y < kIrrH; y++) {
        const double th0 = (y + 0.5) / kIrrH * PI, st = sin(th0), ct = cos(th0);
        for (int x = 0; x < kIrrW; x++) {
            const double ph = ((x + 0.5) / kIrrW - 0.5) * 2.0 * PI;
            const double dx = st * cos(ph), dy = st * sin(ph), dz = ct;
            const double Y[9] = { 0.282095, 0.488603 * dy, 0.488603 * dz, 0.488603 * dx, 1.092548 * dx * dy,
                                  1.092548 * dy * dz, 0.315392 * (3.0 * dz * dz - 1.0), 1.092548 * dx * dz,
                                  0.546274 * (dx * dx - dy * dy) };
            for (int k = 0; k < 3; k++) {
                double v = 0.0;
                for (int i = 0; i < 9; i++) v += kA[i] * c[i][k] * Y[i];
                px[((size_t)y * kIrrW + x) * 4 + k] = (unsigned char)(v < 0.0 ? 0 : v > 255.0 ? 255 : (int)(v + 0.5));
            }
        }
    }
    for (int k = 0; k < 3; k++) { const double m = c[0][k] * 0.282095 / 255.0; prom[k] = (float)(m < 0.0 ? 0.0 : m); }
}
static const IrrFondo* IrradianciaDelFondo() {
#ifndef W3D_SIN_EDITOR
    if (!g_renderBgTex || !g_renderBgTex->iID) return NULL;
    if (gIrr.ok && gIrr.de == g_renderBgTex->iID) return &gIrr;
    TexturaEditable* te = TexEditObtener(g_renderBgTex);
    if (!te || te->w <= 0 || te->h <= 0) return NULL;
    IrradianciaDe(&te->rgba[0], te->w, te->h, gIrr.px, gIrr.prom);
    gIrr.de = g_renderBgTex->iID; gIrr.ok = true;
    return &gIrr;
#else
    return NULL;   // (el juego compilado no tiene los pixeles: el ambiente es el color del fondo)
#endif
}
// EL AMBIENTE del pase con las luces (Render y el juego): el FONDO por su luz (g_renderLuzFondo). Con un color, ese
// color; con el 360, el promedio de su irradiancia. El ALFA lleva el factor: el shader PBR lo multiplica por la
// irradiancia del 360 en la direccion de la normal (w3dGraphicsGLES2.cpp)
static float gAmbPase[4] = { 0.2f, 0.2f, 0.2f, 1.0f };
static void AmbienteDelFondo() {
    const float f = g_renderLuzFondo < 0.0f ? 0.0f : g_renderLuzFondo;
    float c[4] = { g_renderBg[0] * f, g_renderBg[1] * f, g_renderBg[2] * f, f };
    if (W3dFondoConTextura()) {
        const IrrFondo* irr = IrradianciaDelFondo();
        if (irr) for (int k = 0; k < 3; k++) c[k] = irr->prom[k] * f;
    }
    for (int k = 0; k < 4; k++) gAmbPase[k] = c[k];
    gfx::LightModelAmbient(c);
}
// el ambiente del fondo de este pase (la sonda de luz lo repone al terminar un objeto que iluminaba)
void W3dAmbientePase(float c[4]) { for (int k = 0; k < 4; k++) c[k] = gAmbPase[k]; }
struct EntornoCopia { unsigned int tex, de; };
static unsigned int EntornoReducido(EntornoCopia& c, int W, int H, int pasadas) {
    if (!g_renderBgTex || !g_renderBgTex->iID) return 0;
    if (c.tex && c.de == g_renderBgTex->iID) return c.tex;
    std::vector<unsigned char> px;
    if (!ReducirFondo(W, H, pasadas, px)) return 0;
    if (c.tex) gfx::DeleteTexture(c.tex);
    c.tex = gfx::UploadRGBA(&px[0], W, H, true, pasadas == 0, false, pasadas == 0);   // (la nitida: con piramide y 16 bits)
    c.de = g_renderBgTex->iID;
    return c.tex;
}
// LA BORROSA (la luz difusa y los reflejos rugosos de la luz por pixel): la IRRADIANCIA precalculada (64 x 32)
static EntornoCopia gEnvBorrosa = { 0, 0 };
static unsigned int EntornoBorroso() {
    if (!g_renderBgTex || !g_renderBgTex->iID) return 0;
    if (gEnvBorrosa.tex && gEnvBorrosa.de == g_renderBgTex->iID) return gEnvBorrosa.tex;
    const IrrFondo* irr = IrradianciaDelFondo();
    if (!irr) return 0;
    if (gEnvBorrosa.tex) gfx::DeleteTexture(gEnvBorrosa.tex);
    gEnvBorrosa.tex = gfx::UploadRGBA(&irr->px[0], kIrrW, kIrrH, true, false, false, false);
    gEnvBorrosa.de = g_renderBgTex->iID;
    return gEnvBorrosa.tex;
}
// LA NITIDA DE UNA GPU CHICA sin cubemaps: 256x128 en vez de la imagen entera (los reflejos leen el 360 salteado y la
// imagen grande no entraba en el cache de texturas)
static EntornoCopia gEnvChica = { 0, 0 };
static unsigned int EntornoNitido() {
    if (!g_renderBgTex) return 0;
    if (gfx::GpuLimitada() && (g_renderBgTex->ancho > 256 || g_renderBgTex->alto > 128)) {
        const unsigned int t = EntornoReducido(gEnvChica, 256, 128, 0);
        if (t) return t;
    }
    return g_renderBgTex->iID;
}
// EL ENTORNO COMO CUBEMAP (las GPU chicas, gfx::EntornoCubo): la imagen 360 (rgba, w x h) pasada a 6 caras de lado x lado.
// Cada texel de una cara es una DIRECCION (la de GL para esa cara) y toma el pixel de la imagen en esa direccion, con la
// misma cuenta que el shader y el cielo (u = 0.5 + atan2(y,x)/2pi, v = acos(z)/pi; la fila 0, arriba). En la VideoCore IV
// la imagen 360 costaba un atan y un acos por pixel en cada lectura (unos 15 ms por cuadro en el Galaxy Ace)
static unsigned int CuboDe(EntornoCopia& c, int lado, const unsigned char* rgba, int w, int h) {
    std::vector<unsigned char> caras[6];
    const unsigned char* ptr[6];
    for (int f = 0; f < 6; f++) {
        caras[f].resize((size_t)lado * lado * 4);
        for (int j = 0; j < lado; j++)
            for (int i = 0; i < lado; i++) {
                const float a = 2.0f * ((float)i + 0.5f) / (float)lado - 1.0f, b = 2.0f * ((float)j + 0.5f) / (float)lado - 1.0f;
                float d[3];
                switch (f) {   // (las caras de GL: +X -X +Y -Y +Z -Z)
                    case 0:  d[0] =  1; d[1] = -b; d[2] = -a; break;
                    case 1:  d[0] = -1; d[1] = -b; d[2] =  a; break;
                    case 2:  d[0] =  a; d[1] =  1; d[2] =  b; break;
                    case 3:  d[0] =  a; d[1] = -1; d[2] = -b; break;
                    case 4:  d[0] =  a; d[1] = -b; d[2] =  1; break;
                    default: d[0] = -a; d[1] = -b; d[2] = -1; break;
                }
                const float l = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
                const float z = d[2] / l;
                const float u = 0.5f + atan2f(d[1], d[0]) * 0.15915494f, v = acosf(z < -1.0f ? -1.0f : z > 1.0f ? 1.0f : z) * 0.31830989f;
                // bilineal: a lo ancho da la vuelta, a lo alto se corta en los polos
                const float fx = u * (float)w - 0.5f, fy = v * (float)h - 0.5f;
                int x0 = (int)floorf(fx), y0 = (int)floorf(fy);
                const float tx = fx - (float)x0, ty = fy - (float)y0;
                int x1 = x0 + 1, y1 = y0 + 1;
                x0 = ((x0 % w) + w) % w; x1 = ((x1 % w) + w) % w;
                if (y0 < 0) y0 = 0; if (y1 < 0) y1 = 0; if (y0 >= h) y0 = h - 1; if (y1 >= h) y1 = h - 1;
                unsigned char* o = &caras[f][((size_t)j * lado + i) * 4];
                for (int k = 0; k < 3; k++) {
                    const float p00 = rgba[((size_t)y0 * w + x0) * 4 + k], p10 = rgba[((size_t)y0 * w + x1) * 4 + k];
                    const float p01 = rgba[((size_t)y1 * w + x0) * 4 + k], p11 = rgba[((size_t)y1 * w + x1) * 4 + k];
                    const float vv = (p00 * (1 - tx) + p10 * tx) * (1 - ty) + (p01 * (1 - tx) + p11 * tx) * ty;
                    o[k] = (unsigned char)(vv + 0.5f);
                }
                o[3] = 255;
            }
        ptr[f] = &caras[f][0];
    }
    if (c.tex) gfx::DeleteTexture(c.tex);
    c.tex = gfx::SubirCubo(ptr, lado);
    c.de = g_renderBgTex->iID;
    return c.tex;
}
static EntornoCopia gCuboNitido = { 0, 0 }, gCuboBorroso = { 0, 0 };
// el NITIDO: 64 por cara (en una esfera de un telefono, un texel por pixel), desde la imagen entera
static unsigned int EntornoCuboNitido() {
#ifndef W3D_SIN_EDITOR
    if (!g_renderBgTex || !g_renderBgTex->iID) return 0;
    if (gCuboNitido.tex && gCuboNitido.de == g_renderBgTex->iID) return gCuboNitido.tex;
    TexturaEditable* te = TexEditObtener(g_renderBgTex);
    if (!te || te->w <= 0 || te->h <= 0) return 0;
    return CuboDe(gCuboNitido, 64, &te->rgba[0], te->w, te->h);
#else
    return 0;
#endif
}
// el BORROSO: 16 por cara, desde la copia borrosa de 64x32
static unsigned int EntornoCuboBorroso() {
    if (!g_renderBgTex || !g_renderBgTex->iID) return 0;
    if (gCuboBorroso.tex && gCuboBorroso.de == g_renderBgTex->iID) return gCuboBorroso.tex;
    const IrrFondo* irr = IrradianciaDelFondo();   // (la irradiancia, como la borrosa de arriba)
    if (!irr) return 0;
    return CuboDe(gCuboBorroso, 16, &irr->px[0], kIrrW, kIrrH);
}
// el ENTORNO para la luz por pixel: la matriz ojo -> imagen (la vista de ESTE pase invertida por la rotacion del fondo)
// LA VISTA DE ESTE PASE al reves (ojo -> mundo, 3x3 por columnas): la sonda de luz evalua su luz con la normal en el
// mundo (objects/SondaLuz.cpp). Se toma aca: el pase todavia no movio la matriz (es la de la camara)
float g_w3dOjoAMundo[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
static bool gEnPase = false;
// EL ENTORNO DE LA ESCENA de este pase (el que pone EntornoPase): el de cada malla PBR que no tiene una sonda de reflejo
// vuelve a este (EntornoObjeto)
static unsigned int gEnvEscTex = 0, gEnvEscTexB = 0;
static bool gEnvEscCubo = false;
static float gEnvEscC[9];
static const SondaReflejo* gEnvSonda = NULL;   // la sonda puesta ahora (NULL = el de la escena)
static void EntornoEscena(unsigned int tex, unsigned int texB, const float* C, bool cubo) {
    gEnvEscTex = tex; gEnvEscTexB = texB; gEnvEscCubo = cubo && tex;
    for (int i = 0; i < 9; i++) gEnvEscC[i] = C ? C[i] : 0.0f;
    gEnvSonda = NULL;
    if (gEnvEscCubo) gfx::EntornoCubo(tex, texB, gEnvEscC); else gfx::Entorno(tex, texB, tex ? gEnvEscC : NULL);
}
// LA COPIA BORROSA de la imagen de una sonda de reflejo (la irradiancia, 64 x 32): la luz difusa y los reflejos rugosos
// del PBR, como la del fondo. Se rehace cuando la sonda cambia de imagen (version). Sin los pixeles (el juego compilado)
// queda la imagen misma
static unsigned int SondaBorrosa(SondaReflejo* s, Texture* t) {
    if (s->texBorrosa && s->versionBorrosa == s->version) return s->texBorrosa;   // (la de tiempo real la deja lista)
#ifndef W3D_SIN_EDITOR
    TexturaEditable* te = TexEditObtener(t);
    if (!te || te->w <= 0 || te->h <= 0 || (int)te->rgba.size() < te->w * te->h * 4) return 0;
    std::vector<unsigned char> px; float prom[3];
    IrradianciaDe(&te->rgba[0], te->w, te->h, px, prom);
    if (s->texBorrosa) gfx::DeleteTexture(s->texBorrosa);
    s->texBorrosa = gfx::UploadRGBA(&px[0], kIrrW, kIrrH, true, false, false, false);
    s->versionBorrosa = s->version;
    return s->texBorrosa;
#else
    (void)s; (void)t; return 0;
#endif
}
// EL ENTORNO DE UNA MALLA PBR (Mesh::AplicarMaterial, por el gancho): LA SONDA DE REFLEJO que la contiene -la de mas
// importancia- le da los reflejos y la luz difusa del entorno, como en Unity (sin elegirla en el material). Afuera de
// todas, el fondo de la escena. (Las GPU chicas, con el entorno en cubemap, siguen con el fondo)
static bool gCapturandoSonda = false;   // dibujando las caras de una sonda de tiempo real (mas abajo)
static void EntornoObjeto(Object* o) {
    if (!gEnPase || !o || gfx::GpuLimitada()) return;
    if (gCapturandoSonda) return;   // (lo que ve la sonda refleja el fondo: no se ve a si misma)
    Matrix4 W; o->GetWorldMatrix(W);
    Vector3 cl(0, 0, 0);   // (el centro de la malla; con esqueleto, el de su ultima pose: ver SondaLuz.cpp)
    if (o->getType() == ObjectType::mesh) {
        const Mesh* me = (const Mesh*)o;
        if (me->skinArmature && me->skinAabbOk) cl = (me->skinAabbMin + me->skinAabbMax) * 0.5f;
        else if (me->aabbOk) cl = (me->aabbMin + me->aabbMax) * 0.5f;
    }
    const Vector3 cw = W * cl;
    const float p[3] = { cw.x, cw.y, cw.z };
    SondaReflejo* s = SondaReflejoPara(p, true);
    Texture* t = s ? s->Imagen() : NULL;
    if (!t || !t->iID) {
        if (gEnvSonda) EntornoEscena(gEnvEscTex, gEnvEscTexB, gEnvEscC, gEnvEscCubo);
        return;
    }
    // la imagen de la sonda: u = 0.5 + atan2(z, y... del mundo (z, x)) / 2pi, v = acos(y) / pi (SondaReflejo.h). El
    // shader lee q = C * d (d en OJO) con u = atan2(q.y, q.x), v = acos(q.z): C = P * (ojo -> mundo), P = (x, z, y)
    static const float P[9] = { 1, 0, 0,  0, 0, 1,  0, 1, 0 };
    float C[9];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            C[i * 3 + j] = P[i * 3 + 0] * g_w3dOjoAMundo[j * 3 + 0] + P[i * 3 + 1] * g_w3dOjoAMundo[j * 3 + 1] +
                           P[i * 3 + 2] * g_w3dOjoAMundo[j * 3 + 2];
    const unsigned int b = SondaBorrosa(s, t);
    gfx::Entorno(t->iID, b ? b : t->iID, C);
    gEnvSonda = s;
}
extern void (*g_w3dEntornoObjetoHook)(Object*);   // (objects/Mesh.cpp)
namespace { struct InstalarEntornoObjeto { InstalarEntornoObjeto() { g_w3dEntornoObjetoHook = EntornoObjeto; } }; }
static InstalarEntornoObjeto gInstalarEntornoObjeto;
static void EntornoPase() {
    { float V[16]; gfx::GetMatrix(gfx::ModelView, V);
      for (int c = 0; c < 3; c++) for (int r = 0; r < 3; r++) g_w3dOjoAMundo[c * 3 + r] = V[r * 4 + c]; }
    if (!w3dRenderLuces || !W3dFondoConTextura()) { EntornoEscena(0, 0, NULL, false); return; }
    float V[16]; gfx::GetMatrix(gfx::ModelView, V);   // (la vista de la camara: el pase todavia no movio la matriz)
    float M[9]; W3dFondoMatriz(M);
    float C[9];   // C = M * Rv^T (Rv: la rotacion de la vista, mundo -> ojo; V esta por columnas)
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            C[i * 3 + j] = M[i * 3 + 0] * V[0 * 4 + j] + M[i * 3 + 1] * V[1 * 4 + j] + M[i * 3 + 2] * V[2 * 4 + j];
    if (gfx::GpuLimitada()) {   // (una GPU chica: el entorno como cubemap, sin las cuentas de la imagen 360)
        const unsigned int cn = EntornoCuboNitido(), cb = EntornoCuboBorroso();
        if (cn && cb) { EntornoEscena(cn, cb, C, true); return; }
    }
    EntornoEscena(EntornoNitido(), EntornoBorroso(), C, false);
}

// el cielo se vuelve a dibujar cuando una RUTINA limpia el color de la pantalla en medio del pase (la "Limpiar pantalla"
// de la escena de fabrica lo borraba: el cielo se dibuja antes que la escena). El del render, solo adentro del pase con
// las luces (Render y el juego: donde se ve el fondo); uno PROPIO del paso (su imagen, su giro), en cualquier pase
static bool gCieloEnPase = false;
void W3dEscena3DCieloTrasLimpiar(Texture* tex, const float* giro) {
    if (tex) { if (gEnPase) DibujarCielo(tex, giro); }
    else if (gCieloEnPase) DibujarCielo();
}
extern void (*g_cieloTrasLimpiarHook)(Texture*, const float*);   // (objects/Rutina.cpp)
static struct CieloGancho { CieloGancho() { g_cieloTrasLimpiarHook = W3dEscena3DCieloTrasLimpiar; } } gCieloGancho;

// ---------------------------------------------------------------------------
//  LAS SONDAS DE REFLEJO DE TIEMPO REAL: LAS DIBUJA LA GPU
//  Sin rayos: la escena se dibuja desde el centro de la sonda con el MISMO pase de siempre (las luces, el fondo, el PBR,
//  lo que se mueve), una vez por cada cara de un cubo (90 grados) en una imagen fuera de pantalla (gfx::CapturaEmpezar).
//  Se lee y se arma la imagen 360 (la misma que la horneada: u = 0.5 + atan2(z, x) / 2pi, v = acos(y) / pi) y su copia
//  borrosa para el PBR. Cada 'cadaSeg' segundos (0 = cada cuadro). Sin sombras en tiempo real (costarian un mapa por
//  cara), sin las rutinas (ponen su camara) y sin lo que no sale en las sondas (Object::enSondas). Sin la captura (el
//  pipeline fijo, la PSP, la 3DS, las GPU chicas) queda la ultima imagen
// ---------------------------------------------------------------------------
extern unsigned int w3dGetTicks();
static std::vector<unsigned char> gCapPx, gCapIrr;
// LA TABLA de la imagen 360 (un tamanio): de cada pixel, el de abajo a la izquierda de sus 4 en las caras (base: el
// indice en la imagen de las 6 caras), si el de al lado y el de arriba existen (dx, dy: 0 en el borde de la cara) y los
// pesos (0..256)
struct TablaSonda {
    struct Px { unsigned base; unsigned char dx, dy; unsigned short wx, wy; };
    int W, H, cara; std::vector<Px> px;
    TablaSonda() : W(0), H(0), cara(0) {}
};
static TablaSonda gTablaSonda;
static TablaSonda& TablaDe(int W, int H, int cara, const Vector3* ejeR, const Vector3* ejeU, const Vector3* ejeF) {
    TablaSonda& tb = gTablaSonda;
    if (tb.W == W && tb.H == H && tb.cara == cara && !tb.px.empty()) return tb;
    tb.W = W; tb.H = H; tb.cara = cara; tb.px.resize((size_t)W * H);
    const int AW = cara * 3;
    for (int y = 0; y < H; y++) {
        const float te = W3D_PI * ((float)y + 0.5f) / (float)H, st = sinf(te), ct = cosf(te);
        for (int x = 0; x < W; x++) {
            const float fi = 2.0f * W3D_PI * (((float)x + 0.5f) / (float)W - 0.5f);
            const Vector3 d(st * cosf(fi), ct, st * sinf(fi));
            int f = 0; float mejor = -2.0f;
            for (int k = 0; k < 6; k++) { const float p = d.x * ejeF[k].x + d.y * ejeF[k].y + d.z * ejeF[k].z; if (p > mejor) { mejor = p; f = k; } }
            if (mejor < 1e-6f) mejor = 1e-6f;
            const float a = (d.x * ejeR[f].x + d.y * ejeR[f].y + d.z * ejeR[f].z) / mejor;
            const float b = (d.x * ejeU[f].x + d.y * ejeU[f].y + d.z * ejeU[f].z) / mejor;
            float fx = (a * 0.5f + 0.5f) * cara - 0.5f, fy = (b * 0.5f + 0.5f) * cara - 0.5f;   // (filas de abajo)
            if (fx < 0.0f) fx = 0.0f; if (fy < 0.0f) fy = 0.0f;
            if (fx > cara - 1.0f) fx = cara - 1.0f; if (fy > cara - 1.0f) fy = cara - 1.0f;
            const int x0 = (int)fx, y0 = (int)fy;
            TablaSonda::Px& t = tb.px[(size_t)y * W + x];
            t.base = (unsigned)(((f / 3) * cara + y0) * AW + (f % 3) * cara + x0);
            t.dx = (unsigned char)(x0 + 1 < cara ? 1 : 0); t.dy = (unsigned char)(y0 + 1 < cara ? 1 : 0);
            t.wx = (unsigned short)((fx - x0) * 256.0f + 0.5f); t.wy = (unsigned short)((fy - y0) * 256.0f + 0.5f);
        }
    }
    return tb;
}
// las 6 caras: hacia donde mira (F) y su arriba (U)
static const float kCaraF[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
static const float kCaraU[6][3] = { { 0, 1, 0 }, { 0, 1, 0 }, { 0, 0, -1 }, { 0, 0, 1 }, { 0, 1, 0 }, { 0, 1, 0 } };
static bool CapturarSonda(SondaReflejo* s) {
    const int W = s->resolucion < 64 ? 64 : (s->resolucion > 1024 ? 1024 : s->resolucion), H = W / 2;
    const int cara = W / 4;   // (el ecuador de la imagen: un cuarto del ancho por cara)
    if (!gfx::CapturaEmpezar(cara * 3, cara * 2)) return false;
    float c[3]; s->CentroMundo(c);
    // LO QUE EL PASE TOCA y se repone: las matrices, la vista publicada al Core (el culling, el chrome), la luz del
    // normal map y los modos del render
    float P0[16], M0[16]; gfx::GetMatrix(gfx::Projection, P0); gfx::GetMatrix(gfx::ModelView, M0);
    const Vector3 camPos = g_renderCamPos, camR = g_renderCamRight, camU = g_renderCamUp, camF = g_renderCamForward;
    const Quaternion camRot = g_renderCamRot;
    const float fov0 = g_renderCamFov, near0 = g_renderCamNear, far0 = g_renderCamFar, asp0 = g_renderCamAspect;
    const float alto0 = g_renderImagenAltoPx; const bool orto0 = g_renderCamOrto, bind0 = g_vistaBindeada;
    float marco0[4]; for (int k = 0; k < 4; k++) marco0[k] = g_renderMarco[k];
    const Vector3 luzP = g_renderLightPos, luzC = g_renderLightColor;
    const bool ov0 = w3dRenderOverlays, fin0 = gfx::w3dRenderFinal;
    w3dRenderOverlays = false; gfx::w3dRenderFinal = true; gfx::w3dPaseSonda = true; gCapturandoSonda = true;
    gfx::ClearColor(g_renderBg[0], g_renderBg[1], g_renderBg[2], 1.0f);
    gfx::Enable(gfx::ScissorTest);
    Vector3 ejeR[6], ejeU[6], ejeF[6];
    for (int f = 0; f < 6; f++) {
        const int x = (f % 3) * cara, y = (f / 3) * cara;
        gfx::Viewport(x, y, cara, cara); gfx::Scissor(x, y, cara, cara);
        gfx::Clear(gfx::ColorBuffer | gfx::DepthBuffer);
        const Vector3 F(kCaraF[f][0], kCaraF[f][1], kCaraF[f][2]), U(kCaraU[f][0], kCaraU[f][1], kCaraU[f][2]);
        const Vector3 R(F.y * U.z - F.z * U.y, F.z * U.x - F.x * U.z, F.x * U.y - F.y * U.x);   // (F x U)
        Matrix4 B; B.Identity();   // la rotacion de la camara: sus ejes (x = R, y = U, z = -F) en el mundo, por columnas
        B.m[0] = R.x; B.m[1] = R.y; B.m[2] = R.z; B.m[4] = U.x; B.m[5] = U.y; B.m[6] = U.z;
        B.m[8] = -F.x; B.m[9] = -F.y; B.m[10] = -F.z;
        W3dVista3D v;
        v.fov = 90.0f; v.nearC = s->cerca > 0.01f ? s->cerca : 0.01f;
        v.farC = far0 > v.nearC * 2.0f ? far0 : 1000.0f;
        v.aspectoVista = v.aspectoImagen = 1.0f; v.altoImagenPx = (float)cara;
        v.cam.pos = Vector3(c[0], c[1], c[2]); v.cam.rot = Quaternion::FromMatrix(B);
        v.cam.fov = v.fov; v.cam.nearZ = v.nearC; v.cam.farZ = v.farC;
        // (los ejes de lo que se dibujo, de la rotacion misma: la imagen 360 los lee igual)
        ejeR[f] = v.cam.rot * Vector3(1, 0, 0); ejeU[f] = v.cam.rot * Vector3(0, 1, 0); ejeF[f] = v.cam.rot * Vector3(0, 0, -1);
        W3dEscena3DProyeccion(v);
        W3dEscena3DCamara(v, luzP, luzC);
        W3dEscena3DPasada();
    }
    gfx::Disable(gfx::ScissorTest);
    gCapPx.resize((size_t)cara * 3 * cara * 2 * 4);
    gfx::ReadPixelsRGBA(0, 0, cara * 3, cara * 2, &gCapPx[0]);
    gfx::CapturaTerminar();   // (el viewport, el recorte y el color de borrado de antes)
    w3dRenderOverlays = ov0; gfx::w3dRenderFinal = fin0; gfx::w3dPaseSonda = false; gCapturandoSonda = false;
    gfx::MatrixMode(gfx::Projection); gfx::LoadMatrix(P0);
    gfx::MatrixMode(gfx::ModelView); gfx::LoadMatrix(M0);
    { CameraBase cb; cb.pos = camPos; cb.rot = camRot; W3dVistaBind(cb); }
    g_renderCamRight = camR; g_renderCamUp = camU; g_renderCamForward = camF; g_vistaBindeada = bind0;
    g_renderCamFov = fov0; g_renderCamNear = near0; g_renderCamFar = far0; g_renderCamAspect = asp0;
    g_renderCamOrto = orto0; g_renderImagenAltoPx = alto0;
    for (int k = 0; k < 4; k++) g_renderMarco[k] = marco0[k];
    g_renderLightPos = luzP; g_renderLightColor = luzC;
    // LA IMAGEN 360: de cada pixel, la cara que mas lo mira y su punto (bilineal, sin salirse de la cara). La cuenta de
    // las direcciones (senos, cosenos, la cara) es la misma en cada cuadro: queda en una TABLA por tamanio
    const int AW = cara * 3;
    TablaSonda& tb = TablaDe(W, H, cara, ejeR, ejeU, ejeF);
    const unsigned in = (unsigned)((s->intensidad < 0.0f ? 0.0f : (s->intensidad > 16.0f ? 16.0f : s->intensidad)) * 256.0f + 0.5f);
    std::vector<unsigned char>& eq = s->vivoPx;
    eq.resize((size_t)W * H * 4);
    for (size_t i = 0; i < (size_t)W * H; i++) {
        const TablaSonda::Px& t = tb.px[i];
        const unsigned char* p00 = &gCapPx[(size_t)t.base * 4];
        const unsigned char* p10 = p00 + t.dx * 4;
        const unsigned char* p01 = p00 + (size_t)t.dy * AW * 4;
        const unsigned char* p11 = p01 + t.dx * 4;
        const unsigned wx = t.wx, wy = t.wy, ux = 256 - wx, uy = 256 - wy;
        unsigned char* o = &eq[i * 4];
        for (int k = 0; k < 3; k++) {
            const unsigned v = ((p00[k] * ux + p10[k] * wx) * uy + (p01[k] * ux + p11[k] * wx) * wy) >> 8;   // (x 256)
            const unsigned vi = (v * in) >> 16;
            o[k] = (unsigned char)(vi > 255 ? 255 : vi);
        }
        o[3] = 255;
    }
    if (s->vivo.iID && (s->vivo.ancho != W || s->vivo.alto != H)) { gfx::DeleteTexture(s->vivo.iID); s->vivo.iID = 0; }
    if (!s->vivo.iID) {
        s->vivo.iID = gfx::UploadRGBA(&eq[0], W, H, true, false);
        s->vivo.ancho = W; s->vivo.alto = H; s->vivo.path = "sonda:" + s->name;
    } else gfx::UpdateRGBA(s->vivo.iID, 0, 0, W, H, &eq[0], false);
    // (la copia borrosa: la luz difusa y los reflejos rugosos del PBR)
    float prom[3];
    IrradianciaDe(&eq[0], W, H, gCapIrr, prom);
    if (!s->texBorrosa) s->texBorrosa = gfx::UploadRGBA(&gCapIrr[0], kIrrW, kIrrH, true, false, false, false);
    else gfx::UpdateRGBA(s->texBorrosa, 0, 0, kIrrW, kIrrH, &gCapIrr[0], false);
    s->version++; s->versionBorrosa = s->version;
    return true;
}
static void SondasDeTiempoReal() {
    if (gCapturandoSonda || gfx::GpuLimitada()) return;
    // (solo en las vistas con la luz de la escena -Render, el juego- o Material: en Solido o alambre no se usan)
    if (w3dRenderWireframe || w3dRenderSolido || w3dRenderNormalColor || w3dRenderSinLuz || w3dRenderAlpha) return;
    std::vector<SondaReflejo*> ss; SondaReflejoListar(ss);
    const unsigned ahora = w3dGetTicks();
    for (size_t i = 0; i < ss.size(); i++) {
        SondaReflejo* s = ss[i];
        if (!s || !s->visible || s->tipo != W3D_REFLEJO_TIEMPO_REAL) continue;
        // (cada 'cadaSeg'; 0 = cada cuadro, hasta 60 por segundo: varios viewports en el mismo cuadro la dibujan una vez)
        unsigned cada = (unsigned)((s->cadaSeg > 0.0f ? s->cadaSeg : 0.0f) * 1000.0f);
        if (cada < 15) cada = 15;
        if (s->vivo.iID && s->ultimoMs && ahora - s->ultimoMs < cada) continue;
        // (sin pedir otro cuadro: se dibuja cuando el editor redibuja -algo cambio- y en cada cuadro del juego; la
        //  imagen nueva ya la usa el pase que la pidio. Si no se pudo, no lo vuelve a intentar en cada cuadro)
        CapturarSonda(s);
        s->ultimoMs = ahora ? ahora : 1;
    }
}

// ---------------------------------------------------------------------------
//  EL PASE
// ---------------------------------------------------------------------------
void W3dEscena3DFondoArchivo() {
    gCieloEnPase = w3dRenderLuces;
    gEnPase = true;
    EntornoPase();                       // el fondo 360 para los reflejos y la luz por pixel
    if (w3dRenderLuces) DibujarCielo();  // el CIELO: antes que todo, detras de todo (sin esto el render salia negro)
}
void W3dEscena3DFondoArchivoFin() {
    gCieloEnPase = false;
    gEnPase = false;
    gfx::Entorno(0, 0, NULL);
    gEnvSonda = NULL;
}
void W3dEscena3DPasada() {
    if (!SceneCollection || !W3dEscena3DActiva()) return;
    SondasDeTiempoReal();   // (antes que nada: las de tiempo real que tocan, con el mismo pase, fuera de pantalla)
    g_w3dPase++;   // (Mesh::paseDibujado: lo que una rutina dibujo en este cuadro)
    gfx::StatScope cat(gfx::StatCatEscena);   // (el contador de llamadas: esto es la escena)
    // por si un pase anterior dejo anotados sin dibujar
    W3dParticulasLimpiarPendientes();
    W3dDecalesLimpiarPendientes();
    W3dLucesLimpiarPendientes();
    // resync del cache de estado UNA vez por frame, ANTES del recorrido: lo que se
    // dibujo antes (chrome del editor, HUD del frame anterior) pudo tocar GL crudo.
    // En SOLO JUEGO con CONSTRUCTOR no: el estado que dejo el constructor se repone por el cache (la foto) y un
    // cache "desconocido" obligaria a re-mandar todo en cada cuadro (sin chrome del editor no hay GL crudo:
    // lo vigila 'glaudit')
    if (!(g_w3dSoloJuego && W3dHayConstructor())) gfx::Invalidate();
    w3dLoteStamp++;              // sello del pase: las Collection con lote marcan a sus horneadas
    W3dNieblaReiniciar();               // la NIEBLA la prende el objeto Niebla del arbol (desde ahi en adelante)
    gCieloEnPase = w3dRenderLuces;
    gEnPase = true;
    EntornoPase();                      // el fondo 360 para los reflejos y la luz por pixel (o apagado)
    if (w3dRenderLuces) SondaLuzNuevoPase();   // (las sondas de luz que se dibujen en este pase iluminan)
    if (!gCapturandoSonda) W3dSombrasPase();   // LAS SOMBRAS EN TIEMPO REAL: sus mapas, con la vista de la camara (antes del cielo)
    if (w3dRenderLuces) DibujarCielo();  // el FONDO 360 (Rendered / el juego): antes que todo, detras de todo
    W3dLucesPrepase(SceneCollection);   // las luces ANTES de la geometria (el fogonazo ilumina todo)
    SceneCollection->Render();
#ifndef W3D_SIN_EDITOR
    // LAS OTRAS ESCENAS ABIERTAS en el editor (varias a la vez, W3dRaices.h): se ven juntas, en su orden, con las luces
    // de la activa. Jugando no: el Play dibuja su base y lo que cargo encima (cargarEscena)
    if (!W3dScriptHayPartida()) {
        std::vector<Object*> otras;
        W3dRaicesOtrasVisibles(otras);
        for (size_t i = 0; i < otras.size(); i++) otras[i]->Render();
    }
#endif
    W3dLucesPrepaseFin();
    // CALCOMANIAS (sombras) y LUCES ADITIVAS (chispas/halos): no escriben z, asi que
    // van DESPUES de toda la escena opaca o cualquier opaco posterior las borra.
    W3dDecalesDibujarPendientes();
    W3dLucesDibujarPendientes();
    // PARTICULAS translucidas al final, con el z-buffer de la escena ya escrito.
    W3dParticulasDibujarPendientes();
    gCieloEnPase = false;
    gEnPase = false;
    W3dSombrasFin();                    // (lo que se dibuja despues -overlays, HUD- no tiene sombras)
    gfx::Entorno(0, 0, NULL);           // (lo que se dibuja despues -overlays, HUD- no refleja el fondo)
    gEnvSonda = NULL;
    W3dNieblaReiniciar();               // nada de niebla para lo que se dibuja despues (overlays, HUD, UI)
    gfx::EstadoBase();                  // ni el alpha test, el sesgo o la matriz de textura que dejo la escena
    if (!gCapturandoSonda) W3dCapturasHacer();   // las capturas que pidio lua: la imagen 3D sola (el HUD todavia no)
}

// ---------------------------------------------------------------------------
//  CAPTURAS DE PANTALLA (ver EscenaRender.h)
// ---------------------------------------------------------------------------
namespace {
struct Captura {
    Texture* tex;
    unsigned id;          // su textura GL (para saber si el puntero sigue siendo la misma)
    unsigned gen;         // g_w3dTexturasGen cuando se verifico que sigue viva
    int texW, texH;       // lo reservado (potencias de dos)
    int w, h;             // lo copiado la ultima vez (0 = nunca)
    bool pedida, conHud;
    Captura() : tex(NULL), id(0), gen(0), texW(0), texH(0), w(0), h(0), pedida(false), conHud(false) {}
};
std::map<std::string, Captura> gCapturas;
int gMarcoPase[4] = { 0, 0, 0, 0 };   // el cuadro de la imagen del ultimo pase 3D (el HUD se dibuja con otro viewport)
const char kPrefijoCaptura[] = "pantalla:";
const size_t kLargoPrefijo = sizeof(kPrefijoCaptura) - 1;

std::string SinPrefijo(const std::string& n) {
    if (n.compare(0, kLargoPrefijo, kPrefijoCaptura) == 0) return n.substr(kLargoPrefijo);
    // (una ruta que ya paso por el cache de texturas: lo que sigue al prefijo)
    size_t k = n.find(kPrefijoCaptura);
    return k == std::string::npos ? n : n.substr(k + kLargoPrefijo);
}

int Pot(int v) { int p = 16; while (p < v && p < 2048) p <<= 1; return p; }

// la textura sigue viva: al cerrar el proyecto el cache libera todas las suyas (y con ellas las capturas)
bool Viva(Captura& c) {
    if (!c.tex) return false;
    if (c.gen == g_w3dTexturasGen) return true;
    for (size_t i = 0; i < Textures.size(); i++)
        if (Textures[i] == c.tex && c.tex->iID == c.id) { c.gen = g_w3dTexturasGen; return true; }
    c.tex = NULL; c.id = 0; c.w = c.h = 0;
    return false;
}

// el cuadro de la imagen en pixeles de la ventana (GL: y desde abajo): el viewport y el encuadre de la camara
// (g_renderMarco, lo publica W3dEscena3DProyeccion), la misma cuenta que los Recortes
void Marco(int r[4]) {
    int vp[4]; gfx::GetViewport(vp);
    const float mw = vp[2] * g_renderMarco[2], mh = vp[3] * g_renderMarco[3];
    const float mx0 = vp[0] + vp[2] * 0.5f * (1.0f + g_renderMarco[0]) - mw * 0.5f;
    const float my0 = vp[1] + vp[3] * 0.5f * (1.0f + g_renderMarco[1]) - mh * 0.5f;
    int x0 = (int)floorf(mx0 + 0.5f), y0 = (int)floorf(my0 + 0.5f);
    int x1 = (int)floorf(mx0 + mw + 0.5f), y1 = (int)floorf(my0 + mh + 0.5f);
    if (x0 < vp[0]) x0 = vp[0];
    if (y0 < vp[1]) y0 = vp[1];
    if (x1 > vp[0] + vp[2]) x1 = vp[0] + vp[2];
    if (y1 > vp[1] + vp[3]) y1 = vp[1] + vp[3];
    r[0] = x0; r[1] = y0; r[2] = x1 - x0; r[3] = y1 - y0;
}

Captura* Buscar(const std::string& nombre, bool crear) {
    const std::string n = SinPrefijo(nombre);
    if (n.empty()) return NULL;
    std::map<std::string, Captura>::iterator it = gCapturas.find(n);
    if (it == gCapturas.end()) {
        if (!crear) return NULL;
        it = gCapturas.insert(std::make_pair(n, Captura())).first;
    }
    Captura& c = it->second;
    if (!Viva(c) && crear) {
        int r[4]; Marco(r);
        c.texW = Pot(r[2] > 0 ? r[2] : 64); c.texH = Pot(r[3] > 0 ? r[3] : 64);
        Texture* t = new Texture();
        t->iID = gfx::ReservarPantalla(0, c.texW, c.texH);
        t->ancho = c.texW; t->alto = c.texH;
        if (!t->iID || !TexturaRegistrar(std::string(kPrefijoCaptura) + n, t)) { delete t; return NULL; }
        c.tex = t; c.id = t->iID; c.gen = g_w3dTexturasGen; c.w = c.h = 0;
    }
    return &c;
}

void Copiar(Captura& c, const int* rect = NULL) {
    int r[4];
    if (rect) { for (int i = 0; i < 4; i++) r[i] = rect[i]; } else Marco(r);
    if (r[2] < 1 || r[3] < 1) return;
    if (r[2] > c.texW || r[3] > c.texH) {
        // la pantalla crecio (o giro): la misma textura, mas grande (los que la usan tienen el mismo puntero)
        if (r[2] > c.texW) c.texW = Pot(r[2]);
        if (r[3] > c.texH) c.texH = Pot(r[3]);
        gfx::ReservarPantalla(c.id, c.texW, c.texH);
        c.tex->ancho = c.texW; c.tex->alto = c.texH;
    }
    gfx::CopiarPantalla(c.id, r[0], r[1], r[2], r[3]);
    c.w = r[2]; c.h = r[3];
}
}

bool W3dEsRutaCaptura(const std::string& ruta) {
    return ruta.find(kPrefijoCaptura) != std::string::npos;
}

Texture* W3dCapturaTextura(const std::string& nombre, bool crear) {
    Captura* c = Buscar(nombre, crear);
    return (c && Viva(*c)) ? c->tex : NULL;
}

void W3dCapturaPedir(const std::string& nombre, bool conHud) {
    Captura* c = Buscar(nombre, true);
    if (c) { c->pedida = true; c->conHud = conHud; }
}

bool W3dCapturaCopiarYa(const std::string& nombre) {
    Captura* c = Buscar(nombre, true);
    if (!c || !c->tex) return false;
    Copiar(*c);
    return true;
}

bool W3dCapturaUV(const std::string& nombre, float uv[4]) {
    Captura* c = Buscar(nombre, false);
    if (!c || c->pedida || !Viva(*c) || c->w < 1 || c->h < 1) return false;   // (pedida: todavia no se dibujo)
    uv[0] = 0.0f; uv[1] = (float)c->h / (float)c->texH;
    uv[2] = (float)c->w / (float)c->texW; uv[3] = 0.0f;
    // (un driver que la copia al reves: la fila 0 ya es la de arriba)
    if (gfx::CopiaPantallaDadaVuelta()) { uv[3] = uv[1]; uv[1] = 0.0f; }
    return true;
}

void W3dCapturasHacer() {
    Marco(gMarcoPase);
    for (std::map<std::string, Captura>::iterator it = gCapturas.begin(); it != gCapturas.end(); ++it) {
        Captura& c = it->second;
        if (!c.pedida || c.conHud) continue;
        c.pedida = false;
        if (!Viva(c)) Buscar(it->first, true);
        if (c.tex) Copiar(c);
    }
}

void W3dCapturasHacerConHud() {
    for (std::map<std::string, Captura>::iterator it = gCapturas.begin(); it != gCapturas.end(); ++it) {
        Captura& c = it->second;
        if (!c.pedida || !c.conHud) continue;
        c.pedida = false;
        if (!Viva(c)) Buscar(it->first, true);
        if (c.tex) Copiar(c, gMarcoPase);
    }
}

// ---------------------------------------------------------------------------
//  BANDAS del encuadre
// ---------------------------------------------------------------------------
// true si el marco cubre todo el rect (menos de medio pixel de banda por lado): no hay nada que tapar
bool W3dEscena3DSinBandas(float W, float H, float marcoNX, float marcoNY, float zoom, float panX, float panY) {
    const float cx = (panX * 0.5f + 0.5f) * W, cy = (0.5f - panY * 0.5f) * H;
    const float hw = marcoNX * zoom * W * 0.5f, hh = marcoNY * zoom * H * 0.5f;
    return cx - hw <= 0.5f && cy - hh <= 0.5f && cx + hw >= W - 0.5f && cy + hh >= H - 0.5f;
}

void W3dEscena3DBandas(float W, float H, float marcoNX, float marcoNY,
                       float zoom, float panX, float panY, bool opaco) {
    if (W < 1.0f || H < 1.0f) return;
    // el marco sigue el zoom/pan de inspeccion (la MISMA transform que la proyeccion)
    float cx = (panX * 0.5f + 0.5f) * W;
    float cy = (0.5f - panY * 0.5f) * H;
    float hw = marcoNX * zoom * W * 0.5f, hh = marcoNY * zoom * H * 0.5f;
    float x0 = cx - hw, x1 = cx + hw, y0 = cy - hh, y1 = cy + hh;
    // acotadas al rect (el marco puede quedar parcialmente afuera al panear/zoomear)
    float bx0 = x0<0?0:(x0>W?W:x0), bx1 = x1<0?0:(x1>W?W:x1);
    float by0 = y0<0?0:(y0>H?H:y0), by1 = y1<0?0:(y1>H?H:y1);
    // el marco ocupa todo (el aspecto del juego es el de la pantalla): no hay bandas que dibujar
    if (W3dEscena3DSinBandas(W, H, marcoNX, marcoNY, zoom, panX, panY)) return;
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

// ============================================================================
//  UIOverlay.cpp — ver UIOverlay.h.
// ============================================================================
#include "render/UIOverlay.h"
#include "objects/Objects.h"
#include "objects/UI.h"
#include "objects/Elemento2D.h"
#include "objects/Texto2D.h"
#include "objects/Imagen2D.h"
#include "objects/Rect2D.h"
#include "objects/Contenedor2D.h"
#include "objects/Slice9.h"
#include "objects/Boton2D.h"
#include "objects/Expandir2D.h"
#include "objects/Video2D.h"
#include "io/Fuente2D.h"
#include "io/Textura2D.h"
#include "io/Video2DCache.h"
#include "WhiskUI/text/W3dTextAtlas.h"
#include "WhiskUI/draw/glesdraw.h"  // W3dPantallaAlto (el scissor de GL mide desde abajo)
#include "w3dGraphics.h"
#include <stdio.h>    // snprintf (formato de los textos number/float)
#include <stdlib.h>   // atof
#include "render/OpcionesRender.h"  // g_redraw: el video animandose pide redibujar
#include "ViewPorts/Properties.h"   // PropsActivo->renderW/H: la ventana ES el tamano del render

namespace gfx = w3dEngine;

void UI2D_TamanoVentana(float* w, float* h) {
    // la fuente de verdad es el tamano del RENDER: editarlo se ve al instante en el Editor 2D
    *w = PropsActivo ? PropsActivo->renderW : 640.0f;
    *h = PropsActivo ? PropsActivo->renderH : 480.0f;
}

UI* UI2D_UIDelEditor() {
    // el UI de la cadena del activo gana (estas trabajando en el); sino el primero en escena
    for (Object* o = ObjActivo; o; o = o->Parent)
        if (o->getType() == ObjectType::ui) return (UI*)o;
    if (!SceneCollection) return NULL;
    for (size_t i = 0; i < SceneCollection->Childrens.size(); i++) {
        Object* o = SceneCollection->Childrens[i];
        if (o && o->getType() == ObjectType::ui) return (UI*)o;
    }
    return NULL;
}

void UI2D_TamanoLienzo(float* w, float* h) {
    UI* u = UI2D_UIDelEditor();
    if (u && !u->igualQueRender) { *w = u->ancho; *h = u->alto; return; }
    UI2D_TamanoVentana(w, h);
}

bool UI2D_EsElemento2D(Object* o) {
    if (!o) return false;
    ObjectType t = o->getType();
    return t == ObjectType::texto2d || t == ObjectType::imagen2d ||
           t == ObjectType::rect2d  || t == ObjectType::cont2d ||
           t == ObjectType::slice9  || t == ObjectType::boton2d ||
           t == ObjectType::expandir2d || t == ObjectType::video2d;
}

// cast comodo: valido despues de chequear UI2D_EsElemento2D (comparten la base Elemento2D)
static Elemento2D* E2(Object* o) { return (Elemento2D*)o; }

float* UI2D_Rot2dDe(Object* o) {
    return UI2D_EsElemento2D(o) ? &E2(o)->rot2d : NULL;
}

bool UI2D_TamanoElem(Object* o, float** w, float** h) {
    // texto y boton no tienen rect propio (lo calculan de su contenido); expandir tampoco
    if (!UI2D_EsElemento2D(o)) return false;
    ObjectType t = o->getType();
    if (t == ObjectType::texto2d || t == ObjectType::boton2d || t == ObjectType::expandir2d)
        return false;
    *w = &E2(o)->ancho; *h = &E2(o)->alto;
    return true;
}

void UI2D_PuntoAncla(int ancla, float x0, float y0, float w, float h, float* ax, float* ay) {
    // fracciones (fx,fy) del rect por ancla: 0=centro 1=izq 2=der 3=arriba 4=abajo 5..8=esquinas
    float fx = 0.5f, fy = 0.5f;
    switch (ancla) {
        case 1: fx = 0.0f; break;               // izquierda (centro vertical)
        case 2: fx = 1.0f; break;               // derecha
        case 3: fy = 0.0f; break;               // arriba (centro horizontal)
        case 4: fy = 1.0f; break;               // abajo
        case 5: fx = 0.0f; fy = 0.0f; break;    // arriba-izquierda
        case 6: fx = 1.0f; fy = 0.0f; break;    // arriba-derecha
        case 7: fx = 0.0f; fy = 1.0f; break;    // abajo-izquierda
        case 8: fx = 1.0f; fy = 1.0f; break;    // abajo-derecha
        default: break;                          // 0 = centro
    }
    *ax = x0 + w * fx;
    *ay = y0 + h * fy;
}

// ---- accesores UI-o-elemento (el UI no deriva de Elemento2D: es la ventana) ----------------
static float OpacidadDe(Object* o) {
    return UI2D_EsElemento2D(o) ? E2(o)->opacidad : 1.0f;
}
// el padding POR LADO, ya en px de pantalla (proporcional: fraccion del LADO MENOR)
static void PaddingLados(Object* o, float escala, float rw, float rh,
                         float* izq, float* der, float* arr, float* aba) {
    float i, d, a, b;
    bool px;
    if (o->getType() == ObjectType::ui) {
        UI* u = (UI*)o;
        i = u->padIzq; d = u->padDer; a = u->padArr; b = u->padAba; px = u->padGapPx;
    } else {
        Elemento2D* e = E2(o);
        i = e->padIzq; d = e->padDer; a = e->padArr; b = e->padAba; px = e->padGapPx;
    }
    float menor = (rw < rh) ? rw : rh;
    float f = px ? escala : menor;
    *izq = i * f; *der = d * f; *arr = a * f; *aba = b * f;
}
static int     LayoutDe(Object* o)   { return (o->getType() == ObjectType::ui) ? ((UI*)o)->layoutHijos : E2(o)->layoutHijos; }
static float   GapDe(Object* o)      { return (o->getType() == ObjectType::ui) ? ((UI*)o)->gap         : E2(o)->gap; }
static bool    PadGapPxDe(Object* o) { return (o->getType() == ObjectType::ui) ? ((UI*)o)->padGapPx    : E2(o)->padGapPx; }
static bool    RecortaXDe(Object* o) { return (o->getType() == ObjectType::ui) ? ((UI*)o)->recortaX    : E2(o)->recortaX; }
static bool    RecortaYDe(Object* o) { return (o->getType() == ObjectType::ui) ? ((UI*)o)->recortaY    : E2(o)->recortaY; }
static bool    ConScrollDe(Object* o){ return (o->getType() == ObjectType::ui) ? ((UI*)o)->conScroll   : E2(o)->conScroll; }
static float   ScrollXDe(Object* o)  { return (o->getType() == ObjectType::ui) ? ((UI*)o)->scrollX     : E2(o)->scrollX; }
static float   ScrollYDe(Object* o)  { return (o->getType() == ObjectType::ui) ? ((UI*)o)->scrollY     : E2(o)->scrollY; }

static int LayoutAjusteDe(Object* o) {
    return (o->getType() == ObjectType::ui) ? ((UI*)o)->layoutAjuste : E2(o)->layoutAjuste;
}
static int LayoutAlignDe(Object* o) {
    return (o->getType() == ObjectType::ui) ? ((UI*)o)->layoutAlign : E2(o)->layoutAlign;
}

// el UI cuyo arbol se esta dibujando (para resolver los colores de PALETA)
static UI* gUIRaiz = NULL;
// el ZOOM de la vista (el del Editor 2D; 1 en el 3D): el texto pixel se snapea al multiplo
// entero de su tamano LOGICO y despues escala SUAVE con la vista (sin el "tuc" de x1 a x2)
static float gZoomVista = 1.0f;

// snap del tamano del texto pixel: entero en unidades LOGICAS (sin el zoom de la vista)
static float PxSnap(const w3dui::W3dTextAtlas* at, float px, bool paraAbajo) {
    if (!at->pixelPerfect) return px;
    float z = (gZoomVista > 0.0001f) ? gZoomVista : 1.0f;
    float logico = px / z;
    int m = paraAbajo ? (int)(logico / at->fontPx) : (int)(logico / at->fontPx + 0.5f);
    if (m < 1) m = 1;
    return m * at->fontPx * z;
}
// el pen solo se redondea con la vista en zoom ENTERO (ahi el pixel-perfect es exacto)
static bool ZoomEntero() {
    float z = gZoomVista;
    float e = z - (float)(int)(z + 0.5f);
    return e > -0.001f && e < 0.001f;
}
// pal >= 0: el color con nombre de la paleta; sino el color PROPIO del elemento
static const float* ColorResuelto(int pal, const float* propio) {
    if (pal >= 0 && gUIRaiz) {
        std::vector<PaletaColor>& cs = gUIRaiz->Colores();
        if (pal < (int)cs.size()) return cs[pal].rgba;
    }
    return propio;
}

// el CONTENIDO del texto segun su tipo: string tal cual, number entero,
// float con N decimales (0 = sin decimales). Para los HUD numericos.
static std::string TextoSegunTipo(Texto2D* t) {
    if (t->tipo == 0) return t->texto;
    double v = atof(t->texto.c_str());
    int dec = (t->tipo == 1) ? 0 : (int)(t->decimales + 0.5f);
    if (dec < 0) dec = 0; if (dec > 8) dec = 8;
    char buf[64];
    snprintf(buf, sizeof(buf), "%.*f", dec, v);
    return std::string(buf);
}

// ---- TEXTO MULTILINEA ----------------------------------------------------------------------
// parte el texto en lineas al ancho maxW: 0 = una sola linea, 1 = salta por PALABRAS
// (espacios), 2 = salta desde cualquier parte (por codepoint, sin romper UTF-8)
static void PartirLineas(const w3dui::W3dTextAtlas* at, const std::string& txt, float px,
                         float maxW, int modo, std::vector<std::string>& out) {
    out.clear();
    if (modo == 0 || maxW <= 0.0f) { out.push_back(txt); return; }
    std::string linea;
    if (modo == 1) {
        size_t i = 0;
        while (i <= txt.size()) {
            size_t sp = txt.find(' ', i);
            std::string pal = (sp == std::string::npos) ? txt.substr(i) : txt.substr(i, sp - i);
            std::string prueba = linea.empty() ? pal : linea + " " + pal;
            if (!linea.empty() && at->TextWidth(prueba.c_str(), px) > maxW) {
                out.push_back(linea);
                linea = pal;
            } else linea = prueba;
            if (sp == std::string::npos) break;
            i = sp + 1;
        }
    } else {
        size_t i = 0;
        while (i < txt.size()) {
            size_t j = i + 1;   // avanzar un CODEPOINT (los bytes de continuacion son 10xxxxxx)
            while (j < txt.size() && ((unsigned char)txt[j] & 0xC0) == 0x80) j++;
            std::string prueba = linea + txt.substr(i, j - i);
            if (!linea.empty() && at->TextWidth(prueba.c_str(), px) > maxW) {
                out.push_back(linea);
                linea = txt.substr(i, j - i);
            } else linea = prueba;
            i = j;
        }
    }
    if (!linea.empty() || out.empty()) out.push_back(linea);
}

// el tamano de fuente mas grande con el que el texto ENTRA en el area rw x rh
// (biseccion: 'entra' es monotono con el tamano)
static float PxQueEntra(const w3dui::W3dTextAtlas* at, const std::string& txt,
                        int modo, float rw, float rh) {
    float lo = 2.0f, hi = rh;
    std::vector<std::string> lin;
    for (int it = 0; it < 12; it++) {
        float mid = (lo + hi) * 0.5f;
        PartirLineas(at, txt, mid, (modo != 0) ? rw : 1e9f, modo, lin);
        float maxW = 0.0f;
        for (size_t i = 0; i < lin.size(); i++) {
            float w = at->TextWidth(lin[i].c_str(), mid);
            if (w > maxW) maxW = w;
        }
        float bh = at->LineHeight(mid) * lin.size();
        if (maxW <= rw && bh <= rh) lo = mid; else hi = mid;
    }
    return lo;
}

// ---- RECORTE (overflow) --------------------------------------------------------------------
// el scissor de GL es ABSOLUTO: los que dibujan el overlay declaran su viewport aca.
// gClip = el rect de recorte vigente en coords LOCALES del viewport (arranca en el completo).
static int   gBaseX = 0, gBaseY = 0, gBaseW = 0, gBaseH = 0;
static float gClip[4] = { 0, 0, 0, 0 };   // x0, y0, x1, y1

static void AplicarScissorLocal(float x0, float y0, float x1, float y1) {
    if (x1 < x0) x1 = x0;
    if (y1 < y0) y1 = y0;
    int w = (int)(x1 - x0), h = (int)(y1 - y0);
    gfx::Scissor(gBaseX + (int)x0, W3dPantallaAlto - (gBaseY + (int)y0) - h, w, h);
}

void UI2D_BaseRecorte(int vx, int vy, int vw, int vh) {
    gBaseX = vx; gBaseY = vy; gBaseW = vw; gBaseH = vh;
    gClip[0] = 0; gClip[1] = 0; gClip[2] = (float)vw; gClip[3] = (float)vh;
}

// ---- dibujo de contenido -------------------------------------------------------------------
// estado 2D para un quad (el texto arma el suyo en Begin())
static void EstadoQuad(unsigned tex, bool conDepth) {
    if (conDepth) gfx::Enable(gfx::DepthTest);   // modo mundo: la profundidad Z importa
    else          gfx::Disable(gfx::DepthTest);
    gfx::Disable(gfx::Lighting); gfx::Disable(gfx::CullFace);
    gfx::Enable(gfx::Blend); gfx::BlendAlpha();
    gfx::EnableArray(gfx::VertexArray);
    gfx::DisableArray(gfx::ColorArray); gfx::DisableArray(gfx::NormalArray);
    if (tex) {
        gfx::Enable(gfx::Texture2D); gfx::BindTexture(tex);
        gfx::EnableArray(gfx::TexCoordArray);
    } else {
        gfx::Disable(gfx::Texture2D);
        gfx::DisableArray(gfx::TexCoordArray);
    }
}

// quad (x0,y0)-(x1,y1) con UV (u0,v0)-(u1,v1)
static void QuadUV(float x0, float y0, float x1, float y1,
                   float u0, float v0, float u1, float v1) {
    float V[12] = { x0,y0, x1,y0, x1,y1,  x0,y0, x1,y1, x0,y1 };
    float U[12] = { u0,v0, u1,v0, u1,v1,  u0,v0, u1,v1, u0,v1 };
    gfx::VertexPointer2f(0, V);
    gfx::TexCoordPointer2f(0, U);
    gfx::DrawTrianglesArray(6);
}

// una TEXTURA dentro de un rect segun el modo (el nucleo que comparten imagen y video):
// 0 = estirar, 1 = ajustar (entera, con bandas), 2 = cover (llena recortando)
static void DibujarTexturaModo(unsigned tex, int iw, int ih, int modo,
                               float x0, float y0, float x1, float y1) {
    float rw = x1 - x0, rh = y1 - y0;
    if (rw < 1e-6f || rh < 1e-6f) return;
    if (modo == 1) {
        float e = rw / iw; if (rh / ih < e) e = rh / ih;
        float w = iw * e, h = ih * e;
        float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f;
        QuadUV(cx - w * 0.5f, cy - h * 0.5f, cx + w * 0.5f, cy + h * 0.5f, 0, 0, 1, 1);
    } else if (modo == 2) {
        float e = rw / iw; if (rh / ih > e) e = rh / ih;
        float du = (rw / e) / iw * 0.5f;   // mitad del ancho visible, en UV
        float dv = (rh / e) / ih * 0.5f;
        QuadUV(x0, y0, x1, y1, 0.5f - du, 0.5f - dv, 0.5f + du, 0.5f + dv);
    } else {
        QuadUV(x0, y0, x1, y1, 0, 0, 1, 1);
    }
}

// dibuja la textura de una Imagen2D dentro del rect (x0,y0)-(x1,y1) segun su modo.
// Sin textura: un gris translucido para que el rect se vea igual.
static void DibujarImagenRect(Imagen2D* im, float x0, float y0, float x1, float y1,
                              bool conDepth, float op) {
    int iw = 0, ih = 0;
    unsigned tex = Textura2DObtener(im->textura, &iw, &ih);
    EstadoQuad(tex, conDepth);
    if (!tex || iw < 1 || ih < 1) {
        gfx::Color4f(0.5f, 0.5f, 0.55f, 0.35f * op);   // placeholder: elegi una textura
        QuadUV(x0, y0, x1, y1, 0, 0, 1, 1);
        return;
    }
    gfx::TexFilter(im->filtrado);                   // sin filtro = NEAREST pixel-perfect
    if (!im->usarAlpha) gfx::Disable(gfx::Blend);   // sin canal alpha: se dibuja opaca
    const float* c = ColorResuelto(im->palTinte, im->color);   // TINTE (propio o de paleta)
    gfx::Color4f(c[0], c[1], c[2], c[3] * op);
    DibujarTexturaModo(tex, iw, ih, im->modo, x0, y0, x1, y1);
}

// el FRAME actual del video dentro del rect. Con 'reproducir' avanza solo (y pide
// redibujar: el editor es por eventos); sin loop queda clavado en el ultimo frame.
static void DibujarVideoRect(Video2D* vd, float x0, float y0, float x1, float y1,
                             bool conDepth, float op) {
    const VideoPreview* v = Video2DPreview(vd->video);
    if (!v || v->numFrames < 1) {
        EstadoQuad(0, conDepth);
        gfx::Color4f(0.25f, 0.25f, 0.3f, 0.5f * op);   // placeholder: elegi un video
        QuadUV(x0, y0, x1, y1, 0, 0, 1, 1);
        return;
    }
    int f = 0;
    if (vd->reproducir && v->numFrames > 1) {
        unsigned ms = W3dMsAhora();
        if (vd->t0ms == 0) vd->t0ms = ms;
        unsigned df = (unsigned)((ms - vd->t0ms) * v->fps / 1000.0f);
        f = vd->loop ? (int)(df % (unsigned)v->numFrames)
                     : ((int)df >= v->numFrames ? v->numFrames - 1 : (int)df);
        g_redraw = true;   // animandose: el proximo frame tambien se dibuja
    } else vd->t0ms = 0;
    EstadoQuad(v->frames[f], conDepth);
    gfx::TexFilter(vd->filtrado);
    if (!vd->usarAlpha) gfx::Disable(gfx::Blend);   // sin transparencia: opaco
    gfx::Color4f(1.0f, 1.0f, 1.0f, op);
    DibujarTexturaModo(v->frames[f], v->w, v->h, vd->modo, x0, y0, x1, y1);
}

// el borde del slice9 EN LA TEXTURA, clampeado: minimo 1, maximo ceil(N/2)-1 por eje
// (siempre tiene que quedar al menos 1 px de centro para estirar; en 5x5 el maximo es 2)
static void BordesTexturaSlice9(Slice9* s9, int iw, int ih, float* bX, float* bY) {
    float maxBX = (float)((iw + 1) / 2 - 1); if (maxBX < 1.0f) maxBX = 1.0f;
    float maxBY = (float)((ih + 1) / 2 - 1); if (maxBY < 1.0f) maxBY = 1.0f;
    *bX = s9->bordeX; if (*bX < 1.0f) *bX = 1.0f; if (*bX > maxBX) *bX = maxBX;
    *bY = s9->bordeY; if (*bY < 1.0f) *bY = 1.0f; if (*bY > maxBY) *bY = maxBY;
}

void UI2D_CortesSlice9(Slice9* s9, float x0, float y0, float x1, float y1,
                       float escalaDest, float* dX, float* dY) {
    int iw = 0, ih = 0;
    Textura2DObtener(s9->textura, &iw, &ih);
    if (iw < 1) iw = 16;
    if (ih < 1) ih = 16;
    float bX, bY;
    BordesTexturaSlice9(s9, iw, ih, &bX, &bY);
    // el borde DIBUJADO, por eje (esquinas rectangulares si bordeX != bordeY);
    // nunca pasa de la mitad del rect (sino los pedazos se cruzan)
    *dX = bX * s9->escalaBorde * escalaDest;
    *dY = bY * s9->escalaBorde * escalaDest;
    float maxDX = (x1 - x0) * 0.5f, maxDY = (y1 - y0) * 0.5f;
    if (*dX > maxDX) *dX = maxDX;
    if (*dY > maxDY) *dY = maxDY;
}

float UI2D_EscalaGlobalDe(Object* o) {
    for (Object* p = o; p; p = p->Parent)
        if (p->getType() == ObjectType::ui) {
            float gs = ((UI*)p)->escalaGlobal;
            return gs >= 0.25f ? gs : 1.0f;
        }
    return 1.0f;
}

// SLICE 9: la textura partida en 9 (esquinas fijas, lados estirados en un eje, centro
// estirado). 'escalaDest' = px de pantalla por px de lienzo (el borde dibujado mide
// borde * escalaBorde * eso).
static void DibujarSlice9Rect(Slice9* s9, float x0, float y0, float x1, float y1,
                              bool conDepth, float op, float escalaDest) {
    int iw = 0, ih = 0;
    unsigned tex = Textura2DObtener(s9->textura, &iw, &ih);
    EstadoQuad(tex, conDepth);
    if (!tex || iw < 1 || ih < 1) {
        gfx::Color4f(0.5f, 0.5f, 0.55f, 0.35f * op);   // placeholder: elegi una textura
        QuadUV(x0, y0, x1, y1, 0, 0, 1, 1);
        return;
    }
    gfx::TexFilter(s9->filtrado);                   // sin filtro = NEAREST pixel-perfect
    const float* c9 = ColorResuelto(s9->palTinte, s9->color);   // TINTE (propio o de paleta)
    gfx::Color4f(c9[0], c9[1], c9[2], c9[3] * op);
    float bX, bY;
    BordesTexturaSlice9(s9, iw, ih, &bX, &bY);
    float dX, dY;
    UI2D_CortesSlice9(s9, x0, y0, x1, y1, escalaDest, &dX, &dY);
    float bu = bX / iw, bv = bY / ih;
    float xs[4] = { x0, x0 + dX, x1 - dX, x1 };
    float ys[4] = { y0, y0 + dY, y1 - dY, y1 };
    float us[4] = { 0.0f, bu, 1.0f - bu, 1.0f };
    float vs[4] = { 0.0f, bv, 1.0f - bv, 1.0f };
    for (int j = 0; j < 3; j++)
        for (int i = 0; i < 3; i++)
            QuadUV(xs[i], ys[j], xs[i+1], ys[j+1], us[i], vs[j], us[i+1], vs[j+1]);
}

// quad de color solido del RECTANGULO (alpha 0 = invisible)
static void DibujarRectSolido(Rect2D* r, float x0, float y0, float x1, float y1,
                              bool conDepth, float op) {
    const float* c = ColorResuelto(r->palColor, r->color);
    float a = c[3] * op;
    if (a <= 0.0f) return;
    EstadoQuad(0, conDepth);   // mismo estado 2D, sin textura
    gfx::Color4f(c[0], c[1], c[2], a);
    QuadUV(x0, y0, x1, y1, 0, 0, 1, 1);
}

static void DibujarElemRec(Object* o, float rx0, float ry0, float rw, float rh,
                           float escala, std::vector<UI2DPos>* outPos, float op, int celdaEje);
static float MedirNatural(Object* o, float escala, float rw, float rh, bool ejeX);


// reparte los HIJOS en el rect interior del padre segun su layout: libre (cada uno con su
// ancla y posicion), o filas/columnas (celdas del 100% del area repartidas por PESO, con gap)
static void DibujarHijos2D(Object* padre, float rx0, float ry0, float rw, float rh,
                           float escala, std::vector<UI2DPos>* outPos, float op) {
    int lay = LayoutDe(padre);
    if (lay == 0) {
        for (size_t i = 0; i < padre->Childrens.size(); i++)
            DibujarElemRec(padre->Childrens[i], rx0, ry0, rw, rh, escala, outPos, op, 0);
        return;
    }
    int n = 0;
    float sumaPeso = 0.0f;
    for (size_t i = 0; i < padre->Childrens.size(); i++) {
        Object* h = padre->Childrens[i];
        if (!UI2D_EsElemento2D(h) || !h->visible) continue;
        float pe = E2(h)->peso; if (pe < 0.01f) pe = 0.01f;
        sumaPeso += pe;
        n++;
    }
    if (n < 1) return;
    // gap proporcional: fraccion del LADO MENOR (mide lo mismo en filas que en columnas)
    float menor = (rw < rh) ? rw : rh;
    float gap = PadGapPxDe(padre) ? GapDe(padre) * escala : GapDe(padre) * menor;
    bool ejeX = (lay == 2);
    float total = (ejeX ? rw : rh) - gap * (n - 1);
    if (total < 0.0f) total = 0.0f;

    // el MARGEN de un hijo, en px de pantalla: aire ALREDEDOR de su celda (como CSS).
    // Proporcional = fraccion del lado menor del area del padre, igual que el gap.
    float factorMarg = PadGapPxDe(padre) ? escala : menor;
    #define MARGEN4(h, mi, md, ma, mb) { Elemento2D* _e = E2(h); \
        mi = _e->margIzq * factorMarg; md = _e->margDer * factorMarg; \
        ma = _e->margArr * factorMarg; mb = _e->margAba * factorMarg; }

    if (LayoutAjusteDe(padre) == 1) {
        // ---- ajuste MINIMO: cada hijo su tamano natural (+ su margen); el sobrante lo
        // absorben los EXPANDIR y los elementos con el checkbox "expandir" prendido ----
        float suma = 0.0f, sumaPesoExp = 0.0f;
        int nExp = 0;
        for (size_t i = 0; i < padre->Childrens.size(); i++) {
            Object* h = padre->Childrens[i];
            if (!UI2D_EsElemento2D(h) || !h->visible) continue;
            float mi, md, ma, mb; MARGEN4(h, mi, md, ma, mb);
            suma += ejeX ? (mi + md) : (ma + mb);
            if (h->getType() == ObjectType::expandir2d) {
                float pe = E2(h)->peso; if (pe < 0.01f) pe = 0.01f;
                sumaPesoExp += pe; nExp++;
            } else {
                suma += MedirNatural(h, escala, rw, rh, ejeX);
                if (E2(h)->expandir) {
                    float pe = E2(h)->peso; if (pe < 0.01f) pe = 0.01f;
                    sumaPesoExp += pe; nExp++;
                }
            }
        }
        float libre = total - suma;
        if (libre < 0.0f) libre = 0.0f;
        // sin expandibles el bloque entero se ALINEA: 0 = inicio, 1 = centro, 2 = fin
        float ini = 0.0f;
        if (nExp == 0) {
            int al = LayoutAlignDe(padre);
            if (al == 1) ini = libre * 0.5f;
            else if (al == 2) ini = libre;
        }
        float avX = rx0 + (ejeX ? ini : 0.0f);
        float avY = ry0 + (ejeX ? 0.0f : ini);
        for (size_t i = 0; i < padre->Childrens.size(); i++) {
            Object* h = padre->Childrens[i];
            if (!UI2D_EsElemento2D(h) || !h->visible) continue;
            float mi, md, ma, mb; MARGEN4(h, mi, md, ma, mb);
            bool exp2d = (h->getType() == ObjectType::expandir2d);
            float extra = 0.0f;
            if ((exp2d || E2(h)->expandir) && nExp > 0) {
                float pe = E2(h)->peso; if (pe < 0.01f) pe = 0.01f;
                extra = libre * pe / sumaPesoExp;
            }
            float tam = (exp2d ? 0.0f : MedirNatural(h, escala, rw, rh, ejeX)) + extra;
            // la celda va INSETEADA por el margen (en el eje transversal tambien)
            if (ejeX) {
                float ch2 = rh - ma - mb; if (ch2 < 0.0f) ch2 = 0.0f;
                DibujarElemRec(h, avX + mi, ry0 + ma, tam, ch2, escala, outPos, op, 1);
                avX += mi + tam + md + gap;
            } else {
                float cw2 = rw - mi - md; if (cw2 < 0.0f) cw2 = 0.0f;
                DibujarElemRec(h, rx0 + mi, avY + ma, cw2, tam, escala, outPos, op, 2);
                avY += ma + tam + mb + gap;
            }
        }
        return;
    }

    // ---- ajuste ESTIRAR (default): se reparten el 100% por PESO. La celda del hijo
    // incluye su margen; el elemento se dibuja adentro, inseteado. ----
    float avance = rx0, avanceY = ry0;
    for (size_t i = 0; i < padre->Childrens.size(); i++) {
        Object* h = padre->Childrens[i];
        if (!UI2D_EsElemento2D(h) || !h->visible) continue;
        float pe = E2(h)->peso; if (pe < 0.01f) pe = 0.01f;
        float tam = total * pe / sumaPeso;   // el PESO reparte el espacio (no partes iguales)
        float mi, md, ma, mb; MARGEN4(h, mi, md, ma, mb);
        if (lay == 1) {   // FILAS: se apilan hacia abajo
            float cw2 = rw - mi - md; if (cw2 < 0.0f) cw2 = 0.0f;
            float th2 = tam - ma - mb; if (th2 < 0.0f) th2 = 0.0f;
            DibujarElemRec(h, rx0 + mi, avanceY + ma, cw2, th2, escala, outPos, op, 2);
            avanceY += tam + gap;
        } else {          // COLUMNAS: una al lado de la otra
            float ch2 = rh - ma - mb; if (ch2 < 0.0f) ch2 = 0.0f;
            float tw2 = tam - mi - md; if (tw2 < 0.0f) tw2 = 0.0f;
            DibujarElemRec(h, avance + mi, ry0 + ma, tw2, ch2, escala, outPos, op, 1);
            avance += tam + gap;
        }
    }
    #undef MARGEN4
}

// los hijos del objeto, con su padding, su RECORTE (overflow por eje) y su scroll aplicados.
// clip(x0..y1) = el rect del PADRE (donde se recorta); cont* = el area interior (con padding).
static void DibujarHijosRecortados(Object* padre, float clipX0, float clipY0,
                                   float clipX1, float clipY1,
                                   float contX0, float contY0, float contW, float contH,
                                   float escala, std::vector<UI2DPos>* outPos, float op) {
    bool cx = RecortaXDe(padre), cy = RecortaYDe(padre);
    float prev[4] = { gClip[0], gClip[1], gClip[2], gClip[3] };
    if (cx || cy) {
        // interseccion del recorte del padre con el vigente (los recortes se anidan)
        float nx0 = cx ? clipX0 : gClip[0], nx1 = cx ? clipX1 : gClip[2];
        float ny0 = cy ? clipY0 : gClip[1], ny1 = cy ? clipY1 : gClip[3];
        if (nx0 < gClip[0]) nx0 = gClip[0];
        if (ny0 < gClip[1]) ny0 = gClip[1];
        if (nx1 > gClip[2]) nx1 = gClip[2];
        if (ny1 > gClip[3]) ny1 = gClip[3];
        gClip[0] = nx0; gClip[1] = ny0; gClip[2] = nx1; gClip[3] = ny1;
        AplicarScissorLocal(nx0, ny0, nx1, ny1);
    }
    // el scroll corre el CONTENIDO (solo tiene sentido con el recorte del eje activo)
    if (ConScrollDe(padre)) {
        if (cx) contX0 -= ScrollXDe(padre) * escala;
        if (cy) contY0 -= ScrollYDe(padre) * escala;
    }
    DibujarHijos2D(padre, contX0, contY0, contW, contH, escala, outPos, op);
    if (cx || cy) {
        gClip[0] = prev[0]; gClip[1] = prev[1]; gClip[2] = prev[2]; gClip[3] = prev[3];
        AplicarScissorLocal(prev[0], prev[1], prev[2], prev[3]);
    }
}

// dibuja el bloque de TEXTO (con lineas y auto-ajuste) centrado en su ancla; devuelve
// el rect ocupado en outB[4]. rw/rh = area de referencia (para partir lineas y auto-tam).
static void DibujarTextoBloque(Texto2D* t, float sx, float sy, float rw, float rh,
                               float escala, float op, float* outB) {
    outB[0] = sx; outB[1] = sy; outB[2] = sx; outB[3] = sy;
    w3dui::W3dTextAtlas* at = Fuente2DObtener(t->fuente);
    std::string txt = TextoSegunTipo(t);
    if (!at || txt.empty()) return;
    float px = t->tam * escala;
    if (t->autoTam && rw > 4.0f && rh > 4.0f)
        px = PxQueEntra(at, txt, t->lineas, rw, rh);   // ajustar el tamano al area
    // fuente PIXEL: snap al multiplo entero LOGICO (la vista escala suave encima)
    px = PxSnap(at, px, t->autoTam);
    std::vector<std::string> lin;
    PartirLineas(at, txt, px, (t->lineas != 0) ? rw : 1e9f, t->lineas, lin);
    float lh = at->LineHeight(px);
    float bh = lh * lin.size();
    float maxW = 0.0f;
    std::vector<float> anchos(lin.size());
    for (size_t i = 0; i < lin.size(); i++) {
        anchos[i] = at->TextWidth(lin[i].c_str(), px);
        if (anchos[i] > maxW) maxW = anchos[i];
    }
    // el BLOQUE se acomoda alrededor del ancla con align X/Y; cada linea se alinea igual
    float ty = sy;
    if (t->alignV == 1) ty -= bh * 0.5f; else if (t->alignV == 2) ty -= bh;
    float bx = sx;
    if (t->alignH == 1) bx -= maxW * 0.5f; else if (t->alignH == 2) bx -= maxW;
    // fuente pixel: ADEMAS el pen cae en pixel ENTERO de pantalla (medio pixel = sangrado
    // del atlas: aparecen lineas del glifo vecino y columnas grises)
    if (at->pixelPerfect && ZoomEntero()) {   // pen entero SOLO con la vista en zoom entero
        ty = (float)(int)(ty + (ty >= 0 ? 0.5f : -0.5f));
        bx = (float)(int)(bx + (bx >= 0 ? 0.5f : -0.5f));
    }
    outB[0] = bx; outB[1] = ty; outB[2] = bx + maxW; outB[3] = ty + bh;
    const float* tc = ColorResuelto(t->palColor, t->color);
    at->Begin();
    for (size_t i = 0; i < lin.size(); i++) {
        float tx = sx;
        if (t->alignH == 1) tx -= anchos[i] * 0.5f; else if (t->alignH == 2) tx -= anchos[i];
        if (at->pixelPerfect && ZoomEntero()) tx = (float)(int)(tx + (tx >= 0 ? 0.5f : -0.5f));
        at->DrawText(lin[i].c_str(), tx, ty + lh * i, px,
                     tc[0], tc[1], tc[2], tc[3] * op);
    }
    at->End();
}

// mide el tamano NATURAL del boton (padding + icono + texto), en px de PANTALLA.
// rw/rh = rect de referencia (para el boton de solo arte con tamano RELATIVO).
static void MedirBoton(Boton2D* b, float escala, float rw, float rh, float* w, float* h) {
    w3dui::W3dTextAtlas* at = Fuente2DObtener(b->fuente);
    float px = b->tam * escala;
    if (at) px = PxSnap(at, px, false);
    float tw = (at && !b->texto.empty()) ? at->TextWidth(b->texto.c_str(), px) : 0.0f;
    float icoW = 0.0f, icoH = 0.0f;
    if (!b->icono.empty()) {
        int iw = 0, ih = 0;
        Textura2DObtener(b->icono, &iw, &ih);
        icoW = iw * escala; icoH = ih * escala;
    }
    float p = b->pad * escala;
    float contH = (px > icoH) ? px : icoH;
    *w = p * 2.0f + icoW + ((icoW > 0.0f && tw > 0.0f) ? p : 0.0f) + tw;
    *h = p * 2.0f + contH;
    // boton de SOLO ARTE (textura de fondo, sin texto ni icono): manda su propio
    // ancho/alto (midiendo el contenido quedaria de 8px, el pad solo)
    if (tw <= 0.0f && icoW <= 0.0f && !b->texturaFondo.empty()) {
        *w = b->tamPx ? b->ancho * escala : b->ancho * rw;
        *h = b->tamPx ? b->alto  * escala : b->alto  * rh;
    }
}

// la card del boton: fondo + borde de 1px (GEOMETRIA, como la barra del editor) + contenido
static void DibujarBoton(Boton2D* b, float x0, float y0, float x1, float y1,
                         float escala, float op) {
    const float* cFondo = ColorResuelto(b->palFondo, b->colorFondo);
    const float* cTexto = ColorResuelto(b->palTexto, b->colorTexto);
    const float* cBorde = ColorResuelto(b->palBorde, b->colorBorde);
    int tw9 = 0, th9 = 0;
    unsigned tex9 = b->texturaFondo.empty() ? 0 : Textura2DObtener(b->texturaFondo, &tw9, &th9);
    if (tex9 && tw9 > 0 && th9 > 0) {
        // FONDO con textura de 9 pedazos (como el slice9), tenida con el color de fondo
        EstadoQuad(tex9, false);
        gfx::TexFilter(false);
        gfx::Color4f(cFondo[0], cFondo[1], cFondo[2], cFondo[3] * op);
        float bX = b->bordeTexX, bY = b->bordeTexY;
        float mX = (float)((tw9 + 1) / 2 - 1); if (mX < 1) mX = 1;
        float mY = (float)((th9 + 1) / 2 - 1); if (mY < 1) mY = 1;
        if (bX < 1) bX = 1; if (bX > mX) bX = mX;
        if (bY < 1) bY = 1; if (bY > mY) bY = mY;
        float dX = bX * b->escalaBordeTex * escala, dY = bY * b->escalaBordeTex * escala;
        if (dX > (x1 - x0) * 0.5f) dX = (x1 - x0) * 0.5f;
        if (dY > (y1 - y0) * 0.5f) dY = (y1 - y0) * 0.5f;
        float bu = bX / tw9, bv = bY / th9;
        float xs[4] = { x0, x0 + dX, x1 - dX, x1 };
        float ys[4] = { y0, y0 + dY, y1 - dY, y1 };
        float us[4] = { 0, bu, 1.0f - bu, 1 };
        float vs[4] = { 0, bv, 1.0f - bv, 1 };
        for (int j = 0; j < 3; j++)
            for (int i = 0; i < 3; i++)
                QuadUV(xs[i], ys[j], xs[i+1], ys[j+1], us[i], vs[j], us[i+1], vs[j+1]);
    } else {
        // fondo plano
        EstadoQuad(0, false);
        gfx::Color4f(cFondo[0], cFondo[1], cFondo[2], cFondo[3] * op);
        QuadUV(x0, y0, x1, y1, 0, 0, 1, 1);
        // borde de 1px de lienzo (escalado): 4 rectangulos finos (GEOMETRIA, como el editor)
        float g = escala; if (g < 1.0f) g = 1.0f;
        gfx::Color4f(cBorde[0], cBorde[1], cBorde[2], cBorde[3] * op);
        QuadUV(x0, y0, x1, y0 + g, 0, 0, 1, 1);
        QuadUV(x0, y1 - g, x1, y1, 0, 0, 1, 1);
        QuadUV(x0, y0, x0 + g, y1, 0, 0, 1, 1);
        QuadUV(x1 - g, y0, x1, y1, 0, 0, 1, 1);
    }
    // contenido: icono a la izquierda + texto (centrados en vertical)
    float p = b->pad * escala;
    float cx = x0 + p;
    if (!b->icono.empty()) {
        int iw = 0, ih = 0;
        unsigned tex = Textura2DObtener(b->icono, &iw, &ih);
        if (tex) {
            float w = iw * escala, h = ih * escala;
            float cy = (y0 + y1 - h) * 0.5f;
            EstadoQuad(tex, false);
            gfx::TexFilter(false);   // los iconos pixelados, como el editor
            gfx::Color4f(cTexto[0], cTexto[1], cTexto[2],
                         cTexto[3] * op);   // el icono se TINE como el texto
            QuadUV(cx, cy, cx + w, cy + h, 0, 0, 1, 1);
            cx += w + p;
        }
    }
    if (!b->texto.empty()) {
        w3dui::W3dTextAtlas* at = Fuente2DObtener(b->fuente);
        if (at) {
            float px = b->tam * escala;
            px = PxSnap(at, px, false);
            float ty = (y0 + y1 - px) * 0.5f;
            if (at->pixelPerfect && ZoomEntero()) {
                cx = (float)(int)(cx + 0.5f);
                ty = (float)(int)(ty + 0.5f);
            }
            at->Begin();
            at->DrawText(b->texto.c_str(), cx, ty, px,
                         cTexto[0], cTexto[1], cTexto[2], cTexto[3] * op);
            at->End();
        }
    }
}

// el tamano NATURAL de un hijo sobre el eje del layout (px de pantalla). El eje cruzado
// siempre ocupa el 100%. Expandir devuelve 0 (absorbe el sobrante aparte).
static float MedirNatural(Object* o, float escala, float rw, float rh, bool ejeX) {
    ObjectType t = o->getType();
    if (t == ObjectType::expandir2d) return 0.0f;
    if (t == ObjectType::boton2d) {
        float w, h;
        MedirBoton((Boton2D*)o, escala, rw, rh, &w, &h);
        return ejeX ? w : h;
    }
    if (t == ObjectType::texto2d) {
        Texto2D* tx = (Texto2D*)o;
        w3dui::W3dTextAtlas* at = Fuente2DObtener(tx->fuente);
        std::string txt = TextoSegunTipo(tx);
        if (!at || txt.empty()) return 0.0f;
        float px = PxSnap(at, tx->tam * escala, false);
        return ejeX ? at->TextWidth(txt.c_str(), px) : px;
    }
    Elemento2D* e = E2(o);
    if (ejeX) return e->tamPx ? e->ancho * escala : e->ancho * rw;
    return e->tamPx ? e->alto * escala : e->alto * rh;
}

// dibuja un elemento y sus hijos. El rect (rx0,ry0,rw,rh) es el marco de REFERENCIA para el
// ancla (la ventana para los hijos directos del UI; el RECT del padre para los anidados).
// La POSICION se guarda RELATIVA a ese rect (1.0 = todo el ancho): cambiar el tamano de la
// ventana NO toca los valores, solo el resultado final. En una CELDA de filas/columnas la
// posicion se ignora y las imagenes/rectangulos/contenedores llenan la celda entera.
// La rotacion se aplica con la matriz: los HIJOS quedan adentro y la heredan; la OPACIDAD
// tambien (op = la acumulada de los padres).
// celdaEje: 0 = libre (con ancla+posicion); 1 = celda de un layout en COLUMNAS (el ancho
// de la celda ya es la medida del hijo); 2 = celda de FILAS (idem con el alto).
static void DibujarElemRec(Object* o, float rx0, float ry0, float rw, float rh,
                           float escala, std::vector<UI2DPos>* outPos, float op, int celdaEje) {
    if (!o || !o->visible || !UI2D_EsElemento2D(o)) return;
    bool enCelda = (celdaEje != 0);
    Elemento2D* e = E2(o);
    float rot = e->rot2d;
    op *= e->opacidad;

    float ax, ay;
    UI2D_PuntoAncla(e->ancla, rx0, ry0, rw, rh, &ax, &ay);
    float sx = enCelda ? ax : ax + o->pos.x * rw;
    float sy = enCelda ? ay : ay + o->pos.y * rh;
    size_t posIdx = 0;
    if (outPos) {
        UI2DPos p; p.obj = o; p.sx = sx; p.sy = sy;
        p.bx0 = sx; p.by0 = sy; p.bx1 = sx; p.by1 = sy;   // se completa abajo con el rect real
        p.refW = rw; p.refH = rh;
        p.cx0 = gClip[0]; p.cy0 = gClip[1]; p.cx1 = gClip[2]; p.cy1 = gClip[3];
        posIdx = outPos->size();
        outPos->push_back(p);
    }

    bool rotado = (rot != 0.0f);
    if (rotado) {
        gfx::MatrixMode(gfx::ModelView);
        gfx::PushMatrix();
        gfx::Translatef(sx, sy, 0.0f);
        gfx::Rotatef(rot, 0.0f, 0.0f, 1.0f);
        gfx::Translatef(-sx, -sy, 0.0f);
    }

    // el rect que OCUPA el elemento (bounds sin rotar): referencia de ancla para sus hijos
    float bx0 = sx, by0 = sy, bx1 = sx, by1 = sy;

    if (o->getType() == ObjectType::texto2d) {
        float B[4];
        DibujarTextoBloque((Texto2D*)o, sx, sy, rw, rh, escala, op, B);
        bx0 = B[0]; by0 = B[1]; bx1 = B[2]; by1 = B[3];
    } else if (o->getType() == ObjectType::boton2d) {
        Boton2D* b = (Boton2D*)o;
        // el boton se agarra por su CENTRO con su tamano NATURAL; en una celda usa su
        // natural CENTRADO y capado a la celda (llenarla deformaba el arte de fondo:
        // los botones redondos del slot salian estirados a lo alto de la barra)
        float w, h;
        MedirBoton(b, escala, rw, rh, &w, &h);
        if (enCelda) {
            // el EJE del layout ya viene medido en la celda (remedirlo contra la celda
            // achicaba los botones relativos a palitos); el transversal usa su natural
            if (celdaEje == 1) w = rw; else h = rh;
            if (w > rw) w = rw;
            if (h > rh) h = rh;
        }
        // boton de SOLO ARTE: mantiene el ASPECTO del archivo (un boton redondo no se
        // deforma cuando el lienzo cambia de proporcion): se ENCAJA en el rect medido
        if (b->texto.empty() && b->icono.empty() && !b->texturaFondo.empty()) {
            int aw9 = 0, ah9 = 0;
            Textura2DObtener(b->texturaFondo, &aw9, &ah9);
            if (aw9 > 0 && ah9 > 0 && w > 0.5f && h > 0.5f) {
                float asp = (float)aw9 / (float)ah9;
                if (w / h > asp) w = h * asp;
                else             h = w / asp;
            }
        }
        float ccx = enCelda ? rx0 + rw * 0.5f : sx;
        float ccy = enCelda ? ry0 + rh * 0.5f : sy;
        bx0 = ccx - w * 0.5f; by0 = ccy - h * 0.5f;
        bx1 = ccx + w * 0.5f; by1 = ccy + h * 0.5f;
        DibujarBoton(b, bx0, by0, bx1, by1, escala, op);
    } else if (o->getType() == ObjectType::expandir2d) {
        // invisible: solo ocupa su celda (para seleccionarlo en el editor)
        if (enCelda) { bx0 = rx0; by0 = ry0; bx1 = rx0 + rw; by1 = ry0 + rh; }
    } else {
        // imagen / rectangulo / contenedor / slice9: un rect agarrado al ancla por su
        // CENTRO (en una esquina se ve 1/4, en un borde 1/2); en una celda la llena
        // entera. Con tamPx apagado el tamano es RELATIVO al rect del padre (por eje).
        float aw = e->tamPx ? e->ancho * escala : e->ancho * rw;
        float ah = e->tamPx ? e->alto  * escala : e->alto  * rh;
        float hw = aw * 0.5f, hh = ah * 0.5f;
        if (enCelda) { bx0 = rx0; by0 = ry0; bx1 = rx0 + rw; by1 = ry0 + rh; }
        else         { bx0 = sx - hw; by0 = sy - hh; bx1 = sx + hw; by1 = sy + hh; }
        if (o->getType() == ObjectType::imagen2d)
            DibujarImagenRect((Imagen2D*)o, bx0, by0, bx1, by1, false, op);
        else if (o->getType() == ObjectType::video2d)
            DibujarVideoRect((Video2D*)o, bx0, by0, bx1, by1, false, op);
        else if (o->getType() == ObjectType::rect2d)
            DibujarRectSolido((Rect2D*)o, bx0, by0, bx1, by1, false, op);
        else if (o->getType() == ObjectType::slice9)
            DibujarSlice9Rect((Slice9*)o, bx0, by0, bx1, by1, false, op, escala);
        // el contenedor no dibuja nada: es invisible, solo ordena a sus hijos
    }
    if (outPos) {
        (*outPos)[posIdx].bx0 = bx0; (*outPos)[posIdx].by0 = by0;
        (*outPos)[posIdx].bx1 = bx1; (*outPos)[posIdx].by1 = by1;
    }

    // los hijos se anclan al RECT del padre (encogido por su padding POR LADO) y heredan
    // su rotacion y su opacidad; el layout decide como se reparten y el overflow recorta.
    float pi, pd, pa, pb;
    PaddingLados(o, escala, bx1 - bx0, by1 - by0, &pi, &pd, &pa, &pb);
    DibujarHijosRecortados(o, bx0, by0, bx1, by1,
                           bx0 + pi, by0 + pa,
                           (bx1 - bx0) - pi - pd, (by1 - by0) - pa - pb,
                           escala, outPos, op);

    if (rotado) { gfx::MatrixMode(gfx::ModelView); gfx::PopMatrix(); }
}

void UI2D_DibujarOverlay(float x0, float y0, float w, float h, float escala,
                         std::vector<UI2DPos>* outPos, bool saltarVerEn3D) {
    if (!SceneCollection) return;
    gZoomVista = (escala > 0.0001f) ? escala : 1.0f;
    for (size_t i = 0; i < SceneCollection->Childrens.size(); i++) {
        Object* o = SceneCollection->Childrens[i];
        if (!o || o->getType() != ObjectType::ui || !o->visible) continue;
        if (saltarVerEn3D && ((UI*)o)->verEn3D) continue;   // ese se esta viendo EN el mundo
        UI* u = (UI*)o;
        gUIRaiz = u;   // los colores de PALETA se resuelven contra este UI
        if (u->color[3] > 0.0f) {   // fondo de la ventana (transparente por defecto)
            EstadoQuad(0, false);
            gfx::Color4f(u->color[0], u->color[1], u->color[2], u->color[3] * u->opacidad);
            QuadUV(x0, y0, x0 + w, y0 + h, 0, 0, 1, 1);
        }
        // la ESCALA GLOBAL del UI multiplica todo lo que este en px (texto, tamanos,
        // padding, gap, bordes): x1 = N95, x3 = como se ve en PC. La ventana no cambia.
        float esc2 = escala * (u->escalaGlobal >= 0.25f ? u->escalaGlobal : 1.0f);
        float pi, pd, pa, pb;
        PaddingLados(o, esc2, w, h, &pi, &pd, &pa, &pb);
        DibujarHijosRecortados(o, x0, y0, x0 + w, y0 + h,
                               x0 + pi, y0 + pa, w - pi - pd, h - pa - pb,
                               esc2, outPos, u->opacidad);
    }
}

bool UI2D_HayVerEn3D() {
    if (!SceneCollection) return false;
    for (size_t i = 0; i < SceneCollection->Childrens.size(); i++) {
        Object* o = SceneCollection->Childrens[i];
        if (o && o->getType() == ObjectType::ui && o->visible && ((UI*)o)->verEn3D) return true;
    }
    return false;
}

// version MUNDO (ver en 3D): la ventana de referencia es el lienzo del UI parado en el
// origen (1 unidad = 100 px), y la profundidad Z del elemento se ve de verdad. Los
// elementos miran al frente (+Z). Sin recorte (el scissor no aplica en 3D) y el texto
// va en una sola linea: es un modo de INSPECCION de profundidad, no el render final.
static void DibujarElemMundoRec(Object* o, float rx0, float ry0, float rw, float rh,
                                float ventW, float ventH, float op, bool enCelda);

static void DibujarHijosMundo(Object* padre, float rx0, float ry0, float rw, float rh,
                              float ventW, float ventH, float op) {
    int lay = LayoutDe(padre);
    if (lay == 0) {
        for (size_t i = 0; i < padre->Childrens.size(); i++)
            DibujarElemMundoRec(padre->Childrens[i], rx0, ry0, rw, rh, ventW, ventH, op, false);
        return;
    }
    int n = 0;
    float sumaPeso = 0.0f;
    for (size_t i = 0; i < padre->Childrens.size(); i++) {
        Object* h = padre->Childrens[i];
        if (!UI2D_EsElemento2D(h) || !h->visible) continue;
        float pe = E2(h)->peso; if (pe < 0.01f) pe = 0.01f;
        sumaPeso += pe;
        n++;
    }
    if (n < 1) return;
    float menor = (rw < rh) ? rw : rh;
    float gap = PadGapPxDe(padre) ? GapDe(padre) : GapDe(padre) * menor;
    float libre = ((lay == 1) ? rh : rw) - gap * (n - 1);
    if (libre < 0.0f) libre = 0.0f;
    float avance = rx0, avanceY = ry0;
    for (size_t i = 0; i < padre->Childrens.size(); i++) {
        Object* h = padre->Childrens[i];
        if (!UI2D_EsElemento2D(h) || !h->visible) continue;
        float pe = E2(h)->peso; if (pe < 0.01f) pe = 0.01f;
        float tam = libre * pe / sumaPeso;
        if (lay == 1) { DibujarElemMundoRec(h, rx0, avanceY, rw, tam, ventW, ventH, op, true); avanceY += tam + gap; }
        else          { DibujarElemMundoRec(h, avance, ry0, tam, rh, ventW, ventH, op, true); avance += tam + gap; }
    }
}

static void DibujarElemMundoRec(Object* o, float rx0, float ry0, float rw, float rh,
                                float ventW, float ventH, float op, bool enCelda) {
    if (!o || !o->visible || !UI2D_EsElemento2D(o)) return;
    Elemento2D* e = E2(o);
    const float K = 1.0f / 100.0f;                 // px de lienzo -> unidades de mundo
    float rot = e->rot2d;
    op *= e->opacidad;

    float ax, ay;
    UI2D_PuntoAncla(e->ancla, rx0, ry0, rw, rh, &ax, &ay);
    // la posicion es RELATIVA al rect de referencia (en celda se ignora)
    float cxp = enCelda ? ax : ax + o->pos.x * rw;
    float cyp = enCelda ? ay : ay + o->pos.y * rh;   // en px de lienzo
    // en una celda las imagenes/rectangulos la llenan entera: su centro es el de la celda
    if (enCelda && o->getType() != ObjectType::texto2d) {
        cxp = rx0 + rw * 0.5f;
        cyp = ry0 + rh * 0.5f;
    }

    // lienzo -> mundo: x centrado en 0; y del lienzo (hacia abajo) -> altura (hacia arriba)
    float wx = (cxp - ventW * 0.5f) * K;
    float wy = (ventH - cyp) * K;
    float wz = o->pos.z * K;                    // la PROFUNDIDAD, visible al orbitar

    // rect del elemento en px de lienzo (referencia de ancla para sus hijos)
    float bx0 = cxp, by0 = cyp, bx1 = cxp, by1 = cyp;

    gfx::MatrixMode(gfx::ModelView);
    gfx::PushMatrix();
    gfx::Translatef(wx, wy, wz);
    if (rot != 0.0f) gfx::Rotatef(-rot, 0.0f, 0.0f, 1.0f);
    gfx::Scalef(1.0f, -1.0f, 1.0f);             // el contenido es Y-DOWN; el mundo es Y-UP

    if (o->getType() == ObjectType::texto2d) {
        Texto2D* t = (Texto2D*)o;
        w3dui::W3dTextAtlas* at = Fuente2DObtener(t->fuente);
        std::string txt = TextoSegunTipo(t);
        if (at && !txt.empty()) {
            float pxU = t->tam * K;                 // alto del texto en unidades de mundo
            float wU  = at->TextWidth(txt.c_str(), pxU);
            float wpx = wU / K;
            float ox = 0.0f, oy = 0.0f;             // pen del texto, en unidades (espacio Y-DOWN)
            if (t->alignH == 1) ox -= wU * 0.5f; else if (t->alignH == 2) ox -= wU;
            if (t->alignV == 1) oy -= pxU * 0.5f; else if (t->alignV == 2) oy -= pxU;
            bx0 = cxp + ox / K; bx1 = bx0 + wpx;
            by0 = cyp + oy / K; by1 = by0 + t->tam;
            at->Begin();
            gfx::Enable(gfx::DepthTest);           // que la profundidad se note contra la escena
            at->DrawText(txt.c_str(), ox, oy, pxU,
                         t->color[0], t->color[1], t->color[2], t->color[3] * op);
            at->End();
        }
    } else if (o->getType() == ObjectType::boton2d || o->getType() == ObjectType::expandir2d) {
        // en el modo mundo el boton/expandir no se dibujan (es inspeccion de profundidad)
    } else {
        float aw = enCelda ? rw : (e->tamPx ? e->ancho : e->ancho * rw);
        float ah = enCelda ? rh : (e->tamPx ? e->alto  : e->alto  * rh);
        float hw = aw * K * 0.5f, hh = ah * K * 0.5f;
        bx0 = cxp - aw * 0.5f; by0 = cyp - ah * 0.5f;
        bx1 = cxp + aw * 0.5f; by1 = cyp + ah * 0.5f;
        if (o->getType() == ObjectType::imagen2d)
            DibujarImagenRect((Imagen2D*)o, -hw, -hh, hw, hh, true, op);
        else if (o->getType() == ObjectType::video2d)
            DibujarVideoRect((Video2D*)o, -hw, -hh, hw, hh, true, op);
        else if (o->getType() == ObjectType::rect2d)
            DibujarRectSolido((Rect2D*)o, -hw, -hh, hw, hh, true, op);
        else if (o->getType() == ObjectType::slice9)
            DibujarSlice9Rect((Slice9*)o, -hw, -hh, hw, hh, true, op, K);
        // el contenedor no dibuja nada
    }
    gfx::PopMatrix();

    float pi, pd, pa, pb;
    PaddingLados(o, 1.0f, bx1 - bx0, by1 - by0, &pi, &pd, &pa, &pb);
    DibujarHijosMundo(o, bx0 + pi, by0 + pa,
                      (bx1 - bx0) - pi - pd, (by1 - by0) - pa - pb,
                      ventW, ventH, op);
}

void UI2D_DibujarEnMundo() {
    if (!SceneCollection) return;
    for (size_t i = 0; i < SceneCollection->Childrens.size(); i++) {
        Object* o = SceneCollection->Childrens[i];
        if (!o || o->getType() != ObjectType::ui || !o->visible || !((UI*)o)->verEn3D) continue;
        UI* u = (UI*)o;
        gUIRaiz = u;
        float vw, vh;
        if (!u->igualQueRender) { vw = u->ancho; vh = u->alto; }
        else UI2D_TamanoVentana(&vw, &vh);
        float pi, pd, pa, pb;
        PaddingLados(o, 1.0f, vw, vh, &pi, &pd, &pa, &pb);
        DibujarHijosMundo(o, pi, pa, vw - pi - pd, vh - pa - pb,
                          vw, vh, u->opacidad);
    }
}

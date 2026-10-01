// ============================================================================
//  Hitbox.cpp — ver Hitbox.h. Compartido editor/juego (C++03: Symbian lo compila).
// ============================================================================
#include "w3dGraphics.h"            // abstraccion de graficos (independencia de OpenGL)
#include "objects/Hitbox.h"
#include "objects/Mesh.h"           // HitboxCrearAjustado: los vertices del subarbol
#include "objects/RenderColors.h"   // el color de seleccion (el mismo que el resto de los objetos)
#include "io/JsonW3d.h"             // JVal / JsonNumTexto: los campos del .w3d
#include <math.h>

namespace gfx = w3dEngine;

Hitbox::Hitbox(Object* parent, Vector3 pos) : W3dHitboxBase(parent, "Hitbox", pos) {}

bool g_showHitbox = true;

bool HitboxSeDibuja(void) {
    // el JUEGO no la dibuja (no hay overlays), salvo el overlay de debug; el editor si, tambien
    // durante el Play (ahi se pone amarilla cuando toca algo), salvo que se apaguen los overlays
    // o el tipo (el Render Image tampoco: corre sin overlays)
    return (w3dRenderOverlays && g_showHitbox) || g_w3dHitboxVer;
}

void HitboxAristas(const W3dHitboxBase* h, float out[72]) {
    const float hx = 0.5f * fabsf(h->tam[0]), hy = 0.5f * fabsf(h->tam[1]), hz = 0.5f * fabsf(h->tam[2]);
    const float cx = h->centro[0], cy = h->centro[1], cz = h->centro[2];
    // las 8 esquinas: bit 0 = x, bit 1 = y, bit 2 = z (0 = menos, 1 = mas)
    float v[8][3];
    for (int i = 0; i < 8; i++) {
        v[i][0] = cx + ((i & 1) ? hx : -hx);
        v[i][1] = cy + ((i & 2) ? hy : -hy);
        v[i][2] = cz + ((i & 4) ? hz : -hz);
    }
    // una arista une dos esquinas que difieren en UN bit
    static const int kAristas[12][2] = {
        {0,1},{2,3},{4,5},{6,7},   // a lo largo de X
        {0,2},{1,3},{4,6},{5,7},   // a lo largo de Y
        {0,4},{1,5},{2,6},{3,7}    // a lo largo de Z
    };
    int k = 0;
    for (int a = 0; a < 12; a++)
        for (int e = 0; e < 2; e++) {
            const float* p = v[kAristas[a][e]];
            out[k++] = p[0]; out[k++] = p[1]; out[k++] = p[2];
        }
}

void Hitbox::RenderObject() {
    if (!HitboxSeDibuja()) return;
    float c[4];
    if (W3dHitboxTocaAlgo(this))  { c[0] = 1.00f; c[1] = 0.85f; c[2] = 0.10f; }   // tocando: amarillo
    else if (activo)              { c[0] = 0.25f; c[1] = 0.90f; c[2] = 0.35f; }   // activo: verde
    else                          { c[0] = 0.50f; c[1] = 0.50f; c[2] = 0.50f; }   // inactivo: gris
    c[3] = 1.0f;
    if (select) {
        // seleccionado: el color del estado se aclara hacia el de seleccion (se sigue leyendo el
        // estado y a la vez se ve cual esta elegido, igual que los demas objetos del editor)
        const float* s = gRenderColors[(this == ObjActivo) ? RC_selActive : RC_selInactive];
        for (int k = 0; k < 3; k++) c[k] = 0.5f * (c[k] + s[k]) + 0.25f;
        for (int k = 0; k < 3; k++) if (c[k] > 1.0f) c[k] = 1.0f;
    }
    // ESTATICO a proposito: el puntero de coordenadas de textura queda apuntando aca despues del return (el
    // dummy de abajo) y el que dibuja despues sin fijar el suyo (un vacio, la cruz de una instancia) hace que el
    // driver lo lea: en la pila eso era leer memoria de un frame que ya no existe (ASan: stack-use-after-return)
    static float lineas[72];
    HitboxAristas(this, lineas);
    const bool luzEstaba = gfx::IsEnabled(gfx::Lighting);   // restaurar al salir
    const bool texEstaba = gfx::IsEnabled(gfx::Texture2D);
    gfx::Disable(gfx::Lighting);
    gfx::Disable(gfx::Texture2D);
    gfx::DisableArray(gfx::NormalArray);
    gfx::DisableArray(gfx::ColorArray);
    gfx::Color4f(c[0], c[1], c[2], c[3]);
    gfx::LineWidth(select ? 2.0f : 1.0f);
    gfx::VertexPointer3f(0, lineas);
    gfx::TexCoordPointer2f(12, lineas);   // dummy valido (el driver del N95 lo lee aunque no haya textura)
    gfx::DrawLines(24);
    gfx::LineWidth(1.0f);
    gfx::EnableArray(gfx::NormalArray);
    if (texEstaba) gfx::Enable(gfx::Texture2D);
    if (luzEstaba) gfx::Enable(gfx::Lighting);
}

Hitbox* HitboxDuplicar(Hitbox* src) {
    if (!src) return NULL;
    Hitbox* d = new Hitbox(src->Parent, src->pos);
    d->activo = src->activo;
    for (int k = 0; k < 3; k++) { d->tam[k] = src->tam[k]; d->centro[k] = src->centro[k]; }
    d->etiqueta = src->etiqueta;
    d->filtro = src->filtro;
    d->detectarCuerpos = src->detectarCuerpos;
    return d;
}

bool HitboxPadreValido(Object* o) {
    if (!o || o == SceneCollection) return false;
    const ObjectType t = o->getType();
    // las colecciones solo ordenan el arbol; lo 2D vive en una interfaz; un hitbox hijo de otro no
    // tiene sentido (serian el mismo dueno)
    if (t == ObjectType::collection || t == ObjectType::scene || t == ObjectType::hitbox) return false;
    if (t == ObjectType::ui || t == ObjectType::texto2d || t == ObjectType::imagen2d ||
        t == ObjectType::rect2d || t == ObjectType::cont2d || t == ObjectType::slice9 ||
        t == ObjectType::boton2d || t == ObjectType::expandir2d || t == ObjectType::video2d) return false;
    return true;
}

// la caja de las mallas del subarbol de 'o', en el espacio LOCAL de la raiz (la cuenta de la
// definicion de fisica de Add > Physics)
static void HbBoundsRec(Object* o, const Matrix4& aRaiz, bool esRaiz, float mn[3], float mx[3], bool* hay) {
    Matrix4 m = aRaiz;
    if (!esRaiz) {
        Matrix4 loc;
        o->GetMatrixBase(loc);
        m = aRaiz * loc;
    }
    if (o->getType() == ObjectType::mesh) {
        Mesh* me = (Mesh*)o;
        if (me->vertex && me->vertexSize > 0)
            for (int i = 0; i < me->vertexSize; i++) {
                const Vector3 p = m * Vector3(me->vertex[i * 3], me->vertex[i * 3 + 1], me->vertex[i * 3 + 2]);
                for (int k = 0; k < 3; k++) {
                    if (!*hay || p[k] < mn[k]) mn[k] = p[k];
                    if (!*hay || p[k] > mx[k]) mx[k] = p[k];
                }
                *hay = true;
            }
    }
    for (size_t i = 0; i < o->Childrens.size(); i++)
        if (o->Childrens[i] && o->Childrens[i]->getType() != ObjectType::hitbox)
            HbBoundsRec(o->Childrens[i], m, false, mn, mx, hay);
}

Hitbox* HitboxCrearAjustado(Object* padre) {
    if (!padre) return NULL;
    Hitbox* h = new Hitbox(padre, Vector3(0, 0, 0));
    float mn[3] = { 0, 0, 0 }, mx[3] = { 0, 0, 0 };
    bool hay = false;
    Matrix4 ident; ident.Identity();
    HbBoundsRec(padre, ident, true, mn, mx, &hay);
    if (hay)
        for (int k = 0; k < 3; k++) {
            h->tam[k] = mx[k] - mn[k];
            if (h->tam[k] < 0.02f) h->tam[k] = 0.02f;   // una malla plana igual se puede tocar
            h->centro[k] = 0.5f * (mx[k] + mn[k]);
        }
    return h;
}

// ---- .w3d ------------------------------------------------------------------
static void HbSang(std::string& s, int n) { for (int i = 0; i < n; i++) s += "  "; }
static void HbEsc(std::string& s, const std::string& v) {   // mismo escape que el escritor del .w3d
    s += '"';
    for (size_t i = 0; i < v.size(); i++) {
        const char c = v[i];
        if (c == '"' || c == '\\') { s += '\\'; s += c; }
        else if (c == '\n') s += "\\n";
        else s += c;
    }
    s += '"';
}
static void HbVec(std::string& s, const float* v) {
    s += "["; s += JsonNumTexto(v[0]); s += ", "; s += JsonNumTexto(v[1]); s += ", "; s += JsonNumTexto(v[2]); s += "]";
}

void HitboxEscribirCampos(std::string& s, int ind, const W3dHitboxBase* h) {
    s += ",\n"; HbSang(s, ind); s += "\"activo\": "; s += h->activo ? "true" : "false";
    s += ",\n"; HbSang(s, ind); s += "\"tam\": "; HbVec(s, h->tam);
    s += ",\n"; HbSang(s, ind); s += "\"centro\": "; HbVec(s, h->centro);
    s += ",\n"; HbSang(s, ind); s += "\"etiqueta\": "; HbEsc(s, h->etiqueta);
    s += ",\n"; HbSang(s, ind); s += "\"filtro\": "; HbEsc(s, h->filtro);
    s += ",\n"; HbSang(s, ind); s += "\"detectarCuerpos\": "; s += h->detectarCuerpos ? "true" : "false";
}

void HitboxLeerCampos(JVal* j, W3dHitboxBase* h) {
    if (!j || !h) return;
    h->activo = JB(j, "activo", true);
    JVal* t = JHijo(j, "tam", 5);
    if (t) for (size_t i = 0; i < 3 && i < JFilaLen(t); i++) h->tam[i] = fabsf(JFilaNum(t, i, 1.0f));
    JVal* c = JHijo(j, "centro", 5);
    if (c) for (size_t i = 0; i < 3 && i < JFilaLen(c); i++) h->centro[i] = JFilaNum(c, i, 0.0f);
    h->etiqueta = JS(j, "etiqueta", "");
    h->filtro = JS(j, "filtro", "");
    h->detectarCuerpos = JB(j, "detectarCuerpos", false);
}

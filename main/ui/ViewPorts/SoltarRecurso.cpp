// ============================================================================
//  SoltarRecurso.cpp — SOLTAR un recurso de la BIBLIOTECA (el outliner) en el
//  VIEWPORT 3D: lo que hay bajo el puntero lo decide un RAYO desde ese pixel
//  (con la misma lente con que se dibuja el viewport, Viewport3D::LenteActual)
//  contra los triangulos de las mallas visibles de la escena:
//    - malla 3D  -> un OBJETO malla nuevo con ese recurso, en el punto del rayo (sobre
//                   la superficie tocada, o en el plano del cursor 3D si no toca nada);
//    - material  -> a la PARTE de la malla tocada;
//    - shader    -> al material de la PARTE tocada (con el material por defecto: uno nuevo con ese shader);
//    - textura   -> al material de la parte tocada (el por defecto no se edita);
//    - animset   -> al armature de la malla tocada (o el que se toca);
//    - prefab    -> una INSTANCIA en el punto del rayo (io/PrefabsEditor.h).
//  Lo que viene de una LIBRERIA externa (io/Librerias.h) llega con su id GLOBAL: un prefab
//  o una escena suya ("lib:<libreria>/prefab|escena/<elemento>") = un PROXY en el punto del
//  rayo; su malla, material, textura o animset = una REFERENCIA al recurso de la libreria
//  (con su prefijo). Lo que es de una libreria no recibe nada (es de solo lectura).
//  Todo con Ctrl+Z. 'soloProbar' = no hace nada, solo dice si se puede y que haria
//  (el resaltado mientras se arrastra).
//  Solo editor. C++03. Motor generico.
// ============================================================================
#include "objects/Sonido.h"   // soltar un sonido en el 3D: un objeto Sonido
#include "ViewPort3D.h"
#include "io/RecursosProyecto.h"
#include "io/MallasProyecto.h"
#include "importers/import_w3d.h"      // la malla de trabajo fuera de la escena (probar que el recurso carga)
#include "objects/MallaRecurso.h"
#include "objects/Mesh.h"
#include "objects/Materials.h"
#include "objects/Textures.h"
#include "objects/Armature.h"
#include "objects/ObjectMode.h"
#include "animation/W3dAnimSet.h"
#include "io/PrefabsEditor.h"          // soltar un prefab = una instancia nueva
#include "objects/InstanciaPrefab.h"
#include "objects/ProxyW3d.h"          // soltar un prefab o una escena de una libreria = un proxy
#include "io/Librerias.h"
#include "undo/Undo.h"
#include "config/W3dLang.h"
#include "render/OpcionesRender.h"
#include "variables.h"
#include "base/W3dInteractionState.h"
#include "w3dlog.h"
#include <cmath>
#include <cfloat>
#include "io/Prefabs.h"   // W3dPrefabClaveEscena: una escena del proyecto se instancia
#include "objects/ShaderEscrito.h"   // soltar un shader: el material de la parte lo usa

// ---- EL RAYO ----
namespace {
struct Rayo { Vector3 o, d; };
struct Toque { Mesh* m; int parte; float t; Vector3 p; Toque() : m(NULL), parte(-1), t(FLT_MAX) {} };
}
static Rayo RayoDesdePixel(Viewport3D* vp, int mx, int my) {
    const Viewport3D::Lente L = vp->LenteActual();
    float nx = ((float)(mx - vp->x) + 0.5f) / (float)vp->width * 2.0f - 1.0f;
    float ny = 1.0f - ((float)(my - vp->y) + 0.5f) / (float)vp->height * 2.0f;
    nx = (nx - L.panX) / (L.zoom != 0.0f ? L.zoom : 1.0f);
    ny = (ny - L.panY) / (L.zoom != 0.0f ? L.zoom : 1.0f);
    Rayo r;
    if (L.orto) { r.o = L.pos + L.der * (nx * L.sx) + L.arr * (ny * L.sy); r.d = L.fwd; }
    else { r.o = L.pos; r.d = (L.fwd + L.der * (nx * L.sx) + L.arr * (ny * L.sy)).Normalized(); }
    return r;
}
// Moller-Trumbore
static bool RayoTri(const Rayo& r, const Vector3& a, const Vector3& b, const Vector3& c, float& t) {
    const Vector3 e1 = b - a, e2 = c - a;
    const Vector3 p = Vector3::Cross(r.d, e2);
    const float det = e1.Dot(p);
    if (fabsf(det) < 1e-9f) return false;
    const float inv = 1.0f / det;
    const Vector3 s = r.o - a;
    const float u = s.Dot(p) * inv;
    if (u < 0.0f || u > 1.0f) return false;
    const Vector3 q = Vector3::Cross(s, e1);
    const float v = r.d.Dot(q) * inv;
    if (v < 0.0f || u + v > 1.0f) return false;
    t = e2.Dot(q) * inv;
    return t > 1e-5f;
}
static void TocarArbol(Object* o, const Rayo& r, Toque& best) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h || !h->visible) continue;
        if (h->getType() == ObjectType::mesh) {
            Mesh* m = (Mesh*)h;
            if (m->vertex && m->faces && m->facesSize >= 3) {
                Matrix4 W; m->GetWorldMatrix(W);
                for (int k = 0; k + 2 < m->facesSize; k += 3) {
                    const int ia = (int)m->faces[k], ib = (int)m->faces[k + 1], ic = (int)m->faces[k + 2];
                    if (ia >= m->vertexSize || ib >= m->vertexSize || ic >= m->vertexSize) continue;
                    const Vector3 a = W * Vector3(m->vertex[ia * 3], m->vertex[ia * 3 + 1], m->vertex[ia * 3 + 2]);
                    const Vector3 b = W * Vector3(m->vertex[ib * 3], m->vertex[ib * 3 + 1], m->vertex[ib * 3 + 2]);
                    const Vector3 c = W * Vector3(m->vertex[ic * 3], m->vertex[ic * 3 + 1], m->vertex[ic * 3 + 2]);
                    float t;
                    if (!RayoTri(r, a, b, c, t) || t >= best.t) continue;
                    best.t = t; best.m = m; best.p = r.o + r.d * t;
                    best.parte = -1;
                    for (size_t g = 0; g < m->materialsGroup.size(); g++) {
                        const MaterialGroup& mg = m->materialsGroup[g];
                        if (k >= mg.startDrawn && k < mg.startDrawn + mg.indicesDrawnCount) { best.parte = (int)g; break; }
                    }
                    if (best.parte < 0 && !m->materialsGroup.empty()) best.parte = 0;
                }
            }
        }
        TocarArbol(h, r, best);
    }
}
static Toque Tocar(Viewport3D* vp, int mx, int my, Rayo* rayo) {
    Rayo r = RayoDesdePixel(vp, mx, my);
    if (rayo) *rayo = r;
    Toque t;
    TocarArbol(SceneCollection, r, t);
    return t;
}
// el punto donde va un objeto nuevo: lo tocado, o el rayo contra el plano HORIZONTAL del cursor 3D, o el cursor
static Vector3 PuntoDeSoltar(const Toque& t, const Rayo& r) {
    if (t.m) return t.p;
    const float y0 = cursor3D.pos.y;
    if (fabsf(r.d.y) > 1e-5f) {
        const float k = (y0 - r.o.y) / r.d.y;
        if (k > 0.0f && k < 10000.0f) return r.o + r.d * k;
    }
    return cursor3D.pos;
}
// el armature de una malla (su padre, o el de mas arriba) o el objeto mismo si es uno
static Armature* ArmatureDe(Object* o) {
    for (Object* p = o; p && p != SceneCollection; p = p->Parent)
        if (p->getType() == ObjectType::armature) return (Armature*)p;
    return NULL;
}

// lo tocado es de una LIBRERIA (lo genera un proxy, o usa una malla de una libreria): de solo lectura
static bool DeLibreria(Object* o) {
    if (!o) return false;
    if (o->getType() == ObjectType::mesh && ((Mesh*)o)->malla && !((Mesh*)o)->malla->libreria.empty()) return true;
    for (Object* p = W3dInstanciaDe(o); p; p = W3dInstanciaDe(p)) if (p->getType() == ObjectType::proxy) return true;
    return false;
}

// ---- LOS PASOS DE UNDO que no tenia nadie ----
// la TEXTURA base de un material (ida y vuelta). El paso es DUENO de una referencia a 'otra' (la ranura del material
// tiene la suya: Textures.h, TexturaPonerEn): ir y volver intercambia las dos sin tocar la cuenta, y la que el paso
// tenia guardada se suelta cuando se cae del historial
struct UndoTexMat { Material* m; Texture* otra; bool onOtro; int mapa; };   // mapa = W3dMatMapa (la ranura)
// los pasos VIVOS (en el historial): una textura PURGADA que revive le devuelve su referencia a cada paso que la
// guarda (W3dUndoTexturaDuenos; la purga las habia cerrado todas, y el paso la suelta igual al caerse)
static std::vector<UndoTexMat*> gUndoTexMats;
int W3dUndoTexturaDuenos(const Texture* t) {
    int n = 0;
    for (size_t i = 0; i < gUndoTexMats.size(); i++) if (t && gUndoTexMats[i]->otra == t) n++;
    return n;
}
static void UndoTexMatAplicar(void* d) {
    UndoTexMat* u = (UndoTexMat*)d;
    bool vivo = false;
    for (size_t i = 0; i < Materials.size() && !vivo; i++) vivo = (Materials[i] == u->m);
    if (!vivo) return;
    Texture** pr = u->m->RanuraDeMapa(u->mapa);
    if (!pr) return;
    Texture*& ranura = *pr;
    bool& prendida = u->mapa == W3D_MAPA_NORMAL ? u->m->normalMap : u->mapa == W3D_MAPA_BASE ? u->m->textureOn : u->m->mapaConTextura[u->mapa];
    Texture* t = ranura; const bool on = prendida;
    ranura = u->otra; prendida = u->onOtro;
    u->otra = t; u->onOtro = on;
    W3dRecursosRevisarPurgados();
    g_redraw = true;
}
static void UndoTexMatLiberar(void* d) {
    UndoTexMat* u = (UndoTexMat*)d;
    for (size_t i = 0; i < gUndoTexMats.size(); i++) if (gUndoTexMats[i] == u) { gUndoTexMats.erase(gUndoTexMats.begin() + (long)i); break; }
    TexturaSoltar(u->otra);
    delete u;
}
// la textura BASE de un material (prendida), con su paso de undo: la usan soltar en el 3D y en Properties
static void PonerEnRanura(Material* mat, Texture* tex, int mapa) {
    if (!mat || !tex || !mat->RanuraDeMapa(mapa)) return;
    Texture*& ranura = *mat->RanuraDeMapa(mapa);
    bool& prendida = mapa == W3D_MAPA_NORMAL ? mat->normalMap : mapa == W3D_MAPA_BASE ? mat->textureOn : mat->mapaConTextura[mapa];
    UndoTexMat* u = new UndoTexMat();
    u->m = mat; u->otra = ranura; u->onOtro = prendida; u->mapa = mapa;   // (la referencia de la ranura pasa al paso)
    TexturaRetener(tex);                                                      // (y la ranura toma una de la nueva)
    ranura = tex; prendida = true;
    UndoExterno f; f.aplicar = UndoTexMatAplicar; f.liberar = UndoTexMatLiberar;
    gUndoTexMats.push_back(u);
    UndoPushExterno(f, u);
    g_redraw = true;
}
void W3dMaterialPonerTextura(Material* mat, Texture* tex) { PonerEnRanura(mat, tex, W3D_MAPA_BASE); }
// el NORMAL MAP de un material (prendido), con su paso de undo: soltar una textura en su selector de Properties
void W3dMaterialPonerNormal(Material* mat, Texture* tex) { PonerEnRanura(mat, tex, W3D_MAPA_NORMAL); }
// cualquier MAPA (W3dMatMapa): la textura queda en su ranura y el mapa pasa a usarla, con su paso de undo
void W3dMaterialPonerMapa(Material* mat, Texture* tex, int mapa) { PonerEnRanura(mat, tex, mapa); }
// el ANIMSET de un armature (ida y vuelta: el de antes, o ninguno)
struct UndoAnimArm { Armature* a; std::string otro; };
static void PonerAnimSet(Armature* a, const std::string& nombre) {
    while (!a->animations.empty()) W3dArmatureAnimsQuitar(a, (int)a->animations.size() - 1);
    if (!nombre.empty()) { std::string mot; W3dArmatureAsignarAnimSet(a, nombre, NULL, &mot); }
}
static std::string AnimSetDe(Armature* a) {
    if (a->animations.empty()) return std::string();
    const W3dAnimSet* s = W3dArmatureAnimSetCalza(a) ? W3dArmatureAnimSet(a) : NULL;
    return s ? s->nombre : a->animSetNombre;
}
static void UndoAnimArmAplicar(void* d) {
    UndoAnimArm* u = (UndoAnimArm*)d;
    if (!u->a) return;
    const std::string ahora = AnimSetDe(u->a);
    PonerAnimSet(u->a, u->otro);
    u->otro = ahora;
    g_redraw = true;
}
static void UndoAnimArmDesvincular(void* d, Object* borrado) { UndoAnimArm* u = (UndoAnimArm*)d; if ((Object*)u->a == borrado) u->a = NULL; }
static void UndoAnimArmLiberar(void* d) { delete (UndoAnimArm*)d; }

// "sonidos/musica.wav" -> "musica" (el nombre del objeto Sonido)
static std::string HojaSinExt(const std::string& r) {
    size_t a = r.find_last_of("/\\"); a = (a == std::string::npos) ? 0 : a + 1;
    size_t b = r.find_last_of('.'); if (b == std::string::npos || b < a) b = r.size();
    return r.substr(a, b - a);
}
// ---- LA VISTA PREVIA mientras se arrastra un MATERIAL o un SHADER sobre una malla: la parte bajo el puntero se dibuja
// con lo arrastrado (se ve a que parte le vas a pegar); al moverse a otra parte, salir de la malla o soltar en otro lado
// vuelve a su material. Es solo de dibujo: no hay paso de undo ni el proyecto queda cambiado ---
namespace {
struct VistaPrevia {
    Mesh* m; int parte; Material* original;   // lo que tenia la parte
    Material* puesto;                          // lo que se le puso (el material arrastrado o la copia con el shader)
    int tipo; std::string id;
    VistaPrevia() : m(NULL), parte(-1), original(NULL), puesto(NULL), tipo(-1) {}
};
}
static VistaPrevia gPrevia;
static Material* gPreviaShader = NULL;   // la copia del material de la parte con el shader arrastrado (sin registrar)
void W3dSoltarVistaPreviaFin() {
    if (!gPrevia.m) return;
    if (gPrevia.parte < (int)gPrevia.m->materialsGroup.size() &&
        gPrevia.m->materialsGroup[(size_t)gPrevia.parte].material == gPrevia.puesto)
        gPrevia.m->materialsGroup[(size_t)gPrevia.parte].material = gPrevia.original;
    gPrevia = VistaPrevia();
    g_redraw = true;
}
static std::string gPreviaShaderDe;      // de que material + shader es la copia (asi no se recompila en cada movimiento)
static void VistaPreviaPoner(Mesh* m, int parte, int tipo, const std::string& id, Material* mat) {
    Material* original = m->materialsGroup[(size_t)parte].material;
    Material* puesto = mat;
    if (tipo == W3D_VISTA_SHADERS) {
        // una copia del material de la parte (o uno nuevo) con el shader: el de verdad no se toca
        char clave[64]; snprintf(clave, sizeof(clave), "%p|", (void*)original);
        if (!gPreviaShader || gPreviaShaderDe != std::string(clave) + id) {
            if (!gPreviaShader) gPreviaShader = new Material("(preview)", true);
            else W3dShaderMaterialOlvidar(gPreviaShader);
            if (original && original != MaterialDefecto) { *gPreviaShader = *original; gPreviaShader->shaderCPU = NULL; }
            gPreviaShader->shaderOn = true; gPreviaShader->shaderArchivo = id;
            gPreviaShader->shaderId = 0; gPreviaShader->shaderEstado = 0;
            gPreviaShaderDe = std::string(clave) + id;
        }
        puesto = gPreviaShader;
    }
    if (!puesto) return;
    m->materialsGroup[(size_t)parte].material = puesto;
    gPrevia.m = m; gPrevia.parte = parte; gPrevia.original = original; gPrevia.puesto = puesto;
    gPrevia.tipo = tipo; gPrevia.id = id;
    g_redraw = true;
}

bool W3dSoltarRecursoEn3D(Viewport3D* vp, int mx, int my, int tipo, const std::string& id, bool soloProbar, std::string* que) {
    if (que) que->clear();
    // (el rayo y el soltar ven lo de VERDAD: la vista previa se saca primero; si el puntero sigue sobre una parte que
    //  la acepta, se vuelve a poner abajo)
    W3dSoltarVistaPreviaFin();

    if (!vp || !SceneCollection) return false;
    if (InteractionMode != ObjectMode || estado != editNavegacion) {
        if (que) *que = T("Only in Object Mode");
        return false;
    }
    Rayo rayo;
    const Toque t = Tocar(vp, mx, my, &rayo);
    switch (tipo) {
        case W3D_VISTA_MALLAS: {
            MallaRecurso* r = W3dMallaRecursoPorNombre(id);
            if (!r || r->borrado) return false;
            if (que) *que = T("New object with this 3D mesh");
            if (soloProbar) return true;
            // PRIMERO que el recurso cargue: lo vincula una malla de trabajo FUERA de la escena (una .w3db rota o
            // que falta no deja un objeto vacio en la escena, sin undo, ni toca la seleccion). Se suelta recien
            // con el objeto ya vinculado: el recurso no se descarga en el medio.
            Mesh* prueba = NULL;
            if (!r->Cargada()) {
                prueba = W3dMallaTemporalNueva();
                if (!W3dMallaVincular(prueba, r, false)) {
                    W3dMallaTemporalBorrar(prueba);
                    if (que) *que = T("The 3D mesh couldn't be loaded");
                    return false;
                }
            }
            // un OBJETO malla nuevo con ESE recurso (comparte la malla en memoria), en el punto del rayo
            UndoGrupoIniciar();
            Mesh* m = new Mesh(NULL, PuntoDeSoltar(t, rayo));
            // (el objeto de una malla de una LIBRERIA se llama como la malla SIN el prefijo: un nombre de objeto con
            //  '/' no se puede nombrar en una ruta de clip)
            const size_t barra = r->libreria.empty() ? std::string::npos : r->nombre.find_last_of('/');
            m->SetNameObj(barra == std::string::npos ? r->nombre : r->nombre.substr(barra + 1));
            const bool vinculo = W3dMallaVincular(m, r, false);
            if (prueba) W3dMallaTemporalBorrar(prueba);
            if (!vinculo) {
                // (no deberia: ya cargo) el objeto nuevo no queda en la escena
                std::vector<Object*>& ch = (m->Parent ? m->Parent : SceneCollection)->Childrens;
                for (size_t i = 0; i < ch.size(); i++) if (ch[i] == m) { ch.erase(ch.begin() + (long)i); break; }
                m->Parent = NULL;
                DeseleccionarTodo();
                W3dLiberarSubarbol(m);
                ObjActivo = NULL;
                UndoGrupoFin();
                if (que) *que = T("The 3D mesh couldn't be loaded");
                return false;
            }
            if (!m->modificadores.empty()) m->GenerarMallaModificada();
            DeseleccionarTodo();
            m->Seleccionar();
            UndoCapturarCreacion();   // Ctrl+Z lo saca (el recurso vuelve a como estaba)
            UndoGrupoFin();
            g_redraw = true;
            w3dLogf("[biblioteca] malla '%s' soltada en el 3D -> '%s'", id.c_str(), m->name.c_str());
            return true;
        }
        case W3D_VISTA_SONIDOS: {
            // un SONIDO soltado en la vista 3D: un objeto Sonido con ese archivo, donde cayo
            if (que) *que = T("New sound object here");
            if (soloProbar) return true;
            UndoGrupoIniciar();
            Sonido* so = new Sonido(NULL, PuntoDeSoltar(t, rayo));
            so->archivo = id;
            so->SetNameObj(HojaSinExt(id));
            DeseleccionarTodo();
            so->Seleccionar();
            UndoCapturarCreacion();
            UndoGrupoFin();
            g_redraw = true;
            return true;
        }
        case W3D_VISTA_MATERIALES: {
            Material* mat = W3dMaterialDeId(id);
            if (!mat) return false;
            if (!t.m || t.parte < 0) { if (que) *que = T("Drop it on a mesh"); return false; }
            if (DeLibreria(t.m)) { if (que) *que = T("read-only library"); return false; }
            if (que) *que = std::string(T("Material of")) + " " + t.m->name;
            if (soloProbar) { VistaPreviaPoner(t.m, t.parte, tipo, id, mat); return true; }
            UndoCapturarMaterial(t.m, t.parte);
            t.m->materialsGroup[(size_t)t.parte].material = mat;
            // EL MATERIAL ES DATO DE LA MALLA (como en Properties): si es un recurso compartido lo ven TODOS sus
            // objetos. Sin esto el otro usuario seguia con el viejo y al guardar el recurso se partia en dos
            if (t.m->malla && t.parte < (int)t.m->malla->partes.size())
                W3dMallaRecursoCambiarMaterial(t.m->malla, t.parte, mat);
            g_redraw = true;
            return true;
        }
        case W3D_VISTA_SHADERS: {
            // un SHADER: el material de la PARTE tocada lo usa (el tilde "Avanzado: shader personalizado" prendido). Con
            // el material POR DEFECTO (que no se edita) la parte recibe un material NUEVO con ese shader
            if (!t.m || t.parte < 0) { if (que) *que = T("Drop it on a mesh"); return false; }
            Material* mat = t.m->materialsGroup[(size_t)t.parte].material;
            if (DeLibreria(t.m) || (mat && !mat->libreria.empty())) { if (que) *que = T("read-only library"); return false; }
            const bool nuevoMat = (!mat || mat == MaterialDefecto);
            if (que) *que = nuevoMat ? std::string(T("New material with this shader")) : std::string(T("Shader of")) + " " + mat->name;
            if (soloProbar) { VistaPreviaPoner(t.m, t.parte, tipo, id, NULL); return true; }
            UndoCapturarMaterial(t.m, t.parte);
            if (nuevoMat) {
                extern Material* NuevoMaterialEnMeshPart(Mesh*, int);
                mat = NuevoMaterialEnMeshPart(t.m, t.parte);
                if (!mat) { if (que) *que = T("The default material can't be edited"); return false; }
            }
            mat->shaderArchivo = id; mat->shaderOn = true;
            W3dShaderMaterialCompilar(mat);
            extern void RebindMaterialMeshPart(); RebindMaterialMeshPart();
            g_redraw = true;
            w3dLogf("[biblioteca] shader '%s' soltado en '%s' (parte %d)", id.c_str(), t.m->name.c_str(), t.parte);
            return true;
        }
        case W3D_VISTA_TEXTURAS: {
            if (!t.m || t.parte < 0) { if (que) *que = T("Drop it on a mesh"); return false; }
            Material* mat = t.m->materialsGroup[(size_t)t.parte].material;
            if (DeLibreria(t.m) || (mat && !mat->libreria.empty())) { if (que) *que = T("read-only library"); return false; }
            // con el material POR DEFECTO (que no se edita): la parte recibe un material NUEVO con esa textura
            const bool nuevoMat = (!mat || mat == MaterialDefecto);
            if (que) *que = nuevoMat ? std::string(T("New material with this texture")) : std::string(T("Texture of")) + " " + mat->name;
            if (soloProbar) return true;
            if (nuevoMat) {
                extern Material* NuevoMaterialEnMeshPart(Mesh*, int);
                mat = NuevoMaterialEnMeshPart(t.m, t.parte);
                if (!mat) { if (que) *que = T("The default material can't be edited"); return false; }
            }
            Texture* tex = W3dTexturaCargadaDe(id);
            const bool tomada = (tex == NULL);   // (la cargo este gesto: su referencia es temporal)
            if (!tex) tex = TexturaTomar(id);
            if (!tex) { if (que) *que = T("The texture couldn't be loaded"); return false; }
            W3dMaterialPonerTextura(mat, tex);   // (la ranura toma la suya)
            if (tomada) TexturaSoltar(tex);
            return true;
        }
        case W3D_VISTA_ANIMACIONES: {
            // (un clip suelto es "animset/clip"; un animset de una LIBRERIA tambien lleva '/': su prefijo)
            if (id.find('/') != std::string::npos && !W3dAnimSetEsDeLibreria(id)) { if (que) *que = T("Drop the library, not a clip"); return false; }
            Armature* a = t.m ? ArmatureDe(t.m) : NULL;
            if (!a) { if (que) *que = T("Drop it on a character with an armature"); return false; }
            if (DeLibreria(a)) { if (que) *que = T("read-only library"); return false; }
            if (!a->animations.empty() && !W3dArmatureAnimsCompartidas(a)) { if (que) *que = T("The armature has its own clips"); return false; }
            if (que) *que = std::string(T("Animations of")) + " " + a->name;
            if (soloProbar) return true;
            UndoAnimArm* u = new UndoAnimArm();
            u->a = a; u->otro = AnimSetDe(a);
            std::string mot;
            if (!W3dArmatureAsignarAnimSet(a, id, NULL, &mot)) {
                PonerAnimSet(a, u->otro);
                delete u;
                if (que) *que = std::string(T("Not assigned")) + ": " + mot;
                return false;
            }
            UndoExterno f; f.aplicar = UndoAnimArmAplicar; f.desvincular = UndoAnimArmDesvincular; f.liberar = UndoAnimArmLiberar;
            UndoPushExterno(f, u);
            g_redraw = true;
            return true;
        }
        case W3D_VISTA_ESCENAS:
        case W3D_VISTA_PREFABS: {
            // un prefab o una escena de una LIBRERIA: un PROXY donde cae el rayo
            std::string lib, elem; int tipoEl = W3D_LIB_PREFAB;
            if (W3dLibsDeClave(id, &lib, &tipoEl, &elem)) {
                std::string motivo;
                if (!W3dProxySePuedeAgregar(lib, tipoEl, elem, &motivo)) { if (que) *que = T(motivo.c_str()); return false; }
                if (que) *que = T("New W3D proxy of this element");
                if (soloProbar) return true;
                if (!W3dProxyAgregar(lib, tipoEl, elem, PuntoDeSoltar(t, rayo), &motivo)) { if (que) *que = T(motivo.c_str()); return false; }
                w3dLogf("[biblioteca] '%s' de la libreria '%s' soltado en el 3D (proxy)", elem.c_str(), lib.c_str());
                return true;
            }
            // una INSTANCIA del prefab -o de la ESCENA del proyecto ("escena:<nombre>", io/Prefabs.h)- donde cae el
            // rayo (sobre la superficie tocada o el plano del cursor 3D)
            const bool esEscena = (tipo == W3D_VISTA_ESCENAS);
            const std::string clave = esEscena ? W3dPrefabClaveEscena(id) : id;
            std::string motivo;
            if (!W3dPrefabSePuedeAgregar(clave, &motivo)) { if (que) *que = T(motivo.c_str()); return false; }
            if (que) *que = T(esEscena ? "New instance of this scene" : "New instance of this prefab");
            if (soloProbar) return true;
            if (!W3dPrefabAgregar(clave, PuntoDeSoltar(t, rayo), &motivo)) { if (que) *que = T(motivo.c_str()); return false; }
            w3dLogf("[biblioteca] %s '%s' soltado en el 3D", esEscena ? "escena" : "prefab", id.c_str());
            return true;
        }
    }
    if (que) *que = T("This resource can't be dropped in the 3D view");
    return false;
}

// ============================================================================
//  UN ARCHIVO SOLTADO DESDE EL SISTEMA (Windows / Linux / Mac: arrastrarlo desde el explorador a la ventana)
//    - sobre el VIEWPORT 3D: un modelo (.obj .fbx .gltf .glb) se importa a la escena; una imagen se suma a la
//      biblioteca y va al material de la malla que queda bajo el puntero;
//    - sobre la BIBLIOTECA (o cualquier otro panel): queda en la biblioteca del proyecto, en memoria hasta guardar el
//      .w3d. Un modelo pasa a ser un PREFAB (no queda en la escena); imagenes, sonidos, scripts, fuentes y videos, su
//      recurso de siempre.
// ============================================================================
#include "io/W3dContenedor.h"          // W3dImportarAsset / W3dCategoriaPorExtension
#include "Outliner.h"                  // la biblioteca bajo el puntero: a que CARPETA va lo soltado
#include "io/lectura-escritura.h"      // ImportModeloPorExtension (la puerta unica de los modelos)
static unsigned int SerialMaxRec(Object* o) {
    if (!o) return 0;
    unsigned int m = o->serial;
    for (size_t i = 0; i < o->Childrens.size(); i++) { const unsigned int s = SerialMaxRec(o->Childrens[i]); if (s > m) m = s; }
    return m;
}
// selecciona lo de ARRIBA de lo que nacio en el import (serial > 'antes'), donde lo haya colgado (la coleccion activa)
static void SeleccionarNuevos(Object* o, unsigned int antes, Object** primero) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (h->serial > antes && h->getType() != ObjectType::collection) { h->Seleccionar(); if (!*primero) *primero = h; }
        else SeleccionarNuevos(h, antes, primero);
    }
}
static std::string ExtDe(const std::string& r) {
    const size_t p = r.find_last_of('.');
    std::string e = p == std::string::npos ? std::string() : r.substr(p + 1);
    for (size_t i = 0; i < e.size(); i++) if (e[i] >= 'A' && e[i] <= 'Z') e[i] = (char)(e[i] + 32);
    return e;
}
static std::string HojaDe(const std::string& r) {
    const size_t p = r.find_last_of("/\\");
    return p == std::string::npos ? r : r.substr(p + 1);
}
// la CARPETA de la biblioteca donde cae lo soltado: la carpeta bajo el puntero, la de la fila apuntada o, en la
// cuadricula, la que se esta viendo ("" = la raiz; tambien si no es una biblioteca)
static std::string CarpetaBajo(ViewportBase* bajo, int mx, int my) {
    if (!bajo || !bajo->isLeaf() || !VpEsOutliner(bajo)) return std::string();
    Outliner* o = (Outliner*)bajo;
    if (!o->EnBiblioteca() || o->VistaSoloLectura()) return std::string();
    std::vector<OutFilaRec> filas; o->FilasRecursos(filas);
    const int i = o->FilaEnPunto(filas, mx, my);
    if (i >= 0 && filas[(size_t)i].carpeta && !filas[(size_t)i].atras) return filas[(size_t)i].clave;
    if (i >= 0) return filas[(size_t)i].carpetaDe;
    return o->cuadricula ? o->carpetaGrid : std::string();
}
// el TIPO de la biblioteca de un archivo del proyecto (por su categoria)
static int TipoDeCategoria(const std::string& c) {
    if (c == "texturas") return W3D_VISTA_TEXTURAS;
    if (c == "sonidos")  return W3D_VISTA_SONIDOS;
    if (c == "scripts")  return W3D_VISTA_SCRIPTS;
    if (c == "fuentes")  return W3D_VISTA_FUENTES;
    if (c == "videos")   return W3D_VISTA_VIDEOS;
    if (c == "shaders")  return W3D_VISTA_SHADERS;
    return -1;
}
bool W3dSoltarArchivo(const std::string& ruta0, int mx, int my, std::string* que) {
    if (que) que->clear();
    // (Windows suelta "C:\carpeta\foto.png" con barras invertidas: la biblioteca nombra todo con '/', y Windows abre
    //  igual con '/')
    std::string ruta = ruta0;
    for (size_t i = 0; i < ruta.size(); i++) if (ruta[i] == '\\') ruta[i] = '/';
    extern ViewportBase* rootViewport;
    ViewportBase* bajo = rootViewport ? FindViewportUnderMouse(rootViewport, mx, my) : NULL;
    const bool en3D = bajo && bajo->isLeaf() && bajo->ViewportKind() == 1;
    const std::string carpeta = en3D ? std::string() : CarpetaBajo(bajo, mx, my);
    const std::string ext = ExtDe(ruta);
    const std::string cat = W3dCategoriaPorExtension(ruta);
    const bool esModelo = (ext == "obj" || ext == "fbx" || ext == "gltf" || ext == "glb");
    if (!esModelo && cat != "texturas" && cat != "sonidos" && cat != "scripts" && cat != "fuentes" && cat != "videos" && cat != "shaders") {
        if (que) *que = std::string(T("This file type can't be added")) + ": " + HojaDe(ruta);
        return false;
    }
    if (esModelo) {
        if (InteractionMode != ObjectMode || estado != editNavegacion) { if (que) *que = T("Only in Object Mode"); return false; }
        const unsigned int antes = SerialMaxRec(SceneCollection);
#ifndef W3D_SYMBIAN
        if (!ImportModeloPorExtension(ruta)) { if (que) *que = std::string(T("Could not import")) + ": " + HojaDe(ruta); return false; }
#else
        if (que) *que = std::string(T("Could not import")) + ": " + HojaDe(ruta); return false;   // (el telefono no importa modelos sueltos)
#endif
        if (en3D) { if (que) *que = std::string(T("Imported")) + ": " + HojaDe(ruta); return true; }
        // a la BIBLIOTECA: lo que nacio en este import pasa a un prefab nuevo y la instancia se va de la escena
        DeseleccionarTodo();
        Object* primero = NULL;
        SeleccionarNuevos(SceneCollection, antes, &primero);
        if (!primero) { if (que) *que = std::string(T("Imported")) + ": " + HojaDe(ruta); return true; }
        ObjActivo = primero;
        std::string nom, motivo;
        if (!W3dPrefabCrearDesdeSeleccion(&nom, &motivo)) { if (que) *que = T(motivo.c_str()); return false; }
        Eliminar(false);   // (la instancia que dejo el prefab: el modelo queda solo en la biblioteca)
        if (!carpeta.empty()) W3dVistaRecMover(W3D_VISTA_PREFABS, nom, carpeta);   // (a la carpeta donde se solto)
        if (que) *que = std::string(T("Prefab created")) + ": " + nom;
        w3dLogf("[biblioteca] '%s' soltado: prefab '%s'", ruta.c_str(), nom.c_str());
        return true;
    }
    // un ARCHIVO del proyecto: se copia adentro del .w3d (en memoria hasta guardar)
    const std::string entrada = W3dImportarAsset(ruta, cat.c_str());
    if (entrada.empty()) { if (que) *que = std::string(T("Could not import")) + ": " + HojaDe(ruta); return false; }
    if (cat == "texturas") {
        Texture* tex = TexturaTomar(entrada);   // (cargada: la biblioteca la conserva aunque nadie la use todavia)
        if (!tex) { if (que) *que = T("The texture couldn't be loaded"); return false; }
        bool puesta = false;
        if (en3D) puesta = W3dSoltarRecursoEn3D((Viewport3D*)bajo, mx, my, W3D_VISTA_TEXTURAS, entrada, false, que);
        TexturaSoltar(tex);
        W3dRecursosVistaInvalidar();
        if (!puesta && !carpeta.empty()) W3dVistaRecMover(W3D_VISTA_TEXTURAS, entrada, carpeta);
        if (!puesta && que) *que = std::string(T("Added to the library")) + ": " + HojaDe(ruta) + (que->empty() ? "" : " (" + *que + ")");
        g_redraw = true;
        return true;
    }
    W3dRecursosVistaInvalidar();
    // un SONIDO soltado en la vista 3D: ademas de entrar a la biblioteca, un objeto Sonido donde cayo
    if (cat == "sonidos" && en3D) {
        std::string q2;
        W3dSoltarRecursoEn3D((Viewport3D*)bajo, mx, my, W3D_VISTA_SONIDOS, entrada, false, &q2);
        if (que) *que = std::string(T("New sound object here")) + ": " + HojaDe(ruta);
        g_redraw = true;
        return true;
    }
    // un SHADER soltado sobre una malla: ademas de entrar a la biblioteca, el material de la parte tocada lo usa
    if (cat == "shaders" && en3D) {
        std::string q2;
        if (W3dSoltarRecursoEn3D((Viewport3D*)bajo, mx, my, W3D_VISTA_SHADERS, entrada, false, &q2)) {
            if (que) *que = q2 + ": " + HojaDe(ruta);
            g_redraw = true;
            return true;
        }
    }
    if (!carpeta.empty() && TipoDeCategoria(cat) >= 0) W3dVistaRecMover(TipoDeCategoria(cat), entrada, carpeta);
    if (que) *que = std::string(T("Added to the library")) + ": " + HojaDe(ruta);
    g_redraw = true;
    return true;
}

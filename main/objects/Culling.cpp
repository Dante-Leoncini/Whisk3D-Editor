#include "Culling.h"
#include "Camera.h"              // CameraActive (la camara del juego)
#include "objects/CameraBase.h"  // g_renderCam* : la vista bindeada + su lente
#include "objects/Mesh.h"        // Mesh::aabbMin/aabbMax (AABB local cacheado)
#include "render/OpcionesRender.h" // g_renderAspect (el aspecto con el que dibuja el juego)
#include "w3dGraphics.h"         // GetMatrix: P x MV vigentes (autocull de mallas por pase actual)
#include "w3dFilesystem.h"       // metodo Riel: leer el .w3dvis de hijos por nodo
#include "w3dlog.h"              // aviso si el dato no carga
#include "Rutina.h"              // W3dMemoria: el PVS por hijo en una memoria (memoriaVisibles)
#include "objects/Textures.h"    // TexturaTomar / TexturaSoltar: las texturas del mundo BSP
#include <stdio.h>               // snprintf (el nombre de cada memoria de 256)
#include <math.h>
#include <algorithm> // std::sort (orden adelante -> atras)

// ---------------------------------------------------------------------------
//  LA CAMARA DESDE LA QUE MIDEN EL CULLING Y EL LOD (declarada en Culling.h).
//
//  "jugando" = el selector del timeline en Juego Y reproduciendo. En PAUSA se
//  vuelve a la vista que dibuja, que es lo que uno quiere para inspeccionar
//  orbitando el viewport.
//
//  Esta funcion es la UNICA puerta y es de SOLO LECTURA: ni el LOD ni el Culling
//  escriben una variable del viewport para conseguir "la camara del juego". Es la
//  regresion que reporto el dueno ("cuando aprieto play no quiero que me
//  modifiques el paneo/zoom de la camara activa"): dar Play reseteaba la
//  inspeccion (camViewZoom/camViewPanX/Y) del viewport. Ahora Play no toca nada
//  de la vista y la medida sale de aca.
// ---------------------------------------------------------------------------
// HERENCIA de la designacion: mientras se dibuja el SUBARBOL de un Culling con
// soloCamaraActiva, todo el que mida adentro (otro Culling, un LOD anidado) mide
// desde la MISMA camara designada. Reporte del dueno: con la opcion puesta en el
// Culling padre, los LOD de adentro seguian eligiendo hijo con la camara del
// viewport que dibujaba -> "el culling de cada viewport es parcialmente
// independiente". La opcion designa una camara para el RECORTE ENTERO de ese
// subarbol, en todos los viewports por igual. Es un contador (no bool) porque
// los Culling se anidan; es estado de RENDER puro (se prende al entrar a
// RenderHijos y se apaga al salir), nunca queda colgado entre frames.
int g_soloCamaraActivaHerencia = 0;

Camera* W3dCamaraDeMedida(bool soloCamaraActiva) {
    extern bool AnimEsJuego; extern bool PlayAnimation;
    if (!CameraActive) return NULL;
    if (soloCamaraActiva || g_soloCamaraActivaHerencia > 0 ||
        (AnimEsJuego && PlayAnimation)) return CameraActive;
    return NULL;
}

// (el RAII W3dHerenciaMedida vive en Culling.h: lo comparte el LOD)

// un plano a*x+b*y+c*z+d >= 0 = "adentro" (los 6 salen de Gribb-Hartmann)
struct PlanoFrustum { float a, b, c, d; };

// extrae los 6 planos de M = proyeccion * vista (column-major, como todo Matrix4:
// fila i de M = (m[0*4+i], m[1*4+i], m[2*4+i], m[3*4+i])). No normaliza: para el
// test de signo contra el vertice positivo no hace falta.
static void FrustumDeMatriz(const Matrix4& M, PlanoFrustum* p) {
    const float* m = M.m;
    for (int i = 0; i < 3; i++) {
        // planos 2i = fila3 + fila i (left/bottom/near), 2i+1 = fila3 - fila i (right/top/far)
        p[2*i].a   = m[3]  + m[i];      p[2*i+1].a   = m[3]  - m[i];
        p[2*i].b   = m[7]  + m[4+i];    p[2*i+1].b   = m[7]  - m[4+i];
        p[2*i].c   = m[11] + m[8+i];    p[2*i+1].c   = m[11] - m[8+i];
        p[2*i].d   = m[15] + m[12+i];   p[2*i+1].d   = m[15] - m[12+i];
    }
}

// test AABB vs frustum por el VERTICE POSITIVO: si hasta la esquina mas favorable
// al plano queda del lado de afuera, el AABB entero esta afuera. Con el centro y la
// mitad de la caja la esquina favorable sale sin elegir: n . c + |a| ex + |b| ey + |c| ez
// (la misma cuenta). Una comparacion por plano en vez de cuatro: en el ARM11 cada
// comparacion de floats vacia el pipeline del VFP, y la Rutina del mundo de DBZ prueba
// 1332 cajas por cuadro
static bool AabbEnFrustum(const PlanoFrustum* p, const Vector3& mn, const Vector3& mx) {
    const float cx = (mn.x + mx.x) * 0.5f, cy = (mn.y + mx.y) * 0.5f, cz = (mn.z + mx.z) * 0.5f;
    const float ex = (mx.x - mn.x) * 0.5f, ey = (mx.y - mn.y) * 0.5f, ez = (mx.z - mn.z) * 0.5f;
    for (int i = 0; i < 6; i++) {
        const float d = p[i].a*cx + p[i].b*cy + p[i].c*cz + p[i].d
                      + fabsf(p[i].a)*ex + fabsf(p[i].b)*ey + fabsf(p[i].c)*ez;
        if (d < 0.0f) return false;
    }
    return true;
}

// AABB vs el frustum de la CAMARA DE MEDIDA (la del juego jugando, o la vista que
// esta dibujando): el MISMO criterio que un Culling frustum, exportado para que
// otros objetos se autoculleen (el Mirror corta por el rectangulo del agua: espejo
// fuera de pantalla = re-dibujo del target ahorrado). Sin vista real bindeada u
// ortografica devuelve true (no cortar: headless / editor sin lente).
// los 6 planos de la camara de medida (el MISMO criterio que W3dAabbVisible), para el que testea MUCHAS cajas
// por frame (la Rutina: el paso "Test de visibilidad"): se arman una vez y se reusan. false = no cortar
bool W3dFrustumMedidaPlanos(float planos[24], float* posCamara) {
    CameraBase cam;
    float aspect;
    Camera* camMedida = W3dCamaraDeMedida(false);
    if (camMedida) {
        if (camMedida->orthographic) return false;
        cam.pos   = camMedida->GetGlobalPosition();
        cam.rot   = camMedida->Rot();
        cam.fov   = camMedida->fov;
        cam.nearZ = camMedida->nearClip;
        cam.farZ  = camMedida->farClip;
        float adec = camMedida->AspectoDeclarado();
        aspect = (adec > 0.01f) ? adec : ((g_renderAspect > 1e-4f) ? g_renderAspect : 1.0f);
    } else {
        if (!g_vistaBindeada || g_renderCamOrto) return false;
        cam.pos   = g_renderCamPos;
        cam.rot   = g_renderCamRot;
        cam.fov   = g_renderCamFov;
        cam.nearZ = g_renderCamNear;
        cam.farZ  = g_renderCamFar;
        aspect = (g_renderCamAspect > 1e-4f) ? g_renderCamAspect : 1.0f;
    }
    FrustumDeMatriz(cam.ProjectionMatrix(aspect) * cam.ViewMatrix(), (PlanoFrustum*)planos);
    if (posCamara) { posCamara[0] = cam.pos.x; posCamara[1] = cam.pos.y; posCamara[2] = cam.pos.z; }
    return true;
}
bool W3dAabbEnPlanos(const float planos[24], const Vector3& mn, const Vector3& mx) {
    return AabbEnFrustum((const PlanoFrustum*)planos, mn, mx);
}
// la Rutina (paso "Test de visibilidad") los pide por hook: tambien la compila un juego 2D, que no trae este .cpp
struct W3dInstalarPlanosRutina {
    W3dInstalarPlanosRutina() {
        extern bool (*g_rutinaPlanosHook)(float planos[24], float* posCamara);
        extern bool (*g_rutinaAabbPlanosHook)(const float planos[24], const Vector3& mn, const Vector3& mx);
        g_rutinaPlanosHook = W3dFrustumMedidaPlanos;
        g_rutinaAabbPlanosHook = W3dAabbEnPlanos;
    }
};
static W3dInstalarPlanosRutina gInstalarPlanosRutina;

bool W3dAabbVisible(const Vector3& mn, const Vector3& mx) {
    CameraBase cam;
    float aspect;
    Camera* camMedida = W3dCamaraDeMedida(false);
    if (camMedida) {
        cam.pos   = camMedida->GetGlobalPosition();
        cam.rot   = camMedida->Rot();
        cam.fov   = camMedida->fov;
        cam.nearZ = camMedida->nearClip;
        cam.farZ  = camMedida->farClip;
        float adec = camMedida->AspectoDeclarado();
        aspect = (adec > 0.01f) ? adec
                                : ((g_renderAspect > 1e-4f) ? g_renderAspect : 1.0f);
        if (camMedida->orthographic) return true;
    } else {
        if (!g_vistaBindeada || g_renderCamOrto) return true;
        cam.pos   = g_renderCamPos;
        cam.rot   = g_renderCamRot;
        cam.fov   = g_renderCamFov;
        cam.nearZ = g_renderCamNear;
        cam.farZ  = g_renderCamFar;
        aspect = (g_renderCamAspect > 1e-4f) ? g_renderCamAspect : 1.0f;
    }
    PlanoFrustum planos[6];
    FrustumDeMatriz(cam.ProjectionMatrix(aspect) * cam.ViewMatrix(), planos);
    return AabbEnFrustum(planos, mn, mx);
}

// AUTOCULL de mallas: los planos salen de las MATRICES VIGENTES (Projection x ModelView del pase
// que esta dibujando AHORA, con el transform del objeto ya cargado) y el AABB se testea en LOCAL:
// el criterio es EXACTAMENTE lo que el draw esta por mandar, en CUALQUIER pase (viewport del
// editor con orbita u orto, vista desde camara, espejo reflejado, miniatura de la biblioteca,
// render offscreen). NO se reconstruye la camara desde g_renderCam* ni se usa la camara "de
// medida" del juego: ambas pueden NO ser la vista del pase actual (los tests de lote/mallacache/
// prefab/stats quedaban con 0 mallas dibujadas por cullear con la lente equivocada).
static bool W3dMallaVisibleVistaActual(const Vector3& mn, const Vector3& mx) {
    if (!g_vistaBindeada) return true;   // headless / sin lente real: no cortar
    float P[16], MV[16];
    w3dEngine::GetMatrix(w3dEngine::Projection, P);
    w3dEngine::GetMatrix(w3dEngine::ModelView, MV);
    Matrix4 pm, mv;
    for (int i = 0; i < 16; i++) { pm.m[i] = P[i]; mv.m[i] = MV[i]; }
    PlanoFrustum planos[6];
    FrustumDeMatriz(pm * mv, planos);
    return AabbEnFrustum(planos, mn, mx);
}
// instala en el Core el test de visibilidad (Mesh::RenderObject lo consulta antes de skinnear/
// dibujar). Va por hook porque el Core no puede linkear al editor (los juegos de ejemplo compilan
// el Core solo). Constructor estatico, mismo patron que gBindExtra.
struct W3dInstalarAutocullMallas {
    W3dInstalarAutocullMallas() {
        extern bool (*g_mallaVisibleHook)(const Vector3&, const Vector3&);
        g_mallaVisibleHook = W3dMallaVisibleVistaActual;
    }
};
static W3dInstalarAutocullMallas gInstalarAutocullMallas;

// suma al AABB de mundo (mn/mx, 'hay' dice si ya arranco) el AABB local de una
// malla llevado a mundo: las 8 esquinas por su GetWorldMatrix (la EFECTIVA: es
// lo que se va a dibujar). Sin AABB cacheado cae a la esfera radioGeom.
static void SumarMeshBounds(Mesh* m, bool& hay, Vector3& mn, Vector3& mx) {
    Matrix4 W; m->GetWorldMatrix(W);
    if (m->aabbOk) {
        for (int i = 0; i < 8; i++) {
            Vector3 esq((i & 1) ? m->aabbMax.x : m->aabbMin.x,
                        (i & 2) ? m->aabbMax.y : m->aabbMin.y,
                        (i & 4) ? m->aabbMax.z : m->aabbMin.z);
            Vector3 w = W * esq;
            if (!hay) { mn = mx = w; hay = true; continue; }
            if (w.x < mn.x) mn.x = w.x; if (w.x > mx.x) mx.x = w.x;
            if (w.y < mn.y) mn.y = w.y; if (w.y > mx.y) mx.y = w.y;
            if (w.z < mn.z) mn.z = w.z; if (w.z > mx.z) mx.z = w.z;
        }
    } else if (m->radioGeom > 0.0f) {
        // sin AABB (CalcularBordes no corrio aun): esfera conservadora en mundo
        Vector3 c = W * m->centroGeom;
        float r = m->EscalarRadioLocal(m->centroGeom, m->radioGeom);
        Vector3 a(c.x - r, c.y - r, c.z - r), b(c.x + r, c.y + r, c.z + r);
        if (!hay) { mn = a; mx = b; hay = true; return; }
        if (a.x < mn.x) mn.x = a.x; if (b.x > mx.x) mx.x = b.x;
        if (a.y < mn.y) mn.y = a.y; if (b.y > mx.y) mx.y = b.y;
        if (a.z < mn.z) mn.z = a.z; if (b.z > mx.z) mx.z = b.z;
    }
}

// AABB en MUNDO del SUBARBOL de 'o' (union de los bounds de todas sus mallas
// visibles). false = ni una malla medible: a ese hijo no se lo corta nunca.
bool W3dBoundsSubarbol(Object* o, Vector3& mn, Vector3& mx) {
    bool hay = false;
    std::vector<Object*> st; st.push_back(o);
    while (!st.empty()) {
        Object* c = st.back(); st.pop_back();
        if (!c->visible) continue; // lo oculto ni se dibuja ni se mide
        if (c->getType() == ObjectType::mesh) SumarMeshBounds((Mesh*)c, hay, mn, mx);
        for (size_t i = 0; i < c->Childrens.size(); i++) st.push_back(c->Childrens[i]);
    }
    return hay;
}

// estadistica de frame (bench): ver Culling.h
int g_cullHijosTotal = 0;
int g_cullHijosVisibles = 0;

// clave de MATERIAL del subarbol de 'o': el id de GL de la primera textura que
// aparezca en sus mesh parts (0 = sin textura). Sirve para AGRUPAR por textura
// los hijos visibles: los trozos/entidades que comparten atlas quedan contiguos
// y el BindTexture cacheado no re-bindea entre uno y otro (el bench del proyecto
// medido en un nivel real: ~197 binds para ~227 draws, o sea casi cada draw cambiaba de textura).
static unsigned MaterialKeySubarbol(Object* o) {
    std::vector<Object*> st; st.push_back(o);
    while (!st.empty()) {
        Object* c = st.back(); st.pop_back();
        if (!c->visible) continue;
        if (c->getType() == ObjectType::mesh) {
            Mesh* m = (Mesh*)c;
            for (size_t g = 0; g < m->materialsGroup.size(); g++) {
                Material* mat = m->materialsGroup[g].material;
                if (mat && mat->texture && mat->textureOn && mat->texture->iID)
                    return mat->texture->iID;
            }
        }
        for (size_t i = 0; i < c->Childrens.size(); i++) st.push_back(c->Childrens[i]);
    }
    return 0;
}

// hijo visible + su clave de material y su distancia a la camara. El orden es
// POR MATERIAL primero (menos re-binds de textura) y ADELANTE -> ATRAS adentro
// de cada grupo (el z-buffer descarta lo tapado igual que antes; entre OPACOS
// el orden entre grupos no cambia el resultado, solo el estado de GL).
struct HijoOrdenado {
    Object*  o;
    unsigned matKey;
    float    dist2;
    bool operator<(const HijoOrdenado& b) const {
        if (matKey != b.matKey) return matKey < b.matKey;
        return dist2 < b.dist2;
    }
};

// orden ATRAS -> ADELANTE (mas lejos primero) para los hijos TRANSLUCIDOS: el alpha necesita orden de
// PROFUNDIDAD, no de textura (ver Culling::ordenAlpha). Reemplaza al ordenarPorCamara de la Collection.
static bool HijoMasLejosPrimero(const HijoOrdenado& a, const HijoOrdenado& b) {
    return a.dist2 > b.dist2;
}
// adelante -> atras PURO (Culling::ordenCercania): el mas cercano primero, sin agrupar por material
static bool HijoMasCercaPrimero(const HijoOrdenado& a, const HijoOrdenado& b) {
    return a.dist2 < b.dist2;
}

// distancia^2 del punto 'p' al AABB [mn, mx] (0 si esta adentro): el punto del AABB
// mas CERCANO a 'p' es p clampeado a la caja. Sirve para el culling por DISTANCIA:
// "el AABB entero queda mas lejos que distanciaMax" == esta distancia > distanciaMax.
float W3dDist2PuntoAabb(const Vector3& p, const Vector3& mn, const Vector3& mx) {
    float dx = (p.x < mn.x) ? (mn.x - p.x) : (p.x > mx.x ? p.x - mx.x : 0.0f);
    float dy = (p.y < mn.y) ? (mn.y - p.y) : (p.y > mx.y ? p.y - mx.y : 0.0f);
    float dz = (p.z < mn.z) ? (mn.z - p.z) : (p.z > mx.z ? p.z - mx.z : 0.0f);
    return dx*dx + dy*dy + dz*dz;
}

// ---- metodo FRUSTUM (y la segunda mitad de Bsp): cull por AABB de cada hijo.
void Culling::RenderFrustum(const std::vector<char>* filtro) {
    // HERENCIA: la camara designada rige el subarbol entero (LOD/Culling
    // anidados miden desde la misma camara en TODOS los viewports). Cubre
    // tambien la rama "activo=false": apagar el recorte no cambia quien mide.
    W3dHerenciaMedida herencia(soloCamaraActiva);

    // ---- CULLING APAGADO (checkbox "Active"): se dibuja TODO, sin medir nada.
    // El orden por material/profundidad tampoco corre: apagado el objeto tiene que
    // portarse como un Empty comun para que la demo A/B muestre exactamente la
    // escena completa. El bench sigue contando los hijos como testeados Y visibles
    // (asi 'bench' muestra 0 recortados y la evidencia queda en el mismo numero).
    if (!activo) {
        g_cullHijosTotal    += (int)Childrens.size();
        g_cullHijosVisibles += (int)Childrens.size();
        for (size_t i = 0; i < Childrens.size(); i++) Childrens[i]->Render();
        return;
    }

    // ---- la camara del frustum ----
    CameraBase cam;
    float aspect;
    bool orto;
    Camera* camMedida = W3dCamaraDeMedida(soloCamaraActiva);
    if (camMedida) {
        // el cull DEL JUEGO, se mire desde donde se mire: la Camera activa de la
        // escena con SU lente y el aspecto del render (los mismos numeros con los
        // que dibuja el modo juego, ver Viewport3D::Render con ViewFromCameraActive).
        cam.pos   = camMedida->GetGlobalPosition();
        cam.rot   = camMedida->Rot();
        cam.fov   = camMedida->fov;
        cam.nearZ = camMedida->nearClip;
        cam.farZ  = camMedida->farClip;
        // el aspecto del JUEGO: el declarado por la camara (encuadre cinematografico,
        // propio o del riel) o el del render. La misma cuenta que dibuja (W3dAspectoJuego).
        {
            float adec = camMedida->AspectoDeclarado();
            aspect = (adec > 0.01f) ? adec
                                    : ((g_renderAspect > 1e-4f) ? g_renderAspect : 1.0f);
        }
        orto = camMedida->orthographic;
    } else {
        // la vista QUE ESTA DIBUJANDO (bindeada por el viewport): en el editor su
        // orbita, en el juego la camara activa -> el cull del juego sale solo.
        cam.pos   = g_renderCamPos;
        cam.rot   = g_renderCamRot;
        cam.fov   = g_renderCamFov;
        cam.nearZ = g_renderCamNear;
        cam.farZ  = g_renderCamFar;
        aspect = (g_renderCamAspect > 1e-4f) ? g_renderCamAspect : 1.0f;
        // sin vista bindeada (headless antes del primer frame) no hay que cortar
        orto = g_renderCamOrto || !g_vistaBindeada;
    }

    PlanoFrustum planos[6];
    if (!orto) FrustumDeMatriz(cam.ProjectionMatrix(aspect) * cam.ViewMatrix(), planos);

    // culling por DISTANCIA (opcional, distanciaMax > 0): complementa al frustum en los
    // niveles "pasillo", donde TODO el nivel cae adentro del cono de la camara
    // (verificado con el bench del proyecto: jugando, el frustum solo no corta casi nada
    // porque los trozos se alinean con la mirada). Corre tambien en ORTOGRAFICA (la
    // distancia no depende de la proyeccion), pero nunca sin una vista real bindeada
    // (headless antes del primer frame: no hay lente de nadie, no se corta).
    const bool hayVista = (camMedida != NULL) || g_vistaBindeada;
    const float distMax2 = (hayVista && distanciaMax > 0.0f) ? distanciaMax * distanciaMax : -1.0f;

    // ---- cache de hijos ESTATICOS (solo JUGANDO; ver Culling.h) ----
    // El flanco parado->jugando limpia el cache: lo que el usuario movio en el
    // editor entre play y play se re-mide al arrancar.
    extern bool W3dJuegoCorriendo();
    const bool usarCache = W3dJuegoCorriendo();
    if (usarCache && !frusJugaba) frusCache.clear();
    frusJugaba = usarCache;
    if (usarCache && frusCache.size() != Childrens.size())
        frusCache.assign(Childrens.size(), FrusHijo());

    // ---- que hijos se ven + a que distancia ----
    std::vector<HijoOrdenado> visibles;
    visibles.reserve(Childrens.size());
    for (size_t c = 0; c < Childrens.size(); c++) {
        Object* h = Childrens[c];
        // un hijo OCULTO no se dibuja ni se mide: si se midiera (y cacheara) oculto quedaria "sin bounds" y
        // al mostrarlo despues se cortaria por la posicion de su origen, que puede estar lejos de la geometria
        if (!h->visible) continue;
        if (filtro && c < filtro->size() && !(*filtro)[c]) continue;   // lo descarto el PVS (metodo Bsp)
        Vector3 mn, mx;
        bool hayBounds;
        unsigned matKey = 0;
        bool tieneMatKey = false;
        FrusHijo* fc = (usarCache && h->estatico) ? &frusCache[c] : NULL;
        if (fc && fc->valido) {
            hayBounds = fc->hay; mn = fc->mn; mx = fc->mx;
            matKey = fc->matKey; tieneMatKey = true;
        } else {
            hayBounds = W3dBoundsSubarbol(h, mn, mx);
            if (fc) {
                fc->hay = hayBounds;
                if (hayBounds) {
                    // DILATADO a la caja de la esfera del AABB: las frutas son
                    // billboards que giran en su lugar; asi cualquier rotacion
                    // (y el caer corto de una caja desapilada) queda adentro.
                    Vector3 ce((mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f, (mn.z + mx.z) * 0.5f);
                    float hx = mx.x - ce.x, hy = mx.y - ce.y, hz = mx.z - ce.z;
                    float r = sqrtf(hx * hx + hy * hy + hz * hz);
                    mn = Vector3(ce.x - r, ce.y - r, ce.z - r);
                    mx = Vector3(ce.x + r, ce.y + r, ce.z + r);
                    fc->mn = mn; fc->mx = mx;
                }
                fc->matKey = MaterialKeySubarbol(h);
                matKey = fc->matKey; tieneMatKey = true;
                // sin bounds (todo el subarbol oculto por ahora) no se cachea: se vuelve a medir
                fc->valido = hayBounds;
            }
        }
        // en ORTOGRAFICA no se corta (el frustum en perspectiva mentiria; ver
        // g_renderCamOrto); sin bounds tampoco (no hay que medir).
        if (!orto && hayBounds && !AabbEnFrustum(planos, mn, mx)) continue;
        // SIN AABB (emisor de particulas / empty hoja): cullear por POSICION (AABB degenerado = punto en
        // el cono). Sin esto un emisor de HUMO fuera de camara se seguia dibujando (no es malla -> no tenia
        // bounds que medir). Los emisores que siguen a Crash (polvo de pies) miden en su pos actual = Crash,
        // que esta en pantalla, asi que NO se cullean; solo los FIJOS fuera de camara (chimeneas).
        if (!orto && !hayBounds) {
            Vector3 _p = h->GetGlobalPosition();
            if (!AabbEnFrustum(planos, _p, _p)) continue;
        }
        // DISTANCIA: si hasta el punto MAS CERCANO del AABB queda mas lejos que
        // distanciaMax, el hijo entero esta fuera de rango -> no se dibuja.
        if (distMax2 > 0.0f && hayBounds && W3dDist2PuntoAabb(cam.pos, mn, mx) > distMax2) continue;
        HijoOrdenado ho;
        ho.o = h;
        ho.matKey = tieneMatKey ? matKey : MaterialKeySubarbol(h);
        Vector3 centro = hayBounds ? Vector3((mn.x+mx.x)*0.5f, (mn.y+mx.y)*0.5f, (mn.z+mx.z)*0.5f)
                                   : h->GetGlobalPosition();
        Vector3 d = centro - cam.pos;
        ho.dist2 = d.x*d.x + d.y*d.y + d.z*d.z;
        visibles.push_back(ho);
    }

    g_cullHijosTotal    += (int)Childrens.size(); // estadistica de frame (bench)
    g_cullHijosVisibles += (int)visibles.size();

    // ---- orden POR MATERIAL y, adentro de cada grupo, ADELANTE -> ATRAS: menos
    // re-binds de textura + el z-buffer sigue descartando lo tapado sin pintarlo.
    // Es un orden LOCAL del render de este frame: el arbol (Childrens) no se toca,
    // asi que outliner/undo/guardado no se enteran. ----
    if (ordenAlpha) std::sort(visibles.begin(), visibles.end(), HijoMasLejosPrimero); // translucido: atras->adelante (alpha)
    else if (ordenCercania) std::sort(visibles.begin(), visibles.end(), HijoMasCercaPrimero); // opaco: adelante->atras puro
    else            std::sort(visibles.begin(), visibles.end());                      // opaco: por material + adelante->atras (early-z)
    for (size_t i = 0; i < visibles.size(); i++) visibles[i].o->Render();
}

// ===========================================================================
//  METODO GRID: particion espacial por celdas (fusionado del viejo W3dGridCull).
//  Reparte los hijos ESTATICOS en una grilla por su AABB de mundo y descarta la
//  CELDA ENTERA cuando cae fuera del frustum o del rango de distancia; los hijos
//  DINAMICOS (Object::estatico==false) van fuera de la grilla y se miden por
//  frame por su AABB actual (como el metodo Frustum). Reusa las mismas primitivas
//  (PlanoFrustum/FrustumDeMatriz/AabbEnFrustum/W3dBoundsSubarbol/W3dDist2PuntoAabb)
//  de arriba. El orden POR CELDA (pocas, no por objeto) abarata el ordenAlpha.
// ===========================================================================
// clave de celda: 3 x 21 bits (sesgados a positivo) empaquetados en 63 bits.
static long long ClaveCelda(int cx, int cy, int cz) {
    long long ux = (long long)((cx + (1 << 20)) & 0x1FFFFF);
    long long uy = (long long)((cy + (1 << 20)) & 0x1FFFFF);
    long long uz = (long long)((cz + (1 << 20)) & 0x1FFFFF);
    return (ux << 42) | (uy << 21) | uz;
}

void Culling::RebuildGrid() {
    celdas.clear();
    dinamicos.clear();
    selloHijo.assign(Childrens.size(), 0);
    const float inv = (cellSize > 0.01f) ? (1.0f / cellSize) : 1.0f;
    for (size_t i = 0; i < Childrens.size(); i++) {
        Object* h = Childrens[i];
        if (!h->estatico) { dinamicos.push_back((int)i); continue; } // DINAMICO (Crash/enemigo): fuera de la grilla
        // OCULTO al armar la grilla: no se puede medir (se mostraria en la celda de su origen); se mide por frame
        if (!h->visible) { dinamicos.push_back((int)i); continue; }
        Vector3 mn, mx;
        if (!W3dBoundsSubarbol(h, mn, mx)) { Vector3 pn = h->GetGlobalPosition(); mn = mx = pn; }
        int x0 = (int)floorf(mn.x * inv), x1 = (int)floorf(mx.x * inv);
        int z0 = (int)floorf(mn.z * inv), z1 = (int)floorf(mx.z * inv);
        int y0 = modo3D ? (int)floorf(mn.y * inv) : 0;
        int y1 = modo3D ? (int)floorf(mx.y * inv) : 0;
        // guarda: un AABB gigante/basura no debe explotar la grilla -> cae a UNA celda (la del centro)
        long long cuenta = (long long)(x1 - x0 + 1) * (y1 - y0 + 1) * (z1 - z0 + 1);
        if (cuenta > 4096 || cuenta <= 0) {
            Vector3 ce((mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f, (mn.z + mx.z) * 0.5f);
            x0 = x1 = (int)floorf(ce.x * inv);
            z0 = z1 = (int)floorf(ce.z * inv);
            y0 = y1 = modo3D ? (int)floorf(ce.y * inv) : 0;
        }
        for (int cx = x0; cx <= x1; cx++)
        for (int cy = y0; cy <= y1; cy++)
        for (int cz = z0; cz <= z1; cz++) {
            Celda& c = celdas[ClaveCelda(cx, cy, cz)];
            c.hijos.push_back((int)i);
            if (c.hijos.size() == 1) { c.mn = mn; c.mx = mx; }
            else {
                if (mn.x < c.mn.x) c.mn.x = mn.x; if (mn.y < c.mn.y) c.mn.y = mn.y; if (mn.z < c.mn.z) c.mn.z = mn.z;
                if (mx.x > c.mx.x) c.mx.x = mx.x; if (mx.y > c.mx.y) c.mx.y = mx.y; if (mx.z > c.mx.z) c.mx.z = mx.z;
            }
        }
    }
    gridSucia = false;
}

void Culling::RenderGrid() {
    // apagado: dibuja TODO sin medir (igual que Frustum; demo A/B)
    if (!activo) {
        g_cullHijosTotal    += (int)Childrens.size();
        g_cullHijosVisibles += (int)Childrens.size();
        for (size_t i = 0; i < Childrens.size(); i++) Childrens[i]->Render();
        return;
    }
    if (gridSucia || selloHijo.size() != Childrens.size()) RebuildGrid();

    W3dHerenciaMedida herencia(soloCamaraActiva);
    Camera* camMedida = W3dCamaraDeMedida(soloCamaraActiva);
    CameraBase cam; float aspect; bool orto;
    if (camMedida) {
        cam.pos = camMedida->GetGlobalPosition(); cam.rot = camMedida->Rot();
        cam.fov = camMedida->fov; cam.nearZ = camMedida->nearClip; cam.farZ = camMedida->farClip;
        float adec = camMedida->AspectoDeclarado();
        aspect = (adec > 0.01f) ? adec : ((g_renderAspect > 1e-4f) ? g_renderAspect : 1.0f);
        orto = camMedida->orthographic;
    } else {
        cam.pos = g_renderCamPos; cam.rot = g_renderCamRot; cam.fov = g_renderCamFov;
        cam.nearZ = g_renderCamNear; cam.farZ = g_renderCamFar;
        aspect = (g_renderCamAspect > 1e-4f) ? g_renderCamAspect : 1.0f;
        orto = g_renderCamOrto || !g_vistaBindeada;
    }
    PlanoFrustum planos[6];
    if (!orto) FrustumDeMatriz(cam.ProjectionMatrix(aspect) * cam.ViewMatrix(), planos);
    const float distMax2 = (distanciaMax > 0.0f) ? (distanciaMax * distanciaMax) : -1.0f;

    sello++;
    // 1) RECOLECTAR celdas visibles (frustum + distancia) con su distancia a la camara.
    static std::vector<CeldaVis> visibles; visibles.clear();
    for (std::map<long long, Celda>::iterator it = celdas.begin(); it != celdas.end(); ++it) {
        Celda& c = it->second;
        if (!orto && !AabbEnFrustum(planos, c.mn, c.mx)) continue;
        float d2 = W3dDist2PuntoAabb(cam.pos, c.mn, c.mx);
        if (distMax2 > 0.0f && d2 > distMax2) continue;
        CeldaVis cv; cv.c = &c; cv.d2 = d2; visibles.push_back(cv);
    }
    // 2) ORDENAR por distancia (insertion sort: N chico y casi ordenado entre frames). ordenAlpha=lejos->cerca.
    for (size_t a = 1; a < visibles.size(); a++) {
        CeldaVis v = visibles[a]; size_t b = a;
        if (ordenAlpha) { while (b > 0 && visibles[b-1].d2 < v.d2) { visibles[b] = visibles[b-1]; b--; } }
        else            { while (b > 0 && visibles[b-1].d2 > v.d2) { visibles[b] = visibles[b-1]; b--; } }
        visibles[b] = v;
    }
    // 3) DIBUJAR con stamp por-frame (un hijo multi-celda se dibuja UNA vez).
    for (size_t v = 0; v < visibles.size(); v++) {
        Celda& c = *visibles[v].c;
        g_cullHijosTotal += (int)c.hijos.size();
        for (size_t k = 0; k < c.hijos.size(); k++) {
            int hi = c.hijos[k];
            if (selloHijo[hi] == sello) continue;
            selloHijo[hi] = sello;
            g_cullHijosVisibles++;
            Childrens[hi]->Render();
        }
    }
    // 4) hijos DINAMICOS (Crash, enemigos): fuera de la grilla, medidos por su AABB ACTUAL cada frame.
    for (size_t k = 0; k < dinamicos.size(); k++) {
        Object* h = Childrens[dinamicos[k]];
        Vector3 mn, mx;
        bool hay = W3dBoundsSubarbol(h, mn, mx);
        g_cullHijosTotal++;
        if (hay) {
            if (!orto && !AabbEnFrustum(planos, mn, mx)) continue;
            if (distMax2 > 0.0f && W3dDist2PuntoAabb(cam.pos, mn, mx) > distMax2) continue;
        }
        g_cullHijosVisibles++;
        h->Render();
    }
}

// ---- metodo RIEL: la celda del nodo dice QUE HIJOS dibujar y EN QUE ORDEN. Se
// materializa UNA vez por cambio de nodo en listaRender (cache): el render no
// recorre los hijos uno por uno ni evalua nada -- dibuja la lista y listo.
bool Culling::RielAplicarNodo(int nodo) {
    if (!visHijosCargado) {
        visHijosCargado = true;
        if (!visHijosArchivo.empty()) {
            extern std::string g_w3dDirProyecto;
            std::string ruta = visHijosArchivo;
            // JoinPath y no una concatenacion a mano: normaliza separadores (en Symbian
            // una ruta mezclada "E:\...\x/escenario/y.w3dvis" no abre y el Culling caia
            // a frustum EN SILENCIO -> el N95 dibujaba todo).
            // (una entrada de una LIBRERIA externa, "lib:<libreria>/...": el Culling lo genero un proxy; tal cual)
            if (!(ruta.size() > 1 && (ruta[0] == '/' || ruta[1] == ':')) && ruta.compare(0, 4, "lib:") != 0 &&
                !g_w3dDirProyecto.empty())
                ruta = w3dFileSystem::JoinPath(g_w3dDirProyecto, ruta);
            std::vector<unsigned char> bytes;
            std::string err;
            if (!w3dFileSystem::ReadFileBytes(ruta, bytes) || bytes.empty() ||
                !visHijos.CargarW3dvis(&bytes[0], bytes.size(), &err))
                w3dLogfW("[Culling] '%s': no pude cargar visHijos %s (%s) -> frustum",
                         name.c_str(), visHijosArchivo.c_str(), err.c_str());
        }
    }
    if (!visHijos.Valido()) { nodoAplicado = -1; return false; }
    if (nodo < 1 || nodo > visHijos.nCeldas) {
        for (size_t c = 0; c < Childrens.size(); c++) if (Childrens[c]) Childrens[c]->listadoRiel = 255;
        nodoAplicado = -1; listaRender.clear(); return false;
    }
    if (nodo == nodoAplicado) return false;
    // paso +-1 con lista vigente = UN delta; salto = decode desde la celda-clave
    bool ok;
    if (nodoAplicado >= 1 && !visHijosLista.empty() &&
        (nodo - 1 == nodoAplicado || nodo + 1 == nodoAplicado))
        ok = visHijos.Delta(nodoAplicado - 1, nodo - 1, visHijosLista);
    else
        ok = visHijos.Decodificar(nodo - 1, visHijosLista);
    if (!ok) { nodoAplicado = -1; listaRender.clear(); return false; }
    nodoAplicado = nodo;
    listaRender.clear();
    listaRender.reserve(visHijosLista.size());
    for (size_t c = 0; c < Childrens.size(); c++) if (Childrens[c]) Childrens[c]->listadoRiel = 0;
    for (size_t k = 0; k < visHijosLista.size(); k++)
        if (visHijosLista[k] < Childrens.size()) {
            listaRender.push_back(Childrens[visHijosLista[k]]);
            if (Childrens[visHijosLista[k]]) Childrens[visHijosLista[k]]->listadoRiel = 1;
        }
    return true;
}

static bool PlanosLocales(Culling* cu, float planos[24]);
// EL MUNDO del .w3dbsp v2: con el cluster y la fila vigentes (bspCluster / bspFila) y el frustum de la camara de
// medida. Los ids GL de las texturas se refrescan por cuadro (se cargan despues de tomarlas)
void Culling::MundoDibujar(int fase) {
    if (!mundo.Valido()) return;
    for (size_t i = 0; i < mundo.materiales.size() && i * 2 + 1 < mundoTex.size(); i++) {
        mundo.materiales[i].idTex = mundoTex[i * 2]     ? (unsigned)mundoTex[i * 2]->iID     : 0u;
        mundo.materiales[i].idLm  = mundoTex[i * 2 + 1] ? (unsigned)mundoTex[i * 2 + 1]->iID : 0u;
    }
    // apagado (checkbox Active): el mundo entero, sin PVS ni frustum (demo A/B, como con los hijos)
    float pl[24];
    const bool conFrustum = activo && PlanosLocales(this, pl);
    mundo.Dibujar(bsp, activo ? bspCluster : -1, bspFila, conFrustum ? pl : NULL, fase);
}

// ---- metodo BSP: la camara baja por el arbol hasta su hoja -> cluster -> fila del PVS; cada hijo
// pasa si alguno de sus clusters esta en la fila (AND de bitsets, cacheado mientras el cluster no
// cambie); lo que pasa sigue por el frustum de siempre. Todo dato: cero geometria por frame.
bool Culling::BspCargar() {
    if (!bspCargado) {
        bspCargado = true;
        if (!bspArchivo.empty()) {
            extern std::string g_w3dDirProyecto;
            // primero como ENTRADA del contenedor montado ("bsp/nivel.w3dbsp": ReadFileBytes la sirve del zip);
            // si no esta, como ruta relativa al directorio del proyecto (proyecto suelto en disco)
            std::vector<unsigned char> bytes;
            std::string err;
            std::string ruta = bspArchivo;
            if (!w3dFileSystem::ReadFileBytes(ruta, bytes) || bytes.empty()) {
                if (!(ruta.size() > 1 && (ruta[0] == '/' || ruta[1] == ':')) && ruta.compare(0, 4, "lib:") != 0 &&
                    !g_w3dDirProyecto.empty())
                    ruta = w3dFileSystem::JoinPath(g_w3dDirProyecto, ruta);
                bytes.clear();
                w3dFileSystem::ReadFileBytes(ruta, bytes);
            }
            if (bytes.empty() || !bsp.Cargar(&bytes[0], bytes.size(), &err))
                w3dLogfW("[Culling] '%s': no pude cargar el bsp %s (%s) -> frustum",
                         name.c_str(), bspArchivo.c_str(), err.c_str());
            else {
                w3dLogf("[Culling] '%s': bsp %s (v%u): %d nodos, %d hojas, %d clusters, %d hijos (motor %s)", name.c_str(),
                        bspArchivo.c_str(), bsp.version, (int)bsp.nodos.size(), (int)bsp.hojaCluster.size(), bsp.nClusters,
                        (int)bsp.hijos.size(), __DATE__ " " __TIME__);
                // version 2: EL MUNDO. Sus texturas se toman del cache por ruta (una vez; el destructor las suelta)
                MundoSoltar();
                if (bsp.geomOff) {
                    if (!mundo.Cargar(&bytes[0], bytes.size(), bsp.geomOff, bsp, &err))
                        w3dLogfW("[Culling] '%s': el mundo del bsp no carga (%s): solo PVS", name.c_str(), err.c_str());
                    else {
                        mundoTex.resize(mundo.materiales.size() * 2, (Texture*)NULL);
                        for (size_t i = 0; i < mundo.materiales.size(); i++) {
                            const MundoBsp::Material& m = mundo.materiales[i];
                            if (!m.textura.empty())  mundoTex[i * 2]     = TexturaTomar(m.textura);
                            if (!m.lightmap.empty()) mundoTex[i * 2 + 1] = TexturaTomar(m.lightmap);
                        }
                        w3dLogf("[Culling] '%s': mundo: %d materiales, %d grupos, %d caras, %d KB", name.c_str(),
                                (int)mundo.materiales.size(), (int)mundo.grupos.size(), (int)mundo.caras.size(),
                                (int)(mundo.Bytes() / 1024));
                    }
                }
            }
        }
    }
    return bsp.Valido();
}

void Culling::MundoSoltar() {
    for (size_t i = 0; i < mundoTex.size(); i++) if (mundoTex[i]) TexturaSoltar(mundoTex[i]);
    mundoTex.clear();
    mundo.Limpiar();
}

// los 6 planos del frustum de la camara de medida (como RenderFrustum/RenderGrid) en el espacio LOCAL de este objeto
// (el mundo del bsp vive ahi). false = ortografica / sin vista: no se corta por frustum
static bool PlanosLocales(Culling* cu, float planos[24]) {
    CameraBase cam; float aspect; bool orto;
    Camera* camMedida = W3dCamaraDeMedida(cu->soloCamaraActiva);
    if (camMedida) {
        cam.pos = camMedida->GetGlobalPosition(); cam.rot = camMedida->Rot();
        cam.fov = camMedida->fov; cam.nearZ = camMedida->nearClip; cam.farZ = camMedida->farClip;
        float adec = camMedida->AspectoDeclarado();
        aspect = (adec > 0.01f) ? adec : ((g_renderAspect > 1e-4f) ? g_renderAspect : 1.0f);
        orto = camMedida->orthographic;
    } else {
        cam.pos = g_renderCamPos; cam.rot = g_renderCamRot; cam.fov = g_renderCamFov;
        cam.nearZ = g_renderCamNear; cam.farZ = g_renderCamFar;
        aspect = (g_renderCamAspect > 1e-4f) ? g_renderCamAspect : 1.0f;
        orto = g_renderCamOrto || !g_vistaBindeada;
    }
    if (orto) return false;
    PlanoFrustum pl[6];
    FrustumDeMatriz(cam.ProjectionMatrix(aspect) * cam.ViewMatrix(), pl);
    // al espacio local: p' = p * W (un plano es un covector: p . (W x) = (p W) . x)
    Matrix4 W; cu->GetWorldMatrix(W);
    for (int i = 0; i < 6; i++) {
        const float p[4] = { pl[i].a, pl[i].b, pl[i].c, pl[i].d };
        for (int j = 0; j < 4; j++)
            planos[i * 4 + j] = p[0] * W.m[j * 4] + p[1] * W.m[j * 4 + 1] + p[2] * W.m[j * 4 + 2] + p[3] * W.m[j * 4 + 3];
    }
    return true;
}

// el PVS por hijo a las memorias (memoriaVisibles): <nombre>0[0..255], <nombre>1[0..255]... en el orden de los hijos
void Culling::BspEscribirMemoria() {
    if (memoriaVisibles.empty()) return;
    const size_t n = Childrens.size();
    for (size_t i = 0; i < n; i += W3D_MEMORIA_TAM) {
        char nom[96];
        snprintf(nom, sizeof nom, "%s%d", memoriaVisibles.c_str(), (int)(i / W3D_MEMORIA_TAM));
        float* m = W3dMemoria(nom);
        for (size_t k = 0; k < (size_t)W3D_MEMORIA_TAM && i + k < n; k++)
            m[k] = (bspCluster >= 0 && i + k < bspVisible.size()) ? (bspVisible[i + k] ? 1.0f : 0.0f) : 1.0f;
    }
}

void Culling::RenderBsp() {
    if (!activo) { BspCargar(); bspCluster = -1; BspEscribirMemoria(); MundoDibujar(1); RenderFrustum(); MundoDibujar(2); return; }
    if (!BspCargar()) { bspCluster = -1; BspEscribirMemoria(); RenderFrustum(); return; }
    // el ojo: la misma camara con la que mide el frustum (juego jugando / vista que dibuja)
    Vector3 ojo;
    Camera* camMedida = W3dCamaraDeMedida(soloCamaraActiva);
    if (camMedida) ojo = camMedida->GetGlobalPosition();
    else if (g_vistaBindeada) ojo = g_renderCamPos;
    else { bspCluster = -1; RenderFrustum(); return; }   // headless sin vista: no hay ojo que ubicar
    // al espacio LOCAL del Culling (el dato esta en las coordenadas de sus hijos)
    { Matrix4 W, Wi; GetWorldMatrix(W); if (Matrix4::InvertirAfin(W, Wi)) ojo = Wi * ojo; }
    const int cl = bsp.ClusterEn(ojo.x, ojo.y, ojo.z);
    if (cl < 0) { bspCluster = -1; BspEscribirMemoria(); MundoDibujar(1); RenderFrustum(); MundoDibujar(2); return; }   // en solido / fuera del arbol: se ve todo
    if (bspHijosSello != Childrens.size()) {   // los hijos cambiaron (editor): re-emparejar por nombre
        bspHijoIdx.resize(Childrens.size());
        for (size_t c = 0; c < Childrens.size(); c++) bspHijoIdx[c] = bsp.IndiceHijo(Childrens[c]->name);
        bspHijosSello = Childrens.size();
        bspCluster = -1;
    }
    if (cl != bspCluster) {
        if (!bsp.Fila(cl, bspFila)) { bspCluster = -1; MundoDibujar(1); RenderFrustum(); MundoDibujar(2); return; }
        bspCluster = cl;
        bspVisible.assign(Childrens.size(), 0);
        for (size_t c = 0; c < Childrens.size(); c++) {
            const int k = bspHijoIdx[c];
            bspVisible[c] = (k < 0) ? 1 : (VisBsp::Interseca(bspFila, bsp.hijos[(size_t)k].bits) ? 1 : 0);
        }
    }
    // los hijos DINAMICOS (Object::estatico == false) que no estan en el dato: sus clusters por su caja, por cuadro
    // (un NPC que camina: se ve si alguna hoja que toca esta en el PVS). Los estaticos sin dato se dibujan siempre
    for (size_t c = 0; c < Childrens.size(); c++) {
        Object* h = Childrens[c];
        if (bspHijoIdx[c] >= 0 || h->estatico || !h->visible) continue;
        Vector3 mn, mx;
        if (!W3dBoundsSubarbol(h, mn, mx)) { mn = mx = h->GetGlobalPosition(); }
        { Matrix4 W, Wi; GetWorldMatrix(W);
          if (Matrix4::InvertirAfin(W, Wi)) {
              Vector3 e[2] = { Wi * mn, Wi * mx };
              mn = Vector3(e[0].x < e[1].x ? e[0].x : e[1].x, e[0].y < e[1].y ? e[0].y : e[1].y, e[0].z < e[1].z ? e[0].z : e[1].z);
              mx = Vector3(e[0].x > e[1].x ? e[0].x : e[1].x, e[0].y > e[1].y ? e[0].y : e[1].y, e[0].z > e[1].z ? e[0].z : e[1].z);
          } }
        const float a[3] = { mn.x, mn.y, mn.z }, b[3] = { mx.x, mx.y, mx.z };
        bspVisible[c] = (bsp.ClustersDeCaja(a, b, bspBitsTmp) && !VisBsp::Interseca(bspFila, bspBitsTmp)) ? 0 : 1;
    }
    BspEscribirMemoria();   // (cada cuadro: barato, y asi una memoria recien nombrada se llena sin esperar un cambio)
    MundoDibujar(1);            // el mundo opaco
    RenderFrustum(&bspVisible); // los hijos (entidades, lo que mueve el script)
    MundoDibujar(2);            // lo mezclado del mundo (vidrios, haces aditivos): sobre los hijos, como una luz diferida
}

// ---- DISPATCHER: elige el path segun el metodo elegido en el panel.
void Culling::RenderHijos() {
    if (Childrens.empty()) return;
    switch (metodo) {
        case Grid:
            RenderGrid();
            break;
        case Riel:
            // nodo materializado: dibujar LA LISTA, en su orden (cero evaluacion por hijo).
            // Sin nodo (editor sin play, dato ausente): frustum de siempre = se ve todo.
            if (activo && nodoAplicado >= 1) {
                for (size_t i = 0; i < listaRender.size(); i++)
                    if (listaRender[i]) listaRender[i]->Render();
            } else {
                RenderFrustum();
            }
            break;
        case Bsp:
            RenderBsp();
            break;
        case Frustum:
        default:
            RenderFrustum();
            break;
    }
}

const char* CullingMetodoNombre(int m) {
    switch (m) {
        case Culling::Grid:      return "grid";
        case Culling::Bsp:       return "bsp";
        case Culling::Riel:      return "riel";
        default:                 return "frustum";
    }
}
int CullingMetodoDesde(const std::string& s) {
    if (s == "grid")      return Culling::Grid;
    if (s == "bsp")       return Culling::Bsp;
    if (s == "riel")      return Culling::Riel;
    // "triangulo" (retirado del objeto): cae a Frustum; el PVS por triangulo es del
    // modificador "Oclusion" de cada malla, no de este contenedor.
    return Culling::Frustum;
}

#include "Culling.h"
#include "Camera.h"              // CameraActive (la camara del juego)
#include "objects/CameraBase.h"  // g_renderCam* : la vista bindeada + su lente
#include "objects/Mesh.h"        // Mesh::aabbMin/aabbMax (AABB local cacheado)
#include "render/OpcionesRender.h" // g_renderAspect (el aspecto con el que dibuja el juego)
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
// al plano queda del lado de afuera, el AABB entero esta afuera.
static bool AabbEnFrustum(const PlanoFrustum* p, const Vector3& mn, const Vector3& mx) {
    for (int i = 0; i < 6; i++) {
        float px = (p[i].a >= 0.0f) ? mx.x : mn.x;
        float py = (p[i].b >= 0.0f) ? mx.y : mn.y;
        float pz = (p[i].c >= 0.0f) ? mx.z : mn.z;
        if (p[i].a*px + p[i].b*py + p[i].c*pz + p[i].d < 0.0f) return false;
    }
    return true;
}

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

// distancia^2 del punto 'p' al AABB [mn, mx] (0 si esta adentro): el punto del AABB
// mas CERCANO a 'p' es p clampeado a la caja. Sirve para el culling por DISTANCIA:
// "el AABB entero queda mas lejos que distanciaMax" == esta distancia > distanciaMax.
float W3dDist2PuntoAabb(const Vector3& p, const Vector3& mn, const Vector3& mx) {
    float dx = (p.x < mn.x) ? (mn.x - p.x) : (p.x > mx.x ? p.x - mx.x : 0.0f);
    float dy = (p.y < mn.y) ? (mn.y - p.y) : (p.y > mx.y ? p.y - mx.y : 0.0f);
    float dz = (p.z < mn.z) ? (mn.z - p.z) : (p.z > mx.z ? p.z - mx.z : 0.0f);
    return dx*dx + dy*dy + dz*dz;
}

void Culling::RenderHijos() {
    if (Childrens.empty()) return;

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

    // ---- que hijos se ven + a que distancia ----
    std::vector<HijoOrdenado> visibles;
    visibles.reserve(Childrens.size());
    for (size_t c = 0; c < Childrens.size(); c++) {
        Object* h = Childrens[c];
        Vector3 mn, mx;
        bool hayBounds = W3dBoundsSubarbol(h, mn, mx);
        // en ORTOGRAFICA no se corta (el frustum en perspectiva mentiria; ver
        // g_renderCamOrto); sin bounds tampoco (no hay que medir).
        if (!orto && hayBounds && !AabbEnFrustum(planos, mn, mx)) continue;
        // DISTANCIA: si hasta el punto MAS CERCANO del AABB queda mas lejos que
        // distanciaMax, el hijo entero esta fuera de rango -> no se dibuja.
        if (distMax2 > 0.0f && hayBounds && W3dDist2PuntoAabb(cam.pos, mn, mx) > distMax2) continue;
        HijoOrdenado ho;
        ho.o = h;
        ho.matKey = MaterialKeySubarbol(h);
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
    std::sort(visibles.begin(), visibles.end());
    for (size_t i = 0; i < visibles.size(); i++) visibles[i].o->Render();
}

// ============================================================================
//  Particles.cpp — ver Particles.h. El emisor 3D que envuelve al sistema de
//  particulas del Core (graphics/w3dParticles.h): Tick simula en MUNDO, RenderObject
//  ANOTA el emisor y W3dParticlesDrawPending dibuja todas juntas DESPUES
//  de los opacos (billboards a la camara, depth test ON / depth write OFF).
// ============================================================================
#include "w3dGraphics.h"           // abstraccion de graficos (independencia de OpenGL)
#include "Particles.h"
#include "objects/CameraBase.h"   // g_renderCamRight/Up: la vista bindeada (billboards)
#include "objects/Culling.h"      // W3dMeasureCamera: el ojo del emissionRadius
#include "objects/Camera.h"       // Camera (la activa del juego)
#include "objects/RenderColors.h" // gRenderColors / RC_selActive (gizmo del emisor)
#include "render/RenderOptions.h" // g_showOverlays / g_redraw
#include "io/Texture2D.h"         // cache de texturas por ruta (PNG con alpha)
#include "io/UI2DFormat.h"        // g_w3dDirProject: base de las rutas relativas
#include "W3dStore.h"             // contenedor v4 montado: la ruta puede ser una ENTRADA
#include "w3dFilesystem.h"        // FileExists (rutas ya resueltas del formato de texto)
#include "script/BindsGame.h"     // W3dParticlesEmitHook: instalar emitir() de lua
#include <math.h>
#include <sstream>
#include <algorithm> // std::replace (parseo "r, g, b, a")
#include "app/W3dNoNearFar.h"   // (Windows: windows.h define near/far como macros vacias)

namespace gfx = w3dEngine;

// ---------------------------------------------------------------------------
//  La ruta de la textura -> lo que decodifica stb. Mismo contrato que el
//  setTextura de lua (BindsGame.cpp): absoluta tal cual; entrada del contenedor
//  v4 tal cual (ReadFileBytes la resuelve por el montaje); un archivo que existe
//  tal cual (el formato de texto ya resuelve contra la carpeta del .w3d); y si
//  no, colgada de la carpeta del PROYECTO.
// ---------------------------------------------------------------------------
static std::string TexturePath(const std::string& t) {
    if (t.empty()) return t;
    bool absolute = (t[0] == '/') || (t.size() > 1 && t[1] == ':');
    if (absolute) return t;
    W3dStore* alm = W3dMountedStore();
    if (alm && alm->Exists(t)) return t;
    if (w3dFileSystem::FileExists(t)) return t;
    if (!g_w3dDirProject.empty()) return g_w3dDirProject + "/" + t;
    return t;
}

// PRECARGA de las texturas de los emisores: el PNG de un emisor se decodificaba
// en su PRIMERA rafaga (humo/gota/estrella), en pleno gameplay -- un tiron
// visible en el N95. Al abrir el proyecto se tocan todas las rutas: el decode
// (y su cacheo, exitoso o fallido) queda pagado en la CARGA.
void W3dParticlesPreloadTextures(Object* root) {
    if (!root) return;
    std::vector<Object*> st; st.push_back(root);
    while (!st.empty()) {
        Object* o = st.back(); st.pop_back();
        if (o->getType() == ObjectType::particles) {
            Particles* p = (Particles*)o;
            if (!p->texture.empty()) Texture2DGet(TexturePath(p->texture));
        }
        for (size_t i = 0; i < o->Childrens.size(); i++) st.push_back(o->Childrens[i]);
    }
}

// ---------------------------------------------------------------------------
//  config del objeto -> config del Core. Se re-aplica en cada Tick/Emitir:
//  es barato (asignaciones) y el panel edita los campos del OBJETO sin tener
//  que conocer el sistema del Core.
// ---------------------------------------------------------------------------
static void SyncConfig(Particles* p) {
    w3dEngine::ParticleSystem& s = p->sys;
    // variacion (0..1): jitter POR PARTICULA sobre vida y tam via los RANGOS min/max
    // que el Emit() del Core ya sortea con su LCG (deterministico). 0 = rango nulo =
    // exactamente el comportamiento de antes. La velocidad se sortea al emitir (Cono).
    float var = p->variation; if (var < 0.0f) var = 0.0f; if (var > 1.0f) var = 1.0f;
    float v = p->life; if (v < 0.01f) v = 0.01f;
    s.lifeMin = v * (1.0f - var); if (s.lifeMin < 0.01f) s.lifeMin = 0.01f;
    s.lifeMax = v * (1.0f + var);
    float half = p->size * 0.5f;            // el Core usa el MEDIO-lado del quad
    s.sizeMin = half * (1.0f - var); s.sizeMax = half * (1.0f + var); s.sizeEndMul = 1.0f;
    s.gravY = -p->gravity;                // gravedad + = cae (aceleracion -Y de mundo)
    s.turbAmp = (p->turbulence > 0.0f) ? p->turbulence : 0.0f; // deriva suave por particula (Core)
    // MODO DE MEZCLA: el enum del Core va directo (ver Particles.h). El humo/polvo usan Subtract (dst-src),
    // que OSCURECE el fondo en vez de aclararlo.
    s.blend = p->mix;
    s.filter = p->filtered;   // LINEAR/NEAREST de la textura: lo aplica el pase con TexFilter al bindear
    // con mezcla ALFA el color se deja quieto y solo baja el alfa (ver w3dParticles.h); con cualquier otra
    // (aditiva/sustractiva/etc) el color x alfa es lo que hace "desaparecer" la particula sin dejar un agujero.
    s.planeColor = (p->mix == w3dEngine::MixAlpha);
    s.tintR = p->color[0]; s.tintG = p->color[1]; s.tintB = p->color[2];
    float a = p->color[3]; if (a < 0.0f) a = 0.0f; if (a > 1.0f) a = 1.0f;
    s.alphaMin = a; s.alphaMax = a;
    s.fadeIn = 0.0f;
    s.fadeOut = p->fade ? 1.0f : 0.0f;   // 1 = el alfa baja a 0 a lo largo de TODA la vida
    // tope de vivas: lo que pide el rate + margen para las rafagas
    int limit = (int)(p->count * v) + 64;
    if (limit < 64) limit = 64; if (limit > 4096) limit = 4096;
    s.maxParts = limit;
    // FLIPBOOK por edad: el Core recorre la grilla del atlas segun la edad de cada particula
    s.flipCols = p->flipCols > 0 ? p->flipCols : 1;
    s.flipRows = p->flipRows > 0 ? p->flipRows : 1;
    s.flipFrames = p->flipFrames > 0 ? p->flipFrames : 0;
    // SUB-RECT del atlas unico: el sprite (y su grilla) viven en este rect
    s.uvU0 = p->uvRect[0]; s.uvV0 = p->uvRect[1];
    s.uvU1 = p->uvRect[2]; s.uvV1 = p->uvRect[3];
    // lo AVANZADO (curvas por edad, forma): pasa tal cual al Core
    s.grows = p->growth; s.brakeGrows = p->brakeGrowth;
    s.alphaDecays = p->alphaDecays; s.alphaDeath = p->alphaDeath;
    s.drag = p->drag;
    s.useColorFinal = p->useColorFinal;
    s.endColorR = p->colorFinal[0]; s.endColorG = p->colorFinal[1]; s.endColorB = p->colorFinal[2];
    s.alphaEndMul = p->colorFinal[3];
    s.flipFps = p->flipFps; s.flipOnce = p->flipOnce;
    s.appear = p->appear;
    s.near0 = p->fadeNear[0]; s.near1 = p->fadeNear[1];
    s.far0  = p->fadeFar[0]; s.far1 = p->fadeFar[1];
    s.shape = p->shape; s.stretch = p->stretch;
    s.trailPoints = p->trailPoints; s.trailStep = p->trailStep; s.thicknessLine = p->thicknessLine;
}

// origen (mundo) + eje +Y local llevado a mundo (la direccion del cono). La matriz
// EFECTIVA: el chorro sale de donde se VE el emisor (mismo criterio que Culling).
// los ejes LOCALES del emisor en mundo (normalizados): para las cajas de azar y la aceleracion propia
static void LocalAxes(Particles* p, Vector3& ex, Vector3& ey, Vector3& ez) {
    Matrix4 W; p->GetWorldMatrix(W);
    ex = Vector3(W.m[0], W.m[1], W.m[2]); ey = Vector3(W.m[4], W.m[5], W.m[6]); ez = Vector3(W.m[8], W.m[9], W.m[10]);
    float l;
    l = ex.Length(); if (l > 1e-6f) ex = ex * (1.0f / l);
    l = ey.Length(); if (l > 1e-6f) ey = ey * (1.0f / l);
    l = ez.Length(); if (l > 1e-6f) ez = ez * (1.0f / l);
}
// lo AVANZADO de una particula recien nacida: caja de posicion, azar de velocidad y aceleracion propia
// (en ejes locales del emisor, llevados a mundo). Sin nada seteado no toca el LCG (secuencia intacta).
static void ApplyAdvanced(Particles* p, w3dEngine::Particle* q) {
    if (!q) return;
    bool hasPos = p->posRandom[0] != 0.0f || p->posRandom[1] != 0.0f || p->posRandom[2] != 0.0f;
    bool hasVel = p->velRandom[0] != 0.0f || p->velRandom[1] != 0.0f || p->velRandom[2] != 0.0f;
    bool hasBase = p->velLocal[0] != 0.0f || p->velLocal[1] != 0.0f || p->velLocal[2] != 0.0f;
    bool hasAcc = p->acceleration[0] != 0.0f || p->acceleration[1] != 0.0f || p->acceleration[2] != 0.0f;
    if (!hasPos && !hasVel && !hasAcc && !hasBase) return;
    Vector3 ex, ey, ez; LocalAxes(p, ex, ey, ez);
    w3dEngine::ParticleSystem& s = p->sys;
    if (hasPos) {
        Vector3 d = ex * (p->posRandom[0] * s.rnd(-1.0f, 1.0f)) + ey * (p->posRandom[1] * s.rnd(-1.0f, 1.0f)) + ez * (p->posRandom[2] * s.rnd(-1.0f, 1.0f));
        q->x += d.x; q->y += d.y; q->z += d.z;
    }
    if (hasBase) {
        Vector3 d = ex * p->velLocal[0] + ey * p->velLocal[1] + ez * p->velLocal[2];
        q->vx += d.x; q->vy += d.y; q->vz += d.z;
    }
    if (hasVel) {
        Vector3 d = ex * (p->velRandom[0] * s.rnd(-1.0f, 1.0f)) + ey * (p->velRandom[1] * s.rnd(-1.0f, 1.0f)) + ez * (p->velRandom[2] * s.rnd(-1.0f, 1.0f));
        q->vx += d.x; q->vy += d.y; q->vz += d.z;
    }
    if (hasAcc) {
        Vector3 a = ex * p->acceleration[0] + ey * p->acceleration[1] + ez * p->acceleration[2];
        q->ax = a.x; q->ay = a.y; q->az = a.z;
    }
}

static void OriginAndAxis(Particles* p, Vector3& pos, Vector3& axis) {
    Matrix4 W; p->GetWorldMatrix(W);
    pos  = Vector3(W.m[12], W.m[13], W.m[14]);
    axis = Vector3(W.m[4], W.m[5], W.m[6]);   // columna Y (column-major)
    float l = sqrtf(axis.x*axis.x + axis.y*axis.y + axis.z*axis.z);
    if (l < 1e-6f) axis = Vector3(0, 1, 0); else { axis.x /= l; axis.y /= l; axis.z /= l; }
}

// la velocidad de ESTA emision: vel con el jitter de 'variacion' (+-var*100%),
// sorteado con el MISMO LCG del sistema (deterministico; var 0 = vel exacta)
static float VelWithVariation(Particles* p) {
    float var = p->variation; if (var < 0.0f) var = 0.0f; if (var > 1.0f) var = 1.0f;
    if (var <= 0.0f) return p->vel;
    float f = 1.0f + var * (2.0f * p->sys.frnd() - 1.0f);
    return (f > 0.0f) ? p->vel * f : 0.0f;
}

// ROTACION de la particula RECIEN nacida (ver Particles.h). El Emit() del Core
// siempre sortea un rot azaroso (es su default generico); aca el EMISOR decide:
//  - rotacion true  -> se CONSERVA ese angulo azaroso (el "rotz fijo al nacer");
//  - rotacion false -> billboard derecho (rot 0), deterministico como siempre se
//    describio el emisor ("sin giro azaroso: predecible y testeable").
// velRotation (grados/s) = giro continuo con SIGNO azaroso por particula. Solo
// consume un sorteo del LCG cuando se usa: con 0 la secuencia azarosa queda
// IDENTICA a la de antes (round-trip de escenas viejas intacto).
static void ApplyRotation(Particles* p, w3dEngine::Particle* q) {
    if (!q) return;
    if (!p->rotation) {
        // angulo inicial +- azar (grados). Con los dos en 0 = billboard derecho, como siempre.
        float r = p->initialRot;
        if (p->rotRandom != 0.0f) r += p->rotRandom * p->sys.rnd(-1.0f, 1.0f);
        q->rot = r * 0.0174532925f;
    }
    if (p->velRotation != 0.0f || p->velRotRandom != 0.0f) {
        float w = p->velRotation;
        if (p->velRotRandom != 0.0f) w += p->velRotRandom * p->sys.rnd(-1.0f, 1.0f);
        w *= 0.0174532925f;                          // grados/s -> rad/s
        q->spin = (p->turnSignRandom && p->sys.frnd() < 0.5f) ? -w : w;  // signo azaroso POR particula
    } else {
        q->spin = 0.0f;
    }
}

void Particles::Emit(int n) {
    if (n <= 0 || !active) return;   // activo false = apagado del todo (tampoco rafagas)
    SyncConfig(this);
    Vector3 o, e; OriginAndAxis(this, o, e);
    for (int i = 0; i < n; i++) {
        w3dEngine::Particle* q = sys.EmitCone(o.x, o.y, o.z, e.x, e.y, e.z, VelWithVariation(this), dispersion);
        if (!q) break;
        ApplyRotation(this, q);
        ApplyAdvanced(this, q);
    }
    g_redraw = true;
}

void Particles::Tick(float dt, bool canEmit) {
    if (dt <= 0.0f) return;
    SyncConfig(this);
    // la EMISION respeta 'canEmit' (visible); la SIMULACION (sys.Update, abajo) corre SIEMPRE -> las
    // particulas VIVAS (mundo) avanzan y MUEREN aunque el emisor se oculte. Antes, si el objeto era !visible,
    // W3dParticlesTick lo salteaba entero -> las ya emitidas se congelaban y NUNCA morian (acumulacion infinita).
    if (canEmit && active && count > 0.0f) {
        // emision continua con CONO: el acumulador vive aca (sys.rate queda en 0
        // a proposito, el Emit() del Core no sabe de conos ni del transform)
        emAcc += count * dt;
        if (emAcc >= 1.0f) {
            Vector3 o, e; OriginAndAxis(this, o, e);
            while (emAcc >= 1.0f) {
                emAcc -= 1.0f;
                w3dEngine::Particle* q = sys.EmitCone(o.x, o.y, o.z, e.x, e.y, e.z, VelWithVariation(this), dispersion);
                if (!q) { emAcc = 0.0f; break; }
                ApplyRotation(this, q);
                ApplyAdvanced(this, q);
            }
        }
    } else {
        emAcc = 0.0f;
    }
    sys.Update(dt);   // gravedad + vida en espacio de MUNDO
}

// ---------------------------------------------------------------------------
//  "r, g, b, a" <-> color[4] (parseo tolerante, mismo idioma que el LOD)
// ---------------------------------------------------------------------------
std::string Particles::TextColor() const {
    std::ostringstream ss;
    ss << color[0] << ", " << color[1] << ", " << color[2] << ", " << color[3];
    return ss.str();
}
void Particles::SetTextColor(const std::string& s) {
    std::string t = s;
    std::replace(t.begin(), t.end(), ',', ' ');
    std::stringstream ss(t);
    float v[4]; int n = 0;
    while (n < 4 && (ss >> v[n])) n++;
    for (int i = 0; i < n; i++) color[i] = v[i];   // lo que alcanzo a leer; el resto queda
}

// ---------------------------------------------------------------------------
//  El PASE del frame: RenderObject anota, el viewport dibuja despues de opacos.
// ---------------------------------------------------------------------------
static std::vector<Particles*> gPending;

void W3dParticlesClearPending() { gPending.clear(); }

bool Particles::DrawsSomething() {
    extern bool g_showParticles;
    return !sys.parts.empty() || (g_showOverlays && g_showParticles);
}

void Particles::RenderObject() {
    // anotarse UNA sola vez por frame (un Mirror re-renderiza el subarbol y pasaria
    // de nuevo por aca: sin la guarda el flush dibujaria las particulas dos veces)
    bool annotated = false;
    for (size_t i = 0; i < gPending.size(); i++) if (gPending[i] == this) { annotated = true; break; }
    if (!annotated) gPending.push_back(this);   // el dibujo real va DESPUES de los opacos

    // gizmo del EMISOR (overlay del editor, jamas en el render final): una flechita
    // +Y local (la boca del cono), con el color de seleccion de siempre
    if (!g_showOverlays) return;
    { extern bool g_showParticles; if (!g_showParticles) return; } // toggle "Particles": oculta SOLO el gizmo (la flecha), no las particulas
    static const GLfloat arrow[] = {
        0,0,0,       0,0.6f,0,                 // el tallo (+Y local)
        0,0.6f,0,    0.12f,0.42f,0,            // punta
        0,0.6f,0,   -0.12f,0.42f,0,
        -0.25f,0,0,  0.25f,0,0,                // la base (cruz chica)
        0,0,-0.25f,  0,0,0.25f };
    GLboolean lightWas = gfx::IsEnabled(gfx::Lighting);
    gfx::Disable(gfx::Lighting);
    gfx::Disable(gfx::Texture2D);
    gfx::DisableArray(gfx::NormalArray);
    gfx::DisableArray(gfx::TexCoordArray);
    if (select) {
        const float* c = gRenderColors[(this == ActiveObj) ? RC_selActive : RC_selInactive];
        gfx::Color4f(c[0], c[1], c[2], 1.0f);
    } else {
        gfx::Color4f(0.85f, 0.85f, 0.85f, 1.0f);
    }
    gfx::VertexPointer3f(0, arrow);
    gfx::DrawLines(10);
    gfx::EnableArray(gfx::TexCoordArray);
    gfx::EnableArray(gfx::NormalArray);
    if (lightWas) gfx::Enable(gfx::Lighting);
}

// una entrada del pase ordenado: la particula 'idx' de 'sys', con su profundidad de
// vista. operator< = MAS LEJOS primero (back-to-front: el orden correcto para
// translucidas con z-write apagado, TAMBIEN entre emisores distintos que se
// superponen: el humo de una chimenea detras del polvo de las pisadas).
struct SortedPart {
    float prof;                      // profundidad de vista (mas grande = mas lejos)
    w3dEngine::ParticleSystem* sys;
    int   idx;
    bool operator<(const SortedPart& o) const { return prof > o.prof; }
};
// buffer REUSADO entre frames (clear no libera): sin mallocs por frame una vez
// que alcanzo su pico (N total es chico, <500 en el juego).
static std::vector<SortedPart> gSorted;
static std::vector<Particles*> gLines;   // emisores de FORMA LINEA de este frame (pase aparte, sin textura)

// el pase de LINEAS (estelas): sin textura, color por vertice, GL_LINES; un draw por (mezcla, grosor).
// Va despues de los quads con el mismo z-test/z-write. No se ordena por profundidad: las lineas son finas.
static void DrawLines() {
    if (gLines.empty()) return;
    static std::vector<float> lPos;
    static std::vector<unsigned char> lCol;
    gfx::Disable(gfx::Texture2D);
    gfx::DisableArray(gfx::TexCoordArray);
    gfx::EnableArray(gfx::ColorArray);
    for (size_t i = 0; i < gLines.size(); i++) {
        Particles* p = gLines[i];
        lPos.clear(); lCol.clear();
        for (size_t k = 0; k < p->sys.parts.size(); k++) p->sys.AppendTrail(p->sys.parts[k], lPos, lCol);
        if (lPos.empty()) continue;
        gfx::SetMix(p->sys.blend);
        gfx::LineWidth(p->thicknessLine > 0.5f ? p->thicknessLine : 1.0f);
        gfx::VertexPointer3f(0, &lPos[0]);
        gfx::ColorPointer4ub(&lCol[0]);
        gfx::DrawLines((int)(lPos.size() / 3));
    }
    gfx::LineWidth(1.0f);
    gfx::DisableArray(gfx::ColorArray);
    gfx::Enable(gfx::Texture2D);
    gfx::EnableArray(gfx::TexCoordArray);
    gLines.clear();
}

void W3dParticlesDrawPending() {
    if (gPending.empty()) return;
    // ejes del billboard: la camara de la vista bindeada (sin vista: plano XY)
    Vector3 R(1, 0, 0), U(0, 1, 0), F(0, 0, -1);
    if (g_viewBound) { R = g_renderCamRight; U = g_renderCamUp; F = g_renderCamForward; }
    // 1) juntar TODAS las particulas vivas de TODOS los emisores pendientes
    gSorted.clear();
    for (size_t i = 0; i < gPending.size(); i++) {
        Particles* p = gPending[i];
        if (p->sys.parts.empty()) continue;
        if (p->shape == w3dEngine::ShapeLine) { gLines.push_back(p); continue; }   // van en el pase de LINEAS
        unsigned tex = Texture2DGet(TexturePath(p->texture));
        if (!tex) continue;   // sin textura no hay nada que dibujar (el decode fallido se cachea)
        unsigned t[1] = { tex };
        p->sys.SetTextures(t, 1);
        p->sys.eyeX = g_renderCamPos.x; p->sys.eyeY = g_renderCamPos.y; p->sys.eyeZ = g_renderCamPos.z;
        for (size_t k = 0; k < p->sys.parts.size(); k++) {
            const w3dEngine::Particle& q = p->sys.parts[k];
            SortedPart e;
            // profundidad = componente hacia ADELANTE de la camara (z de vista)
            e.prof = (q.x - g_renderCamPos.x) * F.x + (q.y - g_renderCamPos.y) * F.y +
                     (q.z - g_renderCamPos.z) * F.z;
            e.sys = &p->sys; e.idx = (int)k;
            gSorted.push_back(e);
        }
    }
    gPending.clear();
    if (gSorted.empty() && gLines.empty()) return;
    // 2) orden GLOBAL back-to-front (un sort por frame sobre <500 entradas)
    std::sort(gSorted.begin(), gSorted.end());
    // 3) dibujar BATCHEADO (P2): el orden global YA existe; lo que cambia es solo
    //    la EMISION. En vez de un draw POR particula (100 particulas = 100 draw
    //    calls), las consecutivas que comparten (textura, mezcla) se acumulan en
    //    un stream compartido (pos+uv+color POR VERTICE, tope 512 quads) y la
    //    corrida entera sale en UN draw. El orden del pintor se respeta: el lote
    //    solo junta VECINAS de la lista ordenada, jamas reordena. Depth test
    //    PRENDIDO (se tapan detras de los muros) / depth write APAGADO (entre
    //    ellas no se pisan: por eso importa el orden).
    // NINGUNA PARTICULA HEREDA EL ESTADO DE NADIE (mismo criterio que el flush de
    // calcomanias de Mesh.cpp). Este pase corre al final del frame, despues de que
    // dibujo TODO el mundo -- incluidos los espejos, que instalan planos de recorte
    // y una mascara de estencil y juegan con DepthRange/ColorMask para estampar
    // profundidad. El flush de calcomanias afirmaba estas cuatro, pero se saltea
    // entero cuando no hay calcomanias pendientes: entonces las particulas salian
    // recortadas contra la silueta del agua. Son cuatro llamadas por frame.
    gfx::StencilTurnOff();          // sin mascara: la chispa no vive adentro de ningun espejo
    gfx::PlanesClip(0, NULL);       // sin planos de recorte del espejo
    gfx::DepthRange(0.0f, 1.0f);    // el rango normal (el pase B del espejo lo lleva a (1,1))
    gfx::ColorMask(true, true, true, true);
    // LA REGLA DE LO TRANSLUCIDO: z-test SI (la chispa se tapa detras de un muro),
    // z-write NO (ni entre ellas ni contra lo que se dibuje despues: una particula
    // que escribe z le abre un AGUJERO a todo lo que venga atras). DepthFunc tiene
    // que quedar en el normal: el multi-pass de Mesh y los pases del espejo lo
    // dejan en EQUAL/ALWAYS un rato y ahi el z-test mediria cualquier cosa.
    gfx::Enable(gfx::DepthTest);
    gfx::DepthFunc(gfx::DepthLess);
    gfx::DepthMask(false);
    const gfx::StatCat catPrev = gfx::StatCurrentCategory();
    gfx::StatCategory(gfx::StatCatParticles); // presupuesto: estos draws son PARTICULAS
    w3dEngine::ParticleSystem::DrawBillboardState();
    gfx::EnableArray(gfx::ColorArray);          // el lote lleva el color POR VERTICE
    // buffers REUSADOS entre frames (clear no libera): cero allocs tras el pico
    static std::vector<float> bPos, bUV;
    static std::vector<unsigned char> bCol;
    bPos.clear(); bUV.clear(); bCol.clear();
    const size_t kMaxQuads = 512;               // tope del stream por draw (plan P2)
    // FILTRADO por emisor (checkbox "Smooth texture"): TexFilter es estado POR textura, asi que se
    // fija al bindear cada corrida (como el material). Respeta el "modo pixelado" global del editor.
    unsigned texBatch = 0; int blendBatch = -1; bool filtBatch = true;
    for (size_t i = 0; i < gSorted.size(); i++) {
        const SortedPart& e = gSorted[i];
        const w3dEngine::Particle& q = e.sys->parts[e.idx];
        unsigned tex = (e.sys->nTex > 0) ? e.sys->texs[q.tex] : 0;
        if (!tex) continue;
        // corte de corrida: cambio la textura o la mezcla, o el lote llego al tope
        if (!bPos.empty() && (tex != texBatch || e.sys->blend != blendBatch || bPos.size() >= kMaxQuads * 18)) {
            gfx::SetMix(blendBatch); gfx::BindTexture(texBatch);
            gfx::TexFilter(!gfx::PixelatedGlobal() && filtBatch);
            gfx::VertexPointer3f(0, &bPos[0]); gfx::TexCoordPointer2f(0, &bUV[0]);
            gfx::ColorPointer4ub(&bCol[0]);
            gfx::DrawTrianglesArray((int)(bPos.size() / 3));
            bPos.clear(); bUV.clear(); bCol.clear();
        }
        texBatch = tex; blendBatch = e.sys->blend; filtBatch = e.sys->filter;
        e.sys->AppendBillboard(q, R.x, R.y, R.z, U.x, U.y, U.z, bPos, bUV, bCol);
    }
    if (!bPos.empty()) {                        // la ultima corrida
        gfx::SetMix(blendBatch); gfx::BindTexture(texBatch);
        gfx::TexFilter(!gfx::PixelatedGlobal() && filtBatch);
        gfx::VertexPointer3f(0, &bPos[0]); gfx::TexCoordPointer2f(0, &bUV[0]);
        gfx::ColorPointer4ub(&bCol[0]);
        gfx::DrawTrianglesArray((int)(bPos.size() / 3));
    }
    gfx::DisableArray(gfx::ColorArray);
    DrawLines();
    w3dEngine::ParticleSystem::DrawBillboardEnd();
    gfx::StatCategory(catPrev);
    gfx::DepthMask(true);
    gfx::Invalidate();
}

// ---------------------------------------------------------------------------
//  El TICK global (una vez por frame) + "hay algo animando?"
// ---------------------------------------------------------------------------
// las particulas VIVAS se simulan y mueren aunque su emisor o un ancestro esten ocultos (son world-space, ya
// emitidas). La EMISION si respeta la cadena de visibilidad (vis) Y el emissionRadius contra la camara (perf del N95:
// emisores del otro lado del mapa no emiten; las vivas terminan su vida y la sim queda vacia = gratis).
// LOS EMISORES de la escena en el orden del arbol (el de antes: el azar de cada uno sale igual), sin recorrer el arbol
// entero en cada cuadro (en Daytona ~900 nodos por cuadro para 3 emisores; en el N95 milisegundos). Se rearman cuando
// cambia el conjunto de vivos (W3dLiveChanges: nace o muere alguno) o la escena, y cada 120 cuadros de reaseguro (un
// emisor que se mueve a la escena desde otra raiz no avisa). En cada cuadro, la cadena de visibilidad sube por los padres
static std::vector<Particles*> gEmitters;
static void CollectEmitters(Object* o) {
    if (o->getType() == ObjectType::particles) gEmitters.push_back((Particles*)o);
    for (size_t i = 0; i < o->Childrens.size(); i++) CollectEmitters(o->Childrens[i]);
}
void W3dParticlesTick(float dt) {
    if (!SceneCollection || dt <= 0.0f) return;
    static unsigned built = 0;
    static Object* root = 0;
    static int age = 0;
    if (built != W3dLiveChanges() || root != SceneCollection || ++age >= 120) {
        gEmitters.clear();
        if (!W3dLiveOfType((int)ObjectType::particles).empty()) CollectEmitters(SceneCollection);
        built = W3dLiveChanges(); root = SceneCollection; age = 0;
    }
    if (gEmitters.empty()) return;
    // el "ojo" de la medida: la camara del juego jugando, o la vista que dibuja
    Vector3 eye; bool hasEye = false;
    Camera* cam = W3dMeasureCamera(false);
    if (cam) { eye = cam->GetGlobalPosition(); hasEye = true; }
    else if (g_viewBound) { eye = g_renderCamPos; hasEye = true; }
    for (size_t i = 0; i < gEmitters.size(); i++) {
        Particles* p = gEmitters[i];
        // INACTIVO (sin vivas y sin emision continua: las rafagas de emitir() salen en el acto, no pasan por aca):
        // nada que simular ni emitir. Se saltea ANTES de la cadena de visibilidad y de la posicion de mundo (con un
        // Child Of a un hueso cuesta posar el esqueleto): en un nivel con decenas de emisores de efectos esperando
        // su rafaga, en el N95 eran ~14 ms por cuadro.
        if (p->sys.parts.empty() && !(p->active && p->count > 0.0f)) { p->emAcc = 0.0f; continue; }
        bool vis = true;
        const Object* a = p;
        const Object* limit = p;
        for (; a; a = a->Parent) { if (!a->visible) vis = false; limit = a; }
        if (limit != SceneCollection) continue;              // (se fue de la escena: hasta que se rearme)
        bool near = true;
        if (hasEye && p->emissionRadius > 0.0f) {
            Vector3 d = p->GetGlobalPosition() - eye;
            near = d.LengthSq() <= p->emissionRadius * p->emissionRadius;
        }
        p->Tick(dt, vis && near);  // sin emitir: NO emite, SI simula lo vivo
    }
}

bool W3dParticlesAlive() {
    if (!SceneCollection) return false;
    std::vector<Object*> st; st.push_back(SceneCollection);
    while (!st.empty()) {
        Object* o = st.back(); st.pop_back();
        if (!o->visible) continue;
        if (o->getType() == ObjectType::particles && !((Particles*)o)->sys.parts.empty()) return true;
        for (size_t i = 0; i < o->Childrens.size(); i++) st.push_back(o->Childrens[i]);
    }
    return false;
}
bool W3dParticlesAnimating() {
    if (!SceneCollection) return false;
    std::vector<Object*> st; st.push_back(SceneCollection);
    while (!st.empty()) {
        Object* o = st.back(); st.pop_back();
        if (!o->visible) continue;
        if (o->getType() == ObjectType::particles) {
            Particles* p = (Particles*)o;
            if (!p->sys.parts.empty()) return true;              // hay vivas: siguen moviendose
            if (p->active && p->count > 0.0f) return true;       // esta emitiendo solo
        }
        for (size_t i = 0; i < o->Childrens.size(); i++) st.push_back(o->Childrens[i]);
    }
    return false;
}

// ---------------------------------------------------------------------------
//  emitir(obj, n) de lua: el hook vive en BindsGame.cpp (que tambien compila el
//  runtime del juego, sin este objeto); el editor lo instala aca al arrancar.
// ---------------------------------------------------------------------------
static void EmitHook(Object* o, int n) {
    if (o && o->getType() == ObjectType::particles) ((Particles*)o)->Emit(n);
}
static void ActiveHook(Object* o, bool active) {
    if (o && o->getType() == ObjectType::particles) ((Particles*)o)->active = active;
}
struct ParticlesHookReg { ParticlesHookReg() { W3dParticlesEmitHook = EmitHook; W3dActiveParticlesHook = ActiveHook; } };
static ParticlesHookReg gParticlesHookReg;

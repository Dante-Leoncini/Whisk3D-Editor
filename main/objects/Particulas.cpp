// ============================================================================
//  Particulas.cpp — ver Particulas.h. El emisor 3D que envuelve al sistema de
//  particulas del Core (gfx/w3dParticles.h): Tick simula en MUNDO, RenderObject
//  ANOTA el emisor y W3dParticulasDibujarPendientes dibuja todas juntas DESPUES
//  de los opacos (billboards a la camara, depth test ON / depth write OFF).
// ============================================================================
#include "w3dGraphics.h"           // abstraccion de graficos (independencia de OpenGL)
#include "Particulas.h"
#include "objects/CameraBase.h"    // g_renderCamRight/Up: la vista bindeada (billboards)
#include "objects/RenderColors.h"  // gRenderColors / RC_selActive (gizmo del emisor)
#include "render/OpcionesRender.h" // g_mostrarOverlays / g_redraw
#include "io/Textura2D.h"          // cache de texturas por ruta (PNG con alpha)
#include "io/UI2DFormato.h"        // g_w3dDirProyecto: base de las rutas relativas
#include "W3dAlmacen.h"            // contenedor v4 montado: la ruta puede ser una ENTRADA
#include "w3dFilesystem.h"         // FileExists (rutas ya resueltas del formato de texto)
#include "script/BindsJuego.h"     // W3dParticulasEmitirHook: instalar emitir() de lua
#include <math.h>
#include <sstream>
#include <algorithm> // std::replace (parseo "r, g, b, a")

namespace gfx = w3dEngine;

// ---------------------------------------------------------------------------
//  La ruta de la textura -> lo que decodifica stb. Mismo contrato que el
//  setTextura de lua (BindsJuego.cpp): absoluta tal cual; entrada del contenedor
//  v4 tal cual (ReadFileBytes la resuelve por el montaje); un archivo que existe
//  tal cual (el formato de texto ya resuelve contra la carpeta del .w3d); y si
//  no, colgada de la carpeta del PROYECTO.
// ---------------------------------------------------------------------------
static std::string RutaTextura(const std::string& t) {
    if (t.empty()) return t;
    bool absoluta = (t[0] == '/') || (t.size() > 1 && t[1] == ':');
    if (absoluta) return t;
    W3dAlmacen* alm = W3dAlmacenMontado();
    if (alm && alm->Existe(t)) return t;
    if (w3dFileSystem::FileExists(t)) return t;
    if (!g_w3dDirProyecto.empty()) return g_w3dDirProyecto + "/" + t;
    return t;
}

// ---------------------------------------------------------------------------
//  config del objeto -> config del Core. Se re-aplica en cada Tick/Emitir:
//  es barato (asignaciones) y el panel edita los campos del OBJETO sin tener
//  que conocer el sistema del Core.
// ---------------------------------------------------------------------------
static void SincronizarConfig(Particulas* p) {
    w3dEngine::ParticleSystem& s = p->sys;
    // variacion (0..1): jitter POR PARTICULA sobre vida y tam via los RANGOS min/max
    // que el Emit() del Core ya sortea con su LCG (deterministico). 0 = rango nulo =
    // exactamente el comportamiento de antes. La velocidad se sortea al emitir (Cono).
    float var = p->variacion; if (var < 0.0f) var = 0.0f; if (var > 1.0f) var = 1.0f;
    float v = p->vida; if (v < 0.01f) v = 0.01f;
    s.lifeMin = v * (1.0f - var); if (s.lifeMin < 0.01f) s.lifeMin = 0.01f;
    s.lifeMax = v * (1.0f + var);
    float half = p->tam * 0.5f;            // el Core usa el MEDIO-lado del quad
    s.sizeMin = half * (1.0f - var); s.sizeMax = half * (1.0f + var); s.sizeEndMul = 1.0f;
    s.gravY = -p->gravedad;                // gravedad + = cae (aceleracion -Y de mundo)
    s.turbAmp = (p->turbulencia > 0.0f) ? p->turbulencia : 0.0f; // deriva suave por particula (Core)
    // SUSTRACTIVA gana sobre aditiva (ver Particulas.h): dst - src, para el humo
    // y el polvo, que tienen que OSCURECER el fondo en vez de aclararlo.
    s.blend = p->sustractivo ? w3dEngine::MezclaSubtract
            : (p->aditivo    ? w3dEngine::MezclaAdd : w3dEngine::MezclaAlpha);
    // con mezcla ALFA el color se deja quieto y solo baja el alfa (ver w3dParticles.h);
    // con ADITIVA el color x alfa es lo que hace "desaparecer" a la particula
    // con SUSTRACTIVA vale lo mismo que con aditiva: el color tiene que caer con el
    // alfa, si no la particula sigue restando lo mismo hasta el ultimo frame y
    // "desaparece" de golpe dejando un agujero oscuro.
    s.colorPlano = !(p->aditivo || p->sustractivo);
    s.tintR = p->color[0]; s.tintG = p->color[1]; s.tintB = p->color[2];
    float a = p->color[3]; if (a < 0.0f) a = 0.0f; if (a > 1.0f) a = 1.0f;
    s.alphaMin = a; s.alphaMax = a;
    s.fadeIn = 0.0f;
    s.fadeOut = p->desvanecer ? 1.0f : 0.0f;   // 1 = el alfa baja a 0 a lo largo de TODA la vida
    // tope de vivas: lo que pide el rate + margen para las rafagas
    int tope = (int)(p->cantidad * v) + 64;
    if (tope < 64) tope = 64; if (tope > 4096) tope = 4096;
    s.maxParts = tope;
}

// origen (mundo) + eje +Y local llevado a mundo (la direccion del cono). La matriz
// EFECTIVA: el chorro sale de donde se VE el emisor (mismo criterio que Culling).
static void OrigenYEje(Particulas* p, Vector3& pos, Vector3& eje) {
    Matrix4 W; p->GetWorldMatrix(W);
    pos = Vector3(W.m[12], W.m[13], W.m[14]);
    eje = Vector3(W.m[4], W.m[5], W.m[6]);   // columna Y (column-major)
    float l = sqrtf(eje.x*eje.x + eje.y*eje.y + eje.z*eje.z);
    if (l < 1e-6f) eje = Vector3(0, 1, 0); else { eje.x /= l; eje.y /= l; eje.z /= l; }
}

// la velocidad de ESTA emision: vel con el jitter de 'variacion' (+-var*100%),
// sorteado con el MISMO LCG del sistema (deterministico; var 0 = vel exacta)
static float VelConVariacion(Particulas* p) {
    float var = p->variacion; if (var < 0.0f) var = 0.0f; if (var > 1.0f) var = 1.0f;
    if (var <= 0.0f) return p->vel;
    float f = 1.0f + var * (2.0f * p->sys.frnd() - 1.0f);
    return (f > 0.0f) ? p->vel * f : 0.0f;
}

// ROTACION de la particula RECIEN nacida (ver Particulas.h). El Emit() del Core
// siempre sortea un rot azaroso (es su default generico); aca el EMISOR decide:
//  - rotacion true  -> se CONSERVA ese angulo azaroso (el "rotz fijo al nacer");
//  - rotacion false -> billboard derecho (rot 0), deterministico como siempre se
//    describio el emisor ("sin giro azaroso: predecible y testeable").
// velRotacion (grados/s) = giro continuo con SIGNO azaroso por particula. Solo
// consume un sorteo del LCG cuando se usa: con 0 la secuencia azarosa queda
// IDENTICA a la de antes (round-trip de escenas viejas intacto).
static void AplicarRotacion(Particulas* p, w3dEngine::Particle* q) {
    if (!q) return;
    if (!p->rotacion) q->rot = 0.0f;
    if (p->velRotacion != 0.0f) {
        float w = p->velRotacion * 0.0174532925f;   // grados/s -> rad/s
        q->spin = (p->sys.frnd() < 0.5f) ? -w : w;  // signo azaroso POR particula
    } else {
        q->spin = 0.0f;
    }
}

void Particulas::Emitir(int n) {
    if (n <= 0 || !activo) return;   // activo false = apagado del todo (tampoco rafagas)
    SincronizarConfig(this);
    Vector3 o, e; OrigenYEje(this, o, e);
    for (int i = 0; i < n; i++) {
        w3dEngine::Particle* q = sys.EmitCono(o.x, o.y, o.z, e.x, e.y, e.z, VelConVariacion(this), dispersion);
        if (!q) break;
        AplicarRotacion(this, q);
    }
    g_redraw = true;
}

void Particulas::Tick(float dt) {
    if (dt <= 0.0f) return;
    SincronizarConfig(this);
    if (activo && cantidad > 0.0f) {
        // emision continua con CONO: el acumulador vive aca (sys.rate queda en 0
        // a proposito, el Emit() del Core no sabe de conos ni del transform)
        emAcc += cantidad * dt;
        if (emAcc >= 1.0f) {
            Vector3 o, e; OrigenYEje(this, o, e);
            while (emAcc >= 1.0f) {
                emAcc -= 1.0f;
                w3dEngine::Particle* q = sys.EmitCono(o.x, o.y, o.z, e.x, e.y, e.z, VelConVariacion(this), dispersion);
                if (!q) { emAcc = 0.0f; break; }
                AplicarRotacion(this, q);
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
std::string Particulas::ColorTexto() const {
    std::ostringstream ss;
    ss << color[0] << ", " << color[1] << ", " << color[2] << ", " << color[3];
    return ss.str();
}
void Particulas::SetColorTexto(const std::string& s) {
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
static std::vector<Particulas*> gPendientes;

void W3dParticulasLimpiarPendientes() { gPendientes.clear(); }

void Particulas::RenderObject() {
    // anotarse UNA sola vez por frame (un Mirror re-renderiza el subarbol y pasaria
    // de nuevo por aca: sin la guarda el flush dibujaria las particulas dos veces)
    bool anotado = false;
    for (size_t i = 0; i < gPendientes.size(); i++) if (gPendientes[i] == this) { anotado = true; break; }
    if (!anotado) gPendientes.push_back(this);   // el dibujo real va DESPUES de los opacos

    // gizmo del EMISOR (overlay del editor, jamas en el render final): una flechita
    // +Y local (la boca del cono), con el color de seleccion de siempre
    if (!g_mostrarOverlays) return;
    static const GLfloat flecha[] = {
        0,0,0,       0,0.6f,0,                 // el tallo (+Y local)
        0,0.6f,0,    0.12f,0.42f,0,            // punta
        0,0.6f,0,   -0.12f,0.42f,0,
        -0.25f,0,0,  0.25f,0,0,                // la base (cruz chica)
        0,0,-0.25f,  0,0,0.25f };
    GLboolean luzEstaba = gfx::IsEnabled(gfx::Lighting);
    gfx::Disable(gfx::Lighting);
    gfx::Disable(gfx::Texture2D);
    gfx::DisableArray(gfx::NormalArray);
    gfx::DisableArray(gfx::TexCoordArray);
    if (select) {
        const float* c = gRenderColors[(this == ObjActivo) ? RC_selActive : RC_selInactive];
        gfx::Color4f(c[0], c[1], c[2], 1.0f);
    } else {
        gfx::Color4f(0.85f, 0.85f, 0.85f, 1.0f);
    }
    gfx::VertexPointer3f(0, flecha);
    gfx::DrawLines(10);
    gfx::EnableArray(gfx::TexCoordArray);
    gfx::EnableArray(gfx::NormalArray);
    if (luzEstaba) gfx::Enable(gfx::Lighting);
}

// una entrada del pase ordenado: la particula 'idx' de 'sys', con su profundidad de
// vista. operator< = MAS LEJOS primero (back-to-front: el orden correcto para
// translucidas con z-write apagado, TAMBIEN entre emisores distintos que se
// superponen: el humo de una chimenea detras del polvo de las pisadas).
struct PartOrdenada {
    float prof;                      // profundidad de vista (mas grande = mas lejos)
    w3dEngine::ParticleSystem* sys;
    int   idx;
    bool operator<(const PartOrdenada& o) const { return prof > o.prof; }
};
// buffer REUSADO entre frames (clear no libera): sin mallocs por frame una vez
// que alcanzo su pico (N total es chico, <500 en el juego).
static std::vector<PartOrdenada> gOrdenadas;

void W3dParticulasDibujarPendientes() {
    if (gPendientes.empty()) return;
    // ejes del billboard: la camara de la vista bindeada (sin vista: plano XY)
    Vector3 R(1, 0, 0), U(0, 1, 0), F(0, 0, -1);
    if (g_vistaBindeada) { R = g_renderCamRight; U = g_renderCamUp; F = g_renderCamForward; }
    // 1) juntar TODAS las particulas vivas de TODOS los emisores pendientes
    gOrdenadas.clear();
    for (size_t i = 0; i < gPendientes.size(); i++) {
        Particulas* p = gPendientes[i];
        if (p->sys.parts.empty()) continue;
        unsigned tex = Textura2DObtener(RutaTextura(p->textura));
        if (!tex) continue;   // sin textura no hay nada que dibujar (el decode fallido se cachea)
        unsigned t[1] = { tex };
        p->sys.SetTexturas(t, 1);
        for (size_t k = 0; k < p->sys.parts.size(); k++) {
            const w3dEngine::Particle& q = p->sys.parts[k];
            PartOrdenada e;
            // profundidad = componente hacia ADELANTE de la camara (z de vista)
            e.prof = (q.x - g_renderCamPos.x) * F.x + (q.y - g_renderCamPos.y) * F.y +
                     (q.z - g_renderCamPos.z) * F.z;
            e.sys = &p->sys; e.idx = (int)k;
            gOrdenadas.push_back(e);
        }
    }
    gPendientes.clear();
    if (gOrdenadas.empty()) return;
    // 2) orden GLOBAL back-to-front (un sort por frame sobre <500 entradas)
    std::sort(gOrdenadas.begin(), gOrdenadas.end());
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
    gfx::EstencilApagar();          // sin mascara: la chispa no vive adentro de ningun espejo
    gfx::PlanosRecorte(0, NULL);    // sin planos de recorte del espejo
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
    gfx::StatCategoria(gfx::StatCatParticulas); // presupuesto: estos draws son PARTICULAS
    w3dEngine::ParticleSystem::DrawBillboardEstado();
    gfx::EnableArray(gfx::ColorArray);          // el lote lleva el color POR VERTICE
    // buffers REUSADOS entre frames (clear no libera): cero allocs tras el pico
    static std::vector<float> bPos, bUV;
    static std::vector<unsigned char> bCol;
    bPos.clear(); bUV.clear(); bCol.clear();
    const size_t kMaxQuads = 512;               // tope del stream por draw (plan P2)
    unsigned texLote = 0; int blendLote = -1;
    for (size_t i = 0; i < gOrdenadas.size(); i++) {
        const PartOrdenada& e = gOrdenadas[i];
        const w3dEngine::Particle& q = e.sys->parts[e.idx];
        unsigned tex = (e.sys->nTex > 0) ? e.sys->texs[q.tex] : 0;
        if (!tex) continue;
        // corte de corrida: cambio la textura o la mezcla, o el lote llego al tope
        if (!bPos.empty() && (tex != texLote || e.sys->blend != blendLote || bPos.size() >= kMaxQuads * 18)) {
            gfx::SetMezcla(blendLote); gfx::BindTexture(texLote);
            gfx::VertexPointer3f(0, &bPos[0]); gfx::TexCoordPointer2f(0, &bUV[0]);
            gfx::ColorPointer4ub(&bCol[0]);
            gfx::DrawTrianglesArray((int)(bPos.size() / 3));
            bPos.clear(); bUV.clear(); bCol.clear();
        }
        texLote = tex; blendLote = e.sys->blend;
        e.sys->AppendBillboard(q, R.x, R.y, R.z, U.x, U.y, U.z, bPos, bUV, bCol);
    }
    if (!bPos.empty()) {                        // la ultima corrida
        gfx::SetMezcla(blendLote); gfx::BindTexture(texLote);
        gfx::VertexPointer3f(0, &bPos[0]); gfx::TexCoordPointer2f(0, &bUV[0]);
        gfx::ColorPointer4ub(&bCol[0]);
        gfx::DrawTrianglesArray((int)(bPos.size() / 3));
    }
    gfx::DisableArray(gfx::ColorArray);
    w3dEngine::ParticleSystem::DrawBillboardFin();
    gfx::StatCategoria(gfx::StatCatEscena);
    gfx::DepthMask(true);
    gfx::Invalidate();
}

// ---------------------------------------------------------------------------
//  El TICK global (una vez por frame) + "hay algo animando?"
// ---------------------------------------------------------------------------
void W3dParticulasTick(float dt) {
    if (!SceneCollection || dt <= 0.0f) return;
    std::vector<Object*> st; st.push_back(SceneCollection);
    while (!st.empty()) {
        Object* o = st.back(); st.pop_back();
        if (!o->visible) continue;   // oculto (con su subarbol): ni emite ni simula
        if (o->getType() == ObjectType::particulas) ((Particulas*)o)->Tick(dt);
        for (size_t i = 0; i < o->Childrens.size(); i++) st.push_back(o->Childrens[i]);
    }
}

bool W3dParticulasAnimando() {
    if (!SceneCollection) return false;
    std::vector<Object*> st; st.push_back(SceneCollection);
    while (!st.empty()) {
        Object* o = st.back(); st.pop_back();
        if (!o->visible) continue;
        if (o->getType() == ObjectType::particulas) {
            Particulas* p = (Particulas*)o;
            if (!p->sys.parts.empty()) return true;                 // hay vivas: siguen moviendose
            if (p->activo && p->cantidad > 0.0f) return true;       // esta emitiendo solo
        }
        for (size_t i = 0; i < o->Childrens.size(); i++) st.push_back(o->Childrens[i]);
    }
    return false;
}

// ---------------------------------------------------------------------------
//  emitir(obj, n) de lua: el hook vive en BindsJuego.cpp (que tambien compila el
//  runtime del juego, sin este objeto); el editor lo instala aca al arrancar.
// ---------------------------------------------------------------------------
static void EmitirHook(Object* o, int n) {
    if (o && o->getType() == ObjectType::particulas) ((Particulas*)o)->Emitir(n);
}
struct ParticulasHookReg { ParticulasHookReg() { W3dParticulasEmitirHook = EmitirHook; } };
static ParticulasHookReg gParticulasHookReg;

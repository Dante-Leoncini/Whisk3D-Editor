// ============================================================================
//  Sonido.cpp — ver Sonido.h
// ============================================================================
#include "objects/Sonido.h"
#include "objects/Camera.h"            // CameraActive: quien escucha en el juego
#include "objects/RenderColors.h"      // el color de seleccion (como el resto de los objetos)
#include "animation/Animation.h"       // PlayAnimation / CurrentFrame / AnimSonido (g_animSonidoHook)
#include "w3dGraphics.h"
#include "w3dTexture.h"            // UploadRGBA: la textura de la forma de onda
#include "w3dFilesystem.h"
#include "base/w3dlog.h"
#include <map>
#include <set>
#include <vector>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
extern "C" {
#include "lua.h"
#include "lauxlib.h"
}
#include "script/W3dScript.h"          // W3dScriptParamObjeto

// (audio/W3dAudio.h, declarado a mano como en Properties.cpp: no mete el dir de audio en el include path de todas
//  las plataformas)
namespace w3dEngine {
    class W3dSound;
    struct W3dEnvolvente;
    W3dSound* W3dSoundLoadMemory(const void* bytes, size_t len);
    int   W3dSoundPlayEnv(W3dSound* s, float volume, bool loop, float pitch, float pan, double enSeg,
                          const W3dEnvolvente* env, float envioReverb);
    void  W3dSoundStopFade(int voice, float fadeSeg);
    void  W3dSoundSetVolume(int voice, float volume);
    void  W3dSoundSetPitch(int voice, float pitch);
    void  W3dSoundSetPan(int voice, float pan);
    void  W3dSoundSetEnvio(int voice, float envioReverb);
    float W3dSoundPos(int voice);
    void  W3dSoundSetPos(int voice, float seg);
    float W3dSoundDuracion(const W3dSound* s);
    W3dSound* W3dSoundRecortar(const W3dSound* s, float desde, float hasta);
    void  W3dSoundPicos(const W3dSound* s, int n, float* picos);
    double W3dAudioReloj();
    void  W3dAudioReverb(float sala, float amortiguacion, float nivel);
}
namespace gfx = w3dEngine;
extern bool g_showSonidos;   // el overlay "Sounds" del editor (el juego lo tiene apagado)
extern int AnimFPS;

namespace {
std::set<Sonido*> gVivos;                                   // todos (tambien los que el undo saco del arbol)
std::map<std::string, w3dEngine::W3dSound*> gArchivos;      // los archivos cargados (por ruta)
std::map<std::string, w3dEngine::W3dSound*> gTramos;        // los tramos recortados para el bucle ("ruta|ini|fin")
std::map<std::string, unsigned int> gOndas;                 // la forma de onda de cada archivo
bool gReverbPuesta = false;
}

bool (*W3dSonidoOyenteHook)(Vector3& pos, Vector3& der, Vector3& adel) = 0;

float W3dSonidoSegDeFrame(float frame) { return (frame - 1.0f) / (float)(AnimFPS > 0 ? AnimFPS : 30); }
float W3dSonidoFrameDeSeg(float seg) { return 1.0f + seg * (float)(AnimFPS > 0 ? AnimFPS : 30); }

w3dEngine::W3dSound* W3dSonidoCargar(const std::string& archivo) {
    if (archivo.empty()) return NULL;
    std::map<std::string, w3dEngine::W3dSound*>::iterator it = gArchivos.find(archivo);
    if (it != gArchivos.end()) return it->second;
    w3dEngine::W3dSound* s = NULL;
    std::vector<unsigned char> b;
    if (w3dFileSystem::ReadFileBytes(archivo, b) && !b.empty()) s = w3dEngine::W3dSoundLoadMemory(&b[0], b.size());
    if (!s) w3dLogfW("[sonido] no se pudo cargar '%s'", archivo.c_str());
    gArchivos[archivo] = s;   // (tambien el NULL: no se reintenta en cada cuadro)
    return s;
}
// el TRAMO recortado como sonido propio (solo hace falta para el BUCLE: el mixer da la vuelta al final de lo que
// tiene, exacto en la muestra). Sin recortes es el archivo
static w3dEngine::W3dSound* Tramo(const Sonido* o) {
    w3dEngine::W3dSound* base = W3dSonidoCargar(o->archivo);
    if (!base || (o->recorteIni <= 0.0f && o->recorteFin <= 0.0f)) return base;
    const float dur = w3dEngine::W3dSoundDuracion(base);
    char k[64]; snprintf(k, sizeof k, "|%.5f|%.5f", o->recorteIni, o->recorteFin);
    const std::string clave = o->archivo + k;
    std::map<std::string, w3dEngine::W3dSound*>::iterator it = gTramos.find(clave);
    if (it != gTramos.end()) return it->second;
    w3dEngine::W3dSound* t = w3dEngine::W3dSoundRecortar(base, o->recorteIni, dur - o->recorteFin);
    gTramos[clave] = t;
    return t;
}
unsigned int W3dSonidoOnda(const std::string& archivo) {
    std::map<std::string, unsigned int>::iterator it = gOndas.find(archivo);
    if (it != gOndas.end()) return it->second;
    unsigned int id = 0;
    w3dEngine::W3dSound* s = W3dSonidoCargar(archivo);
    if (s) {
        const int W = 4096, H = 64;
        std::vector<float> p(W);
        w3dEngine::W3dSoundPicos(s, W, &p[0]);
        // NORMALIZADA al pico del archivo: un audio bajo tambien se ve (es para ubicarse, no para medir)
        float mx = 0.0f; for (int x = 0; x < W; x++) if (p[x] > mx) mx = p[x];
        if (mx > 1e-4f) for (int x = 0; x < W; x++) p[x] /= mx;
        std::vector<unsigned char> px((size_t)W * H * 4, 0);
        for (int x = 0; x < W; x++) {
            const int a = (int)(p[x] * (H / 2 - 1) + 0.5f);
            for (int y = H / 2 - a; y <= H / 2 + a; y++) {
                if (y < 0 || y >= H) continue;
                unsigned char* q = &px[((size_t)y * W + x) * 4];
                q[0] = q[1] = q[2] = q[3] = 255;
            }
        }
        id = gfx::UploadRGBA(&px[0], W, H, true, false, true, false);
    }
    gOndas[archivo] = id;
    return id;
}

// ============================================================================
//  el objeto
// ============================================================================
Sonido::Sonido(Object* parent, Vector3 pos)
    : Object(parent, "Sound", pos), activo(true), volumen(1.0f), tono(1.0f), reverb(0.0f), paneo(0.0f), inicio(0.0f),
      recorteIni(0.0f), recorteFin(0.0f), bucle(false), posicional(false), radio(2.0f), alcance(30.0f),
      voz(0), tJuego0(-1.0), ondaTex(0) {
    gVivos.insert(this);
}
Sonido::~Sonido() { Callar(); gVivos.erase(this); }
void Sonido::Callar() { if (voz > 0) w3dEngine::W3dSoundStopFade(voz, 0.02f); voz = 0; }
void Sonido::CopiarDe(const Sonido* s) {
    archivo = s->archivo; activo = s->activo; volumen = s->volumen; tono = s->tono; reverb = s->reverb; paneo = s->paneo;
    inicio = s->inicio; recorteIni = s->recorteIni; recorteFin = s->recorteFin; bucle = s->bucle;
    posicional = s->posicional; radio = s->radio; alcance = s->alcance;
}
float Sonido::DuracionArchivo() { return w3dEngine::W3dSoundDuracion(W3dSonidoCargar(archivo)); }
float Sonido::DuracionTramo() {
    float d = DuracionArchivo() - recorteIni - recorteFin;
    return d > 0.0f ? d : 0.0f;
}
float Sonido::DuracionEnEscena() { return DuracionTramo() / (tono > 0.05f ? tono : 0.05f); }

bool Sonido::DibujaAlgo() { return g_showSonidos; }
// el PARLANTE es un ICONO (render.cpp, como la lampara de las luces); aca, si es 3D, las esferas de radio y alcance
void Sonido::RenderObject() {
    if (!g_showSonidos) return;
    static std::vector<GLfloat> v;
    v.clear();
    // las ESFERAS: tres circulos por esfera (los tres planos)
    if (posicional && select) {   // (solo con el sonido SELECCIONADO: con todos a la vista no se entendia nada)
        const float rr[2] = { radio, alcance };
        for (int e = 0; e < 2; e++) {
            const float r = rr[e]; if (r <= 0.0f) continue;
            const int N = 48;
            for (int pl = 0; pl < 3; pl++)
                for (int i = 0; i < N; i++) {
                    const float a0 = i * 6.2831853f / N, a1 = (i + 1) * 6.2831853f / N;
                    float p0[3], p1[3];
                    const float x0 = cosf(a0) * r, y0 = sinf(a0) * r, x1 = cosf(a1) * r, y1 = sinf(a1) * r;
                    if (pl == 0) { p0[0] = x0; p0[1] = y0; p0[2] = 0; p1[0] = x1; p1[1] = y1; p1[2] = 0; }
                    else if (pl == 1) { p0[0] = x0; p0[1] = 0; p0[2] = y0; p1[0] = x1; p1[1] = 0; p1[2] = y1; }
                    else { p0[0] = 0; p0[1] = x0; p0[2] = y0; p1[0] = 0; p1[1] = x1; p1[2] = y1; }
                    for (int q = 0; q < 3; q++) v.push_back(p0[q]);
                    for (int q = 0; q < 3; q++) v.push_back(p1[q]);
                }
        }
    }
    if (v.empty()) return;
    GLboolean luzEstaba = gfx::IsEnabled(gfx::Lighting);
    gfx::Disable(gfx::Lighting);
    gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray);   // (encima del trazado queda su textura puesta)
    gfx::DisableArray(gfx::NormalArray);
    if (select) {
        const float* col = gRenderColors[(this == ObjActivo) ? RC_selActive : RC_selInactive];
        gfx::Color4f(col[0], col[1], col[2], 1.0f);
    } else gfx::Color4f(0.85f, 0.85f, 0.85f, 1.0f);
    if (!v.empty()) { gfx::VertexPointer3f(0, &v[0]); gfx::DrawLines((int)(v.size() / 3)); }
    gfx::EnableArray(gfx::NormalArray);
    if (luzEstaba) gfx::Enable(gfx::Lighting);
}

// ============================================================================
//  la reproduccion
// ============================================================================
static void Juntar(Object* o, std::vector<Sonido*>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (h->getType() == ObjectType::sonido) out.push_back((Sonido*)h);
        Juntar(h, out);
    }
}
// visible considerando los padres (ocultar la coleccion calla sus sonidos)
static bool Visible(Object* o) { for (; o; o = o->Parent) if (!o->visible) return false; return true; }

struct Oyente { Vector3 pos, der, adel; bool hay; };
static Oyente OyenteActual(bool juego) {
    Oyente oy; oy.hay = false;
    if (!juego && W3dSonidoOyenteHook && W3dSonidoOyenteHook(oy.pos, oy.der, oy.adel)) { oy.hay = true; return oy; }
    if (CameraActive) {
        Matrix4 W; CameraActive->GetWorldMatrix(W);
        oy.pos = Vector3(W.m[12], W.m[13], W.m[14]);
        oy.der = Vector3(W.m[0], W.m[1], W.m[2]).Normalized();
        oy.adel = Vector3(-W.m[8], -W.m[9], -W.m[10]).Normalized();   // (la camara mira a su -Z)
        oy.hay = true;
    } else if (juego && W3dSonidoOyenteHook && W3dSonidoOyenteHook(oy.pos, oy.der, oy.adel)) oy.hay = true;
    return oy;
}
// cuanto se escucha y de que lado (3D). Hasta 'radio' entero; despues baja hasta 0 en 'alcance'; a los costados
// panea; atras suena mas bajo y mas al medio (el mezclador es estereo)
static void Espacial(const Sonido* s, const Oyente& oy, float& ganancia, float& pan) {
    ganancia = 1.0f; pan = s->paneo;
    if (!s->posicional) return;
    if (!oy.hay) { pan = 0.0f; return; }
    Matrix4 W; s->GetWorldMatrix(W);
    Vector3 d(W.m[12] - oy.pos.x, W.m[13] - oy.pos.y, W.m[14] - oy.pos.z);
    const float dist = d.Length();
    const float r = s->radio > 0.0f ? s->radio : 0.0f, a = s->alcance > r + 0.01f ? s->alcance : r + 0.01f;
    if (dist >= a) { ganancia = 0.0f; pan = 0.0f; return; }
    if (dist > r) { const float k = 1.0f - (dist - r) / (a - r); ganancia = k * k; }
    if (dist < 1e-4f) { pan = 0.0f; return; }
    d = d * (1.0f / dist);
    const float x = d.x * oy.der.x + d.y * oy.der.y + d.z * oy.der.z;     // derecha +
    const float z = d.x * oy.adel.x + d.y * oy.adel.y + d.z * oy.adel.z;  // adelante +
    pan = x * 0.9f;
    if (z < 0.0f) { ganancia *= 1.0f + 0.35f * z; pan *= 1.0f + 0.4f * z; }   // atras: mas bajo y mas al medio
}

// UNA pasada: 'local' = segundos desde que arranco (de la escena o del juego). sonar = deberia estar sonando
static bool gSaltoEscena = false;
static void Actualizar(Sonido* s, bool sonar, float local, const Oyente& oy, bool escena, bool& hayReverb) {
    w3dEngine::W3dSound* t = NULL;
    float L = 0.0f;
    if (sonar && s->activo && Visible(s) && local >= 0.0f) {
        L = s->DuracionTramo();
        if (L > 0.0f) t = s->bucle ? Tramo(s) : W3dSonidoCargar(s->archivo);
    }
    const float tono = s->tono < 0.05f ? 0.05f : (s->tono > 8.0f ? 8.0f : s->tono);
    float seg = local * tono;   // segundos DEL TRAMO
    if (!t || (!s->bucle && seg >= L)) { s->Callar(); return; }
    float pos = s->bucle ? fmodf(seg, L) : seg;
    // con bucle suena el TRAMO (empieza en 0); sin bucle, el archivo entero desde el recorte
    const float posVoz = s->bucle ? pos : s->recorteIni + pos;
    { char k[96]; snprintf(k, sizeof k, "|%.5f|%.5f|%d", s->recorteIni, s->recorteFin, s->bucle ? 1 : 0);
      const std::string clave = s->archivo + k;
      if (s->voz > 0 && clave != s->vozFuente) s->Callar();   // (otro archivo, otro recorte o el bucle: otra fuente)
      s->vozFuente = clave; }
    float g, pan; Espacial(s, oy, g, pan);
    const float vol = (s->volumen < 0.0f ? 0.0f : s->volumen) * g;
    if (s->voz > 0 && w3dEngine::W3dSoundPos(s->voz) < 0.0f) s->voz = 0;   // (termino o la cortaron)
    if (s->voz <= 0) {
        s->voz = w3dEngine::W3dSoundPlayEnv(t, vol, s->bucle, tono, pan, 0.0, NULL, s->reverb);
        if (s->voz > 0) w3dEngine::W3dSoundSetPos(s->voz, posVoz);
    } else {
        // SINCRONIA: si la linea de tiempo salto (o la voz se corrio mas de 0.12 s) se la lleva a donde va
        float p = w3dEngine::W3dSoundPos(s->voz);
        float dif = p - posVoz;
        if (s->bucle && L > 0.0f) { if (dif > L * 0.5f) dif -= L; if (dif < -L * 0.5f) dif += L; }
        if ((escena && gSaltoEscena) || fabsf(dif) > 0.12f) w3dEngine::W3dSoundSetPos(s->voz, posVoz);
        w3dEngine::W3dSoundSetVolume(s->voz, vol);
        w3dEngine::W3dSoundSetPitch(s->voz, tono);
        w3dEngine::W3dSoundSetPan(s->voz, pan);
        w3dEngine::W3dSoundSetEnvio(s->voz, s->reverb);
        // sin bucle: el recorte del FINAL corta la voz (el archivo seguiria)
        if (!s->bucle && p >= s->recorteIni + L) s->Callar();
    }
    if (s->reverb > 0.0f && s->voz > 0) hayReverb = true;
}

void W3dSonidosCallar() {
    for (std::set<Sonido*>::iterator it = gVivos.begin(); it != gVivos.end(); ++it) { (*it)->Callar(); (*it)->tJuego0 = -1.0; }
}

void W3dSonidosTickEditor() {
    if (AnimEsJuego) return;   // (el juego tiene su tick: W3dSonidosTickJuego)
    static int ultimoFrame = -1000000;
    const bool escena = PlayAnimation && AnimPlayDir > 0 && ActiveAnimKind == 0;
    std::vector<Sonido*> lista; Juntar(SceneCollection, lista);
    std::set<Sonido*> enArbol(lista.begin(), lista.end());
    // los que el undo saco del arbol (o una escena descargada) se callan
    for (std::set<Sonido*>::iterator it = gVivos.begin(); it != gVivos.end(); ++it) {
        if (!enArbol.count(*it)) (*it)->Callar();
        (*it)->tJuego0 = -1.0;
    }
    // EL RELOJ DE LA ESCENA CON SONIDOS = EL DEL AUDIO. El timeline avanza un frame por vuelta del editor: si el editor
    // va a 15 fps (muchas muestras de trazado), la escena iba mas lenta que el sonido, el sonido se corria y se lo
    // volvia a acomodar a cada rato (se escuchaba TRABADO). Ahora el audio no se toca: el timeline SALTEA frames para
    // ir con el (como un video). Solo un salto de VERDAD (mover la linea de tiempo, el loop del rango) reacomoda.
    static bool relojOn = false; static double r0 = 0.0; static float t0 = 0.0f; static int esperado = 0;
    float t = W3dSonidoSegDeFrame((float)CurrentFrame);
    bool hayActivo = false;
    for (size_t i = 0; i < lista.size() && !hayActivo; i++) hayActivo = lista[i]->activo && !lista[i]->archivo.empty();
    if (escena && hayActivo) {
        const double reloj = (w3dEngine::W3dAudioReloj() > 0.0) ? w3dEngine::W3dAudioReloj() : w3dGetTicks() / 1000.0;
        // (lo que hizo AnimTick despues de la ultima vuelta: un frame mas, o la vuelta al Inicio del rango = no es salto)
        const bool normal = relojOn && (CurrentFrame == esperado || CurrentFrame == esperado + 1);
        if (!normal) { relojOn = true; r0 = reloj; t0 = t; gSaltoEscena = true; }
        else {
            gSaltoEscena = false;
            t = t0 + (float)(reloj - r0);
            int f = (int)floorf(W3dSonidoFrameDeSeg(t) + 1e-4f);
            if (f > EndFrame) {   // (el loop del rango: se vuelve al Inicio y se reacomoda desde ahi)
                f = StartFrame; t = W3dSonidoSegDeFrame((float)f); r0 = reloj; t0 = t; gSaltoEscena = true;
            }
            if (f != CurrentFrame) { CurrentFrame = f; extern bool g_redraw; g_redraw = true; }
        }
        esperado = CurrentFrame;
    } else {
        relojOn = false;
        gSaltoEscena = (CurrentFrame - ultimoFrame) > 2 || CurrentFrame < ultimoFrame;
    }
    ultimoFrame = CurrentFrame;
    Oyente oy = OyenteActual(false);
    bool hayReverb = false;
    for (size_t i = 0; i < lista.size(); i++) Actualizar(lista[i], escena, t - lista[i]->inicio, oy, true, hayReverb);
    // la REVERB del mezclador es global: con algun sonido que le manda, una sala mediana; si no, apagada
    if (hayReverb != gReverbPuesta) { w3dEngine::W3dAudioReverb(0.6f, 0.45f, hayReverb ? 1.0f : 0.0f); gReverbPuesta = hayReverb; }
}

void W3dSonidosTickJuego() {
    std::vector<Sonido*> lista; Juntar(SceneCollection, lista);
    std::set<Sonido*> enArbol(lista.begin(), lista.end());
    for (std::set<Sonido*>::iterator it = gVivos.begin(); it != gVivos.end(); ++it) if (!enArbol.count(*it)) (*it)->Callar();
    const double reloj = w3dEngine::W3dAudioReloj();
    Oyente oy = OyenteActual(true);
    bool hayReverb = false;
    for (size_t i = 0; i < lista.size(); i++) {
        Sonido* s = lista[i];
        if (s->tJuego0 < 0.0) s->tJuego0 = reloj;   // (aparecio ahora: arranca desde aca, mas su 'inicio')
        Actualizar(s, true, (float)(reloj - s->tJuego0) - s->inicio, oy, false, hayReverb);
    }
}

// el VOLUMEN / tono / reverb animados (canal AnimSonido del Core)
static void AnimSonidoAplicar(Object* o, int comp, float v) {
    Sonido* s = (Sonido*)o;
    if (comp == AnimX) s->volumen = v < 0.0f ? 0.0f : v;
    else if (comp == AnimY) s->tono = v < 0.05f ? 0.05f : v;
    else if (comp == AnimZ) s->reverb = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

// ============================================================================
//  LUA. Los objetos van como cualquier objeto (buscar("Explosion"), yo, instanciar/destruir un prefab que lo lleva)
//    tocarSonido(obj [, seg])   lo arranca de nuevo (desde 'seg' segundos del tramo)
//    callarSonido(obj)          lo calla (queda apagado hasta tocarSonido o setSonidoObj{activo=true})
//    sonidoObj(obj)             tabla con todo: archivo activo volumen tono reverb paneo inicio recorteIni recorteFin
//                               bucle posicional radio alcance duracion sonando
//    setSonidoObj(obj, {...})   cambia lo que venga en la tabla (archivo = otra fuente)
//    crearSonido(archivo [, padre])  un objeto Sonido nuevo (destruir(obj) lo saca)
// ============================================================================
static Sonido* ArgSonido(lua_State* L, int i) {
    Object* o = W3dScriptParamObjeto((void*)L, i);
    return (o && o->getType() == ObjectType::sonido) ? (Sonido*)o : NULL;
}
static int LTocarSonido(lua_State* L) {
    Sonido* s = ArgSonido(L, 1); if (!s) return 0;
    const float seg = (float)luaL_optnumber(L, 2, 0.0);
    s->Callar(); s->activo = true;
    s->tJuego0 = w3dEngine::W3dAudioReloj() - s->inicio - seg / (s->tono > 0.05f ? s->tono : 0.05f);
    return 0;
}
static int LCallarSonido(lua_State* L) { Sonido* s = ArgSonido(L, 1); if (s) { s->activo = false; s->Callar(); } return 0; }
static void CampoNum(lua_State* L, const char* k, double v) { lua_pushnumber(L, v); lua_setfield(L, -2, k); }
static void CampoBool(lua_State* L, const char* k, bool v) { lua_pushboolean(L, v ? 1 : 0); lua_setfield(L, -2, k); }
static int LSonidoObj(lua_State* L) {
    Sonido* s = ArgSonido(L, 1); if (!s) { lua_pushnil(L); return 1; }
    lua_newtable(L);
    lua_pushstring(L, s->archivo.c_str()); lua_setfield(L, -2, "archivo");
    CampoBool(L, "activo", s->activo); CampoNum(L, "volumen", s->volumen); CampoNum(L, "tono", s->tono);
    CampoNum(L, "reverb", s->reverb); CampoNum(L, "paneo", s->paneo); CampoNum(L, "inicio", s->inicio);
    CampoNum(L, "recorteIni", s->recorteIni); CampoNum(L, "recorteFin", s->recorteFin); CampoBool(L, "bucle", s->bucle);
    CampoBool(L, "posicional", s->posicional); CampoNum(L, "radio", s->radio); CampoNum(L, "alcance", s->alcance);
    CampoNum(L, "duracion", s->DuracionTramo()); CampoBool(L, "sonando", s->voz > 0 && w3dEngine::W3dSoundPos(s->voz) >= 0.0f);
    return 1;
}
static bool LeerNum(lua_State* L, const char* k, float& v) {
    lua_getfield(L, 2, k); const bool hay = lua_isnumber(L, -1) != 0; if (hay) v = (float)lua_tonumber(L, -1); lua_pop(L, 1); return hay;
}
static bool LeerBool(lua_State* L, const char* k, bool& v) {
    lua_getfield(L, 2, k); const bool hay = lua_isboolean(L, -1) != 0; if (hay) v = lua_toboolean(L, -1) != 0; lua_pop(L, 1); return hay;
}
static int LSetSonidoObj(lua_State* L) {
    Sonido* s = ArgSonido(L, 1); if (!s || !lua_istable(L, 2)) return 0;
    lua_getfield(L, 2, "archivo");
    if (lua_isstring(L, -1)) { const std::string a = lua_tostring(L, -1); if (a != s->archivo) { s->Callar(); s->archivo = a; } }
    lua_pop(L, 1);
    const float ri = s->recorteIni, rf = s->recorteFin; const bool bu = s->bucle;
    LeerNum(L, "volumen", s->volumen); LeerNum(L, "tono", s->tono); LeerNum(L, "reverb", s->reverb); LeerNum(L, "paneo", s->paneo);
    LeerNum(L, "inicio", s->inicio); LeerNum(L, "recorteIni", s->recorteIni); LeerNum(L, "recorteFin", s->recorteFin);
    LeerNum(L, "radio", s->radio); LeerNum(L, "alcance", s->alcance);
    LeerBool(L, "bucle", s->bucle); LeerBool(L, "posicional", s->posicional);
    bool act = s->activo; if (LeerBool(L, "activo", act)) { if (act && !s->activo) s->tJuego0 = -1.0; s->activo = act; }
    if (ri != s->recorteIni || rf != s->recorteFin || bu != s->bucle) s->Callar();   // (otra fuente de la voz)
    return 0;
}
static int LCrearSonido(lua_State* L) {
    const char* a = luaL_checkstring(L, 1);
    Object* padre = W3dScriptParamObjeto((void*)L, 2);
    Sonido* s = new Sonido(padre ? padre : SceneCollection);
    s->archivo = a;
    lua_pushlightuserdata(L, (Object*)s);
    return 1;
}
static void RegistrarLua(lua_State* L) {
    lua_pushcfunction(L, LTocarSonido);  lua_setglobal(L, "tocarSonido");
    lua_pushcfunction(L, LCallarSonido); lua_setglobal(L, "callarSonido");
    lua_pushcfunction(L, LSonidoObj);    lua_setglobal(L, "sonidoObj");
    lua_pushcfunction(L, LSetSonidoObj); lua_setglobal(L, "setSonidoObj");
    lua_pushcfunction(L, LCrearSonido);  lua_setglobal(L, "crearSonido");
}

// los ganchos (el Core y BindsJuego no conocen la clase)
extern void (*W3dSonidoLuaHook)(lua_State* L);
extern void (*W3dSonidosJuegoHook)();
namespace {
struct Registro {
    Registro() { g_animSonidoHook = AnimSonidoAplicar; W3dSonidoLuaHook = RegistrarLua; W3dSonidosJuegoHook = W3dSonidosTickJuego; }
} gRegistro;
}

// (harness) la posicion de una voz, en segundos del sonido (< 0 = no suena)
float W3dSoundPosDeVoz(int voz) { return voz > 0 ? w3dEngine::W3dSoundPos(voz) : -1.0f; }

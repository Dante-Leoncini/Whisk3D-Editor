// ============================================================================
//  Sound.cpp — ver Sound.h
// ============================================================================
#include "objects/Sound.h"
#include "objects/Camera.h"            // CameraActive: quien escucha en el juego
#include "objects/RenderColors.h"      // el color de seleccion (como el resto de los objetos)
#include "animation/Animation.h"       // PlayAnimation / CurrentFrame / AnimSound (g_animSoundHook)
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
#include "script/W3dScript.h"          // W3dScriptParamObject

// (audio/W3dAudio.h, declarado a mano como en Properties.cpp: no mete el dir de audio en el include path de todas
//  las plataformas)
namespace w3dEngine {
    class W3dSound;
    struct W3dEnvelope;
    W3dSound* W3dSoundLoadMemory(const void* bytes, size_t len);
    int   W3dSoundPlayEnv(W3dSound* s, float volume, bool loop, float pitch, float pan, double inSeg,
                          const W3dEnvelope* env, float sendReverb);
    void  W3dSoundStopFade(int voice, float fadeSeg);
    int   W3dSoundPlayPitch(W3dSound* s, float volume, bool loop, float pitch);
    void  W3dSoundStopFadeAt(int voice, float fadeSeg, double inSeg);   // (el audio scrubbing)
    void  W3dSoundSetVolume(int voice, float volume);
    void  W3dSoundSetPitch(int voice, float pitch);
    void  W3dSoundSetPan(int voice, float pan);
    void  W3dSoundSetSend(int voice, float sendReverb);
    float W3dSoundPos(int voice);
    void  W3dSoundSetPos(int voice, float seg);
    float W3dSoundDuration(const W3dSound* s);
    W3dSound* W3dSoundCrop(const W3dSound* s, float from, float until);
    void  W3dSoundPeaks(const W3dSound* s, int n, float* peaks);
    double W3dAudioClock();
    void  W3dAudioReverb(float room, float damping, float level);
}
namespace gfx = w3dEngine;
#ifdef W3D_NO_EDITOR
static const bool g_showSounds = false;   // el overlay "Sounds" es del editor: en el juego no se dibuja
#else
extern bool g_showSounds;   // el overlay "Sounds" del editor
#endif
extern int AnimFPS;

namespace {
std::set<Sound*> gLive;                                     // todos (tambien los que el undo saco del arbol)
std::map<std::string, w3dEngine::W3dSound*> gFiles;         // los archivos cargados (por ruta)
std::map<std::string, w3dEngine::W3dSound*> gSegments;      // los tramos recortados para el bucle ("ruta|ini|fin")
std::map<std::string, unsigned int> gWaves;                 // la forma de onda de cada archivo
bool gReverbPlaced = false;
}

bool (*W3dSoundListenerHook)(Vector3& pos, Vector3& right, Vector3& forward) = 0;

float W3dSoundSecFromFrame(float frame) { return (frame - 1.0f) / (float)(AnimFPS > 0 ? AnimFPS : 30); }
float W3dSoundFrameFromSec(float seg) { return 1.0f + seg * (float)(AnimFPS > 0 ? AnimFPS : 30); }

w3dEngine::W3dSound* W3dSoundLoad(const std::string& file) {
    if (file.empty()) return NULL;
    std::map<std::string, w3dEngine::W3dSound*>::iterator it = gFiles.find(file);
    if (it != gFiles.end()) return it->second;
    w3dEngine::W3dSound* s = NULL;
    std::vector<unsigned char> b;
    if (w3dFileSystem::ReadFileBytes(file, b) && !b.empty()) s = w3dEngine::W3dSoundLoadMemory(&b[0], b.size());
    if (!s) w3dLogfW("[sonido] no se pudo cargar '%s'", file.c_str());
    gFiles[file] = s;   // (tambien el NULL: no se reintenta en cada cuadro)
    return s;
}
// el TRAMO recortado como sonido propio (solo hace falta para el BUCLE: el mixer da la vuelta al final de lo que
// tiene, exacto en la muestra). Sin recortes es el archivo
static w3dEngine::W3dSound* Segment(const Sound* o) {
    w3dEngine::W3dSound* base = W3dSoundLoad(o->file);
    if (!base || (o->clipStart <= 0.0f && o->clipEnd <= 0.0f)) return base;
    const float dur = w3dEngine::W3dSoundDuration(base);
    char k[64]; snprintf(k, sizeof k, "|%.5f|%.5f", o->clipStart, o->clipEnd);
    const std::string key = o->file + k;
    std::map<std::string, w3dEngine::W3dSound*>::iterator it = gSegments.find(key);
    if (it != gSegments.end()) return it->second;
    w3dEngine::W3dSound* t = w3dEngine::W3dSoundCrop(base, o->clipStart, dur - o->clipEnd);
    gSegments[key] = t;
    return t;
}
unsigned int W3dSoundWave(const std::string& file) {
    std::map<std::string, unsigned int>::iterator it = gWaves.find(file);
    if (it != gWaves.end()) return it->second;
    unsigned int id = 0;
    w3dEngine::W3dSound* s = W3dSoundLoad(file);
    if (s) {
        const int W = 4096, H = 64;
        std::vector<float> p(W);
        w3dEngine::W3dSoundPeaks(s, W, &p[0]);
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
    gWaves[file] = id;
    return id;
}

// ============================================================================
//  el objeto
// ============================================================================
Sound::Sound(Object* parent, Vector3 pos)
    : Object(parent, "Sound", pos), active(true), volume(1.0f), tone(1.0f), reverb(0.0f), pan(0.0f), start(0.0f),
      clipStart(0.0f), clipEnd(0.0f), loop(false), positional(false), radius(2.0f), range(30.0f),
      voice(0), tGame0(-1.0), waveTex(0) {
    gLive.insert(this);
}
Sound::~Sound() { Silence(); gLive.erase(this); }
void Sound::Silence() { if (voice > 0) w3dEngine::W3dSoundStopFade(voice, 0.02f); voice = 0; }
void Sound::CopyFrom(const Sound* s) {
    file = s->file; active = s->active; volume = s->volume; tone = s->tone; reverb = s->reverb; pan = s->pan;
    start = s->start; clipStart = s->clipStart; clipEnd = s->clipEnd; loop = s->loop;
    positional = s->positional; radius = s->radius; range = s->range;
}
float Sound::DurationFile() { return w3dEngine::W3dSoundDuration(W3dSoundLoad(file)); }
float Sound::DurationSegment() {
    float d = DurationFile() - clipStart - clipEnd;
    return d > 0.0f ? d : 0.0f;
}
float Sound::DurationInScene() { return DurationSegment() / (tone > 0.05f ? tone : 0.05f); }

bool Sound::DrawsSomething() { return g_showSounds; }
// el PARLANTE es un ICONO (render.cpp, como la lampara de las luces); aca, si es 3D, las esferas de radio y alcance
void Sound::RenderObject() {
    if (!g_showSounds) return;
    static std::vector<GLfloat> v;
    v.clear();
    // las ESFERAS: tres circulos por esfera (los tres planos)
    if (positional && select) {   // (solo con el sonido SELECCIONADO: con todos a la vista no se entendia nada)
        const float rr[2] = { radius, range };
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
    GLboolean lightWas = gfx::IsEnabled(gfx::Lighting);
    gfx::Disable(gfx::Lighting);
    gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray);   // (encima del trazado queda su textura puesta)
    gfx::DisableArray(gfx::NormalArray);
    if (select) {
        const float* col = gRenderColors[(this == ActiveObj) ? RC_selActive : RC_selInactive];
        gfx::Color4f(col[0], col[1], col[2], 1.0f);
    } else gfx::Color4f(0.85f, 0.85f, 0.85f, 1.0f);
    if (!v.empty()) { gfx::VertexPointer3f(0, &v[0]); gfx::DrawLines((int)(v.size() / 3)); }
    gfx::EnableArray(gfx::NormalArray);
    if (lightWas) gfx::Enable(gfx::Lighting);
}

// ============================================================================
//  la reproduccion
// ============================================================================
static void Collect(Object* o, std::vector<Sound*>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (h->getType() == ObjectType::sound) out.push_back((Sound*)h);
        Collect(h, out);
    }
}
// visible considerando los padres (ocultar la coleccion calla sus sonidos)
static bool Visible(Object* o) { for (; o; o = o->Parent) if (!o->visible) return false; return true; }

struct Listener { Vector3 pos, right, forward; bool has; };
static Listener CurrentListener(bool game) {
    Listener oy; oy.has = false;
    if (!game && W3dSoundListenerHook && W3dSoundListenerHook(oy.pos, oy.right, oy.forward)) { oy.has = true; return oy; }
    if (CameraActive) {
        Matrix4 W; CameraActive->GetWorldMatrix(W);
        oy.pos   = Vector3(W.m[12], W.m[13], W.m[14]);
        oy.right = Vector3(W.m[0], W.m[1], W.m[2]).Normalized();
        oy.forward = Vector3(-W.m[8], -W.m[9], -W.m[10]).Normalized();   // (la camara mira a su -Z)
        oy.has = true;
    } else if (game && W3dSoundListenerHook && W3dSoundListenerHook(oy.pos, oy.right, oy.forward)) oy.has = true;
    return oy;
}
// cuanto se escucha y de que lado (3D). Hasta 'radio' entero; despues baja hasta 0 en 'alcance'; a los costados
// panea; atras suena mas bajo y mas al medio (el mezclador es estereo)
static void Spatial(const Sound* s, const Listener& oy, float& gain, float& pan) {
    gain = 1.0f; pan = s->pan;
    if (!s->positional) return;
    if (!oy.has) { pan = 0.0f; return; }
    Matrix4 W; s->GetWorldMatrix(W);
    Vector3 d(W.m[12] - oy.pos.x, W.m[13] - oy.pos.y, W.m[14] - oy.pos.z);
    const float dist = d.Length();
    const float r = s->radius > 0.0f ? s->radius : 0.0f, a = s->range > r + 0.01f ? s->range : r + 0.01f;
    if (dist >= a) { gain = 0.0f; pan = 0.0f; return; }
    if (dist > r) { const float k = 1.0f - (dist - r) / (a - r); gain = k * k; }
    if (dist < 1e-4f) { pan = 0.0f; return; }
    d = d * (1.0f / dist);
    const float x = d.x * oy.right.x + d.y * oy.right.y + d.z * oy.right.z;        // derecha +
    const float z = d.x * oy.forward.x + d.y * oy.forward.y + d.z * oy.forward.z;  // adelante +
    pan = x * 0.9f;
    if (z < 0.0f) { gain *= 1.0f + 0.35f * z; pan *= 1.0f + 0.4f * z; }   // atras: mas bajo y mas al medio
}

// UNA pasada: 'local' = segundos desde que arranco (de la escena o del juego). sonar = deberia estar sonando
static bool gJumpScene = false;
static void Update(Sound* s, bool play, float local, const Listener& oy, bool scene, bool& hasReverb) {
    w3dEngine::W3dSound* t = NULL;
    float L = 0.0f;
    if (play && s->active && Visible(s) && local >= 0.0f) {
        L = s->DurationSegment();
        if (L > 0.0f) t = s->loop ? Segment(s) : W3dSoundLoad(s->file);
    }
    const float tone = s->tone < 0.05f ? 0.05f : (s->tone > 8.0f ? 8.0f : s->tone);
    float seg = local * tone;   // segundos DEL TRAMO
    if (!t || (!s->loop && seg >= L)) { s->Silence(); return; }
    float pos = s->loop ? fmodf(seg, L) : seg;
    // con bucle suena el TRAMO (empieza en 0); sin bucle, el archivo entero desde el recorte
    const float posVoice = s->loop ? pos : s->clipStart + pos;
    { char k[96]; snprintf(k, sizeof k, "|%.5f|%.5f|%d", s->clipStart, s->clipEnd, s->loop ? 1 : 0);
      const std::string key = s->file + k;
      if (s->voice > 0 && key != s->voiceSource) s->Silence();   // (otro archivo, otro recorte o el bucle: otra fuente)
      s->voiceSource = key; }
    float g, pan; Spatial(s, oy, g, pan);
    const float vol = (s->volume < 0.0f ? 0.0f : s->volume) * g;
    if (s->voice > 0 && w3dEngine::W3dSoundPos(s->voice) < 0.0f) s->voice = 0;   // (termino o la cortaron)
    if (s->voice <= 0) {
        s->voice = w3dEngine::W3dSoundPlayEnv(t, vol, s->loop, tone, pan, 0.0, NULL, s->reverb);
        if (s->voice > 0) w3dEngine::W3dSoundSetPos(s->voice, posVoice);
    } else {
        // SINCRONIA: si la linea de tiempo salto (o la voz se corrio mas de 0.12 s) se la lleva a donde va
        float p = w3dEngine::W3dSoundPos(s->voice);
        float dif = p - posVoice;
        if (s->loop && L > 0.0f) { if (dif > L * 0.5f) dif -= L; if (dif < -L * 0.5f) dif += L; }
        if ((scene && gJumpScene) || fabsf(dif) > 0.12f) w3dEngine::W3dSoundSetPos(s->voice, posVoice);
        w3dEngine::W3dSoundSetVolume(s->voice, vol);
        w3dEngine::W3dSoundSetPitch(s->voice, tone);
        w3dEngine::W3dSoundSetPan(s->voice, pan);
        w3dEngine::W3dSoundSetSend(s->voice, s->reverb);
        // sin bucle: el recorte del FINAL corta la voz (el archivo seguiria)
        if (!s->loop && p >= s->clipStart + L) s->Silence();
    }
    if (s->reverb > 0.0f && s->voice > 0) hasReverb = true;
}

void W3dSoundsSilence() {
    for (std::set<Sound*>::iterator it = gLive.begin(); it != gLive.end(); ++it) { (*it)->Silence(); (*it)->tGame0 = -1.0; }
}

// AUDIO SCRUBBING (Timeline > Vista): con la animacion QUIETA, cada vez que cambia el frame (las flechas, arrastrar la
// linea de tiempo) suena ese pedacito de cada sonido: un frame de largo (y no menos de 60 ms, para que se entienda).
// Sirve para sincronizar una voz con la boca
bool g_soundScrub = false;
static void ScrubPlay(const std::vector<Sound*>& list, float t) {
    static std::vector<int> voices;
    for (size_t i = 0; i < voices.size(); i++) w3dEngine::W3dSoundStopFade(voices[i], 0.01f);
    voices.clear();
    const double clock = w3dEngine::W3dAudioClock();
    float dur = 1.0f / (float)(AnimFPS > 0 ? AnimFPS : 30);
    if (dur < 0.06f) dur = 0.06f;
    for (size_t i = 0; i < list.size(); i++) {
        Sound* s = list[i];
        if (!s->active || s->file.empty()) continue;
        const float inEsc = s->DurationInScene();
        float local = t - s->start;
        if (local < 0.0f || inEsc <= 0.0f) continue;
        if (local >= inEsc) { if (!s->loop) continue; local = fmodf(local, inEsc); }
        w3dEngine::W3dSound* snd = W3dSoundLoad(s->file);
        if (!snd) continue;
        const float tone = s->tone > 0.05f ? s->tone : 0.05f;
        const int v = w3dEngine::W3dSoundPlayPitch(snd, s->volume, false, tone);
        if (v <= 0) continue;
        w3dEngine::W3dSoundSetPos(v, s->clipStart + local * tone);
        w3dEngine::W3dSoundStopFadeAt(v, 0.01f, (clock > 0.0 ? clock : 0.0) + dur);
        voices.push_back(v);
    }
}

void W3dSoundsTickEditor() {
    if (AnimIsGame) return;   // (el juego tiene su tick: W3dSoundsTickGame)
    static int lastFrame = -1000000;
    const bool scene = PlayAnimation && AnimPlayDir > 0 && ActiveAnimKind == 0;
    std::vector<Sound*> list; Collect(SceneCollection, list);
    std::set<Sound*> inTree(list.begin(), list.end());
    // los que el undo saco del arbol (o una escena descargada) se callan
    for (std::set<Sound*>::iterator it = gLive.begin(); it != gLive.end(); ++it) {
        if (!inTree.count(*it)) (*it)->Silence();
        (*it)->tGame0 = -1.0;
    }
    // EL RELOJ DE LA ESCENA CON SONIDOS = EL DEL AUDIO. El timeline avanza un frame por vuelta del editor: si el editor
    // va a 15 fps (muchas muestras de trazado), la escena iba mas lenta que el sonido, el sonido se corria y se lo
    // volvia a acomodar a cada rato (se escuchaba TRABADO). Ahora el audio no se toca: el timeline SALTEA frames para
    // ir con el (como un video). Solo un salto de VERDAD (mover la linea de tiempo, el loop del rango) reacomoda.
    static bool clockOn = false; static double r0 = 0.0; static float t0 = 0.0f; static int expected = 0;
    float t = W3dSoundSecFromFrame((float)CurrentFrame);
    bool hasActive = false;
    for (size_t i = 0; i < list.size() && !hasActive; i++) hasActive = list[i]->active && !list[i]->file.empty();
    if (scene && hasActive) {
        const double clock = (w3dEngine::W3dAudioClock() > 0.0) ? w3dEngine::W3dAudioClock() : w3dGetTicks() / 1000.0;
        // (lo que hizo AnimTick despues de la ultima vuelta: un frame mas, o la vuelta al Inicio del rango = no es salto)
        const bool normal = clockOn && (CurrentFrame == expected || CurrentFrame == expected + 1);
        if (!normal) { clockOn = true; r0 = clock; t0 = t; gJumpScene = true; }
        else {
            gJumpScene = false;
            t = t0 + (float)(clock - r0);
            int f = (int)floorf(W3dSoundFrameFromSec(t) + 1e-4f);
            if (f > EndFrame) {   // (el loop del rango: se vuelve al Inicio y se reacomoda desde ahi)
                f = StartFrame; t = W3dSoundSecFromFrame((float)f); r0 = clock; t0 = t; gJumpScene = true;
            }
            if (f != CurrentFrame) { CurrentFrame = f; extern bool g_redraw; g_redraw = true; }
        }
        expected = CurrentFrame;
    } else {
        clockOn = false;
        gJumpScene = (CurrentFrame - lastFrame) > 2 || CurrentFrame < lastFrame;
        if (g_soundScrub && !PlayAnimation && CurrentFrame != lastFrame && lastFrame > -1000000) ScrubPlay(list, t);
    }
    lastFrame = CurrentFrame;
    Listener oy = CurrentListener(false);
    bool hasReverb = false;
    for (size_t i = 0; i < list.size(); i++) Update(list[i], scene, t - list[i]->start, oy, true, hasReverb);
    // la REVERB del mezclador es global: con algun sonido que le manda, una sala mediana; si no, apagada
    if (hasReverb != gReverbPlaced) { w3dEngine::W3dAudioReverb(0.6f, 0.45f, hasReverb ? 1.0f : 0.0f); gReverbPlaced = hasReverb; }
}

void W3dSoundsTickGame() {
    std::vector<Sound*> list; Collect(SceneCollection, list);
    std::set<Sound*> inTree(list.begin(), list.end());
    for (std::set<Sound*>::iterator it = gLive.begin(); it != gLive.end(); ++it) if (!inTree.count(*it)) (*it)->Silence();
    const double clock = w3dEngine::W3dAudioClock();
    Listener oy = CurrentListener(true);
    bool hasReverb = false;
    for (size_t i = 0; i < list.size(); i++) {
        Sound* s = list[i];
        if (s->tGame0 < 0.0) s->tGame0 = clock;   // (aparecio ahora: arranca desde aca, mas su 'inicio')
        Update(s, true, (float)(clock - s->tGame0) - s->start, oy, false, hasReverb);
    }
}

// EL AUDIO DE LA ESCENA A UN ARCHIVO (tarjeta Render: "Renderizar audio" y el sonido de los videos): los sonidos como
// si sonara la escena en el cuadro 'frame', con la CAMARA como oyente (la del render). 'primero' = las voces se ponen
// en su lugar. Quien llama mezcla despues lo que dura el cuadro (W3dAudioRecord, con el mixer en pausa)
void W3dSoundsRenderFrame(float frame, bool first) {
    std::vector<Sound*> list; Collect(SceneCollection, list);
    gJumpScene = first;
    const float t = W3dSoundSecFromFrame(frame);
    Listener oy = CurrentListener(true);
    bool hasReverb = false;
    for (size_t i = 0; i < list.size(); i++) Update(list[i], true, t - list[i]->start, oy, true, hasReverb);
    if (hasReverb != gReverbPlaced) { w3dEngine::W3dAudioReverb(0.6f, 0.45f, hasReverb ? 1.0f : 0.0f); gReverbPlaced = hasReverb; }
}
// cuantos sonidos con archivo tiene la escena (sin ninguno, un video no lleva pista de audio)
int W3dSoundsInScene() {
    std::vector<Sound*> list; Collect(SceneCollection, list);
    int n = 0;
    for (size_t i = 0; i < list.size(); i++) if (list[i]->active && !list[i]->file.empty()) n++;
    return n;
}

// el VOLUMEN / tono / reverb animados (canal AnimSound del Core)
static void AnimSoundApply(Object* o, int comp, float v) {
    Sound* s = (Sound*)o;
    if (comp == AnimX) s->volume = v < 0.0f ? 0.0f : v;
    else if (comp == AnimY) s->tone = v < 0.05f ? 0.05f : v;
    else if (comp == AnimZ) s->reverb = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

// ============================================================================
//  LUA. Los objetos van como cualquier objeto (find("Explosion"), yo, instantiate/destroy un prefab que lo lleva)
//    playSound(obj [, seg])   lo arranca de nuevo (desde 'seg' segundos del tramo)
//    muteSound(obj)          lo calla (queda apagado hasta playSound o setSoundObject{activo=true})
//    soundObject(obj)             tabla con todo: archivo activo volumen tono reverb paneo inicio clipStart clipEnd
//                               bucle posicional radio alcance duracion sonando
//    setSoundObject(obj, {...})   cambia lo que venga en la tabla (archivo = otra fuente)
//    createSound(archivo [, padre])  un objeto Sonido nuevo (destroy(obj) lo saca)
// ============================================================================
static Sound* ArgSound(lua_State* L, int i) {
    Object* o = W3dScriptParamObject((void*)L, i);
    return (o && o->getType() == ObjectType::sound) ? (Sound*)o : NULL;
}
static int LTouchSound(lua_State* L) {
    Sound* s = ArgSound(L, 1); if (!s) return 0;
    const float seg = (float)luaL_optnumber(L, 2, 0.0);
    s->Silence(); s->active = true;
    s->tGame0 = w3dEngine::W3dAudioClock() - s->start - seg / (s->tone > 0.05f ? s->tone : 0.05f);
    return 0;
}
static int LSilenceSound(lua_State* L) { Sound* s = ArgSound(L, 1); if (s) { s->active = false; s->Silence(); } return 0; }
static void FieldNum(lua_State* L, const char* k, double v) { lua_pushnumber(L, v); lua_setfield(L, -2, k); }
static void FieldBool(lua_State* L, const char* k, bool v) { lua_pushboolean(L, v ? 1 : 0); lua_setfield(L, -2, k); }
static int LSoundObj(lua_State* L) {
    Sound* s = ArgSound(L, 1); if (!s) { lua_pushnil(L); return 1; }
    lua_newtable(L);
    lua_pushstring(L, s->file.c_str()); lua_setfield(L, -2, "file");
    FieldBool(L, "active", s->active); FieldNum(L, "volume", s->volume); FieldNum(L, "pitch", s->tone);
    FieldNum(L, "reverb", s->reverb); FieldNum(L, "pan", s->pan); FieldNum(L, "start", s->start);
    FieldNum(L, "clipStart", s->clipStart); FieldNum(L, "clipEnd", s->clipEnd); FieldBool(L, "loop", s->loop);
    FieldBool(L, "positional", s->positional); FieldNum(L, "radius", s->radius); FieldNum(L, "range", s->range);
    FieldNum(L, "duration", s->DurationSegment()); FieldBool(L, "playing", s->voice > 0 && w3dEngine::W3dSoundPos(s->voice) >= 0.0f);
    return 1;
}
static bool ReadNum(lua_State* L, const char* k, float& v) {
    lua_getfield(L, 2, k); const bool has = lua_isnumber(L, -1) != 0; if (has) v = (float)lua_tonumber(L, -1); lua_pop(L, 1); return has;
}
static bool ReadBool(lua_State* L, const char* k, bool& v) {
    lua_getfield(L, 2, k); const bool has = lua_isboolean(L, -1) != 0; if (has) v = lua_toboolean(L, -1) != 0; lua_pop(L, 1); return has;
}
static int LSetSoundObj(lua_State* L) {
    Sound* s = ArgSound(L, 1); if (!s || !lua_istable(L, 2)) return 0;
    lua_getfield(L, 2, "file");
    if (lua_isstring(L, -1)) { const std::string a = lua_tostring(L, -1); if (a != s->file) { s->Silence(); s->file = a; } }
    lua_pop(L, 1);
    const float ri = s->clipStart, rf = s->clipEnd; const bool bu = s->loop;
    ReadNum(L, "volume", s->volume); ReadNum(L, "pitch", s->tone); ReadNum(L, "reverb", s->reverb); ReadNum(L, "pan", s->pan);
    ReadNum(L, "start", s->start); ReadNum(L, "clipStart", s->clipStart); ReadNum(L, "clipEnd", s->clipEnd);
    ReadNum(L, "radius", s->radius); ReadNum(L, "range", s->range);
    ReadBool(L, "loop", s->loop); ReadBool(L, "positional", s->positional);
    bool act = s->active; if (ReadBool(L, "active", act)) { if (act && !s->active) s->tGame0 = -1.0; s->active = act; }
    if (ri != s->clipStart || rf != s->clipEnd || bu != s->loop) s->Silence();   // (otra fuente de la voz)
    return 0;
}
static int LCreateSound(lua_State* L) {
    const char* a  = luaL_checkstring(L, 1);
    Object* parent = W3dScriptParamObject((void*)L, 2);
    Sound* s = new Sound(parent ? parent : SceneCollection);
    s->file = a;
    lua_pushlightuserdata(L, (Object*)s);
    return 1;
}
static void RegisterLua(lua_State* L) {
    lua_pushcfunction(L, LTouchSound);  lua_setglobal(L, "playSound");
    lua_pushcfunction(L, LSilenceSound); lua_setglobal(L, "muteSound");
    lua_pushcfunction(L, LSoundObj);    lua_setglobal(L, "soundObject");
    lua_pushcfunction(L, LSetSoundObj); lua_setglobal(L, "setSoundObject");
    lua_pushcfunction(L, LCreateSound);  lua_setglobal(L, "createSound");
}

// los ganchos (el Core y BindsJuego no conocen la clase)
extern void (*W3dSoundLuaHook)(lua_State* L);
extern void (*W3dSoundsGameHook)();
namespace {
struct Registry {
    Registry() { g_animSoundHook = AnimSoundApply; W3dSoundLuaHook = RegisterLua; W3dSoundsGameHook = W3dSoundsTickGame; }
} gRegistry;
}

// (harness) la posicion de una voz, en segundos del sonido (< 0 = no suena)
float W3dSoundVoicePos(int voice) { return voice > 0 ? w3dEngine::W3dSoundPos(voice) : -1.0f; }

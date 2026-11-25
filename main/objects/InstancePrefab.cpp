// ============================================================================
//  InstancePrefab.cpp — ver InstancePrefab.h. Compartido editor/juego (C++03).
//  La GENERACION de los hijos vive en io/Prefabs.cpp (necesita el lector del .w3d).
// ============================================================================
#include "w3dGraphics.h"               // abstraccion de graficos (independencia de OpenGL)
#include "objects/InstancePrefab.h"
#include "objects/RenderColors.h"      // el color de seleccion (el mismo que el resto de los objetos)
#include "io/JsonW3d.h"                // JVal: los campos del .w3d
#include "io/Streaming.h"              // sus pedidos al almacen se sueltan con ella

extern bool g_showEmpty;   // el overlay "Empty" del editor (el juego lo tiene apagado)

InstancePrefab::InstancePrefab(Object* parent, Vector3 pos)
    : Object(parent, "Prefab", pos), generatedVersion(0), notGenerated(false),
      load(W3D_LOAD_ALWAYS), distance(50.0f), streamState(W3D_STREAM_UNLOADED), pinnedStream(0),
      streamPhase(0) {}

// lo que el streaming le pidio al almacen para ella (mallas, animsets, texturas, scripts) se suelta con ella: una
// instancia borrada o destruida jugando no deja recursos retenidos
InstancePrefab::~InstancePrefab() { W3dStreamingForgetInstance(this); }

Object* InstancePrefab::RootGenerated() const {
    return Childrens.empty() ? NULL : Childrens[0];
}

// lo generado se dibuja solo (son hijos de verdad). La instancia misma solo se ve en el editor cuando
// NO genero nada (un prefab que falta): una cruz como la de un vacio, para poder encontrarla y elegirla
void InstancePrefab::RenderObject() {
    if (!Childrens.empty() || !g_showEmpty || !w3dRenderOverlays) return;
    static const float cross[] = {   // (tipos del motor, no de GL: w3dGraphics.h no trae los de GL)
        -0.5f,-0.5f,0,  0.5f,0.5f,0,   -0.5f,0.5f,0,  0.5f,-0.5f,0,   0,0,-0.5f,  0,0,0.5f };
    const bool lightWas = w3dEngine::IsEnabled(w3dEngine::Lighting);
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::DisableArray(w3dEngine::NormalArray);
    if (select) {
        const float* c = gRenderColors[(this == ActiveObj) ? RC_selActive : RC_selInactive];
        w3dEngine::Color4f(c[0], c[1], c[2], 1.0f);
    } else if (notGenerated) {
        w3dEngine::Color4f(0.95f, 0.45f, 0.35f, 1.0f);   // (un prefab que falta: rojizo)
    } else {
        w3dEngine::Color4f(0.45f, 0.65f, 0.95f, 1.0f);   // (DESCARGADA por el streaming: azulada)
    }
    w3dEngine::VertexPointer3f(0, cross);
    w3dEngine::TexCoordPointer2f(12, cross);   // (dummy en los mismos datos: ver Empty::RenderObject)
    w3dEngine::DrawLines(6);
    w3dEngine::EnableArray(w3dEngine::NormalArray);
    if (lightWas) w3dEngine::Enable(w3dEngine::Lighting);
}

// ---- .w3d ------------------------------------------------------------------
static void IpSang(std::string& s, int n) { for (int i = 0; i < n; i++) s += "  "; }
static void IpEsc(std::string& s, const std::string& v) {   // mismo escape que el escritor del .w3d
    s += '"';
    for (size_t i = 0; i < v.size(); i++) {
        const char c = v[i];
        if (c == '"' || c == '\\') { s += '\\'; s += c; }
        else if (c == '\n') s += "\\n";
        else s += c;
    }
    s += '"';
}

// "prefab" siempre; "overrides" solo si hay alguno (una instancia sin tocar sale en una linea). Los
// mapas estan ordenados por clave: el guardado es determinista (mismos bytes al re-guardar)
// ---- LOS CAMPOS que se pueden cambiar en lo generado (ver InstancePrefab.h) ----
static const char* kFieldsInst[W3D_FIELDS_INST] = {
    "pos.x", "pos.y", "pos.z", "rot.x", "rot.y", "rot.z", "esc.x", "esc.y", "esc.z",
    "render", "static", "castsShadow", "receivesShadow", "bouncesLight", "catchesShadows", "inProbes"
};
const char* W3dFieldInstName(int k) { return (k >= 0 && k < W3D_FIELDS_INST) ? kFieldsInst[k] : ""; }
int W3dFieldInstByName(const std::string& n) {
    for (int k = 0; k < W3D_FIELDS_INST; k++) if (n == kFieldsInst[k]) return k;
    return -1;
}
static float* FieldFloat(Object* o, int k) {
    switch (k) {
        case 0: return &o->pos.x; case 1: return &o->pos.y; case 2: return &o->pos.z;
        case 3: return &o->rotEuler.x; case 4: return &o->rotEuler.y; case 5: return &o->rotEuler.z;
        case 6: return &o->scale.x; case 7: return &o->scale.y; case 8: return &o->scale.z;
    }
    return 0;
}
static bool* FieldBool(Object* o, int k) {
    switch (k) {
        case 9: return &o->renderable; case 10: return &o->isStatic; case 11: return &o->projectsShadow;
        case 12: return &o->receivesShadow; case 13: return &o->bouncesLight; case 14: return &o->catchesShadows;
        case 15: return &o->inProbes;
    }
    return 0;
}
float W3dFieldInstRead(Object* o, int k) {
    if (!o) return 0.0f;
    if (float* f = FieldFloat(o, k)) return *f;
    if (bool* b = FieldBool(o, k)) return *b ? 1.0f : 0.0f;
    return 0.0f;
}
int W3dFieldInstByPtr(Object* o, const void* ptr) {
    if (!o || !ptr) return -1;
    for (int k = 0; k < W3D_FIELDS_INST; k++)
        if ((const void*)FieldFloat(o, k) == ptr || (const void*)FieldBool(o, k) == ptr) return k;
    return -1;
}
// pone los campos 'cs' (nombre -> valor) en 'o' (la rotacion por su puerta: el euler manda, ver Object::SetRotEuler)
void W3dFieldsInstSet(Object* o, const std::map<std::string, float>& cs) {
    if (!o) return;
    Vector3 e = o->rotEuler; bool rot = false;
    for (std::map<std::string, float>::const_iterator it = cs.begin(); it != cs.end(); ++it) {
        const int k = W3dFieldInstByName(it->first);
        if (k < 0) continue;
        if (k >= 3 && k <= 5) { (k == 3 ? e.x : k == 4 ? e.y : e.z) = it->second; rot = true; continue; }
        if (float* f = FieldFloat(o, k)) *f = it->second;
        else if (bool* b = FieldBool(o, k)) *b = it->second != 0.0f;
    }
    if (rot) o->SetRotEuler(e);
}

void InstancePrefabWriteFields(std::string& s, int ind, const InstancePrefab* ip) {
    if (!ip) return;
    s += ",\n"; IpSang(s, ind); s += "\"prefab\": "; IpEsc(s, ip->prefab);
    // la CARGA (io/Streaming.h): solo la diferida se escribe (una de siempre sale como antes de que existiera)
    if (ip->load == W3D_LOAD_DISTANCE) {
        s += ",\n"; IpSang(s, ind); s += "\"load\": \"distance\"";
        s += ",\n"; IpSang(s, ind); s += "\"distance\": "; s += JsonNumText(ip->distance);
        if (!ip->target.empty()) { s += ",\n"; IpSang(s, ind); s += "\"target\": "; IpEsc(s, ip->target); }
    }
    if (ip->overProps.empty() && ip->overVisible.empty() && ip->overFields.empty()) return;
    s += ",\n"; IpSang(s, ind); s += "\"overrides\": {";
    bool first = true;
    if (!ip->overProps.empty()) {
        s += " \"properties\": {";
        bool p1 = true;
        for (std::map<std::string, std::string>::const_iterator it = ip->overProps.begin(); it != ip->overProps.end(); ++it) {
            s += p1 ? " " : ", "; p1 = false;
            IpEsc(s, it->first); s += ": "; IpEsc(s, it->second);
        }
        s += " }";
        first = false;
    }
    if (!ip->overVisible.empty()) {
        if (!first) s += ",";
        s += " \"visible\": {";
        bool p1 = true;
        for (std::map<std::string, bool>::const_iterator it = ip->overVisible.begin(); it != ip->overVisible.end(); ++it) {
            s += p1 ? " " : ", "; p1 = false;
            IpEsc(s, it->first); s += ": "; s += it->second ? "true" : "false";
        }
        s += " }";
        first = false;
    }
    // los CAMPOS cambiados en lo generado: { "ruta": { "pos.x": 1.5, "estatico": 0 } }
    if (!ip->overFields.empty()) {
        if (!first) s += ",";
        s += " \"fields\": {";
        bool p1 = true;
        for (std::map<std::string, std::map<std::string, float> >::const_iterator it = ip->overFields.begin(); it != ip->overFields.end(); ++it) {
            s += p1 ? " " : ", "; p1 = false;
            IpEsc(s, it->first); s += ": {";
            bool p2 = true;
            for (std::map<std::string, float>::const_iterator c = it->second.begin(); c != it->second.end(); ++c) {
                s += p2 ? " " : ", "; p2 = false;
                IpEsc(s, c->first); s += ": "; s += JsonNumText(c->second);
            }
            s += " }";
        }
        s += " }";
    }
    s += " }";
}

void InstancePrefabReadFields(JVal* j, InstancePrefab* ip) {
    if (!j || !ip) return;
    ip->prefab = JS(j, "prefab", "");
    // la CARGA (ausente = siempre; una distancia negativa o absurda se acota)
    ip->load = (JS(j, "load", "always") == "distance") ? W3D_LOAD_DISTANCE : W3D_LOAD_ALWAYS;
    ip->distance = JF(j, "distance", 50.0f);
    if (!(ip->distance >= 0.0f)) ip->distance = 0.0f;   // (tambien un NaN)
    if (ip->distance > 1.0e7f) ip->distance = 1.0e7f;
    ip->target = JS(j, "target", "");
    ip->overProps.clear();
    ip->overVisible.clear();
    ip->overFields.clear();
    JVal* ov = JChild(j, "overrides", 4);
    if (!ov) return;
    JVal* pr = JChild(ov, "properties", 4);
    if (pr)
        for (std::map<std::string, JVal*>::iterator it = pr->obj.begin(); it != pr->obj.end(); ++it) {
            if (!it->second) continue;
            // (un valor numerico o bool escrito a mano tambien vale: se guarda como texto, igual que las refs)
            if (it->second->type == 2) ip->overProps[it->first] = it->second->str;
            else if (it->second->type == 1) ip->overProps[it->first] = JsonNumText((float)it->second->num);
            else if (it->second->type == 3) ip->overProps[it->first] = it->second->b ? "true" : "false";
        }
    JVal* vi = JChild(ov, "visible", 4);
    if (vi)
        for (std::map<std::string, JVal*>::iterator it = vi->obj.begin(); it != vi->obj.end(); ++it)
            if (it->second && it->second->type == 3) ip->overVisible[it->first] = it->second->b;
    JVal* ca = JChild(ov, "fields", 4);
    if (ca)
        for (std::map<std::string, JVal*>::iterator it = ca->obj.begin(); it != ca->obj.end(); ++it) {
            if (!it->second || it->second->type != 4) continue;
            for (std::map<std::string, JVal*>::iterator c = it->second->obj.begin(); c != it->second->obj.end(); ++c) {
                if (!c->second || W3dFieldInstByName(c->first) < 0) continue;
                if (c->second->type == 1) ip->overFields[it->first][c->first] = (float)c->second->num;
                else if (c->second->type == 3) ip->overFields[it->first][c->first] = c->second->b ? 1.0f : 0.0f;
            }
        }
}

// ============================================================================
//  ProxyW3d.cpp — ver ProxyW3d.h. Compartido editor/juego (C++03).
//  La GENERACION de los hijos es la de las instancias (io/Prefabs.cpp).
// ============================================================================
#include "objects/ProxyW3d.h"
#include "io/Libraries.h"
#include "io/JsonW3d.h"

ProxyW3d::ProxyW3d(Object* parent, Vector3 pos) : InstancePrefab(parent, pos) {
    name = "Proxy";
}

// la libreria de la clave (tambien de una PARCIAL: "lib:<libreria>/prefab/", libreria elegida sin elemento)
static std::string LibByKey(const std::string& key) {
    if (key.size() < 5 || key.compare(0, 4, "lib:") != 0) return std::string();
    const size_t b = key.find('/', 4);
    return (b == std::string::npos) ? key.substr(4) : key.substr(4, b - 4);
}
std::string ProxyW3d::Lib() const { return LibByKey(prefab); }
std::string ProxyW3d::Element() const {
    std::string e;
    return W3dLibsByKey(prefab, NULL, NULL, &e) ? e : std::string();
}
int ProxyW3d::ElementType() const {
    int t = W3D_LIB_PREFAB;
    return W3dLibsByKey(prefab, NULL, &t, NULL) ? t : W3D_LIB_PREFAB;
}
std::string W3dProxyKey(const std::string& lib, int type, const std::string& element) {
    std::string c = W3dLibsKey(lib, type, element);
    // (una libreria sin elemento todavia: una clave PARCIAL que no genera nada pero recuerda la libreria, asi la
    //  tarjeta la muestra elegida y el guardado la escribe)
    if (c.empty() && !lib.empty()) c = "lib:" + lib + "/prefab/";
    return c;
}
void ProxyW3d::Set(const std::string& lib, int type, const std::string& element) {
    prefab = W3dProxyKey(lib, type, element);
}

// ---- .w3d ------------------------------------------------------------------
static void PxSang(std::string& s, int n) { for (int i = 0; i < n; i++) s += "  "; }
static void PxEsc(std::string& s, const std::string& v) {   // mismo escape que el escritor del .w3d
    s += '"';
    for (size_t i = 0; i < v.size(); i++) {
        const char c = v[i];
        if (c == '"' || c == '\\') { s += '\\'; s += c; }
        else if (c == '\n') s += "\\n";
        else s += c;
    }
    s += '"';
}

// "libreria" y "elemento" siempre; "elementoTipo" solo para una escena (el default es un prefab); los overrides
// como los de una instancia (el mismo escritor: InstancePrefabWriteFields escribe "prefab" + "overrides", y
// aca el "prefab" no va: la clave es de la memoria)
void ProxyW3dWriteFields(std::string& s, int ind, const ProxyW3d* px) {
    if (!px) return;
    std::string lib, elem;
    int type = W3D_LIB_PREFAB;
    if (!W3dLibsByKey(px->prefab, &lib, &type, &elem)) lib = LibByKey(px->prefab);   // (clave parcial)
    s += ",\n"; PxSang(s, ind); s += "\"lib\": "; PxEsc(s, lib);
    s += ",\n"; PxSang(s, ind); s += "\"element\": "; PxEsc(s, elem);
    if (type == W3D_LIB_SCENE) { s += ",\n"; PxSang(s, ind); s += "\"elementType\": \"scene\""; }
    // los overrides: el escritor de la instancia sin su linea "prefab"
    std::string ov;
    InstancePrefabWriteFields(ov, ind, px);
    const size_t p = ov.find(",\n", 2);   // (la primera linea es la del "prefab")
    if (p != std::string::npos) s += ov.substr(p);
}

void ProxyW3dReadFields(JVal* j, ProxyW3d* px) {
    if (!j || !px) return;
    InstancePrefabReadFields(j, px);   // los overrides (su "prefab" no existe en un proxy: queda vacio)
    const std::string type = JS(j, "elementType", "prefab");
    // un proxy ADENTRO del contenido de una libreria nombra una libreria de SU registro (una de adentro): la que le
    // toca en memoria (la del nivel con ese archivo, o una montada oculta)
    std::string lib = JS(j, "lib", "");
    if (!W3dLibContext().empty()) lib = W3dLibsNested(W3dLibContext(), lib);
    px->Set(lib, type == "scene" ? W3D_LIB_SCENE : W3D_LIB_PREFAB, JS(j, "element", ""));
}

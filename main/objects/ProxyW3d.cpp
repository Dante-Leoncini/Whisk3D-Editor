// ============================================================================
//  ProxyW3d.cpp — ver ProxyW3d.h. Compartido editor/juego (C++03).
//  La GENERACION de los hijos es la de las instancias (io/Prefabs.cpp).
// ============================================================================
#include "objects/ProxyW3d.h"
#include "io/Librerias.h"
#include "io/JsonW3d.h"

ProxyW3d::ProxyW3d(Object* parent, Vector3 pos) : InstanciaPrefab(parent, pos) {
    name = "Proxy";
}

// la libreria de la clave (tambien de una PARCIAL: "lib:<libreria>/prefab/", libreria elegida sin elemento)
static std::string LibDeClave(const std::string& clave) {
    if (clave.size() < 5 || clave.compare(0, 4, "lib:") != 0) return std::string();
    const size_t b = clave.find('/', 4);
    return (b == std::string::npos) ? clave.substr(4) : clave.substr(4, b - 4);
}
std::string ProxyW3d::Libreria() const { return LibDeClave(prefab); }
std::string ProxyW3d::Elemento() const {
    std::string e;
    return W3dLibsDeClave(prefab, NULL, NULL, &e) ? e : std::string();
}
int ProxyW3d::TipoElemento() const {
    int t = W3D_LIB_PREFAB;
    return W3dLibsDeClave(prefab, NULL, &t, NULL) ? t : W3D_LIB_PREFAB;
}
std::string W3dProxyClave(const std::string& libreria, int tipo, const std::string& elemento) {
    std::string c = W3dLibsClave(libreria, tipo, elemento);
    // (una libreria sin elemento todavia: una clave PARCIAL que no genera nada pero recuerda la libreria, asi la
    //  tarjeta la muestra elegida y el guardado la escribe)
    if (c.empty() && !libreria.empty()) c = "lib:" + libreria + "/prefab/";
    return c;
}
void ProxyW3d::Fijar(const std::string& libreria, int tipo, const std::string& elemento) {
    prefab = W3dProxyClave(libreria, tipo, elemento);
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
// como los de una instancia (el mismo escritor: InstanciaPrefabEscribirCampos escribe "prefab" + "overrides", y
// aca el "prefab" no va: la clave es de la memoria)
void ProxyW3dEscribirCampos(std::string& s, int ind, const ProxyW3d* px) {
    if (!px) return;
    std::string lib, elem;
    int tipo = W3D_LIB_PREFAB;
    if (!W3dLibsDeClave(px->prefab, &lib, &tipo, &elem)) lib = LibDeClave(px->prefab);   // (clave parcial)
    s += ",\n"; PxSang(s, ind); s += "\"libreria\": "; PxEsc(s, lib);
    s += ",\n"; PxSang(s, ind); s += "\"elemento\": "; PxEsc(s, elem);
    if (tipo == W3D_LIB_ESCENA) { s += ",\n"; PxSang(s, ind); s += "\"elementoTipo\": \"escena\""; }
    // los overrides: el escritor de la instancia sin su linea "prefab"
    std::string ov;
    InstanciaPrefabEscribirCampos(ov, ind, px);
    const size_t p = ov.find(",\n", 2);   // (la primera linea es la del "prefab")
    if (p != std::string::npos) s += ov.substr(p);
}

void ProxyW3dLeerCampos(JVal* j, ProxyW3d* px) {
    if (!j || !px) return;
    InstanciaPrefabLeerCampos(j, px);   // los overrides (su "prefab" no existe en un proxy: queda vacio)
    const std::string tipo = JS(j, "elementoTipo", "prefab");
    // un proxy ADENTRO del contenido de una libreria nombra una libreria de SU registro (una de adentro): la que le
    // toca en memoria (la del nivel con ese archivo, o una montada oculta)
    std::string lib = JS(j, "libreria", "");
    if (!W3dLibContexto().empty()) lib = W3dLibsAnidada(W3dLibContexto(), lib);
    px->Fijar(lib, tipo == "escena" ? W3D_LIB_ESCENA : W3D_LIB_PREFAB, JS(j, "elemento", ""));
}

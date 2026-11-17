// ============================================================================
//  W3dLayouts.cpp - los layouts guardados (ver el .h). El archivo:
//      @Nombre del layout
//      Layout {
//          ...
//      }
//      @Otro
//      ...
//  Una linea que empieza con '@' abre un layout; lo demas, hasta el proximo '@', es su texto.
// ============================================================================
#include "W3dLayouts.h"
#include "w3dFilesystem.h"
#include "W3dLang.h"
#include <stdio.h>

struct LayoutGuardado { std::string nombre, texto; };
static std::vector<LayoutGuardado> gLista;
static bool gLeido = false;
static std::string gRuta;   // "" = layouts.txt de la carpeta del usuario

// el que viene con el programa: el visor 3D arriba y la linea de tiempo abajo
static const char* kAnimacion =
    "Layout {\n"
    "    ViewportColumn {\n"
    "        Viewport3D\n"
    "        Timeline\n"
    "        Split: 0.72\n"
    "    }\n"
    "}";

static std::string Ruta() {
    if (!gRuta.empty()) return gRuta;
    const std::string& dir = w3dFileSystem::GetUserDataDir();
    return dir.empty() ? std::string("layouts.txt") : dir + "/layouts.txt";
}

static void Escribir() {
    FILE* f = fopen(Ruta().c_str(), "wb");
    if (!f) return;
    for (size_t i = 0; i < gLista.size(); i++) fprintf(f, "@%s\n%s\n", gLista[i].nombre.c_str(), gLista[i].texto.c_str());
    fclose(f);
#ifdef __EMSCRIPTEN__
    { extern void W3dWebPersistir(); W3dWebPersistir(); }   // la carpeta del usuario es IndexedDB: que quede al recargar
#endif
}

static void Leer() {
    if (gLeido) return;
    gLeido = true;
    gLista.clear();
    FILE* f = fopen(Ruta().c_str(), "rb");
    if (!f) {
        // PRIMERA VEZ: el de animacion, que viene con el programa (se puede borrar: borrado, layouts.txt queda sin el)
        LayoutGuardado a; a.nombre = T("Animation"); a.texto = kAnimacion;
        gLista.push_back(a);
        Escribir();
        return;
    }
    char linea[1024];
    while (fgets(linea, sizeof(linea), f)) {
        std::string s = linea;
        while (!s.empty() && (s[s.size() - 1] == '\n' || s[s.size() - 1] == '\r')) s.erase(s.size() - 1);
        if (!s.empty() && s[0] == '@') {
            LayoutGuardado l; l.nombre = s.substr(1);
            gLista.push_back(l);
        } else if (!gLista.empty()) {
            std::string& t = gLista.back().texto;
            if (!t.empty()) t += "\n";
            t += s;
        }
    }
    fclose(f);
    // (las lineas en blanco del final de cada uno)
    for (size_t i = 0; i < gLista.size(); i++) {
        std::string& t = gLista[i].texto;
        while (!t.empty() && (t[t.size() - 1] == '\n' || t[t.size() - 1] == ' ')) t.erase(t.size() - 1);
    }
}

std::vector<std::string> W3dLayoutsNombres() {
    Leer();
    std::vector<std::string> r;
    for (size_t i = 0; i < gLista.size(); i++) r.push_back(gLista[i].nombre);
    return r;
}

std::string W3dLayoutTexto(const std::string& nombre) {
    Leer();
    for (size_t i = 0; i < gLista.size(); i++) if (gLista[i].nombre == nombre) return gLista[i].texto;
    return std::string();
}

bool W3dLayoutGuardar(const std::string& nombre, const std::string& texto) {
    // (sin '@' adelante ni saltos de linea: rompian el archivo)
    std::string n;
    for (size_t i = 0; i < nombre.size(); i++) if (nombre[i] != '\n' && nombre[i] != '\r') n += nombre[i];
    while (!n.empty() && (n[0] == ' ' || n[0] == '@')) n.erase(0, 1);
    while (!n.empty() && n[n.size() - 1] == ' ') n.erase(n.size() - 1);
    if (n.empty() || texto.empty()) return false;
    Leer();
    for (size_t i = 0; i < gLista.size(); i++)
        if (gLista[i].nombre == n) { gLista[i].texto = texto; Escribir(); return true; }
    LayoutGuardado l; l.nombre = n; l.texto = texto;
    gLista.push_back(l);
    Escribir();
    return true;
}

bool W3dLayoutBorrar(const std::string& nombre) {
    Leer();
    for (size_t i = 0; i < gLista.size(); i++)
        if (gLista[i].nombre == nombre) { gLista.erase(gLista.begin() + (long)i); Escribir(); return true; }
    return false;
}

void W3dLayoutsRutaFijar(const std::string& ruta) {
    gRuta = ruta;
    gLeido = false;
    gLista.clear();
}

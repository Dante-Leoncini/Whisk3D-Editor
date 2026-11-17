// ============================================================================
//  W3dRecientes.cpp - los ultimos proyectos abiertos o guardados (ver el .h)
// ============================================================================
#include "W3dRecientes.h"
#include "w3dFilesystem.h"
#include <stdio.h>

static const size_t kGuardados = 10;   // se recuerdan 10 (si alguno se borra del disco, igual quedan 5 para mostrar)
static std::vector<std::string> gLista;
static bool gLeida = false;
static std::string gRuta;              // "" = recientes.txt de la carpeta del usuario

static std::string Ruta() {
    if (!gRuta.empty()) return gRuta;
    const std::string& dir = w3dFileSystem::GetUserDataDir();
    return dir.empty() ? std::string("recientes.txt") : dir + "/recientes.txt";
}

static void Leer() {
    if (gLeida) return;
    gLeida = true;
    gLista.clear();
    FILE* f = fopen(Ruta().c_str(), "rb");
    if (!f) return;
    char linea[2048];
    while (fgets(linea, sizeof(linea), f) && gLista.size() < kGuardados) {
        std::string s = linea;
        while (!s.empty() && (s[s.size() - 1] == '\n' || s[s.size() - 1] == '\r')) s.erase(s.size() - 1);
        if (!s.empty()) gLista.push_back(s);
    }
    fclose(f);
}

static void Escribir() {
    FILE* f = fopen(Ruta().c_str(), "wb");
    if (!f) return;
    for (size_t i = 0; i < gLista.size(); i++) fprintf(f, "%s\n", gLista[i].c_str());
    fclose(f);
#ifdef __EMSCRIPTEN__
    { extern void W3dWebPersistir(); W3dWebPersistir(); }   // la carpeta del usuario es IndexedDB: que quede al recargar
#endif
}

void W3dRecientesAgregar(const std::string& ruta) {
    if (ruta.empty()) return;
    Leer();
    if (!gLista.empty() && gLista[0] == ruta) return;   // ya era la primera: no se toca el disco
    for (size_t i = 0; i < gLista.size(); i++)
        if (gLista[i] == ruta) { gLista.erase(gLista.begin() + (long)i); break; }
    gLista.insert(gLista.begin(), ruta);
    if (gLista.size() > kGuardados) gLista.resize(kGuardados);
    Escribir();
}

std::vector<std::string> W3dRecientes(int maximo) {
    Leer();
    std::vector<std::string> r;
    for (size_t i = 0; i < gLista.size() && (int)r.size() < maximo; i++)
        if (w3dFileSystem::FileExists(gLista[i])) r.push_back(gLista[i]);
    return r;
}

void W3dRecientesRutaFijar(const std::string& ruta) {
    gRuta = ruta;
    gLeida = false;
    gLista.clear();
}

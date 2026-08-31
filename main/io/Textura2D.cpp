// ============================================================================
//  Textura2D.cpp — ver Textura2D.h.
// ============================================================================
#include "io/Textura2D.h"
#include "w3dTexture.h"
#include "w3dFilesystem.h"
#include "objects/Textures.h"   // puente al cache 3D (atlas unico: un solo id GL)
#include <stdio.h>
#include <stdlib.h>
#include <map>

namespace gfx = w3dEngine;

struct Tex2DEntrada { unsigned id; int w, h; bool prestada; };
static std::map<std::string, Tex2DEntrada> gTexturas;

// Purga las entradas PRESTADAS del cache 3D (ver Tex3DPorRuta): al cerrar/abrir
// proyecto el 3D puede liberar su textura (TexturaSoltar -> DeleteTexture) y el
// id prestado quedaria colgando. Se llama al ARRANCAR AbrirW3D.
void Textura2DPurgarPrestadas() {
    std::map<std::string, Tex2DEntrada>::iterator it = gTexturas.begin();
    while (it != gTexturas.end()) {
        if (it->second.prestada) {
            std::map<std::string, Tex2DEntrada>::iterator b = it; ++it;
            gTexturas.erase(b);
        } else ++it;
    }
}

// PUENTE AL CACHE 3D (atlas unico): si la MISMA imagen ya la subio un material
// 3D (Textures[] / TexturaCache por ruta), la UI usa ESE id de GL en vez de
// subir una copia. Con el HUD y el 3D compartiendo texturas/atlas.png, el
// juego entero queda en UNA textura -> un solo glBindTexture por frame.
// Las claves pueden diferir (una resuelta absoluta, la otra relativa): se
// normaliza separador/case y se compara por SUFIJO con borde de '/'.
static bool Ruta2DEquivale(std::string x, std::string y) {
    for (size_t i = 0; i < x.size(); i++) {
        if (x[i] == '\\') x[i] = '/';
        else if (x[i] >= 'A' && x[i] <= 'Z') x[i] = (char)(x[i] + 32);
    }
    for (size_t i = 0; i < y.size(); i++) {
        if (y[i] == '\\') y[i] = '/';
        else if (y[i] >= 'A' && y[i] <= 'Z') y[i] = (char)(y[i] + 32);
    }
    if (x == y) return true;
    if (x.size() > y.size()) x.swap(y);          // x = la corta
    if (x.empty() || y.size() <= x.size()) return false;
    return y.compare(y.size() - x.size(), x.size(), x) == 0
           && y[y.size() - x.size() - 1] == '/';
}

static const Texture* Tex3DPorRuta(const std::string& ruta) {
    for (size_t i = 0; i < Textures.size(); i++) {
        Texture* t = Textures[i];
        if (t && t->iID && t->ancho > 0 && Ruta2DEquivale(t->path, ruta))
            return t;
    }
    return 0;
}

// PUENTE WEBP: stb no decodifica webp (el arte de los juegos RQ es todo webp). En el
// editor de PC lo convierte ffmpeg a un png cacheado en /tmp, una vez por sesion; el
// .w3dui guarda la ruta ORIGINAL (el runtime del juego decodifica webp nativo).
std::string Textura2DRutaDecodificable(const std::string& ruta) {
    size_t n = ruta.size();
    if (n < 6) return ruta;
    std::string ext = ruta.substr(n - 5);
    for (size_t i = 0; i < ext.size(); i++)
        if (ext[i] >= 'A' && ext[i] <= 'Z') ext[i] += 32;
    if (ext != ".webp") return ruta;
    unsigned hh = 5381;
    for (size_t i = 0; i < n; i++) hh = hh * 33u + (unsigned char)ruta[i];
    char png[512];
    snprintf(png, sizeof(png), "/tmp/whisk3d-webp-%08x.png", hh);
    if (!w3dFileSystem::FileExists(png)) {
        char cmd[1200];
        snprintf(cmd, sizeof(cmd), "ffmpeg -y -v error -i \"%s\" \"%s\" 2>/dev/null",
                 ruta.c_str(), png);
        if (system(cmd) != 0) return ruta;   // sin ffmpeg: que falle el decode normal
    }
    return std::string(png);
}

void Textura2DListar(std::vector<std::string>& rutas) {
    for (std::map<std::string, Tex2DEntrada>::const_iterator it = gTexturas.begin();
         it != gTexturas.end(); ++it)
        if (it->second.id) rutas.push_back(it->first);
}

unsigned Textura2DObtener(const std::string& ruta, int* w, int* h) {
    if (ruta.empty()) return 0;
    std::map<std::string, Tex2DEntrada>::iterator it = gTexturas.find(ruta);
    if (it == gTexturas.end()) {
        Tex2DEntrada e; e.id = 0; e.w = 0; e.h = 0; e.prestada = false;
        // PRIMERO el cache 3D: la misma ruta ya subida por un material (el
        // atlas unico) se comparte -- ni decode ni segunda copia en GL.
        const Texture* t3 = Tex3DPorRuta(ruta);
        if (t3) {
            e.id = t3->iID; e.w = t3->ancho; e.h = t3->alto; e.prestada = true;
        } else {
            // SIN mipmaps (UploadRGBA, no LoadTexture): la UI 2D casi nunca minifica y los
            // mips promediaban los slice9 chicos (un 5x5 mostraba el color mezclado); ademas
            // asi el filtrado por elemento (TexFilter) manda de verdad.
            unsigned char* rgba = NULL;
            if (gfx::DecodeImage(Textura2DRutaDecodificable(ruta).c_str(), &rgba, &e.w, &e.h) && rgba) {
                e.id = gfx::UploadRGBA(rgba, e.w, e.h, true, false);   // UI 2D sin mips (slice9)
                gfx::FreeImage(rgba);
            }
        }
        gTexturas[ruta] = e;   // se cachea aunque falle (no reintentar por frame)
        it = gTexturas.find(ruta);
    }
    if (w) *w = it->second.w;
    if (h) *h = it->second.h;
    return it->second.id;
}

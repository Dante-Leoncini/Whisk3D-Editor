// ============================================================================
//  Textura2D.cpp — ver Textura2D.h.
// ============================================================================
#include "io/Textura2D.h"
#include "w3dTexture.h"
#include "w3dFilesystem.h"
#include <stdio.h>
#include <stdlib.h>
#include <map>

namespace gfx = w3dEngine;

struct Tex2DEntrada { unsigned id; int w, h; };
static std::map<std::string, Tex2DEntrada> gTexturas;

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

unsigned Textura2DObtener(const std::string& ruta, int* w, int* h) {
    if (ruta.empty()) return 0;
    std::map<std::string, Tex2DEntrada>::iterator it = gTexturas.find(ruta);
    if (it == gTexturas.end()) {
        Tex2DEntrada e; e.id = 0; e.w = 0; e.h = 0;
        // SIN mipmaps (UploadRGBA, no LoadTexture): la UI 2D casi nunca minifica y los
        // mips promediaban los slice9 chicos (un 5x5 mostraba el color mezclado); ademas
        // asi el filtrado por elemento (TexFilter) manda de verdad.
        unsigned char* rgba = NULL;
        if (gfx::DecodeImage(Textura2DRutaDecodificable(ruta).c_str(), &rgba, &e.w, &e.h) && rgba) {
            e.id = gfx::UploadRGBA(rgba, e.w, e.h, true);
            gfx::FreeImage(rgba);
        }
        gTexturas[ruta] = e;   // se cachea aunque falle (no reintentar por frame)
        it = gTexturas.find(ruta);
    }
    if (w) *w = it->second.w;
    if (h) *h = it->second.h;
    return it->second.id;
}

// ============================================================================
//  Textura2D.cpp — ver Textura2D.h.
// ============================================================================
#include "io/Textura2D.h"
#include "w3dTexture.h"
#include <map>

namespace gfx = w3dEngine;

struct Tex2DEntrada { unsigned id; int w, h; };
static std::map<std::string, Tex2DEntrada> gTexturas;

unsigned Textura2DObtener(const std::string& ruta, int* w, int* h) {
    if (ruta.empty()) return 0;
    std::map<std::string, Tex2DEntrada>::iterator it = gTexturas.find(ruta);
    if (it == gTexturas.end()) {
        Tex2DEntrada e; e.id = 0; e.w = 0; e.h = 0;
        // SIN mipmaps (UploadRGBA, no LoadTexture): la UI 2D casi nunca minifica y los
        // mips promediaban los slice9 chicos (un 5x5 mostraba el color mezclado); ademas
        // asi el filtrado por elemento (TexFilter) manda de verdad.
        unsigned char* rgba = NULL;
        if (gfx::DecodeImage(ruta.c_str(), &rgba, &e.w, &e.h) && rgba) {
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

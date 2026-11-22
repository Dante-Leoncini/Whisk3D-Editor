// ============================================================================
//  SkinAtlas.cpp — ver SkinAtlas.h.
// ============================================================================
#include "io/SkinAtlas.h"
#include "WhiskUI/draw/W3dAtlasPacker.h"   // el empaquetador generico (tambien para juegos)
#include "w3dTexture.h"
#include "w3dFilesystem.h"
#include "w3dlog.h"
#include <vector>

namespace gfx = w3dEngine;

bool SkinAtlasBuild(const std::string& skinDir, unsigned* outTex,
                    int* outW, int* outH, IconRect rects[ICON_TOTAL]) {
    // el modo atlas es OPT-IN por skin: existe la carpeta de iconos individuales?
    if (!w3dFileSystem::FileExists(skinDir + "atlas/icons/" +
                                   IconName(0) + std::string(".png"))) return false;

    unsigned char* source = NULL; int fw = 0, fh = 0;
    if (!gfx::DecodeImage((skinDir + "font.png").c_str(), &source, &fw, &fh)) return false;

    // items: font.png FIJO en el (0,0) -asi las coordenadas de siempre de borde/cards/
    // scroll/glifos siguen validas- + cada icono individual que exista
    std::vector<W3dAtlasItem> items;
    std::vector<int> fromThatIcon;   // items[k] -> indice del icono (el 0 es font.png: -1)
    W3dAtlasItem base; base.rgba = source; base.w = fw; base.h = fh;
    base.fixed = true; base.x = 0; base.y = 0;
    items.push_back(base); fromThatIcon.push_back(-1);
    for (size_t i = 0; i < ICON_TOTAL; i++) {
        rects[i].x = -1;   // -1 = sin png propio: usa el arte legacy dentro de font.png
        std::string path = skinDir + "atlas/icons/" + IconName((int)i) + ".png";
        if (!w3dFileSystem::FileExists(path)) continue;
        W3dAtlasItem it; it.rgba = NULL; it.w = 0; it.h = 0; it.fixed = false; it.x = it.y = 0;
        unsigned char* px = NULL;
        if (gfx::DecodeImage(path.c_str(), &px, &it.w, &it.h) && px) {
            it.rgba = px;
            items.push_back(it); fromThatIcon.push_back((int)i);
        }
    }

    std::vector<unsigned char> canvas;
    int W = 0, H = 0;
    bool ok = W3dAtlasPack(&items[0], (int)items.size(), 2, canvas, &W, &H);
    if (ok) {
        for (size_t k = 1; k < items.size(); k++) {
            int i = fromThatIcon[k];
            rects[i].x = items[k].x; rects[i].y = items[k].y;
            rects[i].w = items[k].w; rects[i].h = items[k].h;
        }
        *outTex = gfx::UploadRGBA(&canvas[0], W, H, true);   // LINEAR: como el font.png de siempre
        *outW = W; *outH = H;
        w3dLogf("SkinAtlas: %d x %d armado (font.png + iconos individuales)", W, H);
    } else {
        w3dLogE("SkinAtlas: los iconos no entraron ni en el canvas mas grande");
    }

    for (size_t k = 0; k < items.size(); k++)
        if (items[k].rgba) gfx::FreeImage((unsigned char*)items[k].rgba);
    return ok && *outTex != 0;
}

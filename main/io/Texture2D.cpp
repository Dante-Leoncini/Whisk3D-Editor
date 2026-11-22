// ============================================================================
//  Texture2D.cpp — ver Texture2D.h.
// ============================================================================
#include "io/Texture2D.h"
#include "w3dTexture.h"
#include "w3dFilesystem.h"
#include "objects/Textures.h"   // puente al cache 3D (atlas unico: un solo id GL)
#include "render/SceneRender.h" // las capturas de pantalla ("pantalla:<nombre>")
#include <stdio.h>
#include <stdlib.h>
#include <map>

namespace gfx = w3dEngine;

struct Tex2DEntry { unsigned id; int w, h; bool borrowed; };
static std::map<std::string, Tex2DEntry> gTextures;

// Purga las entradas PRESTADAS del cache 3D (ver Tex3DByPath): al cerrar/abrir
// proyecto el 3D puede liberar su textura (TextureRelease -> DeleteTexture) y el
// id prestado quedaria colgando. Se llama al ARRANCAR OpenW3D.
void Texture2DPurgeBorrowed() {
    std::map<std::string, Tex2DEntry>::iterator it = gTextures.begin();
    while (it != gTextures.end()) {
        if (it->second.borrowed) {
            std::map<std::string, Tex2DEntry>::iterator b = it; ++it;
            gTextures.erase(b);
        } else ++it;
    }
}

// Cierre del proyecto (ver Texture2D.h). Lo llama RestartScene, con el arbol ya
// destruido: ninguna Image2D/Slice9/particula queda para pedir un id viejo.
void Texture2DFreeAll() {
    for (std::map<std::string, Tex2DEntry>::iterator it = gTextures.begin(); it != gTextures.end(); ++it)
        if (!it->second.borrowed && it->second.id) gfx::DeleteTexture(it->second.id);
    gTextures.clear();
}

// PUENTE AL CACHE 3D (atlas unico): si la MISMA imagen ya la subio un material
// 3D (Textures[] / TexturaCache por ruta), la UI usa ESE id de GL en vez de
// subir una copia. Con el HUD y el 3D compartiendo textures/atlas.png, el
// juego entero queda en UNA textura -> un solo glBindTexture por frame.
// Las claves pueden diferir (una resuelta absoluta, la otra relativa): se
// normaliza separador/case y se compara por SUFIJO con borde de '/'.
static bool Path2DEquals(std::string x, std::string y) {
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

static const Texture* Tex3DByPath(const std::string& path) {
    for (size_t i = 0; i < Textures.size(); i++) {
        Texture* t = Textures[i];
        if (t && t->iID && t->width > 0 && Path2DEquals(t->path, path))
            return t;
    }
    return 0;
}

// PUENTE WEBP: stb no decodifica webp (el arte de los juegos RQ es todo webp). En el
// editor de PC lo convierte ffmpeg a un png cacheado en /tmp, una vez por sesion; el
// .w3dui guarda la ruta ORIGINAL (el runtime del juego decodifica webp nativo).
std::string Texture2DDecodablePath(const std::string& path) {
    size_t n = path.size();
    if (n < 6) return path;
    std::string ext = path.substr(n - 5);
    for (size_t i = 0; i < ext.size(); i++)
        if (ext[i] >= 'A' && ext[i] <= 'Z') ext[i] += 32;
    if (ext != ".webp") return path;
    unsigned hh = 5381;
    for (size_t i = 0; i < n; i++) hh = hh * 33u + (unsigned char)path[i];
    char png[512];
    snprintf(png, sizeof(png), "/tmp/whisk3d-webp-%08x.png", hh);
    if (!w3dFileSystem::FileExists(png)) {
        char cmd[1200];
        snprintf(cmd, sizeof(cmd), "ffmpeg -y -v error -i \"%s\" \"%s\" 2>/dev/null",
                 path.c_str(), png);
        if (system(cmd) != 0) return path;   // sin ffmpeg: que falle el decode normal
    }
    return std::string(png);
}

void Texture2DList(std::vector<std::string>& paths) {
    for (std::map<std::string, Tex2DEntry>::const_iterator it = gTextures.begin();
         it != gTextures.end(); ++it)
        if (it->second.id) paths.push_back(it->first);
}

unsigned Texture2DGet(const std::string& path, int* w, int* h) {
    if (path.empty()) return 0;
    // una CAPTURA DE PANTALLA (SceneRender.h): la textura se busca cada vez, sin cache (existe recien cuando el juego
    // copia la pantalla y crece si la pantalla cambia); hasta entonces, ninguna
    if (W3dIsCapturePath(path)) {
        const Texture* t = W3dCaptureTexture(path, false);
        if (w) *w = t ? t->width : 0;
        if (h) *h = t ? t->height : 0;
        return t ? t->iID : 0;
    }
    std::map<std::string, Tex2DEntry>::iterator it = gTextures.find(path);
    if (it == gTextures.end()) {
        Tex2DEntry e; e.id = 0; e.w = 0; e.h = 0; e.borrowed = false;
        // PRIMERO el cache 3D: la misma ruta ya subida por un material (el
        // atlas unico) se comparte -- ni decode ni segunda copia en GL.
        const Texture* t3 = Tex3DByPath(path);
        if (t3) {
            e.id = t3->iID; e.w = t3->width; e.h = t3->height; e.borrowed = true;
        } else {
            // SIN mipmaps (UploadRGBA, no LoadTexture): la UI 2D casi nunca minifica y los
            // mips promediaban los slice9 chicos (un 5x5 mostraba el color mezclado); ademas
            // asi el filtrado por elemento (TexFilter) manda de verdad.
            unsigned char* rgba = NULL;
            if (gfx::DecodeImage(Texture2DDecodablePath(path).c_str(), &rgba, &e.w, &e.h) && rgba) {
                // forcePOT=true: en el N95 una textura NPOT se rechaza y el billboard sale SIN
                // textura (los atlas de fuego 960x24 / 1440x48 son NPOT). El camino de material ya
                // remuestrea a POT; este (particulas / imagenes 2D) se lo saltaba. No-op en PC/Android/Web.
#ifdef W3D_SYMBIAN
                // el proyecto pidio sus texturas a la MITAD en el telefono (TexturesHalfFrom): sube la mitad, pero el
                // tamano que se informa (e.w, e.h) sigue siendo el de la imagen: la UI mide con el
                int sw = e.w, sh = e.h;
                const int half = gfx::TexturesHalfFrom();
                if (half > 0 && (sw > half || sh > half)) {
                    int mw = 0, mh = 0;
                    unsigned char* m = gfx::ReduceHalfRGBA(rgba, sw, sh, mw, mh);
                    if (m) { gfx::FreeImage(rgba); rgba = m; sw = mw; sh = mh; }
                }
                e.id = gfx::UploadRGBA(rgba, sw, sh, true, false, true, true);   // UI 2D sin mips (slice9), POT en N95, 16 bits si entra
#else
                e.id = gfx::UploadRGBA(rgba, e.w, e.h, true, false, true, true);   // UI 2D sin mips (slice9), POT en N95, 16 bits si entra
#endif
                gfx::FreeImage(rgba);
            }
        }
        gTextures[path] = e;   // se cachea aunque falle (no reintentar por frame)
        it = gTextures.find(path);
    }
    if (w) *w = it->second.w;
    if (h) *h = it->second.h;
    return it->second.id;
}

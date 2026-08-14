// ============================================================================
//  Video2DCache.cpp — ver Video2DCache.h.
// ============================================================================
#include "io/Video2DCache.h"
#include "w3dTexture.h"
#include "w3dFilesystem.h"
#include "w3dlog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <vector>
#include <string>

#ifndef _WIN32
#include <time.h>
unsigned W3dMsAhora() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}
#else
#include <windows.h>
unsigned W3dMsAhora() { return (unsigned)GetTickCount(); }
#endif

namespace gfx = w3dEngine;

static std::map<std::string, VideoPreview*> gVideos;

// hash simple de la ruta (nombre de la carpeta de cache)
static unsigned Hash(const std::string& s) {
    unsigned h = 5381;
    for (size_t i = 0; i < s.size(); i++) h = h * 33u + (unsigned char)s[i];
    return h;
}

static bool TerminaEn(const std::string& s, const char* suf) {
    size_t n = strlen(suf);
    if (s.size() < n) return false;
    for (size_t i = 0; i < n; i++) {
        char a = s[s.size() - n + i], b = suf[i];
        if (a >= 'A' && a <= 'Z') a += 32;
        if (a != b) return false;
    }
    return true;
}

// un frame del MEDIO del video como png (miniatura): el primero suele ser negro o
// transparente (los festejos entran con fade), el del medio muestra al personaje
std::string Video2DFramePng(const std::string& ruta) {
    if (ruta.empty()) return std::string();
    char png[512];
    snprintf(png, sizeof(png), "/tmp/whisk3d-vthumb-%08x.png", Hash(ruta));
    if (!w3dFileSystem::FileExists(png)) {
        float dur = 0.0f;
        {
            char cmd[1024];
            snprintf(cmd, sizeof(cmd),
                     "ffprobe -v error -show_entries format=duration -of csv=p=0 \"%s\" 2>/dev/null",
                     ruta.c_str());
            FILE* p = popen(cmd, "r");
            if (p) { if (fscanf(p, "%f", &dur) != 1) dur = 0.0f; pclose(p); }
        }
        float ss = (dur > 1.0f) ? dur * 0.5f : 0.0f;
        const char* dec = TerminaEn(ruta, ".webm") ? "-c:v libvpx-vp9 " : "";
        char cmd[1400];
        snprintf(cmd, sizeof(cmd),
                 "ffmpeg -y -v error -ss %.2f %s-i \"%s\" -frames:v 1 "
                 "-vf \"scale='min(96,iw)':-2\" \"%s\" 2>/dev/null",
                 ss, dec, ruta.c_str(), png);
        if (system(cmd) != 0) return std::string();
    }
    return w3dFileSystem::FileExists(png) ? std::string(png) : std::string();
}

const VideoPreview* Video2DPreview(const std::string& ruta) {
    if (ruta.empty()) return NULL;
    std::map<std::string, VideoPreview*>::iterator it = gVideos.find(ruta);
    if (it != gVideos.end()) return it->second;

    VideoPreview* v = new VideoPreview();
    v->frames = NULL; v->numFrames = 0; v->w = 0; v->h = 0;
    v->anchoReal = 0; v->altoReal = 0; v->fps = 12.0f;
    gVideos[ruta] = v;   // se cachea aunque falle (no reintentar por frame)

    // el tamano REAL, el FPS NATIVO y la DURACION del video (ffprobe); sin ffprobe
    // seguimos igual con defaults
    float dur = 0.0f, fpsNat = 0.0f;
    {
        char cmd[1024];
        snprintf(cmd, sizeof(cmd),
                 "ffprobe -v error -select_streams v:0 -show_entries stream=width,height,r_frame_rate "
                 "-show_entries format=duration -of csv=p=0 \"%s\" 2>/dev/null", ruta.c_str());
        FILE* p = popen(cmd, "r");
        if (p) {
            int fn = 0, fd = 0;
            if (fscanf(p, "%d,%d,%d/%d", &v->anchoReal, &v->altoReal, &fn, &fd) < 2) { v->anchoReal = 0; v->altoReal = 0; }
            if (fn > 0 && fd > 0) fpsNat = (float)fn / (float)fd;
            if (fscanf(p, "%f", &dur) != 1) dur = 0.0f;
            pclose(p);
        }
    }

    // fps de la preview: el NATIVO del archivo (extraer a otro fps SALTEA frames y la
    // reproduccion se ve trabada), con techo 30 para no reventar la VRAM; si el video
    // es largo se baja para que el loop entero entre en MAXF frames
    const int MAXF = 180;
    float fps = (fpsNat > 1.0f) ? fpsNat : 24.0f;
    if (fps > 30.0f) fps = 30.0f;
    if (dur > 0.5f && dur * fps > (float)MAXF) fps = (float)MAXF / dur;
    if (fps < 6.0f) fps = 6.0f;
    v->fps = fps;

    // extraer los frames al cache de /tmp (ancho max 480) UNA vez; la carpeta lleva
    // version para no reusar caches viejos extraidos a otro fps
    char dir[512];
    snprintf(dir, sizeof(dir), "/tmp/whisk3d-video-%08x-v3", Hash(ruta));
    char primero[600];
    snprintf(primero, sizeof(primero), "%s/f001.png", dir);
    if (!w3dFileSystem::FileExists(primero)) {
        char cmd[1400];
        // los .webm se decodifican con libvpx-vp9: es el que trae el CANAL ALPHA
        const char* dec = TerminaEn(ruta, ".webm") ? "-c:v libvpx-vp9 " : "";
        snprintf(cmd, sizeof(cmd),
                 "mkdir -p \"%s\" && ffmpeg -y -v error %s-i \"%s\" "
                 "-vf \"fps=%.3f,scale='min(480,iw)':-2\" -frames:v %d \"%s/f%%03d.png\" 2>/dev/null",
                 dir, dec, ruta.c_str(), fps, MAXF, dir);
        if (system(cmd) != 0)
            w3dLogfW("Video2D: ffmpeg fallo con %s (queda el placeholder)", ruta.c_str());
    }

    // subir los frames que haya
    std::vector<unsigned> texs;
    for (int i = 1; i <= MAXF; i++) {
        char f[600];
        snprintf(f, sizeof(f), "%s/f%03d.png", dir, i);
        if (!w3dFileSystem::FileExists(f)) break;
        unsigned char* rgba = NULL; int w = 0, h = 0;
        if (!gfx::DecodeImage(f, &rgba, &w, &h) || !rgba) break;
        unsigned id = gfx::UploadRGBA(rgba, w, h, true);
        gfx::FreeImage(rgba);
        if (!id) break;
        v->w = w; v->h = h;
        texs.push_back(id);
    }
    if (!texs.empty()) {
        v->numFrames = (int)texs.size();
        v->frames = new unsigned[texs.size()];
        for (size_t i = 0; i < texs.size(); i++) v->frames[i] = texs[i];
        if (!v->anchoReal) { v->anchoReal = v->w; v->altoReal = v->h; }
        w3dLogf("Video2D: %s -> %d frames de preview", ruta.c_str(), v->numFrames);
    }
    return v;
}

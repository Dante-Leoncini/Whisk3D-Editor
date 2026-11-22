// ============================================================================
//  W3dRecent.cpp - los ultimos proyectos abiertos o guardados (ver el .h)
// ============================================================================
#include "W3dRecent.h"
#include "w3dFilesystem.h"
#include <stdio.h>

static const size_t kSaved = 10;   // se recuerdan 10 (si alguno se borra del disco, igual quedan 5 para mostrar)
static std::vector<std::string> gList;
static bool gRead = false;
static std::string gPath;              // "" = recent.txt de la carpeta del usuario

static std::string Path() {
    if (!gPath.empty()) return gPath;
    const std::string& dir = w3dFileSystem::GetUserDataDir();
    return dir.empty() ? std::string("recent.txt") : dir + "/recent.txt";
}

// LA MISMA RUTA escrita de otra forma ("E:\\whisk3d\\a.w3d" y "e:/whisk3d/a.w3d"): en Symbian y Windows son el mismo
// archivo. Sin esto un proyecto quedaba dos veces en la lista
static std::string Key(const std::string& r) {
    std::string k = r;
    for (size_t i = 0; i < k.size(); i++) {
        if (k[i] == '\\') k[i] = '/';
#if defined(_WIN32) || defined(W3D_SYMBIAN)
        if (k[i] >= 'A' && k[i] <= 'Z') k[i] = (char)(k[i] - 'A' + 'a');
#endif
    }
    while (k.size() > 1 && k[k.size() - 1] == '/') k.erase(k.size() - 1);
    return k;
}
static bool Already(const std::vector<std::string>& l, const std::string& r) {
    const std::string k = Key(r);
    for (size_t i = 0; i < l.size(); i++) if (Key(l[i]) == k) return true;
    return false;
}

static void Read() {
    if (gRead) return;
    gRead = true;
    gList.clear();
    FILE* f = fopen(Path().c_str(), "rb");
    if (!f) return;
    char line[2048];
    while (fgets(line, sizeof(line), f) && gList.size() < kSaved) {
        std::string s = line;
        while (!s.empty() && (s[s.size() - 1] == '\n' || s[s.size() - 1] == '\r')) s.erase(s.size() - 1);
        if (!s.empty() && !Already(gList, s)) gList.push_back(s);   // (los repetidos de antes, afuera)
    }
    fclose(f);
}

static void Write() {
    FILE* f = fopen(Path().c_str(), "wb");
    if (!f) return;
    for (size_t i = 0; i < gList.size(); i++) fprintf(f, "%s\n", gList[i].c_str());
    fclose(f);
#ifdef __EMSCRIPTEN__
    { extern void W3dWebPersist(); W3dWebPersist(); }   // la carpeta del usuario es IndexedDB: que quede al recargar
#endif
}

void W3dRecentAdd(const std::string& path) {
    if (path.empty()) return;
    Read();
    if (!gList.empty() && gList[0] == path) return;   // ya era la primera: no se toca el disco
    const std::string k = Key(path);
    for (size_t i = 0; i < gList.size(); )
        if (Key(gList[i]) == k) gList.erase(gList.begin() + (long)i); else i++;
    gList.insert(gList.begin(), path);
    if (gList.size() > kSaved) gList.resize(kSaved);
    Write();
}

std::vector<std::string> W3dRecent(int max) {
    Read();
    std::vector<std::string> r;
    for (size_t i = 0; i < gList.size() && (int)r.size() < max; i++)
        if (!Already(r, gList[i]) && w3dFileSystem::FileExists(gList[i])) r.push_back(gList[i]);
    return r;
}

void W3dRecentPathSet(const std::string& path) {
    gPath = path;
    gRead = false;
    gList.clear();
}

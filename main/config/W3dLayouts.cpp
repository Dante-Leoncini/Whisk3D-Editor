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

struct LayoutSaved { std::string name, text; };
static std::vector<LayoutSaved> gList;
static bool gRead = false;
static std::string gPath;   // "" = layouts.txt de la carpeta del usuario

// el que viene con el programa: el visor 3D arriba y la linea de tiempo abajo
static const char* kAnimation =
    "Layout {\n"
    "    ViewportColumn {\n"
    "        Viewport3D\n"
    "        Timeline\n"
    "        Split: 0.72\n"
    "    }\n"
    "}";

static std::string Path() {
    if (!gPath.empty()) return gPath;
    const std::string& dir = w3dFileSystem::GetUserDataDir();
    return dir.empty() ? std::string("layouts.txt") : dir + "/layouts.txt";
}

static void Write() {
    FILE* f = fopen(Path().c_str(), "wb");
    if (!f) return;
    for (size_t i = 0; i < gList.size(); i++) fprintf(f, "@%s\n%s\n", gList[i].name.c_str(), gList[i].text.c_str());
    fclose(f);
#ifdef __EMSCRIPTEN__
    { extern void W3dWebPersist(); W3dWebPersist(); }   // la carpeta del usuario es IndexedDB: que quede al recargar
#endif
}

static void Read() {
    if (gRead) return;
    gRead = true;
    gList.clear();
    FILE* f = fopen(Path().c_str(), "rb");
    if (!f) {
        // PRIMERA VEZ: el de animacion, que viene con el programa (se puede borrar: borrado, layouts.txt queda sin el)
        LayoutSaved a; a.name = T("Animation"); a.text = kAnimation;
        gList.push_back(a);
        Write();
        return;
    }
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        std::string s = line;
        while (!s.empty() && (s[s.size() - 1] == '\n' || s[s.size() - 1] == '\r')) s.erase(s.size() - 1);
        if (!s.empty() && s[0] == '@') {
            LayoutSaved l; l.name = s.substr(1);
            gList.push_back(l);
        } else if (!gList.empty()) {
            std::string& t = gList.back().text;
            if (!t.empty()) t += "\n";
            t += s;
        }
    }
    fclose(f);
    // (las lineas en blanco del final de cada uno)
    for (size_t i = 0; i < gList.size(); i++) {
        std::string& t = gList[i].text;
        while (!t.empty() && (t[t.size() - 1] == '\n' || t[t.size() - 1] == ' ')) t.erase(t.size() - 1);
    }
}

std::vector<std::string> W3dLayoutsNames() {
    Read();
    std::vector<std::string> r;
    for (size_t i = 0; i < gList.size(); i++) r.push_back(gList[i].name);
    return r;
}

std::string W3dLayoutText(const std::string& name) {
    Read();
    for (size_t i = 0; i < gList.size(); i++) if (gList[i].name == name) return gList[i].text;
    return std::string();
}

bool W3dLayoutSave(const std::string& name, const std::string& text) {
    // (sin '@' adelante ni saltos de linea: rompian el archivo)
    std::string n;
    for (size_t i = 0; i < name.size(); i++) if (name[i] != '\n' && name[i] != '\r') n += name[i];
    while (!n.empty() && (n[0] == ' ' || n[0] == '@')) n.erase(0, 1);
    while (!n.empty() && n[n.size() - 1] == ' ') n.erase(n.size() - 1);
    if (n.empty() || text.empty()) return false;
    Read();
    for (size_t i = 0; i < gList.size(); i++)
        if (gList[i].name == n) { gList[i].text = text; Write(); return true; }
    LayoutSaved l; l.name = n; l.text = text;
    gList.push_back(l);
    Write();
    return true;
}

bool W3dLayoutDelete(const std::string& name) {
    Read();
    for (size_t i = 0; i < gList.size(); i++)
        if (gList[i].name == name) { gList.erase(gList.begin() + (long)i); Write(); return true; }
    return false;
}

void W3dLayoutsPathSet(const std::string& path) {
    gPath = path;
    gRead = false;
    gList.clear();
}

// ============================================================================
//  W3dNetPermissions.cpp — ver W3dNetPermissions.h.
//
//  network.txt (en la carpeta del usuario), una cosa por renglon, separadas por un tabulador:
//      dispositivo<TAB>El nombre de este equipo
//      permitir<TAB>/ruta/absoluta/del/proyecto.w3d
//      negar<TAB>/ruta/absoluta/de/otro.w3d
//  (un archivo aparte del config.start: aquel lee una palabra por clave y esto tiene nombres y rutas con espacios)
// ============================================================================
#include "config/W3dNetPermissions.h"
#include "config/W3dLang.h"
#include "net/W3dNet.h"
#include "w3dFilesystem.h"
#include "objects/Objects.h"
#include "script/W3dScript.h"
#include "W3dRoots.h"
#include "ViewPorts/PopUp/ConfirmPopup.h"
#include "w3dlog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <vector>
#if defined(_WIN32)
    #include <stdlib.h>   // _fullpath
#elif !defined(W3D_SYMBIAN)
    #include <limits.h>
#endif

extern bool PlayAnimation;
extern bool g_redraw;
extern bool g_w3dChangesNoDialog;   // el arnes / MCP: sin carteles
extern bool g_gameMode;             // Symbian: el juego empaquetado (game\game.w3d)

extern bool g_netAskInTests;

namespace {

bool gLoaded = false;
std::string gDevice;
std::map<std::string, bool> gPermissions;   // ruta normalizada -> permitido
std::string gPath;                        // el proyecto abierto (normalizada)
bool gAsking = false;
bool gPlayOnRespond = false;

std::string File() { return w3dFileSystem::GetUserDataDir() + "/network.txt"; }

// la ruta absoluta (dos caminos al mismo archivo son el mismo permiso)
std::string NormalPath(const std::string& path) {
    if (path.empty()) return path;
#if defined(_WIN32)
    char b[1024];
    if (_fullpath(b, path.c_str(), sizeof(b))) return b;
#elif !defined(W3D_SYMBIAN)
    char b[PATH_MAX];
    if (realpath(path.c_str(), b)) return b;
#endif
    return path;
}

void Load() {
    if (gLoaded) return;
    gLoaded = true;
    FILE* f = fopen(File().c_str(), "rb");
    if (f) {
        char line[2048];
        while (fgets(line, sizeof(line), f)) {
            size_t n = strlen(line);
            while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
            char* tab = strchr(line, '\t');
            if (!tab) continue;
            *tab = 0;
            const std::string key = line, value = tab + 1;
            if (key == "device") gDevice = value;
            else if (key == "allow") gPermissions[value] = true;
            else if (key == "deny") gPermissions[value] = false;
        }
        fclose(f);
    }
    w3dNet::SetDeviceName(gDevice);
}

void Save() {
    FILE* f = fopen(File().c_str(), "wb");
    if (!f) { w3dLogfW("red: no se pudo escribir %s", File().c_str()); return; }
    if (!gDevice.empty()) fprintf(f, "dispositivo\t%s\n", gDevice.c_str());
    for (std::map<std::string, bool>::iterator it = gPermissions.begin(); it != gPermissions.end(); ++it)
        fprintf(f, "%s\t%s\n", it->second ? "allow" : "deny", it->first.c_str());
    fclose(f);
}

// alguno de sus scripts sale a la red (hostea, busca o se une): una palabra entera, no "miredBuscar"
bool TextUsesNet(const std::vector<unsigned char>& src) {
    static const char* const kNames[] = { "netHost", "netFind", "netJoin" };
    const std::string s(src.begin(), src.end());
    for (int k = 0; k < 3; k++) {
        const std::string n = kNames[k];
        for (size_t p = s.find(n); p != std::string::npos; p = s.find(n, p + 1)) {
            const char prev = p ? s[p - 1] : ' ';
            const char after = p + n.size() < s.size() ? s[p + n.size()] : ' ';
            const bool letter = (prev == '_' || (prev >= 'a' && prev <= 'z') || (prev >= 'A' && prev <= 'Z') ||
                                (prev >= '0' && prev <= '9'));
            const bool still = (after == '_' || (after >= 'a' && after <= 'z') ||
                                (after >= 'A' && after <= 'Z') || (after >= '0' && after <= '9'));
            if (!letter && !still) return true;
        }
    }
    return false;
}

bool ObjectUsesNet(Object* o, std::map<std::string, bool>& seen) {
    if (!o) return false;
    if (o->scriptData) {
        for (size_t i = 0; i < o->scriptData->scripts.size(); i++) {
            const std::string& r = o->scriptData->scripts[i].path;
            if (r.empty() || seen.count(r)) continue;
            std::vector<unsigned char> src;
            seen[r] = w3dFileSystem::ReadFileBytes(r, src) && TextUsesNet(src);
            if (seen[r]) return true;
        }
    }
    for (size_t i = 0; i < o->Childrens.size(); i++)
        if (ObjectUsesNet(o->Childrens[i], seen)) return true;
    return false;
}

// los scripts del proyecto abierto (todas las raices cargadas)
bool ProjectUsesNet() {
    std::map<std::string, bool> seen;
    if (ObjectUsesNet(SceneCollection, seen)) return true;
    std::vector<Object*> roots;
    W3dRootsInOrder(roots);
    for (size_t i = 0; i < roots.size(); i++)
        if (ObjectUsesNet(roots[i], seen)) return true;
    return false;
}

void Respond(bool yes) {
    gAsking = false;
    if (!gPath.empty()) {
        gPermissions[gPath] = yes;
        Save();
    }
    w3dNet::SetAllowed(yes);
    w3dLogf("red: %s %s", yes ? "permitida para" : "negada para", gPath.c_str());
    // lo que se estaba por hacer (abrir jugando, darle Play), sigue: sin red, el juego se entera por redDisponible()
    if (gPlayOnRespond) PlayAnimation = true;
    gPlayOnRespond = false;
    g_redraw = true;
}
void RespondIf() { Respond(true); }
void RespondNo() { Respond(false); }

void Ask() {
    if (gAsking) return;
    gAsking = true;
    if (!confirmPopup) confirmPopup = new ConfirmPopup();
    std::string name = gPath;
    const size_t bar = name.find_last_of("/\\");
    if (bar != std::string::npos) name = name.substr(bar + 1);
    confirmPopup->Ask(std::string(T("This project uses the network (LAN, Wi-Fi or the Internet) to play with "
                                            "others. Allow it?")) + "\n" + name,
                              T("Allow"), T("Don't allow"), RespondIf, RespondNo);
    g_redraw = true;
}

// decide sin preguntar (lo guardado, el arnes, el juego empaquetado) o pregunta. true = ya esta decidido
bool Decide() {
    Load();
    if (!ProjectUsesNet()) { w3dNet::SetAllowed(false); return true; }
    std::map<std::string, bool>::iterator it = gPermissions.find(gPath);
    if (it != gPermissions.end()) { w3dNet::SetAllowed(it->second); return true; }
    if ((g_w3dChangesNoDialog && !g_netAskInTests) || g_gameMode) { w3dNet::SetAllowed(true); return true; }
    w3dNet::SetAllowed(false);
    return false;
}

} // namespace

bool g_netAskInTests = false;

void NetForget(const std::string& path) {
    Load();
    gPermissions.erase(NormalPath(path));
    Save();
}

void NetOnOpenProject(const std::string& path) {
    gPath = NormalPath(path);
    gAsking = false;
    if (Decide()) return;
    // (abria jugando: espera la respuesta)
    gPlayOnRespond = PlayAnimation;
    PlayAnimation = false;
    Ask();
}

bool NetCanPlay() {
    if (gAsking) { PlayAnimation = false; return false; }
    if (Decide()) return true;
    gPlayOnRespond = true;
    PlayAnimation = false;
    Ask();
    return false;
}

bool NetPermissionProject() {
    Load();
    std::map<std::string, bool>::iterator it = gPermissions.find(gPath);
    return it != gPermissions.end() && it->second;
}

void NetSetPermissionProject(bool yes) {
    Load();
    if (gPath.empty()) return;
    gPermissions[gPath] = yes;
    Save();
    w3dNet::SetAllowed(yes);
}

std::string NetDevice() {
    Load();
    return w3dNet::DeviceName();
}

void NetSetDevice(const std::string& name) {
    Load();
    std::string n = name;
    // (sin tabuladores ni saltos: es un renglon de network.txt y viaja en un paquete)
    for (size_t i = 0; i < n.size(); i++) if (n[i] == '\t' || n[i] == '\n' || n[i] == '\r') n[i] = ' ';
    if (n.size() > 40) n.resize(40);
    if (n == gDevice) return;
    gDevice = n;
    w3dNet::SetDeviceName(n);
    Save();
}

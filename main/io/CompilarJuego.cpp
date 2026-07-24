// ============================================================================
//  CompilarJuego.cpp — ver CompilarJuego.h.
// ============================================================================
#include "io/CompilarJuego.h"
#include "io/UI2DFormato.h"
#include "objects/Objects.h"
#include "objects/UI.h"
#include "objects/Imagen2D.h"
#include "objects/Slice9.h"
#include "objects/Boton2D.h"
#include "objects/Video2D.h"
#include "objects/Texto2D.h"
#include "script/W3dScript.h"
#include "ViewPorts/Notificaciones.h"
#include "w3dFilesystem.h"
#include "w3dlog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

// juntar las RUTAS de todo lo que el arbol referencia (scripts y assets)
static void Recolectar(Object* o, std::vector<std::string>* scripts,
                       std::vector<std::string>* assets) {
    if (!o) return;
    if (o->scriptDatos)
        for (size_t i = 0; i < o->scriptDatos->scripts.size(); i++)
            if (!o->scriptDatos->scripts[i].ruta.empty())
                scripts->push_back(o->scriptDatos->scripts[i].ruta);
    ObjectType t = o->getType();
    if (t == ObjectType::imagen2d) assets->push_back(((Imagen2D*)o)->textura);
    if (t == ObjectType::slice9)   assets->push_back(((Slice9*)o)->textura);
    if (t == ObjectType::video2d)  assets->push_back(((Video2D*)o)->video);
    if (t == ObjectType::boton2d) {
        assets->push_back(((Boton2D*)o)->texturaFondo);
        assets->push_back(((Boton2D*)o)->icono);
    }
    if (t == ObjectType::texto2d)  assets->push_back(((Texto2D*)o)->fuente);
    for (size_t i = 0; i < o->Childrens.size(); i++)
        Recolectar(o->Childrens[i], scripts, assets);
}

static std::string Carpeta(const std::string& ruta) {
    size_t s = ruta.find_last_of("/\\");
    return (s == std::string::npos) ? std::string(".") : ruta.substr(0, s);
}
static std::string Base(const std::string& ruta) {
    size_t s = ruta.find_last_of("/\\");
    return (s == std::string::npos) ? ruta : ruta.substr(s + 1);
}

// escribe una entrada del paquete OFUSCADA (XOR rotante con la clave del formato).
// No es criptografia seria: es para que los datos del juego distribuido no queden
// en texto plano; los ORIGINALES legibles siguen en el proyecto.
static const char* kClave = "Whisk3D-w3dgame";
static bool PackEntrada(FILE* f, const std::string& nombre, const std::string& rutaSrc) {
    std::vector<unsigned char> datos;
    if (!w3dFileSystem::ReadFileBytes(rutaSrc, datos)) return false;
    unsigned nl = (unsigned)nombre.size(), dl = (unsigned)datos.size();
    fwrite(&nl, 4, 1, f); fwrite(nombre.c_str(), 1, nl, f);
    fwrite(&dl, 4, 1, f);
    size_t kn = strlen(kClave);
    for (size_t i = 0; i < datos.size(); i++) datos[i] ^= (unsigned char)(kClave[i % kn] + (i & 0xff));
    if (dl) fwrite(&datos[0], 1, dl, f);
    return true;
}

bool CompilarJuego(UI* u) {
    if (!u) return false;
    std::vector<std::string> scripts, assets;
    Recolectar(u, &scripts, &assets);
    if (scripts.empty()) {
        Notificar("Compilar: el proyecto no tiene ningun script (tarjeta Script)", true);
        return false;
    }
    // el PROYECTO es la carpeta del primer script; todo se compila a build/
    std::string proy = Carpeta(scripts[0]);
    std::string build = proy + "/build";
    std::string nombre = u->name.empty() ? std::string("juego") : u->name;
    { char cmd[600]; snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", build.c_str()); if (system(cmd)) {} }

    // [1/3] el arbol de la interfaz
    Notificar("Compilar [1/3]: exportando " + nombre + ".w3dui", false);
    std::string w3dui = build + "/" + nombre + ".w3dui";
    if (!UI2DGuardar(u, w3dui)) {
        Notificar("Compilar: no pude escribir el .w3dui", true);
        return false;
    }

    // [2/3] cada .lua legible -> .luac (bytecode); el luac compila junto al editor
    Notificar("Compilar [2/3]: scripts lua -> luac", false);
    std::vector<std::string> luacs;
    for (size_t i = 0; i < scripts.size(); i++) {
        std::string salida = build + "/" + Base(scripts[i]);
        size_t p = salida.rfind(".lua");
        if (p != std::string::npos) salida = salida.substr(0, p) + ".luac";
        char cmd[2000];
        // el luac compila con el editor (cmake lo deja en thirdparty/); fallback al del sistema
        snprintf(cmd, sizeof(cmd),
                 "./thirdparty/luac -s -o \"%s\" \"%s\" 2>/dev/null || "
                 "./luac -s -o \"%s\" \"%s\" 2>/dev/null || luac -s -o \"%s\" \"%s\"",
                 salida.c_str(), scripts[i].c_str(), salida.c_str(), scripts[i].c_str(),
                 salida.c_str(), scripts[i].c_str());
        if (system(cmd) != 0) {
            Notificar("Compilar: fallo luac con " + Base(scripts[i]), true);
            return false;
        }
        luacs.push_back(salida);
    }

    // [3/3] TODO en un solo archivo ofuscado
    Notificar("Compilar [3/3]: empaquetando " + nombre + ".w3dgame", false);
    std::string pack = build + "/" + nombre + ".w3dgame";
    FILE* f = fopen(pack.c_str(), "wb");
    if (!f) { Notificar("Compilar: no pude crear el paquete", true); return false; }
    fwrite("W3DG", 1, 4, f);
    unsigned total = 0;
    long posTotal = ftell(f);
    fwrite(&total, 4, 1, f);
    if (PackEntrada(f, nombre + ".w3dui", w3dui)) total++;
    for (size_t i = 0; i < luacs.size(); i++)
        if (PackEntrada(f, Base(luacs[i]), luacs[i])) total++;
    for (size_t i = 0; i < assets.size(); i++) {
        if (assets[i].empty()) continue;
        if (PackEntrada(f, Base(assets[i]), assets[i])) total++;
    }
    fseek(f, posTotal, SEEK_SET);
    fwrite(&total, 4, 1, f);
    fclose(f);

    char fin[256];
    snprintf(fin, sizeof(fin), "Juego compilado: build/%s.w3dgame (%u archivos)",
             nombre.c_str(), total);
    Notificar(fin, false);
    w3dLogf("CompilarJuego: %s (%u entradas)", pack.c_str(), total);
    return true;
}

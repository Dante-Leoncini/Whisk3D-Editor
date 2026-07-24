// ============================================================================
//  GuardarW3D.cpp — ver GuardarW3D.h.
// ============================================================================
#include "io/GuardarW3D.h"
#include "io/UI2DFormato.h"
#include "objects/Objects.h"
#include "objects/UI.h"
#include "objects/Camera.h"
#include "objects/Light.h"
#include "objects/Gamepad.h"
#include "script/W3dScript.h"
#include "ViewPorts/Notificaciones.h"
#include "ViewPorts/PopUp/FileBrowser.h"
#include "render/OpcionesRender.h"
#include "animation/Animation.h"   // AnimFPS (se guarda con el proyecto)
#include "variables.h"   // w3dPath (el archivo abierto)
#include "w3dlog.h"
#include <stdio.h>
#include <string>

static int gMallasOmitidas = 0;

static void Sang(FILE* f, int n) { for (int i = 0; i < n; i++) fputs("    ", f); }
static std::string Carpeta(const std::string& r) {
    size_t s = r.find_last_of("/\\");
    return (s == std::string::npos) ? std::string(".") : r.substr(0, s);
}
static std::string BaseSinExt(const std::string& r) {
    size_t s = r.find_last_of("/\\");
    std::string b = (s == std::string::npos) ? r : r.substr(s + 1);
    size_t p = b.find_last_of('.');
    return (p == std::string::npos) ? b : b.substr(0, p);
}

// escribe UN objeto (y baja a sus hijos donde corresponde)
static void EscribirObjeto(FILE* f, Object* o, int ind, const std::string& dirW3d) {
    if (!o) return;
    ObjectType t = o->getType();

    if (t == ObjectType::ui) {
        // la UI entera vive en un .w3dui HERMANO (arbol 2D + scripts + refs)
        std::string nombre = o->name.empty() ? std::string("ui") : o->name;
        std::string w3dui = nombre + ".w3dui";
        UI2DGuardar((UI*)o, dirW3d + "/" + w3dui);
        Sang(f, ind); fprintf(f, "UI {\n");
        Sang(f, ind + 1); fprintf(f, "archivo: \"%s\"\n", w3dui.c_str());
        Sang(f, ind); fprintf(f, "}\n\n");
        return;
    }

    if (t == ObjectType::gamepad) {
        Gamepad* g = (Gamepad*)o;
        Sang(f, ind); fprintf(f, "Gamepad {\n");
        Sang(f, ind + 1); fprintf(f, "name: \"%s\"\n", o->name.c_str());
        if (g->target) { Sang(f, ind + 1); fprintf(f, "target: \"%s\"\n", g->target->name.c_str()); }
        Sang(f, ind + 1); fprintf(f, "velocidad: %g\n", g->velocidad);
        Sang(f, ind + 1); fprintf(f, "piso: %g\n", g->piso);
        Sang(f, ind + 1); fprintf(f, "gravedad: %g\n", g->gravedad);
        Sang(f, ind + 1); fprintf(f, "limiteIzquierdo: %g\n", g->limiteIzquierdo);
        Sang(f, ind + 1); fprintf(f, "limiteDerecho: %g\n", g->limiteDerecho);
        Sang(f, ind + 1); fprintf(f, "limiteFondo: %g\n", g->limiteFondo);
        Sang(f, ind + 1); fprintf(f, "limiteFrente: %g\n", g->limiteFrente);
        Sang(f, ind + 1); fprintf(f, "velocidadMaximaCaida: %g\n", g->velocidadMaximaCaida);
        Sang(f, ind + 1); fprintf(f, "potenciaSalto: %g\n", g->potenciaSalto);
        // sus scripts + las referencias asignadas (ref_<propiedad>: "objeto")
        if (o->scriptDatos && !o->scriptDatos->scripts.empty()) {
            const W3dScriptEntrada& e = o->scriptDatos->scripts[0];
            std::string rel = e.ruta;
            if (rel.compare(0, dirW3d.size(), dirW3d) == 0 && rel.size() > dirW3d.size())
                rel = rel.substr(dirW3d.size() + 1);
            Sang(f, ind + 1); fprintf(f, "script: \"%s\"\n", rel.c_str());
            for (size_t r = 0; r < e.refs.size(); r++) {
                Sang(f, ind + 1);
                fprintf(f, "ref_%s: \"%s\"\n", e.refs[r].first.c_str(), e.refs[r].second.c_str());
            }
        }
        Sang(f, ind); fprintf(f, "}\n\n");
        return;
    }

    if (t == ObjectType::camera) {
        Camera* c = (Camera*)o;
        Sang(f, ind); fprintf(f, "Camera {\n");
        Sang(f, ind + 1); fprintf(f, "name: \"%s\"\n", o->name.c_str());
        if (c->target) { Sang(f, ind + 1); fprintf(f, "target: \"%s\"\n", c->target->name.c_str()); }
        Sang(f, ind + 1); fprintf(f, "x: %g\n", o->pos.x);
        Sang(f, ind + 1); fprintf(f, "y: %g\n", o->pos.y);
        Sang(f, ind + 1); fprintf(f, "z: %g\n", o->pos.z);
        Sang(f, ind + 1); fprintf(f, "rx: %g\n", o->rotEuler.x);
        Sang(f, ind + 1); fprintf(f, "ry: %g\n", o->rotEuler.y);
        Sang(f, ind + 1); fprintf(f, "rz: %g\n", o->rotEuler.z);
        Sang(f, ind); fprintf(f, "}\n\n");
        return;
    }

    if (t == ObjectType::light) {
        Light* l = (Light*)o;
        Sang(f, ind); fprintf(f, "Light {\n");
        Sang(f, ind + 1); fprintf(f, "name: \"%s\"\n", o->name.c_str());
        Sang(f, ind + 1); fprintf(f, "x: %g\n", o->pos.x);
        Sang(f, ind + 1); fprintf(f, "y: %g\n", o->pos.y);
        Sang(f, ind + 1); fprintf(f, "z: %g\n", o->pos.z);
        Sang(f, ind + 1); fprintf(f, "r: %g\n", l->diffuse[0]);
        Sang(f, ind + 1); fprintf(f, "g: %g\n", l->diffuse[1]);
        Sang(f, ind + 1); fprintf(f, "b: %g\n", l->diffuse[2]);
        Sang(f, ind); fprintf(f, "}\n\n");
        return;
    }

    if (t == ObjectType::collection) {
        Sang(f, ind); fprintf(f, "Collection {\n");
        Sang(f, ind + 1); fprintf(f, "name: \"%s\"\n", o->name.c_str());
        for (size_t i = 0; i < o->Childrens.size(); i++)
            EscribirObjeto(f, o->Childrens[i], ind + 1, dirW3d);
        Sang(f, ind); fprintf(f, "}\n\n");
        return;
    }

    if (t == ObjectType::mesh) {
        // v1: la geometria modelada no se serializa todavia (aviso al final)
        gMallasOmitidas++;
        return;
    }
    // otros tipos (curvas, constraints, armatures...): todavia no se guardan
}

bool GuardarW3D(const std::string& ruta) {
    if (!SceneCollection) return false;
    FILE* f = fopen(ruta.c_str(), "w");
    if (!f) {
        Notificar("Guardar: no pude escribir " + ruta, true);
        return false;
    }
    gMallasOmitidas = 0;
    std::string dir = Carpeta(ruta);
    fprintf(f, "Whisk3D {\n");
    fprintf(f, "    version: 20260724\n");
    fprintf(f, "    Escena {\n");
    fprintf(f, "        fullscreen: false\n");
    fprintf(f, "        fps: %d\n\n", AnimFPS);
    for (size_t i = 0; i < SceneCollection->Childrens.size(); i++)
        EscribirObjeto(f, SceneCollection->Childrens[i], 2, dir);
    fprintf(f, "    }\n\n");
    // el layout clasico (v1: fijo; el del editor en vivo se guardara mas adelante)
    fprintf(f, "    Layout {\n");
    fprintf(f, "        ViewportRow {\n");
    fprintf(f, "            ViewportColumn {\n");
    fprintf(f, "                Editor2D\n");
    fprintf(f, "                Timeline\n");
    fprintf(f, "                Split: 0.8\n");
    fprintf(f, "            }\n");
    fprintf(f, "            ViewportColumn {\n");
    fprintf(f, "                Outliner\n");
    fprintf(f, "                Properties\n");
    fprintf(f, "                Split: 0.4\n");
    fprintf(f, "            }\n");
    fprintf(f, "            Split: 0.72\n");
    fprintf(f, "        }\n");
    fprintf(f, "    }\n");
    fprintf(f, "}\n");
    fclose(f);
    if (gMallasOmitidas > 0) {
        char b[128];
        snprintf(b, sizeof(b), "Guardado (ojo: %d malla(s) modeladas no se guardan todavia)", gMallasOmitidas);
        Notificar(b, false);
    } else {
        Notificar("Proyecto guardado: " + BaseSinExt(ruta) + ".w3d", false);
    }
    w3dLogf("GuardarW3D: %s", ruta.c_str());
    return true;
}

static void GuardarElegido(const std::string& ruta) {
    std::string r = ruta;
    if (r.size() < 4 || r.substr(r.size() - 4) != ".w3d") r += ".w3d";
    if (GuardarW3D(r)) w3dPath = r;   // el proximo Ctrl+S va directo aca
}

// cuenta lo que la v1 del guardado TODAVIA no sabe escribir (mallas modeladas,
// curvas, constraints, armatures...): para no PISAR un proyecto con perdida
static void ContarNoGuardables(Object* o, int* n) {
    if (!o) return;
    ObjectType t = o->getType();
    if (t == ObjectType::mesh || t == ObjectType::curve || t == ObjectType::constraint ||
        t == ObjectType::armature || t == ObjectType::mirror || t == ObjectType::instance)
        (*n)++;
    for (size_t i = 0; i < o->Childrens.size(); i++) ContarNoGuardables(o->Childrens[i], n);
}

void GuardarProyecto() {
    if (!w3dPath.empty()) {
        int noGuardables = 0;
        ContarNoGuardables(SceneCollection, &noGuardables);
        if (noGuardables > 0) {
            // NO pisar el original: este guardado v1 perderia esos objetos.
            // Se guarda AL LADO y se avisa clarito.
            std::string aparte = w3dPath.substr(0, w3dPath.size() - 4) + "_guardado.w3d";
            char b[256];
            snprintf(b, sizeof(b), "Este proyecto tiene %d objeto(s) que el guardado aun no cubre: "
                     "guarde en %s (el original queda intacto)", noGuardables, aparte.c_str());
            Notificar(b, true);
            GuardarW3D(aparte);
            return;
        }
        GuardarW3D(w3dPath);
        return;
    }
    AbrirFileBrowser("Guardar proyecto", "Guardar", ".w3d", GuardarElegido, true /*guardar*/);
}

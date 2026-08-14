#include "lectura-escritura.h"
#include <iostream>
#include "importers/import_obj.h"
#include "importers/import_fbx.h"        // ImportFBX
#include "importers/import_gltf.h"       // ImportGLTF (.gltf/.glb)
#include "objects/Scene.h"               // SceneCollection: lo importado queda colgado ahi
#include "objects/Mesh.h"                // Mesh::origen

// ============================================================================
//  DE QUE ARCHIVO SALIO CADA MALLA IMPORTADA (Mesh::origen).
//
//  Lo seteaba UNICAMENTE import_wobj.cpp (el .wobj de los proyectos viejos). Los
//  TRES importadores que usa el menu -OBJ, glTF/GLB y FBX- no lo tocaban nunca,
//  asi que para el usuario real 'origen' no existia: no habia "reimportar desde
//  el original", el archivo del usuario no se marcaba como referencia externa (no
//  salia el renglon en EXTERNOS.txt) y el aviso de "el archivo de origen no esta"
//  al reabrir no podia dispararse jamas. Lo que se habia probado como OK era el
//  camino legacy, no el que usa el dueno.
//
//  Se marca ACA, en la puerta unica por la que pasan los tres, en vez de en cada
//  importador. Un importador nuevo lo hereda gratis y no se pisa un origen que el
//  importador ya haya puesto.
//
//  COMO SE SABE QUE ES NUEVO: por Object::serial, NO por "los hijos que aparecieron
//  al final de SceneCollection". Los TRES importadores cuelgan lo suyo de
//  CollectionActive (import_obj.cpp, import_gltf.cpp, import_fbx.cpp) y en la escena
//  POR DEFECTO CollectionActive es la "Coleccion" HIJA de Scene Collection, no Scene
//  Collection: mirar solo el top level dejaba la malla del usuario sin origen, o sea
//  el fix no corria en el unico camino que el dueno usa (menu Add > Import). El
//  serial es monotono y no se recicla, asi que "todo lo que nacio despues" se
//  identifica sin suposiciones sobre DONDE lo colgo el importador: se recorre el
//  arbol ENTERO desde la raiz.
// ============================================================================
// serial mas alto vivo en el arbol (0 si no hay nada): la marca de agua del "antes"
static unsigned int SerialMaxRec(Object* o) {
    if (!o) return 0;
    unsigned int mx = o->serial;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        unsigned int s = SerialMaxRec(o->Childrens[i]);
        if (s > mx) mx = s;
    }
    return mx;
}

// marca 'origen' en toda malla NACIDA en este import (serial > la marca de agua)
static void MarcarOrigenRec(Object* o, const std::string& path, unsigned int desdeSerial) {
    if (!o) return;
    if (o->getType() == ObjectType::mesh) {
        Mesh* m = (Mesh*)o;
        if (o->serial > desdeSerial && m->origen.empty()) m->origen = path;
    }
    for (size_t i = 0; i < o->Childrens.size(); i++) MarcarOrigenRec(o->Childrens[i], path, desdeSerial);
}

// dispatch por EXTENSION: .fbx -> ImportFBX; .gltf/.glb -> ImportGLTF; el resto (.obj) -> ImportOBJ.
// Va FUERA de la guarda de plataforma: los tres importadores se compilan en todas, y main.cpp lo
// llama para "--open <archivo>". Lo que es solo de PC es el explorador de archivos, no esto.
// ES LA PUERTA UNICA: cualquier camino que importe un modelo (file browser, menu Add > Import,
// --open, el picker de la web, el comando 'import' del harness) tiene que entrar por aca, o la
// malla queda sin origen y el .w3d guardado sale sin EXTERNOS.txt.
bool ImportModeloPorExtension(const std::string& path) {
    size_t d = path.find_last_of('.');
    std::string ext = (d == std::string::npos) ? std::string() : path.substr(d);
    for (size_t i = 0; i < ext.size(); i++) if (ext[i] >= 'A' && ext[i] <= 'Z') ext[i] += 32;
    const unsigned int antes = SerialMaxRec(SceneCollection);
    bool ok;
    if (ext == ".fbx") ok = ImportFBX(path);
    else if (ext == ".gltf" || ext == ".glb") ok = ImportGLTF(path);
    else               ok = ImportOBJ(path, false);
    MarcarOrigenRec(SceneCollection, path, antes);
    return ok;
}

void ImportModeloCallback(const std::string& path) { ImportModeloPorExtension(path); }

#if defined(ANDROID) || defined(W3D_SYMBIAN)

int abrir() { return 0; }              // TODO: picker propio
int BuscarVertexAnimation() { return 0; }

#else

#include "ViewPorts/PopUp/FileBrowser.h" // el explorador COMPARTIDO (reemplaza tinyfd)

int abrir() {
    // mismo flujo que el menu Add > import: abre el File browser compartido (OBJ + FBX + glTF/GLB)
    AbrirFileBrowser("Importar modelo", "Import 3D model", ".obj .fbx .gltf .glb", ImportModeloCallback);
    return 0;
}

int BuscarVertexAnimation() {
    // (la animacion por vertices todavia no esta implementada; cuando lo este
    //  se abre el browser con filtro .txt y se carga. Sin dialogo nativo.)
    return 0;
}

#endif

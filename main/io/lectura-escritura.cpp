#include "lectura-escritura.h"
#include <iostream>
#include "importers/import_obj.h"

#if defined(ANDROID) || defined(W3D_SYMBIAN)

int abrir() { return 0; }              // TODO: picker propio
int BuscarVertexAnimation() { return 0; }

#else

#include "ViewPorts/PopUp/FileBrowser.h" // el explorador COMPARTIDO (reemplaza tinyfd)
#include "importers/import_fbx.h"        // ImportFBX
#include "importers/import_gltf.h"       // ImportGLTF (.gltf/.glb)

// dispatch por EXTENSION: .fbx -> ImportFBX; el resto (.obj) -> ImportOBJ
static void ImportModeloElegido(const std::string& path) {
    size_t d = path.find_last_of('.');
    std::string ext = (d == std::string::npos) ? std::string() : path.substr(d);
    for (size_t i = 0; i < ext.size(); i++) if (ext[i] >= 'A' && ext[i] <= 'Z') ext[i] += 32;
    if (ext == ".fbx") ImportFBX(path);
    else if (ext == ".gltf" || ext == ".glb") ImportGLTF(path);
    else               ImportOBJ(path, false);
}

int abrir() {
    // mismo flujo que el menu Add > import: abre el File browser compartido (OBJ + FBX + glTF/GLB)
    AbrirFileBrowser("Importar modelo", "Import 3D model", ".obj .fbx .gltf .glb", ImportModeloElegido);
    return 0;
}

int BuscarVertexAnimation() {
    // (la animacion por vertices todavia no esta implementada; cuando lo este
    //  se abre el browser con filtro .txt y se carga. Sin dialogo nativo.)
    return 0;
}

#endif

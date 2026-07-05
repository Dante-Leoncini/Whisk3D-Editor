#include "lectura-escritura.h"
#include <iostream>
#include "importers/import_obj.h"

#if defined(ANDROID) || defined(W3D_SYMBIAN)

int abrir() { return 0; }              // TODO: picker propio
int BuscarVertexAnimation() { return 0; }

#else

#include "ViewPorts/PopUp/FileBrowser.h" // el explorador COMPARTIDO (reemplaza tinyfd)

static void ImportObjElegido(const std::string& path) {
    ImportOBJ(path, false);
}

int abrir() {
    // mismo flujo que el menu Add > import: abre el File browser compartido
    AbrirFileBrowser("Importar modelo", "Import Wavefront OBJ", ".obj", ImportObjElegido);
    return 0;
}

int BuscarVertexAnimation() {
    // (la animacion por vertices todavia no esta implementada; cuando lo este
    //  se abre el browser con filtro .txt y se carga. Sin dialogo nativo.)
    return 0;
}

#endif

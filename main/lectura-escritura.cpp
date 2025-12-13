#include "lectura-escritura.h"
#include <iostream>
#include "importers/import_obj.h"

#ifdef ANDROID

int abrir() {
    // TODO: Implementar picker de archivos en Android
    return 0;
}

int BuscarVertexAnimation() {
    // TODO: Implementar picker de archivos en Android
    return 0;
}

#else

int abrir() {
    const char* filtros[] = { "*.obj" };

    const char* ruta = tinyfd_openFileDialog(
        "Abrir archivo",
        "",
        1, filtros,
        "Archivos Wavefront OBJ",
        0
    );

    if (ruta) {
        std::cout << "Archivo seleccionado: " << ruta << std::endl;
        ImportOBJ(ruta, false);
    } else {
        std::cout << "No se seleccionó nada." << std::endl;
    }

    return 0;
}

int BuscarVertexAnimation() {
    const char* filtros[] = { "*.txt" };

    const char* ruta = tinyfd_openFileDialog(
        "Abrir archivo",
        "",
        1, filtros,
        "Archivos txt con animacion",
        0
    );

    if (ruta) {
        std::cout << "Archivo seleccionado: " << ruta << std::endl;
        // ImportVertexAnimation(ruta);
    } else {
        std::cout << "No se seleccionó nada." << std::endl;
    }

    return 0;
}

#endif
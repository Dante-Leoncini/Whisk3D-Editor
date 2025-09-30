#include <iostream>
#include "./tinyfiledialogs.h"

int abrir() {
    //const char* filtros[] = { "*.txt", "*.cpp", "*.h" };
    const char* filtros[] = { "*.obj" };

    // Abrir diálogo para seleccionar un archivo
    const char* ruta = tinyfd_openFileDialog(
        "Abrir archivo",       // título
        "",                    // path inicial
        1, filtros,            // cantidad y filtros
        "Archivos Wavefront OBJ", // descripción
        0                      // permitir múltiples selecciones? (0 = no)
    );

    if (ruta) {
        std::cout << "Archivo seleccionado: " << ruta << std::endl;
        ImportOBJ(ruta);
    } else {
        std::cout << "No se seleccionó nada." << std::endl;
    }

    return 0;
}
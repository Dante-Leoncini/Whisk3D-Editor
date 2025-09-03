#include <iostream>
#include "./tinyfiledialogs.h"

int abrir() {
    const char* filtros[] = { "*.txt", "*.cpp", "*.h" };

    // Abrir diálogo para seleccionar un archivo
    const char* ruta = tinyfd_openFileDialog(
        "Abrir archivo",       // título
        "",                    // path inicial
        3, filtros,            // cantidad y filtros
        "Archivos de texto y código", // descripción
        0                      // permitir múltiples selecciones? (0 = no)
    );

    if (ruta) {
        std::cout << "Archivo seleccionado: " << ruta << std::endl;
    } else {
        std::cout << "No se seleccionó nada." << std::endl;
    }

    return 0;
}
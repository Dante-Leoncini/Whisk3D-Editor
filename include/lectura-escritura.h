#include <iostream>

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

    int BuscarVertexAnimation(){
        const char* filtros[] = { "*.txt" };

        // Abrir diálogo para seleccionar un archivo
        const char* ruta = tinyfd_openFileDialog(
            "Abrir archivo",       // título
            "",                    // path inicial
            1, filtros,            // cantidad y filtros
            "Archivos txt con animacion", // descripción
            0                      // permitir múltiples selecciones? (0 = no)
        );

        if (ruta) {
            std::cout << "Archivo seleccionado: " << ruta << std::endl;
            //ImportVertexAnimation(ruta);
        } else {
            std::cout << "No se seleccionó nada." << std::endl;
        }

        return 0;  
    }
#endif
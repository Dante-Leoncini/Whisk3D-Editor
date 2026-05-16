#pragma once

#include <string>

namespace w3dFileSystem {
    // Inicializa rutas globales
    void Init();

    // rutas cacheadas
    const std::string& GetExeDir();
    const std::string& GetResDir();

}
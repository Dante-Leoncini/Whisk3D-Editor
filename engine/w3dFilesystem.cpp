// ============================================================
// w3dFileSystem.cpp
// ============================================================

#include "w3dFilesystem.h"

#include <filesystem>
#include <sstream>
#include <iostream>
#include <cstdlib>

#ifdef _WIN32
    #define NOMINMAX
    #include <windows.h>
#else
    #include <unistd.h>
    #include <limits.h>
    #include <cstring>
#endif

namespace w3dFileSystem {

    static std::string gExeDir;
    static std::string gResDir;

    // ========================================================
    // Obtener directorio del ejecutable
    // ========================================================

    static std::string DetectExeDir() {

    #ifdef _WIN32

        char buffer[MAX_PATH];

        GetModuleFileNameA(NULL, buffer, MAX_PATH);

        std::string path(buffer);

        size_t pos = path.find_last_of("\\/");

        if (pos != std::string::npos)
            path = path.substr(0, pos);

        return path;

    #else

        char result[PATH_MAX];

        ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);

        std::string path;

        if (count != -1) {

            path = std::string(result, count);

            size_t pos = path.find_last_of('/');

            if (pos != std::string::npos)
                path = path.substr(0, pos);
        }

        return path;

    #endif
    }

    // ========================================================
    // Obtener carpeta res
    // ========================================================

    static std::string DetectResDir() {
    #ifdef __ANDROID__
        return "res";
    #endif

        std::string exeDir = gExeDir;

        // ======================================================
        // 1. Portable: ./res al lado del exe
        // ======================================================
        if (!exeDir.empty()) {
            std::string portable = exeDir + "/res";
            if (std::filesystem::exists(portable)) {
                std::cout << "Usando res portable\n";
                return portable;
            }
        }

    #ifndef _WIN32

        // ======================================================
        // 2. Instalación del sistema (Linux)
        // ======================================================

        const char* xdg = std::getenv("XDG_DATA_DIRS");
        std::string dirs = xdg ? xdg : "/usr/local/share:/usr/share";

        std::istringstream stream(dirs);
        std::string dir;

        while (std::getline(stream, dir, ':')) {

            std::string path = dir + "/Whisk3d/res";

            if (std::filesystem::exists(path)) {
                std::cout << "Usando res sistema: " << path << "\n";
                return path;
            }
        }

        // fallback directo seguro
        std::string fallback = "/usr/share/Whisk3d/res";

        if (std::filesystem::exists(fallback))
            return fallback;

    #endif

    #ifdef _WIN32
        return exeDir + "/res";
    #endif

        // ======================================================
        // 3. Último fallback seguro (CRÍTICO)
        // ======================================================
        return exeDir + "/res";
    }

    // ========================================================
    // Init
    // ========================================================

    void Init() {
        gExeDir = DetectExeDir();
        gResDir = DetectResDir();
        std::cout << "ExeDir: " << gExeDir << "\n";
        std::cout << "ResDir: " << gResDir << "\n";
    }

    // ========================================================
    // Getters
    // ========================================================

    const std::string& GetExeDir() {
        return gExeDir;
    }

    const std::string& GetResDir() {
        return gResDir;
    }
}

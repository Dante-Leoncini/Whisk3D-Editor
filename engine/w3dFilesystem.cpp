// ============================================================
// w3dFileSystem.cpp
// ============================================================

#include "w3dFileSystem.h"

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
    static std::string gUserConfigDir;

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
    // Obtener config dir usuario
    // ========================================================

    static std::string DetectUserConfigDir() {

    #ifdef WHISK3D_LINUX

        const char* xdgConfigHome = std::getenv("XDG_CONFIG_HOME");

        std::string configDir;

        if (xdgConfigHome && strlen(xdgConfigHome) > 0) {

            configDir = std::string(xdgConfigHome) + "/Whisk3d";

        } else {

            const char* home = std::getenv("HOME");

            if (home)
                configDir = std::string(home) + "/.config/Whisk3d";
        }

        if (!configDir.empty() &&
            !std::filesystem::exists(configDir)) {

            std::filesystem::create_directories(configDir);

            std::cout
                << "Creado directorio config usuario: "
                << configDir << "\n";
        }

        return configDir;

    #else

        return "";

    #endif
    }

    // ========================================================
    // Obtener carpeta res
    // ========================================================

    static std::string DetectResDir() {

    #ifdef __ANDROID__

        return "res";

    #elif defined(WHISK3D_LINUX)

        std::string exeDir = gExeDir;

        // portable
        if (!exeDir.empty()) {

            std::string portablePath = exeDir + "/res";

            if (std::filesystem::exists(portablePath)) {

                std::cout
                    << "Usando res portable\n";

                return portablePath;
            }
        }

        // appimage
        if (!exeDir.empty()) {

            std::string appImagePath =
                exeDir + "/../share/Whisk3d/res";

            if (std::filesystem::exists(appImagePath)) {

                return std::filesystem
                    ::canonical(appImagePath)
                    .string();
            }
        }

        // system install
        const char* xdgDataDirs =
            std::getenv("XDG_DATA_DIRS");

        std::string dataDirs =
            xdgDataDirs
                ? xdgDataDirs
                : "/usr/local/share:/usr/share";

        std::istringstream stream(dataDirs);

        std::string dir;

        while (std::getline(stream, dir, ':')) {

            std::string systemPath =
                dir + "/Whisk3d/res";

            if (std::filesystem::exists(systemPath)) {

                return systemPath;
            }
        }

        return "";

    #elif defined(_WIN32)

        return gExeDir + "/res";

    #else

        return "res";

    #endif
    }

    // ========================================================
    // Init
    // ========================================================

    void Init() {

        gExeDir = DetectExeDir();

        gUserConfigDir = DetectUserConfigDir();

        gResDir = DetectResDir();

        std::cout << "ExeDir: " << gExeDir << "\n";
        std::cout << "ResDir: " << gResDir << "\n";

        if (!gUserConfigDir.empty()) {
            std::cout
                << "UserConfigDir: "
                << gUserConfigDir
                << "\n";
        }
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

    const std::string& GetUserConfigDir() {
        return gUserConfigDir;
    }
}

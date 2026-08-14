// ============================================================================
//  LuaCompilar.cpp — ver LuaCompilar.h. Compila .lua a bytecode stripped con
//  el interprete que ya linkea el editor (nada de luac externo).
// ============================================================================
#include "io/LuaCompilar.h"

#ifdef W3D_SYMBIAN
// El build Symbian no linkea lua (W3dScript.cpp no esta en el .mmp) y tampoco
// tiene el boton "Compilar juego": stub no-op para cumplir la regla del repo
// (todo .cpp nuevo de main/ entra al Whisk3D.mmp).
bool LuaCompilarArchivo(const std::string&, const std::string&, std::string* err) {
    if (err) *err = "compilar lua no esta disponible en Symbian";
    return false;
}
#else

#include <stdio.h>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

// writer de lua_dump: vuelca cada trozo del bytecode al FILE* del .tmp.
// Devuelve 0 si pudo (contrato de lua_Writer; distinto de 0 corta el dump).
static int LcEscribir(lua_State*, const void* p, size_t n, void* ud) {
    if (n == 0) return 0;
    return (fwrite(p, 1, n, (FILE*)ud) == n) ? 0 : 1;
}

bool LuaCompilarArchivo(const std::string& rutaLua, const std::string& rutaSalida,
                        std::string* err) {
    if (err) err->clear();
    // estado efimero SOLO para compilar: sin libs abiertas (no se ejecuta nada)
    lua_State* L = luaL_newstate();
    if (!L) { if (err) *err = "sin memoria para el estado de lua"; return false; }
    // luaL_loadfile: parsea (y salta el BOM si lo hay). Si rutaLua ya fuese
    // bytecode lo carga igual y el dump lo re-emite stripped: inofensivo.
    if (luaL_loadfile(L, rutaLua.c_str()) != LUA_OK) {
        if (err) {
            const char* m = lua_tostring(L, -1);
            *err = m ? m : ("no pude cargar " + rutaLua);
        }
        lua_close(L);
        return false;
    }
    // volcado a <salida>.tmp primero: nunca queda una salida a medias
    std::string tmp = rutaSalida + ".tmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) {
        if (err) *err = "no pude escribir " + tmp;
        lua_close(L);
        return false;
    }
    // strip=1: sin nombres de variables ni numeros de linea (mas chico e ilegible)
    int rc = lua_dump(L, LcEscribir, f, 1);
    bool ok = (fclose(f) == 0) && (rc == 0);
    lua_close(L);
    if (!ok) {
        remove(tmp.c_str());
        if (err) *err = "fallo el volcado del bytecode de " + rutaLua;
        return false;
    }
#ifdef _WIN32
    // Windows: rename() no pisa el destino -> sacarlo antes (no atomico, pero
    // es una copia de staging regenerable, no un archivo del proyecto)
    remove(rutaSalida.c_str());
#endif
    if (rename(tmp.c_str(), rutaSalida.c_str()) != 0) {
        remove(tmp.c_str());
        if (err) *err = "no pude renombrar " + tmp + " -> " + rutaSalida;
        return false;
    }
    return true;
}

#endif // !W3D_SYMBIAN

// ============================================================================
//  LuaCompile.cpp — ver LuaCompile.h. Compila .lua a bytecode stripped con
//  el interprete que ya linkea el editor (nada de luac externo).
// ============================================================================
#include "io/LuaCompile.h"
#include "W3dLang.h"   // T(): el error llega al aviso de "Compilar juego", en el idioma del editor
#include "w3dFilesystem.h"   // ReplaceFile

#ifdef W3D_SYMBIAN
// El build Symbian no linkea lua (W3dScript.cpp no esta en el .mmp) y tampoco
// tiene el boton "Compilar juego": stub no-op para cumplir la regla del repo
// (todo .cpp nuevo de main/ entra al Whisk3D.mmp).
bool LuaCompileFile(const std::string&, const std::string&, std::string* err) {
    if (err) *err = T("compiling Lua is not available on Symbian");
    return false;
}
#else

#include <stdio.h>
#include <string.h>
#include <vector>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

// el detalle del error con el formato YA TRADUCIDO (uno o dos %s). El buffer se mide con el formato y las rutas:
// en cirilico una traduccion ocupa el doble de bytes y las rutas no tienen cota.
static std::string LcError(const char* fmt, const std::string& a, const std::string& b = std::string()) {
    std::vector<char> buf(strlen(fmt) + a.size() + b.size() + 64);
    snprintf(&buf[0], buf.size(), fmt, a.c_str(), b.c_str());
    return std::string(&buf[0]);
}

// writer de lua_dump: vuelca cada trozo del bytecode al FILE* del .tmp.
// Devuelve 0 si pudo (contrato de lua_Writer; distinto de 0 corta el dump).
static int LcWrite(lua_State*, const void* p, size_t n, void* ud) {
    if (n == 0) return 0;
    return (fwrite(p, 1, n, (FILE*)ud) == n) ? 0 : 1;
}

bool LuaCompileFile(const std::string& pathLua, const std::string& outputPath,
                        std::string* err) {
    if (err) err->clear();
    // estado efimero SOLO para compilar: sin libs abiertas (no se ejecuta nada)
    lua_State* L = luaL_newstate();
    if (!L) { if (err) *err = T("out of memory for the Lua state"); return false; }
    // luaL_loadfile: parsea (y salta el BOM si lo hay). Si pathLua ya fuese
    // bytecode lo carga igual y el dump lo re-emite stripped: inofensivo.
    if (luaL_loadfile(L, pathLua.c_str()) != LUA_OK) {
        if (err) {
            const char* m = lua_tostring(L, -1);
            *err = m ? std::string(m) : LcError(T("could not load %s"), pathLua);
        }
        lua_close(L);
        return false;
    }
    // volcado a <salida>.tmp primero: nunca queda una salida a medias
    std::string tmp = outputPath + ".tmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) {
        if (err) *err = LcError(T("could not write %s"), tmp);
        lua_close(L);
        return false;
    }
    // strip=1: sin nombres de variables ni numeros de linea (mas chico e ilegible)
    int rc = lua_dump(L, LcWrite, f, 1);
    bool ok = (fclose(f) == 0) && (rc == 0);
    lua_close(L);
    if (!ok) {
        remove(tmp.c_str());
        if (err) *err = LcError(T("the bytecode dump of %s failed"), pathLua);
        return false;
    }
    if (!w3dFileSystem::ReplaceFile(tmp, outputPath)) {
        remove(tmp.c_str());
        if (err) *err = LcError(T("could not rename %s -> %s"), tmp, outputPath);
        return false;
    }
    return true;
}

#endif // !W3D_SYMBIAN

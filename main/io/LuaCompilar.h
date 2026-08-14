#ifndef LUACOMPILAR_H
#define LUACOMPILAR_H

#include <string>

// ============================================================================
//  LuaCompilar — compila un .lua (fuente) a BYTECODE Lua stripped usando el
//  MISMO interprete que ya linkea el editor (lua_State efimero + luaL_loadfile
//  + lua_dump con strip=1). Sin binario luac externo.
//
//  Lo usa "Compilar juego" en builds de PRODUCCION (checkbox "Modo debug"
//  destildado): cada .lua COPIADO al staging (platform-build/<plat>) se
//  reemplaza por su bytecode CON EL MISMO NOMBRE .lua. Los .lua del PROYECTO
//  no se tocan jamas. El runtime (CorrerArchivo del Core) ya detecta bytecode
//  por el header 0x1B via luaL_loadbuffer, asi que la carga no cambia.
//
//  PORTABILIDAD: el header del bytecode de Lua 5.4 valida tamanos de
//  Instruction / lua_Integer / lua_Number y endianness. Con la config del
//  ecosistema (sin LUA_32BITS ni LUA_USE_C89) los 4 targets quedan iguales al
//  editor x86_64: Instruction=4, lua_Integer=8 (long long), lua_Number=8
//  (double), little-endian (x86_64, ARM32/ARM64 Android, wasm).
//
//  Escritura ATOMICA: escribe a <rutaSalida>.tmp y hace rename() al final
//  (rutaSalida puede ser el mismo rutaLua: reemplaza la copia de staging).
//  Si la compilacion falla (error de sintaxis) devuelve false con el mensaje
//  humano en *err y NO toca rutaSalida: el build debe abortar con ese error
//  (mejor enterarse al compilar que en el device).
// ============================================================================
bool LuaCompilarArchivo(const std::string& rutaLua, const std::string& rutaSalida,
                        std::string* err);

#endif // LUACOMPILAR_H

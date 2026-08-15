// ============================================================================
//  Guardar el config.ini. El load vive en main.cpp desde siempre; el SAVE no existia -- el config se editaba a mano
//  con un editor de texto. La tarjeta "Ajustes" de Properties necesita escribirlo, asi que aca esta el otro lado.
//
//  Se escriben TODAS las claves que el load sabe leer, no solo las que toca la UI: si se escribieran solo algunas,
//  guardar borraria lo que el usuario haya puesto a mano en el resto.
// ============================================================================
#include "variables.h"
#include "W3dLang.h"
#include "w3dFilesystem.h"
#include <stdio.h>
#include <string>

// Donde escribirlo: el mismo archivo que lee loadConfig al arrancar (res/config.ini junto al ejecutable).
static std::string W3dConfigPath(){
    return w3dFileSystem::GetResDir() + "/config.ini";
}

// true = se escribio. false = no se pudo (disco lleno, carpeta de solo lectura, Android sin permiso...).
// NO se toca cfg: el que llama ya la dejo como quiere; aca solo se vuelca.
bool W3dConfigGuardar(){
    const std::string ruta = W3dConfigPath();
    FILE* f = fopen(ruta.c_str(), "wb");
    if (!f) return false;

    fprintf(f, "fullscreen = %s\n",         cfg.fullscreen ? "true" : "false");
    fprintf(f, "enableAntialiasing = %s\n", cfg.enableAntialiasing ? "true" : "false");
    fprintf(f, "scale = %d\n",              cfg.scale);
    fprintf(f, "width = %d\n",              cfg.width);
    fprintf(f, "height = %d\n",             cfg.height);
    fprintf(f, "displayIndex = %d\n",       cfg.displayIndex);
    fprintf(f, "nuevoUsuario = %s\n",       cfg.nuevoUsuario ? "true" : "false");
    // AUTO KEY: preferencia del EDITOR (ver el comentario en variables.h). Se escribe desde el
    // boton del timeline para que quede prendido de una sesion a la otra.
    fprintf(f, "autoKey = %s\n",            cfg.autoKey ? "true" : "false");
    fprintf(f, "SkinName = %s\n",           cfg.SkinName.c_str());
    fprintf(f, "graphicsAPI = %s\n",        cfg.graphicsAPI.c_str());
    // raiz del repo para Compilar (la fija el usuario en Ajustes cuando el editor corre instalado). Solo se
    // escribe si hay una: sin valor no se escribe la clave, asi un editor "desde el arbol" no la clava vacia.
    if (!cfg.repoPath.empty()) fprintf(f, "repoPath = %s\n", cfg.repoPath.c_str());
    // "auto" = seguir al sistema (lo que hace si la clave no esta). Un idioma explicito PISA la deteccion: el que
    // quiere el editor en ingles con Windows en espaniol tiene que poder. Si el usuario NUNCA eligio idioma
    // (g_idiomaForzado false), se escribe "auto": guardar otro ajuste no debe clavar el idioma detectado.
    fprintf(f, "idioma = %s\n",             g_idiomaForzado ? W3dIdiomaCodigo(g_idioma) : "auto");

    // el ferror/fclose es parte del resultado: con stdio bufferado el disco lleno recien
    // aparece en el FLUSH de fclose, y descartarlo devolvia true con el config a medio escribir
    bool ok = (ferror(f) == 0);
    if (fclose(f) != 0) ok = false;
    return ok;
}

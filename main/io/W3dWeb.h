#ifndef W3DWEB_H
#define W3DWEB_H
// ============================================================================
//  W3dWeb.h - la version de NAVEGADOR no tiene un disco propio: el sistema de archivos de emscripten vive en memoria
//  y desaparece al cerrar la pestania. Asi que ahi:
//    - ABRIR (un proyecto, un modelo, una textura, un .lua...) usa el selector de archivos DEL NAVEGADOR: lo elegido
//      se copia a /uploads y se le pasa esa ruta al que lo pidio, igual que si viniera del explorador del editor.
//    - GUARDAR / EXPORTAR descarga el archivo a la PC (la carpeta de descargas del navegador). Los dialogos de
//      "elegir carpeta" se saltean: el archivo se escribe en /descargas y se descarga.
//  Fuera del navegador todo esto no hace nada (cada plataforma tiene su disco y su explorador).
// ============================================================================
#include <string>

// el archivo recien escrito se descarga a la PC (no-op fuera del navegador)
void W3dDownloadIfWeb(const std::string& vfsPath);
// abre el selector de archivos del navegador; lo elegido llega a 'cb' con su ruta en /uploads. false = no es el
// navegador (el que llama sigue con su explorador de siempre). filtro = ".obj .fbx .gltf" (como OpenFileBrowser)
bool W3dWebOpenFile(const std::string& title, const std::string& filter, void (*cb)(const std::string&));
// la carpeta donde se escribe lo que despues se descarga ("" fuera del navegador)
std::string W3dWebFolderDownloads();
#endif // W3DWEB_H

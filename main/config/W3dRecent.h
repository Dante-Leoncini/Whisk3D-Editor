#ifndef W3D_RECENT_H
#define W3D_RECENT_H
// ============================================================================
//  W3dRecent.h - LOS ULTIMOS PROYECTOS abiertos o guardados (la pantalla de inicio muestra los 5 ultimos:
//  ui/ViewPorts/PopUp/HomePopup.cpp). Se anotan solos cada vez que el editor queda con un archivo (abrir, guardar,
//  guardar como: ProjectSyncFields) y viven en recent.txt, en la carpeta del usuario (junto al config.start),
//  una ruta por linea, la mas nueva primero.
// ============================================================================
#include <string>
#include <vector>

// pone 'ruta' primera (si ya estaba, la sube) y lo guarda. No escribe nada si ya era la primera.
void W3dRecentAdd(const std::string& path);
// las que todavia existen, de la mas nueva a la mas vieja, hasta 'maximo'
std::vector<std::string> W3dRecent(int max);
// (harness) otro archivo en vez de recent.txt ("" = el de la carpeta del usuario); lo vuelve a leer
void W3dRecentPathSet(const std::string& path);

#endif // W3D_RECENT_H

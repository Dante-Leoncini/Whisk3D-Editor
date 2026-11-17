#ifndef W3D_RECIENTES_H
#define W3D_RECIENTES_H
// ============================================================================
//  W3dRecientes.h - LOS ULTIMOS PROYECTOS abiertos o guardados (la pantalla de inicio muestra los 5 ultimos:
//  ui/ViewPorts/PopUp/InicioPopup.cpp). Se anotan solos cada vez que el editor queda con un archivo (abrir, guardar,
//  guardar como: ProyectoSincronizarCampos) y viven en recientes.txt, en la carpeta del usuario (junto al config.ini),
//  una ruta por linea, la mas nueva primero.
// ============================================================================
#include <string>
#include <vector>

// pone 'ruta' primera (si ya estaba, la sube) y lo guarda. No escribe nada si ya era la primera.
void W3dRecientesAgregar(const std::string& ruta);
// las que todavia existen, de la mas nueva a la mas vieja, hasta 'maximo'
std::vector<std::string> W3dRecientes(int maximo);
// (harness) otro archivo en vez de recientes.txt ("" = el de la carpeta del usuario); lo vuelve a leer
void W3dRecientesRutaFijar(const std::string& ruta);

#endif // W3D_RECIENTES_H

#ifndef W3D_LAYOUTS_H
#define W3D_LAYOUTS_H
// ============================================================================
//  W3dLayouts.h - LOS LAYOUTS GUARDADOS: como estan repartidos los viewports, con un nombre. Son del PROGRAMA, no del
//  proyecto: viven en layouts.txt, en la carpeta del usuario (junto al config.ini). Cada uno es el texto "Layout { ... }"
//  que entiende el lector del .w3d (W3dLayoutDesdeTexto). El de fabrica ("Por defecto": LayoutPorDefecto) no se guarda
//  ni se borra. La PRIMERA vez (sin layouts.txt) se crea "Animacion": el visor 3D arriba y la linea de tiempo abajo;
//  viene con el programa pero se puede borrar.
//  El menu: la esquina de cada viewport -> Layout (ui/ViewPorts/LayoutInput.cpp).
// ============================================================================
#include <string>
#include <vector>

// los nombres, en el orden en que se guardaron
std::vector<std::string> W3dLayoutsNombres();
// el texto "Layout { ... }" de uno ("" = no existe)
std::string W3dLayoutTexto(const std::string& nombre);
// guarda (o reemplaza) uno. false = nombre vacio o no se pudo escribir
bool W3dLayoutGuardar(const std::string& nombre, const std::string& texto);
// lo borra. false = no existia
bool W3dLayoutBorrar(const std::string& nombre);
// (harness) otro archivo en vez de layouts.txt ("" = el de la carpeta del usuario); lo vuelve a leer
void W3dLayoutsRutaFijar(const std::string& ruta);

#endif // W3D_LAYOUTS_H

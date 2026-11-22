#ifndef W3D_LAYOUTS_H
#define W3D_LAYOUTS_H
// ============================================================================
//  W3dLayouts.h - LOS LAYOUTS GUARDADOS: como estan repartidos los viewports, con un nombre. Son del PROGRAMA, no del
//  proyecto: viven en layouts.txt, en la carpeta del usuario (junto al config.start). Cada uno es el texto "Layout { ... }"
//  que entiende el lector del .w3d (W3dLayoutFromText). El de fabrica ("Por defecto": LayoutDefault) no se guarda
//  ni se borra. La PRIMERA vez (sin layouts.txt) se crea "Animacion": el visor 3D arriba y la linea de tiempo abajo;
//  viene con el programa pero se puede borrar.
//  El menu: la esquina de cada viewport -> Layout (ui/ViewPorts/LayoutInput.cpp).
// ============================================================================
#include <string>
#include <vector>

// los nombres, en el orden en que se guardaron
std::vector<std::string> W3dLayoutsNames();
// el texto "Layout { ... }" de uno ("" = no existe)
std::string W3dLayoutText(const std::string& name);
// guarda (o reemplaza) uno. false = nombre vacio o no se pudo escribir
bool W3dLayoutSave(const std::string& name, const std::string& text);
// lo borra. false = no existia
bool W3dLayoutDelete(const std::string& name);
// (harness) otro archivo en vez de layouts.txt ("" = el de la carpeta del usuario); lo vuelve a leer
void W3dLayoutsPathSet(const std::string& path);

#endif // W3D_LAYOUTS_H

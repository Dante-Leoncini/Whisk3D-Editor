#ifndef W3D_PANELS_H
#define W3D_PANELS_H

// ============================================================================
//  LAS TARJETAS DECLARADAS de Properties: en vez de armar cada tarjeta a mano (crear las filas, guardarlas en
//  miembros, apuntarlas al objeto activo, esconderlas, armar sus desplegables), se DESCRIBEN en texto
//  (ui/panels/PanelsText.cpp) y este modulo las arma y las mantiene. El texto:
//
//    card <pestania> <tipo> "<titulo>" [width=0.40] [after="<titulo>"]
//                                                           width: lo que ocupa la columna de valores; after: la tarjeta
//                                                           va despues de esa (el orden con las que no son declaradas)
//      bool   "<etiqueta>" <campo>                          un tilde
//      float  "<etiqueta>" <campo> [unit=m] [min=0] [max=1] [steps=<fino>,<grueso>,<arrastre>]   un numero
//      vec3   "<etiqueta>" <campo> [unit=...] [steps=...] [axes=full]
//                                                           tres numeros: "<etiqueta> X", "Y", "Z" (axes=full: la etiqueta
//                                                           entera en los tres)
//      color  "<etiqueta>" <campo>                          un color
//      choice "<etiqueta>" <campo> "<opcion 0>|<opcion 1>|..." [order=0,2,1]
//                                                           un desplegable que escribe el numero de la opcion; cada
//                                                           opcion puede llevar su icono ("Cruz:cross"); order = en que
//                                                           orden van en el menu (los numeros no cambian)
//
//  <tipo> es el del .w3d ("fog"), <campo> uno de la tabla de objects/Fields.h ("fog.density"). Cualquier fila puede
//  terminar en  when=<campo>==<n>  o  when=<campo>!=<n> (un tilde vale 0 o 1): se ve solo entonces. Las etiquetas y las opciones pasan por
//  la traduccion (T). '#' empieza un comentario.
// ============================================================================
#include <vector>

class Object;
class GroupPropertie;
struct W3dPanels;

// arma las tarjetas del texto y las agrega a 'cards' (son del panel: las libera con las demas)
W3dPanels* W3dPanelsBuild(std::vector<GroupPropertie*>& cards);
// suelta lo que el modulo guarda de esas tarjetas (llamar antes de que el panel libere las suyas)
void W3dPanelsFree(W3dPanels* p);
// cada fila apunta al campo del objeto activo (o se esconde: otro tipo, o su 'when' no se cumple). Corre en cada
// cuadro (tildar algo cambia un 'when' ya). true = alguna fila aparecio o se fue (el panel rearma su layout)
bool W3dPanelsBind(W3dPanels* p, Object* active);
// que tarjetas se ven con esa pestania y ese objeto
void W3dPanelsShow(W3dPanels* p, int tab, Object* active);
// hay alguna tarjeta para ese objeto en esa pestania (la pestania existe)
bool W3dPanelsHasCard(const W3dPanels* p, int tab, Object* active);

#endif

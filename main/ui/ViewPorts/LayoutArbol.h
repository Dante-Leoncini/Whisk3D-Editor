#ifndef LAYOUTARBOL_H
#define LAYOUTARBOL_H

// ============================================================================
//  VIDA DEL ARBOL DE VIEWPORTS: el aviso de destruccion y el borrado de un arbol entero.
//
//  Antes el layout vivia para siempre: abrir un proyecto armaba uno nuevo y el anterior
//  quedaba HUERFANO ("el viejo queda"), con todo lo que sus viewports tenian adentro (las
//  tarjetas del panel de propiedades, las barras de botones...): ~0,2 MB de heap por
//  apertura, fijos, cualquiera fuera el proyecto. Ahora se libera. Lo que lo impedia es
//  que varios modulos del editor se guardan punteros a viewports FUERA del arbol: el
//  activo (viewPortActive, Viewport3DActive, PropsActivo), el gesto en curso (scroll
//  lockeado, esquina, barra del pincel), el viewport que abrio un menu, el maximizado...
//  Liberar el arbol con esos punteros vivos es un use-after-free esperando al proximo click.
//
//  El mismo patron que el aviso de destruccion de Object (W3dDesvincularRegistrar,
//  Objects.h): cada modulo que guarde punteros a viewports registra un gancho y
//  ~ViewportBase los avisa a todos con el viewport que muere. Vale para CUALQUIER borrado
//  (abrir un proyecto, cambiar el tipo de un viewport, Expand), no solo para este.
//
//  OJO (lo que el gancho NO puede hacer): cuando corre, la parte DERIVADA del viewport ya
//  se destruyo (~Viewport3D, ~Properties... ya terminaron). El gancho solo COMPARA
//  punteros; no puede leer nada del viewport.
//
//  Header aparte (y no ViewPorts.h) a proposito: lo incluyen solo los que registran
//  ganchos o liberan arboles, y tocarlo no recompila medio editor.
// ============================================================================

class ViewportBase;

typedef void (*ViewportOlvidarFn)(ViewportBase* muerto);
enum { ViewportOlvidarMaxGanchos = 12 };
// registra un gancho (idempotente: registrar dos veces el mismo no lo duplica). Devuelve
// false si no quedaban slots (agrandar ViewportOlvidarMaxGanchos). Pensado para llamarse
// desde un constructor global: la lista arranca en NULL por inicializacion ESTATICA
// (constante), o sea antes de cualquier constructor. Def. en ViewPorts.cpp.
bool ViewportOlvidarRegistrar(ViewportOlvidarFn f);

// libera un arbol de viewports ENTERO: contenedores y hojas. Los dtors de ViewportRow /
// ViewportColumn borran childB pero NO childA (a proposito, para evitar un doble free),
// asi que un 'delete raiz' pelado fuga toda la cadena de childA.
// Esto baja a mano y NULL-ea los hijos antes de cada delete. NULL = no hace nada.
void ViewportBorrarArbol(ViewportBase* raiz);

// cuantos viewports (hojas + contenedores) estan vivos en el proceso. Lo cuenta el
// ctor/dtor de ViewportBase; es para el harness: tras abrir N proyectos tienen que quedar
// exactamente los del layout actual.
int ViewportsVivos();

// la raiz del arbol COMPLETO: con un viewport maximizado, rootViewport es solo esa hoja y
// el arbol entero es el que guardo LayoutMaximizar. Def. en LayoutInput.cpp.
ViewportBase* LayoutRaizCompleta();

// re-anclar los punteros de "activo" al layout ACTUAL si quedaron en NULL (porque su
// viewport murio): viewPortActive = la primera hoja 3D (o la primera hoja); PropsActivo =
// el primer panel de propiedades. Viewport3DActive no se toca: lo re-engancha el primer
// render del 3D. Def. en LayoutInput.cpp.
void LayoutAnclarActivos();

#endif

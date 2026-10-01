#ifndef HITBOX_H
#define HITBOX_H
#include "physics/W3dHitbox.h"   // W3dHitboxBase: la definicion + el motor de solapes (Core)
#include <string>

struct JVal;   // io/JsonW3d.h

// ============================================================================
//  Hitbox — CAJA DE DETECCION (trigger estilo Unity). No empuja nada: avisa a
//  los scripts cuando algo le entra, se queda adentro o sale (alEntrar /
//  alQuedarse / alSalir). El motor y las reglas estan en el Core
//  (libs/Whisk3DCore/physics/W3dHitbox.h); aca vive lo del objeto de escena:
//
//    activo          false = no participa (tampoco sus pares: salen)
//    tam[3]          ancho X, alto Y, largo Z en espacio LOCAL (la escala de
//                    mundo se aplica: un hitbox hijo de un objeto escalado crece)
//    centro[3]       centro de la caja respecto del origen del objeto
//    etiqueta        lo que ven los filtros de los otros hitbox ("jugador")
//    filtro          solo reacciona a otros con esa etiqueta ("" = todos)
//    detectarCuerpos tambien contra los cuerpos rigidos (Add > Physics)
//
//  Se dibuja como caja de ALAMBRE: verde activo, gris inactivo, AMARILLA si en
//  el Play esta tocando algo que le importa. En el juego compilado no se ve
//  (salvo el overlay de debug, verHitboxes(true) desde lua).
//
//  Uso tipico: un hitbox HIJO de una puerta; el script de la puerta define
//  alEntrar(otro) y abre. Add > Hitbox con un objeto elegido lo crea como hijo
//  suyo del tamano de su malla.
// ============================================================================
class Hitbox : public W3dHitboxBase {
public:
    Hitbox(Object* parent = NULL, Vector3 pos = Vector3(0, 0, 0));
    void RenderObject() W3D_OVERRIDE;
};

// las 12 aristas de la caja (24 vertices, 72 floats) en el espacio LOCAL del hitbox:
// la comparten el dibujo y el pick del editor
void HitboxAristas(const W3dHitboxBase* h, float out[72]);

// overlay por TIPO del editor (Overlays > Objects > Hitbox): el viewport 3D lo publica por frame
// (= su showHitbox con los overlays prendidos), como g_showEmpty. Default true.
extern bool g_showHitbox;
// la caja se DIBUJA en este pase? Overlays del editor con el tipo prendido, o el overlay de debug
// de lua (verHitboxes). Una sola regla para el dibujo y el pick: lo que no se ve no se clickea.
bool HitboxSeDibuja(void);

// ---- lo que usan el editor y el lector/escritor (una sola fuente de los campos) ----
// copia de los campos propios (W3dDuplicarUno pone despues pos/rot/escala/nombre/scripts)
Hitbox* HitboxDuplicar(Hitbox* src);
// hijo NUEVO de 'padre' con la caja que ENVUELVE sus mallas (todo el subarbol, en el espacio
// local del padre). Sin mallas queda el cubo unitario. Lo usa Add > Hitbox.
Hitbox* HitboxCrearAjustado(Object* padre);
// true si 'o' puede ser padre de un hitbox creado desde Add (un objeto 3D de la escena)
bool HitboxPadreValido(Object* o);
// los campos propios en el .w3d ("tipo":"hitbox"): el escritor agrega ESTO despues de los
// comunes (cada linea empieza con ",\n" + sangria 'ind'), y el lector los lee de 'j'
void HitboxEscribirCampos(std::string& s, int ind, const W3dHitboxBase* h);
void HitboxLeerCampos(JVal* j, W3dHitboxBase* h);

#endif

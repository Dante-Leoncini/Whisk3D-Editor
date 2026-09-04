#ifndef BOXSELECT_H
#define BOXSELECT_H

// ============================================================================
//  BOX SELECT compartido (4 OS). Es una CAJA que se arrastra en pantalla y un par
//  de predicados geometricos; no sabe QUE se selecciona. Cada editor le pregunta
//  "este punto / este segmento, entra?" y decide lo suyo -- por eso lo puede usar
//  el viewport 3D (objetos y sub-elementos), y manana el editor UV o el 2D.
//
//  LOS DOS PASOS:
//    1) ARMAR (tecla B o el menu Select): aparece una CRUZ punteada pegada al
//       cursor, de lado a lado del viewport. No es decoracion: parte la vista en
//       4 cuadrantes y deja ver de que lado del futuro rectangulo va a caer cada
//       cosa antes de empezar a arrastrar.
//    2) ARRASTRAR: apretar y mover dibuja la caja; al soltar se aplica.
//
//  EL SENTIDO DEL ARRASTRE CAMBIA LA REGLA (y el color):
//    IZQUIERDA -> DERECHA (VERDE):  entra solo lo que queda ENTERO adentro.
//                                   Un pixel afuera y no entra.
//    DERECHA -> IZQUIERDA (AZUL):   alcanza con ROZAR. Si la caja toca el objeto
//                                   aunque sea en un punto, entra.
//  La diferencia solo importa para lo que TIENE EXTENSION (objetos, aristas,
//  caras). Un VERTICE es un punto: esta adentro o no, y ahi verde y azul dan lo
//  mismo (el propio dueno lo pidio asi).
//
//  C++03 (compila en Symbian). Comentarios sin acentos.
// ============================================================================

// ---- estado (uno solo: no se pueden arrastrar dos cajas a la vez) ----
bool BoxSelectArmado();       // paso 1: la cruz sigue al cursor, esperando el arrastre
bool BoxSelectArrastrando();  // paso 2: hay una caja en curso
bool BoxSelectActivo();       // armado o arrastrando (para que el viewport no orbite ni pickee)
void BoxSelectArmar();        // B / menu Select
void BoxSelectCancelar();     // Esc, click derecho, cambio de modo

void BoxSelectDown(int mx, int my);   // empieza la caja (coords de PANTALLA)
void BoxSelectMover(int mx, int my);  // la estira (y refresca la cruz mientras esta armado)
// termina el arrastre. false si no habia caja o si quedo degenerada (un click suelto).
// Devuelve el rect NORMALIZADO y la regla: tocar=true (azul) / entero=false (verde).
bool BoxSelectSoltar(int& x0, int& y0, int& x1, int& y1, bool& tocar);

void BoxSelectRect(int& x0, int& y0, int& x1, int& y1); // el rect en curso, normalizado
bool BoxSelectTocar();        // sentido actual: true = azul (rozar), false = verde (entero)

// ---- geometria (la comparten los tres tipos de elemento y los tres editores) ----
bool BoxPunto(int x0, int y0, int x1, int y1, float px, float py);
// el segmento a-b toca el rect? (cruza un lado, o esta entero adentro)
bool BoxSegmento(int x0, int y0, int x1, int y1,
                 float ax, float ay, float bx, float by);

// el punto cae adentro del poligono proyectado? (ray casting). Hace falta para el caso
// "la caja quedo ENTERA adentro de una cara": ahi no toca ningun vertice ni cruza ninguna
// arista, pero visualmente esta encima del objeto -- y en azul eso tiene que entrar.
bool BoxPuntoEnPoligono(const float* px, const float* py, int n, float qx, float qy);

// ---- dibujo: cruz punteada (armado) y caja punteada + relleno (arrastrando) ----
// En el ortho 2D LOCAL del viewport, como el circulo del pincel: recibe el origen
// del viewport en pantalla para pasar las coords del cursor a locales.
void BoxSelectDibujar(int vpX, int vpY, int vpAncho, int vpAlto);

#endif // BOXSELECT_H

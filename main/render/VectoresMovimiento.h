#ifndef VECTORES_MOVIMIENTO_H
#define VECTORES_MOVIMIENTO_H
// ============================================================================
//  VectoresMovimiento.h - LOS VECTORES DE MOVIMIENTO de una imagen y el DESENFOQUE DE MOVIMIENTO por CPU.
//
//  El pase de vectores es una pasada de GL comun (anda en el OpenGL ES 1.1 del N95): cada malla se dibuja SIN luz ni
//  textura con un COLOR POR VERTICE = cuanto se movio ese vertice EN LA PANTALLA desde el cuadro anterior (la camara
//  y el objeto: su matriz, el esqueleto, la vertex anim). GL interpola el color en el triangulo y queda una imagen
//  donde cada pixel dice hacia donde se movio. Con eso, despues, la CPU puede desenfocar la imagen en esa direccion
//  (W3dDesenfoqueMovimiento), y un reescalador temporal (FSR 2) puede reusar el cuadro anterior.
//
//  LA CODIFICACION (el PNG "_vectores"): R = 128 + dx * 2, G = 128 + dy * 2 (en pixeles de la imagen; X a la derecha,
//  Y hacia ABAJO, desde donde estaba hasta donde esta), B = 255 donde hay geometria (0 = el fondo), A = 255.
//  O sea 128 = quieto, cada paso = medio pixel, hasta +-63.5 pixeles (lo que se mueve mas queda en el tope).
//
//  Uso: W3dVecRecordar con la escena en el cuadro ANTERIOR (y la matriz de su camara); despues, con la escena en el
//  cuadro actual, el pase (W3dVecDibujar dentro del render a PNG por tiles) y W3dVecRecordar otra vez para el que sigue.
// ============================================================================
#include <vector>

#define W3D_VEC_PASOS_POR_PIXEL 2.0f    // la escala de la codificacion (2 = medio pixel por paso)

class Object;

// matriz PROYECCION x VISTA de la imagen ENTERA (column-major, como GL): la arma el viewport (Viewport3D::MatrizImagen)
// recuerda la escena TAL COMO ESTA (las posiciones en el mundo de cada malla) y la matriz de su camara = "el anterior"
void W3dVecRecordar(Object* raiz, const float* VP16);
// olvida lo recordado (sin anterior, el pase sale quieto: 128,128)
void W3dVecOlvidar();
// la RANURA de lo recordado: 0 = el render a PNG (por defecto), 1 = la vista "Motion Vectors" del viewport
void W3dVecUsarRanura(int r);
bool W3dVecHayAnterior();
// dibuja el pase de vectores de la escena actual (llamar con la proyeccion del tile y la VISTA en el ModelView: las
// posiciones van en el mundo). VP16 = la de la imagen entera de outW x outH (la cuenta va en pixeles de esa imagen)
void W3dVecDibujar(Object* raiz, const float* VP16, int outW, int outH);
// el color de fondo del pase (quieto, sin geometria)
void W3dVecColorFondo(float* rgba4);
// EL PASE DE PROFUNDIDAD (solo con shaders: ES 2.0 / Vulkan) para el postprocesado: la distancia a la camara / lejos en
// 24 bits (R la parte alta, G la del medio, B la baja; sin alfa). false = sin shaders
bool W3dVecDibujarDatos(Object* raiz, const float* VP16, int outW, int outH, float lejos);
// su fondo: lo mas lejos (blanco)
void W3dVecColorFondoDatos(float* rgba4);

// DESENFOQUE DE MOVIMIENTO (CPU, sin GL: sirve en el N95): cada pixel promedia 'muestras' lecturas a lo largo de SU
// vector (centrado), escalado por el 'obturador' (0..1: la fraccion del cuadro que la camara deja abierto; 0.5 = el
// clasico de 180 grados). rgba y vec del mismo tamano y en el mismo orden de filas. Escribe en 'out' (puede no ser rgba)
void W3dDesenfoqueMovimiento(const unsigned char* rgba, const unsigned char* vec, int w, int h, float obturador, int muestras,
                             unsigned char* out);
#endif

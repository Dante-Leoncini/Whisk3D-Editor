#ifndef REESCALADO_FSR_H
#define REESCALADO_FSR_H
// ============================================================================
//  ReescaladoFSR.h - AMD FidelityFX Super Resolution 1 (FSR 1) por CPU: agrandar una imagen con bordes nitidos.
//
//  Es el algoritmo abierto de AMD (licencia MIT, ffx_fsr1.h): no usa IA ni vectores de movimiento, anda en cualquier
//  maquina (no pide ninguna placa) y es un solo cuadro. Dos pasos:
//    EASU: el agrandado. Por cada pixel de salida mira 12 de la imagen chica, estima hacia donde va el borde (con la
//          luma) y mezcla con un lanczos estirado A LO LARGO del borde: los bordes quedan finos, no borrosos.
//    RCAS: el afilado. Una cruz de 5 pixeles; afila hasta donde no se pasa del minimo/maximo de los vecinos (sin halo).
//  Pensado para el trazado de rayos del viewport (g_rt.escala): se traza a 1/2, 1/3 o 1/4 y esto lo lleva al tamano
//  de la vista. RGBA de 8 bits, filas de arriba hacia abajo; el alfa se copia del vecino mas cercano.
// ============================================================================

// agranda src (sw x sh) a dst (dw x dh). nitidez: 0 = el maximo afilado .. 2 = casi nada (los "stops" de AMD).
// hilos: 0 = los nucleos de la maquina (uno solo donde no hay hilos)
void W3dFSR1(const unsigned char* src, int sw, int sh, unsigned char* dst, int dw, int dh, float nitidez, int hilos);
#endif

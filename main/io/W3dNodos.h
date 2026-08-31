#ifndef W3DNODOS_H
#define W3DNODOS_H

// ===========================================================================
//  OCLUSION POR PATH — el driver automatico del modificador "Oclusion" (CullingTri).
//
//  Un modificador con `pathNombre` apunta a un objeto de la escena que hace de
//  RECORRIDO: una Curve (el mismo riel que sigue la camara) o una malla de aristas
//  creada con Add > Path. Sus NODOS son las celdas del dato precalculado (.w3dvis /
//  .pvs.json): el tick elige el nodo mas cercano al ojo y lo materializa via la
//  maquinaria de sectores de siempre (W3dPVSSincronizar). Estilo SLST de PS1:
//  visibilidad 100% por dato, cero culling en runtime.
//
//  El ojo: con `soloCamaraActiva` (default) manda la CAMARA ACTIVA mientras se
//  juega (PLAY); en pausa el nodo se edita a mano en la card del modificador.
//  Con el checkbox APAGADO manda la vista del viewport activo: volar libre por el
//  escenario muestra en vivo que aparece/desaparece segun el dato.
// ===========================================================================

// un paso por frame: true = algun sector cambio (redibujar).
bool W3dOclusionTick();

#endif

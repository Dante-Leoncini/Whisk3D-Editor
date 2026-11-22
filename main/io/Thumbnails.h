#ifndef THUMBNAILS_H
#define THUMBNAILS_H
// ============================================================================
//  Miniaturas — las MINIATURAS de la cuadricula de la biblioteca (el outliner):
//  texturas (la imagen, reducida), materiales (una esfera con el material),
//  mallas 3D y prefabs (un render chico de frente), escenas y juegos (un render
//  desde su camara activa); sonidos, scripts, fuentes y videos usan el icono de su
//  tipo. PEREZOSAS (se generan recien cuando una celda las pide, pocas por cuadro)
//  y CACHEADAS en memoria (se rehacen si el recurso cambio); NO se guardan en el
//  .w3d. El render es por SOFTWARE (un rasterizador chico con z-buffer, en la CPU):
//  el mismo en los 4 sistemas, sin tocar el estado de GL del viewport.
//  Solo editor. C++03. Motor generico.
// ============================================================================
#include <string>
#include <vector>

enum { W3D_THUMBNAIL_SIDE = 64 };

// la textura GL de la miniatura (0 = todavia no / el tipo no tiene: se dibuja su icono). 'w'/'h' opcionales.
unsigned int W3dThumbnail(int type, const std::string& id, int* w, int* h);
// los PIXELES de la miniatura (RGBA, W3D_THUMBNAIL_SIDE de lado), generados en el momento (el harness los mira).
// false = el tipo no tiene miniatura o no se pudo.
bool W3dThumbnailPixels(int type, const std::string& id, std::vector<unsigned char>& rgba, int& w, int& h);
// un cuadro NUEVO: vuelve a haber presupuesto para generar (lo llama la cuadricula al dibujarse)
void W3dThumbnailsFrame();
// cuantas estan en el cache / cuantas faltaron por el presupuesto en el ultimo cuadro
int  W3dThumbnailsInCache();
int  W3dPendingThumbnails();
// suelta todas (cierre del proyecto; las texturas GL se borran)
void W3dThumbnailsForget();
// no hay RAM libre para generar vistas previas (se soltaron las que habia, por prioridad: ver Thumbnails.cpp)
bool W3dThumbnailsNoMemory();

#endif

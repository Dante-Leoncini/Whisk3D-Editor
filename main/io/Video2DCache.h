#ifndef VIDEO2D_CACHE_H
#define VIDEO2D_CACHE_H
#include <string>

// ============================================================================
//  Video2DCache — la PREVIEW de los videos del Editor 2D: extrae frames con
//  ffmpeg (una vez por sesion, a un cache en /tmp) y los sube como texturas.
//  Sin ffmpeg (u otra plataforma) devuelve 0 frames y el elemento dibuja un
//  placeholder. El juego real NO usa esto: reproduce el video nativo.
// ============================================================================

// VideoPreview, Video2DPreview y W3dMsNow los DECLARA el Core (los usa el dibujo de la UI 2D: WhiskUI/render/UIOverlay)
#include "base/W3dHost.h"

// un png con UN frame del medio del video (miniaturas del explorador, como nautilus).
// Cacheado en /tmp; "" si no se pudo extraer (sin ffmpeg).
std::string Video2DFramePng(const std::string& path);


#endif // VIDEO2D_CACHE_H

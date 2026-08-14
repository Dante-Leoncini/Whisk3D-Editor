#ifndef VIDEO2D_CACHE_H
#define VIDEO2D_CACHE_H
#include <string>

// ============================================================================
//  Video2DCache — la PREVIEW de los videos del Editor 2D: extrae frames con
//  ffmpeg (una vez por sesion, a un cache en /tmp) y los sube como texturas.
//  Sin ffmpeg (u otra plataforma) devuelve 0 frames y el elemento dibuja un
//  placeholder. El juego real NO usa esto: reproduce el video nativo.
// ============================================================================

struct VideoPreview {
    unsigned* frames;   // ids de textura GL (NULL si no se pudo)
    int numFrames;
    int w, h;           // tamano de los frames extraidos
    int anchoReal, altoReal;   // el tamano REAL del video (para el elemento)
    float fps;
};

// la preview del video (se extrae y sube UNA vez por sesion; "" o fallo = NULL)
const VideoPreview* Video2DPreview(const std::string& ruta);

// un png con UN frame del medio del video (miniaturas del explorador, como nautilus).
// Cacheado en /tmp; "" si no se pudo extraer (sin ffmpeg).
std::string Video2DFramePng(const std::string& ruta);

// milisegundos monotonicos (para avanzar la animacion)
unsigned W3dMsAhora();

#endif // VIDEO2D_CACHE_H

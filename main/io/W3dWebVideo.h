#ifndef W3D_WEB_VIDEO_H
#define W3D_WEB_VIDEO_H
// ============================================================================
//  W3dWebVideo - H.264 con el codificador del NAVEGADOR (WebCodecs: VideoEncoder). Solo la web (__EMSCRIPTEN__).
//
//  El navegador codifica; los cuadros comprimidos (Annex B) los junta el contenedor propio (io/W3dMp4.h) y el sonido
//  lo pone el codificador AAC propio (io/W3dAac.h): sin librerias ni nada que bajar. WebCodecs es ASINCRONO (el
//  codificador entrega los cuadros cuando el navegador recupera el control), asi que el render de la animacion a MP4
//  va de a unos cuadros por vuelta del bucle del navegador (W3dWebRenderTick, Properties.cpp): entre vuelta y vuelta
//  salen los cuadros codificados y se ve la barra de progreso.
//
//  Chrome / Edge 94+, Safari 16.4+ y Firefox 130+ (con H.264). Sin WebCodecs (un navegador viejo, o la pagina sin
//  HTTPS) MP4 no se ofrece y el render sigue en PNG / JPG.
// ============================================================================
#ifdef __EMSCRIPTEN__
#include <string>

// pregunta (una vez, al arrancar) si el navegador codifica H.264. La respuesta llega sola: hasta entonces, no
void W3dWebVideoTest();
bool W3dWebVideoHas();

// EL CODIFICADOR de un video: ancho y alto PARES, los fps y el bitrate. Se configura asincronico: mirar el estado
void W3dWebVideoOpen(int w, int h, int fps, int kbps);
enum { W3D_WEBVID_FAILED = -1, W3D_WEBVID_CONFIGURING = 0, W3D_WEBVID_READY = 1, W3D_WEBVID_EMPTYING = 2, W3D_WEBVID_FINISHED = 3 };
int W3dWebVideoState();
std::string W3dWebVideoError();
// los cuadros que esperan en la cola del codificador (para no llenarle la memoria al navegador: se espera con > 2)
int W3dWebVideoQueue();
// UN CUADRO en I420 (Y, despues U y V a la mitad): w * h * 3 / 2 bytes. 'indice' desde 0 (el tiempo = indice / fps)
bool W3dWebVideoFrame(const unsigned char* yuv, int w, int h, int index, bool key);
// el proximo trozo que dio el codificador: lo copia a 'dst' y devuelve su largo (0 = no hay; -largo = no entra en
// 'cap'). 'config' = es la configuracion (SPS + PPS en Annex B), no un cuadro
int W3dWebVideoRemove(unsigned char* dst, int cap, bool& config);
// no hay mas cuadros: que termine lo que tiene (el estado pasa a TERMINADO)
void W3dWebVideoFinish();
// suelta el codificador (al terminar o si algo fallo)
void W3dWebVideoClose();
#endif

#endif

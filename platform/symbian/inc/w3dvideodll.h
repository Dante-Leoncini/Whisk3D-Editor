#ifndef W3DVIDEODLL_H
#define W3DVIDEODLL_H
// ============================================================================
//  w3dvideodll.h - LA DLL DEL CODIFICADOR DE VIDEO (w3dvideo.dll, platform/symbian/src/w3dvideodll.cpp).
//
//  POR QUE UNA DLL APARTE: el codificador del telefono se usa por DevVideoRecord (devvideo.dll). El SDK con el que se
//  compila es mas nuevo que el del N95 y no hay como confirmar que el telefono tenga TODAS las funciones que importa:
//  si una faltara, un exe que la importa NO ARRANCA. Whisk3D carga esta DLL recien al renderizar un video (RLibrary):
//  si al N95 le falta algo, la DLL no carga, el MP4 queda sin ofrecer y el editor sigue andando.
//
//  La DLL no usa la STL ni el libc (solo euser + devvideo) ni tiene datos globales escribibles. Exporta UNA funcion
//  (ordinal 1) que llena la tabla del exe. Lo codificado vuelve por el gancho 'salida' (el exe lo mete en el MP4).
// ============================================================================

struct W3dVideoHost {
    void (*log)(const char* texto);                                   // al log de Whisk3D
    void (*salida)(void* ctx, const unsigned char* datos, int largo);  // un pedazo codificado (un cuadro)
    void* ctx;
};

enum { W3D_VIDEO_DLL_VERSION = 1 };

struct W3dVideoTabla {
    int version;                                                       // W3D_VIDEO_DLL_VERSION
    // los codificadores del telefono, al log (lo que saben hacer)
    void (*Sondear)(const W3dVideoHost* host);
    // los que hay: 1 = H.264, 2 = MPEG-4 (0 = ninguno)
    int (*Hay)();
    // abre el codificador (preferido: 0 = H.264, 1 = MPEG-4; si no esta, el otro). Devuelve el codificador (NULL = no)
    // y deja el que se uso, el tamano real (multiplos de 16) y el error
    void* (*Abrir)(const W3dVideoHost* host, int w, int h, int fps, int kbps, int preferido,
                   int* codec, int* wReal, int* hReal, int* error);
    // un cuadro RGBA (de arriba hacia abajo); 1 = bien
    int (*Cuadro)(void* cod, const unsigned char* rgba, int w, int h);
    // termina (lo que queda sale por 'salida') y lo libera; 1 = bien
    int (*Cerrar)(void* cod);
};

typedef void (*W3dVideoDllTablaFn)(W3dVideoTabla* t);

#endif

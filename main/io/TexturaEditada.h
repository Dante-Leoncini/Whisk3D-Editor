#ifndef TEXTURAEDITADA_H
#define TEXTURAEDITADA_H
// ============================================================================
//  TexturaEditada.h - TEXTURAS EDITABLES: los pixeles en CPU de una textura (generada o cargada) para
//  pintarla, con su estado de guardado. La textura vive en MEMORIA hasta que se guarda: en el
//  desplegable se ve con "*". Interna = entra al .w3d ("texturas/x.png"); externa = un PNG del
//  disco (en el archivo va "ext:" + la ruta, como siempre).
//    - "Save Texture": interna -> la entrada del contenedor montado (o queda pendiente para el proximo
//      guardado del proyecto si el proyecto nunca se guardo); externa -> el PNG del disco.
//    - "Make External..." / "Make Internal": cambia donde vive (y escribe ahi).
//    - El guardado del PROYECTO mete las internas que solo viven en memoria (nunca se dejaria un .w3d
//      con una referencia a algo que no existe).
// ============================================================================
#include <string>
#include <vector>
class Texture; struct W3dFalloff;

struct TexturaEditable {
    Texture* tex;                       // la textura registrada (Textures[])
    std::vector<unsigned char> rgba;    // pixeles (fila 0 = arriba), w*h*4
    int  w, h;
    bool alpha;                         // se guarda como PNG RGBA
    bool modificada;                    // cambios sin guardar (el "*")
    bool enMemoria;                     // interna que todavia no esta en ningun contenedor/disco
    std::vector<unsigned char> pngGuardado;   // "Save" sin contenedor montado: la version guardada, hasta guardar el proyecto
    TexturaEditable() : tex(0), w(0), h(0), alpha(false), modificada(false), enMemoria(false) {}
};

TexturaEditable* TexEditBuscar(Texture* t);                       // NULL si no esta en edicion
TexturaEditable* TexEditObtener(Texture* t);                      // la trae a memoria (decodifica) si hace falta
Texture* TexEditCrear(const std::string& nombre, const std::vector<unsigned char>& rgba, int w, int h, bool alpha); // nueva, en memoria
void TexEditSubir(TexturaEditable* te, int x0, int y0, int x1, int y1);   // sube a la GPU el rect [x0,x1)x[y0,y1)
bool TexEditEsExterna(const Texture* t);
bool TexEditGuardar(TexturaEditable* te, std::string& msg);
bool TexEditHacerExterna(TexturaEditable* te, const std::string& rutaDisco, std::string& msg);
bool TexEditHacerInterna(TexturaEditable* te, std::string& msg);
std::string TexEditEtiqueta(const Texture* t);                   // nombre de archivo + "*" si esta sin guardar
bool TexEditSinGuardar(const Texture* t);
// guardado del proyecto: el PNG de una interna que solo vive en memoria (true = usar estos bytes)
bool TexEditBytesParaGuardar(const std::string& path, std::string& png);
void TexEditProyectoGuardado();                                  // el .w3d se escribio: lo que era memoria ya esta adentro
void TexEditLimpiarTodo();                                       // cierre de proyecto
// PINCEL: un toque en (cx,cy) texels de radio r texels. rect[4] = {x0,y0,x1,y1} acumula lo tocado (-1 = nada).
bool TexEditDab(TexturaEditable* te, float cx, float cy, float r, const unsigned char* rgba, float fuerza, const W3dFalloff& fo, int* rect);
void TexEditMezclar(TexturaEditable* te, int x, int y, const unsigned char* rgba, float a);
void TexEditRectUnir(int* rect, int x, int y);
#endif // TEXTURAEDITADA_H

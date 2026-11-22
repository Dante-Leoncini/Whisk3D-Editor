#ifndef EDITED_TEXTURE_H
#define EDITED_TEXTURE_H
// ============================================================================
//  EditedTexture.h - TEXTURAS EDITABLES: los pixeles en CPU de una textura (generada o cargada) para
//  pintarla, con su estado de guardado. La textura vive en MEMORIA hasta que se guarda: en el
//  desplegable se ve con "*". Interna = entra al .w3d ("textures/x.png"); externa = un PNG del
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

struct TextureEditable {
    Texture* tex;                       // la textura registrada (Textures[])
    std::vector<unsigned char> rgba;    // pixeles (fila 0 = arriba), w*h*4
    int  w, h;
    bool alpha;                       // se guarda como PNG RGBA
    bool modified;                    // cambios sin guardar (el "*")
    bool inMemory;                    // interna que todavia no esta en ningun contenedor/disco
    std::vector<unsigned char> savedPng;   // "Save" sin contenedor montado: la version guardada, hasta guardar el proyecto
    bool mipsPending;                // se subio durante un trazo sin rehacer los mipmaps (TexEditEndStroke los rehace)
    TextureEditable() : tex(0), w(0), h(0), alpha(false), modified(false), inMemory(false), mipsPending(false) {}
};

TextureEditable* TexEditFind(Texture* t);                      // NULL si no esta en edicion
TextureEditable* TexEditGet(Texture* t);                       // la trae a memoria (decodifica) si hace falta
Texture* TexEditCreate(const std::string& name, const std::vector<unsigned char>& rgba, int w, int h, bool alpha); // nueva, en memoria
// sube a la GPU el rect [x0,x1)x[y0,y1). trazo=true (pintura en vivo): sin rehacer los mipmaps en cada toque (en Android
// es la textura entera cada vez); TexEditEndStroke los rehace una sola vez al soltar.
void TexEditUpload(TextureEditable* te, int x0, int y0, int x1, int y1, bool stroke = false);
void TexEditEndStroke(TextureEditable* te);
bool TexEditIsExternal(const Texture* t);
bool TexEditSave(TextureEditable* te, std::string& msg);
bool TexEditMakeExternal(TextureEditable* te, const std::string& diskPath, std::string& msg);
bool TexEditMakeInternal(TextureEditable* te, std::string& msg);
bool TexEditSaveAsInternal(TextureEditable* te, const std::string& name, bool force, bool* exists, std::string& msg);
std::string TexEditLabel(const Texture* t);                   // nombre de archivo + "*" si esta sin guardar
bool TexEditUnsaved(const Texture* t);
// el PNG de lo que se VE ahora (con lo pintado sin guardar): "Guardar como..." de la biblioteca escribe ESO. false = la
// textura no esta en edicion (sus bytes son los de su archivo)
bool TexEditPNGCurrent(const Texture* t, std::vector<unsigned char>& png);
// guardado del proyecto: el PNG de una interna que solo vive en memoria (true = usar estos bytes)
bool TexEditBytesToSave(const std::string& path, std::string& png);
void TexEditSavedProject();                                  // el .w3d se escribio: lo que era memoria ya esta adentro
void TexEditClearAll();                                      // cierre de proyecto
// PINCEL: un toque en (cx,cy) texels de radio r texels. rect[4] = {x0,y0,x1,y1} acumula lo tocado (-1 = nada).
bool TexEditDab(TextureEditable* te, float cx, float cy, float r, const unsigned char* rgba, float force, const W3dFalloff& fo, int* rect);
void TexEditMix(TextureEditable* te, int x, int y, const unsigned char* rgba, float a);
void TexEditRectJoin(int* rect, int x, int y);
#endif // EDITED_TEXTURE_H

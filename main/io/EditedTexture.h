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

// EL DESHACER DE LA PINTURA, POR BALDOSAS de kTexEditTile x kTexEditTile: un trazo guarda los pixeles de ANTES solo de las
// baldosas que toca, la primera vez que las toca. Antes cada paso del Ctrl+Z era una copia de la textura entera: con los
// 100 pasos del historial, una de 1024x1024 podia llegar a 400 MB (en el Galaxy Ace o la Dreamcast no entraban ni diez)
enum { kTexEditTile = 64 };
struct TexEditTiles {
    int w, h;                             // el tamanio de la textura al grabarlas (si cambio, no se aplican)
    std::vector<int> index;               // que baldosa es cada una (fila * columnas + columna)
    std::vector<unsigned char> px;        // sus pixeles, una detras de otra (las del borde derecho y de abajo, recortadas)
    TexEditTiles() : w(0), h(0) {}
};

struct TextureEditable {
    Texture* tex;                       // la textura registrada (Textures[])
    std::vector<unsigned char> rgba;    // pixeles (fila 0 = arriba), w*h*4
    int  w, h;
    bool alpha;                       // se guarda como PNG RGBA
    bool modified;                    // cambios sin guardar (el "*")
    bool inMemory;                    // interna que todavia no esta en ningun contenedor/disco
    std::vector<unsigned char> savedPng;   // "Save" sin contenedor montado: la version guardada, hasta guardar el proyecto
    bool mipsPending;                // se subio durante un trazo sin rehacer los mipmaps (TexEditEndStroke los rehace)
    int  pins;                       // cuantos guardan punteros a rgba ahora (un horneado en curso): no se suelta
    bool pinnedRT;                   // el trazado de rayos guardo punteros a rgba (su escena): no se suelta mas
    TexEditTiles* rec;               // grabando el antes de un trazo (TexEditRecordStart); NULL = no se graba
    std::vector<unsigned char> recDone;   // grabando: 1 = esa baldosa ya tiene su antes guardado
    TextureEditable() : tex(0), w(0), h(0), alpha(false), modified(false), inMemory(false), mipsPending(false),
                        pins(0), pinnedRT(false), rec(0) {}
};

TextureEditable* TexEditFind(Texture* t);                      // NULL si no esta en edicion
TextureEditable* TexEditGet(Texture* t);                       // la trae a memoria (decodifica) si hace falta
// ...y queda FIJA (no se suelta al salir de pintar): el trazado de rayos guarda punteros a sus pixeles
TextureEditable* TexEditGetPinned(Texture* t);
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
// EL ANTES de un trazo (el paso del Ctrl+Z): desde Start, cada pixel que escriben TexEditMix / TexEditDab guarda antes su
// baldosa en 'into'. 'whole' guarda todas ya (el horneado escribe la textura entera sin pasar por el pincel). Stop deja
// de grabar; Swap intercambia lo guardado con lo de ahora (deshacer y rehacer son el mismo intercambio)
void TexEditRecordStart(TextureEditable* te, TexEditTiles* into, bool whole);
void TexEditRecordStop(TextureEditable* te);
bool TexEditSwapTiles(TextureEditable* te, TexEditTiles& t);
// LOS PIXELES PARA LEERLOS UNA VEZ (la copia borrosa del fondo, el cubo del entorno, la de una sonda): los de su edicion si
// se esta pintando (con lo pintado), o decodificados aca y soltados al destruirse. TexEditGet dejaba la copia entera en
// la RAM hasta cerrar el proyecto (un fondo 360 de 4096x2048 son 32 MB) para hacer una copia de 64x32
struct TexPixels {
    const unsigned char* px; int w, h;
    unsigned char* own;
    TexPixels() : px(0), w(0), h(0), own(0) {}
    ~TexPixels();
private:
    TexPixels(const TexPixels&); TexPixels& operator=(const TexPixels&);
};
bool TexEditPixels(Texture* t, TexPixels& out);
// AL SALIR DE PINTAR: las texturas en edicion sin nada propio (sin cambios sin guardar, ya en su archivo, sin un trazo a
// medias ni punteros afuera) sueltan sus pixeles; la proxima vez que se pinten se vuelven a leer de su archivo
void TexEditReleaseIdle();
#endif // EDITED_TEXTURE_H

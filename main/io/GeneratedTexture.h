#ifndef GENERATED_TEXTURE_H
#define GENERATED_TEXTURE_H
// ============================================================================
//  GeneratedTexture.h - texturas de PRUEBA generadas por el editor ("Nueva textura" del UV editor):
//  un color liso, una grilla UV (damero gris con cruces de colores) o una grilla de colores con
//  celdas rotuladas A1..H8. Sirven para ver que el mapeo UV esta bien (estirado, espejado, escala).
//  La imagen se escribe como PNG adentro del proyecto (o en el disco si el proyecto no esta guardado),
//  se carga como cualquier textura y se le asigna al material de la parte activa.
// ============================================================================
#include <string>
#include <vector>

enum { TexGenBlank = 0, TexGenUVGrid, TexGenColorGrid, TexGenTypes };

const char* TexGenTypeName(int type);   // clave de traduccion ("Blank", "UV Grid", "Color Grid")

// lado (px) de las celdas cuadradas de las grillas para una imagen w x h (64 px; imagenes chicas: 4 por lado)
int W3dTexGenCell(int w, int h);

// genera los pixeles RGBA (8 bits, fila 0 = arriba). 'color' = RGBA 0..1 (el liso; su alpha solo se
// usa con 'alpha'). Las grillas salen opacas.
void W3dGenerateTexture(int type, int w, int h, const float* color, bool alpha, std::vector<unsigned char>& rgba);

// genera + registra la textura EN MEMORIA ("textures/<slug>.png", con "*" hasta que se guarde; ver
// EditedTexture.h) y la muestra en el UV editor. No se la asigna a ningun material. "" + 'msg' si falla.
std::string W3dCreateTextureProject(const std::string& name, int type, int w, int h, const float* color, bool alpha, std::string& msg);

int W3dGeneratedTexturesClear();   // (compatibilidad de las pruebas: ya no se escribe nada al crear)
#endif // GENERATED_TEXTURE_H

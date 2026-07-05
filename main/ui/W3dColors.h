#ifndef W3D_COLORS_H
#define W3D_COLORS_H

#ifdef _WIN32
#ifndef W3D_SYMBIAN
    #include <windows.h>
#endif
#endif

#ifdef W3D_SYMBIAN
    #include <GLES/gl.h>
#else
    #include <GL/gl.h>
#endif

// Colores propios del editor 3D (ejes, piso, gizmos de transformacion, overlays de
// normales, seleccion). No van en Whisk3D-UI (que es una libreria de UI generica).
enum W3dColorID {
    W3dColor_naranjaFace = 0,
    W3dColor_rojoEje,
    W3dColor_LineaPiso,
    W3dColor_LineaPisoRoja,
    W3dColor_LineaPisoVerde,
    W3dColor_ColorTransformX,
    W3dColor_ColorTransformY,
    W3dColor_ColorTransformZ,
    W3dColor_normalVertex,
    W3dColor_normalCustom,
    W3dColor_normalFace,
    W3dColor_seleccionInactiva,
    W3dColor_COUNT
};

extern float   W3dColores[W3dColor_COUNT][4];
extern GLubyte W3dColoresUbyte[W3dColor_COUNT][4];

// Devuelve el W3dColorID del nombre, o -1 si no es un color del editor.
int  editorColorIdPorNombre(const char* nombre);

// Carga los colores del editor desde un skin.ini (ignora nombres que no reconoce).
bool loadEditorColorsW3d(const char* aPath);

// Copia la paleta (UI + editor) a la paleta de RENDER del core (gRenderColors).
void SincronizarRenderColores();

#endif // W3D_COLORS_H

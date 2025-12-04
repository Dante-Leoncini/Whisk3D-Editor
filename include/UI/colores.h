#ifndef COLORES_H
#define COLORES_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include <GL/gl.h>     // Para GLubyte, GLfixed si hace falta
#include <cstdint>

// ----------------------
//   ENUM de IDs
// ----------------------
enum class ColorID {
    background,
    blanco,
    accent,
    accentDark,
    negro,
    gris,
    naranjaFace,
    headerColor,
    negroTransparente,
    grisUI,
    rojoEje,
    LineaPiso,
    LineaPisoRoja,
    LineaPisoVerde,
    ColorTransformX,
    ColorTransformY,
    ColorTransformZ
};

// ----------------------
//   Arrays declarados
// ----------------------
extern GLubyte ListaColoresUbyte[17][4];

#ifdef __ANDROID__
extern GLfixed ListaColoresX[17][4];
#else
extern float ListaColores[17][4];
#endif

#endif // COLORES_H
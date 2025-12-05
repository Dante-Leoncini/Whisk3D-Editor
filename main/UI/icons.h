#ifndef ICONS_H
#define ICONS_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include <cstddef>
#include <vector>
#include <GL/gl.h>

#include "UI.h" // para RenglonHeight

// Tamaños iniciales
extern GLshort IconMesh[8];
extern GLshort IconLineMesh[8];
extern int IconSize;
extern int IconSizeGS;

constexpr size_t ICON_TOTAL = 13;

enum class IconType {
    camera,
    light,
    mesh,
    visible,
    hidden,
    archive,
    line,
    arrow,
    arrowRight,
    mirror,
    array,
    gamepad,
    instance
};

struct IconRect {
    int x, y, w, h;
};

class IconUV {
public:
    GLfloat uvs[8];
};

// ===================================================
//  Vector global de íconos UV
// ===================================================
extern std::vector<IconUV*> IconsUV;

// ===================================================
// Funciones
// ===================================================
void SetIconScale(int scale);
void CrearIconos(int texW, int texH);

#endif
#include "w3dOpenGL.h"

typedef float ColorType;

void w3dSetColor(const ColorType c[4]) {
    glColor4fv(c);
}
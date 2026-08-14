// Shim de <GL/glu.h> para WebGL / Emscripten.
// GLU (gluBuild2DMipmaps, gluProject...) es del pipeline fijo de desktop y no existe en web.
// El editor no llama funciones GLU en el path web (los mipmaps/picking van por otro camino en
// ES2), asi que este shim solo satisface el #include y trae los tipos GL (via gl.h ES2).
#pragma once
#include "gl.h"

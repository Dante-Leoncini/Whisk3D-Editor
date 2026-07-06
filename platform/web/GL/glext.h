// Shim de <GL/glext.h> para WebGL / Emscripten.
// El editor incluye <GL/glext.h> (extensiones del pipeline fijo de desktop), pero en web
// NO usa esas extensiones: dibuja por w3dEngine (backend ES2). El glext.h real de Emscripten
// referencia tipos que no existen en ES2 (GLclampd) y no compila. Este shim lo reemplaza:
// solo trae los tipos/constantes via el gl.h de ES2 (mismo directorio).
#pragma once
#include "gl.h"

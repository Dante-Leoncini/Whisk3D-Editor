#ifndef CONSTRUCTOR_H
#define CONSTRUCTOR_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <GL/gl.h>

#include "objects/Camera.h"
#include "objects/Collection.h"
#include "objects/Mesh.h"

#include "ViewPorts/ViewPorts.h"
#include "ViewPorts/Outliner.h"
#include "ViewPorts/ViewPort3D.h"

#include "importers/import_w3d.h"

// Variable global visible en todos los archivos
extern bool running;

// Constructor universal para todas las plataformas
void ConstructUniversal(int argc, char* argv[]);

#endif